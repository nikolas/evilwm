/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2005 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xatom.h>
#include "evilwm.h"
#include "log.h"

void handle_key_event(XKeyEvent *e) {
	Client *c = find_client(e->window);
	KeySym key = XKeycodeToKeysym(dpy,e->keycode,0);

	if (!c)
		c = current;
	if (c) {
		switch (key) {
		/* Sorry about all these if (0)s, but they actually
		 * *do* do something useful... */
		case KEY_LEFT:
			c->x -= 16; if (0)
		case KEY_DOWN:
			c->y += 16; if (0)
		case KEY_UP:
			c->y -= 16; if (0)
		case KEY_RIGHT:
			c->x += 16; if (0)
		case KEY_TOPLEFT:
			{ c->x = c->border; c->y = c->border; } if (0)
		case KEY_TOPRIGHT:
			{ c->x = DisplayWidth(dpy, c->screen->screen)-c->width-c->border; c->y = c->border; } if (0)
		case KEY_BOTTOMLEFT:
			{ c->x = c->border; c->y = DisplayHeight(dpy, c->screen->screen)-c->height-c->border; } if (0)
		case KEY_BOTTOMRIGHT:
			{ c->x = DisplayWidth(dpy, c->screen->screen)-c->width-c->border; c->y = DisplayHeight(dpy, c->screen->screen)-c->height-c->border; }
			moveresize(c);
			setmouse(c->window, c->width + c->border - 1,
					c->height + c->border - 1);
			/* Need to think about this - see note about shaped
			 * windows in TODO */
			break;
		case KEY_KILL:
			send_wm_delete(c); break;
		case KEY_LOWER: case KEY_ALTLOWER:
			XLowerWindow(dpy, c->parent);
			break;
		case KEY_INFO:
			show_info(c, key);
			break;
		case KEY_MAX:
			maximise_horiz(c);
		case KEY_MAXVERT:
			maximise_vert(c);
			moveresize(c);
			setmouse(c->window, c->width + c->border - 1,
					c->height + c->border - 1);
			break;
#ifdef VWM
		case KEY_FIX:
			c->vdesk = c->vdesk == STICKY ? vdesk : STICKY;
			client_update_current(c, current);
			break;
#endif
		}
	}
	switch(key) {
		case KEY_NEW:
			spawn(opt_term); break;
		case KEY_NEXT:
			next();
			break;
#ifdef VWM
		case XK_1: case XK_2: case XK_3: case XK_4:
		case XK_5: case XK_6: case XK_7: case XK_8:
			switch_vdesk(KEY_TO_VDESK(key));
			break;
		case KEY_PREVDESK:
			if (vdesk > KEY_TO_VDESK(XK_1))
				switch_vdesk(vdesk - 1);
			break;
		case KEY_NEXTDESK:
			if (vdesk < KEY_TO_VDESK(XK_8))
				switch_vdesk(vdesk + 1);
			break;
#endif
	}
}

#ifdef MOUSE
void handle_button_event(XButtonEvent *e) {
	Client *c = find_client(e->window);

	if (c && e->window != c->screen->root) {
		switch (e->button) {
			case Button1:
				drag(c); break;
			case Button2:
				sweep(c); break;
			case Button3:
				XLowerWindow(dpy, c->parent); break;
		}
	}
}
#endif

void handle_configure_request(XConfigureRequestEvent *e) {
	Client *c = find_client(e->window);
	XWindowChanges wc;

	wc.sibling = e->above;
	wc.stack_mode = e->detail;
	if (c) {
		ungravitate(c);
		if (e->value_mask & CWWidth) c->width = e->width;
		if (e->value_mask & CWHeight) c->height = e->height;
		if (e->value_mask & CWX) c->x = e->x;
		if (e->value_mask & CWY) c->y = e->y;
		if (c->x == 0 && c->width >= DisplayWidth(dpy, c->screen->screen)) {
			c->x -= c->border;
		}
		if (c->y == 0 && c->height >= DisplayHeight(dpy, c->screen->screen)) {
			c->y -= c->border;
		}
		gravitate(c);

		wc.x = c->x - c->border;
		wc.y = c->y - c->border;
		wc.width = c->width + (c->border*2);
		wc.height = c->height + (c->border*2);
		wc.border_width = 0;
		XConfigureWindow(dpy, c->parent, e->value_mask, &wc);
		send_config(c);
		LOG_DEBUG("handle_configure_request() : window configured to %dx%d+%d+%d\n", c->width, c->height, c->x, c->y);
	}

	wc.x = c ? c->border : e->x;
	wc.y = c ? c->border : e->y;
	wc.width = e->width;
	wc.height = e->height;
	XConfigureWindow(dpy, e->window, e->value_mask, &wc);
}

void handle_map_request(XMapRequestEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
#ifdef VWM
		if (c->vdesk != vdesk)
			switch_vdesk(c->vdesk);
#endif
		unhide(c, RAISE);
	} else {
		XWindowAttributes attr;
		LOG_DEBUG("handle_map_request() : don't know this window, calling make_new_client();\n");
		XGetWindowAttributes(dpy, e->window, &attr);
		make_new_client(e->window, find_screen(attr.root));
	}
}

void handle_unmap_event(XUnmapEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
		if (c->ignore_unmap) c->ignore_unmap--;
		else remove_client(c);
	}
}

void handle_reparent_event(XReparentEvent *e) {
	Client *c = find_client(e->window);
	/* If an unmanaged window is reparented such that its new parent is
	 * a root window and it is not in WithdrawnState, manage it */
	if (!c) {
		ScreenInfo *s = find_screen(e->parent);
		if (s) {
			XWindowAttributes attr;
			XGetWindowAttributes(dpy, e->window, &attr);
			if (attr.map_state != WithdrawnState) {
				LOG_DEBUG("handle_reparent_event(): window reparented to root - making client\n");
				make_new_client(e->window, s);
			}
		}
	}
}

#ifdef COLOURMAP
void handle_colormap_change(XColormapEvent *e) {
	Client *c = find_client(e->window);

	if (c && e->new) {
		c->cmap = e->colormap;
		XInstallColormap(dpy, c->cmap);
	}
}
#endif

void handle_property_change(XPropertyEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
		if (e->atom == XA_WM_NORMAL_HINTS) {
			get_wm_normal_hints(c);
		}
	}
}

void handle_enter_event(XCrossingEvent *e) {
	Client *c;

	current_screen = find_screen(e->root);
	if ((c = find_client(e->window))) {
#ifdef VWM
		if (c->vdesk != vdesk && c->vdesk != STICKY)
			return;
#endif
		select_client(c);
#ifdef MOUSE
		grab_button(c->parent, grabmask2, AnyButton);
#endif
	}
}

#ifdef SHAPE
void handle_shape_event(XShapeEvent *e) {
	Client *c = find_client(e->window);
	if (c)
		set_shape(c);
}
#endif
