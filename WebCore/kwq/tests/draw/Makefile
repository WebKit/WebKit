drawdemo.o: drawdemo.cpp
	c++  -c -I$(QTDIR)/include -fno-exceptions -pipe -g  -o drawdemo.o drawdemo.cpp

drawdemo: drawdemo.o
	c++ -L$(QTDIR)/lib -o ./drawdemo drawdemo.o   -lqt  -L/usr/X11R6/lib   -lz -ljpeg  -lXext -lX11

clean:
	rm *.o drawdemo

all: drawdemo
