/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2005 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include "evilwm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef MWM_HINTS
static PropMwmHints *get_mwm_hints(Window);
#endif
static void init_position(Client *c);
static void reparent(Client *c);

void make_new_client(Window w, ScreenInfo *s) {
	Client *c;
	XWindowAttributes attr;
	long dummy;
	XWMHints *hints;
	char *name;
	XClassHint *class;
#ifdef MWM_HINTS
	PropMwmHints *mhints;
#endif

	XGrabServer(dpy);

	c = malloc(sizeof(Client));
	/* Don't crash the window manager, just fail the operation. */
	if (!c) {
#ifdef STDIO
		fprintf(stderr, "out of memory in new_client; limping onward\n");
#endif
		return;
	}
	/* We do this first of all as a test to see if the window actually
	 * still exists by the time we've got to create a client structure
	 * for it (sometimes they vanish too quickly or something, and lots
	 * of pain ensues). */
	initialising = w;
	XFetchName(dpy, w, &name);
	/* If 'initialising' is now set to None, that means doing the
	 * XFetchName raised BadWindow - the window has been removed before
	 * we got a chance to grab the server. */
	if (initialising == None) {
#ifdef DEBUG
		fprintf(stderr, "make_new_client() : XError occurred for initialising window - aborting...\n");
#endif
		free(c);
		XSync(dpy, False);
		XUngrabServer(dpy);
		return;
	}
	initialising = None;
	c->next = head_client;
	head_client = c;

	/* initialise(c, w); */
	c->screen = s;
	c->window = w;
	c->ignore_unmap = 0;

	c->size = XAllocSizeHints();
#ifdef XDEBUG
	fprintf(stderr, "XGetWMNormalHints(); ");
#endif
	XGetWMNormalHints(dpy, c->window, c->size, &dummy);
	/* Jon Perkin reported a crash with an app called 'sunpci' which we
	 * traced to getting divide-by-zeros because it sets PResizeInc
	 * but then has increments as 0.  So we check for 0s here and set them
	 * to sensible defaults. */
	if (c->size->width_inc == 0)
		c->size->width_inc = 1;
	if (c->size->height_inc == 0)
		c->size->height_inc = 1;

	XGetWindowAttributes(dpy, c->window, &attr);

	c->x = attr.x;
	c->y = attr.y;
	c->width = attr.width;
	c->height = attr.height;
	c->border = opt_bw;
	c->oldw = c->oldh = 0;

#ifdef MWM_HINTS
	if ((mhints = get_mwm_hints(c->window))) {
		if (mhints->flags & MWM_HINTS_DECORATIONS
				&& !(mhints->decorations & MWM_DECOR_ALL)) {
			if (!(mhints->decorations & MWM_DECOR_BORDER)) {
				c->border = 0;
			}
		}
		XFree(mhints);
	}
#endif
	/* If we don't have MWM_HINTS (ie, lesstif) for a client to tell us
	 * it has no border, I include this *really blatant hack* to remove
	 * the border from XMMS. */
	if (name) {
#ifndef MWM_HINTS
		if (!strncmp("XMMS", name, 4))
			c->border = 0;
#endif
		XFree(name);  /* But we want to free this anyway... */
	}

#ifdef COLOURMAP
	c->cmap = attr.colormap;
#endif
#ifdef VWM
	c->vdesk = vdesk;
#endif
#ifdef DEBUG
	{
		Client *p;
		int i = 0;
		for (p = head_client; p; p = p->next)
			i++;
		fprintf(stderr, "make_new_client() : new window %dx%d+%d+%d, wincount=%d\n", c->width, c->height, c->x, c->y, i);
	}
#endif

	/* c->size = XAllocSizeHints();
	XGetWMNormalHints(dpy, c->window, c->size, &dummy); */

	if (attr.map_state == IsViewable) {
		c->ignore_unmap++;
	} else {
		init_position(c);
		if ((hints = XGetWMHints(dpy, w))) {
			if (hints->flags & StateHint)
				set_wm_state(c, hints->initial_state);
			XFree(hints);
		}
	}

	/* client is initialised */

	gravitate(c);

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
					c->width = a->width * ((c->size->flags & PResizeInc) ? c->size->width_inc : 1);
				if (a->geometry_mask & HeightValue)
					c->height = a->height * ((c->size->flags & PResizeInc) ? c->size->height_inc : 1);
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

#ifndef MOUSE
	setmouse(c->window, c->width + c->border - 1,
			c->height + c->border - 1);
#endif
}

static void init_position(Client *c) {
#ifdef MOUSE
	int x, y;
#endif
	int xmax = DisplayWidth(dpy, c->screen->screen);
	int ymax = DisplayHeight(dpy, c->screen->screen);

	/*
	if (c->size->flags & (/+PSize | +/USSize)) {
		c->width = c->size->width;
		c->height = c->size->height;
	}
	*/

	if (c->width < MINSIZE) c->width = MINSIZE;
	if (c->height < MINSIZE) c->height = MINSIZE;
	if (c->width > xmax) c->width = xmax;
	if (c->height > ymax) c->height = ymax;

	if (c->size->flags & (/*PPosition | */USPosition)) {
		c->x = c->size->x;
		c->y = c->size->y;
		if (c->x < 0 || c->y < 0 || c->x > xmax || c->y > ymax)
			c->x = c->y = c->border;
	} else {
#ifdef MOUSE
		get_mouse_position(&x, &y, c->screen->root);
		c->x = (x * (xmax - c->border - c->width)) / xmax;
		c->y = (y * (ymax - c->border - c->height)) / ymax;
#else
		c->x = c->y = 0;
#endif
	}
	/* reposition if maximised horizontally or vertically */
	if (c->x == 0 && c->width == xmax)
		c->x = -c->border;
	if (c->y == 0 && c->height == ymax)
		c->y = -c->border;
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

	send_config(c);
}

#ifdef MWM_HINTS
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
		return (PropMwmHints *)data;
	}
	return NULL;
}
#endif
