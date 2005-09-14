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
#import "KWQBrush.h"
#import "KWQExceptions.h"
#import "KWQFoundationExtras.h"
#import "KWQPaintDevice.h"
#import "KWQPen.h"
#import "KWQPixmap.h"
#import "KWQPrinter.h"
#import "KWQPtrStack.h"
#import "KWQRegion.h"
#import "WebCoreGraphicsBridge.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreImageRendererFactory.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

// NSColor, NSGraphicsContext and WebCoreTextRenderer
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
    QPainterPrivate() : textRenderer(nil), focusRingWidth(0), focusRingOffset(0), hasFocusRingColor(false) { }
    ~QPainterPrivate() { KWQRelease(textRenderer); }
    QPState state;
    QPtrStack<QPState> stack;
    id <WebCoreTextRenderer> textRenderer;
    QFont textRendererFont;
    int focusRingWidth;
    int focusRingOffset;
    bool hasFocusRingColor;
    QColor focusRingColor;
};

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

const QFont& QPainter::font() const
{
    return data->state.font;
}

void QPainter::setFont(const QFont& aFont)
{
    data->state.font = aFont;
}

QFontMetrics QPainter::fontMetrics() const
{
    return data->state.font;
}

const QPen& QPainter::pen() const
{
    return data->state.pen;
}

void QPainter::setPen(const QPen& pen)
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

void QPainter::setBrush(const QBrush& brush)
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

const QBrush& QPainter::brush() const
{
    return data->state.brush;
}

void QPainter::save()
{
    data->stack.push(new QPState(data->state));

    CGContextSaveGState(currentContext());
}

void QPainter::restore()
{
    if (data->stack.isEmpty()) {
        ERROR("ERROR void QPainter::restore() stack is empty");
	return;
    }

    CGContextRestoreGState(currentContext());

    QPState *ps = data->stack.pop();
    data->state = *ps;
    delete ps;
}

void QPainter::drawRect(int x, int y, int w, int h)
{
    if (data->state.paintingDisabled)
        return;

    CGContextRef context = currentContext();

    if (data->state.brush.style() != NoBrush) {
        setFillColorFromCurrentBrush();
        CGContextFillRect(context, CGRectMake(x, y, w, h));
    }

    if (data->state.pen.style() != NoPen) {
        setStrokeColorAndLineWidthFromCurrentPen();
        CGContextStrokeRect(context, CGRectMake(x, y, w, h));
    }
}

void QPainter::setFillColor(const QBrush& brush)
{
    CGColorRef color = cgColor(brush.color());
    CGContextSetFillColorWithColor(currentContext(), color);
    CGColorRelease(color);
}

void QPainter::setFillColorFromCurrentBrush()
{
    CGColorRef color = cgColor(data->state.brush.color());
    CGContextSetFillColorWithColor(currentContext(), color);
    CGColorRelease(color);
}

void QPainter::setFillColorFromCurrentPen()
{
    CGColorRef color = cgColor(data->state.pen.color());
    CGContextSetFillColorWithColor(currentContext(), color);
    CGColorRelease(color);
}

void QPainter::setStrokeColorAndLineWidthFromCurrentPen()
{
    CGContextRef context = currentContext();

    CGColorRef color = cgColor(data->state.pen.color());
    CGContextSetStrokeColorWithColor(context, color);
    CGColorRelease(color);

    unsigned width = data->state.pen.width();
    CGContextSetLineWidth(context, width > 0 ? width : 1);
}

// This is only used to draw borders.
void QPainter::drawLine(int x0, int y0, int x1, int y1)
{
    if (data->state.paintingDisabled)
        return;

    PenStyle penStyle = data->state.pen.style();
    if (penStyle == NoPen)
        return;
    int width = data->state.pen.width();
    if (width < 1)
        width = 1;

    CGPoint points[2] = { { x0, y0 }, { x1, y1 } };
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y0+y1)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.

    if (penStyle == DotLine || penStyle == DashLine) {
        if (x0 == x1) {
            points[0].y += width;
            points[1].y -= width;
        } else {
            points[0].x += width;
            points[1].x -= width;
        }
    }
    
    if (width % 2) {
        if (x0 == x1) {
            // We're a vertical line.  Adjust our x.
            points[0].x += 0.5;
            points[1].x += 0.5;
        } else {
            // We're a horizontal line. Adjust our y.
            points[0].y += 0.5;
            points[1].y += 0.5;
        }
    }

    int patWidth = 0;
    switch (penStyle) {
    case NoPen:
    case SolidLine:
        break;
    case DotLine:
        patWidth = width;
        break;
    case DashLine:
        patWidth = 3 * width;
        break;
    }

    CGContextRef context = currentContext();

    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, FALSE);
    
    setStrokeColorAndLineWidthFromCurrentPen();
        
    if (patWidth) {
        // Do a rect fill of our endpoints. This ensures we always have the
        // appearance of being a border. We then draw the actual dotted/dashed line.
        // FIXME: Can we use CGContextSetLineCap instead?
        setFillColorFromCurrentPen();
        CGRect rects[2];
        if (x0 == x1) {
            rects[0] = CGRectMake(points[0].x - width / 2, points[0].y - width, width, width);
            rects[1] = CGRectMake(points[1].x - width / 2, points[1].y, width, width);
        } else {
            rects[0] = CGRectMake(points[0].x - width, points[0].y-width / 2, width, width);
            rects[1] = CGRectMake(points[1].x, points[1].y - width / 2, width, width);
        }
        CGContextFillRects(context, rects, 2);
        
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = ((x0 == x1) ? (y1 - y0) : (x1 - x0)) - 2 * width;
        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1;
        else {
            bool evenNumberOfSegments = numSegments % 2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2;
                }
                else
                    patternOffset = patWidth / 2;
            }
            else if (!evenNumberOfSegments) {
                if (remainder)
                    patternOffset = (patWidth - remainder)/2;
            }
        }
        
        const float dottedLine[2] = { patWidth, patWidth };
        CGContextSetLineDash(context, patternOffset, dottedLine, 2);
    }
    
    CGContextStrokeLineSegments(context, points, 2);

    CGContextRestoreGState(context);
}


// This method is only used to draw the little circles used in lists.
void QPainter::drawEllipse(int x, int y, int w, int h)
{
    if (data->state.paintingDisabled)
        return;
        
    CGContextRef context = currentContext();

    if (data->state.brush.style() != NoBrush) {
        setFillColorFromCurrentBrush();
        CGContextFillEllipseInRect(context, CGRectMake(x, y, w, h));
    }

    if (data->state.pen.style() != NoPen) {
        setStrokeColorAndLineWidthFromCurrentPen();
        CGContextStrokeEllipseInRect(context, CGRectMake(x, y, w, h));
    }
}

void QPainter::drawArc(int x, int y, int w, int h, int a, int alen)
{ 
    // Only supports arc on circles. That's all KHTML needs.
    ASSERT(w == h);

    if (data->state.paintingDisabled)
        return;

    if (data->state.pen.style() != NoPen) {
        CGContextRef context = currentContext();

        CGContextBeginPath(context);
	float r = w / 2.f;
        float fa = a / 16.f;
        float falen = fa + alen / 16.f;
        CGContextAddArc(context, x + r, y + r, r, -fa * M_PI/180, -falen * M_PI/180, true);

        setStrokeColorAndLineWidthFromCurrentPen();
        CGContextStrokePath(context);
    }
}

void QPainter::drawConvexPolygon(const QPointArray& points)
{
    if (data->state.paintingDisabled)
        return;

    int npoints = points.size();
    if (npoints <= 1)
        return;

    CGContextRef context = currentContext();

    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, FALSE);
    
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, points[0].x(), points[0].y());
    for (int i = 1; i < npoints; i++)
        CGContextAddLineToPoint(context, points[i].x(), points[i].y());
    CGContextClosePath(context);

    bool gotBrush = data->state.brush.style() != NoBrush;
    bool gotPen = data->state.pen.style() != NoPen;

    if (gotBrush)
        setFillColorFromCurrentBrush();
    if (gotPen)
        setStrokeColorAndLineWidthFromCurrentPen();

    if (gotBrush && gotPen)
        CGContextDrawPath(context, kCGPathEOFillStroke);
    else if (gotBrush)
        CGContextEOFillPath(context);
    else if (gotPen)
        CGContextStrokePath(context);

    CGContextRestoreGState(context);
}

void QPainter::drawPixmap(const QPoint& p, const QPixmap& pix)
{        
    drawPixmap(p.x(), p.y(), pix);
}

void QPainter::drawPixmap(const QPoint& p, const QPixmap& pix, const QRect& r)
{
    drawPixmap(p.x(), p.y(), pix, r.x(), r.y(), r.width(), r.height());
}

int QPainter::getCompositeOperation(CGContextRef context)
{
    return [[WebCoreImageRendererFactory sharedFactory] CGCompositeOperationInContext:context];
}

void QPainter::setCompositeOperation(CGContextRef context, const QString& op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperation:compositeOperatorFromString(op) inContext:context];
}

void QPainter::setCompositeOperation(CGContextRef context, int op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperation:op inContext:context];
}

int QPainter::compositeOperatorFromString(const QString& aString)
{
    if (!aString.isEmpty()) {
        const char *operatorString = aString.ascii();
        for (int i = 0; i < NUM_COMPOSITE_OPERATORS; ++i)
            if (strcasecmp(operatorString, compositeOperators[i].name) == 0)
                return compositeOperators[i].value;
    }
    return NSCompositeSourceOver;
}

void QPainter::drawPixmap(const QPoint& p, const QPixmap& pix, const QRect& r, const QString& compositeOperator)
{
    drawPixmap(p.x(), p.y(), pix, r.x(), r.y(), r.width(), r.height(), compositeOperatorFromString(compositeOperator));
}

void QPainter::drawPixmap(int x, int y, const QPixmap& pixmap,
    int sx, int sy, int sw, int sh, int compositeOperator, CGContextRef context)
{
    drawPixmap(x, y, sw, sh, pixmap, sx, sy, sw, sh, compositeOperator, context);
}

void QPainter::drawPixmap(int x, int y, int w, int h, const QPixmap& pixmap,
    int sx, int sy, int sw, int sh, int compositeOperator, CGContextRef context)
{
    drawFloatPixmap(x, y, w, h, pixmap, sx, sy, sw, sh, compositeOperator, context);
}

void QPainter::drawFloatPixmap(float x, float y, float w, float h, const QPixmap& pixmap,
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
                                 fromRect:fromRect
                        compositeOperator:(NSCompositingOperation)compositeOperator
                                  context:context];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::drawTiledPixmap(int x, int y, int w, int h,
                               const QPixmap& pixmap, int sx, int sy, CGContextRef context)
{
    if (data->state.paintingDisabled)
        return;
    
    KWQ_BLOCK_EXCEPTIONS;
    NSRect inRect = { { x, y }, { w, h } }; // workaround for 4213314
    NSPoint fromPoint = { sx, sy };
    [pixmap.imageRenderer tileInRect:inRect fromPoint:fromPoint context:context];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::drawScaledAndTiledPixmap(int x, int y, int w, int h, const QPixmap& pixmap, int sx, int sy, int sw, int sh, 
                                        TileRule hRule, TileRule vRule, CGContextRef context)
{
    if (data->state.paintingDisabled)
        return;
    
    if (hRule == STRETCH && vRule == STRETCH)
        // Just do a scale.
        return drawPixmap(x, y, w, h, pixmap, sx, sy, sw, sh, -1, context);

    KWQ_BLOCK_EXCEPTIONS;
    [pixmap.imageRenderer scaleAndTileInRect:NSMakeRect(x, y, w, h)
                                    fromRect:NSMakeRect(sx, sy, sw, sh) 
                      withHorizontalTileRule:(WebImageTileRule)hRule
                        withVerticalTileRule:(WebImageTileRule)vRule
                                     context:context];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::updateTextRenderer()
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
    
void QPainter::drawText(int x, int y, int tabWidth, int xpos, int, int, int alignmentFlags, const QString& qstring)
{
    if (data->state.paintingDisabled)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // CSS fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);    

    updateTextRenderer();

    const UniChar *str = (const UniChar *)qstring.unicode();

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

void QPainter::drawText(int x, int y, int tabWidth, int xpos, const QChar *str, int len, int from, int to, int toAdd, const QColor& backgroundColor, QPainter::TextDirection d, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps)
{
    if (data->state.paintingDisabled || len <= 0)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);

    updateTextRenderer();

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
    style.rtl = d == RTL;
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
    const QChar *str, int len, int from, int to, int toAdd, const QColor& backgroundColor, 
    QPainter::TextDirection d, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps)
{
    if (data->state.paintingDisabled || len <= 0)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);

    updateTextRenderer();

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
    style.rtl = d == RTL;
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
    updateTextRenderer();
    [data->textRenderer drawLineForCharacters:NSMakePoint(x, y)
                                      yOffset:yOffset
                                        width:width
                                        color:nsColor(data->state.pen.color())
                                    thickness:data->state.pen.width()];
}

void QPainter::drawLineForMisspelling(int x, int y, int width)
{
    if (data->state.paintingDisabled)
        return;
    updateTextRenderer();
    [data->textRenderer drawLineForMisspelling:NSMakePoint(x, y) withWidth:width];
}

int QPainter::misspellingLineThickness() const
{
    return [data->textRenderer misspellingLineThickness];
}

static int getBlendedColorComponent(int c, int a)
{
    return (c + a - 255) * 255 / a;
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

void QPainter::fillRect(int x, int y, int w, int h, const QBrush& brush)
{
    if (data->state.paintingDisabled)
        return;

    if (brush.style() == SolidPattern) {
        setFillColor(brush);
        CGContextFillRect(currentContext(), CGRectMake(x, y, w, h));
    }
}

void QPainter::fillRect(const QRect& rect, const QBrush& brush)
{
    if (data->state.paintingDisabled)
        return;

    if (brush.style() == SolidPattern) {
        setFillColor(brush);
        CGContextFillRect(currentContext(), rect);
    }
}

void QPainter::addClip(const QRect& rect)
{
    if (data->state.paintingDisabled)
        return;

    CGContextClipToRect(currentContext(), rect);
}

void QPainter::addRoundedRectClip(const QRect& rect, const QSize& topLeft, const QSize& topRight,
                                  const QSize& bottomLeft, const QSize& bottomRight)
{
    if (data->state.paintingDisabled)
        return;

    // Need sufficient width and height to contain these curves.  Sanity check our top/bottom
    // values and our width/height values to make sure the curves can all fit.
    int requiredWidth = kMax(topLeft.width() + topRight.width(), bottomLeft.width() + bottomRight.width());
    if (requiredWidth > rect.width())
        return;
    int requiredHeight = kMax(topLeft.height() + bottomLeft.height(), topRight.height() + bottomRight.height());
    if (requiredHeight > rect.height())
        return;
 
    // Clip to our rect.
    addClip(rect);

    // OK, the curves can fit.
    CGContextRef context = currentContext();
    
    CGContextBeginPath(context);

    // Add the four ellipses to the path.  Technically this really isn't good enough, since we could end up
    // not clipping the other 3/4 of the ellipse we don't care about.  We're relying on the fact that for
    // normal use cases these ellipses won't overlap one another (or when they do the curvature of one will
    // be subsumed by the other).
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), topLeft.width() * 2, topLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.x() + rect.width() - topRight.width() * 2, rect.y(),
                                                  topRight.width() * 2, topRight.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y() + rect.height() - bottomLeft.height() * 2,
                                                  bottomLeft.width() * 2, bottomLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.x() + rect.width() - bottomRight.width() * 2,
                                                  rect.y() + rect.height() - bottomRight.height() * 2,
                                                  bottomRight.width() * 2, bottomRight.height() * 2));
    
    // Now add five rects (one for each edge rect in between the rounded corners and one for the interior).
    CGContextAddRect(context, CGRectMake(rect.x() + topLeft.width(), rect.y(),
                                         rect.width() - topLeft.width() - topRight.width(),
                                         kMax(topLeft.height(), topRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x() + bottomLeft.width(), 
                                         rect.y() + rect.height() - kMax(bottomLeft.height(), bottomRight.height()),
                                         rect.width() - bottomLeft.width() - bottomRight.width(),
                                         kMax(bottomLeft.height(), bottomRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x(), rect.y() + topLeft.height(),
                                         kMax(topLeft.width(), bottomLeft.width()), rect.height() - topLeft.height() - bottomLeft.height()));
    CGContextAddRect(context, CGRectMake(rect.x() + rect.width() - kMax(topRight.width(), bottomRight.width()),
                                         rect.y() + topRight.height(),
                                         kMax(topRight.width(), bottomRight.width()), rect.height() - topRight.height() - bottomRight.height()));
    CGContextAddRect(context, CGRectMake(rect.x() + kMax(topLeft.width(), bottomLeft.width()),
                                         rect.y() + kMax(topLeft.height(), topRight.height()),
                                         rect.width() - kMax(topLeft.width(), bottomLeft.width()) - kMax(topRight.width(), bottomRight.width()),
                                         rect.height() - kMax(topLeft.height(), topRight.height()) - kMax(bottomLeft.height(), bottomRight.height())));
    CGContextClip(context);
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
    ASSERT(!data->state.paintingDisabled);
    return (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
}

void QPainter::beginTransparencyLayer(float opacity)
{
    if (data->state.paintingDisabled)
        return;
    CGContextRef context = currentContext();
    CGContextSaveGState(context);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
}

void QPainter::endTransparencyLayer()
{
    if (data->state.paintingDisabled)
        return;
    CGContextRef context = currentContext();
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

void QPainter::setShadow(int x, int y, int blur, const QColor& color)
{
    if (data->state.paintingDisabled)
        return;
    // If the color is invalid, use the default shadow color.
    CGContextRef context = currentContext();
    if (!color.isValid()) {
        CGContextSetShadow(context, CGSizeMake(x, -y), blur); // y is flipped.
    } else {
	CGColorRef colorCG = cgColor(color);
        CGContextSetShadowWithColor(context, CGSizeMake(x, -y), blur, colorCG); // y is flipped.
        CGColorRelease(colorCG);
    }
}

void QPainter::clearShadow()
{
    if (data->state.paintingDisabled)
        return;
    CGContextSetShadowWithColor(currentContext(), CGSizeZero, 0, 0);
}

void QPainter::initFocusRing(int width, int offset)
{
    if (data->state.paintingDisabled)
        return;
    data->focusRingWidth = width;
    data->hasFocusRingColor = false;
    data->focusRingOffset = offset;
    CGContextBeginPath(currentContext());
}

void QPainter::initFocusRing(int width, int offset, const QColor& color)
{
    if (data->state.paintingDisabled)
        return;
    initFocusRing(width, offset);
    data->hasFocusRingColor = true;
    data->focusRingColor = color;
}

void QPainter::addFocusRingRect(int x, int y, int width, int height)
{
    if (data->state.paintingDisabled)
        return;
    CGRect rect = CGRectMake(x, y, width, height);
    int offset = (data->focusRingWidth - 1) / 2 + data->focusRingOffset;
    CGContextAddRect(currentContext(), CGRectInset(rect, -offset, -offset));
}

void QPainter::drawFocusRing()
{
    if (data->state.paintingDisabled)
        return;

    CGContextRef context = currentContext();
    if (CGRectIsEmpty(CGContextGetPathBoundingBox(context)))
        return;
  
    CGContextSaveGState(context);

    int radius = (data->focusRingWidth - 1) / 2;
    NSColor *color = data->hasFocusRingColor ? nsColor(data->focusRingColor) : nil;
    [[WebCoreGraphicsBridge sharedBridge] setFocusRingStyle:NSFocusRingOnly radius:radius color:color];

    CGContextFillPath(context);

    CGContextRestoreGState(context);
}
