/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2009 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdio.h>
#include <stdlib.h>
#include "evilwm.h"
#include "log.h"

static int send_xmessage(Window w, Atom a, long x);

/* used all over the place.  return the client that has specified window as
 * either window or parent */

Client *find_client(Window w) {
	struct list *iter;

	for (iter = clients_tab_order; iter; iter = iter->next) {
		Client *c = iter->data;
		if (w == c->parent || w == c->window)
			return c;
	}
	return NULL;
}

void set_wm_state(Client *c, int state) {
	/* Using "long" for the type of "data" looks wrong, but the
	 * fine people in the X Consortium defined it this way
	 * (even on 64-bit machines).
	 */
	long data[2];
	data[0] = state;
	data[1] = None;
	XChangeProperty(dpy, c->window, xa_wm_state, xa_wm_state, 32,
			PropModeReplace, (unsigned char *)data, 2);
}

void send_config(Client *c) {
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.event = c->window;
	ce.window = c->window;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->width;
	ce.height = c->height;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = False;

	XSendEvent(dpy, c->window, False, StructureNotifyMask, (XEvent *)&ce);
}

/* Support for 'gravitating' clients based on their original border width and
 * configured window manager frame width.  If the 'gravity' arg is non-zero,
 * the win_gravity hint read when the client was initialised is used.  Set sign
 * = 1 to gravitate or sign = -1 to reverse the process. */
void gravitate_client(Client *c, int sign, int gravity) {
	int d0 = sign * c->border;
	int d1 = sign * c->old_border;
	int d2 = sign * (2*c->old_border - c->border);
	if (gravity == 0)
		gravity = c->win_gravity_hint;
	c->win_gravity = gravity;
	switch (gravity) {
		case NorthGravity:
			c->x += d1;
			c->y += d0;
			break;
		case NorthEastGravity:
			c->x += d2;
			c->y += d0;
			break;
		case EastGravity:
			c->x += d2;
			c->y += d1;
			break;
		case SouthEastGravity:
			c->x += d2;
			c->y += d2;
			break;
		case SouthGravity:
			c->x += d1;
			c->y += d2;
			break;
		case SouthWestGravity:
			c->x += d0;
			c->y += d2;
			break;
		case WestGravity:
			c->x += d0;
			c->y += d1;
			break;
		case NorthWestGravity:
		default:
			c->x += d0;
			c->y += d0;
			break;
	}
}

void select_client(Client *c) {
	if (current)
		XSetWindowBorder(dpy, current->parent, current->screen->bg.pixel);
	if (c) {
		unsigned long bpixel;
#ifdef VWM
		if (is_fixed(c))
			bpixel = c->screen->fc.pixel;
		else
#endif
			bpixel = c->screen->fg.pixel;
		XSetWindowBorder(dpy, c->parent, bpixel);
		XInstallColormap(dpy, c->cmap);
		XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
	}
	current = c;
	ewmh_set_net_active_window(c);
}

#ifdef VWM
void client_to_vdesk(Client *c, unsigned int vdesk) {
	if (valid_vdesk(vdesk)) {
		c->vdesk = vdesk;
		if (c->vdesk == c->screen->vdesk || c->vdesk == VDESK_FIXED) {
			unhide(c, NO_RAISE);
		} else {
			hide(c);
		}
		ewmh_set_net_wm_desktop(c);
		select_client(current);
	}
}
#endif

void remove_client(Client *c) {
	LOG_ENTER("remove_client(window=%lx, %s)", c->window, c->remove ? "withdrawing" : "wm quitting");

	XGrabServer(dpy);
	ignore_xerror = 1;

	/* ICCCM 4.1.3.1
	 * "When the window is withdrawn, the window manager will either
	 *  change the state field's value to WithdrawnState or it will
	 *  remove the WM_STATE property entirely."
	 * EWMH 1.3
	 * "The Window Manager should remove the property whenever a
	 *  window is withdrawn but it should leave the property in
	 *  place when it is shutting down." (both _NET_WM_DESKTOP and
	 *  _NET_WM_STATE) */
	if (c->remove) {
		LOG_DEBUG("setting WithdrawnState\n");
		set_wm_state(c, WithdrawnState);
		ewmh_withdraw_client(c);
	} else {
		ewmh_deinit_client(c);
	}

	ungravitate(c);
	XReparentWindow(dpy, c->window, c->screen->root, c->x, c->y);
	XSetWindowBorderWidth(dpy, c->window, c->old_border);
	XRemoveFromSaveSet(dpy, c->window);
	if (c->parent)
		XDestroyWindow(dpy, c->parent);

	clients_tab_order = list_delete(clients_tab_order, c);
	clients_mapping_order = list_delete(clients_mapping_order, c);
	clients_stacking_order = list_delete(clients_stacking_order, c);
	/* If the wm is quitting, we'll remove the client list properties
	 * soon enough, otherwise: */
	if (c->remove) {
		ewmh_set_net_client_list(c->screen);
	}

	if (current == c)
		current = NULL;  /* an enter event should set this up again */
	free(c);
#ifdef DEBUG
	{
		struct list *iter;
		int i = 0;
		for (iter = clients_tab_order; iter; iter = iter->next)
			i++;
		LOG_DEBUG("free(), window count now %d\n", i);
	}
#endif

	XUngrabServer(dpy);
	XSync(dpy, False);
	ignore_xerror = 0;
	LOG_LEAVE();
}

void send_wm_delete(Client *c, int kill_client) {
	int i, n, found = 0;
	Atom *protocols;

	if (!kill_client && XGetWMProtocols(dpy, c->window, &protocols, &n)) {
		for (i = 0; i < n; i++)
			if (protocols[i] == xa_wm_delete)
				found++;
		XFree(protocols);
	}
	if (found)
		send_xmessage(c->window, xa_wm_protos, xa_wm_delete);
	else
		XKillClient(dpy, c->window);
}

static int send_xmessage(Window w, Atom a, long x) {
	XEvent ev;

	ev.type = ClientMessage;
	ev.xclient.window = w;
	ev.xclient.message_type = a;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = x;
	ev.xclient.data.l[1] = CurrentTime;

	return XSendEvent(dpy, w, False, NoEventMask, &ev);
}

#ifdef SHAPE
void set_shape(Client *c) {
	int bounding_shaped;
	int i, b;  unsigned int u;  /* dummies */

	if (!have_shape) return;
	/* Logic to decide if we have a shaped window cribbed from fvwm-2.5.10.
	 * Previous method (more than one rectangle returned from
	 * XShapeGetRectangles) worked _most_ of the time. */
	if (XShapeQueryExtents(dpy, c->window, &bounding_shaped, &i, &i,
				&u, &u, &b, &i, &i, &u, &u) && bounding_shaped) {
		LOG_DEBUG("%d shape extents\n", bounding_shaped);
		XShapeCombineShape(dpy, c->parent, ShapeBounding, 0, 0,
				c->window, ShapeBounding, ShapeSet);
	}
}
#endif
