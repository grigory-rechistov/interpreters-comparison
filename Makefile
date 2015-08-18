#    Makefile for interpreters-comparison - a group of interpreters/translators
#    for a stack virtual machine.
#    Copyright (c) 2015 Grigory Rechistov. All rights reserved.
#

CFLAGS=-std=c11 -O2 -Wextra -Werror -gdwarf-3

ALL=switched threaded predecoded subroutined threaded-cached tailrecursive translated translated-retmanip translated-jmpmanip translated-inline native

all: $(ALL)

switched: switched.c common.h
	$(CC) $(CFLAGS) $< -o $@

threaded: CFLAGS += -fno-gcse -fno-function-cse -fno-thread-jumps -fno-cse-follow-jumps -fno-crossjumping -fno-cse-skip-blocks -fomit-frame-pointer
threaded: threaded.c common.h
	$(CC) $(CFLAGS) $< -o $@

predecoded: predecoded.c common.h
	$(CC) $(CFLAGS) $< -o $@

tailrecursive: CFLAGS += -foptimize-sibling-calls
tailrecursive: tailrecursive.c common.h
	$(CC) $(CFLAGS) $< -o $@

threaded-cached: CFLAGS += -fno-gcse -fno-thread-jumps -fno-cse-follow-jumps -fno-crossjumping -fno-cse-skip-blocks -fomit-frame-pointer
threaded-cached: threaded-cached.c common.h
	$(CC) $(CFLAGS) $< -o $@

subroutined: subroutined.c common.h
	$(CC) $(CFLAGS) $< -o $@

translated: CFLAGS += -std=gnu11
translated: translated.c common.h
	$(CC) $(CFLAGS) $< -o $@

translated-retmanip: CFLAGS += -std=gnu11
translated-retmanip: translated-retmanip.c common.h
	$(CC) $(CFLAGS) $< -o $@

translated-jmpmanip: CFLAGS += -std=gnu11
translated-jmpmanip: translated-jmpmanip.c common.h
	$(CC) $(CFLAGS) $< -o $@

translated-inline: CFLAGS += -std=gnu11
translated-inline: translated-inline.c common.h
	$(CC) $(CFLAGS) $< -o $@

native: native.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(ALL) *.exe

measure: all
	./measure.sh $(ALL)

switched-noopt: CFLAGS=-std=c11 -O0 -Wextra -Werror -g
switched-noopt: switched.c common.h
	$(CC) $(CFLAGS) $< -o $@

# GCC will collapse all indirect jumps into one for this
threaded-notune: threaded.c common.h
	$(CC) $(CFLAGS) $< -o $@

# This will crash with stack overflow
tailrecursive-noopt: CFLAGS += -O0 -fno-optimize-sibling-calls
tailrecursive-noopt: tailrecursive.c common.h
	$(CC) $(CFLAGS) $< -o $@
