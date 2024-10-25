#    Makefile for interpreters-comparison - a group of interpreters/translators
#    for a stack virtual machine.
#    Copyright (c) 2015, 2016 Grigory Rechistov. All rights reserved.
#

CFLAGS=-std=c11 -O2 -Wextra -Werror -gdwarf-3
LDFLAGS = -lm

COMMON_SRC = common.c
COMMON_OBJ := $(COMMON_SRC:.c=.o)
COMMON_HEADERS = common.h

ALL = switched threaded predecoded subroutined threaded-cached tailrecursive asmopt translated native # translated-inline

# Must be the first target for the magic below to work
all: $(ALL)

ALL_SRCS = $(COMMON_SRC) $(ALL:=.c)

# ######################
# The section below is meant to generate dependencies properly using GCC flags
# For explanation, see
# http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

%.o: %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d
-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(ALL_SRCS)))

$(ALL): $(COMMON_OBJ)

# #######################
# Individual applications
#
# Note that some of them use customized CFLAGS

switched: switched.o
	$(CC) $^ -lm -o $@

threaded: CFLAGS += -fno-gcse -fno-function-cse -fno-thread-jumps -fno-cse-follow-jumps -fno-crossjumping -fno-cse-skip-blocks -fomit-frame-pointer
threaded: threaded.o
	$(CC) $^ -lm -o $@

predecoded: predecoded.o
	$(CC) $^ -lm -o $@

tailrecursive: CFLAGS += -foptimize-sibling-calls
tailrecursive: tailrecursive.o
	$(CC) $^ -lm -o $@

asmoptll: asmoptll.o
	$(CC) -g -c $< -o $@

asmopt: CFLAGS += -foptimize-sibling-calls
asmopt: asmoptll.o asmopt.o
	$(CC) -g $^ -lm -o $@

threaded-cached: CFLAGS += -fno-gcse -fno-thread-jumps -fno-cse-follow-jumps -fno-crossjumping -fno-cse-skip-blocks -fomit-frame-pointer
threaded-cached: threaded-cached.o
	$(CC) $^ -lm -o $@

subroutined: subroutined.o
	$(CC) $^ -lm -o $@

translated: CFLAGS += -std=gnu11
translated: translated.o
	$(CC) $^ -lm -o $@

translated-inline: CFLAGS += -std=gnu11
translated-inline: translated-inline.o
	$(CC) $^ -lm -o $@

native: native.o
	$(CC) $^ -lm -o $@

########################
### Maintainance targets

measure: all
	./measure.sh $(ALL)

clean:
	rm -rf $(ALL) *.exe *.d *.o $(DEPDIR)

# Do a quick check that code builds and runs for at least several steps
sanity: all
	for APP in $(ALL); do ./$$APP --steplimit=100 > /dev/null; done
	@echo "Sanity OK"

### Inferior, faulty, broken etc targets, not built by default

# Unoptimized version
switched-noopt: CFLAGS=-std=c11 -O0 -Wextra -Werror -g
switched-noopt: switched.o

# GCC will collapse all indirect jumps into one for this
threaded-notune: threaded.o

# This will crash with stack overflow
tailrecursive-noopt: CFLAGS += -O0 -fno-optimize-sibling-calls
tailrecursive-noopt: tailrecursive.o
