CFLAGS=-Wall -O3

all: going

clean:
	rm -f going

debug:
	$(MAKE) all CFLAGS="$(CFLAGS) -O0 -Wextra -Wshadow -Wstrict-prototypes -g"
