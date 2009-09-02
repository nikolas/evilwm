/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2009 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include <unistd.h>
#include "evilwm.h"
#include "log.h"

/* Root Window Properties (and Related Messages) */
static Atom xa_net_supported;
static Atom xa_net_supporting_wm_check;
Atom xa_net_request_frame_extents;

/* Application Window Properties */
static Atom xa_net_wm_name;
#ifdef VWM
Atom xa_net_wm_desktop;
Atom xa_net_wm_state;
Atom xa_net_wm_state_sticky;
#endif
static Atom xa_net_wm_pid;
Atom xa_net_frame_extents;

void ewmh_init(void) {
	/* Root Window Properties (and Related Messages) */
	xa_net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
	xa_net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	xa_net_request_frame_extents = XInternAtom(dpy, "_NET_REQUEST_FRAME_EXTENTS", False);

	/* Application Window Properties */
	xa_net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
#ifdef VWM
	xa_net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	xa_net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	xa_net_wm_state_sticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
#endif
	xa_net_wm_pid = XInternAtom(dpy, "_NET_WM_PID", False);
	xa_net_frame_extents = XInternAtom(dpy, "_NET_FRAME_EXTENTS", False);
}

void ewmh_init_screen(ScreenInfo *s) {
	unsigned long pid = getpid();
	Atom supported[] = {
		xa_net_supporting_wm_check,
		xa_net_request_frame_extents,

#ifdef VWM
		xa_net_wm_desktop,
		xa_net_wm_state,
		xa_net_wm_state_sticky,
#endif
		xa_net_frame_extents,
	};
	s->supporting = XCreateSimpleWindow(dpy, s->root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, s->root, xa_net_supported,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&supported,
			sizeof(supported) / sizeof(Atom));
	XChangeProperty(dpy, s->root, xa_net_supporting_wm_check,
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *)&s->supporting, 1);
	XChangeProperty(dpy, s->supporting, xa_net_supporting_wm_check,
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *)&s->supporting, 1);
	XChangeProperty(dpy, s->supporting, xa_net_wm_name,
			XA_STRING, 8, PropModeReplace,
			(const unsigned char *)"evilwm", 6);
	XChangeProperty(dpy, s->supporting, xa_net_wm_pid,
			XA_CARDINAL, 32, PropModeReplace,
			(unsigned char *)&pid, 1);
}

void ewmh_deinit_screen(ScreenInfo *s) {
	XDeleteProperty(dpy, s->root, xa_net_supported);
	XDeleteProperty(dpy, s->root, xa_net_supporting_wm_check);
	XDestroyWindow(dpy, s->supporting);
}

#ifdef VWM
void ewmh_set_net_wm_desktop(Client *c) {
	XChangeProperty(dpy, c->window, xa_net_wm_desktop,
			XA_CARDINAL, 32, PropModeReplace,
			(unsigned char *)&c->vdesk, 1);
}

void ewmh_set_net_wm_state(Client *c) {
	Atom state[1];
	int i = 0;
	if (is_sticky(c))
		state[i++] = xa_net_wm_state_sticky;
	XChangeProperty(dpy, c->window, xa_net_wm_state,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&state, i);
}
#endif

void ewmh_set_net_frame_extents(Window w) {
	unsigned long extents[4];
	extents[0] = extents[1] = extents[2] = extents[3] = opt_bw;
	XChangeProperty(dpy, w, xa_net_frame_extents,
			XA_CARDINAL, 32, PropModeReplace,
			(unsigned char *)&extents, 4);
}
