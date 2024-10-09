# Variables
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu18
LOGIN = yourlogin
SUBMITPATH = /path/to/submit

# Targets
all: wsh wsh-dbg

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) -O2 -o $@ $^

wsh-dbg: wsh.c wsh.h
	$(CC) $(CFLAGS) -Og -ggdb -o $@ $^

clean:
	rm -f wsh wsh-dbg

submit:
	cp wsh.c wsh.h Makefile $(SUBMITPATH)
