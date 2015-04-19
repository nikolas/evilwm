/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2015 Ciaran Anscomb <evilwm@6809.org.uk>
 * see README for license and other details. */

/* Basic linked list handling code.  Operations that modify the list
 * return the new list head.  */

#include <stdlib.h>
#include "list.h"

/* Wrap data in a new list container */
static struct list *list_new(void *data) {
	struct list *new;
	new = malloc(sizeof(struct list));
	if (!new) return NULL;
	new->next = NULL;
	new->data = data;
	return new;
}

/* Insert new data before given position */
struct list *list_insert_before(struct list *list, struct list *before, void *data) {
	struct list *elem = list_new(data);
	struct list *iter;
	if (!elem) return list;
	if (!list) return elem;
	elem->next = before;
	if (before == list) return elem;
	for (iter = list; iter; iter = iter->next) {
		if (!iter->next || iter->next == before) {
			iter->next = elem;
			break;
		}
	}
	return list;
}

/* Add new data to head of list */
struct list *list_prepend(struct list *list, void *data) {
	return list_insert_before(list, list, data);
}

/* Add new data to tail of list */
struct list *list_append(struct list *list, void *data) {
	return list_insert_before(list, NULL, data);
}

/* Delete list element containing data */
struct list *list_delete(struct list *list, void *data) {
	struct list **elemp;
	if (!data) return list;
	for (elemp = &list; *elemp; elemp = &(*elemp)->next) {
		if ((*elemp)->data == data) break;
	}
	if (*elemp) {
		struct list *elem = *elemp;
		*elemp = elem->next;
		free(elem);
	}
	return list;
}

/* Move existing list element containing data to head of list */
struct list *list_to_head(struct list *list, void *data) {
	if (!data) return list;
	list = list_delete(list, data);
	return list_prepend(list, data);
}

/* Move existing list element containing data to tail of list */
struct list *list_to_tail(struct list *list, void *data) {
	if (!data) return list;
	list = list_delete(list, data);
	return list_append(list, data);
}

/* Find list element containing data */
struct list *list_find(struct list *list, void *data) {
	struct list *elem;
	for (elem = list; elem; elem = elem->next) {
		if (elem->data == data)
			return elem;
	}
	return NULL;
}
