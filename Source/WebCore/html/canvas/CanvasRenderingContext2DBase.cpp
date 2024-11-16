/*
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2013, 2014 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CanvasRenderingContext2DBase.h"

#include "BitmapImage.h"
#include "CSSFontSelector.h"
#include "CSSMarkup.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "CanvasFilterContextSwitcher.h"
#include "CanvasGradient.h"
#include "CanvasLayerContextSwitcher.h"
#include "CanvasPattern.h"
#include "ColorConversion.h"
#include "ColorSerialization.h"
#include "DOMMatrix.h"
#include "DOMMatrix2DInit.h"
#include "FloatQuad.h"
#include "GeometryUtilities.h"
#include "Gradient.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "OffscreenCanvas.h"
#include "PaintRenderingContext2D.h"
#include "Path2D.h"
#include "PixelBufferConversion.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderStyleInlines.h"
#include "RenderTheme.h"
#include "SVGImageElement.h"
#include "ScriptDisallowedScope.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "TextMetrics.h"
#include "TextRun.h"
#include "WebCodecsVideoFrame.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CanvasRenderingContext2DBase);

static constexpr ImageSmoothingQuality defaultSmoothingQuality = ImageSmoothingQuality::Low;

const int CanvasRenderingContext2DBase::DefaultFontSize = 10;
const ASCIILiteral CanvasRenderingContext2DBase::DefaultFontFamily = "sans-serif"_s;
static constexpr ASCIILiteral DefaultFont = "10px sans-serif"_s;

static CanvasLineCap toCanvasLineCap(LineCap lineCap)
{
    switch (lineCap) {
    case LineCap::Butt:
        return CanvasLineCap::Butt;
    case LineCap::Round:
        return CanvasLineCap::Round;
    case LineCap::Square:
        return CanvasLineCap::Square;
    }
    ASSERT_NOT_REACHED();
    return CanvasLineCap::Butt;
}

static LineCap fromCanvasLineCap(CanvasLineCap canvasLineCap)
{
    switch (canvasLineCap) {
    case CanvasLineCap::Butt:
        return LineCap::Butt;
    case CanvasLineCap::Round:
        return LineCap::Round;
    case CanvasLineCap::Square:
        return LineCap::Square;
    }
    ASSERT_NOT_REACHED();
    return LineCap::Butt;
}

static CanvasLineJoin toCanvasLineJoin(LineJoin lineJoin)
{
    switch (lineJoin) {
    case LineJoin::Round:
        return CanvasLineJoin::Round;
    case LineJoin::Bevel:
        return CanvasLineJoin::Bevel;
    case LineJoin::Miter:
        return CanvasLineJoin::Miter;
    }
    ASSERT_NOT_REACHED();
    return CanvasLineJoin::Round;
}

static LineJoin fromCanvasLineJoin(CanvasLineJoin canvasLineJoin)
{
    switch (canvasLineJoin) {
    case CanvasLineJoin::Round:
        return LineJoin::Round;
    case CanvasLineJoin::Bevel:
        return LineJoin::Bevel;
    case CanvasLineJoin::Miter:
        return LineJoin::Miter;
    }
    ASSERT_NOT_REACHED();
    return LineJoin::Round;
}

static CanvasTextAlign toCanvasTextAlign(TextAlign textAlign)
{
    switch (textAlign) {
    case StartTextAlign:
        return CanvasTextAlign::Start;
    case EndTextAlign:
        return CanvasTextAlign::End;
    case LeftTextAlign:
        return CanvasTextAlign::Left;
    case RightTextAlign:
        return CanvasTextAlign::Right;
    case CenterTextAlign:
        return CanvasTextAlign::Center;
    }
    ASSERT_NOT_REACHED();
    return CanvasTextAlign::Start;
}

static TextAlign fromCanvasTextAlign(CanvasTextAlign canvasTextAlign)
{
    switch (canvasTextAlign) {
    case CanvasTextAlign::Start:
        return StartTextAlign;
    case CanvasTextAlign::End:
        return EndTextAlign;
    case CanvasTextAlign::Left:
        return LeftTextAlign;
    case CanvasTextAlign::Right:
        return RightTextAlign;
    case CanvasTextAlign::Center:
        return CenterTextAlign;
    }
    ASSERT_NOT_REACHED();
    return StartTextAlign;
}

static CanvasTextBaseline toCanvasTextBaseline(TextBaseline textBaseline)
{
    switch (textBaseline) {
    case TopTextBaseline:
        return CanvasTextBaseline::Top;
    case HangingTextBaseline:
        return CanvasTextBaseline::Hanging;
    case MiddleTextBaseline:
        return CanvasTextBaseline::Middle;
    case AlphabeticTextBaseline:
        return CanvasTextBaseline::Alphabetic;
    case IdeographicTextBaseline:
        return CanvasTextBaseline::Ideographic;
    case BottomTextBaseline:
        return CanvasTextBaseline::Bottom;
    }
    ASSERT_NOT_REACHED();
    return CanvasTextBaseline::Top;
}

static TextBaseline fromCanvasTextBaseline(CanvasTextBaseline canvasTextBaseline)
{
    switch (canvasTextBaseline) {
    case CanvasTextBaseline::Top:
        return TopTextBaseline;
    case CanvasTextBaseline::Hanging:
        return HangingTextBaseline;
    case CanvasTextBaseline::Middle:
        return MiddleTextBaseline;
    case CanvasTextBaseline::Alphabetic:
        return AlphabeticTextBaseline;
    case CanvasTextBaseline::Ideographic:
        return IdeographicTextBaseline;
    case CanvasTextBaseline::Bottom:
        return BottomTextBaseline;
    }
    ASSERT_NOT_REACHED();
    return TopTextBaseline;
}

CanvasRenderingContext2DBase::CanvasRenderingContext2DBase(CanvasBase& canvas, CanvasRenderingContext::Type type, CanvasRenderingContext2DSettings&& settings, bool usesCSSCompatibilityParseMode)
    : CanvasRenderingContext(canvas, type)
    , m_stateStack(1)
    , m_usesCSSCompatibilityParseMode(usesCSSCompatibilityParseMode)
    , m_settings(WTFMove(settings))
{
    ASSERT(is2dBase());
}

void CanvasRenderingContext2DBase::unwindStateStack()
{
    // Ensure that the state stack in the ImageBuffer's context
    // is cleared before destruction, to avoid assertions in the
    // GraphicsContext dtor.
    size_t stackSize = m_stateStack.size();
    if (stackSize <= 1)
        return;
    // We need to keep the last state because it is tracked by CanvasBase::m_contextStateSaver.
    if (auto* context = existingDrawingContext())
        context->unwindStateStack(stackSize - 1);
}

CanvasRenderingContext2DBase::~CanvasRenderingContext2DBase()
{
#if ASSERT_ENABLED
    unwindStateStack();
#endif
}

bool CanvasRenderingContext2DBase::isAccelerated() const
{
    auto* context = existingDrawingContext();
    return context && context->renderingMode() == RenderingMode::Accelerated;
}

bool CanvasRenderingContext2DBase::isSurfaceBufferTransparentBlack(SurfaceBuffer) const
{
    // Before the first draw (or first access to the drawing buffer), the drawing buffer is transparent black.
    // Currently the canvas does not support alpha == false.
    return !canvasBase().hasCreatedImageBuffer();
}

#if USE(SKIA)
RefPtr<GraphicsLayerContentsDisplayDelegate> CanvasRenderingContext2DBase::layerContentsDisplayDelegate()
{
    if (auto buffer = canvasBase().buffer())
        return buffer->layerContentsDisplayDelegate();
    return nullptr;
}
#endif

bool CanvasRenderingContext2DBase::hasDeferredOperations() const
{
    // At the time of writing, any draw might linger in IPC buffer or queue of the underlying graphics system, like with Accelerated
    // codepaths of GraphicsContextCG. Currently we don't use GraphicsContext IsDeferred status since that has some subtleties.
    // Lingering draws are problematic, as they might retain references to the source surface that is coming from modifiable source
    // such as a 2D context. Holding them forever is not a good idea, so participate in the canvas preparation phase after any modification.
    return m_hasDeferredOperations;
}

void CanvasRenderingContext2DBase::flushDeferredOperations()
{
    m_hasDeferredOperations = false;
    if (RefPtr buffer = canvasBase().buffer())
        buffer->flushDrawingContextAsync();
}

void CanvasRenderingContext2DBase::reset()
{
    unwindStateStack();

    m_stateStack.resize(1);
    m_stateStack.first() = State();

    m_path.clear();
    m_unrealizedSaveCount = 0;
    m_cachedContents.emplace<CachedContentsTransparent>();
    m_hasDeferredOperations = false;
    clearAccumulatedDirtyRect();
    if (auto* c = existingDrawingContext()) {
        canvasBase().resetGraphicsContextState();
        c->clearRect(FloatRect { { }, canvasBase().size() });
    }
}

CanvasRenderingContext2DBase::State::State()
    : strokeStyle(Color::black)
    , fillStyle(Color::black)
    , lineWidth(1)
    , lineCap(LineCap::Butt)
    , lineJoin(LineJoin::Miter)
    , miterLimit(10)
    , shadowBlur(0)
    , shadowColor(Color::transparentBlack)
    , globalAlpha(1)
    , globalComposite(CompositeOperator::SourceOver)
    , globalBlend(BlendMode::Normal)
    , hasInvertibleTransform(true)
    , lineDashOffset(0)
    , imageSmoothingEnabled(true)
    , imageSmoothingQuality(defaultSmoothingQuality)
    , textAlign(StartTextAlign)
    , textBaseline(AlphabeticTextBaseline)
    , direction(Direction::Inherit)
    , filterString("none"_s)
    , unparsedFont(DefaultFont)
{
}

String CanvasRenderingContext2DBase::State::fontString() const
{
    if (!font.realized())
        return DefaultFont;

    StringBuilder serializedFont;
    const auto& font = this->font.fontDescription();

    auto italic = font.italic() ? "italic "_s : ""_s;
    auto smallCaps = font.variantCaps() == FontVariantCaps::Small ? "small-caps "_s : ""_s;
    serializedFont.append(italic, smallCaps, font.computedSize(), "px"_s);

    for (unsigned i = 0; i < font.familyCount(); ++i) {
        StringView family = font.familyAt(i);
        if (family.startsWith("-webkit-"_s))
            family = family.substring(8);

        auto separator = i ? ", "_s : " "_s;
        serializedFont.append(separator, serializeFontFamily(family.toString()));
    }

    return serializedFont.toString();
}

CanvasLineCap CanvasRenderingContext2DBase::State::canvasLineCap() const
{
    return toCanvasLineCap(lineCap);
}

CanvasLineJoin CanvasRenderingContext2DBase::State::canvasLineJoin() const
{
    return toCanvasLineJoin(lineJoin);
}

CanvasTextAlign CanvasRenderingContext2DBase::State::canvasTextAlign() const
{
    return toCanvasTextAlign(textAlign);
}

CanvasTextBaseline CanvasRenderingContext2DBase::State::canvasTextBaseline() const
{
    return toCanvasTextBaseline(textBaseline);
}

String CanvasRenderingContext2DBase::State::globalCompositeOperationString() const
{
    return compositeOperatorName(globalComposite, globalBlend);
}

String CanvasRenderingContext2DBase::State::shadowColorString() const
{
    return serializationForHTML(shadowColor);
}

CanvasRenderingContext2DBase::FontProxy::~FontProxy()
{
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);
}

CanvasRenderingContext2DBase::FontProxy::FontProxy(const FontProxy& other)
    : m_font(other.m_font)
{
    if (realized())
        m_font.fontSelector()->registerForInvalidationCallbacks(*this);
}

auto CanvasRenderingContext2DBase::FontProxy::operator=(const FontProxy& other) -> FontProxy&
{
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);

    m_font = other.m_font;

    if (realized())
        m_font.fontSelector()->registerForInvalidationCallbacks(*this);

    return *this;
}

inline void CanvasRenderingContext2DBase::FontProxy::update(FontSelector& selector)
{
    ASSERT(&selector == m_font.fontSelector()); // This is an invariant. We should only ever be registered for callbacks on m_font.m_fonts.m_fontSelector.
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);
    m_font.update(&selector);
    if (realized())
        m_font.fontSelector()->registerForInvalidationCallbacks(*this);
    ASSERT(&selector == m_font.fontSelector());
}

void CanvasRenderingContext2DBase::FontProxy::fontsNeedUpdate(FontSelector& selector)
{
    ASSERT_ARG(selector, &selector == m_font.fontSelector());
    ASSERT(realized());

    update(selector);
}

void CanvasRenderingContext2DBase::FontProxy::initialize(FontSelector& fontSelector, const FontCascade& fontCascade)
{
    // Beware! m_font.fontSelector() might not point to document.fontSelector()!
    ASSERT(fontCascade.fontSelector() == &fontSelector);
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);
    m_font = fontCascade;
    m_font.update(&fontSelector);
    ASSERT(&fontSelector == m_font.fontSelector());
    m_font.fontSelector()->registerForInvalidationCallbacks(*this);
}

const FontMetrics& CanvasRenderingContext2DBase::FontProxy::metricsOfPrimaryFont() const
{
    return m_font.metricsOfPrimaryFont();
}

const FontCascadeDescription& CanvasRenderingContext2DBase::FontProxy::fontDescription() const
{
    return m_font.fontDescription();
}

float CanvasRenderingContext2DBase::FontProxy::width(const TextRun& textRun, GlyphOverflow* overflow) const
{
    return m_font.width(textRun, 0, overflow);
}

void CanvasRenderingContext2DBase::FontProxy::drawBidiText(GraphicsContext& context, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction action) const
{
    context.drawBidiText(m_font, run, point, action);
}

void CanvasRenderingContext2DBase::realizeSaves()
{
    if (m_unrealizedSaveCount)
        realizeSavesLoop();

    if (m_unrealizedSaveCount) {
        static NeverDestroyed<String> consoleMessage(MAKE_STATIC_STRING_IMPL("CanvasRenderingContext2D.save() has been called without a matching restore() too many times. Ignoring save()."));

        canvasBase().scriptExecutionContext()->addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, consoleMessage);
    }
}

void CanvasRenderingContext2DBase::realizeSavesLoop()
{
    ASSERT(m_unrealizedSaveCount);
    ASSERT(m_stateStack.size() >= 1);
    GraphicsContext* context = drawingContext();
    do {
        if (m_stateStack.size() > MaxSaveCount)
            break;
        m_stateStack.append(state());
        if (context)
            context->save();
    } while (--m_unrealizedSaveCount);
}

void CanvasRenderingContext2DBase::restore()
{
    if (m_unrealizedSaveCount) {
        --m_unrealizedSaveCount;
        return;
    }
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().transform);
    m_stateStack.removeLast();
    if (std::optional<AffineTransform> inverse = state().transform.inverse())
        m_path.transform(inverse.value());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->restore();
}

void CanvasRenderingContext2DBase::beginLayer()
{
    save();
    realizeSaves();

    RefPtr<Filter> filter;
    if (!state().filterOperations.isEmpty())
        filter = createFilter(backingStoreBounds());

    modifiableState().targetSwitcher = CanvasLayerContextSwitcher::create(*this, backingStoreBounds(), WTFMove(filter));

    // Reset layer rendering state.
    setGlobalAlpha(1.0);
    setGlobalCompositeOperation("source-over"_s);
    setShadowOffsetX(0);
    setShadowOffsetY(0);
    setShadowBlur(0);
    setShadowColor("black"_s);
    setFilterString("none"_s);
}

void CanvasRenderingContext2DBase::endLayer()
{
    // The destructor of CanvasLayerContextSwitcher composites the layer to the destination context.
    realizeSaves();
    restore();

    didDrawEntireCanvas();
}

void CanvasRenderingContext2DBase::setStrokeStyle(CanvasStyle style)
{
    if (state().strokeStyle.isEquivalentColor(style))
        return;

    checkOrigin(style.canvasPattern().get());

    realizeSaves();
    State& state = modifiableState();
    state.strokeStyle = WTFMove(style);
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    state.strokeStyle.applyStrokeColor(*c);
    state.unparsedStrokeColor = String();
}

void CanvasRenderingContext2DBase::setStrokeStyle(std::optional<CanvasStyle> style)
{
    if (!style)
        return;
    return setStrokeStyle(WTFMove(*style));
}

void CanvasRenderingContext2DBase::setFillStyle(CanvasStyle style)
{
    if (state().fillStyle.isEquivalentColor(style))
        return;

    checkOrigin(style.canvasPattern().get());

    realizeSaves();
    State& state = modifiableState();
    state.fillStyle = WTFMove(style);
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;

    state.fillStyle.applyFillColor(*c);
    state.unparsedFillColor = String();
}

void CanvasRenderingContext2DBase::setFillStyle(std::optional<CanvasStyle> style)
{
    if (!style)
        return;
    return setFillStyle(WTFMove(*style));
}

void CanvasRenderingContext2DBase::setLineWidth(double width)
{
    if (!(std::isfinite(width) && width > 0))
        return;
    if (state().lineWidth == width)
        return;
    realizeSaves();
    modifiableState().lineWidth = width;
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    c->setStrokeThickness(width);
}

void CanvasRenderingContext2DBase::setLineCap(CanvasLineCap canvasLineCap)
{
    auto lineCap = fromCanvasLineCap(canvasLineCap);
    if (state().lineCap == lineCap)
        return;
    realizeSaves();
    modifiableState().lineCap = lineCap;
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    c->setLineCap(lineCap);
}

void CanvasRenderingContext2DBase::setLineCap(const String& stringValue)
{
    CanvasLineCap cap;
    if (stringValue == "butt"_s)
        cap = CanvasLineCap::Butt;
    else if (stringValue == "round"_s)
        cap = CanvasLineCap::Round;
    else if (stringValue == "square"_s)
        cap = CanvasLineCap::Square;
    else
        return;
    
    setLineCap(cap);
}

void CanvasRenderingContext2DBase::setLineJoin(CanvasLineJoin canvasLineJoin)
{
    auto lineJoin = fromCanvasLineJoin(canvasLineJoin);
    if (state().lineJoin == lineJoin)
        return;
    realizeSaves();
    modifiableState().lineJoin = lineJoin;
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    c->setLineJoin(lineJoin);
}

void CanvasRenderingContext2DBase::setLineJoin(const String& stringValue)
{
    CanvasLineJoin join;
    if (stringValue == "round"_s)
        join = CanvasLineJoin::Round;
    else if (stringValue == "bevel"_s)
        join = CanvasLineJoin::Bevel;
    else if (stringValue == "miter"_s)
        join = CanvasLineJoin::Miter;
    else
        return;

    setLineJoin(join);
}

void CanvasRenderingContext2DBase::setMiterLimit(double limit)
{
    if (!(std::isfinite(limit) && limit > 0))
        return;
    if (state().miterLimit == limit)
        return;
    realizeSaves();
    modifiableState().miterLimit = limit;
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    c->setMiterLimit(limit);
}

void CanvasRenderingContext2DBase::setShadowOffsetX(float x)
{
    if (!std::isfinite(x))
        return;
    if (state().shadowOffset.width() == x)
        return;
    realizeSaves();
    modifiableState().shadowOffset.setWidth(x);
    applyShadow();
}

void CanvasRenderingContext2DBase::setShadowOffsetY(float y)
{
    if (!std::isfinite(y))
        return;
    if (state().shadowOffset.height() == y)
        return;
    realizeSaves();
    modifiableState().shadowOffset.setHeight(y);
    applyShadow();
}

void CanvasRenderingContext2DBase::setShadowBlur(float blur)
{
    if (!(std::isfinite(blur) && blur >= 0))
        return;
    if (state().shadowBlur == blur)
        return;
    realizeSaves();
    modifiableState().shadowBlur = blur;
    applyShadow();
}

void CanvasRenderingContext2DBase::setShadowColor(const String& colorString)
{
    Color color = parseColor(colorString, canvasBase());
    if (!color.isValid())
        return;
    if (state().shadowColor == color)
        return;
    realizeSaves();
    modifiableState().shadowColor = color;
    applyShadow();
}

static bool lineDashSequenceIsValid(const Vector<double>& dash)
{
    for (size_t i = 0; i < dash.size(); i++) {
        if (!std::isfinite(dash[i]) || dash[i] < 0)
            return false;
    }
    return true;
}

void CanvasRenderingContext2DBase::setLineDash(const Vector<double>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;

    realizeSaves();
    modifiableState().lineDash = dash;
    // Spec requires the concatenation of two copies the dash list when the
    // number of elements is odd
    if (dash.size() % 2)
        modifiableState().lineDash.appendVector(dash);

    applyLineDash();
}

void CanvasRenderingContext2DBase::setWebkitLineDash(const Vector<double>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;

    realizeSaves();
    modifiableState().lineDash = dash;

    applyLineDash();
}

void CanvasRenderingContext2DBase::setLineDashOffset(double offset)
{
    if (!std::isfinite(offset) || state().lineDashOffset == offset)
        return;

    realizeSaves();
    modifiableState().lineDashOffset = offset;
    applyLineDash();
}

void CanvasRenderingContext2DBase::applyLineDash() const
{
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    DashArray convertedLineDash(state().lineDash.size());
    for (size_t i = 0; i < state().lineDash.size(); ++i)
        convertedLineDash[i] = static_cast<DashArrayElement>(state().lineDash[i]);
    c->setLineDash(convertedLineDash, state().lineDashOffset);
}

void CanvasRenderingContext2DBase::setGlobalAlpha(double alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    if (state().globalAlpha == alpha)
        return;
    realizeSaves();
    modifiableState().globalAlpha = alpha;
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    c->setAlpha(alpha);
}

void CanvasRenderingContext2DBase::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op = CompositeOperator::SourceOver;
    BlendMode blendMode = BlendMode::Normal;
    if (!parseCompositeAndBlendOperator(operation, op, blendMode))
        return;
    if ((state().globalComposite == op) && (state().globalBlend == blendMode))
        return;
    realizeSaves();
    modifiableState().globalComposite = op;
    modifiableState().globalBlend = blendMode;
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    c->setCompositeOperation(op, blendMode);
}

void CanvasRenderingContext2DBase::setFilterString(const String& filterString)
{
    if (state().filterString == filterString)
        return;

    // Spec: context.filter = "" should leave the current filter unchanged.
    if (filterString.isEmpty())
        return;

    // Spec: context.filter = null or context.filter = undefined should leave the current filter unchanged.
    if (filterString == "null"_s || filterString == "undefined"_s)
        return;

    auto filterOperations = setFilterStringWithoutUpdatingStyle(filterString);
    if (!filterOperations)
        return;

    realizeSaves();

    // Spec: context.filter = "none" filters will be disabled for the context.
    // Spec: Only parseable inputs should change the current filter.
    modifiableState().filterString = filterString;
    modifiableState().filterOperations = WTFMove(*filterOperations);
}

void CanvasRenderingContext2DBase::scale(double sx, double sy)
{
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    if (!std::isfinite(sx) || !std::isfinite(sy))
        return;

    AffineTransform newTransform = state().transform;
    newTransform.scaleNonUniform(sx, sy);
    if (state().transform == newTransform)
        return;

    realizeSaves();

    if (!sx || !sy) {
        modifiableState().hasInvertibleTransform = false;
        return;
    }

    modifiableState().transform = newTransform;
    c->scale(FloatSize(sx, sy));
    m_path.transform(AffineTransform().scaleNonUniform(1.0 / sx, 1.0 / sy));
}

void CanvasRenderingContext2DBase::rotate(double angleInRadians)
{
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    if (!std::isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().transform;
    newTransform.rotateRadians(angleInRadians);
    if (state().transform == newTransform)
        return;

    realizeSaves();

    modifiableState().transform = newTransform;
    c->rotate(angleInRadians);
    m_path.transform(AffineTransform().rotateRadians(-angleInRadians));
}

void CanvasRenderingContext2DBase::translate(double tx, double ty)
{
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    if (!std::isfinite(tx) || !std::isfinite(ty))
        return;

    AffineTransform newTransform = state().transform;
    newTransform.translate(tx, ty);
    if (state().transform == newTransform)
        return;

    realizeSaves();

    modifiableState().transform = newTransform;
    c->translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2DBase::transform(double m11, double m12, double m21, double m22, double dx, double dy)
{
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    AffineTransform newTransform = state().transform * transform;
    if (state().transform == newTransform)
        return;

    realizeSaves();

    if (auto inverse = transform.inverse()) {
        modifiableState().transform = newTransform;
        c->concatCTM(transform);
        m_path.transform(inverse.value());
        return;
    }
    modifiableState().hasInvertibleTransform = false;
}

Ref<DOMMatrix> CanvasRenderingContext2DBase::getTransform() const
{
    return DOMMatrix::create(state().transform.toTransformationMatrix(), DOMMatrixReadOnly::Is2D::Yes);
}

void CanvasRenderingContext2DBase::setTransform(double m11, double m12, double m21, double m22, double dx, double dy)
{
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    resetTransform();
    transform(m11, m12, m21, m22, dx, dy);
}

ExceptionOr<void> CanvasRenderingContext2DBase::setTransform(DOMMatrix2DInit&& matrixInit)
{
    auto checkValid = DOMMatrixReadOnly::validateAndFixup(matrixInit);
    if (checkValid.hasException())
        return checkValid.releaseException();

    setTransform(matrixInit.m11.value(), matrixInit.m12.value(), matrixInit.m21.value(), matrixInit.m22.value(), matrixInit.m41.value(), matrixInit.m42.value());
    return { };
}

void CanvasRenderingContext2DBase::resetTransform()
{
    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return;

    AffineTransform ctm = state().transform;
    bool hasInvertibleTransform = state().hasInvertibleTransform;

    realizeSaves();

    c->setCTM(baseTransform());
    modifiableState().transform = AffineTransform();

    if (hasInvertibleTransform)
        m_path.transform(ctm);

    modifiableState().hasInvertibleTransform = true;
}

void CanvasRenderingContext2DBase::setStrokeColor(const String& color, std::optional<float> alpha)
{
    if (alpha) {
        if (std::isnan(*alpha))
            return;

        setStrokeStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha.value(), canvasBase()));
        return;
    }

    if (color == state().unparsedStrokeColor)
        return;

    realizeSaves();
    setStrokeStyle(CanvasStyle::createFromString(color, canvasBase()));
    modifiableState().unparsedStrokeColor = color;
}

void CanvasRenderingContext2DBase::setStrokeColor(float grayLevel, float alpha)
{
    if (std::isnan(grayLevel) || std::isnan(alpha))
        return;

    auto color = makeFromComponentsClamping<SRGBA<float>>(grayLevel, grayLevel, grayLevel, alpha);
    if (state().strokeStyle.isEquivalent(color))
        return;
    setStrokeStyle(CanvasStyle(color));
}

void CanvasRenderingContext2DBase::setStrokeColor(float r, float g, float b, float a)
{
    if (std::isnan(r) || std::isnan(g) || std::isnan(b)  || std::isnan(a))
        return;

    auto color = makeFromComponentsClamping<SRGBA<float>>(r, g, b, a);
    if (state().strokeStyle.isEquivalent(color))
        return;
    setStrokeStyle(CanvasStyle(color));
}

void CanvasRenderingContext2DBase::setFillColor(const String& color, std::optional<float> alpha)
{
    if (alpha) {
        if (std::isnan(*alpha))
            return;

        setFillStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha.value(), canvasBase()));
        return;
    }

    if (color == state().unparsedFillColor)
        return;

    realizeSaves();
    setFillStyle(CanvasStyle::createFromString(color, canvasBase()));
    modifiableState().unparsedFillColor = color;
}

void CanvasRenderingContext2DBase::setFillColor(float grayLevel, float alpha)
{
    if (std::isnan(grayLevel) || std::isnan(alpha))
        return;

    auto color = makeFromComponentsClamping<SRGBA<float>>(grayLevel, grayLevel, grayLevel, alpha);
    if (state().fillStyle.isEquivalent(color))
        return;
    setFillStyle(CanvasStyle(color));
}

void CanvasRenderingContext2DBase::setFillColor(float r, float g, float b, float a)
{
    if (std::isnan(r) || std::isnan(g) || std::isnan(b)  || std::isnan(a))
        return;

    auto color = makeFromComponentsClamping<SRGBA<float>>(r, g, b, a);
    if (state().fillStyle.isEquivalent(color))
        return;
    setFillStyle(CanvasStyle(color));
}

void CanvasRenderingContext2DBase::beginPath()
{
    m_path.clear();
}

static bool validateRectForCanvas(double& x, double& y, double& width, double& height)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height))
        return false;

    if (!width && !height)
        return false;

    if (width < 0) {
        width = -width;
        x -= width;
    }

    if (height < 0) {
        height = -height;
        y -= height;
    }

    return true;
}

static bool isFullCanvasCompositeMode(CompositeOperator op)
{
    // See 4.8.11.1.3 Compositing
    // CompositeOperator::SourceAtop and CompositeOperator::DestinationOut are not listed here as the platforms already
    // implement the specification's behavior.
    return op == CompositeOperator::SourceIn || op == CompositeOperator::SourceOut || op == CompositeOperator::DestinationIn || op == CompositeOperator::DestinationAtop;
}

static WindRule toWindRule(CanvasFillRule rule)
{
    return rule == CanvasFillRule::Nonzero ? WindRule::NonZero : WindRule::EvenOdd;
}

void CanvasRenderingContext2DBase::fill(CanvasFillRule windingRule)
{
    fillInternal(m_path, windingRule);
}

void CanvasRenderingContext2DBase::stroke()
{
    strokeInternal(m_path);
}

void CanvasRenderingContext2DBase::clip(CanvasFillRule windingRule)
{
    clipInternal(m_path, windingRule);
}

void CanvasRenderingContext2DBase::fill(Path2D& path, CanvasFillRule windingRule)
{
    fillInternal(path.path(), windingRule);
}

void CanvasRenderingContext2DBase::stroke(Path2D& path)
{
    strokeInternal(path.path());
}

void CanvasRenderingContext2DBase::clip(Path2D& path, CanvasFillRule windingRule)
{
    clipInternal(path.path(), windingRule);
}

static inline IntRect computeImageDataRect(const ImageBuffer& buffer, IntSize sourceSize, IntRect& destRect, IntPoint destOffset)
{
    destRect.intersect(IntRect { { }, sourceSize });
    destRect.moveBy(destOffset);
    destRect.intersect(IntRect { { }, buffer.truncatedLogicalSize() });
    if (destRect.isEmpty())
        return destRect;
    IntRect sourceRect { destRect };
    sourceRect.moveBy(-destOffset);
    sourceRect.intersect(IntRect { { }, sourceSize });
    return sourceRect;
}

void CanvasRenderingContext2DBase::fillInternal(const Path& path, CanvasFillRule windingRule)
{
    std::unique_ptr<CanvasFilterContextSwitcher> targetSwitcher;
    if (!state().filterOperations.isEmpty())
        targetSwitcher = CanvasFilterContextSwitcher::create(*this, path.fastBoundingRect());

    auto* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    // If gradient size is zero, then paint nothing.
    auto gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    if (path.isEmpty())
        return;

    auto savedFillRule = c->fillRule();
    c->setFillRule(toWindRule(windingRule));

    bool repaintEntireCanvas = false;
    if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        c->fillPath(path);
        endCompositeLayer();
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        clearCanvas();
        c->fillPath(path);
        repaintEntireCanvas = true;
    } else
        c->fillPath(path);

    didDraw(repaintEntireCanvas, [&]() {
        return targetSwitcher ? targetSwitcher->expandedBounds() : path.fastBoundingRect();
    });

    c->setFillRule(savedFillRule);
}

void CanvasRenderingContext2DBase::strokeInternal(const Path& path)
{
    std::unique_ptr<CanvasFilterContextSwitcher> targetSwitcher;
    if (!state().filterOperations.isEmpty())
        targetSwitcher = CanvasFilterContextSwitcher::create(*this, inflatedStrokeRect(path.fastBoundingRect()));

    auto* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    // If gradient size is zero, then paint nothing.
    auto gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    if (path.isEmpty())
        return;

    bool repaintEntireCanvas = false;
    if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        c->strokePath(path);
        endCompositeLayer();
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        clearCanvas();
        c->strokePath(path);
        repaintEntireCanvas = true;
    } else
        c->strokePath(path);

    didDraw(repaintEntireCanvas, [&]() {
        return targetSwitcher ? targetSwitcher->expandedBounds() : inflatedStrokeRect(path.fastBoundingRect());
    });
}

void CanvasRenderingContext2DBase::clipInternal(const Path& path, CanvasFillRule windingRule)
{
    auto* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    realizeSaves();
    c->clipPath(path, toWindRule(windingRule));
}

void CanvasRenderingContext2DBase::beginCompositeLayer()
{
#if !USE(CAIRO)
    auto* context = effectiveDrawingContext();
    context->beginTransparencyLayer(state().globalComposite, state().globalBlend);
#endif
}

void CanvasRenderingContext2DBase::endCompositeLayer()
{
#if !USE(CAIRO)
    auto* context = effectiveDrawingContext();
    context->endTransparencyLayer();
#endif
}

bool CanvasRenderingContext2DBase::isPointInPath(double x, double y, CanvasFillRule windingRule)
{
    return isPointInPathInternal(m_path, x, y, windingRule);
}

bool CanvasRenderingContext2DBase::isPointInStroke(double x, double y)
{
    return isPointInStrokeInternal(m_path, x, y);
}

bool CanvasRenderingContext2DBase::isPointInPath(Path2D& path, double x, double y, CanvasFillRule windingRule)
{
    return isPointInPathInternal(path.path(), x, y, windingRule);
}

bool CanvasRenderingContext2DBase::isPointInStroke(Path2D& path, double x, double y)
{
    return isPointInStrokeInternal(path.path(), x, y);
}

bool CanvasRenderingContext2DBase::isPointInPathInternal(const Path& path, double x, double y, CanvasFillRule windingRule)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return false;
    
    if (!effectiveDrawingContext())
        return false;
    auto& state = this->state();
    if (!state.hasInvertibleTransform)
        return false;

    auto transformedPoint = valueOrDefault(state.transform.inverse()).mapPoint(FloatPoint(x, y));
    ASSERT(std::isfinite(transformedPoint.x()) && std::isfinite(transformedPoint.y()));

    return path.contains(transformedPoint, toWindRule(windingRule));
}

bool CanvasRenderingContext2DBase::isPointInStrokeInternal(const Path& path, double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return false;

    if (!effectiveDrawingContext())
        return false;
    auto& state = this->state();
    if (!state.hasInvertibleTransform)
        return false;

    auto transformedPoint = valueOrDefault(state.transform.inverse()).mapPoint(FloatPoint(x, y));
    ASSERT(std::isfinite(transformedPoint.x()) && std::isfinite(transformedPoint.y()));

    return path.strokeContains(transformedPoint, [&state] (GraphicsContext& context) {
        context.setStrokeThickness(state.lineWidth);
        context.setLineCap(state.lineCap);
        context.setLineJoin(state.lineJoin);
        context.setMiterLimit(state.miterLimit);
        auto& lineDash = state.lineDash;
        DashArray convertedLineDash(lineDash.size());
        for (size_t i = 0; i < lineDash.size(); ++i)
            convertedLineDash[i] = static_cast<DashArrayElement>(lineDash[i]);
        context.setLineDash(convertedLineDash, state.lineDashOffset);
    });
}

void CanvasRenderingContext2DBase::clearRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    auto* context = effectiveDrawingContext();
    if (!context)
        return;
    if (!state().hasInvertibleTransform)
        return;
    FloatRect rect(x, y, width, height);

    bool saved = false;
    if (shouldDrawShadows()) {
        context->save();
        saved = true;
        context->setDropShadow({ FloatSize(), 0, Color::transparentBlack, ShadowRadiusMode::Legacy });
    }
    if (state().globalAlpha != 1) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setAlpha(1);
    }
    if (state().globalComposite != CompositeOperator::SourceOver) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setCompositeOperation(CompositeOperator::SourceOver);
    }
    context->clearRect(rect);
    if (saved)
        context->restore();
    didDraw(rect, defaultDidDrawOptionsWithoutPostProcessing());
}

void CanvasRenderingContext2DBase::fillRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    FloatRect rect(x, y, width, height);

    auto targetSwitcher = CanvasFilterContextSwitcher::create(*this, rect);

    auto* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    // from the HTML5 Canvas spec:
    // If x0 = x1 and y0 = y1, then the linear gradient must paint nothing
    // If x0 = x1 and y0 = y1 and r0 = r1, then the radial gradient must paint nothing
    auto gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    bool repaintEntireCanvas = false;
    if (rectContainsCanvas(rect)) {
#if USE(SKIA)
        const bool needsCompositeLayer = shouldDrawShadows() && isFullCanvasCompositeMode(state().globalComposite);
        if (needsCompositeLayer)
            beginCompositeLayer();
#endif
        c->fillRect(rect);
#if USE(SKIA)
        if (needsCompositeLayer)
            endCompositeLayer();
#endif
        repaintEntireCanvas = true;
    } else if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        c->fillRect(rect);
        endCompositeLayer();
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        clearCanvas();
        c->fillRect(rect);
        repaintEntireCanvas = true;
    } else
        c->fillRect(rect);

    didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : rect);
}

void CanvasRenderingContext2DBase::strokeRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    FloatRect rect(x, y, width, height);
    FloatRect inflatedStrokeRect = rect;
    inflatedStrokeRect.inflate(state().lineWidth / 2);

    auto targetSwitcher = CanvasFilterContextSwitcher::create(*this, inflatedStrokeRect);

    auto* c = effectiveDrawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;
    if (!(state().lineWidth >= 0))
        return;

    // If gradient size is zero, then paint nothing.
    auto gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    bool repaintEntireCanvas = false;
    if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        c->strokeRect(rect, state().lineWidth);
        endCompositeLayer();
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        clearCanvas();
        c->strokeRect(rect, state().lineWidth);
        repaintEntireCanvas = true;
    } else
        c->strokeRect(rect, state().lineWidth);

    didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : inflatedStrokeRect);
}

void CanvasRenderingContext2DBase::setShadow(float width, float height, float blur, const String& colorString, std::optional<float> alpha)
{
    if (alpha && std::isnan(*alpha))
        return;

    Color color = Color::transparentBlack;
    if (!colorString.isNull()) {
        color = parseColor(colorString, canvasBase());
        if (!color.isValid())
            return;
    }
    setShadow(FloatSize(width, height), blur, color.colorWithAlpha(alpha));
}

void CanvasRenderingContext2DBase::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    if (std::isnan(grayLevel) || std::isnan(alpha))
        return;

    setShadow(FloatSize(width, height), blur, convertColor<SRGBA<uint8_t>>(makeFromComponentsClamping<SRGBA<float>>(grayLevel, grayLevel, grayLevel, alpha)));
}

void CanvasRenderingContext2DBase::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    if (std::isnan(r) || std::isnan(g) || std::isnan(b)  || std::isnan(a))
        return;

    setShadow(FloatSize(width, height), blur, convertColor<SRGBA<uint8_t>>(makeFromComponentsClamping<SRGBA<float>>(r, g, b, a)));
}

void CanvasRenderingContext2DBase::clearShadow()
{
    setShadow(FloatSize(), 0, Color::transparentBlack);
}

void CanvasRenderingContext2DBase::setShadow(const FloatSize& offset, float blur, const Color& color)
{
    if (state().shadowOffset == offset && state().shadowBlur == blur && state().shadowColor == color)
        return;
    bool wasDrawingShadows = shouldDrawShadows();
    realizeSaves();
    modifiableState().shadowOffset = offset;
    modifiableState().shadowBlur = blur;
    modifiableState().shadowColor = color;
    if (!wasDrawingShadows && !shouldDrawShadows())
        return;
    applyShadow();
}

void CanvasRenderingContext2DBase::applyShadow()
{
    auto* c = effectiveDrawingContext();
    if (!c)
        return;

    if (shouldDrawShadows()) {
        auto shadowOffset = c->platformShadowOffset(state().shadowOffset);
        c->setDropShadow({ shadowOffset, state().shadowBlur, state().shadowColor, ShadowRadiusMode::Legacy });
    } else
        c->setDropShadow({ { }, 0, Color::transparentBlack, ShadowRadiusMode::Legacy });
}

bool CanvasRenderingContext2DBase::shouldDrawShadows() const
{
    return state().shadowColor.isVisible() && (state().shadowBlur || !state().shadowOffset.isZero());
}

enum class ImageSizeType { AfterDevicePixelRatio, BeforeDevicePixelRatio };
static LayoutSize size(CachedImage* cachedImage, RenderElement* renderer, ImageSizeType sizeType = ImageSizeType::BeforeDevicePixelRatio)
{
    if (!cachedImage)
        return { };
    LayoutSize size = cachedImage->imageSizeForRenderer(renderer, 1.0f); // FIXME: Not sure about this.
    if (auto* renderImage = dynamicDowncast<RenderImage>(renderer); sizeType == ImageSizeType::AfterDevicePixelRatio && renderImage && cachedImage->image() && !cachedImage->image()->hasRelativeWidth())
        size.scale(renderImage->imageDevicePixelRatio());
    return size;
}

static LayoutSize size(HTMLImageElement& element, ImageSizeType sizeType = ImageSizeType::BeforeDevicePixelRatio)
{
    return size(element.cachedImage(), element.renderer(), sizeType);
}

static LayoutSize size(SVGImageElement& element, ImageSizeType sizeType = ImageSizeType::BeforeDevicePixelRatio)
{
    return size(element.cachedImage(), element.renderer(), sizeType);
}

static inline FloatSize size(CanvasBase& canvas)
{
    return canvas.size();
}

static inline FloatSize size(ImageBitmap& imageBitmap)
{
    return FloatSize { static_cast<float>(imageBitmap.width()), static_cast<float>(imageBitmap.height()) };
}

#if ENABLE(VIDEO)

static inline FloatSize size(HTMLVideoElement& video)
{
    RefPtr player = video.player();
    if (!player)
        return { };
    return player->naturalSize();
}

#endif

static inline FloatSize size(CSSStyleImageValue& image)
{
    auto* cachedImage = image.image();
    if (!cachedImage)
        return FloatSize();

    return cachedImage->imageSizeForRenderer(nullptr, 1.0f);
}

#if ENABLE(WEB_CODECS)
static inline FloatSize size(const WebCodecsVideoFrame& frame)
{
    return FloatSize { static_cast<float>(frame.displayWidth()), static_cast<float>(frame.displayHeight()) };
}
#endif

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(CanvasImageSource&& image, float dx, float dy)
{
    return WTF::switchOn(image,
        [&] (RefPtr<HTMLImageElement>& imageElement) -> ExceptionOr<void> {
            LayoutSize destRectSize = size(*imageElement, ImageSizeType::AfterDevicePixelRatio);
            LayoutSize sourceRectSize = size(*imageElement, ImageSizeType::BeforeDevicePixelRatio);
            return this->drawImage(*imageElement, FloatRect { 0, 0, sourceRectSize.width(), sourceRectSize.height() }, FloatRect { dx, dy, destRectSize.width(), destRectSize.height() });
        },
        [&] (RefPtr<SVGImageElement>& imageElement) -> ExceptionOr<void> {
            LayoutSize destRectSize = size(*imageElement, ImageSizeType::AfterDevicePixelRatio);
            LayoutSize sourceRectSize = size(*imageElement, ImageSizeType::BeforeDevicePixelRatio);
            return this->drawImage(*imageElement, FloatRect { 0, 0, sourceRectSize.width(), sourceRectSize.height() }, FloatRect { dx, dy, destRectSize.width(), destRectSize.height() });
        },
        [&] (auto& element) -> ExceptionOr<void> {
            FloatSize elementSize = size(*element);
            return this->drawImage(*element, FloatRect { 0, 0, elementSize.width(), elementSize.height() }, FloatRect { dx, dy, elementSize.width(), elementSize.height() });
        }
    );
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(CanvasImageSource&& image, float dx, float dy, float dw, float dh)
{
    return WTF::switchOn(image,
        [&] (auto& element) -> ExceptionOr<void> {
            FloatSize elementSize = size(*element);
            return this->drawImage(*element, FloatRect { 0, 0, elementSize.width(), elementSize.height() }, FloatRect { dx, dy, dw, dh });
        }
    );
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(CanvasImageSource&& image, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh)
{
    return WTF::switchOn(image,
        [&] (auto& element) -> ExceptionOr<void> {
            return this->drawImage(*element, FloatRect { sx, sy, sw, sh }, FloatRect { dx, dy, dw, dh });
        }
    );
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(HTMLImageElement& imageElement, const FloatRect& srcRect, const FloatRect& dstRect)
{
    return drawImage(imageElement, srcRect, dstRect, state().globalComposite, state().globalBlend);
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(HTMLImageElement& imageElement, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator& op, const BlendMode& blendMode)
{
    if (!imageElement.complete())
        return { };

    auto* cachedImage = imageElement.cachedImage();
    if (!cachedImage)
        return { };

    if (cachedImage->status() == CachedImage::Status::DecodeError)
        return Exception { ExceptionCode::InvalidStateError, "The HTMLImageElement provided is in the 'broken' state."_s };

    auto imageRect = FloatRect(FloatPoint(), size(imageElement, ImageSizeType::BeforeDevicePixelRatio));

    auto orientation = ImageOrientation::Orientation::FromImage;
    if (imageElement.allowsOrientationOverride()) {
        if (auto* renderer = imageElement.renderer())
            orientation = renderer->style().imageOrientation().orientation();
        else if (auto* computedStyle = imageElement.computedStyle())
            orientation = computedStyle->imageOrientation().orientation();
    }

    auto result = drawImage(imageElement.document(), *cachedImage, imageElement.renderer(), imageRect, srcRect, dstRect, op, blendMode, orientation);

    if (!result.hasException())
        checkOrigin(&imageElement);
    return result;
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(SVGImageElement& imageElement, const FloatRect& srcRect, const FloatRect& dstRect)
{
    return drawImage(imageElement, srcRect, dstRect, state().globalComposite, state().globalBlend);
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(SVGImageElement& imageElement, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator& op, const BlendMode& blendMode)
{
    auto* cachedImage = imageElement.cachedImage();
    if (!cachedImage)
        return { };

    if (cachedImage->status() == CachedImage::Status::DecodeError)
        return Exception { ExceptionCode::InvalidStateError, "The SVGImageElement provided is in the 'broken' state."_s };

    auto imageRect = FloatRect(FloatPoint(), size(imageElement, ImageSizeType::BeforeDevicePixelRatio));

    auto result = drawImage(imageElement.document(), *cachedImage, imageElement.renderer(), imageRect, srcRect, dstRect, op, blendMode);

    if (!result.hasException())
        checkOrigin(&imageElement);
    return result;
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(CSSStyleImageValue& image, const FloatRect& srcRect, const FloatRect& dstRect)
{
    auto* cachedImage = image.image();
    if (!cachedImage || !image.document())
        return { };
    FloatRect imageRect = FloatRect(FloatPoint(), size(image));

    auto result = drawImage(*image.document(), *cachedImage, nullptr, imageRect, srcRect, dstRect, state().globalComposite, state().globalBlend);

    if (!result.hasException())
        checkOrigin(image);
    return result;
}

#if ENABLE(WEB_CODECS)
ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(WebCodecsVideoFrame& frame, const FloatRect&, const FloatRect& dstRect)
{
    if (frame.isDetached())
        return Exception { ExceptionCode::InvalidStateError, "frame is detached"_s };

    auto* context = effectiveDrawingContext();
    if (!context)
        return { };

    auto internalFrame = frame.internalFrame();
    if (!internalFrame)
        return { };

    // FIXME: Add support for srcRect
    context->drawVideoFrame(*internalFrame, dstRect, ImageOrientation::Orientation::None, frame.shoudlDiscardAlpha());

    auto normalizedDstRect = normalizeRect(dstRect);
    bool repaintEntireCanvas = rectContainsCanvas(normalizedDstRect);
    // FIXME: Can we avoid post-processing in any cases?
    didDraw(repaintEntireCanvas, normalizedDstRect);

    return { };
}
#endif

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(Document& document, CachedImage& cachedImage, const RenderObject* renderer, const FloatRect& imageRect, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator& op, const BlendMode& blendMode, ImageOrientation orientation)
{
    if (!std::isfinite(dstRect.x()) || !std::isfinite(dstRect.y()) || !std::isfinite(dstRect.width()) || !std::isfinite(dstRect.height())
        || !std::isfinite(srcRect.x()) || !std::isfinite(srcRect.y()) || !std::isfinite(srcRect.width()) || !std::isfinite(srcRect.height()))
        return { };

    if (!srcRect.width() || !srcRect.height())
        return { };

    if (!dstRect.width() || !dstRect.height())
        return { };

    auto normalizedSrcRect = normalizeRect(srcRect);
    auto normalizedDstRect = normalizeRect(dstRect);

    if (normalizedSrcRect.isEmpty())
        return { };

    if (normalizedDstRect.isEmpty())
        return { };

    auto targetSwitcher = CanvasFilterContextSwitcher::create(*this, normalizedDstRect);

    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    RefPtr<Image> image = cachedImage.imageForRenderer(renderer);
    if (!image)
        return { };

    auto observer = image->imageObserver();
    auto shouldPostProcess { true };

    if (image->drawsSVGImage()) {
        image->setImageObserver(nullptr);
        image->setContainerSize(imageRect.size());
    }

    if (RefPtr bitmapImage = dynamicDowncast<BitmapImage>(*image)) {
        // Drawing an animated image to a canvas should draw the first frame (except for a few layout tests)
        if (image->isAnimated() && !document.settings().animatedImageDebugCanvasDrawingEnabled()) {
            bitmapImage = BitmapImage::create(image->nativeImage());
            if (!bitmapImage)
                return { };
            image = bitmapImage.copyRef();
        }

        shouldPostProcess = false;
    }

    ImagePaintingOptions options = {
        op,
        blendMode,
        orientation,
        document.settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
        document.settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No
    };

    bool repaintEntireCanvas = false;
    if (rectContainsCanvas(normalizedDstRect)) {
        c->drawImage(*image, normalizedDstRect, normalizedSrcRect, options);
        repaintEntireCanvas = true;
    } else if (isFullCanvasCompositeMode(op)) {
        fullCanvasCompositedDrawImage(*image, normalizedDstRect, normalizedSrcRect, op);
        repaintEntireCanvas = true;
    } else if (op == CompositeOperator::Copy) {
        clearCanvas();
        c->drawImage(*image, normalizedDstRect, normalizedSrcRect, options);
        repaintEntireCanvas = true;
    } else
        c->drawImage(*image, normalizedDstRect, normalizedSrcRect, options);

    didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : normalizedDstRect, shouldPostProcess ? defaultDidDrawOptions() : defaultDidDrawOptionsWithoutPostProcessing());

    if (image->drawsSVGImage())
        image->setImageObserver(WTFMove(observer));

    return { };
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(CanvasBase& sourceCanvas, const FloatRect& srcRect, const FloatRect& dstRect)
{
    FloatRect srcCanvasRect = FloatRect(FloatPoint(), sourceCanvas.size());

    if (!srcCanvasRect.width() || !srcCanvasRect.height())
        return Exception { ExceptionCode::InvalidStateError };

    if (!srcRect.width() || !srcRect.height())
        return { };

    auto normalizedSrcRect = normalizeRect(srcRect);
    auto normalizedDstRect = normalizeRect(dstRect);

    if (normalizedSrcRect.isEmpty())
        return { };

    if (normalizedDstRect.isEmpty())
        return { };

    auto targetSwitcher = CanvasFilterContextSwitcher::create(*this, normalizedDstRect);

    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    Ref protectedCanvas { sourceCanvas };
    checkOrigin(&sourceCanvas);

    RefPtr buffer = sourceCanvas.makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No);
    if (!buffer)
        return { };

    bool repaintEntireCanvas = false;
    if (rectContainsCanvas(normalizedDstRect)) {
        c->drawImageBuffer(*buffer, normalizedDstRect, normalizedSrcRect, { state().globalComposite, state().globalBlend });
        repaintEntireCanvas = true;
    } else if (isFullCanvasCompositeMode(state().globalComposite)) {
        fullCanvasCompositedDrawImage(*buffer, normalizedDstRect, normalizedSrcRect, state().globalComposite);
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        if (&sourceCanvas == &canvasBase()) {
            if (auto copy = c->createImageBuffer(normalizedSrcRect.size(), 1, colorSpace())) {
                copy->context().drawImageBuffer(*buffer, -normalizedSrcRect.location());
                clearCanvas();
                c->drawImageBuffer(*copy, normalizedDstRect, { { }, normalizedSrcRect.size() }, { state().globalComposite, state().globalBlend });
            }
        } else {
            clearCanvas();
            c->drawImageBuffer(*buffer, normalizedDstRect, normalizedSrcRect, { state().globalComposite, state().globalBlend });
        }
        repaintEntireCanvas = true;
    } else
        c->drawImageBuffer(*buffer, normalizedDstRect, normalizedSrcRect, { state().globalComposite, state().globalBlend });

    auto shouldUseDrawOptionsWithoutPostProcessing = sourceCanvas.renderingContext() && sourceCanvas.renderingContext()->is2d() && !sourceCanvas.havePendingCanvasNoiseInjection();
    didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : normalizedDstRect, shouldUseDrawOptionsWithoutPostProcessing ? defaultDidDrawOptionsWithoutPostProcessing() : defaultDidDrawOptions());

    return { };
}

#if ENABLE(VIDEO)

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(HTMLVideoElement& video, const FloatRect& srcRect, const FloatRect& dstRect)
{
    if (video.readyState() == HTMLMediaElement::HAVE_NOTHING || video.readyState() == HTMLMediaElement::HAVE_METADATA)
        return { };

    if (!srcRect.width() || !srcRect.height())
        return { };

    auto normalizedSrcRect = normalizeRect(srcRect);
    auto normalizedDstRect = normalizeRect(dstRect);

    if (normalizedSrcRect.isEmpty())
        return { };

    if (normalizedDstRect.isEmpty())
        return { };

    auto targetSwitcher = CanvasFilterContextSwitcher::create(*this, normalizedDstRect);

    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    checkOrigin(&video);

    bool repaintEntireCanvas = rectContainsCanvas(normalizedDstRect);

#if USE(CG)
    if (c->hasPlatformContext() && video.shouldGetNativeImageForCanvasDrawing()) {
        if (auto image = video.nativeImageForCurrentTime()) {
            c->drawNativeImage(*image, normalizedDstRect, normalizedSrcRect);

            didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : normalizedDstRect, defaultDidDrawOptionsWithoutPostProcessing());
            return { };
        }
    }
#endif

    GraphicsContextStateSaver stateSaver(*c);
    c->clip(normalizedDstRect);
    c->translate(normalizedDstRect.location());
    c->scale(FloatSize(normalizedDstRect.width() / normalizedSrcRect.width(), normalizedDstRect.height() / normalizedSrcRect.height()));
    c->translate(-normalizedSrcRect.location());
    video.paintCurrentFrameInContext(*c, FloatRect(FloatPoint(), size(video)));
    stateSaver.restore();

    didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : normalizedDstRect, defaultDidDrawOptionsWithoutPostProcessing());
    return { };
}

#endif

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(ImageBitmap& imageBitmap, const FloatRect& srcRect, const FloatRect& dstRect)
{
    if (!imageBitmap.width() || !imageBitmap.height())
        return Exception { ExceptionCode::InvalidStateError };

    auto normalizedSrcRect = normalizeRect(srcRect);

    if (normalizedSrcRect.isEmpty())
        return { };

    FloatRect srcBitmapRect = FloatRect(FloatPoint(), FloatSize(imageBitmap.width(), imageBitmap.height()));

    if (!srcBitmapRect.contains(normalizedSrcRect) || !dstRect.width() || !dstRect.height())
        return { };

    auto targetSwitcher = CanvasFilterContextSwitcher::create(*this, dstRect);

    GraphicsContext* c = effectiveDrawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    RefPtr buffer = imageBitmap.buffer();
    if (!buffer)
        return { };

    checkOrigin(&imageBitmap);

    bool repaintEntireCanvas = false;
    if (rectContainsCanvas(dstRect)) {
        c->drawImageBuffer(*buffer, dstRect, srcRect, { state().globalComposite, state().globalBlend });
        repaintEntireCanvas = true;
    } else if (isFullCanvasCompositeMode(state().globalComposite)) {
        fullCanvasCompositedDrawImage(*buffer, dstRect, srcRect, state().globalComposite);
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        clearCanvas();
        c->drawImageBuffer(*buffer, dstRect, srcRect, { state().globalComposite, state().globalBlend });
        repaintEntireCanvas = true;
    } else
        c->drawImageBuffer(*buffer, dstRect, srcRect, { state().globalComposite, state().globalBlend });

    didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : dstRect, defaultDidDrawOptionsWithoutPostProcessing());
    return { };
}

void CanvasRenderingContext2DBase::drawImageFromRect(HTMLImageElement& imageElement, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, const String& compositeOperation)
{
    CompositeOperator op;
    auto blendOp = BlendMode::Normal;
    if (!parseCompositeAndBlendOperator(compositeOperation, op, blendOp) || blendOp != BlendMode::Normal)
        op = CompositeOperator::SourceOver;
    drawImage(imageElement, FloatRect { sx, sy, sw, sh }, FloatRect { dx, dy, dw, dh }, op, BlendMode::Normal);
}

void CanvasRenderingContext2DBase::clearCanvas()
{
    auto* c = effectiveDrawingContext();
    if (!c)
        return;

    c->save();
    c->setCTM(baseTransform());
    c->clearRect(FloatRect(0, 0, canvasBase().width(), canvasBase().height()));
    c->restore();
}

Path CanvasRenderingContext2DBase::transformAreaToDevice(const Path& path) const
{
    Path transformed(path);
    transformed.transform(state().transform);
    transformed.transform(baseTransform());
    return transformed;
}

Path CanvasRenderingContext2DBase::transformAreaToDevice(const FloatRect& rect) const
{
    Path path;
    path.addRect(rect);
    return transformAreaToDevice(path);
}

bool CanvasRenderingContext2DBase::rectContainsCanvas(const FloatRect& rect) const
{
    FloatQuad quad(rect);
    FloatQuad canvasQuad(FloatRect(0, 0, canvasBase().width(), canvasBase().height()));
    return state().transform.mapQuad(quad).containsQuad(canvasQuad);
}

template<class T> IntRect CanvasRenderingContext2DBase::calculateCompositingBufferRect(const T& area, IntSize* croppedOffset)
{
    IntRect canvasRect(0, 0, canvasBase().width(), canvasBase().height());
    canvasRect = baseTransform().mapRect(canvasRect);
    Path path = transformAreaToDevice(area);
    IntRect bufferRect = enclosingIntRect(path.fastBoundingRect());
    IntPoint originalLocation = bufferRect.location();
    bufferRect.intersect(canvasRect);
    if (croppedOffset)
        *croppedOffset = originalLocation - bufferRect.location();
    return bufferRect;
}

void CanvasRenderingContext2DBase::compositeBuffer(ImageBuffer& buffer, const IntRect& bufferRect, CompositeOperator op)
{
    IntRect canvasRect(0, 0, canvasBase().width(), canvasBase().height());
    canvasRect = baseTransform().mapRect(canvasRect);

    auto* c = effectiveDrawingContext();
    if (!c)
        return;

    c->save();
    c->setCTM(AffineTransform());
    c->setCompositeOperation(op);

    c->save();
    c->clipOut(bufferRect);
    c->clearRect(canvasRect);
    c->restore();
    c->drawImageBuffer(buffer, bufferRect.location(), { state().globalComposite });
    c->restore();
}

static void drawImageToContext(Image& image, GraphicsContext& context, const FloatRect& dest, const FloatRect& src, ImagePaintingOptions options)
{
    context.drawImage(image, dest, src, options);
}

static void drawImageToContext(ImageBuffer& imageBuffer, GraphicsContext& context, const FloatRect& dest, const FloatRect& src, ImagePaintingOptions options)
{
    context.drawImageBuffer(imageBuffer, dest, src, options);
}

template<class T> void CanvasRenderingContext2DBase::fullCanvasCompositedDrawImage(T& image, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    ASSERT(isFullCanvasCompositeMode(op));

    IntSize croppedOffset;
    auto bufferRect = calculateCompositingBufferRect(dest, &croppedOffset);
    if (bufferRect.isEmpty()) {
        clearCanvas();
        return;
    }

    auto* c = effectiveDrawingContext();
    if (!c)
        return;

    auto buffer = c->createImageBuffer(bufferRect.size());
    if (!buffer)
        return;

    FloatRect adjustedDest = dest;
    adjustedDest.setLocation(FloatPoint(0, 0));
    AffineTransform effectiveTransform = c->getCTM();
    IntRect transformedAdjustedRect = enclosingIntRect(effectiveTransform.mapRect(adjustedDest));
    buffer->context().translate(-transformedAdjustedRect.location());
    buffer->context().translate(croppedOffset);
    buffer->context().concatCTM(effectiveTransform);
    drawImageToContext(image, buffer->context(), adjustedDest, src, { CompositeOperator::SourceOver });

    compositeBuffer(*buffer, bufferRect, op);
}

static CanvasRenderingContext2DBase::StyleVariant toStyleVariant(const CanvasStyle& style)
{
    return style.visit(
        [](const String& string) -> CanvasRenderingContext2DBase::StyleVariant { return string; },
        [](const Ref<CanvasGradient>& gradient) -> CanvasRenderingContext2DBase::StyleVariant { return gradient.ptr(); },
        [](const Ref<CanvasPattern>& pattern) -> CanvasRenderingContext2DBase::StyleVariant { return pattern.ptr(); }
    );
}

CanvasRenderingContext2DBase::StyleVariant CanvasRenderingContext2DBase::strokeStyle() const
{
    return toStyleVariant(state().strokeStyle);
}

void CanvasRenderingContext2DBase::setStrokeStyle(CanvasRenderingContext2DBase::StyleVariant&& style)
{
    WTF::switchOn(WTFMove(style),
        [this](String&& string) { this->setStrokeColor(WTFMove(string)); },
        [this](RefPtr<CanvasGradient>&& gradient) { this->setStrokeStyle(CanvasStyle(gradient.releaseNonNull())); },
        [this](RefPtr<CanvasPattern>&& pattern) { this->setStrokeStyle(CanvasStyle(pattern.releaseNonNull())); }
    );
}

CanvasRenderingContext2DBase::StyleVariant CanvasRenderingContext2DBase::fillStyle() const
{
    return toStyleVariant(state().fillStyle);
}

void CanvasRenderingContext2DBase::setFillStyle(CanvasRenderingContext2DBase::StyleVariant&& style)
{
    WTF::switchOn(WTFMove(style),
        [this](String&& string) { this->setFillColor(WTFMove(string)); },
        [this](RefPtr<CanvasGradient>&& gradient) { this->setFillStyle(CanvasStyle(gradient.releaseNonNull())); },
        [this](RefPtr<CanvasPattern>&& pattern) { this->setFillStyle(CanvasStyle(pattern.releaseNonNull())); }
    );
}

ExceptionOr<Ref<CanvasGradient>> CanvasRenderingContext2DBase::createLinearGradient(float x0, float y0, float x1, float y1)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1))
        return Exception { ExceptionCode::NotSupportedError };

    return CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
}

ExceptionOr<Ref<CanvasGradient>> CanvasRenderingContext2DBase::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(r0) || !std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(r1))
        return Exception { ExceptionCode::NotSupportedError };

    if (r0 < 0 || r1 < 0)
        return Exception { ExceptionCode::IndexSizeError };

    return CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
}

ExceptionOr<Ref<CanvasGradient>> CanvasRenderingContext2DBase::createConicGradient(float angleInRadians, float x, float y)
{
    if (!std::isfinite(angleInRadians) || !std::isfinite(x) || !std::isfinite(y))
        return Exception { ExceptionCode::NotSupportedError };

    // Angle starts from x-axis for consistency within canvas methods. See https://html.spec.whatwg.org/multipage/canvas.html#dom-context-2d-createconicgradient
    angleInRadians = normalizeAngleInRadians(angleInRadians) + piOverTwoFloat;
    return CanvasGradient::create(FloatPoint(x, y), angleInRadians);
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(CanvasImageSource&& image, const String& repetition)
{
    bool repeatX, repeatY;
    if (!CanvasPattern::parseRepetitionType(repetition, repeatX, repeatY))
        return Exception { ExceptionCode::SyntaxError };

    return WTF::switchOn(image,
        [&] (auto& element) -> ExceptionOr<RefPtr<CanvasPattern>> { return this->createPattern(*element, repeatX, repeatY); }
    );
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(CachedImage& cachedImage, RenderElement* renderer, bool repeatX, bool repeatY)
{
    bool originClean = cachedImage.isOriginClean(canvasBase().securityOrigin());

    // FIXME: SVG images with animations can switch between clean and dirty (leaking cross-origin
    // data). We should either:
    //   1) Take a fixed snapshot of an SVG image when creating a pattern and determine then whether
    //      the origin is clean.
    //   2) Dynamically verify the origin checks at draw time, and dirty the canvas accordingly.
    // To be on the safe side, taint the origin for all patterns containing SVG images for now.
    if (cachedImage.image()->drawsSVGImage())
        originClean = false;

    RefPtr image = cachedImage.imageForRenderer(renderer);
    if (!image)
        return Exception { ExceptionCode::InvalidStateError };

    RefPtr nativeImage = image->nativeImage();
    if (!nativeImage)
        return Exception { ExceptionCode::InvalidStateError };

    return RefPtr<CanvasPattern> { CanvasPattern::create({ nativeImage.releaseNonNull() }, repeatX, repeatY, originClean) };
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(HTMLImageElement& imageElement, bool repeatX, bool repeatY)
{
    CachedResourceHandle cachedImage = imageElement.cachedImage();
    
    // If the image loading hasn't started or the image is not complete, it is not fully decodable.
    if (!cachedImage || !imageElement.complete())
        return nullptr;

    if (cachedImage->status() == CachedResource::LoadError)
        return Exception { ExceptionCode::InvalidStateError };

    // Image may have a zero-width or a zero-height.
    Length intrinsicWidth;
    Length intrinsicHeight;
    FloatSize intrinsicRatio;
    cachedImage->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
    if (intrinsicWidth.isZero() || intrinsicHeight.isZero())
        return nullptr;

    return createPattern(*cachedImage, imageElement.renderer(), repeatX, repeatY);
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(SVGImageElement& imageElement, bool repeatX, bool repeatY)
{
    CachedResourceHandle cachedImage = imageElement.cachedImage();

    // The image loading hasn't started.
    if (!cachedImage)
        return nullptr;

    if (cachedImage->errorOccurred())
        return Exception { ExceptionCode::InvalidStateError };

    // The image loading hasn startedbut it is not complete.
    if (!cachedImage->image())
        return nullptr;

    // Image may have a zero-width or a zero-height.
    Length intrinsicWidth;
    Length intrinsicHeight;
    FloatSize intrinsicRatio;
    cachedImage->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
    if (intrinsicWidth.isZero() || intrinsicHeight.isZero())
        return nullptr;

    return createPattern(*cachedImage, imageElement.renderer(), repeatX, repeatY);
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(CanvasBase& canvas, bool repeatX, bool repeatY)
{
    if (!canvas.width() || !canvas.height())
        return Exception { ExceptionCode::InvalidStateError };
    auto* copiedImage = canvas.copiedImage();

    if (!copiedImage)
        return Exception { ExceptionCode::InvalidStateError };
    
    auto nativeImage = copiedImage->nativeImage();
    if (!nativeImage)
        return Exception { ExceptionCode::InvalidStateError };

    return RefPtr<CanvasPattern> { CanvasPattern::create({ nativeImage.releaseNonNull() }, repeatX, repeatY, canvas.originClean()) };
}
    
#if ENABLE(VIDEO)

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(HTMLVideoElement& videoElement, bool repeatX, bool repeatY)
{
    if (videoElement.readyState() < HTMLMediaElement::HAVE_CURRENT_DATA)
        return nullptr;
    
    checkOrigin(&videoElement);
    bool originClean = canvasBase().originClean();

#if USE(CG)
    if (auto nativeImage = videoElement.nativeImageForCurrentTime())
        return RefPtr<CanvasPattern> { CanvasPattern::create({ nativeImage.releaseNonNull() }, repeatX, repeatY, originClean) };
#endif

    auto renderingMode = drawingContext() ? drawingContext()->renderingMode() : RenderingMode::Unaccelerated;
    auto imageBuffer = videoElement.createBufferForPainting(size(videoElement), renderingMode, colorSpace(), pixelFormat());
    if (!imageBuffer)
        return nullptr;

    videoElement.paintCurrentFrameInContext(imageBuffer->context(), FloatRect(FloatPoint(), size(videoElement)));
    
    return RefPtr<CanvasPattern> { CanvasPattern::create({ imageBuffer.releaseNonNull() }, repeatX, repeatY, originClean) };
}

#endif

#if ENABLE(WEB_CODECS)
ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(WebCodecsVideoFrame& frame, bool repeatX, bool repeatY)
{
    UNUSED_PARAM(frame);
    UNUSED_PARAM(repeatX);
    UNUSED_PARAM(repeatY);
    // FIXME: Implement.
    return Exception { ExceptionCode::TypeError };
}
#endif

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(ImageBitmap& imageBitmap, bool repeatX, bool repeatY)
{
    RefPtr<ImageBuffer> buffer = imageBitmap.buffer();
    if (!buffer)
        return Exception { ExceptionCode::InvalidStateError };

    return RefPtr<CanvasPattern> { CanvasPattern::create({ buffer.releaseNonNull() }, repeatX, repeatY, imageBitmap.originClean()) };
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(CSSStyleImageValue&, bool, bool)
{
    // FIXME: Implement.
    return Exception { ExceptionCode::TypeError };
}

void CanvasRenderingContext2DBase::didDrawEntireCanvas(OptionSet<DidDrawOption> options)
{
    didDraw(backingStoreBounds(), options);
}

void CanvasRenderingContext2DBase::didDraw(std::optional<FloatRect> rect, OptionSet<DidDrawOption> options)
{
    if (!options.contains(DidDrawOption::PreserveCachedContents))
        m_cachedContents.emplace<CachedContentsUnknown>();

    auto* context = effectiveDrawingContext();
    if (!context)
        return;

    m_hasDeferredOperations = true;

    auto shouldApplyPostProcessing = options.contains(DidDrawOption::ApplyPostProcessing) ? ShouldApplyPostProcessingToDirtyRect::Yes : ShouldApplyPostProcessingToDirtyRect::No;

    if (!rect) {
        canvasBase().didDraw(std::nullopt, shouldApplyPostProcessing);
        return;
    }

    auto dirtyRect = rect.value();
    if (dirtyRect.isEmpty())
        return;

    if (!state().hasInvertibleTransform)
        return;

    if (options.contains(DidDrawOption::ApplyTransform))
        dirtyRect = state().transform.mapRect(dirtyRect);

    if (options.contains(DidDrawOption::ApplyShadow) && state().shadowColor.isVisible()) {
        // The shadow gets applied after transformation
        auto shadowRect = dirtyRect;
        shadowRect.move(state().shadowOffset);
        shadowRect.inflate(state().shadowBlur);
        dirtyRect.unite(shadowRect);
    }

    // FIXME: This does not apply the clip because we have no way of reading the clip out of the GraphicsContext.
    if (m_dirtyRect.contains(dirtyRect))
        canvasBase().didDraw(std::nullopt, shouldApplyPostProcessing);
    else {
        // Inflate dirty rect to cover antialiasing on image buffers.
        if (context->shouldAntialias())
            dirtyRect.inflate(1);
#if USE(COORDINATED_GRAPHICS)
        // In COORDINATED_GRAPHICS graphics layer is tiled and tiling logic handles dirty rects
        // internally and thus no unification of rects is needed here because that causes
        // unnecessary invalidation of tiles which are actually not dirty
        m_dirtyRect = dirtyRect;
#else
        m_dirtyRect.unite(dirtyRect);
#endif
        canvasBase().didDraw(m_dirtyRect, shouldApplyPostProcessing);
    }
}

void CanvasRenderingContext2DBase::didDraw(bool entireCanvas, const FloatRect& rect, OptionSet<DidDrawOption> options)
{
    return didDraw(entireCanvas, [&] {
        return rect;
    }, options);
}

template<typename RectProvider>
void CanvasRenderingContext2DBase::didDraw(bool entireCanvas, RectProvider rectProvider, OptionSet<DidDrawOption> options)
{
    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt, options);
    else if (entireCanvas) {
        OptionSet<DidDrawOption> didDrawEntireCanvasOptions { DidDrawOption::ApplyClip };
        if (options.contains(DidDrawOption::ApplyPostProcessing))
            didDrawEntireCanvasOptions.add(DidDrawOption::ApplyPostProcessing);
        didDrawEntireCanvas(didDrawEntireCanvasOptions);
    } else
        didDraw(rectProvider(), options);
}

void CanvasRenderingContext2DBase::clearAccumulatedDirtyRect()
{
    m_dirtyRect = { };
}

bool CanvasRenderingContext2DBase::isEntireBackingStoreDirty() const
{
    return m_dirtyRect == backingStoreBounds();
}

const Vector<CanvasRenderingContext2DBase::State, 1>& CanvasRenderingContext2DBase::stateStack()
{
    realizeSaves();
    return m_stateStack;
}

GraphicsContext* CanvasRenderingContext2DBase::drawingContext() const
{
    if (auto* paintContext = dynamicDowncast<PaintRenderingContext2D>(*this))
        return paintContext->ensureDrawingContext();
    if (auto* buffer = canvasBase().buffer())
        return &buffer->context();
    return nullptr;
}

GraphicsContext* CanvasRenderingContext2DBase::existingDrawingContext() const
{
    if (!canvasBase().hasCreatedImageBuffer())
        return nullptr;
    return drawingContext();
}

GraphicsContext* CanvasRenderingContext2DBase::effectiveDrawingContext() const
{
    auto context = drawingContext();
    if (!context)
        return nullptr;
    if (auto targetSwitcher = state().targetSwitcher)
        return targetSwitcher->drawingContext();
    return context;
}

AffineTransform CanvasRenderingContext2DBase::baseTransform() const
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=275100): The image buffer from CanvasBase should be moved to CanvasRenderingContext2DBase.
    ASSERT(canvasBase().hasCreatedImageBuffer());
    return canvasBase().buffer()->baseTransform();
}

void CanvasRenderingContext2DBase::prepareForDisplay()
{
    if (auto buffer = canvasBase().buffer())
        buffer->flushDrawingContextAsync();
}

bool CanvasRenderingContext2DBase::needsPreparationForDisplay() const
{
    return false;
}

static void initializeEmptyImageData(const ImageData& imageData)
{
    imageData.data().zeroFill();
}

ExceptionOr<Ref<ImageData>> CanvasRenderingContext2DBase::createImageData(ImageData& existingImageData) const
{
    auto newImageData = ImageData::createUninitialized(existingImageData.width(), existingImageData.height(), existingImageData.colorSpace());
    if (!newImageData.hasException())
        initializeEmptyImageData(newImageData.returnValue());
    return newImageData;
}

ExceptionOr<Ref<ImageData>> CanvasRenderingContext2DBase::createImageData(int sw, int sh, std::optional<ImageDataSettings> settings) const
{
    if (!sw || !sh)
        return Exception { ExceptionCode::IndexSizeError };

    auto imageData = ImageData::createUninitialized(std::abs(sw), std::abs(sh), m_settings.colorSpace, settings);
    if (!imageData.hasException())
        initializeEmptyImageData(imageData.returnValue());
    return imageData;
}

void CanvasRenderingContext2DBase::evictCachedImageData()
{
    m_cachedContents.emplace<CachedContentsUnknown>();
}

CanvasRenderingContext2DBase::CachedContentsImageData::CachedContentsImageData(CanvasRenderingContext2DBase& context, Ref<ByteArrayPixelBuffer> imageData)
    : imageData(WTFMove(imageData))
    , evictionTimer(context, &CanvasRenderingContext2DBase::evictCachedImageData, 5_s)
{
}

static constexpr unsigned imageDataSizeThresholdForCaching = 60 * 60;

RefPtr<ByteArrayPixelBuffer> CanvasRenderingContext2DBase::cacheImageDataIfPossible(const ImageData& imageData, const IntRect& sourceRect, const IntPoint& destinationPosition)
{
    if (!destinationPosition.isZero() || !sourceRect.location().isZero() || sourceRect.size() != imageData.size() || sourceRect.size() != canvasBase().size())
        return nullptr;

    auto size = imageData.size();
    if (size.area() > imageDataSizeThresholdForCaching)
        return nullptr;

    if (imageData.colorSpace() != m_settings.colorSpace)
        return nullptr;

    // Consider:
    //   * Real putImageData needs premultiply step.
    //   * Retrieve from cache needs to ensure premultiply + unpremultiply was made to simulate the real putImageData.
    //   * Caching needs at least a memcpy here.
    // Instead of doing the plain memcpy, copy by doing premultiply here.
    // This computation can be used for cache retrieval as well as the real putImageData.
    // We're not doing RGBA -> BGRA swizzle here, as that is not needed for cache retrieval and
    // the swizzle copy can be made at the putImageData copy site.
    auto colorSpace = toDestinationColorSpace(imageData.colorSpace());
    unsigned bytesPerRow = static_cast<unsigned>(size.width()) * 4u;
    PixelBufferFormat cachedFormat { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, colorSpace };
    auto cachedBuffer = ByteArrayPixelBuffer::tryCreate(cachedFormat, size);
    if (!cachedBuffer)
        return nullptr;
    ConstPixelBufferConversionView source {
        .format = { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, colorSpace },
        .bytesPerRow = bytesPerRow,
        .rows = imageData.data().data(),
    };
    PixelBufferConversionView destination {
        .format = cachedFormat,
        .bytesPerRow = bytesPerRow,
        .rows = cachedBuffer->data().data(),
    };
    convertImagePixels(source, destination, size);
    m_cachedContents.emplace<CachedContentsImageData>(*this, *cachedBuffer);
    return cachedBuffer;
}

RefPtr<ImageData> CanvasRenderingContext2DBase::makeImageDataIfContentsCached(const IntRect& sourceRect, PredefinedColorSpace colorSpace) const
{
    if (std::holds_alternative<CachedContentsTransparent>(m_cachedContents)) {
        auto imageData = ImageData::create(sourceRect.size(), colorSpace);
        if (imageData)
            imageData->data().zeroFill();
        return imageData;
    }
    if (std::holds_alternative<CachedContentsUnknown>(m_cachedContents))
        return nullptr;
    static_assert(std::variant_size_v<decltype(m_cachedContents)> == 3); // Written this way to avoid dangling references during visit.
    // Always consume the cached image data.
    Ref pixelBuffer = WTFMove(std::get<CachedContentsImageData>(m_cachedContents).imageData);
    m_cachedContents.emplace<CachedContentsUnknown>();

    if (sourceRect != IntRect { { }, canvasBase().size() })
        return nullptr;

    if (canvasBase().size() != pixelBuffer->size())
        return nullptr;

    if (colorSpace != m_settings.colorSpace)
        return nullptr;

    auto size = pixelBuffer->size();
    auto data = pixelBuffer->takeData();
    unsigned bytesPerRow = static_cast<unsigned>(size.width()) * 4u;
    ConstPixelBufferConversionView source {
        .format = pixelBuffer->format(),
        .bytesPerRow = bytesPerRow,
        .rows = data->data(),
    };
    PixelBufferConversionView destination {
        .format = { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, pixelBuffer->format().colorSpace },
        .bytesPerRow = bytesPerRow,
        .rows = data->data(),
    };
    convertImagePixels(source, destination, size);
    return ImageData::create(size, WTFMove(data), m_settings.colorSpace);
}

ExceptionOr<Ref<ImageData>> CanvasRenderingContext2DBase::getImageData(int sx, int sy, int sw, int sh, std::optional<ImageDataSettings> settings) const
{
    if (!sw || !sh)
        return Exception { ExceptionCode::IndexSizeError };

    RefPtr scriptContext = canvasBase().scriptExecutionContext();
    if (!canvasBase().originClean()) {
        static NeverDestroyed<String> consoleMessage(MAKE_STATIC_STRING_IMPL("Unable to get image data from canvas because the canvas has been tainted by cross-origin data."));
        scriptContext->addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage);
        return Exception { ExceptionCode::SecurityError };
    }

    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }    
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }

    IntRect imageDataRect { sx, sy, sw, sh };

    if (scriptContext && scriptContext->requiresScriptExecutionTelemetry(ScriptTelemetryCategory::Canvas)) {
        RefPtr buffer = canvasBase().createImageForNoiseInjection();
        if (!buffer)
            return Exception { ExceptionCode::InvalidStateError };

        auto format = PixelBufferFormat { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, buffer->colorSpace() };
        RefPtr pixelBuffer = dynamicDowncast<ByteArrayPixelBuffer>(buffer->getPixelBuffer(format, imageDataRect));
        if (!pixelBuffer)
            return Exception { ExceptionCode::InvalidStateError };

        return { { ImageData::create(pixelBuffer.releaseNonNull()) } };
    }

    auto computedColorSpace = ImageData::computeColorSpace(settings, m_settings.colorSpace);

    if (auto imageData = makeImageDataIfContentsCached(imageDataRect, computedColorSpace))
        return imageData.releaseNonNull();

    RefPtr<ImageBuffer> buffer = canvasBase().makeRenderingResultsAvailable();
    if (!buffer) {
        auto imageData = ImageData::createUninitialized(imageDataRect.width(), imageDataRect.height(), m_settings.colorSpace, settings);
        if (!imageData.hasException())
            initializeEmptyImageData(imageData.returnValue());
        return imageData;
    }

    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, toDestinationColorSpace(computedColorSpace) };
    RefPtr pixelBuffer = dynamicDowncast<ByteArrayPixelBuffer>(buffer->getPixelBuffer(format, imageDataRect));
    if (!pixelBuffer) {
        scriptContext->addConsoleMessage(MessageSource::Rendering, MessageLevel::Error,
            makeString("Unable to get image data from canvas. Requested size was "_s, imageDataRect.width(), " x "_s, imageDataRect.height()));
        return Exception { ExceptionCode::InvalidStateError };
    }

    ASSERT(pixelBuffer->format().colorSpace == toDestinationColorSpace(computedColorSpace));

    return { { ImageData::create(pixelBuffer.releaseNonNull()) } };
}

void CanvasRenderingContext2DBase::putImageData(ImageData& data, int dx, int dy)
{
    putImageData(data, dx, dy, 0, 0, data.width(), data.height());
}

void CanvasRenderingContext2DBase::putImageData(ImageData& data, int dx, int dy, int dirtyX, int dirtyY, int dirtyWidth, int dirtyHeight)
{
    RefPtr buffer = canvasBase().buffer();
    if (!buffer)
        return;

    if (data.data().isDetached())
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    IntPoint destOffset { dx, dy };
    IntRect destRect { dirtyX, dirtyY, dirtyWidth, dirtyHeight };
    IntRect sourceRect = computeImageDataRect(*buffer, data.size(), destRect, destOffset);

    OptionSet<DidDrawOption> options; // ignore transform, shadow, clip, and post-processing
    if (!sourceRect.isEmpty()) {
        auto pixelBuffer = cacheImageDataIfPossible(data, sourceRect, destOffset);
        if (pixelBuffer)
            options.add(DidDrawOption::PreserveCachedContents);
        else
            pixelBuffer = data.pixelBuffer();
        buffer->putPixelBuffer(*pixelBuffer, sourceRect, destOffset);
    }

    didDraw(FloatRect { destRect }, options);
}

FloatRect CanvasRenderingContext2DBase::inflatedStrokeRect(const FloatRect& rect) const
{
    // Fast approximation of the stroke's bounding rect.
    // This yields a slightly oversized rect but is very fast
    // compared to Path::strokeBoundingRect().
    static const float root2 = sqrtf(2);
    float delta = state().lineWidth / 2;
    if (state().lineJoin == LineJoin::Miter)
        delta *= state().miterLimit;
    else if (state().lineCap == LineCap::Square)
        delta *= root2;
    auto inflatedStrokeRect = rect;
    inflatedStrokeRect.inflate(delta);
    return inflatedStrokeRect;
}

static inline InterpolationQuality smoothingToInterpolationQuality(ImageSmoothingQuality quality)
{
    switch (quality) {
    case ImageSmoothingQuality::Low:
        return InterpolationQuality::Low;
    case ImageSmoothingQuality::Medium:
        return InterpolationQuality::Medium;
    case ImageSmoothingQuality::High:
        return InterpolationQuality::High;
    }

    ASSERT_NOT_REACHED();
    return InterpolationQuality::Low;
};

void CanvasRenderingContext2DBase::setImageSmoothingQuality(ImageSmoothingQuality quality)
{
    if (quality == state().imageSmoothingQuality)
        return;

    realizeSaves();
    modifiableState().imageSmoothingQuality = quality;

    if (!state().imageSmoothingEnabled)
        return;

    if (auto* context = effectiveDrawingContext())
        context->setImageInterpolationQuality(smoothingToInterpolationQuality(quality));
}

void CanvasRenderingContext2DBase::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().imageSmoothingEnabled)
        return;

    realizeSaves();
    modifiableState().imageSmoothingEnabled = enabled;
    auto* c = effectiveDrawingContext();
    if (c)
        c->setImageInterpolationQuality(enabled ? smoothingToInterpolationQuality(state().imageSmoothingQuality) : InterpolationQuality::DoNotInterpolate);
}

void CanvasRenderingContext2DBase::setPath(Path2D& path)
{
    m_path = path.path();
}

Ref<Path2D> CanvasRenderingContext2DBase::getPath() const
{
    return Path2D::create(m_path);
}

void CanvasRenderingContext2DBase::setTextAlign(CanvasTextAlign canvasTextAlign)
{
    auto textAlign = fromCanvasTextAlign(canvasTextAlign);
    if (state().textAlign == textAlign)
        return;
    realizeSaves();
    modifiableState().textAlign = textAlign;
}

void CanvasRenderingContext2DBase::setTextBaseline(CanvasTextBaseline canvasTextBaseline)
{
    auto textBaseline = fromCanvasTextBaseline(canvasTextBaseline);
    if (state().textBaseline == textBaseline)
        return;
    realizeSaves();
    modifiableState().textBaseline = textBaseline;
}

void CanvasRenderingContext2DBase::setDirection(Direction direction)
{
    if (state().direction == direction)
        return;

    realizeSaves();
    modifiableState().direction = direction;
}

bool CanvasRenderingContext2DBase::canDrawText(double x, double y, bool fill, std::optional<double> maxWidth)
{
    if (!fontProxy()->realized())
        return false;

    auto* c = effectiveDrawingContext();
    if (!c)
        return false;
    if (!state().hasInvertibleTransform)
        return false;
    if (!std::isfinite(x) || !std::isfinite(y))
        return false;
    if (maxWidth && (!std::isfinite(maxWidth.value()) || maxWidth.value() <= 0))
        return false;

    // If gradient size is zero, nothing would be painted.
    auto gradient = c->strokeGradient();
    if (!fill && gradient && gradient->isZeroSize())
        return false;

    gradient = c->fillGradient();
    if (fill && gradient && gradient->isZeroSize())
        return false;

    return true;
}

static inline bool isSpaceThatNeedsReplacing(UChar c)
{
    // According to specification all space characters should be replaced with 0x0020 space character.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-canvas-element.html#text-preparation-algorithm
    // The space characters according to specification are : U+0020, U+0009, U+000A, U+000C, and U+000D.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#space-character
    // This function returns true for 0x000B also, so that this is backward compatible.
    // Otherwise, the test LayoutTests/canvas/philip/tests/2d.text.draw.space.collapse.space.html will fail
    return c == 0x0009 || c == 0x000A || c == 0x000B || c == 0x000C || c == 0x000D;
}

String CanvasRenderingContext2DBase::normalizeSpaces(const String& text)
{
    size_t i = text.find(isSpaceThatNeedsReplacing);
    if (i == notFound)
        return text;

    unsigned textLength = text.length();
    Vector<UChar> charVector(textLength);
    StringView(text).getCharacters(charVector.mutableSpan());

    charVector[i++] = ' ';

    for (; i < textLength; ++i) {
        if (isSpaceThatNeedsReplacing(charVector[i]))
            charVector[i] = ' ';
    }
    return String::adopt(WTFMove(charVector));
}

void CanvasRenderingContext2DBase::drawText(const String& text, double x, double y, bool fill, std::optional<double> maxWidth)
{
    if (!canDrawText(x, y, fill, maxWidth))
        return;

    String normalizedText = normalizeSpaces(text);
    auto direction = (state().direction == Direction::Rtl) ? TextDirection::RTL : TextDirection::LTR;
    TextRun textRun(normalizedText, 0, 0, ExpansionBehavior::allowRightOnly(), direction, false, true);
    drawTextUnchecked(textRun, x, y, fill, maxWidth);
}

void CanvasRenderingContext2DBase::drawTextUnchecked(const TextRun& textRun, double x, double y, bool fill, std::optional<double> maxWidth)
{
    auto measureTextRun = [&](const TextRun& textRun) -> std::tuple<float, FontMetrics>  {
        auto& fontProxy = *this->fontProxy();

        // FIXME: Need to turn off font smoothing.
        return { fontProxy.width(textRun), fontProxy.metricsOfPrimaryFont() };
    };

    auto [fontWidth, fontMetrics] = measureTextRun(textRun);
    bool useMaxWidth = maxWidth && maxWidth.value() < fontWidth;
    float width = useMaxWidth ? maxWidth.value() : fontWidth;
    FloatPoint location(x, y);
    location += textOffset(width, textRun.direction());

    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    FloatRect textRect = FloatRect(location.x() - fontMetrics.intHeight() / 2, location.y() - fontMetrics.intAscent() - fontMetrics.intLineGap(),
        width + fontMetrics.intHeight(), fontMetrics.intLineSpacing());
    if (!fill)
        textRect = inflatedStrokeRect(textRect);

    auto targetSwitcher = CanvasFilterContextSwitcher::create(*this, textRect);

    // FIXME: Need to refetch fontProxy. CanvasFilterContextSwitcher might have called save().
    // https://bugs.webkit.org/show_bug.cgi?id=193077.
    auto* c = effectiveDrawingContext();
    auto& fontProxy = *this->fontProxy();

#if USE(CG)
    const CanvasStyle& drawStyle = fill ? state().fillStyle : state().strokeStyle;
    if (drawStyle.canvasGradient() || drawStyle.canvasPattern()) {
        IntRect maskRect = enclosingIntRect(textRect);

        // If we have a shadow, we need to draw it before the mask operation.
        // Follow a procedure similar to paintTextWithShadows in TextPainter.

        if (shouldDrawShadows()) {
            GraphicsContextStateSaver stateSaver(*c);

            FloatSize offset(0, 2 * maskRect.height());

            auto shadow = c->dropShadow();
            ASSERT(shadow);

            FloatRect shadowRect(maskRect);
            shadowRect.inflate(shadow->radius * 1.4);
            shadowRect.move(shadow->offset * -1);
            c->clip(shadowRect);

            c->setDropShadow({ shadow->offset + offset, shadow->radius, shadow->color, ShadowRadiusMode::Legacy });

            if (fill)
                c->setFillColor(Color::black);
            else
                c->setStrokeColor(Color::black);

            fontProxy.drawBidiText(*c, textRun, location + offset, FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady);
        }

        auto maskImage = c->createAlignedImageBuffer(maskRect.size());
        if (!maskImage)
            return;

        auto& maskImageContext = maskImage->context();

        if (fill)
            maskImageContext.setFillColor(Color::black);
        else {
            maskImageContext.setStrokeColor(Color::black);
            maskImageContext.setStrokeThickness(c->strokeThickness());
        }

        maskImageContext.setTextDrawingMode(fill ? TextDrawingMode::Fill : TextDrawingMode::Stroke);

        if (useMaxWidth) {
            maskImageContext.translate(location - maskRect.location());
            // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
            maskImageContext.scale(FloatSize((fontWidth > 0 ? (width / fontWidth) : 0), 1));
            fontProxy.drawBidiText(maskImageContext, textRun, FloatPoint(0, 0), FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady);
        } else {
            maskImageContext.translate(-maskRect.location());
            fontProxy.drawBidiText(maskImageContext, textRun, location, FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady);
        }

        GraphicsContextStateSaver stateSaver(*c);
        c->clipToImageBuffer(*maskImage, maskRect);
        drawStyle.applyFillColor(*c);
        c->fillRect(maskRect);
        didDraw(false, FloatRect { maskRect });
        return;
    }
#endif

    c->setTextDrawingMode(fill ? TextDrawingMode::Fill : TextDrawingMode::Stroke);

    GraphicsContextStateSaver stateSaver(*c);
    if (useMaxWidth) {
        c->translate(location);
        // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
        c->scale(FloatSize((fontWidth > 0 ? (width / fontWidth) : 0), 1));
        location = FloatPoint();
    }

    bool repaintEntireCanvas = false;
    if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady);
        endCompositeLayer();
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        clearCanvas();
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady);
        repaintEntireCanvas = true;
    } else
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady);

    didDraw(repaintEntireCanvas, targetSwitcher ? targetSwitcher->expandedBounds() : textRect);
}

Ref<TextMetrics> CanvasRenderingContext2DBase::measureTextInternal(const String& text)
{
    String normalizedText = normalizeSpaces(text);
    auto direction = (state().direction == Direction::Rtl) ? TextDirection::RTL : TextDirection::LTR;
    TextRun textRun(normalizedText, 0, 0, ExpansionBehavior::allowRightOnly(), direction, false, true);
    return measureTextInternal(textRun);
}

Ref<TextMetrics> CanvasRenderingContext2DBase::measureTextInternal(const TextRun& textRun)
{
    Ref<TextMetrics> metrics = TextMetrics::create();

    auto& font = *fontProxy();
    auto& fontMetrics = font.metricsOfPrimaryFont();

    GlyphOverflow glyphOverflow;
    glyphOverflow.computeBounds = true;
    float fontWidth = font.width(textRun, &glyphOverflow);
    metrics->setWidth(fontWidth);

    FloatPoint offset = textOffset(fontWidth, textRun.direction());
    int ascent = fontMetrics.intAscent();
    int descent = fontMetrics.intDescent();

    metrics->setActualBoundingBoxAscent(glyphOverflow.top - offset.y());
    metrics->setActualBoundingBoxDescent(glyphOverflow.bottom + offset.y());
    metrics->setFontBoundingBoxAscent(ascent - offset.y());
    metrics->setFontBoundingBoxDescent(descent + offset.y());
    metrics->setEmHeightAscent(ascent - offset.y());
    metrics->setEmHeightDescent(descent + offset.y());
    metrics->setHangingBaseline(ascent - offset.y());
    metrics->setAlphabeticBaseline(-offset.y());
    metrics->setIdeographicBaseline(-descent - offset.y());

    metrics->setActualBoundingBoxLeft(glyphOverflow.left - offset.x());
    metrics->setActualBoundingBoxRight(fontWidth + glyphOverflow.right + offset.x());

    return metrics;
}

FloatPoint CanvasRenderingContext2DBase::textOffset(float width, TextDirection direction)
{
    auto& fontMetrics = fontProxy()->metricsOfPrimaryFont();
    FloatPoint offset;

    switch (state().textBaseline) {
    case TopTextBaseline:
    case HangingTextBaseline:
        offset.setY(fontMetrics.intAscent());
        break;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        offset.setY(-fontMetrics.intDescent());
        break;
    case MiddleTextBaseline:
        offset.setY(fontMetrics.intHeight() / 2 - fontMetrics.intDescent());
        break;
    case AlphabeticTextBaseline:
    default:
        break;
    }

    bool isRTL = direction == TextDirection::RTL;
    auto align = state().textAlign;
    if (align == StartTextAlign)
        align = isRTL ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = isRTL ? LeftTextAlign : RightTextAlign;

    switch (align) {
    case CenterTextAlign:
        offset.setX(-width / 2);
        break;
    case RightTextAlign:
        offset.setX(-width);
        break;
    default:
        break;
    }
    return offset;
}

ImageBufferPixelFormat CanvasRenderingContext2DBase::pixelFormat() const
{
    // FIXME: Take m_settings.alpha into account here and add PixelFormat::BGRX8.
    return ImageBufferPixelFormat::BGRA8;
}

DestinationColorSpace CanvasRenderingContext2DBase::colorSpace() const
{
    return toDestinationColorSpace(m_settings.colorSpace);
}

bool CanvasRenderingContext2DBase::willReadFrequently() const
{
    return m_settings.willReadFrequently;
}

OptionSet<ImageBufferOptions> CanvasRenderingContext2DBase::adjustImageBufferOptionsForTesting(OptionSet<ImageBufferOptions> bufferOptions)
{
    if (!m_settings.renderingModeForTesting)
        return bufferOptions;
    switch (*m_settings.renderingModeForTesting) {
    case CanvasRenderingContext2DSettings::RenderingMode::Unaccelerated:
        bufferOptions.remove(ImageBufferOptions::Accelerated);
        bufferOptions.add(ImageBufferOptions::AvoidBackendSizeCheckForTesting);
        break;
    case CanvasRenderingContext2DSettings::RenderingMode::Accelerated:
        bufferOptions.add(ImageBufferOptions::Accelerated);
        bufferOptions.add(ImageBufferOptions::AvoidBackendSizeCheckForTesting);
        break;
    }
    return bufferOptions;
}

std::optional<CanvasRenderingContext2DBase::RenderingMode> CanvasRenderingContext2DBase::getEffectiveRenderingModeForTesting()
{
    if (auto* buffer = canvasBase().buffer()) {
        buffer->ensureBackendCreated();
        if (buffer->hasBackend())
            return buffer->renderingMode();
    }
    return std::nullopt;
}

} // namespace WebCore
