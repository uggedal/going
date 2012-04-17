PREFIX=/usr

CFLAGS=-std=gnu99 -Os -Wall -pedantic -Werror -Wextra -Wshadow
CFLAGS+=-Wstrict-prototypes -Wunreachable-code -Waggregate-return
LDFLAGS=-s

.PHONY: clean doc debug

all: going

clean:
	@rm -f going

install: all
	@install going $(PREFIX)/sbin

doc:
	@docco going.c

debug:
	@$(MAKE) --no-print-directory clean all CFLAGS='$(CFLAGS) -O0 -g' LDFLAGS=''
	@cppcheck --enable=all going.c
	@valgrind --leak-check=full --show-reachable=yes ./going -d test/going.d
