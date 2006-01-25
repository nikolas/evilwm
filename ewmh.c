/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2006 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include "evilwm.h"
#include "log.h"

#ifdef VWM
void update_net_wm_desktop(Client *c) {
	XChangeProperty(dpy, c->window, xa_net_wm_desktop,
			XA_CARDINAL, 32, PropModeReplace,
			(unsigned char *)&c->vdesk, 1);
}

void update_net_wm_state(Client *c) {
	Atom state[1];
	int i = 0;
	if (is_sticky(c))
		state[i++] = xa_net_wm_state_sticky;
	XChangeProperty(dpy, c->window, xa_net_wm_state,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&state, i);
}
#endif
