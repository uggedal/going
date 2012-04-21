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
	@rm -f going src/going.[ch].html man/going.[15].html man/going.[15]

install: all
	@install -d $(DESTDIR)$(bindir)
	@install going $(DESTDIR)$(bindir)/
	@install -d $(DESTDIR)$(mandir)/man{1,5}
	@gzip -c man/going.1 > $(DESTDIR)$(mandir)/man1/going.1.gz
	@gzip -c man/going.5 > $(DESTDIR)$(mandir)/man5/going.5.gz

uninstall:
	@rm -f $(DESTDIR)$(bindir)/going $(DESTDIR)$(mandir)/man[15]/going.[15]

doc:
	@rocco src/going.c && mv src/going{,.c}.html
	@rocco src/going.h && mv src/going{,.h}.html
	@ronn --roff --html --organization='Going $(VERSION)' --style=toc \
		man/going.[15].ronn

publish: doc
	@rm -rf tmp-pages
	git clone -q -b gh-pages git@github.com:uggedal/going.git tmp-pages
	cd tmp-pages && \
	cp -p ../src/going.[ch].html . && \
	cp -p ../man/going.[15].html . && \
	git add going.[ch15].html && \
	git commit -m "Documentation rebuild." && \
	git push origin gh-pages
	@rm -rf tmp-pages

debug:
	@$(MAKE) --no-print-directory clean all CFLAGS='$(CFLAGS) -O0 -g' LDFLAGS=''
	@cppcheck --enable=all src/going.c
	@valgrind --leak-check=full --show-reachable=yes --time-stamp=yes \
		./going -d test/going.d
