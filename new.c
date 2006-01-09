/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2005 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "evilwm.h"
#include "log.h"

static PropMwmHints *get_mwm_hints(Window);
#ifdef XDEBUG
static const char *map_state_string(int map_state);
static const char *gravity_string(int gravity);
static void debug_wm_normal_hints(XSizeHints *size);
#else
# define debug_wm_normal_hints(s)
#endif
static void init_geometry(Client *c);
static void reparent(Client *c);

void make_new_client(Window w, ScreenInfo *s) {
	Client *c;
	char *name;
	XClassHint *class;
	PropMwmHints *mhints;

	XGrabServer(dpy);

	/* First a bit of interaction with the error handler due to X's
	 * tendancy to batch event notifications.  We set a global variable to
	 * the id of the window we're initialising then do simple X call on
	 * that window.  If an error is raised by this (and nothing else should
	 * do so as we've grabbed the server), the error handler resets the
	 * variable indicating the window has already disappeared, so we stop
	 * trying to manage it. */
	initialising = w;
	XFetchName(dpy, w, &name);
	XSync(dpy, False);
	/* If 'initialising' is now set to None, that means doing the
	 * XFetchName raised BadWindow - the window has been removed before
	 * we got a chance to grab the server. */
	if (initialising == None) {
		LOG_DEBUG("make_new_client() : XError occurred for initialising window - aborting...\n");
		XSync(dpy, False);
		XUngrabServer(dpy);
		return;
	}
	initialising = None;
	LOG_DEBUG("make_new_client(): %s\n", name ? name : "Untitled");

	c = malloc(sizeof(Client));
	/* Don't crash the window manager, just fail the operation. */
	if (!c) {
		LOG_ERROR("out of memory in new_client; limping onward\n");
		return;
	}
	c->next = head_client;
	head_client = c;

	c->screen = s;
	c->window = w;
	c->ignore_unmap = 0;
	c->remove = 0;

	c->border = opt_bw;
	if ((mhints = get_mwm_hints(c->window))) {
		if (mhints->flags & MWM_HINTS_DECORATIONS
				&& !(mhints->decorations & MWM_DECOR_ALL)) {
			if (!(mhints->decorations & MWM_DECOR_BORDER)) {
				c->border = 0;
			}
		}
		XFree(mhints);
	}
	if (name)
		XFree(name);

	init_geometry(c);

#ifdef VWM
	c->vdesk = vdesk;
#endif

#ifdef DEBUG
	{
		Client *p;
		int i = 0;
		for (p = head_client; p; p = p->next)
			i++;
		LOG_DEBUG("make_new_client() : new window %dx%d+%d+%d, wincount=%d\n", c->width, c->height, c->x, c->y, i);
	}
#endif

#ifdef COLOURMAP
	XSelectInput(dpy, c->window, ColormapChangeMask | EnterWindowMask | PropertyChangeMask);
#else
	XSelectInput(dpy, c->window, EnterWindowMask | PropertyChangeMask);
#endif

	reparent(c);

	XMapWindow(dpy, c->window);
	XMapRaised(dpy, c->parent);
	set_wm_state(c, NormalState);

	XSync(dpy, False);
	XUngrabServer(dpy);

#ifdef SHAPE
	if (have_shape) {
	    XShapeSelectInput(dpy, c->window, ShapeNotifyMask);
	    set_shape(c);
	}
#endif

	/* Read instance/class information for client and check against list
	 * built with -app options */
	class = XAllocClassHint();
	if (class) {
		Application *a = head_app;
		XGetClassHint(dpy, w, class);
		while (a) {
			if ((!a->res_name || (class->res_name && !strcmp(class->res_name, a->res_name)))
					&& (!a->res_class || (class->res_class && !strcmp(class->res_class, a->res_class)))) {
				if (a->geometry_mask & WidthValue)
					c->width = a->width * c->width_inc;
				if (a->geometry_mask & HeightValue)
					c->height = a->height * c->height_inc;
				if (a->geometry_mask & XValue) {
					if (a->geometry_mask & XNegative)
						c->x = a->x + DisplayWidth(dpy, s->screen)-c->width-c->border;
					else
						c->x = a->x + c->border;
				}
				if (a->geometry_mask & YValue) {
					if (a->geometry_mask & YNegative)
						c->y = a->y + DisplayHeight(dpy, s->screen)-c->height-c->border;
					else
						c->y = a->y + c->border;
				}
				moveresize(c);
#ifdef VWM
				if (a->vdesk != -1) c->vdesk = a->vdesk;
				if (vdesk != c->vdesk && c->vdesk != STICKY) {
					hide(c);
				}
#endif
			}
			a = a->next;
		}
		XFree(class->res_name);
		XFree(class->res_class);
		XFree(class);
	}

	/* send_config(c); */
#ifndef MOUSE
	set_mouse_corner(c);
#endif
}

/* Calls XGetWindowAttributes, XGetWMHints and XGetWMNormalHints to determine
 * window's initial geometry.
 *
 * XGetWindowAttributes 
 */
static void init_geometry(Client *c) {
	CARD32 size_flags;
	XWindowAttributes attr;
	XWMHints *wm;
	int reconfigure = 0;

	/* Get current window attributes */
	LOG_XDEBUG("XGetWindowAttributes()\n");
	XGetWindowAttributes(dpy, c->window, &attr);
	LOG_XDEBUG("\t(%s) %dx%d+%d+%d, bw = %d\n", map_state_string(attr.map_state), attr.width, attr.height, attr.x, attr.y, attr.border_width);
	c->old_border = attr.border_width;
	c->oldw = c->oldh = 0;
#ifdef COLOURMAP
	c->cmap = attr.colormap;
#endif

	size_flags = get_wm_normal_hints(c);

	if ((attr.width >= c->min_width) && (attr.height >= c->min_height)) {
	/* if (attr.map_state == IsViewable || (size_flags & (PSize | USSize))) { */
		c->width = attr.width;
		c->height = attr.height;
	} else {
		c->width = c->min_width;
		c->height = c->min_height;
		reconfigure = 1;
	}
	if ((attr.map_state == IsViewable)
			|| (size_flags & (/*PPosition |*/ USPosition))) {
		c->x = attr.x;
		c->y = attr.y;
	} else {
#ifdef MOUSE
		int xmax = DisplayWidth(dpy, c->screen->screen);
		int ymax = DisplayHeight(dpy, c->screen->screen);
		int x, y;
		get_mouse_position(&x, &y, c->screen->root);
		c->x = (x * (xmax - c->border - c->width)) / xmax;
		c->y = (y * (ymax - c->border - c->height)) / ymax;
#else
		c->x = c->y = c->border;
#endif
		reconfigure = 1;
	}

	LOG_DEBUG("\twindow started as %dx%d +%d+%d\n", c->width, c->height, c->x, c->y);
	if (attr.map_state == IsViewable) {
		c->ignore_unmap++;
	} else {
		/* Initialise position if needed */
		if (reconfigure) {
			send_config(c);
		}
		if ((wm = XGetWMHints(dpy, c->window))) {
			if (wm->flags & StateHint)
				set_wm_state(c, wm->initial_state);
			XFree(wm);
		}
	}
	gravitate(c);
}

static void reparent(Client *c) {
	XSetWindowAttributes p_attr;

	p_attr.override_redirect = True;
	p_attr.background_pixel = c->screen->bg.pixel;
	p_attr.event_mask = ChildMask | ButtonPressMask | ExposureMask | EnterWindowMask;
	c->parent = XCreateWindow(dpy, c->screen->root, c->x-c->border, c->y-c->border,
		c->width+(c->border*2), c->height + (c->border*2), 0,
		DefaultDepth(dpy, c->screen->screen), CopyFromParent,
		DefaultVisual(dpy, c->screen->screen),
		CWOverrideRedirect | CWBackPixel | CWEventMask, &p_attr);

	XAddToSaveSet(dpy, c->window);
	XSetWindowBorderWidth(dpy, c->window, 0);
	XReparentWindow(dpy, c->window, c->parent, c->border, c->border);
}

/* Get WM_NORMAL_HINTS property */
CARD32 get_wm_normal_hints(Client *c) {
	XSizeHints *size;
	CARD32 flags;
	long dummy;
	size = XAllocSizeHints();
	LOG_XDEBUG("XGetWMNormalHints()\n");
	XGetWMNormalHints(dpy, c->window, size, &dummy);
	debug_wm_normal_hints(size);
	flags = size->flags;
	if (flags & PMinSize) {
		c->min_width = size->min_width;
		c->min_height = size->min_height;
	} else {
		c->min_width = c->min_height = 0;
	}
	if (flags & PMaxSize) {
		c->max_width = size->max_width;
		c->max_height = size->max_height;
	} else {
		c->max_width = c->max_height = 0;
	}
	if (flags & PBaseSize) {
		c->base_width = size->base_width;
		c->base_height = size->base_height;
	} else {
		c->base_width = c->min_width;
		c->base_height = c->min_height;
	}
	c->width_inc = c->height_inc = 1;
	if (flags & PResizeInc) {
		c->width_inc = size->width_inc ? size->width_inc : 1;
		c->height_inc = size->height_inc ? size->height_inc : 1;
	}
	if (!(flags & PMinSize)) {
		c->min_width = c->base_width + c->width_inc;
		c->min_height = c->base_height + c->height_inc;
	}
	if (flags & PWinGravity) {
		c->win_gravity = size->win_gravity;
	} else {
		c->win_gravity = NorthWestGravity;
	}
	XFree(size);
	return flags;
}

static PropMwmHints *get_mwm_hints(Window w) {
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	PropMwmHints *data;
	if (XGetWindowProperty(dpy, w, mwm_hints, 0L,
				(long)PROP_MWM_HINTS_ELEMENTS, False,
				mwm_hints, &actual_type, &actual_format,
				&nitems, &bytes_after,
				(unsigned char **)&data)
			== Success && nitems >= PROP_MWM_HINTS_ELEMENTS) {
		return data;
	}
	return NULL;
}

#ifdef XDEBUG
static const char *map_state_string(int map_state) {
	const char *map_states[4] = {
		"IsUnmapped",
		"IsUnviewable",
		"IsViewable",
		"Unknown"
	};
	return ((unsigned int)map_state < 3)
		? map_states[map_state]
		: map_states[3];
}

static const char *gravity_string(int gravity) {
	const char *gravities[12] = {
		"ForgetGravity",
		"NorthWestGravity",
		"NorthGravity",
		"NorthEastGravity",
		"WestGravity",
		"CenterGravity",
		"EastGravity",
		"SouthWestGravity",
		"SouthGravity",
		"SouthEastGravity",
		"StaticGravity",
		"Unknown"
	};
	return ((unsigned int)gravity < 11) ? gravities[gravity] : gravities[11];
}

static void debug_wm_normal_hints(XSizeHints *size) {
	if (size->flags & 15) {
		LOG_XDEBUG("\t");
		if (size->flags & USPosition) {
			LOG_XDEBUG("USPosition ");
		}
		if (size->flags & USSize) {
			LOG_XDEBUG("USSize ");
		}
		if (size->flags & PPosition) {
			LOG_XDEBUG("PPosition ");
		}
		if (size->flags & PSize) {
			LOG_XDEBUG("PSize");
		}
		LOG_XDEBUG("\n");
	}
	if (size->flags & PMinSize) {
		LOG_XDEBUG("\tPMinSize: min_width = %d, min_height = %d\n", size->min_width, size->min_height);
	}
	if (size->flags & PMaxSize) {
		LOG_XDEBUG("\tPMaxSize: max_width = %d, max_height = %d\n", size->max_width, size->max_height);
	}
	if (size->flags & PResizeInc) {
		LOG_XDEBUG("\tPResizeInc: width_inc = %d, height_inc = %d\n",
				size->width_inc, size->height_inc);
	}
	if (size->flags & PAspect) {
		LOG_XDEBUG("\tPAspect: min_aspect = %d/%d, max_aspect = %d/%d\n",
				size->min_aspect.x, size->min_aspect.y,
				size->max_aspect.x, size->max_aspect.y);
	}
	if (size->flags & PBaseSize) {
		LOG_XDEBUG("\tPBaseSize: base_width = %d, base_height = %d\n",
				size->base_width, size->base_height);
	}
	if (size->flags & PWinGravity) {
		LOG_XDEBUG("\tPWinGravity: %s\n", gravity_string(size->win_gravity));
	}
}
#endif
