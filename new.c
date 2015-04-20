/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2015 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "evilwm.h"
#include "log.h"

static void init_geometry(Client *c);
static void reparent(Client *c);
static void update_window_type_flags(Client *c, unsigned int type);
#ifdef XDEBUG
static const char *map_state_string(int map_state);
static const char *gravity_string(int gravity);
static void debug_wm_normal_hints(XSizeHints *size);
#else
# define debug_wm_normal_hints(s)
#endif

void make_new_client(Window w, ScreenInfo *s) {
	Client *c;
	char *name;
	XClassHint *class;
	unsigned int window_type;

	LOG_ENTER("make_new_client(window=%lx)", w);

	XGrabServer(dpy);

	/* First a bit of interaction with the error handler due to X's
	 * tendency to batch event notifications.  We set a global variable to
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
		LOG_DEBUG("XError occurred for initialising window - aborting...\n");
		XUngrabServer(dpy);
		LOG_LEAVE();
		return;
	}
	initialising = None;
	LOG_DEBUG("screen=%d\n", s->screen);
	LOG_DEBUG("name=%s\n", name ? name : "Untitled");
	if (name)
		XFree(name);

	window_type = ewmh_get_net_wm_window_type(w);
	/* Don't manage DESKTOP type windows */
	if (window_type & EWMH_WINDOW_TYPE_DESKTOP) {
		XMapWindow(dpy, w);
		XUngrabServer(dpy);
		return;
	}

	c = malloc(sizeof(Client));
	/* Don't crash the window manager, just fail the operation. */
	if (!c) {
		LOG_ERROR("out of memory in new_client; limping onward\n");
		LOG_LEAVE();
		XUngrabServer(dpy);
		return;
	}
	clients_tab_order = list_prepend(clients_tab_order, c);
	clients_mapping_order = list_append(clients_mapping_order, c);
	clients_stacking_order = list_append(clients_stacking_order, c);

	c->screen = s;
	c->window = w;
	c->ignore_unmap = 0;
	c->remove = 0;

	/* Ungrab the X server as soon as possible. Now that the client is
	 * malloc()ed and attached to the list, it is safe for any subsequent
	 * X calls to raise an X error and thus flag it for removal. */
	XUngrabServer(dpy);

	c->border = opt_bw;

	update_window_type_flags(c, window_type);
	init_geometry(c);

#ifdef DEBUG
	{
		struct list *iter;
		int i = 0;
		for (iter = clients_tab_order; iter; iter = iter->next)
			i++;
		LOG_DEBUG("new window %dx%d+%d+%d, wincount=%d\n", c->width, c->height, c->x, c->y, i);
	}
#endif

	XSelectInput(dpy, c->window, ColormapChangeMask | EnterWindowMask | PropertyChangeMask);

	reparent(c);

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
		struct list *aiter = applications;
		XGetClassHint(dpy, w, class);
		while (aiter) {
			Application *a = aiter->data;
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
				if (a->is_dock) c->is_dock = 1;
#ifdef VWM
				if (a->vdesk != VDESK_NONE) c->vdesk = a->vdesk;
#endif
			}
			aiter = aiter->next;
		}
		XFree(class->res_name);
		XFree(class->res_class);
		XFree(class);
	}
	ewmh_init_client(c);
	ewmh_set_net_client_list(c->screen);
	ewmh_set_net_client_list_stacking(c->screen);

	/* Only map the window frame (and thus the window) if it's supposed
	 * to be visible on this virtual desktop. */
#ifdef VWM
	if (is_fixed(c) || c->vdesk == s->vdesk)
#endif
	{
		client_show(c);
		client_raise(c);
		if (!(window_type & (EWMH_WINDOW_TYPE_DOCK|EWMH_WINDOW_TYPE_NOTIFICATION))) {
			select_client(c);
#ifdef WARP_POINTER
			setmouse(c->window, c->width + c->border - 1,
				 c->height + c->border - 1);
#endif
			discard_enter_events(c);
		}
	}
#ifdef VWM
	else {
		set_wm_state(c, IconicState);
	}
	ewmh_set_net_wm_desktop(c);
#endif
	LOG_LEAVE();
}

/* Calls XGetWindowAttributes, XGetWMHints and XGetWMNormalHints to determine
 * window's initial geometry. */
static void init_geometry(Client *c) {
	long size_flags;
	XWindowAttributes attr;
	unsigned long *eprop;
	unsigned long nitems;
	PropMwmHints *mprop;
#ifdef VWM
	unsigned long *lprop;
#endif

	if ( (mprop = get_property(c->window, mwm_hints, mwm_hints, &nitems)) ) {
		if (nitems >= PROP_MWM_HINTS_ELEMENTS
				&& (mprop->flags & MWM_HINTS_DECORATIONS)
				&& !(mprop->decorations & MWM_DECOR_ALL)
				&& !(mprop->decorations & MWM_DECOR_BORDER)) {
			c->border = 0;
		}
		XFree(mprop);
	}

#ifdef VWM
	c->vdesk = c->screen->vdesk;
	if ( (lprop = get_property(c->window, xa_net_wm_desktop, XA_CARDINAL, &nitems)) ) {
		/* NB, Xlib not only returns a 32bit value in a long (which may
		 * not be 32bits), it also sign extends the 32bit value */
		if (nitems && valid_vdesk(lprop[0] & UINT32_MAX)) {
			c->vdesk = lprop[0] & UINT32_MAX;
		}
		XFree(lprop);
	}
#endif

	get_window_type(c);

	/* Get current window attributes */
	LOG_XENTER("XGetWindowAttributes(window=%lx)", c->window);
	XGetWindowAttributes(dpy, c->window, &attr);
	LOG_XDEBUG("(%s) %dx%d+%d+%d, bw = %d\n", map_state_string(attr.map_state), attr.width, attr.height, attr.x, attr.y, attr.border_width);
	LOG_XLEAVE();
	c->old_border = attr.border_width;
	c->oldw = c->oldh = 0;
	c->cmap = attr.colormap;

	if ( (eprop = get_property(c->window, xa_evilwm_unmaximised_horz, XA_CARDINAL, &nitems)) ) {
		if (nitems == 2) {
			c->oldx = eprop[0];
			c->oldw = eprop[1];
		}
		XFree(eprop);
	}
	if ( (eprop = get_property(c->window, xa_evilwm_unmaximised_vert, XA_CARDINAL, &nitems)) ) {
		if (nitems == 2) {
			c->oldy = eprop[0];
			c->oldh = eprop[1];
		}
		XFree(eprop);
	}

	size_flags = get_wm_normal_hints(c);

	if ((attr.width >= c->min_width) && (attr.height >= c->min_height)) {
	/* if (attr.map_state == IsViewable || (size_flags & (PSize | USSize))) { */
		c->width = attr.width;
		c->height = attr.height;
	} else {
		c->width = c->min_width;
		c->height = c->min_height;
		send_config(c);
	}
	if ((attr.map_state == IsViewable)
			|| (size_flags & (/*PPosition |*/ USPosition))) {
		c->x = attr.x;
		c->y = attr.y;
	} else {
		int xmax = DisplayWidth(dpy, c->screen->screen);
		int ymax = DisplayHeight(dpy, c->screen->screen);
		int x, y;
		get_mouse_position(&x, &y, c->screen->root);
		c->x = (x * (xmax - c->border - c->width)) / xmax;
		c->y = (y * (ymax - c->border - c->height)) / ymax;
		send_config(c);
	}

	LOG_DEBUG("window started as %dx%d +%d+%d\n", c->width, c->height, c->x, c->y);
	if (attr.map_state == IsViewable) {
		/* The reparent that is to come would trigger an unmap event */
		c->ignore_unmap++;
	}
	c->x += c->old_border;
	c->y += c->old_border;
	gravitate_border(c, -c->old_border);
	gravitate_border(c, c->border);
}

static void reparent(Client *c) {
	XSetWindowAttributes p_attr;

	p_attr.border_pixel = c->screen->bg.pixel;
	p_attr.override_redirect = True;
	p_attr.event_mask = ChildMask | ButtonPressMask | EnterWindowMask;
	c->parent = XCreateWindow(dpy, c->screen->root, c->x-c->border, c->y-c->border,
		c->width, c->height, c->border,
		DefaultDepth(dpy, c->screen->screen), CopyFromParent,
		DefaultVisual(dpy, c->screen->screen),
		CWOverrideRedirect | CWBorderPixel | CWEventMask, &p_attr);

	XAddToSaveSet(dpy, c->window);
	XSetWindowBorderWidth(dpy, c->window, 0);
	XReparentWindow(dpy, c->window, c->parent, 0, 0);
	XMapWindow(dpy, c->window);
	grab_button(c->parent, grabmask2, AnyButton);
	grab_button(c->parent, grabmask2 | altmask, AnyButton);
}

/* Get WM_NORMAL_HINTS property */
long get_wm_normal_hints(Client *c) {
	XSizeHints *size;
	long flags;
	long dummy;
	size = XAllocSizeHints();

	LOG_XENTER("XGetWMNormalHints(window=%lx)", c->window);
	XGetWMNormalHints(dpy, c->window, size, &dummy);
	debug_wm_normal_hints(size);
	LOG_XLEAVE();
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
		c->win_gravity_hint = size->win_gravity;
	} else {
		c->win_gravity_hint = NorthWestGravity;
	}
	c->win_gravity = c->win_gravity_hint;
	XFree(size);
	return flags;
}

static void update_window_type_flags(Client *c, unsigned int type) {
	c->is_dock = (type & EWMH_WINDOW_TYPE_DOCK) ? 1 : 0;
}

/* Determine window type and update flags accordingly */
void get_window_type(Client *c) {
	unsigned int type = ewmh_get_net_wm_window_type(c->window);
	update_window_type_flags(c, type);
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
		LOG_XDEBUG("");
		if (size->flags & USPosition) {
			LOG_XDEBUG_("USPosition ");
		}
		if (size->flags & USSize) {
			LOG_XDEBUG_("USSize ");
		}
		if (size->flags & PPosition) {
			LOG_XDEBUG_("PPosition ");
		}
		if (size->flags & PSize) {
			LOG_XDEBUG_("PSize");
		}
		LOG_XDEBUG_("\n");
	}
	if (size->flags & PMinSize) {
		LOG_XDEBUG("PMinSize: min_width = %d, min_height = %d\n", size->min_width, size->min_height);
	}
	if (size->flags & PMaxSize) {
		LOG_XDEBUG("PMaxSize: max_width = %d, max_height = %d\n", size->max_width, size->max_height);
	}
	if (size->flags & PResizeInc) {
		LOG_XDEBUG("PResizeInc: width_inc = %d, height_inc = %d\n",
				size->width_inc, size->height_inc);
	}
	if (size->flags & PAspect) {
		LOG_XDEBUG("PAspect: min_aspect = %d/%d, max_aspect = %d/%d\n",
				size->min_aspect.x, size->min_aspect.y,
				size->max_aspect.x, size->max_aspect.y);
	}
	if (size->flags & PBaseSize) {
		LOG_XDEBUG("PBaseSize: base_width = %d, base_height = %d\n",
				size->base_width, size->base_height);
	}
	if (size->flags & PWinGravity) {
		LOG_XDEBUG("PWinGravity: %s\n", gravity_string(size->win_gravity));
	}
}
#endif
