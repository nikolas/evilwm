/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2006 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdio.h>
#include <stdlib.h>
#include "evilwm.h"
#include "log.h"

static int send_xmessage(Window w, Atom a, long x);

/* used all over the place.  return the client that has specified window as
 * either window or parent */

Client *find_client(Window w) {
	Client *c;

	for (c = head_client; c; c = c->next)
		if (w == c->parent || w == c->window)
			return c;
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

/* Support for 'gravitating' clients based on their original
 * border width and configured window manager frame width. */
void gravitate_client(Client *c, int sign) {
	int d0 = sign * c->border;
	int d1 = sign * c->old_border;
	int d2 = sign * (2*c->old_border - c->border);
	switch (c->win_gravity) {
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
		if (is_sticky(c))
			bpixel = c->screen->fc.pixel;
		else
#endif
			bpixel = c->screen->fg.pixel;
		XSetWindowBorder(dpy, c->parent, bpixel);
#ifdef COLOURMAP
		XInstallColormap(dpy, c->cmap);
#endif
		XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
	}
	current = c;
}

#ifdef VWM
void fix_client(Client *c) {
	toggle_sticky(c);
	select_client(c);
	update_net_wm_state(c);
}
#endif

void remove_client(Client *c) {
	Client *p;

	LOG_DEBUG("remove_client() : Removing...\n");

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
		LOG_DEBUG("\tremove_client() : setting WithdrawnState\n");
		set_wm_state(c, WithdrawnState);
#ifdef VWM
		XDeleteProperty(dpy, c->window, xa_net_wm_desktop);
		XDeleteProperty(dpy, c->window, xa_net_wm_state);
#endif
	}

	ungravitate(c);
	XReparentWindow(dpy, c->window, c->screen->root, c->x, c->y);
	XSetWindowBorderWidth(dpy, c->window, c->old_border);
	XRemoveFromSaveSet(dpy, c->window);
	if (c->parent)
		XDestroyWindow(dpy, c->parent);

	if (head_client == c) head_client = c->next;
	else for (p = head_client; p && p->next; p = p->next)
		if (p->next == c) p->next = c->next;

	if (current == c)
		current = NULL;  /* an enter event should set this up again */
	free(c);
#ifdef DEBUG
	{
		Client *pp;
		int i = 0;
		for (pp = head_client; pp; pp = pp->next)
			i++;
		LOG_DEBUG("\tremove_client() : free(), window count now %d\n", i);
	}
#endif

	XUngrabServer(dpy);
	XSync(dpy, False);
	ignore_xerror = 0;
	LOG_DEBUG("remove_client() returning\n");
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
