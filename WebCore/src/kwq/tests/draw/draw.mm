/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * This program tests the subset of the Qt drawing API implemented in the
 * KWQ emulation package.  Only the API in the emulation package should
 * be used.
 *
 * Classes tested (and needed to link):
 *	QApplication
 *	QBrush
 *	QByteArray
 *	QColor
 *	QFont
 *	QFontMetrics
 *	QPainter
 *	QPen
 *	QPixmap
 *	QPoint
 *	QRect
 *	QSize
 *	QString
 *	QWidget
 *	QWMatrix
 *
 *  Also needs QPaintEvent.  This is only used by the Qt version of the app.
*/

#include <qwidget.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qapplication.h>

// Voodoo required to get compiler to compile correctly.
#undef DEBUG
#import <Cocoa/Cocoa.h>

//
// This function draws a color wheel.
// The coordinate system x=(0..500), y=(0..500) spans the paint device.
//

#define SLICE_WIDTH 68
#define SLICE_HEIGHT 10

void drawColors( QPainter *p )
{
    int x = 0, y = 0, height, r = 0, g = 255, b = 125;
    p->save();
    
    QFont f( "helvetica", 10, QFont::Bold );
    p->setFont( f );
    p->setPen( Qt::black );

    QFontMetrics fm = p->fontMetrics();
    height = fm.height();

    for ( int i=0; i<36; i++ ) {		// draws 36 rotated rectangles
        QColor c;
        c.setRgb( r, g, b );
        r += 255/36;
        g -= 255/36;
        p->setBrush( c );			// solid fill with color c
        x += SLICE_WIDTH + 5;
        if (i % 6 == 0) {
            y += SLICE_HEIGHT + height + 2;
            x = 10;
        }
        p->drawRect( x, y, SLICE_WIDTH, SLICE_HEIGHT );		// draw the rectangle

        QString n;
        n.sprintf( "%d,%d,%d", r,g,b );
        p->drawText( x, y + SLICE_HEIGHT + height, n );		// draw the hue number
    }
    p->restore();
}


void drawImages( QPainter *p)
{
    QByteArray *byteArray[3];
    QPixmap *pixmap[3];
    NSString *files[3] = { @"qt.png", @"powermac.jpg", @"yahoo.gif" };
    NSData *data[3];
    int i;
    QPoint *point;
    
    for (i = 0; i < 3; i++){
        point = new QPoint (10 + 60 * i, 200);
        data[i] = [[NSData alloc] initWithContentsOfFile: files[i]];
        byteArray[i] = new QByteArray();
        byteArray[i]->setRawData ((const char *)[data[i] bytes], (unsigned int)[data[i] length]);
        pixmap[i] = new QPixmap (*byteArray[i]);
        if (i == 1)
            p->setRasterOp(Qt::XorROP);
        else
            p->setRasterOp(Qt::CopyROP);
        if (i == 2){
            QWMatrix matrix;
            matrix.scale((double)0.5, (double)0.5);
            QPixmap rp = pixmap[i]->xForm (matrix);
            p->drawPixmap (*point,rp); 
        }
        else
            p->drawPixmap (*point,*pixmap[i]); 
        delete point;
    }

    for (i = 0; i < 3; i++){
        point = new QPoint (10 + 60 * i, 200);

        QSize imageSize = pixmap[i]->size();
        
        p->setPen( Qt::yellow );
        p->drawRect (point->x(), point->y(), imageSize.width(), imageSize.height());

        p->setPen( Qt::blue );
        p->drawLine (point->x(), point->y(), point->x() + imageSize.width(), point->y() + imageSize.height());
        p->drawLine (point->x(), point->y() + imageSize.height(), point->x() + imageSize.width(), point->y());
        delete point;
    }

    int x = 480, y = 20, w = 400, h = 300;
    p->drawTiledPixmap (x, y, w, h, *pixmap[0], 0, 0);
    p->drawLine (x, y, x + w, y + h);
    p->drawLine (x, y + h, x + w, y);
    
    for (i = 0; i < 3; i++){
        delete byteArray[i];
        delete pixmap[i];
        // Problematic, as QByteArray expects ownership.
        //[data[i] release];
    }
}


//
// This function draws a few lines of text using different fonts.
//

#define DRAW_FONT_OFFSET 360.0F

void drawFonts( QPainter *p )
{
    static const char *fonts[] = { "Helvetica", "Courier", "Times", 0 };
    static int	 sizes[] = { 10, 12, 18, 24, 36, 0 };
    int f = 0;
    int y = 0;
    
    p->save();

#ifdef USE_TRANSLATION
    QWMatrix matrix;
    matrix.translate( 0.0F, DRAW_FONT_OFFSET );	// move to center
    p->setWorldMatrix( matrix );		// use this world matrix
#else
    y += (int)DRAW_FONT_OFFSET;
#endif

    p->setPen( Qt::black );
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

#define DRAW_SHAPES_OFFSET 690.0F

void drawShapes( QPainter *p )
{
    QBrush b1( Qt::blue );
    QBrush b2( Qt::green, Qt::Dense6Pattern );		// green 12% fill
    QBrush b3( Qt::NoBrush );				// void brush
    QBrush b4( Qt::CrossPattern );			// black cross pattern

    int y = 10;
    
    p->save();
    
#ifdef USE_TRANSLATION
    QWMatrix matrix;
    matrix.translate( 0.0F, DRAW_SHAPES_OFFSET );	// move to center
    p->setWorldMatrix( matrix );		// use this world matrix
#else
    y += (int)DRAW_SHAPES_OFFSET;
#endif
    p->setPen( Qt::red );
    p->setBrush( b1 );
    p->drawRect( 10, y, 200, 100 );
    p->setBrush( b3 );
    p->drawEllipse( 250, y, 200, 100 );
    p->setBrush( b4 );
    p->drawArc( 250, y+140, 200, 100, 45*16, 90*16 );

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
    resize( 900,900 );
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
    
    drawColors (&paint);
    drawImages (&paint);
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
