# Native build. For the fat PowerPC/Intel build see build/build-fat.sh
CC      ?= cc
CFLAGS  ?= -O2 -Wall
OBJS     = src/md5.o src/pefpatch.o src/cli.o

oldmacpatch: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) oldmacpatch

.PHONY: clean
