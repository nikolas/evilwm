/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2002 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include "evilwm.h"
#include <stdlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif

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
			move(c, 1);
			setmouse(c->window, c->width + c->border - 1,
					c->height + c->border - 1);
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
			resize(c, 1);
			setmouse(c->window, c->width + c->border - 1,
					c->height + c->border - 1);
			break;
#ifdef VWM
		case KEY_FIX:
			c->vdesk = c->vdesk == STICKY ? vdesk : STICKY;
#ifdef VDESK_BOTH
			spawn_vdesk(c->vdesk, c);
#endif
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
				move(c, 0); break;
			case Button2:
				resize(c, 0); break;
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
#ifdef DEBUG
		fprintf(stderr, "handle_configure_request() : window configured to %dx%d+%d+%d\n", wc.width, wc.height, wc.x, wc.y);
#endif
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
#ifdef DEBUG
		fprintf(stderr, "handle_map_request() : don't know this window, calling make_new_client();\n");
#endif
		XGetWindowAttributes(dpy, e->window, &attr);
		make_new_client(e->window, find_screen(attr.root));
	}
}

void handle_unmap_event(XUnmapEvent *e) {
	Client *c = find_client(e->window);

	if (c) {
#ifdef DEBUG
		/* fprintf(stderr, "handle_unmap_event() : ignore_unmap = %d\n", c->ignore_unmap);
		 * */
#endif
		if (c->ignore_unmap) c->ignore_unmap--;
		else remove_client(c);
	}
}

#ifdef VDESK
void handle_client_message(XClientMessageEvent *e) {
	Client *c = find_client(e->window);

	if (c && e->message_type == xa_wm_change_state &&
		e->format == 32 && e->data.l[0] == IconicState) hide(c);
}
#endif

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
	long dummy;

	if (c) {
		if (e->atom == XA_WM_NORMAL_HINTS)
				XGetWMNormalHints(dpy, c->window, c->size, &dummy);
	}
}

void handle_enter_event(XCrossingEvent *e) {
	Client *c;
#ifdef VWM
	int wdesk;
#endif

	current_screen = find_screen(e->root);
	/* if (e->window == root && !current) {
		XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
		return;
	} */
	if ((c = find_client(e->window))) {
#ifdef VWM
		wdesk = c->vdesk;
		if (wdesk != vdesk && wdesk != STICKY)
			return;
#endif
#ifdef COLOURMAP
		XInstallColormap(dpy, c->cmap);
#endif
		client_update_current(current, c);
		client_update_current(c, current);
		XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
#ifdef MOUSE
		grab_button(c->parent, Mod1Mask, AnyButton);
#endif
	}
}

void handle_leave_event(XCrossingEvent *e) {
	/* if (e->window == root && !e->same_screen) {
		client_update_current(current, NULL);
	} */
}

#ifdef SHAPE
void handle_shape_event(XShapeEvent *e) {
	Client *c = find_client(e->window);
	if (c)
		set_shape(c);
}
#endif
