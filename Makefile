# If you don't use CC 
CC       = gcc

# Edit this line if you don't want evilwm to install under /usr.
# Note that $(DESTDIR) is used by the Debian build process.
prefix = $(DESTDIR)/usr

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
LIBS	+= -lXext

# Uncomment for mouse support.  You probably want this.
DEFINES += -DMOUSE

# Uncomment for snap-to-border support (thanks, Neil Drumm)
# Start evilwm with -snap num to enable (num is proximity in pixels to snap to)
DEFINES += -DSNAP

# Uncomment to compile in certain text messages like help.  You want this too
# unless you *really* want to trim the bytes.
# Note that snprintf(3) is always part of the build.
DEFINES += -DSTDIO

# You can save a few bytes if you know you won't need colour map support
# (e.g., for 16 or more bit displays)
DEFINES += -DCOLOURMAP

# Uncomment the following line if you want to use Ctrl+Alt+q to kill windows
# instead of Ctrl+Alt+Escape (or just set it to what you want).  This is
# useful under XFree86/Cygwin and VMware (probably)
#DEFINES += -DKEY_KILL=XK_q

# Print whatever debugging messages I've left in this release.
#DEFINES += -DDEBUG	# miscellaneous debugging

# ----- You shouldn't need to change anything under this line ------ #

version = 0.99.25

distname = evilwm-$(version)

#DEFINES += -DXDEBUG	# show some X calls

DEFINES += -DVERSION=\"$(version)\" $(DEBIAN)
CFLAGS  += $(INCLUDES) $(DEFINES) -Os -Wall
#CFLAGS  += $(INCLUDES) $(DEFINES) -g -Wall
CFLAGS  += -W -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline -Wwrite-strings -Wundef -Wsign-compare -Wmissing-prototypes -Wredundant-decls
LDFLAGS += $(LDPATH) $(LIBS)

HEADERS  = evilwm.h log.h
SRCS     = client.c events.c main.c misc.c new.c screen.c ewmh.c
OBJS     = $(SRCS:.c=.o)

.PHONY: all install dist debuild clean

all: evilwm

evilwm: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

install: evilwm
	if [ -f evilwm.exe ]; then mv evilwm.exe evilwm; fi
	mkdir -p $(prefix)/bin $(prefix)/share/man/man1
	install -s evilwm $(prefix)/bin
	install evilwm.1 $(prefix)/share/man/man1
	#gzip -9 $(prefix)/share/man/man1/evilwm.1

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
