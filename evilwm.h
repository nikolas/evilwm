#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif

#include "keymap.h"

/* Required for interpreting MWM hints: */
#define _XA_MWM_HINTS           "_MOTIF_WM_HINTS"
#define PROP_MWM_HINTS_ELEMENTS 3
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
typedef struct {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
} PropMwmHints;

#define MAXIMISE_HORZ        (1<<0)
#define MAXIMISE_VERT        (1<<1)

/* sanity on options */
#if defined(INFOBANNER_MOVERESIZE) && !defined(INFOBANNER)
# define INFOBANNER
#endif

/* default settings */

#define DEF_FONT        "variable"
#define DEF_FG          "goldenrod"
#define DEF_BG          "grey50"
#define DEF_BW          1
#define DEF_FC          "blue"
#define SPACE           3
#ifdef DEBIAN
#define DEF_TERM        "x-terminal-emulator"
#else
#define DEF_TERM        "xterm"
#endif

/* readability stuff */

#define KEY_TO_VDESK(key) ((key) - XK_1)
#define valid_vdesk(v) ((unsigned)(v) < KEY_TO_VDESK(XK_8))

#define RAISE           1
#define NO_RAISE        0       /* for unhide() */

/* some coding shorthand */

#define ChildMask       (SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask      (ButtonPressMask|ButtonReleaseMask)
#define MouseMask       (ButtonMask|PointerMotionMask)

#define grab_pointer(w, mask, curs) \
	(XGrabPointer(dpy, w, False, mask, GrabModeAsync, GrabModeAsync, \
	None, curs, CurrentTime) == GrabSuccess)
#define grab_keysym(w, mask, keysym) \
	XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), (mask), w, True, \
	         GrabModeAsync, GrabModeAsync); \
	XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), LockMask|(mask), w, True, \
	         GrabModeAsync, GrabModeAsync); \
	if (numlockmask) { \
		XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), numlockmask|(mask), \
		         w, True, GrabModeAsync, GrabModeAsync); \
		XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), \
		         numlockmask|LockMask|(mask), w, True, \
		         GrabModeAsync, GrabModeAsync); \
	}
#define grab_button(w, mask, button) \
	XGrabButton(dpy, button, (mask), w, False, ButtonMask, \
	            GrabModeAsync, GrabModeSync, None, None); \
	XGrabButton(dpy, button, LockMask|(mask), w, False, ButtonMask, \
	            GrabModeAsync, GrabModeSync, None, None); \
	if (numlockmask) { \
		XGrabButton(dpy, button, numlockmask|(mask), w, False, \
		            ButtonMask, GrabModeAsync, GrabModeSync, \
		            None, None); \
		XGrabButton(dpy, button, numlockmask|LockMask|(mask), w, False, \
		            ButtonMask, GrabModeAsync, GrabModeSync, \
		            None, None); \
	}
#define setmouse(w, x, y) XWarpPointer(dpy, None, w, 0, 0, 0, 0, x, y)
#define gravitate(c) gravitate_client(c, 1)
#define ungravitate(c) gravitate_client(c, -1)

#define is_sticky(c) (c->sticky)
#define add_sticky(c) c->sticky = 1
#define remove_sticky(c) c->sticky = 0
#define toggle_sticky(c) c->sticky = !c->sticky

#define discard_enter_events() do { \
		XEvent dummy; \
		XSync(dpy, False); \
		while (XCheckMaskEvent(dpy, EnterWindowMask, &dummy)); \
	} while (0)

/* screen structure */

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo {
	int screen;
	Window root;
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
	Client  *next;
	Window  window;
	Window  parent;
	ScreenInfo      *screen;

#ifdef COLOURMAP
	Colormap        cmap;
#endif
	int             ignore_unmap;

	int             x, y, width, height;
	int             border;
	int             oldx, oldy, oldw, oldh;  /* used when maximising */

	int             min_width, min_height;
	int             max_width, max_height;
	int             width_inc, height_inc;
	int             base_width, base_height;
	int             win_gravity;
	int             old_border;
#ifdef VWM
	int             vdesk;
	int             sticky;
#endif /* def VWM */
#ifdef SHAPE
	int             bounding_shaped;
#endif
	int             remove;  /* set when client needs to be removed */
};

typedef struct Application Application;
struct Application {
	char *res_name;
	char *res_class;
	int geometry_mask;
	int x, y;
	unsigned int width, height;
#ifdef VWM
	int vdesk;
	int sticky;
#endif
	Application *next;
};

/* declarations for global variables in main.c */

extern Display          *dpy;
extern int              num_screens;
extern ScreenInfo       *screens;
extern ScreenInfo       *current_screen;
extern Client           *current;
extern volatile Window  initialising;
extern XFontStruct      *font;
extern Client           *head_client;
extern Application      *head_app;
extern Cursor           move_curs;
extern Cursor           resize_curs;
extern const char       *opt_display;
extern const char       *opt_font;
extern const char       *opt_fg;
extern const char       *opt_bg;
extern const char       *opt_term[3];
extern int              opt_bw;
#ifdef VWM
extern const char       *opt_fc;
extern int              vdesk;
#endif
#ifdef SNAP
extern int              opt_snap;
#endif
#ifdef SOLIDDRAG
extern int              solid_drag;
#else
# define solid_drag (0)
#endif
#ifdef SHAPE
extern int              have_shape, shape_event;
#endif
extern unsigned int numlockmask;
extern unsigned int grabmask2;
extern unsigned int altmask;

/* Standard X protocol atoms */
extern Atom             xa_wm_state;
extern Atom             xa_wm_protos;
extern Atom             xa_wm_delete;
extern Atom             xa_wm_cmapwins;
/* Motif atoms */
extern Atom             mwm_hints;
/* EWMH atoms */
extern Atom xa_net_wm_desktop;
extern Atom xa_net_wm_state;
extern Atom xa_net_wm_state_sticky;

/* client.c */

Client *find_client(Window w);
int wm_state(Client *c);
void gravitate_client(Client *c, int sign);
void select_client(Client *c);
#ifdef VWM
void fix_client(Client *c);
#endif
void remove_client(Client *c);
void send_config(Client *c);
void send_wm_delete(Client *c, int kill_client);
void set_wm_state(Client *c, int state);
void set_shape(Client *c);
void client_update_current(Client *c, Client *newcurrent);

/* events.c */

void event_main_loop(void);

/* main.c */

int main(int argc, char *argv[]);
void scan_windows(void);

/* misc.c */

extern int need_client_tidy;
extern int ignore_xerror;
/* void do_event_loop(void); */
int handle_xerror(Display *dsply, XErrorEvent *e);
void spawn(const char *const cmd[]);
void handle_signal(int signo);
#ifdef DEBUG
void show_event(XEvent e);
#endif

/* new.c */

void make_new_client(Window w, ScreenInfo *s);
long get_wm_normal_hints(Client *c);

/* screen.c */

void drag(Client *c);
void get_mouse_position(int *x, int *y, Window root);
void moveresize(Client *c);
void recalculate_sweep(Client *c, int x1, int y1, int x2, int y2);
void maximise_client(Client *c, int hv);
void show_info(Client *c, KeySym key);
void sweep(Client *c);
void unhide(Client *c, int raise_win);
void next(void);
#ifdef VWM
void hide(Client *c);
void switch_vdesk(int v);
#endif /* def VWM */
ScreenInfo *find_screen(Window root);

/* ewmh.c */

#ifdef VWM
void update_net_wm_desktop(Client *c);
void update_net_wm_state(Client *c);
#endif
