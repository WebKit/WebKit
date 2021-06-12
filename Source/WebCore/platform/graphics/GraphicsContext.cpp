/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
#include "MediaPlayer.h"
#include "MediaPlayerPrivate.h"
#include "NullGraphicsContext.h"
#include "RoundedRect.h"
#include "TextRun.h"
#include <wtf/text/TextStream.h>

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

#define CHECK_FOR_CHANGED_PROPERTY(flag, property) \
    if (m_changeFlags.contains(GraphicsContextState::flag) && (m_state.property != state.property)) \
        changeFlags.add(GraphicsContextState::flag);

GraphicsContextState::GraphicsContextState()
    : shouldAntialias(true)
    , shouldSmoothFonts(true)
    , shouldSubpixelQuantizeFonts(true)
    , shadowsIgnoreTransforms(false)
    , drawLuminanceMask(false)
{
}

GraphicsContextState::~GraphicsContextState() = default;
GraphicsContextState::GraphicsContextState(const GraphicsContextState&) = default;
GraphicsContextState::GraphicsContextState(GraphicsContextState&&) = default;
GraphicsContextState& GraphicsContextState::operator=(const GraphicsContextState&) = default;
GraphicsContextState& GraphicsContextState::operator=(GraphicsContextState&&) = default;

GraphicsContextState::StateChangeFlags GraphicsContextStateChange::changesFromState(const GraphicsContextState& state) const
{
    GraphicsContextState::StateChangeFlags changeFlags;

    CHECK_FOR_CHANGED_PROPERTY(StrokeGradientChange, strokeGradient);
    CHECK_FOR_CHANGED_PROPERTY(StrokePatternChange, strokePattern);
    CHECK_FOR_CHANGED_PROPERTY(FillGradientChange, fillGradient);
    CHECK_FOR_CHANGED_PROPERTY(FillPatternChange, fillPattern);

    if (m_changeFlags.contains(GraphicsContextState::ShadowChange)
        && (m_state.shadowOffset != state.shadowOffset
            || m_state.shadowBlur != state.shadowBlur
            || m_state.shadowColor != state.shadowColor))
        changeFlags.add(GraphicsContextState::ShadowChange);

    CHECK_FOR_CHANGED_PROPERTY(StrokeThicknessChange, strokeThickness);
    CHECK_FOR_CHANGED_PROPERTY(TextDrawingModeChange, textDrawingMode);
    CHECK_FOR_CHANGED_PROPERTY(StrokeColorChange, strokeColor);
    CHECK_FOR_CHANGED_PROPERTY(FillColorChange, fillColor);
    CHECK_FOR_CHANGED_PROPERTY(StrokeStyleChange, strokeStyle);
    CHECK_FOR_CHANGED_PROPERTY(FillRuleChange, fillRule);
    CHECK_FOR_CHANGED_PROPERTY(AlphaChange, alpha);

    if (m_changeFlags.containsAny({ GraphicsContextState::CompositeOperationChange, GraphicsContextState::BlendModeChange })
        && (m_state.compositeOperator != state.compositeOperator || m_state.blendMode != state.blendMode)) {
        changeFlags.add(GraphicsContextState::CompositeOperationChange);
        changeFlags.add(GraphicsContextState::BlendModeChange);
    }

    CHECK_FOR_CHANGED_PROPERTY(ShouldAntialiasChange, shouldAntialias);
    CHECK_FOR_CHANGED_PROPERTY(ShouldSmoothFontsChange, shouldSmoothFonts);
    CHECK_FOR_CHANGED_PROPERTY(ShouldSubpixelQuantizeFontsChange, shouldSubpixelQuantizeFonts);
    CHECK_FOR_CHANGED_PROPERTY(ShadowsIgnoreTransformsChange, shadowsIgnoreTransforms);
    CHECK_FOR_CHANGED_PROPERTY(DrawLuminanceMaskChange, drawLuminanceMask);
    CHECK_FOR_CHANGED_PROPERTY(ImageInterpolationQualityChange, imageInterpolationQuality);

#if HAVE(OS_DARK_MODE_SUPPORT)
    CHECK_FOR_CHANGED_PROPERTY(UseDarkAppearanceChange, useDarkAppearance);
#endif

    return changeFlags;
}

void GraphicsContextStateChange::accumulate(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    // FIXME: This code should move to GraphicsContextState.
    auto strokeFlags = { GraphicsContextState::StrokeColorChange, GraphicsContextState::StrokeGradientChange, GraphicsContextState::StrokePatternChange };
    if (flags.containsAny(strokeFlags)) {
        m_state.strokeColor = state.strokeColor;
        m_state.strokeGradient = state.strokeGradient;
        m_state.strokePattern = state.strokePattern;
        m_changeFlags.remove(strokeFlags);
    }

    auto fillFlags = { GraphicsContextState::FillColorChange, GraphicsContextState::FillGradientChange, GraphicsContextState::FillPatternChange };
    if (flags.containsAny(fillFlags)) {
        m_state.fillColor = state.fillColor;
        m_state.fillGradient = state.fillGradient;
        m_state.fillPattern = state.fillPattern;
        m_changeFlags.remove(fillFlags);
    }

    if (flags.contains(GraphicsContextState::ShadowChange)) {
        m_state.shadowOffset = state.shadowOffset;
        m_state.shadowBlur = state.shadowBlur;
        m_state.shadowColor = state.shadowColor;
        m_state.shadowRadiusMode = state.shadowRadiusMode;
    }

    if (flags.contains(GraphicsContextState::StrokeThicknessChange))
        m_state.strokeThickness = state.strokeThickness;

    if (flags.contains(GraphicsContextState::TextDrawingModeChange))
        m_state.textDrawingMode = state.textDrawingMode;

    if (flags.contains(GraphicsContextState::StrokeStyleChange))
        m_state.strokeStyle = state.strokeStyle;

    if (flags.contains(GraphicsContextState::FillRuleChange))
        m_state.fillRule = state.fillRule;

    if (flags.contains(GraphicsContextState::AlphaChange))
        m_state.alpha = state.alpha;

    if (flags.containsAny({ GraphicsContextState::CompositeOperationChange, GraphicsContextState::BlendModeChange })) {
        m_state.compositeOperator = state.compositeOperator;
        m_state.blendMode = state.blendMode;
    }

    if (flags.contains(GraphicsContextState::ShouldAntialiasChange))
        m_state.shouldAntialias = state.shouldAntialias;

    if (flags.contains(GraphicsContextState::ShouldSmoothFontsChange))
        m_state.shouldSmoothFonts = state.shouldSmoothFonts;

    if (flags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange))
        m_state.shouldSubpixelQuantizeFonts = state.shouldSubpixelQuantizeFonts;

    if (flags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange))
        m_state.shadowsIgnoreTransforms = state.shadowsIgnoreTransforms;

    if (flags.contains(GraphicsContextState::DrawLuminanceMaskChange))
        m_state.drawLuminanceMask = state.drawLuminanceMask;

    if (flags.contains(GraphicsContextState::ImageInterpolationQualityChange))
        m_state.imageInterpolationQuality = state.imageInterpolationQuality;

#if HAVE(OS_DARK_MODE_SUPPORT)
    if (flags.contains(GraphicsContextState::UseDarkAppearanceChange))
        m_state.useDarkAppearance = state.useDarkAppearance;
#endif

    m_changeFlags.add(flags);
}

void GraphicsContextStateChange::apply(GraphicsContext& context) const
{
    if (m_changeFlags.contains(GraphicsContextState::StrokeGradientChange))
        context.setStrokeGradient(*m_state.strokeGradient, m_state.strokeGradientSpaceTransform);

    if (m_changeFlags.contains(GraphicsContextState::StrokePatternChange))
        context.setStrokePattern(*m_state.strokePattern);

    if (m_changeFlags.contains(GraphicsContextState::FillGradientChange))
        context.setFillGradient(*m_state.fillGradient, m_state.fillGradientSpaceTransform);

    if (m_changeFlags.contains(GraphicsContextState::FillPatternChange))
        context.setFillPattern(*m_state.fillPattern);

    if (m_changeFlags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange))
        context.setShadowsIgnoreTransforms(m_state.shadowsIgnoreTransforms);

    if (m_changeFlags.contains(GraphicsContextState::ShadowChange))
        context.setShadow(m_state.shadowOffset, m_state.shadowBlur, m_state.shadowColor, m_state.shadowRadiusMode);

    if (m_changeFlags.contains(GraphicsContextState::StrokeThicknessChange))
        context.setStrokeThickness(m_state.strokeThickness);

    if (m_changeFlags.contains(GraphicsContextState::TextDrawingModeChange))
        context.setTextDrawingMode(m_state.textDrawingMode);

    if (m_changeFlags.contains(GraphicsContextState::StrokeColorChange))
        context.setStrokeColor(m_state.strokeColor);

    if (m_changeFlags.contains(GraphicsContextState::FillColorChange))
        context.setFillColor(m_state.fillColor);

    if (m_changeFlags.contains(GraphicsContextState::StrokeStyleChange))
        context.setStrokeStyle(m_state.strokeStyle);

    if (m_changeFlags.contains(GraphicsContextState::FillRuleChange))
        context.setFillRule(m_state.fillRule);

    if (m_changeFlags.contains(GraphicsContextState::AlphaChange))
        context.setAlpha(m_state.alpha);

    if (m_changeFlags.containsAny({ GraphicsContextState::CompositeOperationChange, GraphicsContextState::BlendModeChange }))
        context.setCompositeOperation(m_state.compositeOperator, m_state.blendMode);

    if (m_changeFlags.contains(GraphicsContextState::ShouldAntialiasChange))
        context.setShouldAntialias(m_state.shouldAntialias);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSmoothFontsChange))
        context.setShouldSmoothFonts(m_state.shouldSmoothFonts);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange))
        context.setShouldSubpixelQuantizeFonts(m_state.shouldSubpixelQuantizeFonts);

    if (m_changeFlags.contains(GraphicsContextState::DrawLuminanceMaskChange))
        context.setDrawLuminanceMask(m_state.drawLuminanceMask);

    if (m_changeFlags.contains(GraphicsContextState::ImageInterpolationQualityChange))
        context.setImageInterpolationQuality(m_state.imageInterpolationQuality);

#if HAVE(OS_DARK_MODE_SUPPORT)
    if (m_changeFlags.contains(GraphicsContextState::UseDarkAppearanceChange))
        context.setUseDarkAppearance(m_state.useDarkAppearance);
#endif
}

void GraphicsContextStateChange::dump(TextStream& ts) const
{
    ts.dumpProperty("change-flags", m_changeFlags.toRaw());

    if (m_changeFlags.contains(GraphicsContextState::StrokeGradientChange))
        ts.dumpProperty("stroke-gradient", m_state.strokeGradient.get());

    if (m_changeFlags.contains(GraphicsContextState::StrokePatternChange))
        ts.dumpProperty("stroke-pattern", m_state.strokePattern.get());

    if (m_changeFlags.contains(GraphicsContextState::FillGradientChange))
        ts.dumpProperty("fill-gradient", m_state.fillGradient.get());

    if (m_changeFlags.contains(GraphicsContextState::FillPatternChange))
        ts.dumpProperty("fill-pattern", m_state.fillPattern.get());

    if (m_changeFlags.contains(GraphicsContextState::ShadowChange)) {
        ts.dumpProperty("shadow-blur", m_state.shadowBlur);
        ts.dumpProperty("shadow-offset", m_state.shadowOffset);
        ts.dumpProperty("shadows-use-legacy-radius", m_state.shadowRadiusMode == ShadowRadiusMode::Legacy);
    }

    if (m_changeFlags.contains(GraphicsContextState::StrokeThicknessChange))
        ts.dumpProperty("stroke-thickness", m_state.strokeThickness);

    if (m_changeFlags.contains(GraphicsContextState::TextDrawingModeChange))
        ts.dumpProperty("text-drawing-mode", m_state.textDrawingMode.toRaw());

    if (m_changeFlags.contains(GraphicsContextState::StrokeColorChange))
        ts.dumpProperty("stroke-color", m_state.strokeColor);

    if (m_changeFlags.contains(GraphicsContextState::FillColorChange))
        ts.dumpProperty("fill-color", m_state.fillColor);

    if (m_changeFlags.contains(GraphicsContextState::StrokeStyleChange))
        ts.dumpProperty("stroke-style", m_state.strokeStyle);

    if (m_changeFlags.contains(GraphicsContextState::FillRuleChange))
        ts.dumpProperty("fill-rule", m_state.fillRule);

    if (m_changeFlags.contains(GraphicsContextState::AlphaChange))
        ts.dumpProperty("alpha", m_state.alpha);

    if (m_changeFlags.contains(GraphicsContextState::CompositeOperationChange))
        ts.dumpProperty("composite-operator", m_state.compositeOperator);

    if (m_changeFlags.contains(GraphicsContextState::BlendModeChange))
        ts.dumpProperty("blend-mode", m_state.blendMode);

    if (m_changeFlags.contains(GraphicsContextState::ShouldAntialiasChange))
        ts.dumpProperty("should-antialias", m_state.shouldAntialias);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSmoothFontsChange))
        ts.dumpProperty("should-smooth-fonts", m_state.shouldSmoothFonts);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange))
        ts.dumpProperty("should-subpixel-quantize-fonts", m_state.shouldSubpixelQuantizeFonts);

    if (m_changeFlags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange))
        ts.dumpProperty("shadows-ignore-transforms", m_state.shadowsIgnoreTransforms);

    if (m_changeFlags.contains(GraphicsContextState::DrawLuminanceMaskChange))
        ts.dumpProperty("draw-luminance-mask", m_state.drawLuminanceMask);

#if HAVE(OS_DARK_MODE_SUPPORT)
    if (m_changeFlags.contains(GraphicsContextState::UseDarkAppearanceChange))
        ts.dumpProperty("use-dark-appearance", m_state.useDarkAppearance);
#endif
}

TextStream& operator<<(TextStream& ts, const GraphicsContextStateChange& stateChange)
{
    stateChange.dump(ts);
    return ts;
}

GraphicsContext::~GraphicsContext()
{
    ASSERT(m_stack.isEmpty());
    ASSERT(!m_transparencyLayerCount);
}

void GraphicsContext::save()
{
    m_stack.append(m_state);
}

void GraphicsContext::restore()
{
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
}

void GraphicsContext::drawRaisedEllipse(const FloatRect& rect, const Color& ellipseColor, const Color& shadowColor)
{
    save();

    setStrokeColor(shadowColor);
    setFillColor(shadowColor);

    drawEllipse(FloatRect(rect.x(), rect.y() + 1, rect.width(), rect.height()));

    setStrokeColor(ellipseColor);
    setFillColor(ellipseColor);

    drawEllipse(rect);  

    restore();
}

void GraphicsContext::setStrokeColor(const Color& color)
{
    m_state.strokeColor = color;
    m_state.strokeGradient = nullptr;
    m_state.strokePattern = nullptr;
    updateState(m_state, GraphicsContextState::StrokeColorChange);
}

void GraphicsContext::setShadow(const FloatSize& offset, float blur, const Color& color, ShadowRadiusMode radiusMode)
{
    m_state.shadowOffset = offset;
    m_state.shadowBlur = blur;
    m_state.shadowColor = color;
    m_state.shadowRadiusMode = radiusMode;
    updateState(m_state, GraphicsContextState::ShadowChange);
}

void GraphicsContext::clearShadow()
{
    m_state.shadowOffset = FloatSize();
    m_state.shadowBlur = 0;
    m_state.shadowColor = Color();
    m_state.shadowRadiusMode = ShadowRadiusMode::Default;
    updateState(m_state, GraphicsContextState::ShadowChange);
}

bool GraphicsContext::getShadow(FloatSize& offset, float& blur, Color& color) const
{
    offset = m_state.shadowOffset;
    blur = m_state.shadowBlur;
    color = m_state.shadowColor;

    return hasShadow();
}

void GraphicsContext::setFillColor(const Color& color)
{
    m_state.fillColor = color;
    m_state.fillGradient = nullptr;
    m_state.fillPattern = nullptr;
    updateState(m_state, GraphicsContextState::FillColorChange);
}

void GraphicsContext::setStrokePattern(Ref<Pattern>&& pattern)
{
    m_state.strokeColor = { };
    m_state.strokeGradient = nullptr;
    m_state.strokePattern = WTFMove(pattern);
    updateState(m_state, GraphicsContextState::StrokePatternChange);
}

void GraphicsContext::setFillPattern(Ref<Pattern>&& pattern)
{
    m_state.fillColor = { };
    m_state.fillGradient = nullptr;
    m_state.fillPattern = WTFMove(pattern);
    updateState(m_state, GraphicsContextState::FillPatternChange);
}

void GraphicsContext::setStrokeGradient(Ref<Gradient>&& gradient, const AffineTransform& strokeGradientSpaceTransform)
{
    m_state.strokeColor = { };
    m_state.strokeGradient = WTFMove(gradient);
    m_state.strokeGradientSpaceTransform = strokeGradientSpaceTransform;
    m_state.strokePattern = nullptr;
    updateState(m_state, GraphicsContextState::StrokeGradientChange);
}

void GraphicsContext::setFillGradient(Ref<Gradient>&& gradient, const AffineTransform& fillGradientSpaceTransform)
{
    m_state.fillColor = { };
    m_state.fillGradient = WTFMove(gradient);
    m_state.fillGradientSpaceTransform = fillGradientSpaceTransform;
    m_state.fillPattern = nullptr;
    updateState(m_state, GraphicsContextState::FillGradientChange);
    // FIXME: also fill pattern?
}

void GraphicsContext::beginTransparencyLayer(float)
{
    ++m_transparencyLayerCount;
}

void GraphicsContext::endTransparencyLayer()
{
    ASSERT(m_transparencyLayerCount > 0);
    --m_transparencyLayerCount;
}

FloatSize GraphicsContext::drawText(const FontCascade& font, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    // Display list recording for text content is done at glyphs level. See GraphicsContext::drawGlyphs.
    return font.drawText(*this, run, point, from, to);
}

void GraphicsContext::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
{
    FontCascade::drawGlyphs(*this, font, glyphs, advances, numGlyphs, point, fontSmoothingMode);
}

void GraphicsContext::drawEmphasisMarks(const FontCascade& font, const TextRun& run, const AtomString& mark, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    font.drawEmphasisMarks(*this, run, mark, point, from, to);
}

void GraphicsContext::drawBidiText(const FontCascade& font, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
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
        subrun.setDirection(isRTL ? TextDirection::RTL : TextDirection::LTR);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        auto advance = font.drawText(*this, subrun, currPoint, 0, std::nullopt, customFontNotReadyAction);
        currPoint.move(advance);

        bidiRun = bidiRun->next();
    }

    bidiRuns.clear();
}

ImageDrawResult GraphicsContext::drawImage(Image& image, const FloatPoint& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    return drawImage(image, FloatRect(destination, image.size()), FloatRect(FloatPoint(), image.size()), imagePaintingOptions);
}

ImageDrawResult GraphicsContext::drawImage(Image& image, const FloatRect& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    FloatRect srcRect(FloatPoint(), image.size(imagePaintingOptions.orientation()));
    return drawImage(image, destination, srcRect, imagePaintingOptions);
}

ImageDrawResult GraphicsContext::drawImage(Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    return image.draw(*this, destination, source, options);
}

ImageDrawResult GraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    return image.drawTiled(*this, destination, source, tileSize, spacing, options);
}

ImageDrawResult GraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor,
    Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& options)
{
    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        return drawImage(image, destination, source, options);
    }

    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    return image.drawTiled(*this, destination, source, tileScaleFactor, hRule, vRule, options.compositeOperator());
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatPoint& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    drawImageBuffer(image, FloatRect(destination, image.logicalSize()), FloatRect(FloatPoint(), image.logicalSize()), imagePaintingOptions);
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatRect& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    drawImageBuffer(image, destination, FloatRect(FloatPoint(), FloatSize(image.logicalSize())), imagePaintingOptions);
}

void GraphicsContext::drawImageBuffer(ImageBuffer& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    image.draw(*this, destination, source, options);
}

void GraphicsContext::drawConsumingImageBuffer(RefPtr<ImageBuffer> image, const FloatPoint& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    if (!image)
        return;
    IntSize imageLogicalSize = image->logicalSize();
    drawConsumingImageBuffer(WTFMove(image), FloatRect(destination, imageLogicalSize), FloatRect(FloatPoint(), imageLogicalSize), imagePaintingOptions);
}

void GraphicsContext::drawConsumingImageBuffer(RefPtr<ImageBuffer> image, const FloatRect& destination, const ImagePaintingOptions& imagePaintingOptions)
{
    if (!image)
        return;
    IntSize imageLogicalSize = image->logicalSize();
    drawConsumingImageBuffer(WTFMove(image), destination, FloatRect(FloatPoint(), FloatSize(imageLogicalSize)), imagePaintingOptions);
}

void GraphicsContext::drawConsumingImageBuffer(RefPtr<ImageBuffer> image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& options)
{
    if (!image)
        return;
    InterpolationQualityMaintainer interpolationQualityForThisScope(*this, options.interpolationQuality());
    ImageBuffer::drawConsuming(WTFMove(image), *this, destination, source, options);
}

void GraphicsContext::clipRoundedRect(const FloatRoundedRect& rect)
{
    Path path;
    path.addRoundedRect(rect);
    clipPath(path);
}

void GraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    if (!rect.isRounded()) {
        clipOut(rect.rect());
        return;
    }

    Path path;
    path.addRoundedRect(rect);
    clipOut(path);
}

GraphicsContext::ClipToDrawingCommandsResult GraphicsContext::clipToDrawingCommands(const FloatRect& destination, const DestinationColorSpace& colorSpace, Function<void(GraphicsContext&)>&& drawingFunction)
{
    auto imageBuffer = ImageBuffer::createCompatibleBuffer(destination.size(), colorSpace, *this);
    if (!imageBuffer)
        return ClipToDrawingCommandsResult::FailedToCreateImageBuffer;

    drawingFunction(imageBuffer->context());
    clipToImageBuffer(*imageBuffer, destination);
    return ClipToDrawingCommandsResult::Success;
}

void GraphicsContext::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
    imageBuffer.clipToMask(*this, destinationRect);
}

IntRect GraphicsContext::clipBounds() const
{
    ASSERT_NOT_REACHED();
    return IntRect();
}

void GraphicsContext::fillRect(const FloatRect& rect, Gradient& gradient)
{
    gradient.fill(*this, rect);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    CompositeOperator previousOperator = compositeOperation();
    setCompositeOperation(op, blendMode);
    fillRect(rect, color);
    setCompositeOperation(previousOperator);
}

void GraphicsContext::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    if (rect.isRounded()) {
        setCompositeOperation(compositeOperation(), blendMode);
        fillRoundedRectImpl(rect, color);
        setCompositeOperation(compositeOperation());
    } else
        fillRect(rect.rect(), color, compositeOperation(), blendMode);
}

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();
    
    setFillRule(WindRule::EvenOdd);
    setFillColor(color);

    fillPath(path);
    
    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

void GraphicsContext::setCompositeOperation(CompositeOperator compositeOperation, BlendMode blendMode)
{
    m_state.compositeOperator = compositeOperation;
    m_state.blendMode = blendMode;
    updateState(m_state, GraphicsContextState::CompositeOperationChange);
}

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

FloatSize GraphicsContext::scaleFactor() const
{
    AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    return FloatSize(transform.xScale(), transform.yScale());
}
    
FloatSize GraphicsContext::scaleFactorForDrawing(const FloatRect& destRect, const FloatRect& srcRect) const
{
    AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    auto transformedDestRect = transform.mapRect(destRect);
    return transformedDestRect.size() / srcRect.size();
}

void GraphicsContext::drawPath(const Path& path)
{
    fillPath(path);
    strokePath(path);
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

void GraphicsContext::drawLineForText(const FloatRect& rect, bool printing, bool doubleUnderlines, StrokeStyle style)
{
    drawLinesForText(rect.location(), rect.height(), DashArray { 0, rect.width() }, printing, doubleUnderlines, style);
}

FloatRect GraphicsContext::computeUnderlineBoundsForText(const FloatRect& rect, bool printing)
{
    Color dummyColor;
    return computeLineBoundsAndAntialiasingModeForText(rect, printing, dummyColor);
}

FloatRect GraphicsContext::computeLineBoundsAndAntialiasingModeForText(const FloatRect& rect, bool printing, Color& color)
{
    FloatPoint origin = rect.location();
    float thickness = std::max(rect.height(), 0.5f);
    if (printing)
        return FloatRect(origin, FloatSize(rect.width(), thickness));

    AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    // Just compute scale in x dimension, assuming x and y scales are equal.
    float scale = transform.b() ? std::hypot(transform.a(), transform.b()) : transform.a();
    if (scale < 1.0) {
        // This code always draws a line that is at least one-pixel line high,
        // which tends to visually overwhelm text at small scales. To counter this
        // effect, an alpha is applied to the underline color when text is at small scales.
        static const float minimumUnderlineAlpha = 0.4f;
        float shade = scale > minimumUnderlineAlpha ? scale : minimumUnderlineAlpha;
        color = color.colorWithAlphaMultipliedBy(shade);
    }

    FloatPoint devicePoint = transform.mapPoint(rect.location());
    // Visual overflow might occur here due to integral roundf/ceilf. visualOverflowForDecorations adjusts the overflow value for underline decoration.
    FloatPoint deviceOrigin = FloatPoint(roundf(devicePoint.x()), ceilf(devicePoint.y()));
    if (auto inverse = transform.inverse())
        origin = inverse.value().mapPoint(deviceOrigin);
    return FloatRect(origin, FloatSize(rect.width(), thickness));
}

float GraphicsContext::dashedLineCornerWidthForStrokeWidth(float strokeWidth) const
{
    float thickness = strokeThickness();
    return strokeStyle() == DottedStroke ? thickness : std::min(2.0f * thickness, std::max(thickness, strokeWidth / 3.0f));
}

float GraphicsContext::dashedLinePatternWidthForStrokeWidth(float strokeWidth) const
{
    float thickness = strokeThickness();
    return strokeStyle() == DottedStroke ? thickness : std::min(3.0f * thickness, std::max(thickness, strokeWidth / 3.0f));
}

float GraphicsContext::dashedLinePatternOffsetForPatternAndStrokeWidth(float patternWidth, float strokeWidth) const
{
    // Pattern starts with full fill and ends with the empty fill.
    // 1. Let's start with the empty phase after the corner.
    // 2. Check if we've got odd or even number of patterns and whether they fully cover the line.
    // 3. In case of even number of patterns and/or remainder, move the pattern start position
    // so that the pattern is balanced between the corners.
    float patternOffset = patternWidth;
    int numberOfSegments = std::floor(strokeWidth / patternWidth);
    bool oddNumberOfSegments = numberOfSegments % 2;
    float remainder = strokeWidth - (numberOfSegments * patternWidth);
    if (oddNumberOfSegments && remainder)
        patternOffset -= remainder / 2.0f;
    else if (!oddNumberOfSegments) {
        if (remainder)
            patternOffset += patternOffset - (patternWidth + remainder) / 2.0f;
        else
            patternOffset += patternWidth / 2.0f;
    }

    return patternOffset;
}

Vector<FloatPoint> GraphicsContext::centerLineAndCutOffCorners(bool isVerticalLine, float cornerWidth, FloatPoint point1, FloatPoint point2) const
{
    // Center line and cut off corners for pattern painting.
    if (isVerticalLine) {
        float centerOffset = (point2.x() - point1.x()) / 2.0f;
        point1.move(centerOffset, cornerWidth);
        point2.move(-centerOffset, -cornerWidth);
    } else {
        float centerOffset = (point2.y() - point1.y()) / 2.0f;
        point1.move(cornerWidth, centerOffset);
        point2.move(-cornerWidth, -centerOffset);
    }

    return { point1, point2 };
}

#if ENABLE(VIDEO)
void GraphicsContext::paintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    player.playerPrivate()->paintCurrentFrameInContext(*this, destination);
}
#endif

void NullGraphicsContext::drawConsumingImageBuffer(RefPtr<ImageBuffer>, const FloatRect&, const FloatRect&, const ImagePaintingOptions&)
{
}

}
