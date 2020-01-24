CC=gcc
CFLAGS=-std=c89 -Wall -Wextra -Wno-unused-parameter
CFLAGS+=$(shell pkg-config prs --cflags)
LDFLAGS=$(shell pkg-config prs --libs)
TARGET=prsed

DESTDIR=
PREFIX=usr/local

SRCDIR=src
BINDIR=bin
OBJDIR=obj
PRNAME=$(basename $(SRCDIR))
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
	cd ..; tar cv $(PRNAME) | xz -9 > $(PRNAME)-$(VERSION).txz

dist-clean: clean
	rm -rf bin obj

install:
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	install $(BINDIR)/$(TARGET) $(DESTDIR)/$(PREFIX)/bin/$(TARGET)

clean:
	rm -f *~ $(OBJECTS) $(BINDIR)/$(TARGET)

