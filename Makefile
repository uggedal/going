PREFIX=/usr

CFLAGS=-std=gnu99 -Os -Wall -pedantic -Werror -Wextra -Wshadow
CFLAGS+=-Wstrict-prototypes -Wunreachable-code -Waggregate-return
LDFLAGS=-s

.PHONY: clean doc debug

all: src/going
	@mv src/going .

clean:
	@rm -f going

install: all
	@install going $(PREFIX)/sbin

doc:
	@docco src/going.[ch]

publish: doc
	git checkout -q gh-pages
	cp doc/* .
	git add *.html *.css
	git commit -m "Documentation rebuild."
	git push origin gh-pages
	git checkout -q master

debug:
	@$(MAKE) --no-print-directory clean all CFLAGS='$(CFLAGS) -O0 -g' LDFLAGS=''
	@cppcheck --enable=all src/going.c
	@valgrind --leak-check=full --show-reachable=yes --time-stamp=yes \
		./going -d test/going.d
