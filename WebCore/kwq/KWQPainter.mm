/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQPainter.h"

#import "KWQAssertions.h"
#import "KWQExceptions.h"
#import "KWQFontMetrics.h"
#import "KWQKHTMLPart.h"
#import "KWQPaintDevice.h"
#import "KWQPixmap.h"
#import "KWQPointArray.h"
#import "KWQPrinter.h"
#import "KWQPtrStack.h"
#import "KWQWidget.h"
#import "KWQFoundationExtras.h"
#import "WebCoreGraphicsBridge.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreImageRendererFactory.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

// NSColor, NSBezierPath, NSGraphicsContext and WebCoreTextRenderer
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

struct QPState {
    QPState() : paintingDisabled(false) { }
    QFont font;
    QPen pen;
    QBrush brush;
    QRegion clip;
    bool paintingDisabled;
};

struct QPainterPrivate {
    QPainterPrivate() : textRenderer(0), focusRingPath(0), focusRingWidth(0), focusRingOffset(0),
                        hasFocusRingColor(false) { }
    ~QPainterPrivate() { KWQRelease(textRenderer); KWQRelease(focusRingPath); }
    QPState state;
    QPtrStack<QPState> stack;
    id <WebCoreTextRenderer> textRenderer;
    QFont textRendererFont;
    NSBezierPath *focusRingPath;
    int focusRingWidth;
    int focusRingOffset;
    bool hasFocusRingColor;
    QColor focusRingColor;
};

static inline void _fillRectXX(float x, float y, float w, float h, const QColor& col);
QPainter::QPainter() : data(new QPainterPrivate), _isForPrinting(false), _usesInactiveTextBackgroundColor(false), _updatingControlTints(false)
{
}

QPainter::QPainter(bool forPrinting) : data(new QPainterPrivate), _isForPrinting(forPrinting), 
    _usesInactiveTextBackgroundColor(false), _updatingControlTints(false)
{
}

QPainter::~QPainter()
{
    delete data;
}

QPaintDevice *QPainter::device() const
{
    static QPrinter printer;
    static QPaintDevice screen;
    return _isForPrinting ? &printer : &screen;
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

void QPainter::setPen(QRgb rgb)
{
    data->state.pen.setStyle(SolidLine);
    data->state.pen.setColor(rgb);
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

void QPainter::setBrush(QRgb rgb)
{
    data->state.brush.setStyle(SolidPattern);
    data->state.brush.setColor(rgb);
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
        ERROR("ERROR void QPainter::restore() stack is empty");
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
        
    if (data->state.brush.style() != NoBrush)
        _fillRectXX(x, y, w, h, data->state.brush.color());

    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        NSFrameRect(NSMakeRect(x, y, w, h));
    }
}

void QPainter::_setColorFromBrush()
{
    [nsColor(data->state.brush.color()) set];
}

void QPainter::_setColorFromPen()
{
    [nsColor(data->state.pen.color()) set];
}

// This is only used to draw borders.
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

    NSPoint p1 = NSMakePoint(x1, y1);
    NSPoint p2 = NSMakePoint(x2, y2);
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DotLine || penStyle == DashLine) {
        if (x1 == x2) {
            p1.y += width;
            p2.y -= width;
        }
        else {
            p1.x += width;
            p2.x -= width;
        }
    }
    
    if (((int)width)%2) {
        if (x1 == x2) {
            // We're a vertical line.  Adjust our x.
            p1.x += 0.5;
            p2.x += 0.5;
        }
        else {
            // We're a horizontal line. Adjust our y.
            p1.y += 0.5;
            p2.y += 0.5;
        }
    }
    
    NSBezierPath *path = [[NSBezierPath alloc] init];
    [path setLineWidth:width];

    int patWidth = 0;
    switch (penStyle) {
    case NoPen:
    case SolidLine:
        break;
    case DotLine:
        patWidth = (int)width;
        break;
    case DashLine:
        patWidth = 3*(int)width;
        break;
    }

    _setColorFromPen();
    
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    BOOL flag = [graphicsContext shouldAntialias];
    [graphicsContext setShouldAntialias: NO];
    
    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (x1 == x2) {
            _fillRectXX(p1.x-width/2, p1.y-width, width, width, data->state.pen.color());
            _fillRectXX(p2.x-width/2, p2.y, width, width, data->state.pen.color());
        }
        else {
            _fillRectXX(p1.x-width, p1.y-width/2, width, width, data->state.pen.color());
            _fillRectXX(p2.x, p2.y-width/2, width, width, data->state.pen.color());
        }
        
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = ((x1 == x2) ? (y2 - y1) : (x2 - x1)) - 2*(int)width;
        int remainder = distance%patWidth;
        int coverage = distance-remainder;
        int numSegments = coverage/patWidth;

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = numSegments%2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder/2;
                }
                else
                    patternOffset = patWidth/2;
            }
            else if (!evenNumberOfSegments) {
                if (remainder)
                    patternOffset = (patWidth - remainder)/2;
            }
        }
        
        const float dottedLine[2] = { patWidth, patWidth };
        [path setLineDash:dottedLine count:2 phase:patternOffset];
    }
    
    [path moveToPoint:p1];
    [path lineToPoint:p2];

    [path stroke];
    
    [path release];

    [graphicsContext setShouldAntialias: flag];
}


// This method is only used to draw the little circles used in lists.
void QPainter::drawEllipse(int x, int y, int w, int h)
{
    // This code can only handle circles, not ellipses. But khtml only
    // uses it for circles.
    ASSERT(w == h);

    if (data->state.paintingDisabled)
        return;
        
    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextBeginPath(context);
    float r = (float)w / 2;
    CGContextAddArc(context, x + r, y + r, r, 0, 2*M_PI, true);
    CGContextClosePath(context);

    if (data->state.brush.style() != NoBrush) {
        _setColorFromBrush();
	if (data->state.pen.style() != NoPen) {
	    // stroke and fill
	    _setColorFromPen();
	    uint penWidth = data->state.pen.width();
	    if (penWidth == 0) 
		penWidth++;
	    CGContextSetLineWidth(context, penWidth);
	    CGContextDrawPath(context, kCGPathFillStroke);
	} else {
	    CGContextFillPath(context);
	}
    }
    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
	uint penWidth = data->state.pen.width();
	if (penWidth == 0) 
	    penWidth++;
        CGContextSetLineWidth(context, penWidth);
        CGContextStrokePath(context);
    }
}


void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)
{ 
    // Only supports arc on circles.  That's all khtml needs.
    ASSERT(w == h);

    if (data->state.paintingDisabled)
        return;
    
    if (data->state.pen.style() != NoPen) {
        CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        CGContextBeginPath(context);
	
	float r = (float)w / 2;
        float fa = (float)a / 16;
        float falen =  fa + (float)alen / 16;
        CGContextAddArc(context, x + r, y + r, r, -fa * M_PI/180, -falen * M_PI/180, true);
	
        _setColorFromPen();
        CGContextSetLineWidth(context, data->state.pen.width());
        CGContextStrokePath(context);
    }
}

void QPainter::drawConvexPolygon(const QPointArray &points)
{
    if (data->state.paintingDisabled)
        return;

    int npoints = points.size();
    if (npoints <= 1)
        return;

    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, points[0].x(), points[0].y());
    for (int i = 1; i < npoints; i++)
        CGContextAddLineToPoint(context, points[i].x(), points[i].y());
    CGContextClosePath(context);

    if (data->state.brush.style() != NoBrush) {
        _setColorFromBrush();
        CGContextEOFillPath(context);
    }

    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        CGContextSetLineWidth(context, data->state.pen.width());
        CGContextStrokePath(context);
    }
}

void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix)
{        
    drawPixmap(p.x(), p.y(), pix);
}

void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix, const QRect &r)
{
    drawPixmap(p.x(), p.y(), pix, r.x(), r.y(), r.width(), r.height());
}

struct CompositeOperator
{
    const char *name;
    NSCompositingOperation value;
};

const int NUM_COMPOSITE_OPERATORS = 14;

struct CompositeOperator compositeOperators[NUM_COMPOSITE_OPERATORS] = {
    { "clear", NSCompositeClear },
    { "copy", NSCompositeCopy },
    { "source-over", NSCompositeSourceOver },
    { "source-in", NSCompositeSourceIn },
    { "source-out", NSCompositeSourceOut },
    { "source-atop", NSCompositeSourceAtop },
    { "destination-over", NSCompositeDestinationOver },
    { "destination-in", NSCompositeDestinationIn },
    { "destination-out", NSCompositeDestinationOut },
    { "destination-atop", NSCompositeDestinationAtop },
    { "xor", NSCompositeXOR },
    { "darker", NSCompositePlusDarker },
    { "highlight", NSCompositeHighlight },
    { "lighter", NSCompositePlusLighter }
};

int QPainter::getCompositeOperation(CGContextRef context)
{
    return (int)[[WebCoreImageRendererFactory sharedFactory] CGCompositeOperationInContext:context];
}

void QPainter::setCompositeOperation (CGContextRef context, QString op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperationFromString:op.getNSString() inContext:context];
}

void QPainter::setCompositeOperation (CGContextRef context, int op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperation:op inContext:context];
}

int QPainter::compositeOperatorFromString (QString aString)
{
    NSCompositingOperation op = NSCompositeSourceOver;
    
    if (aString.length()) {
        const char *operatorString = aString.ascii();
        for (int i = 0; i < NUM_COMPOSITE_OPERATORS; i++) {
            if (strcasecmp (operatorString, compositeOperators[i].name) == 0) {
                return compositeOperators[i].value;
            }
        }
    }
    return (int)op;
}

void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix, const QRect &r, const QString &compositeOperator)
{
    drawPixmap(p.x(), p.y(), pix, r.x(), r.y(), r.width(), r.height(), compositeOperatorFromString(compositeOperator));
}

void QPainter::drawPixmap( int x, int y, const QPixmap &pixmap,
                           int sx, int sy, int sw, int sh, int compositeOperator, CGContextRef context)
{
    drawPixmap (x, y, sw, sh, pixmap, sx, sy, sw, sh, compositeOperator, context);
}

void QPainter::drawPixmap( int x, int y, int w, int h, const QPixmap &pixmap,
                           int sx, int sy, int sw, int sh, int compositeOperator, CGContextRef context)
{
    drawFloatPixmap ((float)x, (float)y, (float)w, (float)h, pixmap, (float)sx, (float)sy, (float)sw, (float)sh, compositeOperator, context);
}

void QPainter::drawFloatPixmap( float x, float y, float w, float h, const QPixmap &pixmap,
                           float sx, float sy, float sw, float sh, int compositeOperator, CGContextRef context)
{
    if (data->state.paintingDisabled)
        return;
        
    if (sw == -1)
        sw = pixmap.width();
    if (sh == -1)
        sh = pixmap.height();

    if (w == -1)
        w = pixmap.width();
    if (h == -1)
        h = pixmap.height();

    NSRect inRect = NSMakeRect(x, y, w, h);
    NSRect fromRect = NSMakeRect(sx, sy, sw, sh);
    
    KWQ_BLOCK_EXCEPTIONS;
    [pixmap.imageRenderer drawImageInRect:inRect
                                      fromRect:fromRect compositeOperator:(NSCompositingOperation)compositeOperator context:context];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::drawTiledPixmap( int x, int y, int w, int h,
				const QPixmap &pixmap, int sx, int sy, CGContextRef context)
{
    if (data->state.paintingDisabled)
        return;
    
    KWQ_BLOCK_EXCEPTIONS;
    [pixmap.imageRenderer tileInRect:NSMakeRect(x, y, w, h) fromPoint:NSMakePoint(sx, sy) context:context];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::_updateRenderer()
{
    if (data->textRenderer == 0 || data->state.font != data->textRendererFont) {
        data->textRendererFont = data->state.font;
        id <WebCoreTextRenderer> oldRenderer = data->textRenderer;
	KWQ_BLOCK_EXCEPTIONS;
        data->textRenderer = KWQRetain([[WebCoreTextRendererFactory sharedFactory]
            rendererWithFont:data->textRendererFont.getNSFont()
            usingPrinterFont:data->textRendererFont.isPrinterFont()]);
        KWQRelease(oldRenderer);
	KWQ_UNBLOCK_EXCEPTIONS;
    }
}
    
void QPainter::drawText(int x, int y, int tabWidth, int xpos, int, int, int alignmentFlags, const QString &qstring)
{
    if (data->state.paintingDisabled)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);    

    _updateRenderer();

    const UniChar* str = (const UniChar*)qstring.unicode();

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, str, qstring.length(), 0, qstring.length());
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(data->state.pen.color());
    style.families = families;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    
    if (alignmentFlags & Qt::AlignRight)
        x -= ROUND_TO_INT([data->textRenderer floatWidthForRun:&run style:&style widths:0]);

    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = NSMakePoint(x, y);
    [data->textRenderer drawRun:&run style:&style geometry:&geometry];
}

void QPainter::drawText(int x, int y, int tabWidth, int xpos, const QChar *str, int len, int from, int to, int toAdd, const QColor &backgroundColor, QPainter::TextDirection d, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps)
{
    if (data->state.paintingDisabled || len <= 0)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);

    _updateRenderer();

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;
        
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)str, len, from, to);    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(data->state.pen.color());
    style.backgroundColor = backgroundColor.isValid() ? nsColor(backgroundColor) : nil;
    style.rtl = d == RTL ? true : false;
    style.visuallyOrdered = visuallyOrdered;
    style.letterSpacing = letterSpacing;
    style.wordSpacing = wordSpacing;
    style.smallCaps = smallCaps;
    style.families = families;
    style.padding = toAdd;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = NSMakePoint(x, y);
    [data->textRenderer drawRun:&run style:&style geometry:&geometry];
}

void QPainter::drawHighlightForText(int x, int y, int h, int tabWidth, int xpos,
    const QChar *str, int len, int from, int to, int toAdd, const QColor &backgroundColor, 
    QPainter::TextDirection d, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps)
{
    if (data->state.paintingDisabled || len <= 0)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);

    _updateRenderer();

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;
        
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)str, len, from, to);    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(data->state.pen.color());
    style.backgroundColor = backgroundColor.isValid() ? nsColor(backgroundColor) : nil;
    style.rtl = d == RTL ? true : false;
    style.visuallyOrdered = visuallyOrdered;
    style.letterSpacing = letterSpacing;
    style.wordSpacing = wordSpacing;
    style.smallCaps = smallCaps;
    style.families = families;    
    style.padding = toAdd;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = NSMakePoint(x, y);
    geometry.selectionY = y;
    geometry.selectionHeight = h;
    geometry.useFontMetricsForSelectionYAndHeight = false;
    [data->textRenderer drawHighlightForRun:&run style:&style geometry:&geometry];
}

void QPainter::drawLineForText(int x, int y, int yOffset, int width)
{
    if (data->state.paintingDisabled)
        return;
    _updateRenderer();
    [data->textRenderer
        drawLineForCharacters: NSMakePoint(x, y)
               yOffset:(float)yOffset
                 width: width
                 color:nsColor(data->state.pen.color())
             thickness:data->state.pen.width()];
}

void QPainter::drawLineForMisspelling(int x, int y, int width)
{
    if (data->state.paintingDisabled)
        return;
    _updateRenderer();
    [data->textRenderer drawLineForMisspelling:NSMakePoint(x, y) withWidth:width];
}

int QPainter::misspellingLineThickness() const
{
    return [data->textRenderer misspellingLineThickness];
}

static int getBlendedColorComponent(int c, int a)
{
    // We use white.
    float alpha = (float)(a) / 255;
    int whiteBlend = 255 - a;
    c -= whiteBlend;
    return (int)(c/alpha);
}

QColor QPainter::selectedTextBackgroundColor() const
{
    NSColor *color = _usesInactiveTextBackgroundColor ? [NSColor secondarySelectedControlColor] : [NSColor selectedTextBackgroundColor];
    // this needs to always use device colorspace so it can de-calibrate the color for
    // QColor to possibly recalibrate it
    color = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    
    QColor col = QColor((int)(255 * [color redComponent]), (int)(255 * [color greenComponent]), (int)(255 * [color blueComponent]));
    
    // Attempt to make the selection 60% transparent.  We do this by applying a standard blend and then
    // seeing if the resultant color is still within the 0-255 range.
    int alpha = 153;
    int red = getBlendedColorComponent(col.red(), alpha);
    int green = getBlendedColorComponent(col.green(), alpha);
    int blue = getBlendedColorComponent(col.blue(), alpha);
    if (red >= 0 && red <= 255 && green >= 0 && green <= 255 && blue >= 0 && blue <= 255)
        return QColor(qRgba(red, green, blue, alpha));
    return col;
}

// A fillRect helper to work around the fact that NSRectFill uses copy mode, not source over.
static inline void _fillRectXX(float x, float y, float w, float h, const QColor& col)
{
    [nsColor(col) set];
    NSRectFillUsingOperation(NSMakeRect(x,y,w,h), NSCompositeSourceOver);
}

void QPainter::fillRect(int x, int y, int w, int h, const QBrush &brush)
{
    if (data->state.paintingDisabled)
        return;

    if (brush.style() == SolidPattern)
        _fillRectXX(x, y, w, h, brush.color());
}

void QPainter::fillRect(const QRect &rect, const QBrush &brush)
{
    fillRect(rect.left(), rect.top(), rect.width(), rect.height(), brush);
}

void QPainter::addClip(const QRect &rect)
{
    [NSBezierPath clipRect:rect];
}

Qt::RasterOp QPainter::rasterOp() const
{
    return CopyROP;
}

void QPainter::setRasterOp(RasterOp)
{
}

void QPainter::setPaintingDisabled(bool f)
{
    data->state.paintingDisabled = f;
}

bool QPainter::paintingDisabled() const
{
    return data->state.paintingDisabled;
}

CGContextRef QPainter::currentContext()
{
    return (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
}

void QPainter::beginTransparencyLayer(float opacity)
{
    [NSGraphicsContext saveGraphicsState];
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
}

void QPainter::endTransparencyLayer()
{
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    CGContextEndTransparencyLayer(context);
    [NSGraphicsContext restoreGraphicsState];
}

void QPainter::setShadow(int x, int y, int blur, const QColor& color)
{
    // Check for an invalid color, as this means that the color was not set for the shadow
    // and we should therefore just use the default shadow color.
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    if (!color.isValid()) {
        CGContextSetShadow(context, CGSizeMake(x,-y), blur); // y is flipped.
    } else {
	CGColorRef colorCG = cgColor(color);
        CGContextSetShadowWithColor(context,
                                    CGSizeMake(x,-y), // y is flipped.
                                    blur, 
                                    colorCG);
        CGColorRelease(colorCG);
    }
}

void QPainter::clearShadow()
{
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    CGContextSetShadowWithColor(context, CGSizeZero, 0, NULL);
}

void QPainter::initFocusRing(int width, int offset)
{
    clearFocusRing();
    data->focusRingWidth = width;
    data->hasFocusRingColor = false;
    data->focusRingOffset = offset;
    data->focusRingPath = KWQRetainNSRelease([[NSBezierPath alloc] init]);
    [data->focusRingPath setWindingRule:NSNonZeroWindingRule];
}

void QPainter::initFocusRing(int width, int offset, const QColor &color)
{
    initFocusRing(width, offset);
    data->hasFocusRingColor = true;
    data->focusRingColor = color;
}

void QPainter::addFocusRingRect(int x, int y, int width, int height)
{
    ASSERT(data->focusRingPath);
    NSRect rect = NSMakeRect(x, y, width, height);
    int offset = (data->focusRingWidth-1)/2 + data->focusRingOffset;
    rect = NSInsetRect(rect, -offset, -offset);
    [data->focusRingPath appendBezierPathWithRect:rect];
}

void QPainter::drawFocusRing()
{
    ASSERT(data->focusRingPath);
    if (data->state.paintingDisabled)
        return;

    if ([data->focusRingPath elementCount] == 0) {
        ERROR("Request to draw focus ring with no control points");
        return;
    }
    
    NSRect bounds = [data->focusRingPath bounds];
    if (!NSIsEmptyRect(bounds)) {
        int radius = (data->focusRingWidth-1)/2;
        NSColor *color = data->hasFocusRingColor ? nsColor(data->focusRingColor) : nil;
        [NSGraphicsContext saveGraphicsState];
        [[WebCoreGraphicsBridge sharedBridge] setFocusRingStyle:NSFocusRingOnly radius:radius color:color];
        [data->focusRingPath fill];
        [NSGraphicsContext restoreGraphicsState];   
    }
}

void QPainter::clearFocusRing()
{
    if (data->focusRingPath) {
        KWQRelease(data->focusRingPath);
        data->focusRingPath = nil;
    }
}
