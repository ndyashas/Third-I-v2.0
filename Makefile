FLAGS  = `pkg-config fuse --cflags --libs` -DFUSE_USE_VERSION=25 -lm -g
INCDIR = include
SRCDIR = sources
FILES  = 
OPFLAG = -o fs.ti
COMPIR = gcc


start : run

run : compile
	./fs.ti -f mountPoint -o nonempty

debug : compile
	valgrind --track-origins=yes ./fs.ti -d -f -s mountPoint -o nonempty

compile :
	$(COMPIR) -Wall -g $(FILES) -I $(INCDIR) $(FLAGS) $(OPFLAG)

clean: stop

stop :
	rm -rf *.ti *.o
	sudo umount mountPoint

format :
	rm -rf metaFiles/*
