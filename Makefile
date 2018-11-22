FLAGS  = `pkg-config fuse --cflags --libs` -DFUSE_USE_VERSION=25 -lm -g
SRCDIR = sources
FILES  = $(SRCDIR)/ti_main.c
OPFLAG = -o fs.ti
COMPIR = gcc


start : debug

run : compile
	./fs.ti -f mountPoint -o nonempty

debug : compile
	valgrind --track-origins=yes --leak-check=full ./fs.ti -d -f -s mountPoint -o nonempty

compile :
	$(COMPIR) -Wall -g $(FILES) $(FLAGS) $(OPFLAG)

stop :
	rm -rf *.ti *.o
	sudo umount mountPoint

format :
	rm -rf metaFiles/*
	rm -rf *.ti *.o
	sudo umount mountPoint
