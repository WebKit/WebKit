draw.o: draw.mm
	cc  -c -I$(QTDIR)/include -fno-exceptions -pipe -g  -o draw.o draw.mm

draw: draw.o
	cc -L$(QTDIR)/lib -o ./draw draw.o   -lqt  -L/usr/X11R6/lib   -lz -ljpeg  -lXext -lX11 -framework Cocoa

clean:
	rm *.o draw

all: draw
