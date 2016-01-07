CC = clang++
LFLAGS = --std=c++11 -Wall
CFLAGS = -c $(LFLAGS)
OBJS = snapshot.o memdiff.o

prog : $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o prog

snapshot.o : snapshot.cpp snapshot.h
	$(CC) $(CFLAGS) snapshot.cpp

memdiff.o : memdiff.cpp
	$(CC) $(CFLAGS) memdiff.cpp

clean:
	rm *.o prog
