CC=gcc
CFLAGS=-std=c89 -Wall -Wextra -Wno-unused-parameter
LDFLAGS=
TARGET=prsed
DEBUG=no

ifeq ($(DEBUG),yes)
CFLAGS+=-g
else
CFLAGS+=-O2
endif

DESTDIR=
PREFIX=usr/local

SRCDIR=src
BINDIR=bin
OBJDIR=obj
PRNAME=$(TARGET)
VERSION=0.1

SOURCES=$(wildcard $(SRCDIR)/*.c)
OBJECTS=$(subst $(SRCDIR),$(OBJDIR),$(SOURCES:.c=.c.o))

.PHONY: all mkdirs dist dist-clean install clean
all: mkdirs $(BINDIR)/$(TARGET)

$(OBJDIR)/%.c.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR)/$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

mkdirs:
	@[ ! -d "$(BINDIR)" ] && mkdir $(BINDIR) || exit 0
	@[ ! -d "$(OBJDIR)" ] && mkdir $(OBJDIR) || exit 0

dist: dist-clean
	cd ..; tar cv --exclude=.git $(PRNAME) | xz -9 > $(PRNAME)-$(VERSION).txz

dist-clean: clean
	rm -rf bin obj

install:
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	install $(BINDIR)/$(TARGET) $(DESTDIR)/$(PREFIX)/bin/$(TARGET)

clean:
	rm -f *~ $(OBJECTS) $(BINDIR)/$(TARGET)

