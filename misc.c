/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2015 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <X11/Xproto.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "evilwm.h"
#include "log.h"

int need_client_tidy = 0;
int ignore_xerror = 0;

/* Now do this by fork()ing twice so we don't have to worry about SIGCHLDs */
void spawn(const char *const cmd[]) {
	ScreenInfo *current_screen = find_current_screen();
	pid_t pid;

	if (current_screen && current_screen->display)
		putenv(current_screen->display);
	if (!(pid = fork())) {
		setsid();
		switch (fork()) {
			/* execvp()'s prototype is (char *const *) suggesting that it
			 * modifies the contents of the strings.  The prototype is this
			 * way due to SUS maintaining compatability with older code.
			 * However, execvp guarantees not to modify argv, so the following
			 * cast is valid. */
			case 0: execvp(cmd[0], (char *const *)cmd);
			default: _exit(0);
		}
	}
	if (pid > 0)
		wait(NULL);
}

void handle_signal(int signo) {
	(void)signo;  /* unused */
	wm_exit = 1;
}

int handle_xerror(Display *dsply, XErrorEvent *e) {
	Client *c;
	(void)dsply;  /* unused */

	LOG_ENTER("handle_xerror(error=%d, request=%d/%d, resourceid=%lx)", e->error_code, e->request_code, e->minor_code, e->resourceid);

	if (ignore_xerror) {
		LOG_DEBUG("ignoring...\n");
		LOG_LEAVE();
		return 0;
	}
	/* If this error actually occurred while setting up the new
	 * window, best let make_new_client() know not to bother */
	if (initialising != None && e->resourceid == initialising) {
		LOG_DEBUG("error caught while initialising window=%lx\n", initialising);
		initialising = None;
		LOG_LEAVE();
		return 0;
	}
	if (e->error_code == BadAccess && e->request_code == X_ChangeWindowAttributes) {
		LOG_ERROR("root window unavailable (maybe another wm is running?)\n");
		exit(1);
	}

	c = find_client(e->resourceid);
	if (c) {
		LOG_DEBUG("flagging client for removal\n");
		c->remove = 1;
		need_client_tidy = 1;
	} else {
		LOG_DEBUG("unknown error: not handling\n");
	}
	LOG_LEAVE();
	return 0;
}

/* Remove all enter events from the queue except the last of any corresponding
 * to "except"s parent. */
void discard_enter_events(Client *except) {
	XEvent tmp, putback_ev;
	int putback = 0;
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &tmp)) {
		if (tmp.xcrossing.window == except->parent) {
			memcpy(&putback_ev, &tmp, sizeof(XEvent));
			putback = 1;
		}
	}
	if (putback) {
		XPutBackEvent(dpy, &putback_ev);
	}
}
