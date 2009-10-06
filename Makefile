############################################################################
# Installation paths

prefix = /usr
bindir = $(prefix)/bin
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man
man1dir = $(mandir)/man1
desktopfilesdir = $(datarootdir)/applications

############################################################################
# Features

# Uncomment to enable info banner on holding Ctrl+Alt+I.
CPPFLAGS += -DINFOBANNER

# Uncomment to show the same banner on moves and resizes.  Can be SLOW!
#CPPFLAGS += -DINFOBANNER_MOVERESIZE

# Uncomment for mouse support.  Recommended.
CPPFLAGS += -DMOUSE

# Uncomment to support the Xrandr extension (thanks, Yura Semashko).
CPPFLAGS += -DRANDR
LDLIBS   += -lXrandr

# Uncomment to support shaped windows.
CPPFLAGS += -DSHAPE
LDLIBS   += -lXext

# Uncomment to enable solid window drags.  This can be slow on old systems.
CPPFLAGS += -DSOLIDDRAG

# Uncomment to compile in certain text messages like help.  Recommended.
CPPFLAGS += -DSTDIO

# Uncomment to support virtual desktops.
CPPFLAGS += -DVWM

# Uncomment to use Ctrl+Alt+q instead of Ctrl+Alt+Escape.  Useful for Cygwin.
#CPPFLAGS += -DKEY_KILL=XK_q

# Uncomment to include whatever debugging messages I've left in this release.
#CPPFLAGS += -DDEBUG   # miscellaneous debugging
#CPPFLAGS += -DXDEBUG  # show some X calls

############################################################################
# Include file and library paths

# Most Linux distributions don't separate out X11 from the rest of the
# system, but some other OSs still require extra information:

# Solaris 10:
#CPPFLAGS += -I/usr/X11/include
#LDFLAGS  += -R/usr/X11/lib -L/usr/X11/lib

# Solaris <= 9 doesn't support RANDR feature above, so disable it there

############################################################################
# Build tools

# Change this if you don't use gcc:
CC = gcc

# Override if desired:
CFLAGS = -Os

# For Cygwin:
#EXEEXT = .exe

# Override INSTALL_STRIP if you don't want a stripped binary
INSTALL = install
INSTALL_STRIP = -s
INSTALL_DIR = $(INSTALL) -d -m 0755
INSTALL_FILE = $(INSTALL) -m 0644
INSTALL_PROGRAM = $(INSTALL) -m 0755 $(INSTALL_STRIP)

############################################################################
# You shouldn't need to change anything beyond this point

version = 1.1.0
distname = evilwm-$(version)

# Generally shouldn't be overridden:
EVILWM_CPPFLAGS = -DVERSION=\"$(version)\" -Wall -W -Wstrict-prototypes \
	-Wpointer-arith -Wcast-align -Wcast-qual -Wshadow -Waggregate-return \
	-Wnested-externs -Winline -Wwrite-strings -Wundef -Wsign-compare \
	-Wmissing-prototypes -Wredundant-decls
EVILWM_LDLIBS = -lX11

HEADERS = evilwm.h keymap.h list.h log.h xconfig.h
OBJS = client.o events.o ewmh.o list.o main.o misc.o new.o screen.o xconfig.o

.PHONY: all
all: evilwm$(EXEEXT)

$(OBJS): $(HEADERS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EVILWM_CPPFLAGS) -c $<

evilwm$(EXEEXT): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(EVILWM_LDLIBS) $(LDLIBS)

.PHONY: install
install: evilwm$(EXEEXT)
	$(INSTALL_DIR) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) evilwm$(EXEEXT) $(DESTDIR)$(bindir)/
	$(INSTALL_DIR) $(DESTDIR)$(man1dir)
	$(INSTALL_FILE) evilwm.1 $(DESTDIR)$(man1dir)/
	$(INSTALL_DIR) $(DESTDIR)$(desktopfilesdir)
	$(INSTALL_FILE) evilwm.desktop $(DESTDIR)$(desktopfilesdir)/

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(bindir)/evilwm$(EXEEXT)
	rm -f $(DESTDIR)$(man1dir)/evilwm.1
	rm -f $(DESTDIR)$(desktopfilesdir)/evilwm.desktop

.PHONY: dist
dist:
	darcs dist --dist-name $(distname)
	mv $(distname).tar.gz ..

.PHONY: debuild
debuild: dist
	-cd ..; rm -rf $(distname)/ $(distname).orig/
	cd ..; mv $(distname).tar.gz evilwm_$(version).orig.tar.gz
	cd ..; tar xfz evilwm_$(version).orig.tar.gz
	cp -a debian ../$(distname)/
	rm -rf ../$(distname)/debian/_darcs/
	cd ../$(distname); debuild

.PHONY: clean
clean:
	rm -f evilwm$(EXEEXT) $(OBJS)
