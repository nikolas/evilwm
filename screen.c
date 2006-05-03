/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2006 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "evilwm.h"
#include "log.h"

#ifdef INFOBANNER
Window info_window = None;

static void create_info_window(Client *c);
static void update_info_window(Client *c);
static void remove_info_window(void);
static void grab_keysym(Window w, unsigned int mask, KeySym keysym);

static void create_info_window(Client *c) {
	info_window = XCreateSimpleWindow(dpy, c->screen->root, -4, -4, 2, 2,
			0, c->screen->fg.pixel, c->screen->fg.pixel);
	XMapRaised(dpy, info_window);
	update_info_window(c);
}

static void update_info_window(Client *c) {
	char *name;
	char buf[27];
	int namew, iwinx, iwiny, iwinw, iwinh;
	int width_inc = c->width_inc, height_inc = c->height_inc;

	if (!info_window)
		return;
	snprintf(buf, sizeof(buf), "%dx%d+%d+%d", (c->width-c->base_width)/width_inc,
		(c->height-c->base_height)/height_inc, c->x, c->y);
	iwinw = XTextWidth(font, buf, strlen(buf)) + 2;
	iwinh = font->max_bounds.ascent + font->max_bounds.descent;
	XFetchName(dpy, c->window, &name);
	if (name) {
		namew = XTextWidth(font, name, strlen(name));
		if (namew > iwinw)
			iwinw = namew + 2;
		iwinh = iwinh * 2;
	}
	iwinx = c->x + c->border + c->width - iwinw;
	iwiny = c->y - c->border;
	if (iwinx + iwinw > DisplayWidth(dpy, c->screen->screen))
		iwinx = DisplayWidth(dpy, c->screen->screen) - iwinw;
	if (iwinx < 0)
		iwinx = 0;
	if (iwiny + iwinh > DisplayHeight(dpy, c->screen->screen))
		iwiny = DisplayHeight(dpy, c->screen->screen) - iwinh;
	if (iwiny < 0)
		iwiny = 0;
	XMoveResizeWindow(dpy, info_window, iwinx, iwiny, iwinw, iwinh);
	XClearWindow(dpy, info_window);
	if (name) {
		XDrawString(dpy, info_window, c->screen->invert_gc,
				1, iwinh / 2 - 1, name, strlen(name));
		XFree(name);
	}
	XDrawString(dpy, info_window, c->screen->invert_gc, 1, iwinh - 1,
			buf, strlen(buf));
}

static void remove_info_window(void) {
	if (info_window)
		XDestroyWindow(dpy, info_window);
	info_window = None;
}
#endif  /* INFOBANNER */

#if defined(MOUSE) || !defined(INFOBANNER)
static void draw_outline(Client *c) {
#ifndef INFOBANNER_MOVERESIZE
	char buf[27];
	int width_inc = c->width_inc, height_inc = c->height_inc;
#endif  /* ndef INFOBANNER_MOVERESIZE */

	XDrawRectangle(dpy, c->screen->root, c->screen->invert_gc,
		c->x - c->border, c->y - c->border,
		c->width + c->border, c->height + c->border);

#ifndef INFOBANNER_MOVERESIZE
	snprintf(buf, sizeof(buf), "%dx%d+%d+%d", (c->width-c->base_width)/width_inc,
			(c->height-c->base_height)/height_inc, c->x, c->y);
	XDrawString(dpy, c->screen->root, c->screen->invert_gc,
		c->x + c->width - XTextWidth(font, buf, strlen(buf)) - SPACE,
		c->y + c->height - SPACE,
		buf, strlen(buf));
#endif  /* ndef INFOBANNER_MOVERESIZE */
}
#endif

static void recalculate_sweep(Client *c, int x1, int y1, int x2, int y2) {
	c->width = abs(x1 - x2);
	c->height = abs(y1 - y2);
	c->width -= (c->width - c->base_width) % c->width_inc;
	c->height -= (c->height - c->base_height) % c->height_inc;
	if (c->min_width && c->width < c->min_width) c->width = c->min_width;
	if (c->min_height && c->height < c->min_height) c->height = c->min_height;
	if (c->max_width && c->width > c->max_width) c->width = c->max_width;
	if (c->max_height && c->height > c->max_height) c->height = c->max_height;
	c->x = (x1 <= x2) ? x1 : x1 - c->width;
	c->y = (y1 <= y2) ? y1 : y1 - c->height;
}

#ifdef MOUSE
void sweep(Client *c) {
	XEvent ev;
	int old_cx = c->x;
	int old_cy = c->y;

	if (!grab_pointer(c->screen->root, MouseMask, resize_curs)) return;

	XRaiseWindow(dpy, c->parent);
#ifdef INFOBANNER_MOVERESIZE
	create_info_window(c);
#endif
	XGrabServer(dpy);
	draw_outline(c);

	setmouse(c->window, c->width, c->height);
	for (;;) {
		XMaskEvent(dpy, MouseMask, &ev);
		switch (ev.type) {
			case MotionNotify:
				draw_outline(c); /* clear */
				XUngrabServer(dpy);
				recalculate_sweep(c, old_cx, old_cy, ev.xmotion.x, ev.xmotion.y);
#ifdef INFOBANNER_MOVERESIZE
				update_info_window(c);
#endif
				XSync(dpy, False);
				XGrabServer(dpy);
				draw_outline(c);
				break;
			case ButtonRelease:
				draw_outline(c); /* clear */
				XUngrabServer(dpy);
#ifdef INFOBANNER_MOVERESIZE
				remove_info_window();
#endif
				XUngrabPointer(dpy, CurrentTime);
				moveresize(c);
				return;
			default: break;
		}
	}
}
#endif

void show_info(Client *c, KeySym key) {
	XEvent ev;
	XKeyboardState keyboard;

	XGetKeyboardControl(dpy, &keyboard);
	XAutoRepeatOff(dpy);
#ifdef INFOBANNER
	create_info_window(c);
#else
	XGrabServer(dpy);
	draw_outline(c);
#endif
	do {
		XMaskEvent(dpy, KeyReleaseMask, &ev);
	} while (XKeycodeToKeysym(dpy,ev.xkey.keycode,0) != key);
#ifdef INFOBANNER
	remove_info_window();
#else
	draw_outline(c);
	XUngrabServer(dpy);
#endif
	if (keyboard.global_auto_repeat == AutoRepeatModeOn)
		XAutoRepeatOn(dpy);
}

#ifdef MOUSE
#ifdef SNAP
static int absmin(int a, int b) {
	if (abs(a) < abs(b))
		return a;
	return b;
}

static void snap_client(Client *c) {
	int dx, dy;
	int dpy_width = DisplayWidth(dpy, c->screen->screen);
	int dpy_height = DisplayHeight(dpy, c->screen->screen);
	Client *ci;

	/* snap to screen border */
	if (abs(c->x - c->border) < opt_snap) c->x = c->border;
	if (abs(c->y - c->border) < opt_snap) c->y = c->border;
	if (abs(c->x + c->width + c->border - dpy_width) < opt_snap)
		c->x = dpy_width - c->width - c->border;
	if (abs(c->y + c->height + c->border - dpy_height) < opt_snap)
		c->y = dpy_height - c->height - c->border;

	/* snap to other windows */
	dx = dy = opt_snap;
	for (ci = head_client; ci; ci = ci->next) {
		if (ci != c
				&& (ci->screen == c->screen)
#ifdef VWM
				&& (ci->vdesk == c->vdesk)
#endif
				) {
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
	}
	if (abs(dx) < opt_snap)
		c->x += dx;
	if (abs(dy) < opt_snap)
		c->y += dy;
	if (abs(c->x) == c->border && c->width == dpy_width)
		c->x = 0;
	if (abs(c->y) == c->border && c->height == dpy_height)
		c->y = 0;
}
#endif /* def SNAP */

void drag(Client *c) {
	XEvent ev;
	int x1, y1;
	int old_cx = c->x;
	int old_cy = c->y;

	if (!grab_pointer(c->screen->root, MouseMask, move_curs)) return;
	XRaiseWindow(dpy, c->parent);
	get_mouse_position(&x1, &y1, c->screen->root);
#ifdef INFOBANNER_MOVERESIZE
	create_info_window(c);
#endif
	if (!solid_drag) {
		XGrabServer(dpy);
		draw_outline(c);
	}
	for (;;) {
		XMaskEvent(dpy, MouseMask, &ev);
		switch (ev.type) {
			case MotionNotify:
				if (!solid_drag) {
					draw_outline(c); /* clear */
					XUngrabServer(dpy);
				}
				c->x = old_cx + (ev.xmotion.x - x1);
				c->y = old_cy + (ev.xmotion.y - y1);
#ifdef SNAP
				if (opt_snap)
					snap_client(c);
#endif

#ifdef INFOBANNER_MOVERESIZE
				update_info_window(c);
#endif
				if (!solid_drag) {
					XSync(dpy, False);
					XGrabServer(dpy);
					draw_outline(c);
				} else {
					XMoveWindow(dpy, c->parent,
							c->x - c->border,
							c->y - c->border);
					send_config(c);
				}
				break;
			case ButtonRelease:
				if (!solid_drag) {
					draw_outline(c); /* clear */
					XUngrabServer(dpy);
				}
#ifdef INFOBANNER_MOVERESIZE
				remove_info_window();
#endif
				XUngrabPointer(dpy, CurrentTime);
				if (!solid_drag) {
					moveresize(c);
				}
				return;
			default: break;
		}
	}
}
#endif /* def MOUSE */

void moveresize(Client *c) {
	XRaiseWindow(dpy, c->parent);
	XMoveResizeWindow(dpy, c->parent, c->x - c->border, c->y - c->border,
			c->width, c->height);
	XMoveResizeWindow(dpy, c->window, 0, 0, c->width, c->height);
	send_config(c);
}

void maximise_client(Client *c, int hv) {
	if (hv & MAXIMISE_HORZ) {
		if (c->oldw) {
			c->x = c->oldx;
			c->width = c->oldw;
			c->oldw = 0;
		} else {
			c->oldx = c->x;
			c->oldw = c->width;
			c->x = 0;
			c->width = DisplayWidth(dpy, c->screen->screen);
		}
	}
	if (hv & MAXIMISE_VERT) {
		if (c->oldh) {
			c->y = c->oldy;
			c->height = c->oldh;
			c->oldh = 0;
		} else {
			c->oldy = c->y;
			c->oldh = c->height;
			c->y = 0;
			c->height = DisplayHeight(dpy, c->screen->screen);
		}
	}
	moveresize(c);
	discard_enter_events();
}

#ifdef VWM
void hide(Client *c) {
	/* This will generate an unmap event.  Tell event handler
	 * to ignore it. */
	c->ignore_unmap++;
	LOG_XDEBUG("screen:XUnmapWindow(parent); ");
	XUnmapWindow(dpy, c->parent);
	set_wm_state(c, IconicState);
}
#endif

void unhide(Client *c, int raise_win) {
	raise_win ? XMapRaised(dpy, c->parent) : XMapWindow(dpy, c->parent);
	set_wm_state(c, NormalState);
}

void next(void) {
	Client *newc = current;
	do {
		if (newc) {
			newc = newc->next;
			if (!newc && !current)
				return;
		}
		if (!newc)
			newc = head_client;
		if (!newc)
			return;
		if (newc == current)
			return;
	}
#ifdef VWM
	/* NOTE: Checking against newc->screen->vdesk implies we can Alt+Tab
	 * across screen boundaries.  Is this what we want? */
	while (newc->vdesk != newc->screen->vdesk);
#else
	while (0);
#endif
	if (!newc)
		return;
	unhide(newc, RAISE);
	select_client(newc);
	setmouse(newc->window, 0, 0);
	setmouse(newc->window, newc->width + newc->border - 1,
			newc->height + newc->border - 1);
	discard_enter_events();
}

#ifdef VWM
void switch_vdesk(ScreenInfo *s, int v) {
	Client *c;
#ifdef DEBUG
	int hidden = 0, raised = 0;
#endif

	if (v == s->vdesk)
		return;
	if (current && !is_sticky(current)) {
		select_client(NULL);
	}
	LOG_DEBUG("switch_vdesk(): Switching screen %d to desk %d", s->screen, v);
	for (c = head_client; c; c = c->next) {
		if (c->screen != s)
			continue;
		if (is_sticky(c) && c->vdesk != v) {
			c->vdesk = v;
			update_net_wm_desktop(c);
		}
		if (c->vdesk == s->vdesk) {
			hide(c);
#ifdef DEBUG
			hidden++;
#endif
		} else if (c->vdesk == v) {
			unhide(c, NO_RAISE);
#ifdef DEBUG
			raised++;
#endif
		}
	}
	s->vdesk = v;
	LOG_DEBUG(" (%d hidden, %d raised)\n", hidden, raised);
}
#endif /* def VWM */

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

void grab_keys_for_screen(ScreenInfo *s) {
	KeySym *keysym;
	KeySym keys_to_grab[] = {
		KEY_NEW, KEY_KILL,
		KEY_TOPLEFT, KEY_TOPRIGHT, KEY_BOTTOMLEFT, KEY_BOTTOMRIGHT,
		KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP,
		KEY_LOWER, KEY_ALTLOWER, KEY_INFO, KEY_MAXVERT, KEY_MAX,
#ifdef VWM
		KEY_FIX, KEY_PREVDESK, KEY_NEXTDESK,
		XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8,
#endif
		0
	};
	KeySym alt_keys_to_grab[] = {
		KEY_KILL, KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP, 0
	};

	/* Release any previous grabs */
	XUngrabKey(dpy, AnyKey, AnyModifier, s->root);
	/* Grab key combinations we're interested in */
	for (keysym = keys_to_grab; *keysym; keysym++) {
		grab_keysym(s->root, grabmask1, *keysym);
	}
	for (keysym = alt_keys_to_grab; *keysym; keysym++) {
		grab_keysym(s->root, grabmask1 | altmask, *keysym);
	}
	grab_keysym(s->root, grabmask2, KEY_NEXT);
}
