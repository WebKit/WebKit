
qdraw.o: draw.mm
	cc  -c -I$(QTDIR)/include -fno-exceptions -pipe -g  -o qdraw.o draw.mm

qdraw: qdraw.o
	cc -L$(QTDIR)/lib -o ./qdraw qdraw.o   -lqt  -L/usr/X11R6/lib   -lz -ljpeg  -lXext -lX11 -framework Cocoa

#kwqdraw.o: draw.mm
#	cc  -c -DQT_NO_DATASTREAM -D_KWQ_COMPLETE_ -D_KWQ_ -I../.. -I../../qt -fno-exceptions -pipe -g  -o kwqdraw.o draw.mm

#kwqdraw: kwqdraw.o
#	cc -L$(QTDIR)/lib -o ./kwqdraw kwqdraw.o   -L../.. -L../../qt -lkwq -l_qt -lstdc++ -framework Cocoa

#clean:
#	rm *.o qdraw kwqdraw
clean:

	rm *.o qdraw
#all: qdraw kwqdraw
all: qdraw
