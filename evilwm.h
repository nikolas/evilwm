#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#ifdef MWM_HINTS
#include <Xm/MwmUtil.h>
#endif

#include "keymap.h"

#ifdef VWM
#ifdef VDESK
#define VDESK_BOTH 1
#endif
#endif

/* default settings */

#define DEF_FONT	"variable"
#define DEF_FG		"goldenrod"
#define DEF_BG		"grey50"
#define DEF_BW		1
#define DEF_FC		"blue"
#define SPACE		3
#define MINSIZE		15
#ifdef DEBIAN
#define DEF_TERM	"x-terminal-emulator"
#else
#define DEF_TERM	"xterm"
#endif

/* readability stuff */
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif

#define STICKY 0	/* Desktop number for sticky clients */
#define KEY_TO_VDESK( key ) ( ( key ) - XK_1 + 1 )

#define RAISE		1
#define NO_RAISE	0	/* for unhide() */

/* some coding shorthand */

#define ChildMask	(SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask	(ButtonPressMask|ButtonReleaseMask)
#define MouseMask	(ButtonMask|PointerMotionMask)

#define grab_pointer(w, mask, curs) \
	(XGrabPointer(dpy, w, False, mask, GrabModeAsync, GrabModeAsync, \
	None, curs, CurrentTime) == GrabSuccess)
#define grab_keysym(w, mask, keysym) \
	XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), mask, w, True, \
			GrabModeAsync, GrabModeAsync); \
	XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), LockMask|mask, w, True, \
			GrabModeAsync, GrabModeAsync); \
	if (numlockmask) { \
		XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), numlockmask|mask, \
				w, True, GrabModeAsync, GrabModeAsync); \
		XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), \
				numlockmask|LockMask|mask, w, True, \
				GrabModeAsync, GrabModeAsync); \
	}
#define grab_button(w, mask, button) \
	XGrabButton(dpy, button, mask, w, False, ButtonMask, \
			GrabModeAsync, GrabModeSync, None, None); \
	XGrabButton(dpy, button, LockMask|mask, w, False, ButtonMask, \
			GrabModeAsync, GrabModeSync, None, None); \
	if (numlockmask) { \
		XGrabButton(dpy, button, numlockmask|mask, w, False, \
				ButtonMask, GrabModeAsync, GrabModeSync, \
				None, None); \
		XGrabButton(dpy, button, numlockmask|LockMask|mask, w, False, \
				ButtonMask, GrabModeAsync, GrabModeSync, \
				None, None); \
	}
#define setmouse(w, x, y) \
	XWarpPointer(dpy, None, w, 0, 0, 0, 0, x, y)
#define gravitate(c) \
	change_gravity(c, 1)
#define ungravitate(c) \
	change_gravity(c, -1)

/* screen structure */

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo {
	int screen;
	Window root;
	Colormap def_cmap;
	GC invert_gc;
#ifdef VWM
	XColor fg, bg, fc;
#else
	XColor fg, bg;
#endif
	char *display;
};

/* client structure */

typedef struct Client Client;

struct Client {
	Client	*next;
	Window	window;
	Window	parent;
	ScreenInfo	*screen;

#ifdef COLOURMAP
	Colormap	cmap;
#endif
	XSizeHints	*size;
	int		ignore_unmap;

	int		x, y, width, height;
	int		oldx, oldy, oldw, oldh;  /* used when maximising */
	int		border;
#ifdef VWM
	int		vdesk;
#endif /* def VWM */
};

/* declarations for global variables in main.c */

extern Display		*dpy;
/* extern int		screen;
extern Window		root;
extern GC		invert_gc;
#ifdef VWM
extern XColor		fg, bg, fc;
#else
extern XColor		fg, bg;
#endif
*/
extern int		num_screens;
extern ScreenInfo	*screens;
extern ScreenInfo	*current_screen;
extern Client		*current;
extern Window		initialising;
extern XFontStruct	*font;
extern Client		*head_client;
extern Atom		xa_wm_state;
extern Atom		xa_wm_change_state;
extern Atom		xa_wm_protos;
extern Atom		xa_wm_delete;
extern Atom		xa_wm_cmapwins;
extern Cursor		move_curs;
extern Cursor		resize_curs;
extern const char	*opt_display;
extern const char	*opt_font;
extern const char	*opt_fg;
extern const char	*opt_bg;
extern const char	**opt_term;
extern int		opt_bw;
#ifdef VWM
extern const char	*opt_fc;
extern int		vdesk;
#endif
#ifdef SNAP
extern int		opt_snap;
#endif
#ifdef SHAPE
extern int		have_shape, shape_event;
#endif
extern int		quitting;
#ifdef MWM_HINTS
extern Atom		mwm_hints;
#endif
extern unsigned int numlockmask;

/* client.c */

Client *find_client(Window w);
int wm_state(Client *c);
void change_gravity(Client *c, int multiplier);
void select_client(Client *c);
void remove_client(Client *c);
void send_config(Client *c);
void send_wm_delete(Client *c);
void set_wm_state(Client *c, int state);
void set_shape(Client *c);
void client_update_current(Client *c, Client *newcurrent);

/* events.c */

void handle_key_event(XKeyEvent *e);
void handle_button_event(XButtonEvent *e);
/* static void handle_client_message(XClientMessageEvent *e); */
#ifdef COLOURMAP
void handle_colormap_change(XColormapEvent *e);
#endif
void handle_configure_request(XConfigureRequestEvent *e);
void handle_enter_event(XCrossingEvent *e);
void handle_leave_event(XCrossingEvent *e);
void handle_map_request(XMapRequestEvent *e);
void handle_property_change(XPropertyEvent *e);
void handle_unmap_event(XUnmapEvent *e);
#ifdef SHAPE
void handle_shape_event(XShapeEvent *e);
#endif

/* main.c */

int main(int argc, char *argv[]);
void scan_windows(void);
void setup_display(void);

/* misc.c */
#ifdef SANITY
void sanity_check(void);
#endif
#ifdef VDESK_BOTH
void spawn_vdesk(int todesk, Client *c);
#endif

/* void do_event_loop(void); */
int handle_xerror(Display *dsply, XErrorEvent *e);
int ignore_xerror(Display *dsply, XErrorEvent *e);
void dump_clients(void);
void spawn(const char *const cmd[]);
void handle_signal(int signo);
#ifdef DEBUG
void show_event(XEvent e);
#endif

/* new.c */

void init_position(Client *c);
void make_new_client(Window w, ScreenInfo *s);
void reparent(Client *c);

/* screen.c */

void drag(Client *c);
void draw_outline(Client *c);
void get_mouse_position(int *x, int *y, Window root);
void move(Client *c, int set);
void recalculate_sweep(Client *c, int x1, int y1, int x2, int y2);
void resize(Client *c, int set);
void maximise_vert(Client *c);
void maximise_horiz(Client *c);
void show_info(Client *c, KeySym key);
void sweep(Client *c);
void unhide(Client *c, int raise);
void next(void);
#ifdef VWM
void hide(Client *c);
void switch_vdesk(int v);
#else
# ifdef VDESK
 void hide(Client *c);
# endif
#endif /* def VWM */
#ifdef VDESK
void handle_client_message(XClientMessageEvent *e);
#endif
ScreenInfo *find_screen(Window root);
