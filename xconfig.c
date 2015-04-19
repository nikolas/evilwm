/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2015 Ciaran Anscomb
 * see README for license and other details. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xconfig.h"

/* Break a space-separated string into an array of strings.
 * Backslash escapes next character. */
static char **split_string(const char *arg) {
	int nelem = 0, elem = 0;
	char **list = NULL;
	char *string, *head, *tail;

	head = tail = string = malloc(strlen(arg) + 1);
	if (string == NULL)
		return NULL;

	for (;;) {
		if (*arg == '\\' && *(arg+1) != 0) {
			arg++;
			*(tail++) = *(arg++);
		} else if (*arg == 0 || isspace(*arg)) {
			*tail = 0;
			if (*head) {
				if ((elem + 1) >= nelem) {
					char **nlist;
					nelem += 4;
					nlist = realloc(list, nelem * sizeof(char *));
					if (nlist == NULL) {
						if (list)
							free(list);
						free(string);
						return NULL;
					}
					list = nlist;
				}
				list[elem++] = head;
				tail++;
				head = tail;
			}
			while (isspace(*arg)) {
				arg++;
			}
			if (*arg == 0) {
				break;
			}
		} else {
			*(tail++) = *(arg++);
		}
	}
	if (elem == 0) {
		free(string);
		return NULL;
	}
	list[elem] = NULL;
	return list;
}

static struct xconfig_option *find_option(struct xconfig_option *options,
		const char *opt) {
	int i;
	for (i = 0; options[i].type != XCONFIG_END; i++) {
		if (0 == strcmp(options[i].name, opt)) {
			return &options[i];
		}
	}
	return NULL;
}

static void set_option(struct xconfig_option *option, const char *arg) {
	switch (option->type) {
		case XCONFIG_BOOL:
			*(int *)option->dest = 1;
			break;
		case XCONFIG_INT:
			*(int *)option->dest = strtol(arg, NULL, 0);
			break;
		case XCONFIG_STRING:
			*(char **)option->dest = strdup(arg);
			break;
		case XCONFIG_STR_LIST:
			*(char ***)option->dest = split_string(arg);
			break;
		case XCONFIG_CALL_0:
			((void (*)(void))option->dest)();
			break;
		case XCONFIG_CALL_1:
			((void (*)(const char *))option->dest)(arg);
			break;
		default:
			break;
	}
}

/* Simple parser: one directive per line, "option argument" */
enum xconfig_result xconfig_parse_file(struct xconfig_option *options,
		const char *filename) {
	struct xconfig_option *option;
	char buf[256];
	char *line, *opt, *arg;
	FILE *cfg;
	cfg = fopen(filename, "r");
	if (cfg == NULL) return XCONFIG_FILE_ERROR;
	while ((line = fgets(buf, sizeof(buf), cfg))) {
		while (isspace((int)*line))
			line++;
		if (*line == 0 || *line == '#')
			continue;
		opt = strtok(line, "\t\n\v\f\r =");
		if (opt == NULL) continue;
		option = find_option(options, opt);
		if (option == NULL) {
			fclose(cfg);
			return XCONFIG_BAD_OPTION;
		}
		if (option->type == XCONFIG_STR_LIST) {
			/* special case: spaces here mean something */
			arg = strtok(NULL, "\n\v\f\r");
			while (isspace(*arg) || *arg == '=') {
				arg++;
			}
		} else {
			arg = strtok(NULL, "\t\n\v\f\r =");
		}
		set_option(option, arg);
	}
	fclose(cfg);
	return XCONFIG_OK;
}

enum xconfig_result xconfig_parse_cli(struct xconfig_option *options,
		int argc, char **argv, int *argn) {
	struct xconfig_option *option;
	int _argn;
	char *optstr;
	_argn = argn ? *argn : 1;

	while (_argn < argc) {
		if (argv[_argn][0] != '-') {
			break;
		}
		if (0 == strcmp("--", argv[_argn])) {
			_argn++;
			break;
		}
		optstr = argv[_argn]+1;
		if (*optstr == '-') optstr++;
		option = find_option(options, optstr);
		if (option == NULL) {
			if (argn) *argn = _argn;
			return XCONFIG_BAD_OPTION;
		}
		if (option->type == XCONFIG_BOOL
				|| option->type == XCONFIG_CALL_0) {
			set_option(option, NULL);
			_argn++;
			continue;
		}
		if ((_argn + 1) >= argc) {
			if (argn) *argn = _argn;
			return XCONFIG_MISSING_ARG;
		}
		set_option(option, argv[_argn+1]);
		_argn += 2;
	}
	if (argn) *argn = _argn;
	return XCONFIG_OK;
}
