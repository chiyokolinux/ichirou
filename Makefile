# this file is part of ichirou
# Copyright (c) 2020-2021 Emily <elishikawa@jagudev.net>
#
# ichirou is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ichirou is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ichirou.  If not, see <https://www.gnu.org/licenses/>.

include config.mk

INITOBJ = ichirou.o
INITBIN = ichirou

SERVOBJ = kanrisha.o
SERVBIN = kanrisha

CONFS = confs/rc.conf confs/rc.init confs/rc.local confs/rc.postinit confs/rc.shutdown
BOOTSCRIPTS = confs/rc.init confs/rc.local confs/rc.postinit confs/rc.shutdown
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

confs/rc.conf: ;

confs/%:
	sed '/^# .* script for ichirou$$/d; /^# Copyright (c) .* Emily .*$$/d; /^# for license and .*$$/d; /^# See LICENSE file.*$$/d' $@.in > $@

scripts/%:
	sed '/^# .* script for ichirou$$/d; /^# Copyright (c) .* Emily .*$$/d; /^# for license and .*$$/d; /^# See LICENSE file.*$$/d' $@.in > $@

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
	rm -f $(INITBIN) $(SERVBIN) $(INITOBJ) $(SERVOBJ) $(BOOTSCRIPTS) $(SCRIPTS) ichirou-$(VERSION).tar.gz

.SUFFIXES: .def.h

.def.h.h:
	cp $< $@

.PHONY:
	all install uninstall dist clean
