/* evilwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2015 Ciaran Anscomb
 * see README for license and other details. */

#ifndef __XCONFIG_H__
#define __XCONFIG_H__

enum xconfig_result {
	XCONFIG_OK = 0,
	XCONFIG_BAD_OPTION,
	XCONFIG_MISSING_ARG,
	XCONFIG_FILE_ERROR
};

enum xconfig_option_type {
	XCONFIG_BOOL,
	XCONFIG_INT,
	XCONFIG_STRING,
	XCONFIG_STR_LIST,
	XCONFIG_CALL_0,
	XCONFIG_CALL_1,
	XCONFIG_END
};

struct xconfig_option {
	enum xconfig_option_type type;
	const char *name;
	void *dest;
};

enum xconfig_result xconfig_parse_file(struct xconfig_option *options,
		const char *filename);

enum xconfig_result xconfig_parse_cli(struct xconfig_option *options,
		int argc, char **argv, int *argn);

#endif  /* __XCONFIG_H__ */
