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

#include <qpainter.h>
#include <qwidget.h>
#include <qfontmetrics.h>
#include <qpixmap.h>
#include <qstack.h>
#include <qpoint.h>


#import <Cocoa/Cocoa.h>

struct QPState {				// painter state
    QFont	font;
    QPen	pen;
    QBrush	brush;
    NSCompositingOperation compositingOperation;
};

typedef QStack<QPState> QPStateStack;


QPainter::QPainter()
{
}


QPainter::QPainter(const QPaintDevice *pdev)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


//  How do we handle ownership of widget?
QPainter::QPainter(QWidget *widget)
{
    _initialize (widget);
}

void QPainter::_initialize(QWidget *widget)
{
    data = (struct KWQPainterData *)calloc (1, sizeof (struct KWQPainterData));
    data->widget = widget;
    data->qpen = QPen (Qt::black);
    data->isFocusLocked = 0;
    data->compositingOperation = NSCompositeCopy;
    data->bufferDevice = 0L;
}


QPainter::~QPainter()
{
    free (data);
}

    
const QFont &QPainter::font() const
{
    return data->qfont;
}


void QPainter::setFont(const QFont &aFont)
{
    data->qfont = aFont;
}


QFontMetrics QPainter::fontMetrics() const
{
    return QFontMetrics( data->qfont );
}

const QPen &QPainter::pen() const
{
    return data->qpen;
}


void QPainter::setPen(const QPen &pen)
{
    data->qpen = pen;
}


void QPainter::setPen(PenStyle)
{
    NSLog (@"WARNING (NOT YET IMPLEMENTED) void QPainter::setPen(PenStyle)\n");
}


void QPainter::setBrush(const QBrush &brush)
{
    data->qbrush = brush;
}


void QPainter::setBrush(BrushStyle style)
{
    // Either NoBrush or SolidPattern.
    data->qbrush.qbrushstyle = style;
}


QRect QPainter::xForm(const QRect &) const
{
    NSLog (@"WARNING (NOT YET IMPLEMENTED) QRect QPainter::xForm(const QRect &) const\n");
}


void QPainter::save()
{
    QPStateStack *pss = data->ps_stack;
    if ( pss == 0 ) {
	pss = new QStack<QPState>;
	data->ps_stack = pss;
    }
    
    QPState *ps = new QPState;

    ps->font  = data->qfont;
    ps->pen   = data->qpen;
    ps->brush = data->qbrush;
    ps->compositingOperation = data->compositingOperation;
    pss->push( ps );
}


void QPainter::restore()
{
    QPStateStack *pss = (QPStateStack *)data->ps_stack;
    if ( pss == 0 || pss->isEmpty() ) {
        NSLog (@"ERROR void QPainter::restore() stack is empty\n");
	return;
    }
    QPState *ps = pss->pop();

    if ( ps->font != data->qfont )
	setFont( ps->font );
    if ( ps->pen != data->qpen )
	setPen( ps->pen );
    if ( ps->brush != data->qbrush )
	setBrush( ps->brush );
    ps->compositingOperation = data->compositingOperation;

    delete ps;
}


// Draws a filled rectangle with a stroked border.
void QPainter::drawRect(int x, int y, int w, int h)
{
    _lockFocus();
    if (data->qbrush.qbrushstyle == SolidPattern){
        _setColorFromBrush();
        [NSBezierPath fillRect:NSMakeRect(x, y, w, h)];
    }
    _setColorFromPen();
    [NSBezierPath strokeRect:NSMakeRect(x, y, w, h)];
    _unlockFocus();
}


void QPainter::_setColorFromBrush()
{
    [data->qbrush.qcolor.color set];
}


void QPainter::_setColorFromPen()
{
    [data->qpen.qcolor.color set];
}


void QPainter::drawLine(int x1, int y1, int x2, int y2)
{
    _lockFocus();
    _setColorFromPen();
    [NSBezierPath strokeLineFromPoint:NSMakePoint(x1, y1) toPoint:NSMakePoint(x2, y2)];
    _unlockFocus();
}


void QPainter::drawEllipse(int x, int y, int w, int h)
{
    NSBezierPath *path;

    path = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (x, y, w, h)];
    
    _lockFocus();
    if (data->qbrush.qbrushstyle == SolidPattern){
        _setColorFromBrush();
        [path fill];
    }
    _setColorFromPen();
    [path stroke];
    _unlockFocus();
}


// Only supports arc on circles.  That's all khtml needs.
void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)
{
    NSBezierPath *path;
    float fa, falen;
    
    if (w != h){
        NSLog (@"ERROR (INCOMPLETE IMPLEMENTATION) void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)\nOnly supports drawing arcs on a circle.\n");
    }
    
    path = [[[NSBezierPath alloc] init] autorelease];
    fa = (float)(a/16);
    falen =  fa + (float)(alen/16);
    [path appendBezierPathWithArcWithCenter: NSMakePoint(x + w/2, y + h/2) 
                radius: (float)(w/2) 
                startAngle: -fa
                endAngle: -falen
                clockwise: YES];

    _lockFocus();

    _setColorFromPen();
    [path stroke];

    _unlockFocus();
}


void QPainter::drawPolyline(const QPointArray &points, int index, int npoints)
{
    _drawPoints (points, 0, index, npoints, FALSE);
}


void QPainter::drawPolygon(const QPointArray &points, bool winding, int index,
    int npoints)
{
    _drawPoints (points, winding, index, npoints, TRUE);
}


void QPainter::_drawPoints (const QPointArray &_points, bool winding, int index, int _npoints, bool fill)
{
    NSBezierPath *path;
    float fa, falen;
    int i;
    int npoints = _npoints != -1 ? _npoints : _points.size()-index;

        
    {
        NSPoint points[npoints];
        
        for (i = 0; i < npoints; i++){
            points[i].x = _points[index+i].x();
            points[i].y = _points[index+i].y();
        }
        
        
        path = [[[NSBezierPath alloc] init] autorelease];
        [path appendBezierPathWithPoints: &points[0] count: npoints];
        [path closePath];	// Qt always closes the path.  Determined empirically.
        
        _lockFocus();

        if (fill == TRUE && data->qbrush.qbrushstyle == SolidPattern){
            if (winding == TRUE)
                [path setWindingRule: NSNonZeroWindingRule];
            else
                [path setWindingRule: NSEvenOddWindingRule];
            _setColorFromBrush();
            [path fill];
        }
        _setColorFromPen();
        [path stroke];
        
        _unlockFocus();
    }
}


void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix)
{
    drawPixmap (p.x(), p.y(), pix);
}


void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix, const QRect &r)
{
    drawPixmap (p.x(), p.y(), pix, r.x(), r.y(), r.width(), r.height());
}


void QPainter::drawPixmap( int x, int y, const QPixmap &pixmap,
			int sx, int sy, int sw, int sh )
{
    NSSize originalSize;

    _lockFocus();

    if (pixmap.nsimage != nil){
        if (pixmap.xmatrix.empty == FALSE){
            originalSize = [pixmap.nsimage size];
            [pixmap.nsimage setScalesWhenResized: YES];
            [pixmap.nsimage setSize: NSMakeSize (originalSize.width * pixmap.xmatrix.sx,
                                                originalSize.height * pixmap.xmatrix.sy)];
        }
        
        if (sw == -1)
            sw = (int)[pixmap.nsimage size].width;
        if (sh == -1)
            sh = (int)[pixmap.nsimage size].height;
        [pixmap.nsimage drawInRect: NSMakeRect(x, y, sw, sh) 
                    fromRect: NSMakeRect(sx, sy, sw, sh)
                    operation: NSCompositeSourceOver	// Renders transparency correctly
                    fraction: 1.0];
                    
        if (pixmap.xmatrix.empty == FALSE)
            [pixmap.nsimage setSize: originalSize];
    }
    
    _unlockFocus();
}

static void drawTile( QPainter *p, int x, int y, int w, int h,
		      const QPixmap &pixmap, int xOffset, int yOffset )
{
    int yPos, xPos, drawH, drawW, yOff, xOff;
    yPos = y;
    yOff = yOffset;
    while( yPos < y + h ) {
	drawH = pixmap.height() - yOff;    // Cropping first row
	if ( yPos + drawH > y + h )	   // Cropping last row
	    drawH = y + h - yPos;
	xPos = x;
	xOff = xOffset;
	while( xPos < x + w ) {
	    drawW = pixmap.width() - xOff; // Cropping first column
	    if ( xPos + drawW > x + w )	   // Cropping last column
		drawW = x + w - xPos;
	    p->drawPixmap( xPos, yPos, pixmap, xOff, yOff, drawW, drawH );
	    xPos += drawW;
	    xOff = 0;
	}
	yPos += drawH;
	yOff = 0;
    }
}


void QPainter::drawTiledPixmap( int x, int y, int w, int h,
				const QPixmap &pixmap, int sx, int sy )
{
    int sw = pixmap.width();
    int sh = pixmap.height();
    if (!sw || !sh )
	return;
    if ( sx < 0 )
	sx = sw - -sx % sw;
    else
	sx = sx % sw;
    if ( sy < 0 )
	sy = sh - -sy % sh;
    else
	sy = sy % sh;

    drawTile( this, x, y, w, h, pixmap, sx, sy );
}


// y is the baseline
void QPainter::drawText(int x, int y, const QString &qstring, int len)
{
    NSString *string;
    NSFont *font;
    const char *ascii;
    
    _lockFocus();
    
    font = data->qfont.data->font;    
    
    if (len == -1)
        string = QSTRING_TO_NSSTRING(qstring);
    else
        string = QSTRING_TO_NSSTRING_LENGTH(qstring,len);

    // This will draw the text from the top of the bounding box down.
    // Qt expects to draw from the baseline.
    y = y - (int)([font defaultLineHeightForFont] + [font descender]);
    [string drawAtPoint:NSMakePoint(x, y) withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, data->qpen.qcolor.color, NSForegroundColorAttributeName, nil]];

    _unlockFocus();
}


void QPainter::drawText(int x, int y, int w, int h, int flags, const QString&qstring, int len, 
    QRect *br, char **internal)
{
    NSString *string;
    NSFont *font;
    const char *ascii;
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    
    _lockFocus();
    
    font = data->qfont.data->font;    
    
    if (len == -1)
        string = QSTRING_TO_NSSTRING(qstring);
    else
        string = QSTRING_TO_NSSTRING_LENGTH(qstring,len);

    if (flags & Qt::WordBreak){
        [style setLineBreakMode: NSLineBreakByWordWrapping];
    }
    else {
        [style setLineBreakMode: NSLineBreakByClipping];
    }
    
    if (flags & Qt::AlignRight){
        [style setAlignment: NSRightTextAlignment];
    }
    
    if (flags & Qt::AlignLeft){
        [style setAlignment: NSLeftTextAlignment];
    }
    
    [string drawInRect:NSMakeRect(x, y, w, h) withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, data->qpen.qcolor.color, NSForegroundColorAttributeName, style, NSParagraphStyleAttributeName, nil]];

    _unlockFocus();
}



void QPainter::fillRect(int x, int y, int w, int h, const QBrush &brush)
{
    _lockFocus();
    if (brush.qbrushstyle == SolidPattern){
        [brush.qcolor.color set];
        [NSBezierPath fillRect:NSMakeRect(x, y, w, h)];
    }
    _unlockFocus();
}


void QPainter::setClipping(bool)
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QPainter::setClipRegion(const QRegion &)
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


const QRegion &QPainter::clipRegion() const
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool QPainter::hasClipping() const
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}

void QPainter::setClipRect(const QRect &)
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QPainter::setClipRect(int,int,int,int)
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


Qt::RasterOp QPainter::rasterOp() const
{
#ifdef _SUPPORT_RASTER_OP
    if (data->compositingOperation == NSCompositeSourceOver)
        rerturn OrROP;
    else if (data->compositingOperation == NSCompositeXOR)
        return XorROP;
    return CopyROP;
#else
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
#endif
}


void QPainter::setRasterOp(RasterOp op)
{
#ifdef _SUPPORT_RASTER_OP
    if (op == OrROP)
        data->compositingOperation = NSCompositeSourceOver;
    else if (op == XorROP)
        data->compositingOperation = NSCompositeXOR;
    else
        data->compositingOperation = NSCompositeCopy;
#else
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
#endif
}

void QPainter::translate(double dx, double dy)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QPainter::scale(double dx, double dy)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool QPainter::begin(const QPaintDevice *bd)
{
    data->bufferDevice = bd;
}


bool QPainter::end()
{
    data->bufferDevice = 0L;
}


QPaintDevice *QPainter::device() const
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}

void QPainter::_lockFocus(){
    if (data->isFocusLocked == 0){
        if (data->bufferDevice != 0L){
            const QPixmap *pixmap = (QPixmap *)(data->bufferDevice);
            [pixmap->nsimage lockFocus];
        }
        else {
            [data->widget->getView() lockFocus];
        }
        data->isFocusLocked = 1;
    }	
}

void QPainter::_unlockFocus(){
    if (data->isFocusLocked == 1){
        if (data->bufferDevice != 0L){
            const QPixmap *pixmap = (QPixmap *)(data->bufferDevice);
            [pixmap->nsimage unlockFocus];
        }
        else  {
            [data->widget->getView() unlockFocus];
        }
        data->isFocusLocked = 0;
    }	
}


