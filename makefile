s-talk: s-talk.o list.o
	gcc -o s-talk -pthread s-talk.o list.o

s-talk.o: s-talk.c list.h
	gcc -c s-talk.c 

clean:
	rm s-talk.o s-talk

