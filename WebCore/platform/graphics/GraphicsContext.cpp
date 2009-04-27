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
#include "Generator.h"
#include "GraphicsContextPrivate.h"
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
    void increment() { m_offset++; }
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
    m_common->state.strokeColorSpace = SolidColorSpace;
    m_common->state.strokeColor = color;
    setPlatformStrokeColor(color);
}

ColorSpace GraphicsContext::strokeColorSpace() const
{
    return m_common->state.strokeColorSpace;
}

void GraphicsContext::setShadow(const IntSize& size, int blur, const Color& color)
{
    m_common->state.shadowSize = size;
    m_common->state.shadowBlur = blur;
    m_common->state.shadowColor = color;
    setPlatformShadow(size, blur, color);
}

void GraphicsContext::clearShadow()
{
    m_common->state.shadowSize = IntSize();
    m_common->state.shadowBlur = 0;
    m_common->state.shadowColor = Color();
    clearPlatformShadow();
}

bool GraphicsContext::getShadow(IntSize& size, int& blur, Color& color) const
{
    size = m_common->state.shadowSize;
    blur = m_common->state.shadowBlur;
    color = m_common->state.shadowColor;

    return color.isValid() && color.alpha() && (blur || size.width() || size.height());
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

WindRule GraphicsContext::fillRule() const
{
    return m_common->state.fillRule;
}

void GraphicsContext::setFillRule(WindRule fillRule)
{
    m_common->state.fillRule = fillRule;
}

void GraphicsContext::setFillColor(const Color& color)
{
    m_common->state.fillColorSpace = SolidColorSpace;
    m_common->state.fillColor = color;
    setPlatformFillColor(color);
}

Color GraphicsContext::fillColor() const
{
    return m_common->state.fillColor;
}

void GraphicsContext::setShouldAntialias(bool b)
{
    m_common->state.shouldAntialias = b;
    setPlatformShouldAntialias(b);
}

bool GraphicsContext::shouldAntialias() const
{
    return m_common->state.shouldAntialias;
}

void GraphicsContext::setStrokePattern(PassRefPtr<Pattern> pattern)
{
    ASSERT(pattern);
    if (!pattern) {
        setStrokeColor(Color::black);
        return;
    }
    m_common->state.strokeColorSpace = PatternColorSpace;
    m_common->state.strokePattern = pattern;
}

void GraphicsContext::setFillPattern(PassRefPtr<Pattern> pattern)
{
    ASSERT(pattern);
    if (!pattern) {
        setFillColor(Color::black);
        return;
    }
    m_common->state.fillColorSpace = PatternColorSpace;
    m_common->state.fillPattern = pattern;
}

void GraphicsContext::setStrokeGradient(PassRefPtr<Gradient> gradient)
{
    ASSERT(gradient);
    if (!gradient) {
        setStrokeColor(Color::black);
        return;
    }
    m_common->state.strokeColorSpace = GradientColorSpace;
    m_common->state.strokeGradient = gradient;
}

void GraphicsContext::setFillGradient(PassRefPtr<Gradient> gradient)
{
    ASSERT(gradient);
    if (!gradient) {
        setFillColor(Color::black);
        return;
    }
    m_common->state.fillColorSpace = GradientColorSpace;
    m_common->state.fillGradient = gradient;
}

Gradient* GraphicsContext::fillGradient() const
{
    return m_common->state.fillGradient.get();
}

ColorSpace GraphicsContext::fillColorSpace() const
{
    return m_common->state.fillColorSpace;
}

Gradient* GraphicsContext::strokeGradient() const
{
    return m_common->state.strokeGradient.get();
}

Pattern* GraphicsContext::fillPattern() const
{
    return m_common->state.fillPattern.get();
}

Pattern* GraphicsContext::strokePattern() const
{
    return m_common->state.strokePattern.get();
}

void GraphicsContext::setShadowsIgnoreTransforms(bool ignoreTransforms)
{
    m_common->state.shadowsIgnoreTransforms = ignoreTransforms;
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

void GraphicsContext::drawText(const Font& font, const TextRun& run, const IntPoint& point, int from, int to)
{
    if (paintingDisabled())
        return;
    
    font.drawText(this, run, point, from, to);
}

void GraphicsContext::drawBidiText(const Font& font, const TextRun& run, const FloatPoint& point)
{
    if (paintingDisabled())
        return;

    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    WTF::Unicode::Direction paragraphDirection = run.ltr() ? WTF::Unicode::LeftToRight : WTF::Unicode::RightToLeft;

    bidiResolver.setStatus(BidiStatus(paragraphDirection, paragraphDirection, paragraphDirection, BidiContext::create(run.ltr() ? 0 : 1, paragraphDirection, run.directionalOverride())));

    bidiResolver.setPosition(TextRunIterator(&run, 0));
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));

    if (!bidiResolver.runCount())
        return;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiResolver.firstRun();
    while (bidiRun) {

        TextRun subrun = run;
        subrun.setText(run.data(bidiRun->start()), bidiRun->stop() - bidiRun->start());
        subrun.setRTL(bidiRun->level() % 2);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        font.drawText(this, subrun, currPoint);

        bidiRun = bidiRun->next();
        // FIXME: Have Font::drawText return the width of what it drew so that we don't have to re-measure here.
        if (bidiRun)
            currPoint.move(font.floatWidth(subrun), 0.f);
    }

    bidiResolver.deleteRuns();
}

void GraphicsContext::drawHighlightForText(const Font& font, const TextRun& run, const IntPoint& point, int h, const Color& backgroundColor, int from, int to)
{
    if (paintingDisabled())
        return;

    fillRect(font.selectionRectForText(run, point, h, from, to), backgroundColor);
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

void GraphicsContext::drawImage(Image* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op, bool useLowQualityScale)
{
    if (paintingDisabled() || !image)
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

    if (useLowQualityScale) {
        save();
        setImageInterpolationQuality(InterpolationNone);
    }
    image->draw(this, FloatRect(dest.location(), FloatSize(tw, th)), FloatRect(src.location(), FloatSize(tsw, tsh)), op);
    if (useLowQualityScale)
        restore();
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& rect, const IntPoint& srcPoint, const IntSize& tileSize, CompositeOperator op)
{
    if (paintingDisabled() || !image)
        return;

    image->drawTiled(this, rect, srcPoint, tileSize, op);
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& dest, const IntRect& srcRect, Image::TileRule hRule, Image::TileRule vRule, CompositeOperator op)
{
    if (paintingDisabled() || !image)
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile)
        // Just do a scale.
        return drawImage(image, dest, srcRect, op);

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

void GraphicsContext::fillRect(const FloatRect& rect, Generator& generator)
{
    if (paintingDisabled())
        return;
    generator.fill(this, rect);
}

#if !PLATFORM(CG) && !PLATFORM(SKIA)
// Implement this if you want to go ahead and push the drawing mode into your native context
// immediately.
void GraphicsContext::setPlatformTextDrawingMode(int mode)
{
}
#endif

#if !PLATFORM(QT) && !PLATFORM(CAIRO) && !PLATFORM(SKIA)
void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle&)
{
}
#endif

}
