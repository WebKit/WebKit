/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <qpainter.h>
#import <qwidget.h>
#import <qfontmetrics.h>
#import <qpixmap.h>
#import <qstack.h>
#import <qpoint.h>

#import <kwqdebug.h>

#import <WebCoreTextRendererFactory.h>
#import <WebCoreTextRenderer.h>
#import <WebCoreImageRenderer.h>

struct QPState {				// painter state
    QFont	font;
    QPen	pen;
    QBrush	brush;
    NSCompositingOperation compositingOperation;
};

typedef QPtrStack<QPState> QPStateStack;


struct QPainterPrivate {
friend class QPainter;
public:
    
    QPainterPrivate(QWidget *widget) : 
        widget(widget), 
        qfont(), 
        qbrush(),
        qpen(), 
        isFocusLocked(0), 
        ps_stack(0),
        compositingOperation(NSCompositeCopy), 
        bufferDevice(0)
    {
    }
    
    ~QPainterPrivate() {}
    
private:
    QWidget *widget;	// Has a reference to a KWQView.
    QFont qfont;
    QBrush qbrush;
    QPen qpen;
    uint isFocusLocked:1;
    QPStateStack *ps_stack;
    NSCompositingOperation compositingOperation;
    const QPaintDevice *bufferDevice;
};


QPainter::QPainter()
{
    _initialize(0);
}


QPainter::QPainter(const QPaintDevice *pdev)
{
    _logNeverImplemented();
}


//  How do we handle ownership of widget?
QPainter::QPainter(QWidget *widget)
{
    _initialize (widget);
}

void QPainter::_initialize(QWidget *widget)
{
    data = new QPainterPrivate(widget);
}


QPainter::~QPainter()
{
    delete data;
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


void QPainter::setPen(PenStyle style)
{
    data->qpen.setStyle(style);
    data->qpen.setColor(Qt::black);
    data->qpen.setWidth(0);
}


void QPainter::setBrush(const QBrush &brush)
{
    data->qbrush = brush;
}

void QPainter::setBrush(BrushStyle style)
{
    data->qbrush.setStyle(style);
    data->qbrush.setColor(Qt::black);
}

const QBrush &QPainter::brush() const
{
    return data->qbrush;
}

QRect QPainter::xForm(const QRect &) const
{
    _logNotYetImplemented();
    return QRect();
}


void QPainter::save()
{
    QPStateStack *pss = data->ps_stack;
    if ( pss == 0 ) {
	pss = new QPStateStack;
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
    QPStateStack *pss = data->ps_stack;
    if ( pss == 0 || pss->isEmpty() ) {
        KWQDEBUG("ERROR void QPainter::restore() stack is empty\n");
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
    if (data->qbrush.style() != NoBrush) {
        _setColorFromBrush();
        [NSBezierPath fillRect:NSMakeRect(x, y, w, h)];
    }
    if (data->qpen.style() != NoPen) {
        _setColorFromPen();
        [NSBezierPath strokeRect:NSMakeRect(x, y, w, h)];
    }
    _unlockFocus();
}


void QPainter::_setColorFromBrush()
{
    [data->qbrush.color().getNSColor() set];
}


void QPainter::_setColorFromPen()
{
    [data->qpen.color().getNSColor() set];
}


// This is only used to draw borders around text, and lines over text.
void QPainter::drawLine(int x1, int y1, int x2, int y2)
{
    PenStyle penStyle = data->qpen.style();
    if (penStyle == NoPen)
        return;
    float width = data->qpen.width();
    if (width < 1)
        width = 1;
    
    NSPoint p1 = NSMakePoint(x1 + width/2, y1 + width/2);
    NSPoint p2 = NSMakePoint(x2 + width/2, y2 + width/2);
    
    // This hack makes sure we don't end up with lines hanging off the ends, while
    // keeping lines horizontal or vertical.
    if (x1 != x2)
        p2.x -= width;
    if (y1 != y2)
        p2.y -= width;
    
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path setLineWidth:width];
#if 0
    switch (penStyle) {
    case NoPen:
    case SolidLine:
        break;
    case DotLine:
        {
            const float dottedLine[2] = { 1, 1 };
            [path setLineDash:dottedLine count:2 phase:0];
        }
        break;
    case DashLine:
        {
            const float dashedLine[2] = { 3, 2 };
            [path setLineDash:dashedLine count:2 phase:0];
        }
        break;
    }
#endif
    [path moveToPoint:p1];
    [path lineToPoint:p2];
    [path closePath];

    _lockFocus();
    _setColorFromPen();
    [path stroke];
    _unlockFocus();
}


// This method is only used to draw the little circles used in lists.
void QPainter::drawEllipse(int x, int y, int w, int h)
{
    NSBezierPath *path;
    
    path = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (x, y, w, h)];
    
    _lockFocus();
    if (data->qbrush.style() != NoBrush) {
        _setColorFromBrush();
        [path fill];
    }
    if (data->qpen.style() != NoPen) {
        _setColorFromPen();
        [path stroke];
    }
    _unlockFocus();
}


// Only supports arc on circles.  That's all khtml needs.
void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)
{
    if (data->qpen.style() != NoPen){

        NSBezierPath *path;
        float fa, falen;
    
        if (w != h){
            KWQDEBUG("ERROR (INCOMPLETE IMPLEMENTATION) void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)\nOnly supports drawing arcs on a circle.\n");
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
}

void QPainter::drawLineSegments(const QPointArray &points, int index, int nlines)
{
    _drawPoints (points, 0, index, nlines, FALSE);
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

void QPainter::drawConvexPolygon(const QPointArray &points)
{
    _drawPoints (points, FALSE, 0, -1, TRUE);
}

void QPainter::_drawPoints (const QPointArray &_points, bool winding, int index, 
    int _npoints, bool fill)
{
    NSBezierPath *path;
    int i;
    int npoints = _npoints != -1 ? _npoints : _points.size()-index;

    NSPoint points[npoints];
    
    for (i = 0; i < npoints; i++){
        points[i].x = _points[index+i].x();
        points[i].y = _points[index+i].y();
    }
            
    path = [[[NSBezierPath alloc] init] autorelease];
    [path appendBezierPathWithPoints: &points[0] count: npoints];
    [path closePath];	// Qt always closes the path.  Determined empirically.
    
    _lockFocus();

    if (fill == TRUE && data->qbrush.style() != NoBrush){
        if (winding == TRUE)
            [path setWindingRule: NSNonZeroWindingRule];
        else
            [path setWindingRule: NSEvenOddWindingRule];
        _setColorFromBrush();
        [path fill];
    }

    if (data->qpen.style() != NoPen){
        _setColorFromPen();
        [path stroke];
    }
    
    _unlockFocus();
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
    _lockFocus();

    if (sw == -1)
        sw = pixmap.width();
    if (sh == -1)
        sh = pixmap.height();
    
    [pixmap.imageRenderer beginAnimationInRect:NSMakeRect(x, y, sw, sh)
                                      fromRect:NSMakeRect(sx, sy, sw, sh)];
    
    _unlockFocus();
}

void QPainter::drawTiledPixmap( int x, int y, int w, int h,
				const QPixmap &pixmap, int sx, int sy )
{
    [pixmap.imageRenderer tileInRect:NSMakeRect(x, y, w, h) fromPoint:NSMakePoint(sx, sy)];
}

// y is the baseline
void QPainter::drawText(int x, int y, const QString &qstring, int len)
{
    _lockFocus();
    
    if (len == -1)
        len = qstring.length();

    [[[WebCoreTextRendererFactory sharedFactory]
        rendererWithFamily:data->qfont.getNSFamily() traits:data->qfont.getNSTraits() size:data->qfont.getNSSize()]
        drawCharacters:(const UniChar *)qstring.unicode() length: len atPoint:NSMakePoint(x,y) withColor:data->qpen.color().getNSColor()];

    _unlockFocus();
}


void QPainter::drawText (int x, int y, const QString &qstring, int len, TextDirection dir)
{
    if (dir == RTL) {
        _logPartiallyImplemented();
    }

    drawText(x, y, qstring, len);
}


void QPainter::drawText (int x, int y, const QString &qstring, int len, int pos, TextDirection dir)
{
    _logNotYetImplemented();
}


void QPainter::drawUnderlineForText(int x, int y, const QString &qstring, int len)
{
    NSString *string;
    
    _lockFocus();
    
    if (len == -1)
        string = QSTRING_TO_NSSTRING(qstring);
    else
        string = QSTRING_TO_NSSTRING_LENGTH(qstring,len);

    [[[WebCoreTextRendererFactory sharedFactory]
        rendererWithFamily:data->qfont.getNSFamily() traits:data->qfont.getNSTraits() size:data->qfont.getNSSize()]
        drawUnderlineForString:string atPoint:NSMakePoint(x,y) withColor:data->qpen.color().getNSColor()];

    _unlockFocus();
}


void QPainter::drawText(int x, int y, int w, int h, int flags, const QString &qstring, int len, 
    QRect *br, char **internal)
{
    NSString *string;
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    
    _lockFocus();
    
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
    
    [[[WebCoreTextRendererFactory sharedFactory]
        rendererWithFamily:data->qfont.getNSFamily() traits:data->qfont.getNSTraits() size:data->qfont.getNSSize()]
        drawString:string inRect:NSMakeRect(x, y, w, h) withColor:data->qpen.color().getNSColor() paragraphStyle:style];

    _unlockFocus();
}



void QPainter::fillRect(int x, int y, int w, int h, const QBrush &brush)
{
    _lockFocus();
    if (brush.style() == SolidPattern) {
        [brush.color().getNSColor() set];
        [NSBezierPath fillRect:NSMakeRect(x, y, w, h)];
    }
    _unlockFocus();
}


void QPainter::setClipping(bool)
{
    _logNotYetImplemented();
}

void QPainter::setClipRegion(const QRegion &)
{
    _logNotYetImplemented();
}

const QRegion &QPainter::clipRegion() const
{
    _logPartiallyImplemented();
    static QRegion null;
    return null;
}

bool QPainter::hasClipping() const
{
    _logNotYetImplemented();
    return false;
}

void QPainter::setClipRect(const QRect &)
{
    _logNotYetImplemented();
}


void QPainter::setClipRect(int,int,int,int)
{
    _logNotYetImplemented();
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
    _logNeverImplemented();
    return CopyROP;
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
     _logNotYetImplemented();
#endif
}

void QPainter::translate(double dx, double dy)
{
    _logNeverImplemented();
}


void QPainter::scale(double dx, double dy)
{
    _logNeverImplemented();
}


bool QPainter::begin(const QPaintDevice *bd)
{
    _logNeverImplemented();
    data->bufferDevice = bd;
    return true;
}


bool QPainter::end()
{
    _logNeverImplemented();
    data->bufferDevice = 0L;
    return true;
}


QPaintDevice *QPainter::device() const
{
    _logPartiallyImplemented();
    return (QPaintDevice *)data->bufferDevice;
}

void QPainter::_lockFocus()
{
#if 0
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
#endif
}

void QPainter::_unlockFocus()
{
#if 0
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
#endif
}


