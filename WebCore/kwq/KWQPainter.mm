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
    QPState() : exclusiveOrMode(false), paintingDisabled(false) { }
    QFont font;
    QPen pen;
    QBrush brush;
    QRegion clip;
    bool exclusiveOrMode;
    bool paintingDisabled;
};

struct QPainterPrivate {
    QPState state;
    QPtrStack<QPState> stack;
};

QPainter::QPainter() : data(new QPainterPrivate)
{
}

QPainter::~QPainter()
{
    delete data;
}
    
const QFont &QPainter::font() const
{
    return data->state.font;
}

void QPainter::setFont(const QFont &aFont)
{
    data->state.font = aFont;
}

QFontMetrics QPainter::fontMetrics() const
{
    return data->state.font;
}

const QPen &QPainter::pen() const
{
    return data->state.pen;
}

void QPainter::setPen(const QPen &pen)
{
    data->state.pen = pen;
}

void QPainter::setPen(PenStyle style)
{
    data->state.pen.setStyle(style);
    data->state.pen.setColor(Qt::black);
    data->state.pen.setWidth(0);
}

void QPainter::setBrush(const QBrush &brush)
{
    data->state.brush = brush;
}

void QPainter::setBrush(BrushStyle style)
{
    data->state.brush.setStyle(style);
    data->state.brush.setColor(Qt::black);
}

const QBrush &QPainter::brush() const
{
    return data->state.brush;
}

QRect QPainter::xForm(const QRect &aRect) const
{
    // No difference between device and model coords, so the identity transform is ok.
    return aRect;
}

void QPainter::save()
{
    data->stack.push(new QPState(data->state));
    [NSGraphicsContext saveGraphicsState]; 
}

void QPainter::restore()
{
    if (data->stack.isEmpty()) {
        KWQDEBUG("ERROR void QPainter::restore() stack is empty\n");
	return;
    }
    QPState *ps = data->stack.pop();
    data->state = *ps;
    delete ps;

    [NSGraphicsContext restoreGraphicsState];
}

// Draws a filled rectangle with a stroked border.
void QPainter::drawRect(int x, int y, int w, int h)
{
    if (data->state.paintingDisabled)
        return;
        
    if (data->state.brush.style() != NoBrush) {
        _setColorFromBrush();
        [NSBezierPath fillRect:NSMakeRect(x, y, w, h)];
    }
    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        [NSBezierPath strokeRect:NSMakeRect(x, y, w, h)];
    }
}

void QPainter::_setColorFromBrush()
{
    [data->state.brush.color().getNSColor() set];
}

void QPainter::_setColorFromPen()
{
    [data->state.pen.color().getNSColor() set];
}

// This is only used to draw borders around text, and lines over text.
void QPainter::drawLine(int x1, int y1, int x2, int y2)
{
    if (data->state.paintingDisabled)
        return;
        
    PenStyle penStyle = data->state.pen.style();
    if (penStyle == NoPen)
        return;
    float width = data->state.pen.width();
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

    _setColorFromPen();
    [path stroke];
}


// This method is only used to draw the little circles used in lists.
void QPainter::drawEllipse(int x, int y, int w, int h)
{
    if (data->state.paintingDisabled)
        return;
        
    NSBezierPath *path = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (x, y, w, h)];
    
    if (data->state.brush.style() != NoBrush) {
        _setColorFromBrush();
        [path fill];
    }
    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        [path stroke];
    }
}


// Only supports arc on circles.  That's all khtml needs.
void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)
{
    if (data->state.paintingDisabled)
        return;
        
    if (data->state.pen.style() != NoPen) {
        if (w != h) {
            KWQDEBUG("ERROR (INCOMPLETE IMPLEMENTATION) void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)\nOnly supports drawing arcs on a circle.\n");
        }
        
        NSBezierPath *path = [[NSBezierPath alloc] init];
        float fa = (float)a / 16;
        float falen =  fa + (float)alen / 16;
        [path appendBezierPathWithArcWithCenter:NSMakePoint(x + w / 2, y + h / 2) 
                                         radius:(float)w / 2 
                                     startAngle:-fa
                                       endAngle:-falen
                                      clockwise:YES];
    
        _setColorFromPen();
        [path stroke];
        [path release];
    }
}

void QPainter::drawLineSegments(const QPointArray &points, int index, int nlines)
{
    if (data->state.paintingDisabled)
        return;
    
    _drawPoints (points, 0, index, nlines, false);
}

void QPainter::drawPolyline(const QPointArray &points, int index, int npoints)
{
    _drawPoints (points, 0, index, npoints, false);
}

void QPainter::drawPolygon(const QPointArray &points, bool winding, int index, 
    int npoints)
{
    _drawPoints (points, winding, index, npoints, true);
}

void QPainter::drawConvexPolygon(const QPointArray &points)
{
    _drawPoints (points, false, 0, -1, true);
}

void QPainter::_drawPoints (const QPointArray &_points, bool winding, int index, int _npoints, bool fill)
{
    if (data->state.paintingDisabled)
        return;
        
    int npoints = _npoints != -1 ? _npoints : _points.size()-index;
    NSPoint points[npoints];
    for (int i = 0; i < npoints; i++) {
        points[i].x = _points[index+i].x();
        points[i].y = _points[index+i].y();
    }
            
    NSBezierPath *path = [[NSBezierPath alloc] init];
    [path appendBezierPathWithPoints:&points[0] count:npoints];
    [path closePath]; // Qt always closes the path.  Determined empirically.
    
    if (fill && data->state.brush.style() != NoBrush) {
        [path setWindingRule:winding ? NSNonZeroWindingRule : NSEvenOddWindingRule];
        _setColorFromBrush();
        [path fill];
    }

    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        [path stroke];
    }
    
    [path release];
}


void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix)
{        
    drawPixmap(p.x(), p.y(), pix);
}


void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix, const QRect &r)
{
    drawPixmap(p.x(), p.y(), pix, r.x(), r.y(), r.width(), r.height());
}

void QPainter::drawPixmap( int x, int y, const QPixmap &pixmap,
                           int sx, int sy, int sw, int sh )
{
    if (data->state.paintingDisabled)
        return;
        
    if (sw == -1)
        sw = pixmap.width();
    if (sh == -1)
        sh = pixmap.height();
    
    [pixmap.imageRenderer beginAnimationInRect:NSMakeRect(x, y, sw, sh)
                                      fromRect:NSMakeRect(sx, sy, sw, sh)];
}

void QPainter::drawTiledPixmap( int x, int y, int w, int h,
				const QPixmap &pixmap, int sx, int sy )
{
    if (data->state.paintingDisabled)
        return;
    
    [pixmap.imageRenderer tileInRect:NSMakeRect(x, y, w, h) fromPoint:NSMakePoint(sx, sy)];
}

void QPainter::drawText(int x, int y, int, int, int alignmentFlags, const QString &qstring)
{
    if (data->state.paintingDisabled)
        return;
        
    [[[WebCoreTextRendererFactory sharedFactory]
        rendererWithFamily:data->state.font.getNSFamily() traits:data->state.font.getNSTraits() size:data->state.font.getNSSize()]
    	drawCharacters:(const UniChar *)qstring.unicode() stringLength:qstring.length()
        fromCharacterPosition:0 toCharacterPosition:qstring.length() atPoint:NSMakePoint(x, y)
        withTextColor:data->state.pen.color().getNSColor() backgroundColor:nil];
}

void QPainter::drawText(int x, int y, const QChar *str, int len, int from, int to, const QColor &backgroundColor)
{
    if (data->state.paintingDisabled || len <= 0)
        return;
        
    [[[WebCoreTextRendererFactory sharedFactory]
        rendererWithFamily:data->state.font.getNSFamily() traits:data->state.font.getNSTraits() size:data->state.font.getNSSize()]
    	drawCharacters:(const UniChar *)str stringLength:len
        fromCharacterPosition:from toCharacterPosition:to atPoint:NSMakePoint(x, y)
        withTextColor:data->state.pen.color().getNSColor() backgroundColor:backgroundColor.isValid() ? backgroundColor.getNSColor() : nil];
}

void QPainter::drawUnderlineForText(int x, int y, const QChar *str, int len)
{
    if (data->state.paintingDisabled)
        return;
        
    [[[WebCoreTextRendererFactory sharedFactory]
        rendererWithFamily:data->state.font.getNSFamily() traits:data->state.font.getNSTraits() size:data->state.font.getNSSize()]
        drawUnderlineForCharacters:(const UniChar *)str stringLength:len
        atPoint:NSMakePoint(x,y) withColor:data->state.pen.color().getNSColor()];
}

QColor QPainter::selectedTextBackgroundColor()
{
    NSColor *color = [[NSColor selectedTextBackgroundColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    return QColor((int)(255 * [color redComponent]), (int)(255 * [color greenComponent]), (int)(255 * [color blueComponent]));
}

void QPainter::fillRect(int x, int y, int w, int h, const QBrush &brush)
{
    if (data->state.paintingDisabled)
        return;
        
    if (brush.style() == SolidPattern) {
        [brush.color().getNSColor() set];
        [NSBezierPath fillRect:NSMakeRect(x, y, w, h)];
    }
}

void QPainter::addClip(const QRect &rect)
{
    [NSBezierPath clipRect:rect];
}

Qt::RasterOp QPainter::rasterOp() const
{
    return data->state.exclusiveOrMode ? XorROP : CopyROP;
}

void QPainter::setRasterOp(RasterOp op)
{
    data->state.exclusiveOrMode = op == XorROP;
}

void QPainter::setPaintingDisabled(bool f)
{
    data->state.paintingDisabled = f;
}
