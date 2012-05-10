prefix=/usr
bindir=$(prefix)/sbin
mandir=$(prefix)/share/man

VERSION=0.1.0

CFLAGS=-std=gnu99 -Os -Wall -pedantic -Werror -Wextra -Wshadow
CFLAGS+=-Wstrict-prototypes -Wunreachable-code -Waggregate-return
LDFLAGS=-s

.PHONY: clean doc debug publish

all: src/going
	@mv src/going .

clean:
	@rm -f going doc/going.[ch].html doc/going.[85].html man/going.[85] \
		going-${VERSION}.tar.gz

dist: clean doc
	@mkdir -p going-${VERSION}/man
	@cp -R README.md CHANGELOG.md LICENSE Makefile src going-${VERSION}
	@cp man/going.[85] going-${VERSION}/man
	@tar -czf going-${VERSION}.tar.gz going-${VERSION}
	@rm -rf going-${VERSION}

install: all
	@install -d $(DESTDIR)$(bindir)
	@install going $(DESTDIR)$(bindir)/
	@install -d $(DESTDIR)$(mandir)/man{8,5}
	@gzip -c man/going.8 > $(DESTDIR)$(mandir)/man8/going.8.gz
	@gzip -c man/going.5 > $(DESTDIR)$(mandir)/man5/going.5.gz

uninstall:
	@rm -f $(DESTDIR)$(bindir)/going $(DESTDIR)$(mandir)/man[85]/going.[85]

doc:
	@rocco src/going.c && mv src/going.html doc/going.c.html
	@rocco src/going.h && mv src/going.html doc/going.h.html
	@sed -i '6 a \
  <style> \
    th.code, \
    td.code { \
      background-color: #eee; \
      border-left: 1px solid #dbdbdb; \
    } \
  </style>' doc/going.[ch].html
	@ronn --roff --html --organization='Going $(VERSION)' --style=toc,80c \
		man/going.[85].ronn && mv man/going.[85].html doc/

publish: doc
	@rm -rf tmp-pages
	git clone -q -b gh-pages git@github.com:uggedal/going.git tmp-pages
	cd tmp-pages && \
	cp -p ../doc/*.html . && \
	git add index.html going.[ch85].html && \
	git commit -m "Documentation rebuild." && \
	git push origin gh-pages
	@rm -rf tmp-pages

debug:
	@$(MAKE) --no-print-directory clean all CFLAGS='$(CFLAGS) -O0 -g' LDFLAGS=''
	@cppcheck --enable=all src/going.c
	@valgrind --leak-check=full --show-reachable=yes --time-stamp=yes \
		./going -d test/going.d
