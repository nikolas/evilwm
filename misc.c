/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2005 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <X11/Xproto.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include "evilwm.h"
#include "log.h"

/* Now do this by fork()ing twice so we don't have to worry about SIGCHLDs */
void spawn(const char *const cmd[]) {
	pid_t pid;

	if (current_screen && current_screen->display)
		putenv(current_screen->display);
	if (!(pid = fork())) {
		setsid();
		switch (fork()) {
			/* explicitly hack around broken SUS execvp prototype */
			case 0: execvp(cmd[0], (char *const *)&cmd[1]);
			default: _exit(0);
		}
	}
	if (pid > 0)
		wait(NULL);
}

#ifdef VDESK_BOTH
void spawn_vdesk(int todesk, Client *c) {
	char *vdesk_cmd[5];
	char vdesk_num[2];
	char vdesk_window[11];

	vdesk_cmd[3] = vdesk_cmd[4] = NULL;
	vdesk_cmd[0] = "vdesk";
	vdesk_cmd[1] = "vdesk";
	vdesk_num[0] = todesk + '0';
	vdesk_num[1] = 0;
	vdesk_cmd[2] = vdesk_num;
	if (c) {
		snprintf(vdesk_window, sizeof(vdesk_window), "%ld", c->window);
		vdesk_cmd[3] = vdesk_window;
	}
	LOG_DEBUG("%s %s %s %s\n", vdesk_cmd[0], vdesk_cmd[1], vdesk_cmd[2], vdesk_cmd[3]);
	spawn(vdesk_cmd);
}
#endif

void handle_signal(int signo) {
	int i;
	/* SIGCHLD check no longer necessary */
	/* Quit Nicely */
	quitting = 1;
	while(head_client) remove_client(head_client);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	if (font) XFreeFont(dpy, font);
	for (i = 0; i < num_screens; i++) {
		XFreeGC(dpy, screens[i].invert_gc);
		XInstallColormap(dpy, DefaultColormap(dpy, screens[i].screen));
	}
	free(screens);
	XCloseDisplay(dpy);
	exit(0);
}

int handle_xerror(Display *dsply, XErrorEvent *e) {
	Client *c = find_client(e->resourceid);

	/* If this error actually occurred while setting up the new
	 * window, best let make_new_client() know not to bother */
	if (initialising != None && e->resourceid == initialising) {
		LOG_DEBUG("\t **SAVED?** handle_xerror() caught error %d while initialising\n", e->error_code);
		initialising = None;
		return 0;
	}
	LOG_DEBUG("**ERK** handle_xerror() caught an XErrorEvent: %d\n", e->error_code);
	/* if (e->error_code == BadAccess && e->resourceid == root) { */
	if (e->error_code == BadAccess && e->request_code == X_ChangeWindowAttributes) {
		LOG_ERROR("root window unavailable (maybe another wm is running?)\n");
		exit(1);
	}
	LOG_XDEBUG("XError %x ", e->error_code);
	/* Kludge around IE misbehaviour */
	if (e->error_code == 0x8 && e->request_code == 0x0c && e->minor_code == 0x00) {
		LOG_DEBUG("\thandle_xerror() : IE kludge - ignoring XError\n");
		return 0;
	}

	if (c) {
		LOG_DEBUG("\thandle_xerror() : calling remove_client()\n");
		remove_client(c);
	}
	return 0;
}

int ignore_xerror(Display *dsply, XErrorEvent *e) {
	LOG_DEBUG("ignore_xerror() caught an XErrorEvent: %d\n", e->error_code);
	return 0;
}

#ifdef DEBUG
void dump_clients() {
	Client *c;
	XWindowAttributes attr;

	for (c = head_client; c; c = c->next) {
		XGetWindowAttributes(dpy, c->window, &attr);
		LOG_DEBUG("MISC: (%d, %d) @ %d,%d\n", attr.map_state,
			c->ignore_unmap, c->x, c->y);
	}
}
#endif /* DEBUG */
