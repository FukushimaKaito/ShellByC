PROGRAM       = main
OBJS          = main.o
CC            = gcc
CFLAGS        = -Wall

.PHONY: all
all:            $(PROGRAM)

$(PROGRAM):     $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(PROGRAM)
.PHONY: clean
clean:;         rm -f *.o *~ $(PROGRAM)
