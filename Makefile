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
	git fetch -q origin
	rev=$$(git rev-parse origin/gh-pages)
	git clone -q -b gh-pages . tmp-pages
	cd tmp-pages && \
	git reset --hard $$rev && \
	cp -p ../docs/* . && \
	git add *.html *.css && \
	git commit -m "Documentation rebuild." && \
	git push git@github.com:uggedal/going.git gh-pages
	rm -rf tmp-pages

debug:
	@$(MAKE) --no-print-directory clean all CFLAGS='$(CFLAGS) -O0 -g' LDFLAGS=''
	@cppcheck --enable=all src/going.c
	@valgrind --leak-check=full --show-reachable=yes --time-stamp=yes \
		./going -d test/going.d
