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

#import "config.h"
#import "GraphicsContext.h"

#import "Font.h"
#import "FloatRect.h"
#import "FoundationExtras.h"
#import "IntPointArray.h"
#import "KRenderingDeviceQuartz.h"
#import "KWQExceptions.h"
#import "WebCoreGraphicsBridge.h"
#import "WebCoreImageRendererFactory.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

// FIXME: A lot more of this should use CoreGraphics instead of AppKit.
// FIXME: A lot more of this should move into GraphicsContextCG.cpp.

namespace WebCore {

// NSColor, NSBezierPath, NSGraphicsContext and WebCoreTextRenderer
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

struct GraphicsContextState {
    GraphicsContextState() : paintingDisabled(false) { }
    Font font;
    Pen pen;
    Brush brush;
    bool paintingDisabled;
};

struct GraphicsContextPrivate {
    GraphicsContextPrivate();
    ~GraphicsContextPrivate();

    GraphicsContextState state;    
    Vector<GraphicsContextState> stack;
    id <WebCoreTextRenderer> textRenderer;
    Font textRendererFont;
    CGMutablePathRef focusRingPath;
    int focusRingWidth;
    int focusRingOffset;
#if SVG_SUPPORT
    KRenderingDevice *renderingDevice;
#endif
};

// A fillRect helper to work around the fact that NSRectFill uses copy mode, not source over.
static inline void fillRectSourceOver(float x, float y, float w, float h, const Color& col)
{
    [nsColor(col) set];
    NSRectFillUsingOperation(NSMakeRect(x, y, w, h), NSCompositeSourceOver);
}


GraphicsContextPrivate::GraphicsContextPrivate()
    : textRenderer(0), focusRingPath(0), focusRingWidth(0), focusRingOffset(0)
{
}

GraphicsContextPrivate::~GraphicsContextPrivate()
{
    KWQRelease(textRenderer);
    CGPathRelease(focusRingPath);
}

GraphicsContext::GraphicsContext()
    : m_data(new GraphicsContextPrivate)
    , m_isForPrinting(false)
    , m_usesInactiveTextBackgroundColor(false)
    , m_updatingControlTints(false)
{
}

GraphicsContext::GraphicsContext(bool forPrinting)
    : m_data(new GraphicsContextPrivate)
    , m_isForPrinting(forPrinting)
    , m_usesInactiveTextBackgroundColor(false)
    , m_updatingControlTints(false)
{
}

GraphicsContext::~GraphicsContext()
{
    delete m_data;
}

const Font& GraphicsContext::font() const
{
    return m_data->state.font;
}

void GraphicsContext::setFont(const Font& aFont)
{
    m_data->state.font = aFont;
}

const Pen& GraphicsContext::pen() const
{
    return m_data->state.pen;
}

void GraphicsContext::setPen(const Pen& pen)
{
    m_data->state.pen = pen;
}

void GraphicsContext::setPen(Pen::PenStyle style)
{
    m_data->state.pen.setStyle(style);
    m_data->state.pen.setColor(Color::black);
    m_data->state.pen.setWidth(0);
}

void GraphicsContext::setPen(RGBA32 rgb)
{
    m_data->state.pen.setStyle(Pen::SolidLine);
    m_data->state.pen.setColor(rgb);
    m_data->state.pen.setWidth(0);
}

void GraphicsContext::setBrush(const Brush& brush)
{
    m_data->state.brush = brush;
}

void GraphicsContext::setBrush(Brush::BrushStyle style)
{
    m_data->state.brush.setStyle(style);
    m_data->state.brush.setColor(Color::black);
}

void GraphicsContext::setBrush(RGBA32 rgb)
{
    m_data->state.brush.setStyle(Brush::SolidPattern);
    m_data->state.brush.setColor(rgb);
}

const Brush& GraphicsContext::brush() const
{
    return m_data->state.brush;
}

void GraphicsContext::save()
{
    if (m_data->state.paintingDisabled)
        return;

    m_data->stack.append(m_data->state);

    [NSGraphicsContext saveGraphicsState]; 
}

void GraphicsContext::restore()
{
    if (m_data->state.paintingDisabled)
        return;

    if (m_data->stack.isEmpty()) {
        LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }
    m_data->state = m_data->stack.last();
    m_data->stack.removeLast();
     
    [NSGraphicsContext restoreGraphicsState];
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(int x, int y, int w, int h)
{
    if (m_data->state.paintingDisabled)
        return;
         
    if (m_data->state.brush.style() != Brush::NoBrush)
        fillRectSourceOver(x, y, w, h, m_data->state.brush.color());

    if (m_data->state.pen.style() != Pen::Pen::NoPen) {
        setColorFromPen();
        NSFrameRect(NSMakeRect(x, y, w, h));
    }
}

void GraphicsContext::setColorFromBrush()
{
    [nsColor(m_data->state.brush.color()) set];
}
  
void GraphicsContext::setColorFromPen()
{
    [nsColor(m_data->state.pen.color()) set];
}
  
  // This is only used to draw borders.
void GraphicsContext::drawLine(int x1, int y1, int x2, int y2)
{
    if (m_data->state.paintingDisabled)
        return;

    Pen::PenStyle penStyle = m_data->state.pen.style();
    if (penStyle == Pen::Pen::NoPen)
        return;
    float width = m_data->state.pen.width();
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

    setColorFromPen();
    
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    BOOL flag = [graphicsContext shouldAntialias];
    [graphicsContext setShouldAntialias: NO];
    
    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (x1 == x2) {
            fillRectSourceOver(p1.x-width/2, p1.y-width, width, width, m_data->state.pen.color());
            fillRectSourceOver(p2.x-width/2, p2.y, width, width, m_data->state.pen.color());
        } else {
            fillRectSourceOver(p1.x-width, p1.y-width/2, width, width, m_data->state.pen.color());
            fillRectSourceOver(p2.x, p2.y-width/2, width, width, m_data->state.pen.color());
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
void GraphicsContext::drawEllipse(int x, int y, int w, int h)
{
    // FIXME: CG added CGContextAddEllipseinRect in Tiger, so we should be able to quite easily draw an ellipse.
    // This code can only handle circles, not ellipses. But khtml only
    // uses it for circles.
    ASSERT(w == h);

    if (m_data->state.paintingDisabled)
        return;
        
    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextBeginPath(context);
    float r = (float)w / 2;
    CGContextAddArc(context, x + r, y + r, r, 0, 2*M_PI, true);
    CGContextClosePath(context);

    if (m_data->state.brush.style() != Brush::NoBrush) {
        setColorFromBrush();
        if (m_data->state.pen.style() != Pen::NoPen) {
            // stroke and fill
            setColorFromPen();
            uint penWidth = m_data->state.pen.width();
            if (penWidth == 0) 
                penWidth++;
            CGContextSetLineWidth(context, penWidth);
            CGContextDrawPath(context, kCGPathFillStroke);
        } else {
            CGContextFillPath(context);
        }
    }
    if (m_data->state.pen.style() != Pen::NoPen) {
        setColorFromPen();
        uint penWidth = m_data->state.pen.width();
        if (penWidth == 0) 
            penWidth++;
        CGContextSetLineWidth(context, penWidth);
        CGContextStrokePath(context);
    }
}


void GraphicsContext::drawArc (int x, int y, int w, int h, int a, int alen)
{ 
    // Only supports arc on circles.  That's all khtml needs.
    ASSERT(w == h);

    if (m_data->state.paintingDisabled)
        return;
    
    if (m_data->state.pen.style() != Pen::NoPen) {
        CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        CGContextBeginPath(context);
        
        float r = (float)w / 2;
        float fa = (float)a / 16;
        float falen =  fa + (float)alen / 16;
        CGContextAddArc(context, x + r, y + r, r, -fa * M_PI/180, -falen * M_PI/180, true);
        
        setColorFromPen();
        CGContextSetLineWidth(context, m_data->state.pen.width());
        CGContextStrokePath(context);
    }
}

void GraphicsContext::drawConvexPolygon(const IntPointArray& points)
{
    if (m_data->state.paintingDisabled)
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

    if (m_data->state.brush.style() != Brush::NoBrush) {
        setColorFromBrush();
        CGContextEOFillPath(context);
    }

    if (m_data->state.pen.style() != Pen::NoPen) {
        setColorFromPen();
        CGContextSetLineWidth(context, m_data->state.pen.width());
        CGContextStrokePath(context);
    }

    CGContextRestoreGState(context);
}

int GraphicsContext::getCompositeOperation(CGContextRef context)
{
    return (int)[[WebCoreImageRendererFactory sharedFactory] CGCompositeOperationInContext:context];
}

void GraphicsContext::setCompositeOperation (CGContextRef context, const QString& op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperationFromString:op.getNSString() inContext:context];
}

void GraphicsContext::setCompositeOperation (CGContextRef context, int op)
{
    [[WebCoreImageRendererFactory sharedFactory] setCGCompositeOperation:op inContext:context];
}

void GraphicsContext::drawFloatImage(Image* image, float x, float y, float w, float h, 
                              float sx, float sy, float sw, float sh, Image::CompositeOperator compositeOperator, void* context)
{
    if (m_data->state.paintingDisabled)
        return;

    float tsw = sw;
    float tsh = sh;
    float tw = w;
    float th = h;
        
    if (tsw == -1)
        tsw = image->width();
    if (tsh == -1)
        tsh = image->height();

    if (tw == -1)
        tw = image->width();
    if (th == -1)
        th = image->height();

    image->drawInRect(FloatRect(x, y, tw, th), FloatRect(sx, sy, tsw, tsh), compositeOperator, context);
}

void GraphicsContext::drawTiledImage(Image* image, int x, int y, int w, int h, int sx, int sy, void* context)
{
    if (m_data->state.paintingDisabled)
        return;
    
    image->tileInRect(FloatRect(x, y, w, h), FloatPoint(sx, sy), context);
}

void GraphicsContext::drawScaledAndTiledImage(Image* image, int x, int y, int w, int h, int sx, int sy, int sw, int sh, 
    Image::TileRule hRule, Image::TileRule vRule, void* context)
{
    if (m_data->state.paintingDisabled)
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile)
        // Just do a scale.
        return drawImage(image, x, y, w, h, sx, sy, sw, sh, Image::CompositeSourceOver, context);

    image->scaleAndTileInRect(FloatRect(x, y, w, h), FloatRect(sx, sy, sw, sh), hRule, vRule, context);
}

void GraphicsContext::updateTextRenderer()
{
    if (m_data->textRenderer == 0 || m_data->state.font != m_data->textRendererFont) {
        m_data->textRendererFont = m_data->state.font;
        id <WebCoreTextRenderer> oldRenderer = m_data->textRenderer;
        KWQ_BLOCK_EXCEPTIONS;
        m_data->textRenderer = KWQRetain([[WebCoreTextRendererFactory sharedFactory]
            rendererWithFont:m_data->textRendererFont.getWebCoreFont()]);
        KWQRelease(oldRenderer);
        KWQ_UNBLOCK_EXCEPTIONS;
    }
}
    
void GraphicsContext::drawText(int x, int y, int tabWidth, int xpos, int, int, int alignmentFlags, const QString& qstring)
{
    if (m_data->state.paintingDisabled)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(m_data->state.font, families);    

    updateTextRenderer();

    const UniChar* str = (const UniChar*)qstring.unicode();

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, str, qstring.length(), 0, qstring.length());
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(m_data->state.pen.color());
    style.families = families;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    
    if (alignmentFlags & Qt::AlignRight)
        x -= lroundf([m_data->textRenderer floatWidthForRun:&run style:&style]);

    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = NSMakePoint(x, y);
    [m_data->textRenderer drawRun:&run style:&style geometry:&geometry];
}

void GraphicsContext::drawText(int x, int y, int tabWidth, int xpos, const QChar *str, int len, int from, int to, int toAdd,
    const Color& backgroundColor, TextDirection d, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps)
{
    if (m_data->state.paintingDisabled || len <= 0)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(m_data->state.font, families);

    updateTextRenderer();

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;
        
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)str, len, from, to);    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(m_data->state.pen.color());
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
    [m_data->textRenderer drawRun:&run style:&style geometry:&geometry];
}

void GraphicsContext::drawHighlightForText(int x, int y, int h, int tabWidth, int xpos,
    const QChar *str, int len, int from, int to, int toAdd, const Color& backgroundColor, 
    TextDirection d, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps)
{
    if (m_data->state.paintingDisabled || len <= 0)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(m_data->state.font, families);

    updateTextRenderer();

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;
        
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)str, len, from, to);    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(m_data->state.pen.color());
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
    [m_data->textRenderer drawHighlightForRun:&run style:&style geometry:&geometry];
}

void GraphicsContext::drawLineForText(int x, int y, int yOffset, int width)
{
    if (m_data->state.paintingDisabled)
        return;
    updateTextRenderer();
    [m_data->textRenderer
        drawLineForCharacters:NSMakePoint(x, y)
                      yOffset:(float)yOffset
                        width:width
                        color:nsColor(m_data->state.pen.color())
                    thickness:m_data->state.pen.width()];
}

void GraphicsContext::drawLineForMisspelling(int x, int y, int width)
{
    if (m_data->state.paintingDisabled)
        return;
    updateTextRenderer();
    [m_data->textRenderer drawLineForMisspelling:NSMakePoint(x, y) withWidth:width];
}

int GraphicsContext::misspellingLineThickness() const
{
    return [m_data->textRenderer misspellingLineThickness];
}

static int getBlendedColorComponent(int c, int a)
{
    // We use white.
    float alpha = (float)(a) / 255;
    int whiteBlend = 255 - a;
    c -= whiteBlend;
    return (int)(c/alpha);
}

Color GraphicsContext::selectedTextBackgroundColor() const
{
    NSColor *color = m_usesInactiveTextBackgroundColor ? [NSColor secondarySelectedControlColor] : [NSColor selectedTextBackgroundColor];
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

void GraphicsContext::fillRect(int x, int y, int w, int h, const Brush& brush)
{
    if (m_data->state.paintingDisabled)
        return;

    if (brush.style() == Brush::SolidPattern)
        fillRectSourceOver(x, y, w, h, brush.color());
}

void GraphicsContext::fillRect(const IntRect& rect, const Brush& brush)
{
    fillRect(rect.x(), rect.y(), rect.width(), rect.height(), brush);
}

void GraphicsContext::addClip(const IntRect& rect)
{
    if (m_data->state.paintingDisabled)
        return;

    [NSBezierPath clipRect:rect];
}

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                  const IntSize& bottomLeft, const IntSize& bottomRight)
{
    if (m_data->state.paintingDisabled)
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
    CGContextRef context = currentCGContext();
    
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

void GraphicsContext::setPaintingDisabled(bool f)
{
    m_data->state.paintingDisabled = f;
}

bool GraphicsContext::paintingDisabled() const
{
    return m_data->state.paintingDisabled;
}

CGContextRef GraphicsContext::currentCGContext()
{
    return (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
}

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(currentCGContext());
}
#endif

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (m_data->state.paintingDisabled)
        return;
    CGContextRef context = currentCGContext();
    CGContextSaveGState(context);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
}

void GraphicsContext::endTransparencyLayer()
{
    if (m_data->state.paintingDisabled)
        return;
    CGContextRef context = currentCGContext();
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

void GraphicsContext::setShadow(int x, int y, int blur, const Color& color)
{
    if (m_data->state.paintingDisabled)
        return;
    // Check for an invalid color, as this means that the color was not set for the shadow
    // and we should therefore just use the default shadow color.
    CGContextRef context = currentCGContext();
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

void GraphicsContext::clearShadow()
{
    if (m_data->state.paintingDisabled)
        return;
    CGContextRef context = currentCGContext();
    CGContextSetShadowWithColor(context, CGSizeZero, 0, NULL);
}

void GraphicsContext::initFocusRing(int width, int offset)
{
    if (m_data->state.paintingDisabled)
        return;
    clearFocusRing();
    m_data->focusRingPath = CGPathCreateMutable();
    m_data->focusRingWidth = width;
    m_data->focusRingOffset = offset;
}

void GraphicsContext::addFocusRingRect(int x, int y, int width, int height)
{
    if (m_data->state.paintingDisabled)
        return;
    int radius = (m_data->focusRingWidth - 1) / 2;
    int offset = radius + m_data->focusRingOffset;
    CGPathAddRect(m_data->focusRingPath, 0, CGRectInset(CGRectMake(x, y, width, height), -offset, -offset));
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (m_data->state.paintingDisabled)
        return;
    ASSERT(m_data->focusRingPath);
    int radius = (m_data->focusRingWidth - 1) / 2;
    CGColorRef colorRef = color.isValid() ? cgColor(color) : 0;
    [[WebCoreGraphicsBridge sharedBridge] drawFocusRingWithPath:m_data->focusRingPath radius:radius color:colorRef];
    CGColorRelease(colorRef);
}

void GraphicsContext::clearFocusRing()
{
    CGPathRelease(m_data->focusRingPath);
    m_data->focusRingPath = 0;
}

}
