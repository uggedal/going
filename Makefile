CFLAGS=-std=gnu99 -Wall -O3 -pedantic -Werror -Wextra -Wshadow -Wstrict-prototypes -Wunreachable-code -Waggregate-return

all: going

clean:
	rm -f going

debug:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) -O0 -g"
	cppcheck --enable=all going.c
