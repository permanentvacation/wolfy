CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
CFLAGS  += $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags)
LIBS    := $(shell pkg-config --libs sdl2 2>/dev/null || sdl2-config --libs) -lm

wolfy: wolfy.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

run: wolfy
	./wolfy

test: wolfy
	./wolfy --selftest
	./wolfy --demo 600

clean:
	rm -f wolfy

.PHONY: run test clean
