/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2015 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#ifdef RANDR
#include <X11/extensions/Xrandr.h>
#endif

#ifndef __GNUC__
# define  __attribute__(x)
#endif

#include "keymap.h"
#include "list.h"

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

#define VDESK_NONE  (0xfffffffe)
#define VDESK_FIXED (0xffffffff)
#define VDESK_MAX   (7)
#define KEY_TO_VDESK(key) ((key) - XK_1)
#define valid_vdesk(v) ((v) == VDESK_FIXED || (v) <= VDESK_MAX)

#define RAISE           1
#define NO_RAISE        0       /* for unhide() */

/* EWMH hints use these definitions, so for simplicity my functions
 * will too: */
#define NET_WM_STATE_REMOVE     0    /* remove/unset property */
#define NET_WM_STATE_ADD        1    /* add/set property */
#define NET_WM_STATE_TOGGLE     2    /* toggle property  */

/* EWMH window type bits */
#define EWMH_WINDOW_TYPE_DESKTOP (1<<0)
#define EWMH_WINDOW_TYPE_DOCK    (1<<1)
#define EWMH_WINDOW_TYPE_NOTIFICATION (1<<2)

#define MAXIMISE_HORZ   (1<<0)
#define MAXIMISE_VERT   (1<<1)

/* some coding shorthand */

#define ChildMask       (SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask      (ButtonPressMask|ButtonReleaseMask)
#define MouseMask       (ButtonMask|PointerMotionMask)

#define grab_pointer(w, mask, curs) \
	(XGrabPointer(dpy, w, False, mask, GrabModeAsync, GrabModeAsync, \
	None, curs, CurrentTime) == GrabSuccess)
#define grab_button(w, mask, button) do { \
		XGrabButton(dpy, button, (mask), w, False, ButtonMask, \
		            GrabModeAsync, GrabModeSync, None, None); \
		XGrabButton(dpy, button, LockMask|(mask), w, False, ButtonMask,\
		            GrabModeAsync, GrabModeSync, None, None); \
		XGrabButton(dpy, button, numlockmask|(mask), w, False, \
		            ButtonMask, GrabModeAsync, GrabModeSync, \
		            None, None); \
		XGrabButton(dpy, button, numlockmask|LockMask|(mask), w, False,\
		            ButtonMask, GrabModeAsync, GrabModeSync, \
		            None, None); \
	} while (0)
#define setmouse(w, x, y) XWarpPointer(dpy, None, w, 0, 0, 0, 0, x, y)
#define get_mouse_position(xp,yp,root) do { \
		Window dw; \
		int di; \
		unsigned int dui; \
		XQueryPointer(dpy, root, &dw, &dw, xp, yp, &di, &di, &dui); \
	} while (0)

#define is_fixed(c) (c->vdesk == VDESK_FIXED)
#define add_fixed(c) c->vdesk = VDESK_FIXED
#define remove_fixed(c) c->vdesk = c->screen->vdesk

/* screen structure */

typedef struct ScreenInfo ScreenInfo;
struct ScreenInfo {
	int screen;
	Window root;
	Window supporting;  /* Dummy window for EWMH */
	GC invert_gc;
	XColor fg, bg;
#ifdef VWM
	unsigned int vdesk;
	XColor fc;
	unsigned old_vdesk; /* most recently unmapped vdesk, so user may toggle back to it */
#endif
	char *display;
	int docks_visible;
};

/* client structure */

typedef struct Client Client;
struct Client {
	Window  window;
	Window  parent;
	ScreenInfo      *screen;
	Colormap        cmap;
	int             ignore_unmap;

	int             x, y, width, height;
	int             border;
	int             oldx, oldy, oldw, oldh;  /* used when maximising */

	int             min_width, min_height;
	int             max_width, max_height;
	int             width_inc, height_inc;
	int             base_width, base_height;
	int             win_gravity_hint;
	int             win_gravity;
	int             old_border;
#ifdef VWM
	unsigned int vdesk;
#endif
	int             is_dock;
	int             remove;  /* set when client needs to be removed */
};

typedef struct Application Application;
struct Application {
	char *res_name;
	char *res_class;
	int geometry_mask;
	int x, y;
	unsigned int width, height;
	int is_dock;
#ifdef VWM
	unsigned int vdesk;
#endif
};

/* Declarations for global variables in main.c */

/* Commonly used X information */
extern Display      *dpy;
extern XFontStruct  *font;
extern Cursor       move_curs;
extern Cursor       resize_curs;
extern int          num_screens;
extern ScreenInfo   *screens;
#ifdef SHAPE
extern int          have_shape, shape_event;
#endif
#ifdef RANDR
extern int          have_randr, randr_event_base;
#endif

/* Standard X protocol atoms */
extern Atom xa_wm_state;
extern Atom xa_wm_protos;
extern Atom xa_wm_delete;
extern Atom xa_wm_cmapwins;

/* Motif atoms */
extern Atom mwm_hints;

/* evilwm atoms */
extern Atom xa_evilwm_unmaximised_horz;
extern Atom xa_evilwm_unmaximised_vert;

/* EWMH: Root Window Properties (and Related Messages) */
#ifdef VWM
extern Atom xa_net_current_desktop;
#endif
extern Atom xa_net_active_window;

/* EWMH: Other Root Window Messages */
extern Atom xa_net_close_window;
extern Atom xa_net_moveresize_window;
extern Atom xa_net_restack_window;
extern Atom xa_net_request_frame_extents;

/* EWMH: Application Window Properties */
#ifdef VWM
extern Atom xa_net_wm_desktop;
#endif
extern Atom xa_net_wm_window_type;
extern Atom xa_net_wm_window_type_dock;
extern Atom xa_net_wm_state;
extern Atom xa_net_wm_state_maximized_vert;
extern Atom xa_net_wm_state_maximized_horz;
extern Atom xa_net_wm_state_fullscreen;
extern Atom xa_net_frame_extents;

/* Things that affect user interaction */
extern unsigned int     numlockmask;
extern unsigned int     grabmask1;
extern unsigned int     grabmask2;
extern unsigned int     altmask;
extern char             **opt_term;
extern int              opt_bw;
extern int              opt_snap;
#ifdef SOLIDDRAG
extern int              no_solid_drag;
#else
# define no_solid_drag (1)
#endif
extern struct list      *applications;

/* Client tracking information */
extern struct list      *clients_tab_order;
extern struct list      *clients_mapping_order;
extern struct list      *clients_stacking_order;
extern Client           *current;
extern volatile Window  initialising;

/* Event loop will run until this flag is set */
extern int wm_exit;

/* client.c */

Client *find_client(Window w);
void client_hide(Client *c);
void client_show(Client *c);
void client_raise(Client *c);
void client_lower(Client *c);
void gravitate_border(Client *c, int bw);
void select_client(Client *c);
#ifdef VWM
void client_to_vdesk(Client *c, unsigned int vdesk);
#endif
void remove_client(Client *c);
void send_config(Client *c);
void send_wm_delete(Client *c, int kill_client);
void set_wm_state(Client *c, int state);
void set_shape(Client *c);
void *get_property(Window w, Atom property, Atom req_type, unsigned long *nitems_return);

/* events.c */

void event_main_loop(void);

/* misc.c */

extern int need_client_tidy;
extern int ignore_xerror;
int handle_xerror(Display *dsply, XErrorEvent *e);
void spawn(const char *const cmd[]);
void handle_signal(int signo);
void discard_enter_events(Client *except);

/* new.c */

void make_new_client(Window w, ScreenInfo *s);
long get_wm_normal_hints(Client *c);
void get_window_type(Client *c);

/* screen.c */

void drag(Client *c);
void moveresize(Client *c);
void maximise_client(Client *c, int action, int hv);
void show_info(Client *c, unsigned int keycode);
void sweep(Client *c);
void next(void);
#ifdef VWM
void switch_vdesk(ScreenInfo *s, unsigned int v);
#endif
void set_docks_visible(ScreenInfo *s, int is_visible);
ScreenInfo *find_screen(Window root);
ScreenInfo *find_current_screen(void);
void grab_keys_for_screen(ScreenInfo *s);

/* ewmh.c */

void ewmh_init(void);
void ewmh_init_screen(ScreenInfo *s);
void ewmh_deinit_screen(ScreenInfo *s);
void ewmh_init_client(Client *c);
void ewmh_deinit_client(Client *c);
void ewmh_withdraw_client(Client *c);
void ewmh_select_client(Client *c);
void ewmh_set_net_client_list(ScreenInfo *s);
void ewmh_set_net_client_list_stacking(ScreenInfo *s);
#ifdef VWM
void ewmh_set_net_current_desktop(ScreenInfo *s);
#endif
void ewmh_set_net_active_window(Client *c);
#ifdef VWM
void ewmh_set_net_wm_desktop(Client *c);
#endif
unsigned int ewmh_get_net_wm_window_type(Window w);
void ewmh_set_net_wm_state(Client *c);
void ewmh_set_net_frame_extents(Window w);
