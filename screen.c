/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2002 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "evilwm.h"

#ifdef VWM
# define HAS_HIDE 1
#endif
#ifdef VDESK
# define HAS_HIDE 1
#endif

#ifdef INFOBANNER
Window info_window = None;

void create_info_window(Client *c) {
	char *name;
	char buf[24];
	int namew, iwinx, iwiny, iwinw, iwinh;
	int width_inc = 1, height_inc = 1;

#ifdef DEBUG
	fprintf(stderr, "create_info_window() : Creating...\n");
#endif
	if (c->size->flags & PResizeInc) {
		width_inc = c->size->width_inc;
		height_inc = c->size->height_inc;
	}
	snprintf(buf, sizeof(buf), "%dx%d+%d+%d", c->width/width_inc,
		c->height/height_inc, c->x, c->y);
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
	if (info_window)
		XDestroyWindow(dpy, info_window);
	info_window = XCreateSimpleWindow(dpy, c->screen->root, iwinx, iwiny, iwinw, iwinh,
			0, c->screen->fg.pixel, c->screen->fg.pixel);
	XMapRaised(dpy, info_window);
	if (name) {
		XDrawString(dpy, info_window, c->screen->invert_gc,
			1, iwinh / 2 - 1,
			name, strlen(name));
		XFree(name);
	}
	XDrawString(dpy, info_window, c->screen->invert_gc,
		1, iwinh - 1,
		buf, strlen(buf));
}

void remove_info_window(void) {
#ifdef DEBUG
	fprintf(stderr, "remove_info_window() : Removing...\n");
#endif
	if (info_window)
		XDestroyWindow(dpy, info_window);
	info_window = None;
}
#endif  /* INFOBANNER */

void draw_outline(Client *c) {
#ifndef INFOBANNER_MOVERESIZE
	char buf[24];
	int width_inc = 1, height_inc = 1;
#endif  /* ndef INFOBANNER */

	XDrawRectangle(dpy, c->screen->root, c->screen->invert_gc,
		c->x - c->border, c->y - c->border,
		c->width + c->border, c->height + c->border);

#ifndef INFOBANNER_MOVERESIZE
	if (c->size->flags & PResizeInc) {
		width_inc = c->size->width_inc;
		height_inc = c->size->height_inc;
	}
	snprintf(buf, sizeof(buf), "%dx%d+%d+%d", c->width/width_inc,
			c->height/height_inc, c->x, c->y);
	XDrawString(dpy, c->screen->root, c->screen->invert_gc,
		c->x + c->width - XTextWidth(font, buf, strlen(buf)) - SPACE,
		c->y + c->height - SPACE,
		buf, strlen(buf));
#endif  /* ndef INFOBANNER */
}

#ifdef MOUSE
void get_mouse_position(int *x, int *y, Window root) {
	Window dw1, dw2;
	int t1, t2;
	unsigned int t3;

	XQueryPointer(dpy, root, &dw1, &dw2, x, y, &t1, &t2, &t3);
}
#endif

void recalculate_sweep(Client *c, int x1, int y1, int x2, int y2) {
	int basex, basey;

	c->width = abs(x1 - x2);
	c->height = abs(y1 - y2);

	if (c->size->flags & PResizeInc) {
		basex = (c->size->flags & PBaseSize) ? c->size->base_width :
			(c->size->flags & PMinSize) ? c->size->min_width : 0;
		basey = (c->size->flags & PBaseSize) ? c->size->base_height :
			(c->size->flags & PMinSize) ? c->size->min_height : 0;
		c->width -= (c->width - basex) % c->size->width_inc;
		c->height -= (c->height - basey) % c->size->height_inc;
	}

	if (c->size->flags & PMinSize) {
		if (c->width < c->size->min_width) c->width = c->size->min_width;
		if (c->height < c->size->min_height) c->height = c->size->min_height;
	}

	if (c->size->flags & PMaxSize) {
		if (c->width > c->size->max_width) c->width = c->size->max_width;
		if (c->height > c->size->max_height) c->height = c->size->max_height;
	}

	c->x = (x1 <= x2) ? x1 : x1 - c->width;
	c->y = (y1 <= y2) ? y1 : y1 - c->height;
}

#ifdef MOUSE
void sweep(Client *c) {
	XEvent ev;
	int old_cx = c->x;
	int old_cy = c->y;

	if (!grab_pointer(c->screen->root, MouseMask, resize_curs)) return;

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
#ifdef INFOBANNER_MOVERESIZE
				remove_info_window();
#endif
				recalculate_sweep(c, old_cx, old_cy, ev.xmotion.x, ev.xmotion.y);
				XSync(dpy, False);
#ifdef INFOBANNER_MOVERESIZE
				create_info_window(c);
#endif
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
				return;
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
	Client *ci;

	/* snap to screen border */
	if (abs(c->x - c->border) < opt_snap) c->x = c->border;
	if (abs(c->y - c->border) < opt_snap) c->y = c->border;
	if (abs(c->x + c->width + c->border - DisplayWidth(dpy, c->screen->screen)) < opt_snap)
		c->x = DisplayWidth(dpy, c->screen->screen) - c->width - c->border;
	if (abs(c->y + c->height + c->border - DisplayHeight(dpy, c->screen->screen)) < opt_snap)
		c->y = DisplayHeight(dpy, c->screen->screen) - c->height - c->border;

	/* snap to other windows */
	dx = dy = opt_snap;
	for (ci = head_client; ci; ci = ci->next) {
		if (ci != c && (ci->vdesk == vdesk || ci->vdesk == STICKY)) {
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
}
#endif /* def SNAP */

void drag(Client *c) {
	XEvent ev;
	int x1, y1;
	int old_cx = c->x;
	int old_cy = c->y;
	int reallymove = 0;

	if (!grab_pointer(c->screen->root, MouseMask, move_curs)) return;
	get_mouse_position(&x1, &y1, c->screen->root);
#ifdef INFOBANNER_MOVERESIZE
	create_info_window(c);
#endif
#ifndef SOLIDDRAG
	XGrabServer(dpy);
	draw_outline(c);
#endif 
	for (;;) {
		XMaskEvent(dpy, MouseMask, &ev);
		switch (ev.type) {
			case MotionNotify:
#ifndef SOLIDDRAG
				draw_outline(c); /* clear */
				XUngrabServer(dpy);
#endif
#ifdef INFOBANNER_MOVERESIZE
				remove_info_window();
#endif
				c->x = old_cx + (ev.xmotion.x - x1);
				c->y = old_cy + (ev.xmotion.y - y1);
#ifdef SNAP
				if (opt_snap)
					snap_client(c);
#endif

				if ((abs(c->x - old_cx)>5) || (abs(c->y - old_cy)>5))
					reallymove = 1;
#ifdef INFOBANNER_MOVERESIZE
				create_info_window(c);
#endif
#ifndef SOLIDDRAG
				XSync(dpy, False);
				XGrabServer(dpy);
				draw_outline(c);
#else
				if (reallymove) {
					XMoveWindow(dpy, c->parent, c->x - c->border,
						c->y - c->border);
					send_config(c);
				}
#endif 
				break;
			case ButtonRelease:
#ifndef SOLIDDRAG
				draw_outline(c); /* clear */
				XUngrabServer(dpy);
#endif
#ifdef INFOBANNER_MOVERESIZE
				remove_info_window();
#endif
				XUngrabPointer(dpy, CurrentTime);
				if (!reallymove) {
					c->x = old_cx;
					c->y = old_cy;
				}
				return;
			default:
				break;
		}
	}
}
#endif /* def MOUSE */

void move(Client *c, int set) {
	if (c) {
		XRaiseWindow(dpy, c->parent);
#ifdef MOUSE
		if (!set)
			drag(c);
#endif
		XMoveWindow(dpy, c->parent, c->x - c->border, c->y - c->border);
		send_config(c);
	}
}

void resize(Client *c, int set) {
	if (c) {
		XRaiseWindow(dpy, c->parent);
#ifdef MOUSE
		if (!set)
			sweep(c);
#endif
		XMoveResizeWindow(dpy, c->parent, c->x - c->border,
				c->y - c->border, c->width + (c->border*2),
				c->height + (c->border*2));
		XMoveResizeWindow(dpy, c->window, c->border, c->border,
				c->width, c->height);
		send_config(c);
	}
}

void maximise_horiz(Client *c) {
#ifdef DEBUG
	fprintf(stderr, "SCREEN: maximise_horiz()\n");
#endif
	if (c->oldw) {
		c->x = c->oldx;
		c->width = c->oldw;
		c->oldw = 0;
	} else {
		c->oldx = c->x;
		c->oldw = c->width;
		recalculate_sweep(c, 0, c->y, DisplayWidth(dpy, c->screen->screen),
				c->y + c->height);
	}
}

void maximise_vert(Client *c) {
#ifdef DEBUG
	fprintf(stderr, "SCREEN: maximise_vert()\n");
#endif
	if (c->oldh) {
		c->y = c->oldy;
		c->height = c->oldh;
		c->oldh = 0;
	} else {
		c->oldy = c->y;
		c->oldh = c->height;
		recalculate_sweep(c, c->x, 0, c->x + c->width,
				DisplayHeight(dpy, c->screen->screen));
	}
}

#ifdef HAS_HIDE
void hide(Client *c) {
	if (c) {
		c->ignore_unmap += 2;
#ifdef XDEBUG
		fprintf(stderr, "screen:XUnmapWindow(parent); ");
#endif
		XUnmapWindow(dpy, c->parent);
#ifdef XDEBUG
		fprintf(stderr, "screen:XUnmapWindow(window); ");
#endif
		XUnmapWindow(dpy, c->window);
		set_wm_state(c, IconicState);
	}
}
#endif

void unhide(Client *c, int raise) {
	XMapWindow(dpy, c->window);
	raise ? XMapRaised(dpy, c->parent) : XMapWindow(dpy, c->parent);
	set_wm_state(c, NormalState);
}

void next(void) {
	Client *newc = current;

	if (!newc) {
#ifdef DEBUG
		fprintf(stderr,"NEXT: no current window, looking on this desktop\n");
#endif
		newc = head_client;
#ifdef VWM
		if(newc->vdesk != vdesk && newc->vdesk != STICKY) {
			do {
				newc = newc->next;
			} while (newc && newc->vdesk != vdesk && newc->vdesk != STICKY);
		}
#endif
	} else {
#ifdef VWM
		do {
#endif
			newc = newc->next;
			if (current && !newc)
				newc = head_client;
#ifdef VWM
		} while (newc && newc->vdesk != vdesk && newc->vdesk != STICKY);
#endif
	}
	if (newc) {
#ifdef VWM
		if (newc->vdesk == vdesk || newc->vdesk == STICKY) {
#endif
			unhide(newc, RAISE);
			setmouse(newc->window, 0, 0);
			setmouse(newc->window, newc->width + newc->border - 1,
				newc->height + newc->border - 1);
#ifdef VWM
		}
#endif
	}
#ifdef DEBUG
       else {
               fprintf(stderr,"NEXT: hmm, no next window\n");
       }
#endif

}

#ifdef VWM
#ifdef VDESK_BOTH
void switch_vdesk(int v) {
	spawn_vdesk(v, NULL);
	vdesk = v;
}
#else
void switch_vdesk(int v) {
	Client *c;
	int wdesk;
#ifdef VWM_WARP
	int warped = 0;
#endif
#ifdef DEBUG
	int hidden = 0, raised = 0;
#endif

	if (v == vdesk)
		return;
	if (current && current->vdesk != STICKY) {
		client_update_current(current, NULL);
	}
#ifdef DEBUG
	fprintf(stderr, "switch_vdesk() : Switching to desk %d\n", v);
#endif
	for (c = head_client; c; c = c->next) {
		wdesk = c->vdesk;
		if (wdesk == vdesk) {
			hide(c);
#ifdef DEBUG
			hidden++;
#endif
		} else if (wdesk == v) {
			unhide(c, NO_RAISE);
#ifdef DEBUG
			raised++;
#endif
#ifdef VWM_WARP
			if (!warped) {
				setmouse(c->window, c->width, c->height);
				warped = 1;
			}
#endif
		}
	}
	vdesk = v;
#ifdef DEBUG
	fprintf(stderr, "\tswitch_vdesk() : %d hidden, %d raised\n", hidden, raised);
#endif
}
#endif /* def VDESK_BOTH */
#endif /* def VWM */

ScreenInfo *find_screen(Window root) {
	int i;
	for (i = 0; i < num_screens; i++) {
		if (screens[i].root == root)
			return &screens[i];
	}
	return NULL;
}
