/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2006 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <X11/cursorfont.h>
#include "evilwm.h"
#include "log.h"

/* Commonly used X information */
Display     *dpy;
XFontStruct *font;
Cursor      move_curs;
Cursor      resize_curs;
int         num_screens;
ScreenInfo  *screens;
#ifdef SHAPE
int         have_shape, shape_event;
#endif

/* Standard X protocol atoms */
Atom xa_wm_state;
Atom xa_wm_protos;
Atom xa_wm_delete;
Atom xa_wm_cmapwins;
/* Motif atoms */
Atom mwm_hints;
/* EWMH atoms */
#ifdef VWM
Atom xa_net_wm_desktop;
Atom xa_net_wm_state;
Atom xa_net_wm_state_sticky;
#endif

/* Things that affect user interaction */
static const char   *opt_display = "";
static const char   *opt_font = DEF_FONT;
static const char   *opt_fg = DEF_FG;
static const char   *opt_bg = DEF_BG;
#ifdef VWM
static const char   *opt_fc = DEF_FC;
#endif
unsigned int numlockmask = 0;
unsigned int grabmask1 = ControlMask|Mod1Mask;
unsigned int grabmask2 = Mod1Mask;
unsigned int altmask = ShiftMask;
const char   *opt_term[3] = { DEF_TERM, DEF_TERM, NULL };
int          opt_bw = DEF_BW;
#ifdef SNAP
int          opt_snap = 0;
#endif
#ifdef SOLIDDRAG
int          solid_drag = 1;  /* use solid drag by default */
#endif
Application  *head_app = NULL;

/* Client tracking information */
Client          *head_client = NULL;
Client          *current = NULL;
volatile Window initialising = None;

static void setup_display(void);
static void *xmalloc(size_t size);
static unsigned int parse_modifiers(char *s);

int main(int argc, char *argv[]) {
	struct sigaction act;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-fn") && i+1<argc)
			opt_font = argv[++i];
		else if (!strcmp(argv[i], "-display") && i+1<argc) {
			opt_display = argv[++i];
		}
		else if (!strcmp(argv[i], "-fg") && i+1<argc)
			opt_fg = argv[++i];
		else if (!strcmp(argv[i], "-bg") && i+1<argc)
			opt_bg = argv[++i];
#ifdef VWM
		else if (!strcmp(argv[i], "-fc") && i+1<argc)
			opt_fc = argv[++i];
#endif
		else if (!strcmp(argv[i], "-bw") && i+1<argc)
			opt_bw = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-term") && i+1<argc) {
			opt_term[0] = argv[++i];
			opt_term[1] = opt_term[0];
#ifdef SNAP
		} else if (!strcmp(argv[i], "-snap") && i+1<argc) {
			opt_snap = atoi(argv[++i]);
#endif
		} else if (!strcmp(argv[i], "-app") && i+1<argc) {
			Application *new = xmalloc(sizeof(Application));
			char *tmp;
			i++;
			new->res_name = new->res_class = NULL;
			new->geometry_mask = 0;
#ifdef VWM
			new->vdesk = -1;
			new->sticky = 0;
#endif
			if ((tmp = strchr(argv[i], '/'))) {
				*(tmp++) = 0;
			}
			if (strlen(argv[i]) > 0) {
				new->res_name = xmalloc(strlen(argv[i])+1);
				strcpy(new->res_name, argv[i]);
			}
			if (tmp && strlen(tmp) > 0) {
				new->res_class = xmalloc(strlen(tmp)+1);
				strcpy(new->res_class, tmp);
			}
			new->next = head_app;
			head_app = new;
		} else if (!strcmp(argv[i], "-g") && i+1<argc) {
			i++;
			if (!head_app)
				continue;
			head_app->geometry_mask = XParseGeometry(argv[i],
					&head_app->x, &head_app->y,
					&head_app->width, &head_app->height);
#ifdef VWM
		} else if (!strcmp(argv[i], "-v") && i+1<argc) {
			int v = atoi(argv[++i]);
			if (head_app && valid_vdesk(v))
				head_app->vdesk = v;
		} else if (!strcmp(argv[i], "-s")) {
			if (head_app)
				head_app->sticky = 1;
#endif
		} else if (!strcmp(argv[i], "-mask1") && i+1<argc) {
			i++;
			grabmask1 = parse_modifiers(argv[i]);
		} else if (!strcmp(argv[i], "-mask2") && i+1<argc) {
			i++;
			grabmask2 = parse_modifiers(argv[i]);
		} else if (!strcmp(argv[i], "-altmask") && i+1<argc) {
			i++;
			altmask = parse_modifiers(argv[i]);
#ifdef SOLIDDRAG
		} else if (!strcmp(argv[i], "-nosoliddrag")) {
			solid_drag = 0;
#endif
#ifdef STDIO
		} else if (!strcmp(argv[i], "-V")) {
			LOG_INFO("evilwm version " VERSION "\n");
			exit(0);
#endif
		} else {
			LOG_INFO("usage: evilwm [-display display] [-term termprog] [-fn fontname]\n");
			LOG_INFO("              [-fg foreground]");
#ifdef VWM
			LOG_INFO(" [-fc fixed]");
#endif
			LOG_INFO(" [-bg background] [-bw borderwidth]\n");
			LOG_INFO("              [-mask1 modifiers] [-mask2 modifiers] [-altmask modifiers]\n");
			LOG_INFO("              [-snap num]");
#ifdef VWM
			LOG_INFO(" [-app name/class] [-g geometry] [-v vdesk] [-s]");
#endif
#ifdef SOLIDDRAG
			LOG_INFO("\n              [-nosoliddrag]");
#endif
			LOG_INFO(" [-V]\n");
			exit((!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))?0:1);
		}
	}

	act.sa_handler = handle_signal;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGHUP, &act, NULL);

	setup_display();

	event_main_loop();

	return 1;
}

static void *xmalloc(size_t size) {
	void *ptr = malloc(size);
	if (!ptr) {
		/* C99 defines the 'z' printf modifier for variables of
		 * type size_t.  Fall back to casting to unsigned long. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
		LOG_ERROR("out of memory, looking for %zu bytes\n", size);
#else
		LOG_ERROR("out of memory, looking for %lu bytes\n",
				(unsigned long)size);
#endif
		exit(1);
	}
	return ptr;
}

static void setup_display(void) {
	XGCValues gv;
	XSetWindowAttributes attr;
	XColor dummy;
	XModifierKeymap *modmap;
	/* used in scanning windows (XQueryTree) */
	unsigned int i, j, nwins;
	Window dw1, dw2, *wins;
	XWindowAttributes winattr;

	dpy = XOpenDisplay(opt_display);
	if (!dpy) { 
		LOG_ERROR("can't open display %s\n", opt_display);
		exit(1);
	}
	XSetErrorHandler(handle_xerror);
	/* XSynchronize(dpy, True); */

	/* Standard X protocol atoms */
	xa_wm_state = XInternAtom(dpy, "WM_STATE", False);
	xa_wm_protos = XInternAtom(dpy, "WM_PROTOCOLS", False);
	xa_wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
#ifdef COLOURMAP
	xa_wm_cmapwins = XInternAtom(dpy, "WM_COLORMAP_WINDOWS", False);
#endif
	/* Motif atoms */
	mwm_hints = XInternAtom(dpy, _XA_MWM_HINTS, False);
	/* EWMH atoms */
#ifdef VWM
	xa_net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	xa_net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	xa_net_wm_state_sticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
#endif

	font = XLoadQueryFont(dpy, opt_font);
	if (!font) font = XLoadQueryFont(dpy, DEF_FONT);
	if (!font) {
		LOG_ERROR("couldn't find a font to use: try starting with -fn fontname\n");
		exit(1);
	}

	move_curs = XCreateFontCursor(dpy, XC_fleur);
	resize_curs = XCreateFontCursor(dpy, XC_plus);

	/* find out which modifier is NumLock - we'll use this when grabbing
	 * every combination of modifiers we can think of */
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < (unsigned int)modmap->max_keypermod; j++) {
			if (modmap->modifiermap[i*modmap->max_keypermod+j] == XKeysymToKeycode(dpy, XK_Num_Lock)) {
				numlockmask = (1<<i);
				LOG_DEBUG("setup_display() : XK_Num_Lock is (1<<0x%02x)\n", i);
			}
		}
	}
	XFreeModifiermap(modmap);

	/* set up GC parameters - same for each screen */
	gv.function = GXinvert;
	gv.subwindow_mode = IncludeInferiors;
	gv.line_width = 1;  /* opt_bw */
	gv.font = font->fid;

	/* set up root window attributes - same for each screen */
#ifdef COLOURMAP
	attr.event_mask = ChildMask | EnterWindowMask | ColormapChangeMask;
#else
	attr.event_mask = ChildMask | EnterWindowMask;
#endif

	/* SHAPE extension? */
#ifdef SHAPE
	{
		int e_dummy;
		have_shape = XShapeQueryExtension(dpy, &shape_event, &e_dummy);
	}
#endif

	/* now set up each screen in turn */
	num_screens = ScreenCount(dpy);
	if (num_screens < 0) {
		LOG_ERROR("Can't count screens\n");
		exit(1);
	}
	screens = xmalloc(num_screens * sizeof(ScreenInfo));
	for (i = 0; i < (unsigned int)num_screens; i++) {
		char *ds, *colon, *dot;
		ds = DisplayString(dpy);
		/* set up DISPLAY environment variable to use */
		colon = strrchr(ds, ':');
		if (colon && num_screens > 1) {
			screens[i].display = xmalloc(14 + strlen(ds));
			strcpy(screens[i].display, "DISPLAY=");
			strcat(screens[i].display, ds);
			colon = strrchr(screens[i].display, ':');
			dot = strchr(colon, '.');
			if (!dot)
				dot = colon + strlen(colon);
			snprintf(dot, 5, ".%d", i);
		} else
			screens[i].display = NULL;

		screens[i].screen = i;
		screens[i].root = RootWindow(dpy, i);
#ifdef VWM
		screens[i].vdesk = KEY_TO_VDESK(XK_1);
#endif

		XAllocNamedColor(dpy, DefaultColormap(dpy, i), opt_fg, &screens[i].fg, &dummy);
		XAllocNamedColor(dpy, DefaultColormap(dpy, i), opt_bg, &screens[i].bg, &dummy);
#ifdef VWM
		XAllocNamedColor(dpy, DefaultColormap(dpy, i), opt_fc, &screens[i].fc, &dummy);
#endif

		screens[i].invert_gc = XCreateGC(dpy, screens[i].root, GCFunction | GCSubwindowMode | GCLineWidth | GCFont, &gv);

		XChangeWindowAttributes(dpy, screens[i].root, CWEventMask, &attr);
		grab_keys_for_screen(&screens[i]);

		/* scan all the windows on this screen */
		LOG_XDEBUG("main:XQueryTree(); ");
		XQueryTree(dpy, screens[i].root, &dw1, &dw2, &wins, &nwins);
		LOG_XDEBUG("%d windows\n", nwins);
		for (j = 0; j < nwins; j++) {
			XGetWindowAttributes(dpy, wins[j], &winattr);
			if (!winattr.override_redirect && winattr.map_state == IsViewable)
				make_new_client(wins[j], &screens[i]);
		}
		XFree(wins);
	}
}

/* Used for overriding the default WM modifiers */
static unsigned int parse_modifiers(char *s) {
	static struct {
		const char *name;
		unsigned int mask;
	} modifiers[9] = {
		{ "shift", ShiftMask },
		{ "lock", LockMask },
		{ "control", ControlMask },
		{ "alt", Mod1Mask },
		{ "mod1", Mod1Mask },
		{ "mod2", Mod2Mask },
		{ "mod3", Mod3Mask },
		{ "mod4", Mod4Mask },
		{ "mod5", Mod5Mask }
	};
	char *tmp = strtok(s, ",+");
	unsigned int ret = 0;
	int i;
	if (!tmp)
		return 0;
	do {
		for (i = 0; i < 9; i++) {
			if (!strcmp(modifiers[i].name, tmp))
				ret |= modifiers[i].mask;
		}
		tmp = strtok(NULL, ",+");
	} while (tmp);
	return ret;
}
