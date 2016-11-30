all:
	gcc player.c -o player -lalut -lopenal -lpthread -lm

clean:
	rm player
