/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2009 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <X11/cursorfont.h>
#include "evilwm.h"
#include "log.h"
#include "xconfig.h"

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
#ifdef RANDR
int         have_randr, randr_event_base;
#endif

/* Standard X protocol atoms */
Atom xa_wm_state;
Atom xa_wm_protos;
Atom xa_wm_delete;
Atom xa_wm_cmapwins;

/* Motif atoms */
Atom mwm_hints;

/* evilwm atoms */
Atom xa_evilwm_unmaximised_horz;
Atom xa_evilwm_unmaximised_vert;

/* Things that affect user interaction */
#define CONFIG_FILE ".evilwmrc"
static const char   *opt_display = "";
static const char   *opt_font = DEF_FONT;
static const char   *opt_fg = DEF_FG;
static const char   *opt_bg = DEF_BG;
#ifdef VWM
static const char   *opt_fc = DEF_FC;
#endif
static char *opt_grabmask1 = NULL;
static char *opt_grabmask2 = NULL;
static char *opt_altmask = NULL;
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
int          no_solid_drag = 0;  /* use solid drag by default */
#endif
Application  *head_app = NULL;

/* Client tracking information */
Client          *head_client = NULL;
Client          *current = NULL;
volatile Window initialising = None;

/* Event loop will run until this flag is set */
int wm_exit;

static void set_app(const char *arg);
static void set_app_geometry(const char *arg);
#ifdef VWM
static void set_app_vdesk(const char *arg);
static void set_app_sticky(void);
#endif

static struct xconfig_option evilwm_options[] = {
	{ XCONFIG_STRING,   "fn",           &opt_font },
	{ XCONFIG_STRING,   "display",      &opt_display },
	{ XCONFIG_STRING,   "fg",           &opt_fg },
	{ XCONFIG_STRING,   "bg",           &opt_bg },
#ifdef VWM
	{ XCONFIG_STRING,   "fc",           &opt_fc },
#endif
	{ XCONFIG_STRING,   "bw",           &opt_bw },
	{ XCONFIG_STRING,   "term",         &opt_term[0] },
#ifdef SNAP
	{ XCONFIG_INT,      "snap",         &opt_snap },
#endif
	{ XCONFIG_STRING,   "mask1",        &opt_grabmask1 },
	{ XCONFIG_STRING,   "mask2",        &opt_grabmask2 },
	{ XCONFIG_STRING,   "altmask",      &opt_altmask },
	{ XCONFIG_CALL_1,   "app",          &set_app },
	{ XCONFIG_CALL_1,   "geometry",     &set_app_geometry },
	{ XCONFIG_CALL_1,   "g",            &set_app_geometry },
#ifdef VWM
	{ XCONFIG_CALL_1,   "vdesk",        &set_app_vdesk },
	{ XCONFIG_CALL_1,   "v",            &set_app_vdesk },
	{ XCONFIG_CALL_0,   "sticky",       &set_app_sticky },
	{ XCONFIG_CALL_0,   "s",            &set_app_sticky },
#endif
#ifdef SOLIDDRAG
	{ XCONFIG_BOOL,     "nosoliddrag",  &no_solid_drag },
#endif
	{ XCONFIG_END, NULL, NULL }
};

static void setup_display(void);
static void *xmalloc(size_t size);
static unsigned int parse_modifiers(char *s);

#ifdef STDIO
static void helptext(void) {
	puts(
"usage: evilwm [-display display] [-term termprog] [-fn fontname]\n"
"              [-fg foreground]"
#ifdef VWM
" [-fc fixed]"
#endif
" [-bg background] [-bw borderwidth]\n"
"              [-mask1 modifiers] [-mask2 modifiers] [-altmask modifiers]\n"
"              [-snap num]"
#ifdef VWM
" [-app name/class] [-g geometry] [-v vdesk] [-s]"
#endif
#ifdef SOLIDDRAG
"\n              [-nosoliddrag]"
#endif
" [-V]"
	);
}
#else
#define helptext()
#endif

int main(int argc, char *argv[]) {
	struct sigaction act;
	int argn = 1, ret;

	{
		const char *home = getenv("HOME");
		if (home) {
			char *conffile = xmalloc(strlen(home) + sizeof(CONFIG_FILE) + 2);
			strcpy(conffile, home);
			strcat(conffile, "/" CONFIG_FILE);
			xconfig_parse_file(evilwm_options, conffile);
			free(conffile);
		}
	}
	ret = xconfig_parse_cli(evilwm_options, argc, argv, &argn);
	if (ret == XCONFIG_MISSING_ARG) {
		fprintf(stderr, "%s: missing argument to `%s'\n", argv[0], argv[argn]);
		exit(1);
	} else if (ret == XCONFIG_BAD_OPTION) {
		if (0 == strcmp(argv[argn], "-h")
				|| 0 == strcmp(argv[argn], "--help")) {
			helptext();
			exit(0);
#ifdef STDIO
		} else if (0 == strcmp(argv[argn], "-V")
				|| 0 == strcmp(argv[argn], "--version")) {
			LOG_INFO("evilwm version " VERSION "\n");
			exit(0);
#endif
		} else {
			helptext();
			exit(1);
		}
	}

	opt_term[1] = opt_term[0];
	if (opt_grabmask1) grabmask1 = parse_modifiers(opt_grabmask1);
	if (opt_grabmask2) grabmask2 = parse_modifiers(opt_grabmask2);
	if (opt_altmask) altmask = parse_modifiers(opt_altmask);

	wm_exit = 0;
	act.sa_handler = handle_signal;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGHUP, &act, NULL);

	setup_display();

	event_main_loop();

	/* Quit Nicely */
	while(head_client) remove_client(head_client);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	if (font) XFreeFont(dpy, font);
	{
		int i;
		for (i = 0; i < num_screens; i++) {
			ewmh_deinit_screen(&screens[i]);
			XFreeGC(dpy, screens[i].invert_gc);
			XInstallColormap(dpy, DefaultColormap(dpy, i));
		}
	}
	free(screens);
	XCloseDisplay(dpy);

	return 0;
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
	/* evilwm atoms */
	xa_evilwm_unmaximised_horz = XInternAtom(dpy, "_EVILWM_UNMAXIMISED_HORZ", False);
	xa_evilwm_unmaximised_vert = XInternAtom(dpy, "_EVILWM_UNMAXIMISED_VERT", False);
	/* EWMH atoms */
	ewmh_init();

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
	/* Xrandr extension? */
#ifdef RANDR
	{
		int e_dummy;
		have_randr = XRRQueryExtension(dpy, &randr_event_base, &e_dummy);
#ifdef DEBUG
		if (!have_randr)
			LOG_DEBUG("Xrandr is not supported on this display.\n");
#endif
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
#ifdef RANDR
		if (have_randr) {
			XRRSelectInput(dpy, screens[i].root, RRScreenChangeNotifyMask);
		}
#endif
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
		ewmh_init_screen(&screens[i]);
	}
}

/**************************************************************************/
/* Option parsing callbacks */

static void set_app(const char *arg) {
	Application *new = xmalloc(sizeof(Application));
	char *tmp;
	new->res_name = new->res_class = NULL;
	new->geometry_mask = 0;
#ifdef VWM
	new->vdesk = -1;
	new->sticky = 0;
#endif
	if ((tmp = strchr(arg, '/'))) {
		*(tmp++) = 0;
	}
	if (strlen(arg) > 0) {
		new->res_name = xmalloc(strlen(arg)+1);
		strcpy(new->res_name, arg);
	}
	if (tmp && strlen(tmp) > 0) {
		new->res_class = xmalloc(strlen(tmp)+1);
		strcpy(new->res_class, tmp);
	}
	new->next = head_app;
	head_app = new;
}

static void set_app_geometry(const char *arg) {
	if (!head_app)
		return;
	head_app->geometry_mask = XParseGeometry(arg,
			&head_app->x, &head_app->y,
			&head_app->width, &head_app->height);
}

#ifdef VWM
static void set_app_vdesk(const char *arg) {
	int v = atoi(arg);
	if (head_app && valid_vdesk(v))
		head_app->vdesk = v;
}

static void set_app_sticky(void) {
	if (head_app)
		head_app->sticky = 1;
}
#endif

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
