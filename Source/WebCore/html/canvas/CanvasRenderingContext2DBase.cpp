/*
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "ColorConversion.h"
#include "ColorSerialization.h"
#include "DOMMatrix.h"
#include "DOMMatrix2DInit.h"
#include "DisplayListDrawingContext.h"
#include "DisplayListRecorder.h"
#include "DisplayListReplayer.h"
#include "FloatQuad.h"
#include "Gradient.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "OffscreenCanvas.h"
#include "Path2D.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "TextMetrics.h"
#include "TextRun.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(CanvasRenderingContext2DBase);

static constexpr ImageSmoothingQuality defaultSmoothingQuality = ImageSmoothingQuality::Low;

const int CanvasRenderingContext2DBase::DefaultFontSize = 10;
const char* const CanvasRenderingContext2DBase::DefaultFontFamily = "sans-serif";
static constexpr ASCIILiteral DefaultFont = "10px sans-serif"_s;

static CanvasLineCap toCanvasLineCap(LineCap lineCap)
{
    switch (lineCap) {
    case ButtCap:
        return CanvasLineCap::Butt;
    case RoundCap:
        return CanvasLineCap::Round;
    case SquareCap:
        return CanvasLineCap::Square;
    }
    ASSERT_NOT_REACHED();
    return CanvasLineCap::Butt;
}

static LineCap fromCanvasLineCap(CanvasLineCap canvasLineCap)
{
    switch (canvasLineCap) {
    case CanvasLineCap::Butt:
        return ButtCap;
    case CanvasLineCap::Round:
        return RoundCap;
    case CanvasLineCap::Square:
        return SquareCap;
    }
    ASSERT_NOT_REACHED();
    return ButtCap;
}

static CanvasLineJoin toCanvasLineJoin(LineJoin lineJoin)
{
    switch (lineJoin) {
    case RoundJoin:
        return CanvasLineJoin::Round;
    case BevelJoin:
        return CanvasLineJoin::Bevel;
    case MiterJoin:
        return CanvasLineJoin::Miter;
    }
    ASSERT_NOT_REACHED();
    return CanvasLineJoin::Round;
}

static LineJoin fromCanvasLineJoin(CanvasLineJoin canvasLineJoin)
{
    switch (canvasLineJoin) {
    case CanvasLineJoin::Round:
        return RoundJoin;
    case CanvasLineJoin::Bevel:
        return BevelJoin;
    case CanvasLineJoin::Miter:
        return MiterJoin;
    }
    ASSERT_NOT_REACHED();
    return RoundJoin;
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

CanvasRenderingContext2DBase::CanvasRenderingContext2DBase(CanvasBase& canvas, CanvasRenderingContext2DSettings&& settings, bool usesCSSCompatibilityParseMode)
    : CanvasRenderingContext(canvas)
    , m_stateStack(1)
    , m_usesCSSCompatibilityParseMode(usesCSSCompatibilityParseMode)
    , m_settings(WTFMove(settings))
{
}

void CanvasRenderingContext2DBase::unwindStateStack()
{
    // Ensure that the state stack in the ImageBuffer's context
    // is cleared before destruction, to avoid assertions in the
    // GraphicsContext dtor.
    if (size_t stackSize = m_stateStack.size()) {
        if (auto* context = canvasBase().existingDrawingContext()) {
            while (--stackSize)
                context->restore();
        }
    }
}

CanvasRenderingContext2DBase::~CanvasRenderingContext2DBase()
{
#if ASSERT_ENABLED
    unwindStateStack();
#endif
}

bool CanvasRenderingContext2DBase::isAccelerated() const
{
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    auto* context = canvasBase().existingDrawingContext();
    return context && context->renderingMode() == RenderingMode::Accelerated;
#else
    return false;
#endif
}

void CanvasRenderingContext2DBase::reset()
{
    unwindStateStack();
    m_stateStack.resize(1);
    m_stateStack.first() = State();
    m_path.clear();
    m_unrealizedSaveCount = 0;
    
    m_recordingContext = nullptr;
}

CanvasRenderingContext2DBase::State::State()
    : strokeStyle(Color::black)
    , fillStyle(Color::black)
    , lineWidth(1)
    , lineCap(ButtCap)
    , lineJoin(MiterJoin)
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
    , unparsedFont(DefaultFont)
{
}

String CanvasRenderingContext2DBase::State::fontString() const
{
    if (!font.realized())
        return DefaultFont;

    StringBuilder serializedFont;
    const auto& font = this->font.fontDescription();

    auto italic = font.italic() ? "italic " : "";
    auto smallCaps = font.variantCaps() == FontVariantCaps::Small ? "small-caps " : "";
    serializedFont.append(italic, smallCaps, font.computedPixelSize(), "px");

    for (unsigned i = 0; i < font.familyCount(); ++i) {
        StringView family = font.familyAt(i);
        if (family.startsWith("-webkit-"))
            family = family.substring(8);

        auto separator = i ? ", " : " ";
        auto quote = family.contains(' ') ? "\"" : "";
        serializedFont.append(separator, quote, family, quote);
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

const FontMetrics& CanvasRenderingContext2DBase::FontProxy::fontMetrics() const
{
    return m_font.fontMetrics();
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

void CanvasRenderingContext2DBase::setStrokeStyle(CanvasStyle style)
{
    if (!style.isValid())
        return;

    if (state().strokeStyle.isEquivalentColor(style))
        return;

    if (style.isCurrentColor())
        style = CanvasStyle(currentColor(canvasBase()).colorWithAlpha(style.overrideAlpha()));
    else
        checkOrigin(style.canvasPattern().get());

    realizeSaves();
    State& state = modifiableState();
    state.strokeStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state.strokeStyle.applyStrokeColor(*c);
    state.unparsedStrokeColor = String();
}

void CanvasRenderingContext2DBase::setFillStyle(CanvasStyle style)
{
    if (!style.isValid())
        return;

    if (state().fillStyle.isEquivalentColor(style))
        return;

    if (style.isCurrentColor())
        style = CanvasStyle(currentColor(canvasBase()).colorWithAlpha(style.overrideAlpha()));
    else
        checkOrigin(style.canvasPattern().get());

    realizeSaves();
    State& state = modifiableState();
    state.fillStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state.fillStyle.applyFillColor(*c);
    state.unparsedFillColor = String();
}

void CanvasRenderingContext2DBase::setLineWidth(double width)
{
    if (!(std::isfinite(width) && width > 0))
        return;
    if (state().lineWidth == width)
        return;
    realizeSaves();
    modifiableState().lineWidth = width;
    GraphicsContext* c = drawingContext();
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
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineCap(lineCap);
}

void CanvasRenderingContext2DBase::setLineCap(const String& stringValue)
{
    CanvasLineCap cap;
    if (stringValue == "butt")
        cap = CanvasLineCap::Butt;
    else if (stringValue == "round")
        cap = CanvasLineCap::Round;
    else if (stringValue == "square")
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
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineJoin(lineJoin);
}

void CanvasRenderingContext2DBase::setLineJoin(const String& stringValue)
{
    CanvasLineJoin join;
    if (stringValue == "round")
        join = CanvasLineJoin::Round;
    else if (stringValue == "bevel")
        join = CanvasLineJoin::Bevel;
    else if (stringValue == "miter")
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
    GraphicsContext* c = drawingContext();
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
    Color color = parseColorOrCurrentColor(colorString, canvasBase());
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
    GraphicsContext* c = drawingContext();
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
    GraphicsContext* c = drawingContext();
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
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setCompositeOperation(op, blendMode);
}

void CanvasRenderingContext2DBase::scale(double sx, double sy)
{
    GraphicsContext* c = drawingContext();
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
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    if (!std::isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().transform;
    newTransform.rotate(angleInRadians / piDouble * 180.0);
    if (state().transform == newTransform)
        return;

    realizeSaves();

    modifiableState().transform = newTransform;
    c->rotate(angleInRadians);
    m_path.transform(AffineTransform().rotate(-angleInRadians / piDouble * 180.0));
}

void CanvasRenderingContext2DBase::translate(double tx, double ty)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    if (!std::isfinite(tx) | !std::isfinite(ty))
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
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    if (!std::isfinite(m11) | !std::isfinite(m21) | !std::isfinite(dx) | !std::isfinite(m12) | !std::isfinite(m22) | !std::isfinite(dy))
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
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (!std::isfinite(m11) | !std::isfinite(m21) | !std::isfinite(dx) | !std::isfinite(m12) | !std::isfinite(m22) | !std::isfinite(dy))
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
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    AffineTransform ctm = state().transform;
    bool hasInvertibleTransform = state().hasInvertibleTransform;

    realizeSaves();

    c->setCTM(canvasBase().baseTransform());
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
    if (!std::isfinite(x) | !std::isfinite(y) | !std::isfinite(width) | !std::isfinite(height))
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

void CanvasRenderingContext2DBase::fillInternal(const Path& path, CanvasFillRule windingRule)
{
    auto* c = drawingContext();
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
    
    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else
        didDraw(path.fastBoundingRect());

    c->setFillRule(savedFillRule);
}

void CanvasRenderingContext2DBase::strokeInternal(const Path& path)
{
    auto* c = drawingContext();
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

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else {
        auto dirtyRect = path.fastBoundingRect();
        inflateStrokeRect(dirtyRect);
        didDraw(dirtyRect);
    }
}

void CanvasRenderingContext2DBase::clipInternal(const Path& path, CanvasFillRule windingRule)
{
    auto* c = drawingContext();
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
    drawingContext()->beginTransparencyLayer(1);
#endif
}

void CanvasRenderingContext2DBase::endCompositeLayer()
{
#if !USE(CAIRO)
    drawingContext()->endTransparencyLayer();    
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
    if (!drawingContext())
        return false;
    auto& state = this->state();
    if (!state.hasInvertibleTransform)
        return false;

    auto transformedPoint = state.transform.inverse().value_or(AffineTransform()).mapPoint(FloatPoint(x, y));
    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

    return path.contains(transformedPoint, toWindRule(windingRule));
}

bool CanvasRenderingContext2DBase::isPointInStrokeInternal(const Path& path, double x, double y)
{
    if (!drawingContext())
        return false;
    auto& state = this->state();
    if (!state.hasInvertibleTransform)
        return false;

    auto transformedPoint = state.transform.inverse().value_or(AffineTransform()).mapPoint(FloatPoint(x, y));
    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

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
    auto* context = drawingContext();
    if (!context)
        return;
    if (!state().hasInvertibleTransform)
        return;
    FloatRect rect(x, y, width, height);

    bool saved = false;
    if (shouldDrawShadows()) {
        context->save();
        saved = true;
        context->setShadow(FloatSize(), 0, Color::transparentBlack, ShadowRadiusMode::Legacy);
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
    didDraw(rect);
}

void CanvasRenderingContext2DBase::fillRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    auto* c = drawingContext();
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

    FloatRect rect(x, y, width, height);

    bool repaintEntireCanvas = false;
    if (rectContainsCanvas(rect)) {
        c->fillRect(rect);
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

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else
        didDraw(rect);
}

void CanvasRenderingContext2DBase::strokeRect(double x, double y, double width, double height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    auto* c = drawingContext();
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

    FloatRect rect(x, y, width, height);
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

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else {
        auto boundingRect = rect;
        boundingRect.inflate(state().lineWidth / 2);
        didDraw(boundingRect);
    }
}

void CanvasRenderingContext2DBase::setShadow(float width, float height, float blur, const String& colorString, std::optional<float> alpha)
{
    if (alpha && std::isnan(*alpha))
        return;

    Color color = Color::transparentBlack;
    if (!colorString.isNull()) {
        color = parseColorOrCurrentColor(colorString, canvasBase());
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
    auto* c = drawingContext();
    if (!c)
        return;

    if (shouldDrawShadows()) {
        float width = state().shadowOffset.width();
        float height = state().shadowOffset.height();
        c->setShadow(FloatSize(width, -height), state().shadowBlur, state().shadowColor, ShadowRadiusMode::Legacy);
    } else
        c->setShadow(FloatSize(), 0, Color::transparentBlack, ShadowRadiusMode::Legacy);
}

bool CanvasRenderingContext2DBase::shouldDrawShadows() const
{
    return state().shadowColor.isVisible() && (state().shadowBlur || !state().shadowOffset.isZero());
}

enum class ImageSizeType { AfterDevicePixelRatio, BeforeDevicePixelRatio };
static LayoutSize size(HTMLImageElement& element, ImageSizeType sizeType = ImageSizeType::BeforeDevicePixelRatio)
{
    LayoutSize size;
    if (auto* cachedImage = element.cachedImage()) {
        size = cachedImage->imageSizeForRenderer(element.renderer(), 1.0f); // FIXME: Not sure about this.
        if (sizeType == ImageSizeType::AfterDevicePixelRatio && is<RenderImage>(element.renderer()) && cachedImage->image() && !cachedImage->image()->hasRelativeWidth())
            size.scale(downcast<RenderImage>(*element.renderer()).imageDevicePixelRatio());
    }
    return size;
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
    auto player = video.player();
    if (!player)
        return { };
    return player->naturalSize();
}

#endif

#if ENABLE(CSS_TYPED_OM)
static inline FloatSize size(CSSStyleImageValue& image)
{
    auto* cachedImage = image.image();
    if (!cachedImage)
        return FloatSize();

    return cachedImage->imageSizeForRenderer(nullptr, 1.0f);
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
    FloatRect imageRect = FloatRect(FloatPoint(), size(imageElement, ImageSizeType::BeforeDevicePixelRatio));

    auto orientation = ImageOrientation::FromImage;
    if (imageElement.allowsOrientationOverride()) {
        if (auto* renderer = imageElement.renderer())
            orientation = renderer->style().imageOrientation();
        else if (auto* computedStyle = imageElement.computedStyle())
            orientation = computedStyle->imageOrientation();
    }

    auto result = drawImage(imageElement.document(), imageElement.cachedImage(), imageElement.renderer(), imageRect, srcRect, dstRect, op, blendMode, orientation);

    if (!result.hasException())
        checkOrigin(&imageElement);
    return result;
}

#if ENABLE(CSS_TYPED_OM)
ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(CSSStyleImageValue& image, const FloatRect& srcRect, const FloatRect& dstRect)
{
    auto* cachedImage = image.image();
    if (!cachedImage || !image.document())
        return { };
    FloatRect imageRect = FloatRect(FloatPoint(), size(image));

    auto result = drawImage(*image.document(), cachedImage, nullptr, imageRect, srcRect, dstRect, state().globalComposite, state().globalBlend);

    if (!result.hasException())
        checkOrigin(image);
    return result;
}
#endif

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(Document& document, CachedImage* cachedImage, const RenderObject* renderer, const FloatRect& imageRect, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator& op, const BlendMode& blendMode, ImageOrientation orientation)
{
    if (!std::isfinite(dstRect.x()) || !std::isfinite(dstRect.y()) || !std::isfinite(dstRect.width()) || !std::isfinite(dstRect.height())
        || !std::isfinite(srcRect.x()) || !std::isfinite(srcRect.y()) || !std::isfinite(srcRect.width()) || !std::isfinite(srcRect.height()))
        return { };

    if (!dstRect.width() || !dstRect.height())
        return { };

    FloatRect normalizedSrcRect = normalizeRect(srcRect);
    FloatRect normalizedDstRect = normalizeRect(dstRect);

    if (!srcRect.width() || !srcRect.height())
        return Exception { IndexSizeError };

    // When the source rectangle is outside the source image, the source rectangle must be clipped
    // to the source image and the destination rectangle must be clipped in the same proportion.
    FloatRect originalNormalizedSrcRect = normalizedSrcRect;
    normalizedSrcRect.intersect(imageRect);
    if (normalizedSrcRect.isEmpty())
        return { };

    if (normalizedSrcRect != originalNormalizedSrcRect) {
        normalizedDstRect.setWidth(normalizedDstRect.width() * normalizedSrcRect.width() / originalNormalizedSrcRect.width());
        normalizedDstRect.setHeight(normalizedDstRect.height() * normalizedSrcRect.height() / originalNormalizedSrcRect.height());
        if (normalizedDstRect.isEmpty())
            return { };
    }

    GraphicsContext* c = drawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    if (!cachedImage)
        return { };

    RefPtr<Image> image = cachedImage->imageForRenderer(renderer);
    if (!image)
        return { };

    ImageObserver* observer = image->imageObserver();

    if (image->drawsSVGImage()) {
        image->setImageObserver(nullptr);
        image->setContainerSize(imageRect.size());
    }

    if (image->isBitmapImage()) {
        // Drawing an animated image to a canvas should draw the first frame (except for a few layout tests)
        if (image->isAnimated() && !document.settings().animatedImageDebugCanvasDrawingEnabled())
            image = BitmapImage::create(image->nativeImage());
        downcast<BitmapImage>(*image).updateFromSettings(document.settings());
    }

    ImagePaintingOptions options = { op, blendMode, orientation };

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

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else
        didDraw(normalizedDstRect);

    if (image->drawsSVGImage())
        image->setImageObserver(observer);

    return { };
}

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(CanvasBase& sourceCanvas, const FloatRect& srcRect, const FloatRect& dstRect)
{
    FloatRect srcCanvasRect = FloatRect(FloatPoint(), sourceCanvas.size());

    if (!srcCanvasRect.width() || !srcCanvasRect.height())
        return Exception { InvalidStateError };

    if (!srcRect.width() || !srcRect.height())
        return Exception { IndexSizeError };

    if (!srcCanvasRect.contains(normalizeRect(srcRect)) || !dstRect.width() || !dstRect.height())
        return { };

    GraphicsContext* c = drawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    // FIXME: Do this through platform-independent GraphicsContext API.
    ImageBuffer* buffer = sourceCanvas.buffer();
    if (!buffer)
        return { };

    checkOrigin(&sourceCanvas);

    sourceCanvas.makeRenderingResultsAvailable();

    bool repaintEntireCanvas = false;
    if (rectContainsCanvas(dstRect)) {
        c->drawImageBuffer(*buffer, dstRect, srcRect, { state().globalComposite, state().globalBlend });
        repaintEntireCanvas = true;
    } else if (isFullCanvasCompositeMode(state().globalComposite)) {
        fullCanvasCompositedDrawImage(*buffer, dstRect, srcRect, state().globalComposite);
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        if (&sourceCanvas == &canvasBase()) {
            if (auto copy = buffer->copyRectToBuffer(srcRect, colorSpace(), *c)) {
                clearCanvas();
                c->drawImageBuffer(*copy, dstRect, { { }, srcRect.size() }, { state().globalComposite, state().globalBlend });
            }
        } else {
            clearCanvas();
            c->drawImageBuffer(*buffer, dstRect, srcRect, { state().globalComposite, state().globalBlend });
        }
        repaintEntireCanvas = true;
    } else
        c->drawImageBuffer(*buffer, dstRect, srcRect, { state().globalComposite, state().globalBlend });

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else
        didDraw(dstRect);

    return { };
}

#if ENABLE(VIDEO)

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(HTMLVideoElement& video, const FloatRect& srcRect, const FloatRect& dstRect)
{
    if (video.readyState() == HTMLMediaElement::HAVE_NOTHING || video.readyState() == HTMLMediaElement::HAVE_METADATA)
        return { };

    FloatRect videoRect = FloatRect(FloatPoint(), size(video));
    if (!srcRect.width() || !srcRect.height())
        return Exception { IndexSizeError };

    if (!videoRect.contains(normalizeRect(srcRect)) || !dstRect.width() || !dstRect.height())
        return { };

    GraphicsContext* c = drawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    checkOrigin(&video);

#if USE(CG)
    if (auto image = video.nativeImageForCurrentTime()) {
        c->drawNativeImage(*image, FloatSize(video.videoWidth(), video.videoHeight()), dstRect, srcRect);

        if (isEntireBackingStoreDirty())
            didDraw(std::nullopt);
        else if (rectContainsCanvas(dstRect))
            didDrawEntireCanvas();
        else
            didDraw(dstRect);

        return { };
    }
#endif

    GraphicsContextStateSaver stateSaver(*c);
    c->clip(dstRect);
    c->translate(dstRect.location());
    c->scale(FloatSize(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height()));
    c->translate(-srcRect.location());
    video.paintCurrentFrameInContext(*c, FloatRect(FloatPoint(), size(video)));
    stateSaver.restore();

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else
        didDraw(dstRect);

    return { };
}

#endif

ExceptionOr<void> CanvasRenderingContext2DBase::drawImage(ImageBitmap& imageBitmap, const FloatRect& srcRect, const FloatRect& dstRect)
{
    if (!imageBitmap.width() || !imageBitmap.height())
        return Exception { InvalidStateError };

    auto normalizedSrcRect = normalizeRect(srcRect);

    if (normalizedSrcRect.isEmpty())
        return { };

    FloatRect srcBitmapRect = FloatRect(FloatPoint(), FloatSize(imageBitmap.width(), imageBitmap.height()));

    if (!srcBitmapRect.contains(normalizedSrcRect) || !dstRect.width() || !dstRect.height())
        return { };

    GraphicsContext* c = drawingContext();
    if (!c)
        return { };
    if (!state().hasInvertibleTransform)
        return { };

    ImageBuffer* buffer = imageBitmap.buffer();
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

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else
        didDraw(dstRect);

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
    auto* c = drawingContext();
    if (!c)
        return;

    c->save();
    c->setCTM(canvasBase().baseTransform());
    c->clearRect(FloatRect(0, 0, canvasBase().width(), canvasBase().height()));
    c->restore();
}

Path CanvasRenderingContext2DBase::transformAreaToDevice(const Path& path) const
{
    Path transformed(path);
    transformed.transform(state().transform);
    transformed.transform(canvasBase().baseTransform());
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
    canvasRect = canvasBase().baseTransform().mapRect(canvasRect);
    Path path = transformAreaToDevice(area);
    IntRect bufferRect = enclosingIntRect(path.fastBoundingRect());
    IntPoint originalLocation = bufferRect.location();
    bufferRect.intersect(canvasRect);
    if (croppedOffset)
        *croppedOffset = originalLocation - bufferRect.location();
    return bufferRect;
}

RefPtr<ImageBuffer> CanvasRenderingContext2DBase::createCompositingBuffer(const IntRect& bufferRect)
{
    return ImageBuffer::create(bufferRect.size(), isAccelerated() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, 1, colorSpace(), pixelFormat());
}

void CanvasRenderingContext2DBase::compositeBuffer(ImageBuffer& buffer, const IntRect& bufferRect, CompositeOperator op)
{
    IntRect canvasRect(0, 0, canvasBase().width(), canvasBase().height());
    canvasRect = canvasBase().baseTransform().mapRect(canvasRect);

    auto* c = drawingContext();
    if (!c)
        return;

    c->save();
    c->setCTM(AffineTransform());
    c->setCompositeOperation(op);

    c->save();
    c->clipOut(bufferRect);
    c->clearRect(canvasRect);
    c->restore();
    c->drawImageBuffer(buffer, bufferRect.location(), state().globalComposite);
    c->restore();
}

static void drawImageToContext(Image& image, GraphicsContext& context, const FloatRect& dest, const FloatRect& src, const ImagePaintingOptions& options)
{
    context.drawImage(image, dest, src, options);
}

static void drawImageToContext(ImageBuffer& imageBuffer, GraphicsContext& context, const FloatRect& dest, const FloatRect& src, const ImagePaintingOptions& options)
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

    auto buffer = createCompositingBuffer(bufferRect);
    if (!buffer)
        return;

    auto* c = drawingContext();
    if (!c)
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
    if (auto gradient = style.canvasGradient())
        return gradient;
    if (auto pattern = style.canvasPattern())
        return pattern;
    return style.color();
}

CanvasRenderingContext2DBase::StyleVariant CanvasRenderingContext2DBase::strokeStyle() const
{
    return toStyleVariant(state().strokeStyle);
}

void CanvasRenderingContext2DBase::setStrokeStyle(CanvasRenderingContext2DBase::StyleVariant&& style)
{
    WTF::switchOn(style,
        [this] (const String& string) { this->setStrokeColor(string); },
        [this] (const RefPtr<CanvasGradient>& gradient) { this->setStrokeStyle(CanvasStyle(*gradient)); },
        [this] (const RefPtr<CanvasPattern>& pattern) { this->setStrokeStyle(CanvasStyle(*pattern)); }
    );
}

CanvasRenderingContext2DBase::StyleVariant CanvasRenderingContext2DBase::fillStyle() const
{
    return toStyleVariant(state().fillStyle);
}

void CanvasRenderingContext2DBase::setFillStyle(CanvasRenderingContext2DBase::StyleVariant&& style)
{
    WTF::switchOn(style,
        [this] (const String& string) { this->setFillColor(string); },
        [this] (const RefPtr<CanvasGradient>& gradient) { this->setFillStyle(CanvasStyle(*gradient)); },
        [this] (const RefPtr<CanvasPattern>& pattern) { this->setFillStyle(CanvasStyle(*pattern)); }
    );
}

ExceptionOr<Ref<CanvasGradient>> CanvasRenderingContext2DBase::createLinearGradient(float x0, float y0, float x1, float y1)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1))
        return Exception { NotSupportedError };

    return CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1), canvasBase());
}

ExceptionOr<Ref<CanvasGradient>> CanvasRenderingContext2DBase::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(r0) || !std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(r1))
        return Exception { NotSupportedError };

    if (r0 < 0 || r1 < 0)
        return Exception { IndexSizeError };

    return CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1, canvasBase());
}

ExceptionOr<Ref<CanvasGradient>> CanvasRenderingContext2DBase::createConicGradient(float angleInRadians, float x, float y)
{
    if (!std::isfinite(angleInRadians) || !std::isfinite(x) || !std::isfinite(y))
        return Exception { NotSupportedError };

    return CanvasGradient::create(FloatPoint(x, y), angleInRadians, canvasBase());
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(CanvasImageSource&& image, const String& repetition)
{
    bool repeatX, repeatY;
    if (!CanvasPattern::parseRepetitionType(repetition, repeatX, repeatY))
        return Exception { SyntaxError };

    return WTF::switchOn(image,
        [&] (auto& element) -> ExceptionOr<RefPtr<CanvasPattern>> { return this->createPattern(*element, repeatX, repeatY); }
    );
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(HTMLImageElement& imageElement, bool repeatX, bool repeatY)
{
    auto* cachedImage = imageElement.cachedImage();

    // If the image loading hasn't started or the image is not complete, it is not fully decodable.
    if (!cachedImage || !imageElement.complete())
        return nullptr;

    if (cachedImage->status() == CachedResource::LoadError)
        return Exception { InvalidStateError };

    bool originClean = cachedImage->isOriginClean(canvasBase().securityOrigin());

    // FIXME: SVG images with animations can switch between clean and dirty (leaking cross-origin
    // data). We should either:
    //   1) Take a fixed snapshot of an SVG image when creating a pattern and determine then whether
    //      the origin is clean.
    //   2) Dynamically verify the origin checks at draw time, and dirty the canvas accordingly.
    // To be on the safe side, taint the origin for all patterns containing SVG images for now.
    if (cachedImage->image()->drawsSVGImage())
        originClean = false;

    auto* image = cachedImage->imageForRenderer(imageElement.renderer());
    if (!image)
        return Exception { InvalidStateError };

    auto nativeImage = image->nativeImage();
    if (!nativeImage)
        return Exception { InvalidStateError };

    return RefPtr<CanvasPattern> { CanvasPattern::create(nativeImage.releaseNonNull(), repeatX, repeatY, originClean) };
}

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(CanvasBase& canvas, bool repeatX, bool repeatY)
{
    if (!canvas.width() || !canvas.height())
        return Exception { InvalidStateError };
    auto* copiedImage = canvas.copiedImage();

    if (!copiedImage)
        return Exception { InvalidStateError };
    
    auto nativeImage = copiedImage->nativeImage();
    if (!nativeImage)
        return Exception { InvalidStateError };

    return RefPtr<CanvasPattern> { CanvasPattern::create(nativeImage.releaseNonNull(), repeatX, repeatY, canvas.originClean()) };
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
        return RefPtr<CanvasPattern> { CanvasPattern::create(nativeImage.releaseNonNull(), repeatX, repeatY, originClean) };
#endif

    auto renderingMode = drawingContext() ? drawingContext()->renderingMode() : RenderingMode::Unaccelerated;
    auto imageBuffer = videoElement.createBufferForPainting(size(videoElement), renderingMode, colorSpace(), pixelFormat());
    if (!imageBuffer)
        return nullptr;

    videoElement.paintCurrentFrameInContext(imageBuffer->context(), FloatRect(FloatPoint(), size(videoElement)));
    
    return RefPtr<CanvasPattern> { CanvasPattern::create(ImageBuffer::sinkIntoNativeImage(WTFMove(imageBuffer)).releaseNonNull(), repeatX, repeatY, originClean) };
}

#endif

ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(ImageBitmap&, bool, bool)
{
    // FIXME: Implement.
    return Exception { TypeError };
}

#if ENABLE(CSS_TYPED_OM)
ExceptionOr<RefPtr<CanvasPattern>> CanvasRenderingContext2DBase::createPattern(CSSStyleImageValue&, bool, bool)
{
    // FIXME: Implement.
    return Exception { TypeError };
}
#endif

void CanvasRenderingContext2DBase::didDrawEntireCanvas()
{
    didDraw(backingStoreBounds(), DidDrawOption::ApplyClip);
}

void CanvasRenderingContext2DBase::didDraw(std::optional<FloatRect> rect, OptionSet<DidDrawOption> options)
{
    if (!drawingContext())
        return;

    if (!rect) {
        canvasBase().didDraw(std::nullopt);
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
        canvasBase().didDraw(std::nullopt);
    else {
        m_dirtyRect.unite(dirtyRect);
        canvasBase().didDraw(m_dirtyRect);
    }
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

void CanvasRenderingContext2DBase::paintRenderingResultsToCanvas()
{
    if (!m_recordingContext)
        return;

    ASSERT(m_usesDisplayListDrawing);

    auto& displayList = m_recordingContext->displayList();
    if (!displayList.isEmpty()) {
        DisplayList::Replayer replayer(*canvasBase().drawingContext(), displayList);
        replayer.replay(backingStoreBounds());
        displayList.clear();
    }
}

GraphicsContext* CanvasRenderingContext2DBase::drawingContext() const
{
    if (UNLIKELY(m_usesDisplayListDrawing)) {
        if (!m_recordingContext)
            m_recordingContext = makeUnique<DisplayList::DrawingContext>(canvasBase().size());
        return &m_recordingContext->context();
    }

    return canvasBase().drawingContext();
}

void CanvasRenderingContext2DBase::prepareForDisplay()
{
    if (auto buffer = canvasBase().buffer())
        buffer->flushDrawingContextAsync();
}

bool CanvasRenderingContext2DBase::needsPreparationForDisplay() const
{
    auto buffer = canvasBase().buffer();
    if (buffer && buffer->prefersPreparationForDisplay())
        return true;

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
        return Exception { IndexSizeError };

    auto imageData = ImageData::createUninitialized(std::abs(sw), std::abs(sh), m_settings.colorSpace, settings);
    if (!imageData.hasException())
        initializeEmptyImageData(imageData.returnValue());
    return imageData;
}

ExceptionOr<Ref<ImageData>> CanvasRenderingContext2DBase::getImageData(int sx, int sy, int sw, int sh, std::optional<ImageDataSettings> settings) const
{
    if (!sw || !sh)
        return Exception { IndexSizeError };

    if (!canvasBase().originClean()) {
        static NeverDestroyed<String> consoleMessage(MAKE_STATIC_STRING_IMPL("Unable to get image data from canvas because the canvas has been tainted by cross-origin data."));
        canvasBase().scriptExecutionContext()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage);
        return Exception { SecurityError };
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

    ImageBuffer* buffer = canvasBase().buffer();
    if (!buffer) {
        auto imageData = ImageData::createUninitialized(imageDataRect.width(), imageDataRect.height(), m_settings.colorSpace, settings);
        if (!imageData.hasException())
            initializeEmptyImageData(imageData.returnValue());
        return imageData;
    }

    auto computedColorSpace = ImageData::computeColorSpace(settings, m_settings.colorSpace);

    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, toDestinationColorSpace(computedColorSpace) };
    auto pixelBuffer = buffer->getPixelBuffer(format, imageDataRect);
    if (!pixelBuffer) {
        canvasBase().scriptExecutionContext()->addConsoleMessage(MessageSource::Rendering, MessageLevel::Error,
            makeString("Unable to get image data from canvas. Requested size was ", imageDataRect.width(), " x ", imageDataRect.height()));
        return Exception { InvalidStateError };
    }

    ASSERT(pixelBuffer->format().colorSpace == toDestinationColorSpace(computedColorSpace));

    return { { ImageData::create(WTFMove(*pixelBuffer)) } };
}

void CanvasRenderingContext2DBase::putImageData(ImageData& data, int dx, int dy)
{
    putImageData(data, dx, dy, 0, 0, data.width(), data.height());
}

void CanvasRenderingContext2DBase::putImageData(ImageData& data, int dx, int dy, int dirtyX, int dirtyY, int dirtyWidth, int dirtyHeight)
{
    ImageBuffer* buffer = canvasBase().buffer();
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

    IntRect clipRect { dirtyX, dirtyY, dirtyWidth, dirtyHeight };
    clipRect.intersect(IntRect { 0, 0, data.width(), data.height() });
    IntSize destOffset { dx, dy };
    IntRect destRect = clipRect;
    destRect.move(destOffset);
    destRect.intersect(IntRect { { }, buffer->logicalSize() });
    if (destRect.isEmpty())
        return;
    IntRect sourceRect { destRect };
    sourceRect.move(-destOffset);
    sourceRect.intersect(IntRect { 0, 0, data.width(), data.height() });

    if (!sourceRect.isEmpty())
        buffer->putPixelBuffer(data.pixelBuffer(), sourceRect, IntPoint { destOffset });

    didDraw(FloatRect { destRect }, { }); // ignore transform, shadow and clip
}

void CanvasRenderingContext2DBase::inflateStrokeRect(FloatRect& rect) const
{
    // Fast approximation of the stroke's bounding rect.
    // This yields a slightly oversized rect but is very fast
    // compared to Path::strokeBoundingRect().
    static const float root2 = sqrtf(2);
    float delta = state().lineWidth / 2;
    if (state().lineJoin == MiterJoin)
        delta *= state().miterLimit;
    else if (state().lineCap == SquareCap)
        delta *= root2;
    rect.inflate(delta);
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

    if (auto* context = drawingContext())
        context->setImageInterpolationQuality(smoothingToInterpolationQuality(quality));
}

void CanvasRenderingContext2DBase::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().imageSmoothingEnabled)
        return;

    realizeSaves();
    modifiableState().imageSmoothingEnabled = enabled;
    auto* c = drawingContext();
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

    auto* c = drawingContext();
    if (!c)
        return false;
    if (!state().hasInvertibleTransform)
        return false;
    if (!std::isfinite(x) | !std::isfinite(y))
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
    StringView(text).getCharactersWithUpconvert(charVector.data());

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
    TextRun textRun(normalizedText, 0, 0, AllowRightExpansion, direction, false, true);
    drawTextUnchecked(textRun, x, y, fill, maxWidth);
}

void CanvasRenderingContext2DBase::drawTextUnchecked(const TextRun& textRun, double x, double y, bool fill, std::optional<double> maxWidth)
{
    auto* c = drawingContext();
    auto& fontProxy = *this->fontProxy();
    const auto& fontMetrics = fontProxy.fontMetrics();

    // FIXME: Need to turn off font smoothing.

    float fontWidth = fontProxy.width(textRun);
    bool useMaxWidth = maxWidth && maxWidth.value() < fontWidth;
    float width = useMaxWidth ? maxWidth.value() : fontWidth;
    FloatPoint location(x, y);
    location += textOffset(width, textRun.direction());

    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    FloatRect textRect = FloatRect(location.x() - fontMetrics.height() / 2, location.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
        width + fontMetrics.height(), fontMetrics.lineSpacing());
    if (!fill)
        inflateStrokeRect(textRect);

#if USE(CG)
    const CanvasStyle& drawStyle = fill ? state().fillStyle : state().strokeStyle;
    if (drawStyle.canvasGradient() || drawStyle.canvasPattern()) {
        IntRect maskRect = enclosingIntRect(textRect);

        // If we have a shadow, we need to draw it before the mask operation.
        // Follow a procedure similar to paintTextWithShadows in TextPainter.

        if (shouldDrawShadows()) {
            GraphicsContextStateSaver stateSaver(*c);

            FloatSize offset(0, 2 * maskRect.height());

            FloatSize shadowOffset;
            float shadowRadius;
            Color shadowColor;
            c->getShadow(shadowOffset, shadowRadius, shadowColor);

            FloatRect shadowRect(maskRect);
            shadowRect.inflate(shadowRadius * 1.4);
            shadowRect.move(shadowOffset * -1);
            c->clip(shadowRect);

            shadowOffset += offset;

            c->setShadow(shadowOffset, shadowRadius, shadowColor, ShadowRadiusMode::Legacy);

            if (fill)
                c->setFillColor(Color::black);
            else
                c->setStrokeColor(Color::black);

            fontProxy.drawBidiText(*c, textRun, location + offset, FontCascade::UseFallbackIfFontNotReady);
        }

        GraphicsContextStateSaver stateSaver(*c);

        auto paintMaskImage = [&] (GraphicsContext& maskImageContext) {
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
                fontProxy.drawBidiText(maskImageContext, textRun, FloatPoint(0, 0), FontCascade::UseFallbackIfFontNotReady);
            } else {
                maskImageContext.translate(-maskRect.location());
                fontProxy.drawBidiText(maskImageContext, textRun, location, FontCascade::UseFallbackIfFontNotReady);
            }
        };

        // FIXME: Handling gradients and patterns by painting the text into a mask is probably the wrong thing to do in the presence of color glyphs.
        if (c->clipToDrawingCommands(maskRect, colorSpace(), WTFMove(paintMaskImage)) == GraphicsContext::ClipToDrawingCommandsResult::FailedToCreateImageBuffer)
            return;

        drawStyle.applyFillColor(*c);
        c->fillRect(maskRect);
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
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::UseFallbackIfFontNotReady);
        endCompositeLayer();
        repaintEntireCanvas = true;
    } else if (state().globalComposite == CompositeOperator::Copy) {
        clearCanvas();
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::UseFallbackIfFontNotReady);
        repaintEntireCanvas = true;
    } else
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::UseFallbackIfFontNotReady);

    if (isEntireBackingStoreDirty())
        didDraw(std::nullopt);
    else if (repaintEntireCanvas)
        didDrawEntireCanvas();
    else
        didDraw(textRect);
}

Ref<TextMetrics> CanvasRenderingContext2DBase::measureTextInternal(const String& text)
{
    String normalizedText = normalizeSpaces(text);
    auto direction = (state().direction == Direction::Rtl) ? TextDirection::RTL : TextDirection::LTR;
    TextRun textRun(normalizedText, 0, 0, AllowRightExpansion, direction, false, true);
    return measureTextInternal(textRun);
}

Ref<TextMetrics> CanvasRenderingContext2DBase::measureTextInternal(const TextRun& textRun)
{
    Ref<TextMetrics> metrics = TextMetrics::create();

    auto& font = *fontProxy();
    auto& fontMetrics = font.fontMetrics();

    GlyphOverflow glyphOverflow;
    glyphOverflow.computeBounds = true;
    float fontWidth = font.width(textRun, &glyphOverflow);
    metrics->setWidth(fontWidth);

    FloatPoint offset = textOffset(fontWidth, textRun.direction());

    metrics->setActualBoundingBoxAscent(glyphOverflow.top - offset.y());
    metrics->setActualBoundingBoxDescent(glyphOverflow.bottom + offset.y());
    metrics->setFontBoundingBoxAscent(fontMetrics.ascent() - offset.y());
    metrics->setFontBoundingBoxDescent(fontMetrics.descent() + offset.y());
    metrics->setEmHeightAscent(fontMetrics.ascent() - offset.y());
    metrics->setEmHeightDescent(fontMetrics.descent() + offset.y());
    metrics->setHangingBaseline(fontMetrics.ascent() - offset.y());
    metrics->setAlphabeticBaseline(-offset.y());
    metrics->setIdeographicBaseline(-fontMetrics.descent() - offset.y());

    metrics->setActualBoundingBoxLeft(glyphOverflow.left - offset.x());
    metrics->setActualBoundingBoxRight(fontWidth + glyphOverflow.right + offset.x());

    return metrics;
}

FloatPoint CanvasRenderingContext2DBase::textOffset(float width, TextDirection direction)
{
    auto& fontMetrics = fontProxy()->fontMetrics();
    FloatPoint offset;

    switch (state().textBaseline) {
    case TopTextBaseline:
    case HangingTextBaseline:
        offset.setY(fontMetrics.ascent());
        break;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        offset.setY(-fontMetrics.descent());
        break;
    case MiddleTextBaseline:
        offset.setY(fontMetrics.height() / 2 - fontMetrics.descent());
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

PixelFormat CanvasRenderingContext2DBase::pixelFormat() const
{
    // FIXME: Take m_settings.alpha into account here and add PixelFormat::BGRX8.
    return PixelFormat::BGRA8;
}

DestinationColorSpace CanvasRenderingContext2DBase::colorSpace() const
{
    return toDestinationColorSpace(m_settings.colorSpace);
}

} // namespace WebCore
