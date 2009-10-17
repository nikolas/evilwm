/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2009 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "evilwm.h"

/*
 * Infobanner window functions
 */

static Window info_window = None;
static void infobanner_create(Client *c);
static void infobanner_update(Client *c);
static void infobanner_remove(Client *c);

static void infobanner_create(Client *c) {
	assert(info_window == None);
	info_window = XCreateWindow(dpy, c->screen->root, -4, -4, 2, 2, 0,
	                            CopyFromParent, InputOutput, CopyFromParent,
	                            CWSaveUnder | CWBackPixel,
	                            &(XSetWindowAttributes){
	                                .background_pixel = c->screen->fg.pixel,
	                                .save_under = True
	                            });
	XMapRaised(dpy, info_window);
	infobanner_update(c);
}

static void infobanner_update(Client *c) {
	char *name;
	char buf[27];
	int namew, iwinx, iwiny, iwinw, iwinh;
	int width_inc = c->width_inc, height_inc = c->height_inc;

	if (!info_window)
		return;
	snprintf(buf, sizeof(buf), "%dx%d+%d+%d", (c->width-c->base_width)/width_inc,
		(c->height-c->base_height)/height_inc,
	        c->x, c->y);
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

static void infobanner_remove(Client *c) {
	(void) c;
	if (info_window)
		XDestroyWindow(dpy, info_window);
	info_window = None;
}

/*
 * XOR decoration functions
 */

static void xor_draw_outline(Client *c) {
	int screen_x = c->x;
	int screen_y = c->y;

	XDrawRectangle(dpy, c->screen->root, c->screen->invert_gc,
		screen_x - c->border, screen_y - c->border,
		c->width + 2*c->border-1, c->height + 2*c->border-1);
}

static void xor_draw_info(Client *c) {
	int screen_x = c->x;
	int screen_y = c->y;

	char buf[27];
	snprintf(buf, sizeof(buf), "%dx%d+%d+%d", (c->width-c->base_width)/c->width_inc,
			(c->height-c->base_height)/c->height_inc, screen_x, screen_y);
	XDrawString(dpy, c->screen->root, c->screen->invert_gc,
		screen_x + c->width - XTextWidth(font, buf, strlen(buf)) - SPACE,
		screen_y + c->height - SPACE,
		buf, strlen(buf));
}

static unsigned grabbed = 0;
static void xor_init(void) {
	if (!grabbed++) {
		XGrabServer(dpy);
	}
}

static void xor_fini(void) {
	if (!--grabbed) {
		XUngrabServer(dpy);
		XSync(dpy, False);
	}
}

#define xor_template(name) \
	static void xor_ ## name ## _create(Client *c) { \
		xor_init(); \
		xor_draw_ ## name (c); \
	} \
	static void xor_ ## name ## _remove(Client *c) { \
		xor_draw_ ## name (c); \
		xor_fini(); \
	}

xor_template(outline);
xor_template(info);

/*
 * XShape decoration functions
 */
#ifdef SHAPE
static Window shape_outline_window = None;

static void shape_outline_shape(Client *c) {
	unsigned width = c->width + 2 * c->border;
	unsigned height = c->height + 2 * c->border;

	Region r = XCreateRegion();
	Region r_in = XCreateRegion();
	XRectangle rect = { .x=0, .y=0, .width = width, .height = height };
	XUnionRectWithRegion(&rect, r, r);
	rect.x = rect.y = 1 /* or could be: * c->border */;
	rect.width -= 2 /* or could be: * c->border*/;
	rect.height -= 2 /* or could be: * c->border */;
	XUnionRectWithRegion(&rect, r_in, r_in);
	XSubtractRegion(r, r_in, r);
	XShapeCombineRegion(dpy, shape_outline_window, ShapeBounding, 0,0, r, ShapeSet);
}

static void shape_outline_create(Client *c) {
	if (shape_outline_window != None)
		return;

	int screen_x = c->x - c->border;
	int screen_y = c->y - c->border;
	unsigned width = c->width + 2 * c->border;
	unsigned height = c->height + 2 * c->border;

	shape_outline_window =
		XCreateWindow(dpy, c->screen->root, screen_x, screen_y, width, height, 0,
		              CopyFromParent, InputOutput, CopyFromParent,
		              CWSaveUnder | CWBackPixel,
		              &(XSetWindowAttributes){
		                  .background_pixel = c->screen->fg.pixel,
		                  .save_under = True
		              });

	shape_outline_shape(c);
	XMapRaised(dpy, shape_outline_window);
}

static void shape_outline_remove(Client *c) {
	(void) c;
	if (shape_outline_window != None)
		XDestroyWindow(dpy, shape_outline_window);
	shape_outline_window = None;
}

static void shape_outline_update(Client *c) {
	int screen_x = c->x - c->border;
	int screen_y = c->y - c->border;
	unsigned width = c->width + 2 * c->border;
	unsigned height = c->height + 2 * c->border;
	XMoveResizeWindow(dpy, shape_outline_window, screen_x, screen_y, width, height);
	shape_outline_shape(c);
}

#endif

/*
 * Annotation method tables
 */
typedef struct {
	void (*create)(Client *c);
	void (*preupdate)(Client *c);
	void (*update)(Client *c);
	void (*remove)(Client *c);
} annotate_funcs;

const annotate_funcs x11_infobanner = {
	.create = infobanner_create,
	.preupdate = NULL,
	.update = infobanner_update,
	.remove = infobanner_remove,
};

const annotate_funcs xor_info = {
	.create = xor_info_create,
	.preupdate = xor_info_remove,
	.update = xor_info_create,
	.remove = xor_info_remove,
};

const annotate_funcs xor_outline = {
	.create = xor_outline_create,
	.preupdate = xor_outline_remove,
	.update = xor_outline_create,
	.remove = xor_outline_remove,
};

#ifdef SHAPE
const annotate_funcs shape_outline = {
	.create = shape_outline_create,
	.preupdate = NULL,
	.update = shape_outline_update,
	.remove = shape_outline_remove,
};
#else
# define shape_outline xor_outline
#endif

/* compile time defaults */
#ifdef INFOBANNER
# define ANNOTATE_INFOBANNER x11_infobanner
#else
# define ANNOTATE_INFOBANNER xor_info
#endif

#ifdef INFOBANNER_MOVERESIZE
# define ANNOTATE_MOVERESIZE x11_infobanner
#else
# define ANNOTATE_MOVERESIZE xor_info
#endif

struct annotate_ctx {
	const annotate_funcs *outline;
	const annotate_funcs *info;
};
typedef struct annotate_ctx annotate_ctx_t;

annotate_ctx_t annotate_info_ctx = { NULL, &ANNOTATE_INFOBANNER };
annotate_ctx_t annotate_drag_ctx = { &shape_outline, &ANNOTATE_MOVERESIZE };
annotate_ctx_t annotate_sweep_ctx = { &shape_outline, &ANNOTATE_MOVERESIZE };

/*
 * Annotation functions
 */
#define annotate_template(name) \
	void annotate_ ## name (Client *c, annotate_ctx_t *a) { \
		if (!a) return; \
		if (a->outline && a->outline->name) a->outline->name(c); \
		if (a->info && a->info->name) a->info->name(c); \
	}
annotate_template(create);
annotate_template(preupdate);
annotate_template(update);
annotate_template(remove);
