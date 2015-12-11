VERSION ?= 0.7
CFLAGS ?= -Wall -W -Wno-unused-parameter -g -O2
ARCH ?= linux

BASEDIR ?= $(DESTDIR)

CFLAGS += -std=c99 -D_DEFAULT_SOURCE
CFLAGS += -D'IFUPDOWN_VERSION="$(VERSION)"'

DEFNFILES := inet.defn ipx.defn inet6.defn can.defn

OBJ := main.o addrfam.o execute.o config.o \
	$(patsubst %.defn,%.o,$(DEFNFILES)) archcommon.o arch$(ARCH).o meta.o link.o

MAN := $(patsubst %.defn,%.man,$(DEFNFILES))

DEFNFILES += meta.defn link.defn

all : ifup ifdown ifquery ifup.8 ifdown.8 ifquery.8 interfaces.5

.PHONY : all clean distclean
.SECONDARY: link.c ipx.c can.c meta.c inet6.c inet.c

install :
	install -m 0755 -d     ${BASEDIR}/sbin
	install -m 0755 ifup   ${BASEDIR}/sbin
	ln -s /sbin/ifup ${BASEDIR}/sbin/ifdown
	ln -s /sbin/ifup ${BASEDIR}/sbin/ifquery
	install -D -m 0755 settle-dad.sh $(BASEDIR)/lib/ifupdown/settle-dad.sh
	install -D -m 0755 wait-for-ll6.sh $(BASEDIR)/lib/ifupdown/wait-for-ll6.sh

clean :
	rm -f *.o $(patsubst %.defn,%.c,$(DEFNFILES)) *~
	rm -f $(patsubst %.defn,%.man,$(DEFNFILES))
	rm -f ifup ifdown ifquery interfaces.5 ifdown.8 ifquery.8
	-rm -f ./tests/testcase.* ./tests/up*

distclean : clean

ifup: $(OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(OUTPUT_OPTION)

ifdown: ifup
	ln -sf ifup ifdown

ifquery: ifup
	ln -sf ifup ifquery

ARCH := $(shell dpkg-architecture -qDEB_HOST_ARCH_OS)
check:
	@echo running ./tests/testbuild-$(ARCH)
	@if ! exec ./tests/testbuild-$(ARCH); then \
	     echo '=================================================='; \
	     echo 'AUTOMATIC TESTS FAILED -- Something built wrong or'; \
	     echo 'there is a bug in the code!!! Either way something'; \
	     echo 'is completely screwed up!!! File a bug!'; \
	     echo ''; \
	     echo 'Aborting build.'; \
	     echo '=================================================='; \
	     exit 1; \
	fi

interfaces.5: interfaces.5.pre $(MAN)
	sed $(foreach man,$(MAN),-e '/^##ADDRESSFAM##$$/r $(man)') \
	     -e '/^##ADDRESSFAM##$$/d' < $< > $@	

ifdown.8 ifquery.8: ifup.8
	ln -sf $< $@

%.5.ps: %.5
	groff -mandoc -Tps $< > $@

%.8.ps: %.8
	groff -mandoc -Tps $< > $@

%.c : %.defn defn2c.pl
	./defn2c.pl $< > $@

%.man: %.defn defn2man.pl
	./defn2man.pl $< > $@
