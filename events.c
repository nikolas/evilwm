/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2005 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include <X11/Xatom.h>
#include "evilwm.h"
#include "log.h"

static void handle_key_event(XKeyEvent *e) {
	Client *c = find_client(e->window);
	KeySym key = XKeycodeToKeysym(dpy,e->keycode,0);

	if (!c)
		c = current;
	if (c) {
		switch (key) {
		case KEY_LEFT:
			c->x -= 16;
			goto move_client;
		case KEY_DOWN:
			c->y += 16;
			goto move_client;
		case KEY_UP:
			c->y -= 16;
			goto move_client;
		case KEY_RIGHT:
			c->x += 16;
			goto move_client;
		case KEY_TOPLEFT:
			c->x = c->border;
			c->y = c->border;
			goto move_client;
		case KEY_TOPRIGHT:
			c->x = DisplayWidth(dpy, c->screen->screen)
				- c->width-c->border;
			c->y = c->border;
			goto move_client;
		case KEY_BOTTOMLEFT:
			c->x = c->border;
			c->y = DisplayHeight(dpy, c->screen->screen)
				- c->height-c->border;
			goto move_client;
		case KEY_BOTTOMRIGHT:
			c->x = DisplayWidth(dpy, c->screen->screen)
				- c->width-c->border;
			c->y = DisplayHeight(dpy, c->screen->screen)
				- c->height-c->border;
			goto move_client;
		case KEY_KILL:
			if (e->state & ShiftMask)
				XKillClient(dpy, c->window);
			else
				send_wm_delete(c);
			break;
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
			goto move_client;
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
			spawn(opt_term);
			break;
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
	return;
move_client:
	moveresize(c);
	set_mouse_corner(c);
	return;
}

#ifdef MOUSE
static void handle_button_event(XButtonEvent *e) {
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

static void handle_configure_request(XConfigureRequestEvent *e) {
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

static void handle_map_request(XMapRequestEvent *e) {
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

static void handle_unmap_event(XUnmapEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
		if (c->ignore_unmap) {
			c->ignore_unmap--;
		} else {
			LOG_DEBUG("handle_unmap_event(): flagging client for removal\n");
			c->remove = 1;
			need_client_tidy = 1;
		}
	}
}

#ifdef COLOURMAP
static void handle_colormap_change(XColormapEvent *e) {
	Client *c = find_client(e->window);

	if (c && e->new) {
		c->cmap = e->colormap;
		XInstallColormap(dpy, c->cmap);
	}
}
#endif

static void handle_property_change(XPropertyEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
		if (e->atom == XA_WM_NORMAL_HINTS) {
			get_wm_normal_hints(c);
		}
	}
}

static void handle_enter_event(XCrossingEvent *e) {
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
static void handle_shape_event(XShapeEvent *e) {
	Client *c = find_client(e->window);
	if (c)
		set_shape(c);
}
#endif

void event_main_loop(void) {
	XEvent ev;
	/* main event loop here */
	for (;;) {
		XNextEvent(dpy, &ev);
		switch (ev.type) {
			case KeyPress:
				handle_key_event(&ev.xkey); break;
#ifdef MOUSE
			case ButtonPress:
				handle_button_event(&ev.xbutton); break;
#endif
			case ConfigureRequest:
				handle_configure_request(&ev.xconfigurerequest); break;
			case MapRequest:
				handle_map_request(&ev.xmaprequest); break;
#ifdef COLOURMAP
			case ColormapNotify:
				handle_colormap_change(&ev.xcolormap); break;
#endif
			case EnterNotify:
				handle_enter_event(&ev.xcrossing); break;
			case PropertyNotify:
				handle_property_change(&ev.xproperty); break;
			case UnmapNotify:
				handle_unmap_event(&ev.xunmap); break;
#ifdef SHAPE
			default:
				if (have_shape && ev.type == shape_event) {
					handle_shape_event((XShapeEvent *)&ev);
				}
#endif
		}
		if (need_client_tidy) {
			Client *c, *nc;
			for (c = head_client; c; c = nc) {
				nc = c->next;
				if (c->remove)
					remove_client(c);
			}
		}
	}
}
