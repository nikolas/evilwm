/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2006 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include "evilwm.h"
#include "log.h"

static void current_to_head(void) {
	Client *c;
	if (current && current != head_client) {
		for (c = head_client; c; c = c->next) {
			if (c->next == current) {
				c->next = current->next;
				current->next = head_client;
				head_client = current;
				break;
			}
		}
	}
}

static void handle_key_event(XKeyEvent *e) {
	KeySym key = XKeycodeToKeysym(dpy,e->keycode,0);
	Client *c;
	int width_inc, height_inc;
#ifdef VWM
	ScreenInfo *current_screen = find_current_screen();
#endif

	switch(key) {
		case KEY_NEW:
			spawn(opt_term);
			break;
		case KEY_NEXT:
			next();
			if (XGrabKeyboard(dpy, e->root, False, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) {
				XEvent ev;
				do {
					XMaskEvent(dpy, KeyPressMask|KeyReleaseMask, &ev);
					if (ev.type == KeyPress && XKeycodeToKeysym(dpy,ev.xkey.keycode,0) == KEY_NEXT)
						next();
				} while (ev.type == KeyPress || XKeycodeToKeysym(dpy,ev.xkey.keycode,0) == KEY_NEXT);
				XUngrabKeyboard(dpy, CurrentTime);
			}
			current_to_head();
			break;
#ifdef VWM
		case XK_1: case XK_2: case XK_3: case XK_4:
		case XK_5: case XK_6: case XK_7: case XK_8:
			switch_vdesk(current_screen, KEY_TO_VDESK(key));
			break;
		case KEY_PREVDESK:
			if (current_screen->vdesk > KEY_TO_VDESK(XK_1)) {
				switch_vdesk(current_screen,
						current_screen->vdesk - 1);
			}
			break;
		case KEY_NEXTDESK:
			if (current_screen->vdesk < KEY_TO_VDESK(XK_8)) {
				switch_vdesk(current_screen,
						current_screen->vdesk + 1);
			}
			break;
#endif
		default: break;
	}
	c = current;
	if (c == NULL) return;
	width_inc = (c->width_inc > 1) ? c->width_inc : 16;
	height_inc = (c->height_inc > 1) ? c->height_inc : 16;
	switch (key) {
		case KEY_LEFT:
			if (e->state & altmask) {
				if ((c->width - width_inc) >= c->min_width)
					c->width -= width_inc;
			} else {
				c->x -= 16;
			}
			goto move_client;
		case KEY_DOWN:
			if (e->state & altmask) {
				if (!c->max_height || (c->height + height_inc) <= c->max_height)
					c->height += height_inc;
			} else {
				c->y += 16;
			}
			goto move_client;
		case KEY_UP:
			if (e->state & altmask) {
				if ((c->height - height_inc) >= c->min_height)
					c->height -= height_inc;
			} else {
				c->y -= 16;
			}
			goto move_client;
		case KEY_RIGHT:
			if (e->state & altmask) {
				if (!c->max_width || (c->width + width_inc) <= c->max_width)
					c->width += width_inc;
			} else {
				c->x += 16;
			}
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
			send_wm_delete(c, e->state & altmask);
			break;
		case KEY_LOWER: case KEY_ALTLOWER:
			XLowerWindow(dpy, c->parent);
			break;
		case KEY_INFO:
			show_info(c, key);
			break;
		case KEY_MAX:
			maximise_client(c, MAXIMISE_HORZ|MAXIMISE_VERT);
			break;
		case KEY_MAXVERT:
			maximise_client(c, MAXIMISE_VERT);
			break;
#ifdef VWM
		case KEY_FIX:
			fix_client(c);
			break;
#endif
		default: break;
	}
	return;
move_client:
	moveresize(c);
	setmouse(c->window, c->width + c->border - 1,
			c->height + c->border - 1);
	discard_enter_events();
	return;
}

#ifdef MOUSE
static void handle_button_event(XButtonEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
		switch (e->button) {
			case Button1:
				drag(c); break;
			case Button2:
				sweep(c); break;
			case Button3:
				XLowerWindow(dpy, c->parent); break;
			default: break;
		}
	}
}
#endif

static void handle_configure_request(XConfigureRequestEvent *e) {
	Client *c = find_client(e->window);
	XWindowChanges wc;
	unsigned int value_mask = e->value_mask;

	wc.sibling = e->above;
	wc.stack_mode = e->detail;
	wc.width = e->width;
	wc.height = e->height;
	if (c) {
		ungravitate(c);
		if (value_mask & CWWidth) c->width = e->width;
		if (value_mask & CWHeight) c->height = e->height;
		if (value_mask & CWX) c->x = e->x;
		if (value_mask & CWY) c->y = e->y;
		if (value_mask & CWStackMode && value_mask & CWSibling) {
			Client *sibling = find_client(e->above);
			if (sibling) {
				wc.sibling = sibling->parent;
			}
		}
		if (c->x == 0 && c->width >= DisplayWidth(dpy, c->screen->screen)) {
			c->x -= c->border;
		}
		if (c->y == 0 && c->height >= DisplayHeight(dpy, c->screen->screen)) {
			c->y -= c->border;
		}
		gravitate(c);

		wc.x = c->x - c->border;
		wc.y = c->y - c->border;
		wc.border_width = c->border;
		LOG_XDEBUG("XConfigureWindow(dpy, parent(%x), %lx, &wc);\n", (unsigned int)c->parent, value_mask);
		XConfigureWindow(dpy, c->parent, value_mask, &wc);
		XMoveResizeWindow(dpy, c->window, 0, 0, c->width, c->height);
		if ((value_mask & (CWX|CWY)) && !(value_mask & (CWWidth|CWHeight))) {
			send_config(c);
		}
		wc.border_width = 0;
	} else {
		wc.x = c ? 0 : e->x;
		wc.y = c ? 0 : e->y;
		LOG_XDEBUG("XConfigureWindow(dpy, window(%x), %lx, &wc);\n", (unsigned int)e->window, value_mask);
		XConfigureWindow(dpy, e->window, value_mask, &wc);
	}
}

static void handle_map_request(XMapRequestEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
#ifdef VWM
		if (c->vdesk != c->screen->vdesk)
			switch_vdesk(c->screen, c->vdesk);
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

	LOG_DEBUG("handle_unmap_event(): ");
	if (c) {
		if (c->ignore_unmap) {
			c->ignore_unmap--;
			LOG_DEBUG("ignored (%d ignores remaining)\n", c->ignore_unmap);
		} else {
			LOG_DEBUG("flagging client for removal\n");
			c->remove = 1;
			need_client_tidy = 1;
		}
	} else {
		LOG_DEBUG("unknown client!\n");
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

	if ((c = find_client(e->window))) {
#ifdef VWM
		if (c->vdesk != c->screen->vdesk)
			return;
#endif
		select_client(c);
		current_to_head();
	}
}

static void handle_mappingnotify_event(XMappingEvent *e) {
	XRefreshKeyboardMapping(e);
	if (e->request == MappingKeyboard) {
		int i;
		for (i = 0; i < num_screens; i++) {
			grab_keys_for_screen(&screens[i]);
		}
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
			case MappingNotify:
				handle_mappingnotify_event(&ev.xmapping); break;
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
