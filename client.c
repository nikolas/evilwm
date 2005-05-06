/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2005 Ciaran Anscomb <evilwm@6809.org.uk>
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
#ifdef COLOURMAP
	XInstallColormap(dpy, c->cmap);
#endif
	client_update_current(current, c);
	client_update_current(c, current);
	XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
}

void remove_client(Client *c) {
	Client *p;

	LOG_DEBUG("remove_client() : Removing...\n");

	XGrabServer(dpy);
	XSetErrorHandler(ignore_xerror);

	if (!quitting) {
		LOG_DEBUG("\tremove_client() : setting WithdrawnState\n");
		set_wm_state(c, WithdrawnState);
		XRemoveFromSaveSet(dpy, c->window);
	}

	ungravitate(c);
	/* Restore window's original border width */
	XSetWindowBorderWidth(dpy, c->window, c->old_border);
	XReparentWindow(dpy, c->window, c->screen->root, c->x, c->y);
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

	XSync(dpy, False);
	XSetErrorHandler(handle_xerror);
	XUngrabServer(dpy);
}

void send_wm_delete(Client *c) {
	int i, n, found = 0;
	Atom *protocols;

	if (c) {
		if (XGetWMProtocols(dpy, c->window, &protocols, &n)) {
			for (i=0; i<n; i++) if (protocols[i] == xa_wm_delete) found++;
			XFree(protocols);
		}
		if (found) send_xmessage(c->window, xa_wm_protos, xa_wm_delete);
		else XKillClient(dpy, c->window);
	}
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
	int n, order;
	XRectangle *rect;

	if (!have_shape) return;
	rect = XShapeGetRectangles(dpy, c->window, ShapeBounding, &n, &order);
	if (n > 1)
		XShapeCombineShape(dpy, c->parent, ShapeBounding, c->border,
				c->border, c->window, ShapeBounding, ShapeSet);
	XFree((void *)rect);
}
#endif

void client_update_current(Client *c, Client *newcurrent) {
	if (c) {
		unsigned long bpixel;
#ifdef VWM
		if (c->vdesk == STICKY)
			bpixel = c == newcurrent ? c->screen->fc.pixel : c->screen->bg.pixel;
		else
#endif
			bpixel = c == newcurrent ? c->screen->fg.pixel : c->screen->bg.pixel;
		XSetWindowBackground(dpy, c->parent, bpixel);
		XClearWindow(dpy, c->parent);
	}
	current = newcurrent;
}
