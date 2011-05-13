/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2011 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "evilwm.h"
#include "log.h"

static void grab_keysym(Window w, unsigned int mask, KeySym keysym);

static void recalculate_sweep(Client *c, int x1, int y1, int x2, int y2, unsigned force) {
	if (force || c->oldw == 0) {
		c->oldw = 0;
		c->width = abs(x1 - x2);
		c->width -= (c->width - c->base_width) % c->width_inc;
		if (c->min_width && c->width < c->min_width)
			c->width = c->min_width;
		if (c->max_width && c->width > c->max_width)
			c->width = c->max_width;
		c->x = (x1 <= x2) ? x1 : x1 - c->width;
	}
	if (force || c->oldh == 0)  {
		c->oldh = 0;
		c->height = abs(y1 - y2);
		c->height -= (c->height - c->base_height) % c->height_inc;
		if (c->min_height && c->height < c->min_height)
			c->height = c->min_height;
		if (c->max_height && c->height > c->max_height)
			c->height = c->max_height;
		c->y = (y1 <= y2) ? y1 : y1 - c->height;
	}
}

void sweep(Client *c) {
	XEvent ev;
	int old_cx = c->x;
	int old_cy = c->y;

	if (!grab_pointer(c->screen->root, MouseMask, resize_curs)) return;

	client_raise(c);
	annotate_create(c, &annotate_sweep_ctx);

	setmouse(c->window, c->width, c->height);
	for (;;) {
		XMaskEvent(dpy, MouseMask, &ev);
		switch (ev.type) {
			case MotionNotify:
				if (ev.xmotion.root != c->screen->root)
					break;
				annotate_preupdate(c, &annotate_sweep_ctx);
				recalculate_sweep(c, old_cx, old_cy, ev.xmotion.x, ev.xmotion.y, ev.xmotion.state & altmask);
				annotate_update(c, &annotate_sweep_ctx);
				break;
			case ButtonRelease:
				annotate_remove(c, &annotate_sweep_ctx);
				XUngrabPointer(dpy, CurrentTime);
				moveresizeraise(c);
				/* In case maximise state has changed: */
				ewmh_set_net_wm_state(c);
				return;
			default: break;
		}
	}
}

/** predicate_keyrepeatpress:
 *  predicate function for use with XCheckIfEvent.
 *  When used with XCheckIfEvent, this function will return true if
 *  there is a KeyPress event queued of the same keycode and time
 *  as @arg.
 *
 *  @arg must be a poiner to an XEvent of type KeyRelease
 */
static Bool predicate_keyrepeatpress(Display *dummy, XEvent *ev, XPointer arg) {
	(void) dummy;
	XEvent* release_event = (XEvent*) arg;
	if (ev->type != KeyPress)
		return False;
	if (release_event->xkey.keycode != ev->xkey.keycode)
		return False;
	return release_event->xkey.time == ev->xkey.time;
}

void show_info(Client *c, unsigned int keycode) {
	XEvent ev;
	XKeyboardState keyboard;

	if (XGrabKeyboard(dpy, c->screen->root, False, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess)
		return;

	/* keyboard repeat might not have any effect, newer X servers seem to
	 * only change the keyboard control after all keys have been physically
	 * released. */
	XGetKeyboardControl(dpy, &keyboard);
	XChangeKeyboardControl(dpy, KBAutoRepeatMode, &(XKeyboardControl){.auto_repeat_mode = AutoRepeatModeOff});
	annotate_create(c, &annotate_info_ctx);
	do {
		XMaskEvent(dpy, KeyReleaseMask, &ev);
		if (ev.xkey.keycode != keycode)
			continue;
		if (XCheckIfEvent(dpy, &ev, predicate_keyrepeatpress, (XPointer)&ev)) {
			/* This is a key press event with the same time as the previous
			 * key release event. */
			continue;
		}
		break; /* escape */
	} while (1);
	annotate_remove(c, &annotate_info_ctx);
	XChangeKeyboardControl(dpy, KBAutoRepeatMode, &(XKeyboardControl){.auto_repeat_mode = keyboard.global_auto_repeat});
	XUngrabKeyboard(dpy, CurrentTime);
}

static int absmin(int a, int b) {
	if (abs(a) < abs(b))
		return a;
	return b;
}

static void snap_client(Client *c) {
	int dx, dy;
	int dpy_width = DisplayWidth(dpy, c->screen->screen);
	int dpy_height = DisplayHeight(dpy, c->screen->screen);
	struct list *iter;
	Client *ci;

	/* snap to other windows */
	dx = dy = opt_snap;
	for (iter = clients_tab_order; iter; iter = iter->next) {
		ci = iter->data;
		if (ci == c) continue;
		if (ci->screen != c->screen) continue;
		if (!is_fixed(ci) && ci->vdesk != c->screen->vdesk) continue;
		if (ci->is_dock && !c->screen->docks_visible) continue;
		if (ci->y - ci->border - c->border - c->height - c->y <= opt_snap && c->y - c->border - ci->border - ci->height - ci->y <= opt_snap) {
			dx = absmin(dx, ci->x + ci->width - c->x + c->border + ci->border);
			dx = absmin(dx, ci->x + ci->width - c->x - c->width);
			dx = absmin(dx, ci->x - c->x - c->width - c->border - ci->border);
			dx = absmin(dx, ci->x - c->x);
		}
		if (ci->x - ci->border - c->border - c->width - c->x <= opt_snap && c->x - c->border - ci->border - ci->width - ci->x <= opt_snap) {
			dy = absmin(dy, ci->y + ci->height - c->y + c->border + ci->border);
			dy = absmin(dy, ci->y + ci->height - c->y - c->height);
			dy = absmin(dy, ci->y - c->y - c->height - c->border - ci->border);
			dy = absmin(dy, ci->y - c->y);
		}
	}
	if (abs(dx) < opt_snap)
		c->x += dx;
	if (abs(dy) < opt_snap)
		c->y += dy;

	/* snap to screen border */
	if (abs(c->x - c->border) < opt_snap) c->x = c->border;
	if (abs(c->y - c->border) < opt_snap) c->y = c->border;
	if (abs(c->x + c->width + c->border - dpy_width) < opt_snap)
		c->x = dpy_width - c->width - c->border;
	if (abs(c->y + c->height + c->border - dpy_height) < opt_snap)
		c->y = dpy_height - c->height - c->border;

	if (abs(c->x) == c->border && c->width == dpy_width)
		c->x = 0;
	if (abs(c->y) == c->border && c->height == dpy_height)
		c->y = 0;
}

void drag(Client *c) {
	XEvent ev;
	int x1, y1;
	int old_cx = c->x;
	int old_cy = c->y;

	if (!grab_pointer(c->screen->root, MouseMask, move_curs)) return;
	client_raise(c);
	get_mouse_position(&x1, &y1, c->screen->root);
	annotate_create(c, &annotate_drag_ctx);
	for (;;) {
		XMaskEvent(dpy, MouseMask, &ev);
		switch (ev.type) {
			case MotionNotify:
				if (ev.xmotion.root != c->screen->root)
					break;
				annotate_preupdate(c, &annotate_drag_ctx);
				c->x = old_cx + (ev.xmotion.x - x1);
				c->y = old_cy + (ev.xmotion.y - y1);
				if (opt_snap && !(ev.xmotion.state & altmask))
					snap_client(c);

				if (!no_solid_drag) {
					XMoveWindow(dpy, c->parent,
							c->x - c->border,
							c->y - c->border);
					send_config(c);
				}
				annotate_update(c, &annotate_drag_ctx);
				break;
			case ButtonRelease:
				annotate_remove(c, &annotate_drag_ctx);
				XUngrabPointer(dpy, CurrentTime);
				if (no_solid_drag) {
					moveresizeraise(c);
				}
				return;
			default: break;
		}
	}
}

void moveresize(Client *c) {
	XMoveResizeWindow(dpy, c->parent, c->x - c->border, c->y - c->border,
			c->width, c->height);
	XMoveResizeWindow(dpy, c->window, 0, 0, c->width, c->height);
	send_config(c);
}

void moveresizeraise(Client *c) {
	client_raise(c);
	moveresize(c);
}

void maximise_client(Client *c, int action, int hv) {
	if (hv & MAXIMISE_HORZ) {
		if (c->oldw) {
			if (action == NET_WM_STATE_REMOVE
					|| action == NET_WM_STATE_TOGGLE) {
				c->x = c->oldx;
				c->width = c->oldw;
				c->oldw = 0;
				XDeleteProperty(dpy, c->window, xa_evilwm_unmaximised_horz);
			}
		} else {
			if (action == NET_WM_STATE_ADD
					|| action == NET_WM_STATE_TOGGLE) {
				unsigned long props[2];
				c->oldx = c->x;
				c->oldw = c->width;
				c->x = 0;
				c->width = DisplayWidth(dpy, c->screen->screen);
				props[0] = c->oldx;
				props[1] = c->oldw;
				XChangeProperty(dpy, c->window, xa_evilwm_unmaximised_horz,
						XA_CARDINAL, 32, PropModeReplace,
						(unsigned char *)&props, 2);
			}
		}
	}
	if (hv & MAXIMISE_VERT) {
		if (c->oldh) {
			if (action == NET_WM_STATE_REMOVE
					|| action == NET_WM_STATE_TOGGLE) {
				c->y = c->oldy;
				c->height = c->oldh;
				c->oldh = 0;
				XDeleteProperty(dpy, c->window, xa_evilwm_unmaximised_vert);
			}
		} else {
			if (action == NET_WM_STATE_ADD
					|| action == NET_WM_STATE_TOGGLE) {
				unsigned long props[2];
				c->oldy = c->y;
				c->oldh = c->height;
				c->y = 0;
				c->height = DisplayHeight(dpy, c->screen->screen);
				props[0] = c->oldy;
				props[1] = c->oldh;
				XChangeProperty(dpy, c->window, xa_evilwm_unmaximised_vert,
						XA_CARDINAL, 32, PropModeReplace,
						(unsigned char *)&props, 2);
			}
		}
	}
	ewmh_set_net_wm_state(c);
	moveresizeraise(c);
	discard_enter_events(c);
}

void next(void) {
	struct list *newl = list_find(clients_tab_order, current);
	Client *newc = current;
	do {
		if (newl) {
			newl = newl->next;
			if (!newl && !current)
				return;
		}
		if (!newl)
			newl = clients_tab_order;
		if (!newl)
			return;
		newc = newl->data;
		if (newc == current)
			return;
	}
	/* NOTE: Checking against newc->screen->vdesk implies we can Alt+Tab
	 * across screen boundaries.  Is this what we want? */
	while ((!is_fixed(newc) && (newc->vdesk != newc->screen->vdesk)) || (newc->is_dock && !newc->screen->docks_visible));

	if (!newc)
		return;
	client_show(newc);
	client_raise(newc);
	select_client(newc);
#ifdef WARP_POINTER
	setmouse(newc->window, newc->width + newc->border - 1,
			newc->height + newc->border - 1);
#endif
	discard_enter_events(newc);
}

void switch_vdesk(ScreenInfo *s, unsigned int v) {
	struct list *iter;
#ifdef DEBUG
	int hidden = 0, raised = 0;
#endif
	if (!valid_vdesk(v))
		return;

	if (v == s->vdesk)
		return;
	LOG_ENTER("switch_vdesk(screen=%d, from=%d, to=%d)", s->screen, s->vdesk, v);
	if (current && !is_fixed(current)) {
		select_client(NULL);
	}
	for (iter = clients_tab_order; iter; iter = iter->next) {
		Client *c = iter->data;
		if (c->screen != s)
			continue;
		if (c->vdesk == s->vdesk) {
			client_hide(c);
#ifdef DEBUG
			hidden++;
#endif
		} else if (c->vdesk == v) {
			if (!c->is_dock || s->docks_visible)
				client_show(c);
#ifdef DEBUG
			raised++;
#endif
		}
	}
	/* cache the value of the current vdesk, so that user may toggle back to it */
	s->old_vdesk = s->vdesk;
	s->vdesk = v;
	ewmh_set_net_current_desktop(s);
	LOG_DEBUG("%d hidden, %d raised\n", hidden, raised);
	LOG_LEAVE();
}

void set_docks_visible(ScreenInfo *s, int is_visible) {
	struct list *iter;

	LOG_ENTER("set_docks_visible(screen=%d, is_visible=%d)", s->screen, is_visible);
	s->docks_visible = is_visible;
	for (iter = clients_tab_order; iter; iter = iter->next) {
		Client *c = iter->data;
		if (c->screen != s)
			continue;
		if (c->is_dock) {
			if (is_visible) {
				if (is_fixed(c) || (c->vdesk == s->vdesk)) {
					client_show(c);
					client_raise(c);
				}
			} else {
				client_hide(c);
			}
		}
	}
	LOG_LEAVE();
}

static int scale_pos(int new_screen_size, int old_screen_size, int cli_pos, int cli_size) {
	if (old_screen_size != cli_size && new_screen_size != cli_size) {
		new_screen_size -= cli_size;
		old_screen_size -= cli_size;
	}
	return new_screen_size * cli_pos / old_screen_size;
}

/* If a screen has been resized, eg, due to xrandr, some windows
 * have the possibility of:
 *   a) not being visible
 *   b) being vertically/horizontally maximised to the wrong extent
 * Currently, i can't think of a good policy for doing this, but
 * the minimal modification is to fix (b), and ensure (a) is visible
 * (NB, maximised windows will need their old* values updating according
 * to (a).
 */
void fix_screen_after_resize(ScreenInfo *s, int oldw, int oldh) {
	int neww = DisplayWidth(dpy, s->screen);
	int newh = DisplayHeight(dpy, s->screen);

	for (struct list *iter = clients_tab_order; iter; iter = iter->next) {
		Client *c = iter->data;
		/* only handle clients on the screen being resized */
		if (c->screen != s)
			continue;

		if (c->oldw) {
			/* horiz maximised: update width, update old x pos */
			c->width = neww;
			c->oldx = scale_pos(neww, oldw, c->oldx, c->oldw + c->border);
		} else {
			/* horiz normal: update x pos */
			c->x = scale_pos(neww, oldw, c->x, c->width + c->border);
		}

		if (c->oldh) {
			/* vert maximised: update height, update old y pos */
			c->height = newh;
			c->oldy = scale_pos(newh, oldh, c->oldy, c->oldh + c->border);
		} else {
			/* vert normal: update y pos */
			c->y = scale_pos(newh, oldh, c->y, c->height + c->border);
		}
		moveresize(c);
	}
}


ScreenInfo *find_screen(Window root) {
	int i;
	for (i = 0; i < num_screens; i++) {
		if (screens[i].root == root)
			return &screens[i];
	}
	return NULL;
}

ScreenInfo *find_current_screen(void) {
	Window cur_root, dw;
	int di;
	unsigned int dui;

	/* XQueryPointer is useful for getting the current pointer root */
	XQueryPointer(dpy, screens[0].root, &cur_root, &dw, &di, &di, &di, &di, &dui);
	return find_screen(cur_root);
}

static void grab_keysym(Window w, unsigned int mask, KeySym keysym) {
	KeyCode keycode = XKeysymToKeycode(dpy, keysym);
	XGrabKey(dpy, keycode, mask, w, True,
			GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, keycode, mask|LockMask, w, True,
			GrabModeAsync, GrabModeAsync);
	if (numlockmask) {
		XGrabKey(dpy, keycode, mask|numlockmask, w, True,
				GrabModeAsync, GrabModeAsync);
		XGrabKey(dpy, keycode, mask|numlockmask|LockMask, w, True,
				GrabModeAsync, GrabModeAsync);
	}
}

static KeySym keys_to_grab[] = {
#ifdef VWM
	KEY_FIX, KEY_PREVDESK, KEY_NEXTDESK, KEY_TOGGLEDESK,
	XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9, XK_0,
#endif
	KEY_NEW, KEY_KILL,
	KEY_TOPLEFT, KEY_TOPRIGHT, KEY_BOTTOMLEFT, KEY_BOTTOMRIGHT,
	KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP,
	KEY_LOWER, KEY_ALTLOWER, KEY_INFO, KEY_MAXVERT, KEY_MAX,
	KEY_DOCK_TOGGLE
};
#define NUM_GRABS (int)(sizeof(keys_to_grab) / sizeof(KeySym))

static KeySym alt_keys_to_grab[] = {
	KEY_KILL, KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP,
	KEY_MAXVERT,
};
#define NUM_ALT_GRABS (int)(sizeof(alt_keys_to_grab) / sizeof(KeySym))

void grab_keys_for_screen(ScreenInfo *s) {
	int i;
	/* Release any previous grabs */
	XUngrabKey(dpy, AnyKey, AnyModifier, s->root);
	/* Grab key combinations we're interested in */
	for (i = 0; i < NUM_GRABS; i++) {
		grab_keysym(s->root, grabmask1, keys_to_grab[i]);
	}
	for (i = 0; i < NUM_ALT_GRABS; i++) {
		grab_keysym(s->root, grabmask1 | altmask, alt_keys_to_grab[i]);
	}
	grab_keysym(s->root, grabmask2, KEY_NEXT);
}
