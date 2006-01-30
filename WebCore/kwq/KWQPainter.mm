/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KWQPainter.h"

#import <kxmlcore/Assertions.h>
#import <kxmlcore/Vector.h>
#import "Brush.h"
#import "KWQExceptions.h"
#import "KWQFont.h"
#import "KWQFoundationExtras.h"
#import "Pen.h"
#import "Image.h"
#import "KWQPrinter.h"
#import "KWQRegion.h"
#import "KWQFontMetrics.h"
#import "WebCoreGraphicsBridge.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreImageRendererFactory.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

#if SVG_SUPPORT
#import "kcanvas/device/quartz/KRenderingDeviceQuartz.h"
#endif

namespace WebCore {

// NSColor, NSBezierPath, NSGraphicsContext and WebCoreTextRenderer
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

struct QPState {
    QPState() : paintingDisabled(false) { }
    QFont font;
    Pen pen;
    WebCore::Brush brush;
    QRegion clip;
    bool paintingDisabled;
};

struct QPainterPrivate {
    QPainterPrivate();
    ~QPainterPrivate();
    QPState state;
    
    Vector<QPState> stack;
    id <WebCoreTextRenderer> textRenderer;
    QFont textRendererFont;
    NSBezierPath *focusRingPath;
    int focusRingWidth;
    int focusRingOffset;
    bool hasFocusRingColor;
    Color focusRingColor;
#if SVG_SUPPORT
    KRenderingDevice *renderingDevice;
#endif
};

QPainterPrivate::QPainterPrivate() : textRenderer(0), focusRingPath(0), focusRingWidth(0), focusRingOffset(0),
                        hasFocusRingColor(false)
{

}

QPainterPrivate::~QPainterPrivate()
{
    KWQRelease(textRenderer);
    KWQRelease(focusRingPath);
}

static inline void _fillRectXX(float x, float y, float w, float h, const Color& col);
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

const Pen &QPainter::pen() const
{
    return data->state.pen;
}

void QPainter::setPen(const Pen &pen)
{
    data->state.pen = pen;
}

void QPainter::setPen(Pen::PenStyle style)
{
    data->state.pen.setStyle(style);
    data->state.pen.setColor(Color::black);
    data->state.pen.setWidth(0);
}

void QPainter::setPen(RGBA32 rgb)
{
    data->state.pen.setStyle(Pen::SolidLine);
    data->state.pen.setColor(rgb);
    data->state.pen.setWidth(0);
}

void QPainter::setBrush(const WebCore::Brush &brush)
{
    data->state.brush = brush;
}

void QPainter::setBrush(WebCore::Brush::BrushStyle style)
{
    data->state.brush.setStyle(style);
    data->state.brush.setColor(Color::black);
}

void QPainter::setBrush(RGBA32 rgb)
{
    data->state.brush.setStyle(WebCore::Brush::SolidPattern);
    data->state.brush.setColor(rgb);
}

const WebCore::Brush& QPainter::brush() const
{
    return data->state.brush;
}

IntRect QPainter::xForm(const IntRect &aRect) const
{
    // No difference between device and model coords, so the identity transform is ok.
    return aRect;
}

void QPainter::save()
{
    if (data->state.paintingDisabled)
        return;

    data->stack.append(data->state);

    [NSGraphicsContext saveGraphicsState]; 
}

void QPainter::restore()
{
    if (data->state.paintingDisabled)
        return;

    if (data->stack.isEmpty()) {
        ERROR("ERROR void QPainter::restore() stack is empty");
        return;
    }
    data->state = data->stack.last();
    data->stack.removeLast();
     
    [NSGraphicsContext restoreGraphicsState];
}

// Draws a filled rectangle with a stroked border.
void QPainter::drawRect(int x, int y, int w, int h)
{
    if (data->state.paintingDisabled)
        return;
         
    if (data->state.brush.style() != WebCore::Brush::NoBrush)
        _fillRectXX(x, y, w, h, data->state.brush.color());

    if (data->state.pen.style() != Pen::Pen::NoPen) {
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

    Pen::PenStyle penStyle = data->state.pen.style();
    if (penStyle == Pen::Pen::NoPen)
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
    if (penStyle == Pen::DotLine || penStyle == Pen::DashLine) {
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
    case Pen::NoPen:
    case Pen::SolidLine:
        break;
    case Pen::DotLine:
        patWidth = (int)width;
        break;
    case Pen::DashLine:
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
    // FIXME: CG added CGContextAddEllipseinRect in Tiger, so we should be able to quite easily draw an ellipse.
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

    if (data->state.brush.style() != WebCore::Brush::NoBrush) {
        _setColorFromBrush();
        if (data->state.pen.style() != Pen::NoPen) {
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
    if (data->state.pen.style() != Pen::NoPen) {
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
    
    if (data->state.pen.style() != Pen::NoPen) {
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

void QPainter::drawConvexPolygon(const IntPointArray &points)
{
    if (data->state.paintingDisabled)
        return;

    int npoints = points.size();
    if (npoints <= 1)
        return;

    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, false);
    
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, points[0].x(), points[0].y());
    for (int i = 1; i < npoints; i++)
        CGContextAddLineToPoint(context, points[i].x(), points[i].y());
    CGContextClosePath(context);

    if (data->state.brush.style() != WebCore::Brush::NoBrush) {
        _setColorFromBrush();
        CGContextEOFillPath(context);
    }

    if (data->state.pen.style() != Pen::NoPen) {
        _setColorFromPen();
        CGContextSetLineWidth(context, data->state.pen.width());
        CGContextStrokePath(context);
    }

    CGContextRestoreGState(context);
}

void QPainter::drawImageAtPoint(const Image& image, const IntPoint &p, Image::CompositeOperator compositeOperator)
{        
    drawImage(image, p.x(), p.y(), 0, 0, -1, -1, compositeOperator);
}

void QPainter::drawImageInRect(const Image& image, const IntRect &r, Image::CompositeOperator compositeOperator)
{
    drawImage(image, r.x(), r.y(), r.width(), r.height(), 0, 0, -1, -1, compositeOperator);
}

int QPainter::getCompositeOperation(CGContextRef context)
{
    return (int)[[WebCoreImageRendererFactory sharedFactory] CGCompositeOperationInContext:context];
}

void QPainter::setCompositeOperation (CGContextRef context, const QString &op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperationFromString:op.getNSString() inContext:context];
}

void QPainter::setCompositeOperation (CGContextRef context, int op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperation:op inContext:context];
}

void QPainter::drawImage(const Image& image, int x, int y,
                         int sx, int sy, int sw, int sh, Image::CompositeOperator compositeOperator, void* context)
{
    drawImage(image, x, y, sw, sh, sx, sy, sw, sh, compositeOperator, context);
}

void QPainter::drawImage(const Image& image, int x, int y, int w, int h,
                         int sx, int sy, int sw, int sh, Image::CompositeOperator compositeOperator, void* context)
{
    drawFloatImage(image, (float)x, (float)y, (float)w, (float)h, (float)sx, (float)sy, (float)sw, (float)sh, compositeOperator, context);
}

void QPainter::drawFloatImage(const Image &image, float x, float y, float w, float h, 
                              float sx, float sy, float sw, float sh, Image::CompositeOperator compositeOperator, void* context)
{
    if (data->state.paintingDisabled)
        return;
                
    KWQ_BLOCK_EXCEPTIONS;

    float tsw = sw;
    float tsh = sh;
    float tw = w;
    float th = h;
        
    if (tsw == -1)
        tsw = image.width();
    if (tsh == -1)
        tsh = image.height();

    if (tw == -1)
        tw = image.width();
    if (th == -1)
        th = image.height();

    NSRect inRect = NSMakeRect(x, y, tw, th);
    NSRect fromRect = NSMakeRect(sx, sy, tsw, tsh);
    
    [image.imageRenderer() drawImageInRect:inRect fromRect:fromRect compositeOperator:(NSCompositingOperation)compositeOperator context:(CGContextRef)context];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::drawTiledImage(const Image& image, int x, int y, int w, int h,
                              int sx, int sy, void* context)
{
    if (data->state.paintingDisabled)
        return;
    
    KWQ_BLOCK_EXCEPTIONS;
    NSRect tempRect = { {x, y}, {w, h} }; // workaround for 4213314
    NSPoint tempPoint = { sx, sy };
    [image.imageRenderer() tileInRect:tempRect fromPoint:tempPoint context:(CGContextRef)context];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::drawScaledAndTiledImage(const Image &image, int x, int y, int w, int h, int sx, int sy, int sw, int sh, 
                                       TileRule hRule, TileRule vRule, void* context)
{
    if (data->state.paintingDisabled)
        return;
    
    if (hRule == STRETCH && vRule == STRETCH)
        // Just do a scale.
        return drawImage(image, x, y, w, h, sx, sy, sw, sh, Image::CompositeSourceOver, context);

    KWQ_BLOCK_EXCEPTIONS;
    [image.imageRenderer() scaleAndTileInRect:NSMakeRect(x, y, w, h) fromRect:NSMakeRect(sx, sy, sw, sh) 
                        withHorizontalTileRule:(WebImageTileRule)hRule withVerticalTileRule:(WebImageTileRule)vRule context:(CGContextRef)context];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QPainter::_updateRenderer()
{
    if (data->textRenderer == 0 || data->state.font != data->textRendererFont) {
        data->textRendererFont = data->state.font;
        id <WebCoreTextRenderer> oldRenderer = data->textRenderer;
        KWQ_BLOCK_EXCEPTIONS;
        data->textRenderer = KWQRetain([[WebCoreTextRendererFactory sharedFactory]
            rendererWithFont:data->textRendererFont.getWebCoreFont()]);
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
        x -= lroundf([data->textRenderer floatWidthForRun:&run style:&style]);

    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = NSMakePoint(x, y);
    [data->textRenderer drawRun:&run style:&style geometry:&geometry];
}

void QPainter::drawText(int x, int y, int tabWidth, int xpos, const QChar *str, int len, int from, int to, int toAdd, const Color &backgroundColor, QPainter::TextDirection d, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps)
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
    style.rtl = d == RTL;
    style.directionalOverride = visuallyOrdered;
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
    const QChar *str, int len, int from, int to, int toAdd, const Color &backgroundColor, 
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
    style.rtl = d == RTL;
    style.directionalOverride = visuallyOrdered;
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

Color QPainter::selectedTextBackgroundColor() const
{
    NSColor *color = _usesInactiveTextBackgroundColor ? [NSColor secondarySelectedControlColor] : [NSColor selectedTextBackgroundColor];
    // this needs to always use device colorspace so it can de-calibrate the color for
    // Color to possibly recalibrate it
    color = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    
    Color col = Color((int)(255 * [color redComponent]), (int)(255 * [color greenComponent]), (int)(255 * [color blueComponent]));
    
    // Attempt to make the selection 60% transparent.  We do this by applying a standard blend and then
    // seeing if the resultant color is still within the 0-255 range.
    int alpha = 153;
    int red = getBlendedColorComponent(col.red(), alpha);
    int green = getBlendedColorComponent(col.green(), alpha);
    int blue = getBlendedColorComponent(col.blue(), alpha);
    if (red >= 0 && red <= 255 && green >= 0 && green <= 255 && blue >= 0 && blue <= 255)
        return Color(red, green, blue, alpha);
    return col;
}

// A fillRect helper to work around the fact that NSRectFill uses copy mode, not source over.
static inline void _fillRectXX(float x, float y, float w, float h, const Color& col)
{
    [nsColor(col) set];
    NSRectFillUsingOperation(NSMakeRect(x,y,w,h), NSCompositeSourceOver);
}

void QPainter::fillRect(int x, int y, int w, int h, const WebCore::Brush &brush)
{
    if (data->state.paintingDisabled)
        return;

    if (brush.style() == WebCore::Brush::SolidPattern)
        _fillRectXX(x, y, w, h, brush.color());
}

void QPainter::fillRect(const IntRect &rect, const WebCore::Brush &brush)
{
    fillRect(rect.x(), rect.y(), rect.width(), rect.height(), brush);
}

void QPainter::addClip(const IntRect &rect)
{
    if (data->state.paintingDisabled)
        return;

    [NSBezierPath clipRect:rect];
}

void QPainter::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                  const IntSize& bottomLeft, const IntSize& bottomRight)
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

    // Ok, the curves can fit.
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    
    // Add the four ellipses to the path.  Technically this really isn't good enough, since we could end up
    // not clipping the other 3/4 of the ellipse we don't care about.  We're relying on the fact that for
    // normal use cases these ellipses won't overlap one another (or when they do the curvature of one will
    // be subsumed by the other).
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), topLeft.width() * 2, topLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - topRight.width() * 2, rect.y(),
                                                  topRight.width() * 2, topRight.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.bottom() - bottomLeft.height() * 2,
                                                  bottomLeft.width() * 2, bottomLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - bottomRight.width() * 2,
                                                  rect.bottom() - bottomRight.height() * 2,
                                                  bottomRight.width() * 2, bottomRight.height() * 2));
    
    // Now add five rects (one for each edge rect in between the rounded corners and one for the interior).
    CGContextAddRect(context, CGRectMake(rect.x() + topLeft.width(), rect.y(),
                                         rect.width() - topLeft.width() - topRight.width(),
                                         kMax(topLeft.height(), topRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x() + bottomLeft.width(), 
                                         rect.bottom() - kMax(bottomLeft.height(), bottomRight.height()),
                                         rect.width() - bottomLeft.width() - bottomRight.width(),
                                         kMax(bottomLeft.height(), bottomRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x(), rect.y() + topLeft.height(),
                                         kMax(topLeft.width(), bottomLeft.width()), rect.height() - topLeft.height() - bottomLeft.height()));
    CGContextAddRect(context, CGRectMake(rect.right() - kMax(topRight.width(), bottomRight.width()),
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

#if SVG_SUPPORT
KRenderingDeviceContext *QPainter::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(currentContext());
}

KRenderingDevice *QPainter::renderingDevice()
{
    static KRenderingDevice *sharedRenderingDevice = new KRenderingDeviceQuartz();
    return sharedRenderingDevice;
}
#endif

void QPainter::beginTransparencyLayer(float opacity)
{
    if (data->state.paintingDisabled)
        return;
    [NSGraphicsContext saveGraphicsState];
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
}

void QPainter::endTransparencyLayer()
{
    if (data->state.paintingDisabled)
        return;
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    CGContextEndTransparencyLayer(context);
    [NSGraphicsContext restoreGraphicsState];
}

void QPainter::setShadow(int x, int y, int blur, const Color& color)
{
    if (data->state.paintingDisabled)
        return;
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
    if (data->state.paintingDisabled)
        return;
    CGContextRef context = (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);
    CGContextSetShadowWithColor(context, CGSizeZero, 0, NULL);
}

void QPainter::initFocusRing(int width, int offset)
{
    if (data->state.paintingDisabled)
        return;
    clearFocusRing();
    data->focusRingWidth = width;
    data->hasFocusRingColor = false;
    data->focusRingOffset = offset;
    data->focusRingPath = KWQRetainNSRelease([[NSBezierPath alloc] init]);
    [data->focusRingPath setWindingRule:NSNonZeroWindingRule];
}

void QPainter::initFocusRing(int width, int offset, const Color &color)
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
    ASSERT(data->focusRingPath);
    NSRect rect = NSMakeRect(x, y, width, height);
    int offset = (data->focusRingWidth-1)/2 + data->focusRingOffset;
    rect = NSInsetRect(rect, -offset, -offset);
    [data->focusRingPath appendBezierPathWithRect:rect];
}

void QPainter::drawFocusRing()
{
    if (data->state.paintingDisabled)
        return;

    ASSERT(data->focusRingPath);

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

}
