/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2015 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

#ifndef __LIST_H__
#define __LIST_H__

struct list {
	struct list *next;
	void *data;
};

/* Each of these return the new pointer to the head of the list: */
struct list *list_insert_before(struct list *list, struct list *before, void *data);
struct list *list_prepend(struct list *list, void *data);
struct list *list_append(struct list *list, void *data);
struct list *list_delete(struct list *list, void *data);
struct list *list_to_head(struct list *list, void *data);
struct list *list_to_tail(struct list *list, void *data);

/* Returns element in list: */
struct list *list_find(struct list *list, void *data);

#endif  /* def __LIST_H__ */
