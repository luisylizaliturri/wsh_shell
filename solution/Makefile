#Variables
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu18
LOGIN = ylizaliturri
SUBMITPATH = /home/cs537-1/handin/ylizaliturri
PROJECTPATH = /home/ylizaliturri/private/cs537/p3

#Targets
all: wsh wsh-dbg

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) -O2 -o $@ $^

wsh-dbg: wsh.c wsh.h
	$(CC) $(CFLAGS) -Og -ggdb -o $@ $^


clean-tests:
	rm -f *.test *.wsh

clean:
	rm -f wsh wsh-dbg 

submit:
	cp -rf $(PROJECTPATH) $(SUBMITPATH)
