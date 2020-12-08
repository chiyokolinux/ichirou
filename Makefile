include config.mk

INITOBJ = ichirou.o
INITBIN = ichirou

SERVOBJ = kanrisha.o
SERVBIN = kanrisha

CONFS = confs/rc.conf confs/rc.init confs/rc.local confs/rc.postinit confs/rc.shutdown

all: $(INITBIN) $(SERVBIN) confs

$(INITBIN): $(INITOBJ)
	$(CC) $(LDFLAGS) -o $@ $(INITOBJ) $(LDLIBS)

$(SERVBIN): $(SERVOBJ)
	$(CC) $(LDFLAGS) -o $@ $(SERVOBJ) $(LDLIBS)

$(INITOBJ): config.h
$(SERVOBJ): config.h

confs:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(CONFS) $(DESTDIR)$(PREFIX)/bin

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	mkdir -vp $(DESTDIR)$(PREFIX)/etc/kanrisha.d/{enabled,available}
	cp -f $(INITBIN) $(DESTDIR)$(PREFIX)/sbin
	cp -f $(SERVBIN) $(DESTDIR)$(PREFIX)/sbin
	ln -s $(DESTDIR)$(PREFIX)/sbin/$(INITBIN) $(DESTDIR)$(PREFIX)/sbin/init

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/sbin/$(INITBIN) $(DESTDIR)$(PREFIX)/sbin/$(SERVBIN)

dist: clean
	mkdir -p ichirou-$(VERSION)
	mkdir -p ichirou-$(VERSION)/confs
	cp LICENSE Makefile README config.def.h config.mk ichirou.c kanrisha.c ichirou-$(VERSION)
	cp confs/rc.conf confs/rc.init confs/rc.local confs/rc.postinit \
	   confs/rc.shutdown ichirou-$(VERSION)/confs
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
