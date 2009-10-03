# If you don't use CC 
CC       = gcc

# Edit this line if you don't want evilwm to install under /usr.
# Note that $(DESTDIR) is used by the Debian build process.
prefix = $(DESTDIR)/usr

INSTALL = install
INSTALL_STRIP = -s
INSTALL_DIR = $(INSTALL) -d -m 0755
INSTALL_FILE = $(INSTALL) -m 0644
INSTALL_PROGRAM = $(INSTALL) -m 0755 $(INSTALL_STRIP)

XROOT    = /usr/X11R6
INCLUDES = -I$(XROOT)/include
LDPATH   = -L$(XROOT)/lib
LIBS     = -lX11

DEFINES  = $(EXTRA_DEFINES)
# Configure evilwm by editing the following DEFINES lines.  You can also
# add options by setting EXTRA_DEFINES on the make(1) command line,
# e.g., make EXTRA_DEFINES="-DDEBUG".

# Uncomment to enable solid window drags.  This can be slow on old systems.
DEFINES += -DSOLIDDRAG
# Enable a more informative and clear banner to be displayed on Ctrl+Alt+I.
DEFINES += -DINFOBANNER
# Uncomment to show the same banner on moves and resizes.  Note this can
# make things very SLOW!
#DEFINES += -DINFOBANNER_MOVERESIZE

# To support virtual desktops, uncomment the following line.
DEFINES += -DVWM

# To support shaped windows properly, uncomment the following two lines:
DEFINES += -DSHAPE
LIBS    += -lXext

# Uncomment to support the Xrandr extension (thanks, Yura Semashko).
#
# Be sure that libXrandr is in your library search directory (e.g., under
# Solaris, it is in /usr/X11R6/lib, so can be built against with the default
# XPATH above, but won't necessarily be in the library search path).
DEFINES += -DRANDR
LIBS    += -lXrandr

# Uncomment for mouse support.  You probably want this.
DEFINES += -DMOUSE

# Uncomment to compile in certain text messages like help.  You want this too
# unless you *really* want to trim the bytes.
# Note that snprintf(3) is always part of the build.
DEFINES += -DSTDIO

# Uncomment the following line if you want to use Ctrl+Alt+q to kill windows
# instead of Ctrl+Alt+Escape (or just set it to what you want).  This is
# useful under XFree86/Cygwin and VMware (probably)
#DEFINES += -DKEY_KILL=XK_q

# Print whatever debugging messages I've left in this release.
#DEFINES += -DDEBUG  # miscellaneous debugging

# ----- You shouldn't need to change anything under this line ------ #

version = 1.1.0

distname = evilwm-$(version)

#DEFINES += -DXDEBUG  # show some X calls

DEFINES += -DVERSION=\"$(version)\" $(DEBIAN)
CFLAGS  ?= -Os
#CFLAGS  ?= -g
CFLAGS  += $(DEFINES) $(INCLUDES) -Wall -W -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline -Wwrite-strings -Wundef -Wsign-compare -Wmissing-prototypes -Wredundant-decls
LDFLAGS += $(LDPATH) $(LIBS)

HEADERS  = evilwm.h log.h xconfig.h
SRCS     = client.c events.c list.c main.c misc.c new.c screen.c ewmh.c \
	xconfig.c
OBJS     = $(SRCS:.c=.o)

.PHONY: all install dist debuild clean

all: evilwm

evilwm: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

install: evilwm
	if [ -f evilwm.exe ]; then mv evilwm.exe evilwm; fi
	$(INSTALL_DIR) $(prefix)/bin
	$(INSTALL_DIR) $(prefix)/share/man/man1
	$(INSTALL_DIR) $(prefix)/share/applications
	$(INSTALL_DIR) $(prefix)/share/xsessions
	$(INSTALL_PROGRAM) evilwm $(prefix)/bin
	$(INSTALL_FILE) evilwm.1 $(prefix)/share/man/man1
	$(INSTALL_FILE) applications.desktop $(prefix)/share/applications
	$(INSTALL_FILE) xsession.desktop $(prefix)/share/xsessions

dist:
	darcs dist --dist-name $(distname)
	mv $(distname).tar.gz ..

debuild: dist
	-cd ..; rm -rf $(distname)/ $(distname).orig/
	cd ..; mv $(distname).tar.gz evilwm_$(version).orig.tar.gz
	cd ..; tar xfz evilwm_$(version).orig.tar.gz
	cp -a debian ../$(distname)/
	rm -rf ../$(distname)/debian/_darcs/
	cd ../$(distname); debuild

clean:
	rm -f evilwm evilwm.exe $(OBJS)
