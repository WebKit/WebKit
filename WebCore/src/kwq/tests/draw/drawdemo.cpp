/****************************************************************************
** $Id$
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include <qwidget.h>
#include <qpainter.h>
#include <qapplication.h>
#include <math.h>


//
// This function draws a color wheel.
// The coordinate system x=(0..500), y=(0..500) spans the paint device.
//

#define SLICE_LENGTH 15

void drawColorWheel( QPainter *p )
{
    p->save();
    
    QFont f( "times", 18, QFont::Bold );
    p->setFont( f );
    p->setPen( Qt::black );
    //p->setWindow( 0, 0, 500, 500 );		// defines coordinate system

    for ( int i=0; i<36; i++ ) {		// draws 36 rotated rectangles

        QWMatrix matrix;
        matrix.translate( 390.0F, 140.0F );	// move to center
        matrix.shear( 0.0F, 0.3F );		// twist it
        matrix.rotate( (float)i*10 );		// rotate 0,10,20,.. degrees
        p->setWorldMatrix( matrix );		// use this world matrix

        QColor c;
        c.setHsv( i*10, 255, 255 );		// rainbow effect
        p->setBrush( c );			// solid fill with color c
        p->drawRect( 70, -10, SLICE_LENGTH, 10 );		// draw the rectangle

        QString n;
        n.sprintf( "H=%d", i*10 );
        p->drawText( SLICE_LENGTH+70+5, 0, n );		// draw the hue number
    }
    p->restore();
}


//
// This function draws a few lines of text using different fonts.
//

#define DRAW_FONT_OFFSET 270.0F

void drawFonts( QPainter *p )
{
    static const char *fonts[] = { "Helvetica", "Courier", "Times", 0 };
    static int	 sizes[] = { 10, 12, 18, 24, 36, 0 };
    int f = 0;
    int y = 0;
    
    p->save();

    QWMatrix matrix;
    matrix.translate( 0.0F, DRAW_FONT_OFFSET );	// move to center
    p->setWorldMatrix( matrix );		// use this world matrix
    while ( fonts[f] ) {
        int s = 0;
        while ( sizes[s] ) {
            QFont font( fonts[f], sizes[s] );
            p->setFont( font );
            QFontMetrics fm = p->fontMetrics();
            y += fm.ascent();
            p->drawText( 10, y, fonts[f] );
            p->drawText( 10 + p->fontMetrics().width(fonts[f]), y, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefhijklmnopqrstuvwxyz" );
            y += fm.descent();
            s++;
        }
        f++;
    }
    p->restore();
}


//
// This function draws some shapes
//

#define DRAW_SHAPES_OFFSET 600.0F

void drawShapes( QPainter *p )
{
    QBrush b1( Qt::blue );
    QBrush b2( Qt::green, Qt::Dense6Pattern );		// green 12% fill
    QBrush b3( Qt::NoBrush );				// void brush
    QBrush b4( Qt::CrossPattern );			// black cross pattern

    p->save();
    
    QWMatrix matrix;
    matrix.translate( 0.0F, DRAW_SHAPES_OFFSET );	// move to center
    p->setWorldMatrix( matrix );		// use this world matrix

    p->setPen( Qt::red );
    p->setBrush( b1 );
    p->drawRect( 10, 10, 200, 100 );
    p->setBrush( b2 );
    p->drawRoundRect( 10, 150, 200, 100, 20, 20 );
    p->setBrush( b3 );
    p->drawEllipse( 250, 10, 200, 100 );
    p->setBrush( b4 );
    p->drawPie( 250, 150, 200, 100, 45*16, 90*16 );

    p->restore();
}


class DrawView : public QWidget
{
public:
    DrawView();
    ~DrawView();
protected:
    void   paintEvent( QPaintEvent * );
private:
};


//
// Construct the DrawView with buttons.
//

DrawView::DrawView()
{
    setCaption( "Qt Draw Demo Application" );
    setBackgroundColor( white );    
    resize( 640,900 );
}

//
// Clean up
//
DrawView::~DrawView()
{
}

//
// Called when the widget needs to be updated.
//

void DrawView::paintEvent( QPaintEvent * )
{
    QPainter paint( this );
    
    drawColorWheel (&paint);
    drawFonts (&paint);
    drawShapes (&paint);
}


//
// Create and display our widget.
//

int main( int argc, char **argv )
{
    QApplication app( argc, argv );
    DrawView   draw;
    app.setMainWidget( &draw );
    draw.show();
    return app.exec();
}
