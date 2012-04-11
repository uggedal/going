CFLAGS=-std=gnu99 -Wall -Os -pedantic -Werror -Wextra -Wshadow -Wstrict-prototypes -Wunreachable-code -Waggregate-return

all: going

clean:
	@rm -f going

debug:
	@$(MAKE) --no-print-directory clean all CFLAGS="$(CFLAGS) -O0 -g"
	@cppcheck --enable=all going.c
	@valgrind --leak-check=full --show-reachable=yes ./going -d test/going.d
