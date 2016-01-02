/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContext.h"

#include "BidiResolver.h"
#include "BitmapImage.h"
#include "FloatRoundedRect.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "RoundedRect.h"
#include "TextRun.h"

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
    UCharDirection direction() const { return atEnd() ? U_OTHER_NEUTRAL : u_charDirection(current()); }

    bool operator==(const TextRunIterator& other)
    {
        return m_offset == other.m_offset && m_textRun == other.m_textRun;
    }

    bool operator!=(const TextRunIterator& other) { return !operator==(other); }

private:
    const TextRun* m_textRun;
    unsigned m_offset;
};

class InterpolationQualityMaintainer {
public:
    explicit InterpolationQualityMaintainer(GraphicsContext& graphicsContext, InterpolationQuality interpolationQualityToUse)
        : m_graphicsContext(graphicsContext)
        , m_currentInterpolationQuality(graphicsContext.imageInterpolationQuality())
        , m_interpolationQualityChanged(m_currentInterpolationQuality != interpolationQualityToUse)
    {
        if (m_interpolationQualityChanged)
            m_graphicsContext.setImageInterpolationQuality(interpolationQualityToUse);
    }

    ~InterpolationQualityMaintainer()
    {
        if (m_interpolationQualityChanged)
            m_graphicsContext.setImageInterpolationQuality(m_currentInterpolationQuality);
    }

private:
    GraphicsContext& m_graphicsContext;
    InterpolationQuality m_currentInterpolationQuality;
    bool m_interpolationQualityChanged;
};

GraphicsContext::GraphicsContext(PlatformGraphicsContext* platformGraphicsContext)
    : m_updatingControlTints(false)
    , m_transparencyCount(0)
{
    platformInit(platformGraphicsContext);
}

GraphicsContext::~GraphicsContext()
{
    ASSERT(m_stack.isEmpty());
    ASSERT(!m_transparencyCount);
    platformDestroy();
}

void GraphicsContext::save()
{
    if (paintingDisabled())
        return;

    m_stack.append(m_state);

    savePlatformState();
}

void GraphicsContext::restore()
{
    if (paintingDisabled())
        return;

    if (m_stack.isEmpty()) {
        LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }
    m_state = m_stack.last();
    m_stack.removeLast();

    // Make sure we deallocate the state stack buffer when it goes empty.
    // Canvas elements will immediately save() again, but that goes into inline capacity.
    if (m_stack.isEmpty())
        m_stack.clear();

    restorePlatformState();
}

void GraphicsContext::drawRaisedEllipse(const FloatRect& rect, const Color& ellipseColor, const Color& shadowColor)
{
    if (paintingDisabled())
        return;

    save();

    setStrokeColor(shadowColor);
    setFillColor(shadowColor);

    drawEllipse(FloatRect(rect.x(), rect.y() + 1, rect.width(), rect.height()));

    setStrokeColor(ellipseColor);
    setFillColor(ellipseColor);

    drawEllipse(rect);  

    restore();
}

void GraphicsContext::setStrokeThickness(float thickness)
{
    m_state.strokeThickness = thickness;
    setPlatformStrokeThickness(thickness);
}

void GraphicsContext::setStrokeStyle(StrokeStyle style)
{
    m_state.strokeStyle = style;
    setPlatformStrokeStyle(style);
}

void GraphicsContext::setStrokeColor(const Color& color)
{
    m_state.strokeColor = color;
    m_state.strokeGradient = nullptr;
    m_state.strokePattern = nullptr;
    setPlatformStrokeColor(color);
}

void GraphicsContext::setShadow(const FloatSize& offset, float blur, const Color& color)
{
    m_state.shadowOffset = offset;
    m_state.shadowBlur = blur;
    m_state.shadowColor = color;
    setPlatformShadow(offset, blur, color);
}

void GraphicsContext::setLegacyShadow(const FloatSize& offset, float blur, const Color& color)
{
    m_state.shadowOffset = offset;
    m_state.shadowBlur = blur;
    m_state.shadowColor = color;
#if USE(CG)
    m_state.shadowsUseLegacyRadius = true;
#endif
    setPlatformShadow(offset, blur, color);
}

void GraphicsContext::clearShadow()
{
    m_state.shadowOffset = FloatSize();
    m_state.shadowBlur = 0;
    m_state.shadowColor = Color();
    clearPlatformShadow();
}

bool GraphicsContext::getShadow(FloatSize& offset, float& blur, Color& color) const
{
    offset = m_state.shadowOffset;
    blur = m_state.shadowBlur;
    color = m_state.shadowColor;

    return hasShadow();
}

#if USE(CAIRO)
bool GraphicsContext::mustUseShadowBlur() const
{
    // We can't avoid ShadowBlur if the shadow has blur.
    if (hasBlurredShadow())
        return true;
    // We can avoid ShadowBlur and optimize, since we're not drawing on a
    // canvas and box shadows are affected by the transformation matrix.
    if (!m_state.shadowsIgnoreTransforms)
        return false;
    // We can avoid ShadowBlur, since there are no transformations to apply to the canvas.
    if (getCTM().isIdentity())
        return false;
    // Otherwise, no chance avoiding ShadowBlur.
    return true;
}
#endif

void GraphicsContext::setFillColor(const Color& color)
{
    m_state.fillColor = color;
    m_state.fillGradient = nullptr;
    m_state.fillPattern = nullptr;
    setPlatformFillColor(color);
}

void GraphicsContext::setShouldAntialias(bool shouldAntialias)
{
    m_state.shouldAntialias = shouldAntialias;
    setPlatformShouldAntialias(shouldAntialias);
}

void GraphicsContext::setShouldSmoothFonts(bool shouldSmoothFonts)
{
    m_state.shouldSmoothFonts = shouldSmoothFonts;
    setPlatformShouldSmoothFonts(shouldSmoothFonts);
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality imageInterpolationQuality)
{
    m_state.imageInterpolationQuality = imageInterpolationQuality;

    if (paintingDisabled())
        return;

    setPlatformImageInterpolationQuality(imageInterpolationQuality);
}

void GraphicsContext::setAntialiasedFontDilationEnabled(bool antialiasedFontDilationEnabled)
{
    m_state.antialiasedFontDilationEnabled = antialiasedFontDilationEnabled;
}

void GraphicsContext::setStrokePattern(Ref<Pattern>&& pattern)
{
    m_state.strokeGradient = nullptr;
    m_state.strokePattern = WTFMove(pattern);
}

void GraphicsContext::setFillPattern(Ref<Pattern>&& pattern)
{
    m_state.fillGradient = nullptr;
    m_state.fillPattern = WTFMove(pattern);
}

void GraphicsContext::setStrokeGradient(Ref<Gradient>&& gradient)
{
    m_state.strokeGradient = WTFMove(gradient);
    m_state.strokePattern = nullptr;
}

void GraphicsContext::setFillGradient(Ref<Gradient>&& gradient)
{
    m_state.fillGradient = WTFMove(gradient);
    m_state.fillPattern = nullptr;
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    beginPlatformTransparencyLayer(opacity);
    ++m_transparencyCount;
}

void GraphicsContext::endTransparencyLayer()
{
    endPlatformTransparencyLayer();
    ASSERT(m_transparencyCount > 0);
    --m_transparencyCount;
}

void GraphicsContext::setUpdatingControlTints(bool b)
{
    setPaintingDisabled(b);
    m_updatingControlTints = b;
}

float GraphicsContext::drawText(const FontCascade& font, const TextRun& run, const FloatPoint& point, int from, int to)
{
    if (paintingDisabled())
        return 0;

    return font.drawText(*this, run, point, from, to);
}

void GraphicsContext::drawGlyphs(const FontCascade& fontCascade, const Font& font, const GlyphBuffer& buffer, int from, int numGlyphs, const FloatPoint& point)
{
    if (paintingDisabled())
        return;

    fontCascade.drawGlyphs(*this, font, buffer, from, numGlyphs, point);
}

void GraphicsContext::drawEmphasisMarks(const FontCascade& font, const TextRun& run, const AtomicString& mark, const FloatPoint& point, int from, int to)
{
    if (paintingDisabled())
        return;

    font.drawEmphasisMarks(*this, run, mark, point, from, to);
}

void GraphicsContext::drawBidiText(const FontCascade& font, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
    if (paintingDisabled())
        return;

    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(run.direction(), run.directionalOverride()));
    bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

    // FIXME: This ownership should be reversed. We should pass BidiRunList
    // to BidiResolver in createBidiRunsForLine.
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));

    if (!bidiRuns.runCount())
        return;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
    while (bidiRun) {
        TextRun subrun = run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
        bool isRTL = bidiRun->level() % 2;
        subrun.setDirection(isRTL ? RTL : LTR);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        float width = font.drawText(*this, subrun, currPoint, 0, -1, customFontNotReadyAction);
        currPoint.move(width, 0);

        bidiRun = bidiRun->next();
    }

    bidiRuns.deleteRuns();
}

void GraphicsContext::drawImage(Image& image, const FloatPoint& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    drawImage(image, FloatRect(destination, image.size()), FloatRect(FloatPoint(), image.size()), imagePaintingOptions);
}

void GraphicsContext::drawImage(Image& image, const FloatRect& destination, const ImagePaintingOptions& imagePaintingOptions)
{
#if PLATFORM(IOS)
    FloatRect srcRect(FloatPoint(), image.originalSize());
#else
    FloatRect srcRect(FloatPoint(), image.size());
#endif
        
    drawImage(image, destination, srcRect, imagePaintingOptions);
}

void GraphicsContext::drawImage(Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& imagePaintingOptions)
{
    if (paintingDisabled())
        return;

    // FIXME (49002): Should be InterpolationLow
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, imagePaintingOptions.m_useLowQualityScale ? InterpolationNone : imageInterpolationQuality());
    image.draw(*this, destination, source, imagePaintingOptions.m_compositeOperator, imagePaintingOptions.m_blendMode, imagePaintingOptions.m_orientationDescription);
}

void GraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& imagePaintingOptions)
{
    if (paintingDisabled())
        return;

    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, imagePaintingOptions.m_useLowQualityScale ? InterpolationLow : imageInterpolationQuality());
    image.drawTiled(*this, destination, source, tileSize, spacing, imagePaintingOptions.m_compositeOperator, imagePaintingOptions.m_blendMode);
}

void GraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor,
    Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& imagePaintingOptions)
{
    if (paintingDisabled())
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        drawImage(image, destination, source, imagePaintingOptions);
        return;
    }

    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, imagePaintingOptions.m_useLowQualityScale ? InterpolationLow : imageInterpolationQuality());
    image.drawTiled(*this, destination, source, tileScaleFactor, hRule, vRule, imagePaintingOptions.m_compositeOperator);
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatPoint& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    drawImageBuffer(image, FloatRect(destination, image.logicalSize()), FloatRect(FloatPoint(), image.logicalSize()), imagePaintingOptions);
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatRect& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    drawImageBuffer(image, destination, FloatRect(FloatPoint(), FloatSize(image.logicalSize())), imagePaintingOptions);
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& imagePaintingOptions)
{
    if (paintingDisabled())
        return;

    // FIXME (49002): Should be InterpolationLow
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, imagePaintingOptions.m_useLowQualityScale ? InterpolationNone : imageInterpolationQuality());
    image.draw(*this, destination, source, imagePaintingOptions.m_compositeOperator, imagePaintingOptions.m_blendMode, imagePaintingOptions.m_useLowQualityScale);
}

void GraphicsContext::drawConsumingImageBuffer(std::unique_ptr<ImageBuffer> image, const FloatPoint& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    if (!image)
        return;
    IntSize imageLogicalSize = image->logicalSize();
    drawConsumingImageBuffer(WTFMove(image), FloatRect(destination, imageLogicalSize), FloatRect(FloatPoint(), imageLogicalSize), imagePaintingOptions);
}

void GraphicsContext::drawConsumingImageBuffer(std::unique_ptr<ImageBuffer> image, const FloatRect& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    if (!image)
        return;
    IntSize imageLogicalSize = image->logicalSize();
    drawConsumingImageBuffer(WTFMove(image), destination, FloatRect(FloatPoint(), FloatSize(imageLogicalSize)), imagePaintingOptions);
}

void GraphicsContext::drawConsumingImageBuffer(std::unique_ptr<ImageBuffer> image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& imagePaintingOptions)
{
    if (paintingDisabled() || !image)
        return;
    
    // FIXME (49002): Should be InterpolationLow
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, imagePaintingOptions.m_useLowQualityScale ? InterpolationNone : imageInterpolationQuality());

    ImageBuffer::drawConsuming(WTFMove(image), *this, destination, source, imagePaintingOptions.m_compositeOperator, imagePaintingOptions.m_blendMode, imagePaintingOptions.m_useLowQualityScale);
}

void GraphicsContext::clip(const IntRect& rect)
{
    clip(FloatRect(rect));
}

void GraphicsContext::clipRoundedRect(const FloatRoundedRect& rect)
{
    if (paintingDisabled())
        return;

    Path path;
    path.addRoundedRect(rect);
    clipPath(path);
}

void GraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    if (paintingDisabled())
        return;

    if (!rect.isRounded()) {
        clipOut(rect.rect());
        return;
    }

    Path path;
    path.addRoundedRect(rect);
    clipOut(path);
}

void GraphicsContext::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    buffer.clip(*this, rect);
}

#if !USE(CG) && !USE(CAIRO)
IntRect GraphicsContext::clipBounds() const
{
    ASSERT_NOT_REACHED();
    return IntRect();
}
#endif

void GraphicsContext::setTextDrawingMode(TextDrawingModeFlags mode)
{
    m_state.textDrawingMode = mode;
    if (paintingDisabled())
        return;
    setPlatformTextDrawingMode(mode);
}

void GraphicsContext::fillRect(const FloatRect& rect, Gradient& gradient)
{
    if (paintingDisabled())
        return;
    gradient.fill(this, rect);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    if (paintingDisabled())
        return;

    CompositeOperator previousOperator = compositeOperation();
    setCompositeOperation(op, blendMode);
    fillRect(rect, color);
    setCompositeOperation(previousOperator);
}

void GraphicsContext::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    if (rect.isRounded()) {
        setCompositeOperation(compositeOperation(), blendMode);
        platformFillRoundedRect(rect, color);
        setCompositeOperation(compositeOperation());
    } else
        fillRect(rect.rect(), color, compositeOperation(), blendMode);
}

#if !USE(CG) && !USE(CAIRO)
void GraphicsContext::fillRectWithRoundedHole(const IntRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (paintingDisabled())
        return;

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();
    
    setFillRule(RULE_EVENODD);
    setFillColor(color);

    fillPath(path);
    
    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}
#endif

void GraphicsContext::setAlpha(float alpha)
{
    m_state.alpha = alpha;
    setPlatformAlpha(alpha);
}

void GraphicsContext::setCompositeOperation(CompositeOperator compositeOperation, BlendMode blendMode)
{
    m_state.compositeOperator = compositeOperation;
    m_state.blendMode = blendMode;
    setPlatformCompositeOperation(compositeOperation, blendMode);
}

#if !USE(CG)
// Implement this if you want to go push the drawing mode into your native context immediately.
void GraphicsContext::setPlatformTextDrawingMode(TextDrawingModeFlags)
{
}
#endif

#if !USE(CAIRO)
void GraphicsContext::setPlatformStrokeStyle(StrokeStyle)
{
}
#endif

#if !USE(CG)
void GraphicsContext::setPlatformShouldSmoothFonts(bool)
{
}
#endif

#if !USE(CG) && !USE(CAIRO)
bool GraphicsContext::isAcceleratedContext() const
{
    return false;
}
#endif

void GraphicsContext::adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, WebKit will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DottedStroke || penStyle == DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (static_cast<int>(strokeWidth) % 2) { //odd
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5f);
            p2.setX(p2.x() + 0.5f);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5f);
            p2.setY(p2.y() + 0.5f);
        }
    }
}

static bool scalesMatch(AffineTransform a, AffineTransform b)
{
    return a.xScale() == b.xScale() && a.yScale() == b.yScale();
}

std::unique_ptr<ImageBuffer> GraphicsContext::createCompatibleBuffer(const FloatSize& size, bool hasAlpha) const
{
    // Make the buffer larger if the context's transform is scaling it so we need a higher
    // resolution than one pixel per unit. Also set up a corresponding scale factor on the
    // graphics context.

    AffineTransform transform = getCTM(DefinitelyIncludeDeviceScale);
    FloatSize scaledSize(static_cast<int>(ceil(size.width() * transform.xScale())), static_cast<int>(ceil(size.height() * transform.yScale())));

    std::unique_ptr<ImageBuffer> buffer = ImageBuffer::createCompatibleBuffer(scaledSize, 1, ColorSpaceSRGB, *this, hasAlpha);
    if (!buffer)
        return nullptr;

    buffer->context().scale(FloatSize(scaledSize.width() / size.width(), scaledSize.height() / size.height()));

    return buffer;
}

bool GraphicsContext::isCompatibleWithBuffer(ImageBuffer& buffer) const
{
    GraphicsContext& bufferContext = buffer.context();

    return scalesMatch(getCTM(), bufferContext.getCTM()) && isAcceleratedContext() == bufferContext.isAcceleratedContext();
}

#if !USE(CG)
void GraphicsContext::platformApplyDeviceScaleFactor(float)
{
}
#endif

void GraphicsContext::applyDeviceScaleFactor(float deviceScaleFactor)
{
    scale(FloatSize(deviceScaleFactor, deviceScaleFactor));
    platformApplyDeviceScaleFactor(deviceScaleFactor);
}

void GraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    platformFillEllipse(ellipse);
}

void GraphicsContext::strokeEllipse(const FloatRect& ellipse)
{
    platformStrokeEllipse(ellipse);
}

void GraphicsContext::fillEllipseAsPath(const FloatRect& ellipse)
{
    Path path;
    path.addEllipse(ellipse);
    fillPath(path);
}

void GraphicsContext::strokeEllipseAsPath(const FloatRect& ellipse)
{
    Path path;
    path.addEllipse(ellipse);
    strokePath(path);
}

#if !USE(CG)
void GraphicsContext::platformFillEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    fillEllipseAsPath(ellipse);
}

void GraphicsContext::platformStrokeEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    strokeEllipseAsPath(ellipse);
}
#endif

FloatRect GraphicsContext::computeLineBoundsAndAntialiasingModeForText(const FloatPoint& point, float width, bool printing, bool& shouldAntialias, Color& color)
{
    FloatPoint origin = point;
    float thickness = std::max(strokeThickness(), 0.5f);

    shouldAntialias = true;
    if (!printing) {
        AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
        if (transform.preservesAxisAlignment())
            shouldAntialias = false;

        // This code always draws a line that is at least one-pixel line high,
        // which tends to visually overwhelm text at small scales. To counter this
        // effect, an alpha is applied to the underline color when text is at small scales.

        // Just compute scale in x dimension, assuming x and y scales are equal.
        float scale = transform.b() ? sqrtf(transform.a() * transform.a() + transform.b() * transform.b()) : transform.a();
        if (scale < 1.0) {
            static const float minimumUnderlineAlpha = 0.4f;
            float shade = scale > minimumUnderlineAlpha ? scale : minimumUnderlineAlpha;
            int alpha = color.alpha() * shade;
            color = Color(color.red(), color.green(), color.blue(), alpha);
        }

        FloatPoint devicePoint = transform.mapPoint(point);
        FloatPoint deviceOrigin = FloatPoint(roundf(devicePoint.x()), ceilf(devicePoint.y()));
        if (auto inverse = transform.inverse())
            origin = inverse.value().mapPoint(deviceOrigin);
    }
    return FloatRect(origin.x(), origin.y(), width, thickness);
}

}
