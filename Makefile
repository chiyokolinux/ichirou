# Makefile for ichirou & kanrisha
# See LICENSE file for license and copyright information.

include config.mk

INITOBJ = ichirou.o
INITBIN = ichirou

SERVOBJ = kanrisha.o
SERVBIN = kanrisha

CONFS = confs/rc.conf confs/rc.init confs/rc.local confs/rc.postinit confs/rc.shutdown
SCRIPTS = scripts/halt scripts/hibernate scripts/reboot scripts/shutdown

all: $(INITBIN) $(SERVBIN)

$(INITBIN): $(INITOBJ)
	$(CC) $(LDFLAGS) -o $@ $(INITOBJ) $(LDLIBS)

$(SERVBIN): $(SERVOBJ)
	$(CC) $(LDFLAGS) -o $@ $(SERVOBJ) $(LDLIBS)

$(INITOBJ): config.h
$(SERVOBJ): config.h

confs: $(CONFS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -Dm700 $(SCRIPTS) $(DESTDIR)$(PREFIX)/bin/

scripts: $(SCRIPTS)
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	install -Dm755 $(SCRIPTS) $(DESTDIR)$(PREFIX)/sbin/

confs/%:
	sed '/^# .* script for ichirou$$/d; /^# See LICENSE file.*$$/d' $@.in > $@

scripts/%:
	sed '/^# .* script for ichirou$$/d; /^# See LICENSE file.*$$/d' $@.in > $@

install: all confs scripts
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	mkdir -vp $(DESTDIR)$(PREFIX)/etc/kanrisha.d/{enabled,available}
	install -Dm700 $(INITBIN) $(DESTDIR)$(PREFIX)/sbin/$(INITBIN)
	install -Dm755 $(SERVBIN) $(DESTDIR)$(PREFIX)/sbin/$(SERVBIN)
	ln -s $(DESTDIR)$(PREFIX)/sbin/$(INITBIN) $(DESTDIR)$(PREFIX)/sbin/init

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/sbin/$(INITBIN) $(DESTDIR)$(PREFIX)/sbin/$(SERVBIN)

dist: clean
	mkdir -p ichirou-$(VERSION)
	mkdir -p ichirou-$(VERSION)/confs
	mkdir -p ichirou-$(VERSION)/scripts
	cp LICENSE Makefile README config.def.h config.mk ichirou.c kanrisha.c ichirou-$(VERSION)
	cp confs/rc.conf confs/rc.init confs/rc.local confs/rc.postinit \
	   confs/rc.shutdown ichirou-$(VERSION)/confs
	cp scripts/halt scripts/hibernate scripts/reboot scripts/shutdown ichirou-$(VERSION)/scripts
	tar -cf ichirou-$(VERSION).tar ichirou-$(VERSION)
	gzip ichirou-$(VERSION).tar
	rm -rf ichirou-$(VERSION)

clean:
	rm -f $(INITBIN) $(SERVBIN) $(INITOBJ) $(SERVOBJ) ichirou-$(VERSION).tar.gz

.SUFFIXES: .def.h

.def.h.h:
	cp $< $@

.PHONY:
	all install uninstall dist clean
