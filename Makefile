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
	rm -rf tmp-pages
	git clone -q -b gh-pages git@github.com:uggedal/going.git tmp-pages
	cd tmp-pages && \
	cp -p ../docs/* . && \
	git add -u *.html *.css && \
	git commit -m "Documentation rebuild." && \
	git push origin gh-pages
	rm -rf tmp-pages

debug:
	@$(MAKE) --no-print-directory clean all CFLAGS='$(CFLAGS) -O0 -g' LDFLAGS=''
	@cppcheck --enable=all src/going.c
	@valgrind --leak-check=full --show-reachable=yes --time-stamp=yes \
		./going -d test/going.d
