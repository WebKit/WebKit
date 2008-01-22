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
#include "GraphicsContext.h"

#include "BidiResolver.h"
#include "Font.h"

using namespace std;

namespace WebCore {

class TextRunIterator {
public:
    TextRunIterator()
        : m_textRun(0)
        , m_offset(0)
    {
    }

    TextRunIterator(const TextRun* textRun, unsigned offset)
        : m_textRun(textRun)
        , m_offset(offset)
    {
    }

    TextRunIterator(const TextRunIterator& other)
        : m_textRun(other.m_textRun)
        , m_offset(other.m_offset)
    {
    }

    unsigned offset() const { return m_offset; }
    void increment(BidiResolver<TextRunIterator, BidiCharacterRun>&) { m_offset++; }
    bool atEnd() const { return !m_textRun || m_offset >= m_textRun->length(); }
    UChar current() const { return (*m_textRun)[m_offset]; }
    WTF::Unicode::Direction direction() const { return atEnd() ? WTF::Unicode::OtherNeutral : WTF::Unicode::direction(current()); }

    bool operator==(const TextRunIterator& other)
    {
        return m_offset == other.m_offset && m_textRun == other.m_textRun;
    }

    bool operator!=(const TextRunIterator& other) { return !operator==(other); }

private:
    const TextRun* m_textRun;
    int m_offset;
};

struct GraphicsContextState {
    GraphicsContextState() 
    : strokeStyle(SolidStroke)
    , strokeThickness(0)
    , strokeColor(Color::black)
    , fillColor(Color::black)
    , textDrawingMode(cTextFill)
    , paintingDisabled(false)
    {}
    
    Font font;
    StrokeStyle strokeStyle;
    float strokeThickness;
    Color strokeColor;
    Color fillColor;
    int textDrawingMode;
    bool paintingDisabled;
};
        
class GraphicsContextPrivate {
public:
    GraphicsContextPrivate();
    
    GraphicsContextState state;
    Vector<GraphicsContextState> stack;
    Vector<IntRect> m_focusRingRects;
    int m_focusRingWidth;
    int m_focusRingOffset;
    bool m_updatingControlTints;
};

GraphicsContextPrivate::GraphicsContextPrivate()
    : m_focusRingWidth(0)
    , m_focusRingOffset(0)
    , m_updatingControlTints(false)
{
}

GraphicsContextPrivate* GraphicsContext::createGraphicsContextPrivate()
{
    return new GraphicsContextPrivate;
}

void GraphicsContext::destroyGraphicsContextPrivate(GraphicsContextPrivate* deleteMe)
{
    delete deleteMe;
}

void GraphicsContext::save()
{
    if (paintingDisabled())
        return;

    m_common->stack.append(m_common->state);
    
    savePlatformState();
}

void GraphicsContext::restore()
{
    if (paintingDisabled())
        return;

    if (m_common->stack.isEmpty()) {
        LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }
    m_common->state = m_common->stack.last();
    m_common->stack.removeLast();
    
    restorePlatformState();
}

const Font& GraphicsContext::font() const
{
    return m_common->state.font;
}

void GraphicsContext::setFont(const Font& aFont)
{
    m_common->state.font = aFont;
    setPlatformFont(aFont);
}

void GraphicsContext::setStrokeThickness(float thickness)
{
    m_common->state.strokeThickness = thickness;
    setPlatformStrokeThickness(thickness);
}

void GraphicsContext::setStrokeStyle(const StrokeStyle& style)
{
    m_common->state.strokeStyle = style;
    setPlatformStrokeStyle(style);
}

void GraphicsContext::setStrokeColor(const Color& color)
{
    m_common->state.strokeColor = color;
    setPlatformStrokeColor(color);
}

float GraphicsContext::strokeThickness() const
{
    return m_common->state.strokeThickness;
}

StrokeStyle GraphicsContext::strokeStyle() const
{
    return m_common->state.strokeStyle;
}

Color GraphicsContext::strokeColor() const
{
    return m_common->state.strokeColor;
}

void GraphicsContext::setFillColor(const Color& color)
{
    m_common->state.fillColor = color;
    setPlatformFillColor(color);
}

Color GraphicsContext::fillColor() const
{
    return m_common->state.fillColor;
}

bool GraphicsContext::updatingControlTints() const
{
    return m_common->m_updatingControlTints;
}

void GraphicsContext::setUpdatingControlTints(bool b)
{
    setPaintingDisabled(b);
    m_common->m_updatingControlTints = b;
}

void GraphicsContext::setPaintingDisabled(bool f)
{
    m_common->state.paintingDisabled = f;
}

bool GraphicsContext::paintingDisabled() const
{
    return m_common->state.paintingDisabled;
}

void GraphicsContext::drawImage(Image* image, const IntPoint& p, CompositeOperator op)
{        
    drawImage(image, p, IntRect(0, 0, -1, -1), op);
}

void GraphicsContext::drawImage(Image* image, const IntRect& r, CompositeOperator op, bool useLowQualityScale)
{
    drawImage(image, r, IntRect(0, 0, -1, -1), op, useLowQualityScale);
}

void GraphicsContext::drawImage(Image* image, const IntPoint& dest, const IntRect& srcRect, CompositeOperator op)
{
    drawImage(image, IntRect(dest, srcRect.size()), srcRect, op);
}

void GraphicsContext::drawImage(Image* image, const IntRect& dest, const IntRect& srcRect, CompositeOperator op, bool useLowQualityScale)
{
    drawImage(image, FloatRect(dest), srcRect, op, useLowQualityScale);
}

void GraphicsContext::drawText(const TextRun& run, const IntPoint& point, int from, int to)
{
    if (paintingDisabled())
        return;
    
    font().drawText(this, run, point, from, to);
}

void GraphicsContext::drawBidiText(const TextRun& run, const IntPoint& point)
{
    if (paintingDisabled())
        return;

    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    WTF::Unicode::Direction paragraphDirection = run.ltr() ? WTF::Unicode::LeftToRight : WTF::Unicode::RightToLeft;

    bidiResolver.setStatus(BidiStatus(paragraphDirection, paragraphDirection, paragraphDirection, new BidiContext(run.ltr() ? 0 : 1, paragraphDirection, run.directionalOverride())));

    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, 0), TextRunIterator(&run, run.length()));

    if (!bidiResolver.runCount())
        return;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiResolver.firstRun();
    while (bidiRun) {

        TextRun subrun = run;
        subrun.setText(run.data(bidiRun->start()), bidiRun->stop() - bidiRun->start());
        subrun.setRTL(bidiRun->level() % 2);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        font().drawText(this, subrun, currPoint);

        bidiRun = bidiRun->next();
        // FIXME: Have Font::drawText return the width of what it drew so that we don't have to re-measure here.
        if (bidiRun)
            currPoint.move(font().floatWidth(subrun), 0.f);
    }

    bidiResolver.deleteRuns();
}

void GraphicsContext::drawHighlightForText(const TextRun& run, const IntPoint& point, int h, const Color& backgroundColor, int from, int to)
{
    if (paintingDisabled())
        return;

    fillRect(font().selectionRectForText(run, point, h, from, to), backgroundColor);
}

void GraphicsContext::initFocusRing(int width, int offset)
{
    if (paintingDisabled())
        return;
    clearFocusRing();
    
    m_common->m_focusRingWidth = width;
    m_common->m_focusRingOffset = offset;
}

void GraphicsContext::clearFocusRing()
{
    m_common->m_focusRingRects.clear();
}

IntRect GraphicsContext::focusRingBoundingRect()
{
    IntRect result = IntRect(0, 0, 0, 0);
    
    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();
    for (unsigned i = 0; i < rectCount; i++)
        result.unite(rects[i]);
        
    return result;
}

void GraphicsContext::addFocusRingRect(const IntRect& rect)
{
    if (paintingDisabled() || rect.isEmpty())
        return;
    m_common->m_focusRingRects.append(rect);
}

int GraphicsContext::focusRingWidth() const
{
    return m_common->m_focusRingWidth;
}

int GraphicsContext::focusRingOffset() const
{
    return m_common->m_focusRingOffset;
}

const Vector<IntRect>& GraphicsContext::focusRingRects() const
{
    return m_common->m_focusRingRects;
}

static const int cInterpolationCutoff = 800 * 800;

void GraphicsContext::drawImage(Image* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op, bool useLowQualityScale)
{
    if (paintingDisabled())
        return;

    float tsw = src.width();
    float tsh = src.height();
    float tw = dest.width();
    float th = dest.height();
        
    if (tsw == -1)
        tsw = image->width();
    if (tsh == -1)
        tsh = image->height();

    if (tw == -1)
        tw = image->width();
    if (th == -1)
        th = image->height();

    bool shouldUseLowQualityInterpolation = useLowQualityScale && (tsw != tw || tsh != th) && tsw * tsh > cInterpolationCutoff;
    if (shouldUseLowQualityInterpolation) {
        save();
        setUseLowQualityImageInterpolation(true);
    }
    image->draw(this, FloatRect(dest.location(), FloatSize(tw, th)), FloatRect(src.location(), FloatSize(tsw, tsh)), op);
    if (shouldUseLowQualityInterpolation)
        restore();
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& rect, const IntPoint& srcPoint, const IntSize& tileSize, CompositeOperator op)
{
    if (paintingDisabled())
        return;

    image->drawTiled(this, rect, srcPoint, tileSize, op);
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& dest, const IntRect& srcRect, Image::TileRule hRule, Image::TileRule vRule, CompositeOperator op)
{
    if (paintingDisabled())
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile)
        // Just do a scale.
        return drawImage(image, dest, srcRect);

    image->drawTiled(this, dest, srcRect, hRule, vRule, op);
}

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
    const IntSize& bottomLeft, const IntSize& bottomRight)
{
    if (paintingDisabled())
        return;

    clip(Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight));
}

void GraphicsContext::clipOutRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                         const IntSize& bottomLeft, const IntSize& bottomRight)
{
    if (paintingDisabled())
        return;

    clipOut(Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight));
}

int GraphicsContext::textDrawingMode()
{
    return m_common->state.textDrawingMode;
}

void GraphicsContext::setTextDrawingMode(int mode)
{
    m_common->state.textDrawingMode = mode;
    if (paintingDisabled())
        return;
    setPlatformTextDrawingMode(mode);
}

#if !PLATFORM(CG)
// Implement this if you want to go ahead and push the drawing mode into your native context
// immediately.
void GraphicsContext::setPlatformTextDrawingMode(int mode)
{
}
#endif

#if !PLATFORM(QT) && !PLATFORM(CAIRO)
void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle&)
{
}
#endif

#if !PLATFORM(QT)
void GraphicsContext::setPlatformFont(const Font&)
{
}
#endif

}
