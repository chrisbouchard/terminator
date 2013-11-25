# Makefile
#
# Chris Bouchard
# 21 Nov 2013
#
# The makefile to build terminator.
#

.PHONY: all clean
all: terminator

terminator: terminator.c my_assert.h
	gcc -o terminator -Wall terminator.c

clean:
	rm terminator

