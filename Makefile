CFLAGS=-std=gnu99 -Wall -O3

all: going

clean:
	rm -f going

debug:
	$(MAKE) all CFLAGS="$(CFLAGS) -O0 -pedantic -Werror -Wextra -Wshadow -Wstrict-prototypes -Wunreachable-code -Waggregate-return -g"
