/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "CanvasRenderingContext2D.h"

#include "CSSFontSelector.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "DOMPath.h"
#include "DisplayListRecorder.h"
#include "DisplayListReplayer.h"
#include "ExceptionCodePlaceholder.h"
#include "FloatQuad.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "SecurityOrigin.h"
#include "StrokeStyleApplier.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "TextMetrics.h"
#include "TextRun.h"
#include "TextStream.h"

#include <wtf/CheckedArithmetic.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#if USE(CG)
#if !PLATFORM(IOS)
#include <ApplicationServices/ApplicationServices.h>
#endif // !PLATFORM(IOS)
#endif

#if PLATFORM(IOS)
#include "Settings.h"
#endif

#if USE(CG)
#define DefaultSmoothingQuality SmoothingQuality::Low
#else
#define DefaultSmoothingQuality SmoothingQuality::Medium
#endif

namespace WebCore {

using namespace HTMLNames;

static const int defaultFontSize = 10;
static const char* const defaultFontFamily = "sans-serif";
static const char* const defaultFont = "10px sans-serif";

struct DisplayListDrawingContext {
    GraphicsContext context;
    DisplayList::Recorder recorder;
    DisplayList::DisplayList displayList;
    
    DisplayListDrawingContext(const FloatRect& clip)
        : recorder(context, displayList, clip, AffineTransform())
    {
    }
};

typedef HashMap<const CanvasRenderingContext2D*, std::unique_ptr<DisplayList::DisplayList>> ContextDisplayListHashMap;

static ContextDisplayListHashMap& contextDisplayListMap()
{
    static NeverDestroyed<ContextDisplayListHashMap> sharedHashMap;
    return sharedHashMap;
}

class CanvasStrokeStyleApplier : public StrokeStyleApplier {
public:
    CanvasStrokeStyleApplier(CanvasRenderingContext2D* canvasContext)
        : m_canvasContext(canvasContext)
    {
    }

    virtual void strokeStyle(GraphicsContext* c) override
    {
        c->setStrokeThickness(m_canvasContext->lineWidth());
        c->setLineCap(m_canvasContext->getLineCap());
        c->setLineJoin(m_canvasContext->getLineJoin());
        c->setMiterLimit(m_canvasContext->miterLimit());
        const Vector<float>& lineDash = m_canvasContext->getLineDash();
        DashArray convertedLineDash(lineDash.size());
        for (size_t i = 0; i < lineDash.size(); ++i)
            convertedLineDash[i] = static_cast<DashArrayElement>(lineDash[i]);
        c->setLineDash(convertedLineDash, m_canvasContext->lineDashOffset());
    }

private:
    CanvasRenderingContext2D* m_canvasContext;
};

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas, bool usesCSSCompatibilityParseMode, bool usesDashboardCompatibilityMode)
    : CanvasRenderingContext(canvas)
    , m_stateStack(1)
    , m_usesCSSCompatibilityParseMode(usesCSSCompatibilityParseMode)
#if ENABLE(DASHBOARD_SUPPORT)
    , m_usesDashboardCompatibilityMode(usesDashboardCompatibilityMode)
#endif
{
#if !ENABLE(DASHBOARD_SUPPORT)
    ASSERT_UNUSED(usesDashboardCompatibilityMode, !usesDashboardCompatibilityMode);
#endif
}

void CanvasRenderingContext2D::unwindStateStack()
{
    // Ensure that the state stack in the ImageBuffer's context
    // is cleared before destruction, to avoid assertions in the
    // GraphicsContext dtor.
    if (size_t stackSize = m_stateStack.size()) {
        if (GraphicsContext* context = canvas()->existingDrawingContext()) {
            while (--stackSize)
                context->restore();
        }
    }
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
#if !ASSERT_DISABLED
    unwindStateStack();
#endif

    if (UNLIKELY(tracksDisplayListReplay()))
        contextDisplayListMap().remove(this);
}

bool CanvasRenderingContext2D::isAccelerated() const
{
#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
    if (!canvas()->hasCreatedImageBuffer())
        return false;
    GraphicsContext* context = drawingContext();
    return context && context->isAcceleratedContext();
#else
    return false;
#endif
}

void CanvasRenderingContext2D::reset()
{
    unwindStateStack();
    m_stateStack.resize(1);
    m_stateStack.first() = State();
    m_path.clear();
    m_unrealizedSaveCount = 0;
    
    m_recordingContext = nullptr;
}

CanvasRenderingContext2D::State::State()
    : strokeStyle(Color::black)
    , fillStyle(Color::black)
    , lineWidth(1)
    , lineCap(ButtCap)
    , lineJoin(MiterJoin)
    , miterLimit(10)
    , shadowBlur(0)
    , shadowColor(Color::transparent)
    , globalAlpha(1)
    , globalComposite(CompositeSourceOver)
    , globalBlend(BlendModeNormal)
    , hasInvertibleTransform(true)
    , lineDashOffset(0)
    , imageSmoothingEnabled(true)
    , imageSmoothingQuality(DefaultSmoothingQuality)
    , textAlign(StartTextAlign)
    , textBaseline(AlphabeticTextBaseline)
    , direction(Direction::Inherit)
    , unparsedFont(defaultFont)
{
}

CanvasRenderingContext2D::State::State(const State& other)
    : unparsedStrokeColor(other.unparsedStrokeColor)
    , unparsedFillColor(other.unparsedFillColor)
    , strokeStyle(other.strokeStyle)
    , fillStyle(other.fillStyle)
    , lineWidth(other.lineWidth)
    , lineCap(other.lineCap)
    , lineJoin(other.lineJoin)
    , miterLimit(other.miterLimit)
    , shadowOffset(other.shadowOffset)
    , shadowBlur(other.shadowBlur)
    , shadowColor(other.shadowColor)
    , globalAlpha(other.globalAlpha)
    , globalComposite(other.globalComposite)
    , globalBlend(other.globalBlend)
    , transform(other.transform)
    , hasInvertibleTransform(other.hasInvertibleTransform)
    , lineDashOffset(other.lineDashOffset)
    , imageSmoothingEnabled(other.imageSmoothingEnabled)
    , imageSmoothingQuality(other.imageSmoothingQuality)
    , textAlign(other.textAlign)
    , textBaseline(other.textBaseline)
    , direction(other.direction)
    , unparsedFont(other.unparsedFont)
    , font(other.font)
{
}

CanvasRenderingContext2D::State& CanvasRenderingContext2D::State::operator=(const State& other)
{
    if (this == &other)
        return *this;

    unparsedStrokeColor = other.unparsedStrokeColor;
    unparsedFillColor = other.unparsedFillColor;
    strokeStyle = other.strokeStyle;
    fillStyle = other.fillStyle;
    lineWidth = other.lineWidth;
    lineCap = other.lineCap;
    lineJoin = other.lineJoin;
    miterLimit = other.miterLimit;
    shadowOffset = other.shadowOffset;
    shadowBlur = other.shadowBlur;
    shadowColor = other.shadowColor;
    globalAlpha = other.globalAlpha;
    globalComposite = other.globalComposite;
    globalBlend = other.globalBlend;
    transform = other.transform;
    hasInvertibleTransform = other.hasInvertibleTransform;
    imageSmoothingEnabled = other.imageSmoothingEnabled;
    imageSmoothingQuality = other.imageSmoothingQuality;
    textAlign = other.textAlign;
    textBaseline = other.textBaseline;
    direction = other.direction;
    unparsedFont = other.unparsedFont;
    font = other.font;

    return *this;
}

CanvasRenderingContext2D::FontProxy::~FontProxy()
{
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);
}

CanvasRenderingContext2D::FontProxy::FontProxy(const FontProxy& other)
    : m_font(other.m_font)
{
    if (realized())
        m_font.fontSelector()->registerForInvalidationCallbacks(*this);
}

auto CanvasRenderingContext2D::FontProxy::operator=(const FontProxy& other) -> FontProxy&
{
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);

    m_font = other.m_font;

    if (realized())
        m_font.fontSelector()->registerForInvalidationCallbacks(*this);

    return *this;
}

inline void CanvasRenderingContext2D::FontProxy::update(FontSelector& selector)
{
    ASSERT(&selector == m_font.fontSelector()); // This is an invariant. We should only ever be registered for callbacks on m_font.m_fonts.m_fontSelector.
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);
    m_font.update(&selector);
    if (realized())
        m_font.fontSelector()->registerForInvalidationCallbacks(*this);
    ASSERT(&selector == m_font.fontSelector());
}

void CanvasRenderingContext2D::FontProxy::fontsNeedUpdate(FontSelector& selector)
{
    ASSERT_ARG(selector, &selector == m_font.fontSelector());
    ASSERT(realized());

    update(selector);
}

inline void CanvasRenderingContext2D::FontProxy::initialize(FontSelector& fontSelector, RenderStyle& newStyle)
{
    // Beware! m_font.fontSelector() might not point to document.fontSelector()!
    ASSERT(newStyle.fontCascade().fontSelector() == &fontSelector);
    if (realized())
        m_font.fontSelector()->unregisterForInvalidationCallbacks(*this);
    m_font = newStyle.fontCascade();
    m_font.update(&fontSelector);
    ASSERT(&fontSelector == m_font.fontSelector());
    m_font.fontSelector()->registerForInvalidationCallbacks(*this);
}

inline FontMetrics CanvasRenderingContext2D::FontProxy::fontMetrics() const
{
    return m_font.fontMetrics();
}

inline const FontCascadeDescription& CanvasRenderingContext2D::FontProxy::fontDescription() const
{
    return m_font.fontDescription();
}

inline float CanvasRenderingContext2D::FontProxy::width(const TextRun& textRun) const
{
    return m_font.width(textRun);
}

inline void CanvasRenderingContext2D::FontProxy::drawBidiText(GraphicsContext& context, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction action) const
{
    context.drawBidiText(m_font, run, point, action);
}

void CanvasRenderingContext2D::realizeSavesLoop()
{
    ASSERT(m_unrealizedSaveCount);
    ASSERT(m_stateStack.size() >= 1);
    GraphicsContext* context = drawingContext();
    do {
        m_stateStack.append(state());
        if (context)
            context->save();
    } while (--m_unrealizedSaveCount);
}

void CanvasRenderingContext2D::restore()
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
    if (Optional<AffineTransform> inverse = state().transform.inverse())
        m_path.transform(inverse.value());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->restore();
}

void CanvasRenderingContext2D::setStrokeStyle(CanvasStyle style)
{
    if (!style.isValid())
        return;

    if (state().strokeStyle.isValid() && state().strokeStyle.isEquivalentColor(style))
        return;

    if (style.isCurrentColor()) {
        if (style.hasOverrideAlpha())
            style = CanvasStyle(colorWithOverrideAlpha(currentColor(canvas()), style.overrideAlpha()));
        else
            style = CanvasStyle(currentColor(canvas()));
    } else
        checkOrigin(style.canvasPattern());

    realizeSaves();
    State& state = modifiableState();
    state.strokeStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state.strokeStyle.applyStrokeColor(c);
    state.unparsedStrokeColor = String();
}

void CanvasRenderingContext2D::setFillStyle(CanvasStyle style)
{
    if (!style.isValid())
        return;

    if (state().fillStyle.isValid() && state().fillStyle.isEquivalentColor(style))
        return;

    if (style.isCurrentColor()) {
        if (style.hasOverrideAlpha())
            style = CanvasStyle(colorWithOverrideAlpha(currentColor(canvas()), style.overrideAlpha()));
        else
            style = CanvasStyle(currentColor(canvas()));
    } else
        checkOrigin(style.canvasPattern());

    realizeSaves();
    State& state = modifiableState();
    state.fillStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state.fillStyle.applyFillColor(c);
    state.unparsedFillColor = String();
}

float CanvasRenderingContext2D::lineWidth() const
{
    return state().lineWidth;
}

void CanvasRenderingContext2D::setLineWidth(float width)
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

String CanvasRenderingContext2D::lineCap() const
{
    return lineCapName(state().lineCap);
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    if (state().lineCap == cap)
        return;
    realizeSaves();
    modifiableState().lineCap = cap;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineCap(cap);
}

String CanvasRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().lineJoin);
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    if (state().lineJoin == join)
        return;
    realizeSaves();
    modifiableState().lineJoin = join;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineJoin(join);
}

float CanvasRenderingContext2D::miterLimit() const
{
    return state().miterLimit;
}

void CanvasRenderingContext2D::setMiterLimit(float limit)
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

float CanvasRenderingContext2D::shadowOffsetX() const
{
    return state().shadowOffset.width();
}

void CanvasRenderingContext2D::setShadowOffsetX(float x)
{
    if (!std::isfinite(x))
        return;
    if (state().shadowOffset.width() == x)
        return;
    realizeSaves();
    modifiableState().shadowOffset.setWidth(x);
    applyShadow();
}

float CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().shadowOffset.height();
}

void CanvasRenderingContext2D::setShadowOffsetY(float y)
{
    if (!std::isfinite(y))
        return;
    if (state().shadowOffset.height() == y)
        return;
    realizeSaves();
    modifiableState().shadowOffset.setHeight(y);
    applyShadow();
}

float CanvasRenderingContext2D::shadowBlur() const
{
    return state().shadowBlur;
}

void CanvasRenderingContext2D::setShadowBlur(float blur)
{
    if (!(std::isfinite(blur) && blur >= 0))
        return;
    if (state().shadowBlur == blur)
        return;
    realizeSaves();
    modifiableState().shadowBlur = blur;
    applyShadow();
}

String CanvasRenderingContext2D::shadowColor() const
{
    return Color(state().shadowColor).serialized();
}

void CanvasRenderingContext2D::setShadowColor(const String& color)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    if (state().shadowColor == rgba)
        return;
    realizeSaves();
    modifiableState().shadowColor = rgba;
    applyShadow();
}

const Vector<float>& CanvasRenderingContext2D::getLineDash() const
{
    return state().lineDash;
}

static bool lineDashSequenceIsValid(const Vector<float>& dash)
{
    for (size_t i = 0; i < dash.size(); i++) {
        if (!std::isfinite(dash[i]) || dash[i] < 0)
            return false;
    }
    return true;
}

void CanvasRenderingContext2D::setLineDash(const Vector<float>& dash)
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

void CanvasRenderingContext2D::setWebkitLineDash(const Vector<float>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;

    realizeSaves();
    modifiableState().lineDash = dash;

    applyLineDash();
}

float CanvasRenderingContext2D::lineDashOffset() const
{
    return state().lineDashOffset;
}

void CanvasRenderingContext2D::setLineDashOffset(float offset)
{
    if (!std::isfinite(offset) || state().lineDashOffset == offset)
        return;

    realizeSaves();
    modifiableState().lineDashOffset = offset;
    applyLineDash();
}

float CanvasRenderingContext2D::webkitLineDashOffset() const
{
    return lineDashOffset();
}

void CanvasRenderingContext2D::setWebkitLineDashOffset(float offset)
{
    setLineDashOffset(offset);
}

void CanvasRenderingContext2D::applyLineDash() const
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    DashArray convertedLineDash(state().lineDash.size());
    for (size_t i = 0; i < state().lineDash.size(); ++i)
        convertedLineDash[i] = static_cast<DashArrayElement>(state().lineDash[i]);
    c->setLineDash(convertedLineDash, state().lineDashOffset);
}

float CanvasRenderingContext2D::globalAlpha() const
{
    return state().globalAlpha;
}

void CanvasRenderingContext2D::setGlobalAlpha(float alpha)
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

String CanvasRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(state().globalComposite, state().globalBlend);
}

void CanvasRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op = CompositeSourceOver;
    BlendMode blendMode = BlendModeNormal;
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

void CanvasRenderingContext2D::scale(float sx, float sy)
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

void CanvasRenderingContext2D::rotate(float angleInRadians)
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

void CanvasRenderingContext2D::translate(float tx, float ty)
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

void CanvasRenderingContext2D::transform(float m11, float m12, float m21, float m22, float dx, float dy)
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

    if (auto inverse = newTransform.inverse()) {
        modifiableState().transform = newTransform;
        c->concatCTM(transform);
        m_path.transform(inverse.value());
        return;
    }
    modifiableState().hasInvertibleTransform = false;
}

void CanvasRenderingContext2D::setTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (!std::isfinite(m11) | !std::isfinite(m21) | !std::isfinite(dx) | !std::isfinite(m12) | !std::isfinite(m22) | !std::isfinite(dy))
        return;

    AffineTransform ctm = state().transform;
    if (!ctm.isInvertible())
        return;

    realizeSaves();
    
    c->setCTM(canvas()->baseTransform());
    modifiableState().transform = AffineTransform();
    m_path.transform(ctm);

    modifiableState().hasInvertibleTransform = true;
    transform(m11, m12, m21, m22, dx, dy);
}

void CanvasRenderingContext2D::setStrokeColor(const String& color)
{
    if (color == state().unparsedStrokeColor)
        return;
    realizeSaves();
    setStrokeStyle(CanvasStyle::createFromString(color, &canvas()->document()));
    modifiableState().unparsedStrokeColor = color;
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel)
{
    if (state().strokeStyle.isValid() && state().strokeStyle.isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setStrokeStyle(CanvasStyle(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color, float alpha)
{
    setStrokeStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel, float alpha)
{
    if (state().strokeStyle.isValid() && state().strokeStyle.isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setStrokeStyle(CanvasStyle(grayLevel, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float r, float g, float b, float a)
{
    if (state().strokeStyle.isValid() && state().strokeStyle.isEquivalentRGBA(r, g, b, a))
        return;
    setStrokeStyle(CanvasStyle(r, g, b, a));
}

void CanvasRenderingContext2D::setStrokeColor(float c, float m, float y, float k, float a)
{
    if (state().strokeStyle.isValid() && state().strokeStyle.isEquivalentCMYKA(c, m, y, k, a))
        return;
    setStrokeStyle(CanvasStyle(c, m, y, k, a));
}

void CanvasRenderingContext2D::setFillColor(const String& color)
{
    if (color == state().unparsedFillColor)
        return;
    realizeSaves();
    setFillStyle(CanvasStyle::createFromString(color, &canvas()->document()));
    modifiableState().unparsedFillColor = color;
}

void CanvasRenderingContext2D::setFillColor(float grayLevel)
{
    if (state().fillStyle.isValid() && state().fillStyle.isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setFillStyle(CanvasStyle(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setFillColor(const String& color, float alpha)
{
    setFillStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel, float alpha)
{
    if (state().fillStyle.isValid() && state().fillStyle.isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setFillStyle(CanvasStyle(grayLevel, alpha));
}

void CanvasRenderingContext2D::setFillColor(float r, float g, float b, float a)
{
    if (state().fillStyle.isValid() && state().fillStyle.isEquivalentRGBA(r, g, b, a))
        return;
    setFillStyle(CanvasStyle(r, g, b, a));
}

void CanvasRenderingContext2D::setFillColor(float c, float m, float y, float k, float a)
{
    if (state().fillStyle.isValid() && state().fillStyle.isEquivalentCMYKA(c, m, y, k, a))
        return;
    setFillStyle(CanvasStyle(c, m, y, k, a));
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

static bool validateRectForCanvas(float& x, float& y, float& width, float& height)
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

#if ENABLE(DASHBOARD_SUPPORT)
void CanvasRenderingContext2D::clearPathForDashboardBackwardCompatibilityMode()
{
    if (m_usesDashboardCompatibilityMode)
        m_path.clear();
}
#endif

static bool isFullCanvasCompositeMode(CompositeOperator op)
{
    // See 4.8.11.1.3 Compositing
    // CompositeSourceAtop and CompositeDestinationOut are not listed here as the platforms already
    // implement the specification's behavior.
    return op == CompositeSourceIn || op == CompositeSourceOut || op == CompositeDestinationIn || op == CompositeDestinationAtop;
}

static bool parseWinding(const String& windingRuleString, WindRule& windRule)
{
    if (windingRuleString == "nonzero")
        windRule = RULE_NONZERO;
    else if (windingRuleString == "evenodd")
        windRule = RULE_EVENODD;
    else
        return false;
    
    return true;
}

void CanvasRenderingContext2D::fill(const String& windingRuleString)
{
    fillInternal(m_path, windingRuleString);

#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

void CanvasRenderingContext2D::stroke()
{
    strokeInternal(m_path);

#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

void CanvasRenderingContext2D::clip(const String& windingRuleString)
{
    clipInternal(m_path, windingRuleString);

#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

void CanvasRenderingContext2D::fill(DOMPath* path, const String& windingRuleString)
{
    fillInternal(path->path(), windingRuleString);
}

void CanvasRenderingContext2D::stroke(DOMPath* path)
{
    strokeInternal(path->path());
}

void CanvasRenderingContext2D::clip(DOMPath* path, const String& windingRuleString)
{
    clipInternal(path->path(), windingRuleString);
}

void CanvasRenderingContext2D::fillInternal(const Path& path, const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    if (!path.isEmpty()) {
        WindRule windRule = c->fillRule();
        WindRule newWindRule = RULE_NONZERO;
        if (!parseWinding(windingRuleString, newWindRule))
            return;
        c->setFillRule(newWindRule);

        if (isFullCanvasCompositeMode(state().globalComposite)) {
            beginCompositeLayer();
            c->fillPath(path);
            endCompositeLayer();
            didDrawEntireCanvas();
        } else if (state().globalComposite == CompositeCopy) {
            clearCanvas();
            c->fillPath(path);
            didDrawEntireCanvas();
        } else {
            c->fillPath(path);
            didDraw(path.fastBoundingRect());
        }
        
        c->setFillRule(windRule);
    }
}

void CanvasRenderingContext2D::strokeInternal(const Path& path)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    if (!path.isEmpty()) {
        if (isFullCanvasCompositeMode(state().globalComposite)) {
            beginCompositeLayer();
            c->strokePath(path);
            endCompositeLayer();
            didDrawEntireCanvas();
        } else if (state().globalComposite == CompositeCopy) {
            clearCanvas();
            c->strokePath(path);
            didDrawEntireCanvas();
        } else {
            FloatRect dirtyRect = path.fastBoundingRect();
            inflateStrokeRect(dirtyRect);
            c->strokePath(path);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::clipInternal(const Path& path, const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    WindRule newWindRule = RULE_NONZERO;
    if (!parseWinding(windingRuleString, newWindRule))
        return;

    realizeSaves();
    c->canvasClip(path, newWindRule);
}

inline void CanvasRenderingContext2D::beginCompositeLayer()
{
#if !USE(CAIRO)
    drawingContext()->beginTransparencyLayer(1);
#endif
}

inline void CanvasRenderingContext2D::endCompositeLayer()
{
#if !USE(CAIRO)
    drawingContext()->endTransparencyLayer();    
#endif
}

bool CanvasRenderingContext2D::isPointInPath(const float x, const float y, const String& windingRuleString)
{
    return isPointInPathInternal(m_path, x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInStroke(const float x, const float y)
{
    return isPointInStrokeInternal(m_path, x, y);
}

bool CanvasRenderingContext2D::isPointInPath(DOMPath* path, const float x, const float y, const String& windingRuleString)
{
    return isPointInPathInternal(path->path(), x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInStroke(DOMPath* path, const float x, const float y)
{
    return isPointInStrokeInternal(path->path(), x, y);
}

bool CanvasRenderingContext2D::isPointInPathInternal(const Path& path, float x, float y, const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().hasInvertibleTransform)
        return false;

    FloatPoint transformedPoint = state().transform.inverse().valueOr(AffineTransform()).mapPoint(FloatPoint(x, y));

    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

    WindRule windRule = RULE_NONZERO;
    if (!parseWinding(windingRuleString, windRule))
        return false;
    
    return path.contains(transformedPoint, windRule);
}

bool CanvasRenderingContext2D::isPointInStrokeInternal(const Path& path, float x, float y)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().hasInvertibleTransform)
        return false;

    FloatPoint transformedPoint = state().transform.inverse().valueOr(AffineTransform()).mapPoint(FloatPoint(x, y));
    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

    CanvasStrokeStyleApplier applier(this);
    return path.strokeContains(&applier, transformedPoint);
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    GraphicsContext* context = drawingContext();
    if (!context)
        return;
    if (!state().hasInvertibleTransform)
        return;
    FloatRect rect(x, y, width, height);

    bool saved = false;
    if (shouldDrawShadows()) {
        context->save();
        saved = true;
        context->setLegacyShadow(FloatSize(), 0, Color::transparent);
    }
    if (state().globalAlpha != 1) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setAlpha(1);
    }
    if (state().globalComposite != CompositeSourceOver) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setCompositeOperation(CompositeSourceOver);
    }
    context->clearRect(rect);
    if (saved)
        context->restore();
    didDraw(rect);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    // from the HTML5 Canvas spec:
    // If x0 = x1 and y0 = y1, then the linear gradient must paint nothing
    // If x0 = x1 and y0 = y1 and r0 = r1, then the radial gradient must paint nothing
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);

    if (rectContainsCanvas(rect)) {
        c->fillRect(rect);
        didDrawEntireCanvas();
    } else if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        c->fillRect(rect);
        endCompositeLayer();
        didDrawEntireCanvas();
    } else if (state().globalComposite == CompositeCopy) {
        clearCanvas();
        c->fillRect(rect);
        didDrawEntireCanvas();
    } else {
        c->fillRect(rect);
        didDraw(rect);
    }
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;
    if (!(state().lineWidth >= 0))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);
    if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        c->strokeRect(rect, state().lineWidth);
        endCompositeLayer();
        didDrawEntireCanvas();
    } else if (state().globalComposite == CompositeCopy) {
        clearCanvas();
        c->strokeRect(rect, state().lineWidth);
        didDrawEntireCanvas();
    } else {
        FloatRect boundingRect = rect;
        boundingRect.inflate(state().lineWidth / 2);
        c->strokeRect(rect, state().lineWidth);
        didDraw(boundingRect);
    }
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur)
{
    setShadow(FloatSize(width, height), blur, Color::transparent);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    setShadow(FloatSize(width, height), blur, rgba);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, 1));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    setShadow(FloatSize(width, height), blur, colorWithOverrideAlpha(rgba, alpha));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, alpha));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(r, g, b, a));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    setShadow(FloatSize(width, height), blur, makeRGBAFromCMYKA(c, m, y, k, a));
}

void CanvasRenderingContext2D::clearShadow()
{
    setShadow(FloatSize(), 0, Color::transparent);
}

void CanvasRenderingContext2D::setShadow(const FloatSize& offset, float blur, RGBA32 color)
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

void CanvasRenderingContext2D::applyShadow()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (shouldDrawShadows()) {
        float width = state().shadowOffset.width();
        float height = state().shadowOffset.height();
        c->setLegacyShadow(FloatSize(width, -height), state().shadowBlur, state().shadowColor);
    } else
        c->setLegacyShadow(FloatSize(), 0, Color::transparent);
}

bool CanvasRenderingContext2D::shouldDrawShadows() const
{
    return alphaChannel(state().shadowColor) && (state().shadowBlur || !state().shadowOffset.isZero());
}

enum ImageSizeType {
    ImageSizeAfterDevicePixelRatio,
    ImageSizeBeforeDevicePixelRatio
};

static LayoutSize size(HTMLImageElement* imageElement, ImageSizeType sizeType)
{
    LayoutSize size;
    if (CachedImage* cachedImage = imageElement->cachedImage()) {
        size = cachedImage->imageSizeForRenderer(imageElement->renderer(), 1.0f); // FIXME: Not sure about this.

        if (sizeType == ImageSizeAfterDevicePixelRatio && is<RenderImage>(imageElement->renderer()) && cachedImage->image() && !cachedImage->image()->hasRelativeWidth())
            size.scale(downcast<RenderImage>(*imageElement->renderer()).imageDevicePixelRatio());
    }
    return size;
}

#if ENABLE(VIDEO)
static FloatSize size(HTMLVideoElement* video)
{
    if (MediaPlayer* player = video->player())
        return player->naturalSize();
    return FloatSize();
}
#endif

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(std::min(rect.x(), rect.maxX()),
        std::min(rect.y(), rect.maxY()),
        std::max(rect.width(), -rect.width()),
        std::max(rect.height(), -rect.height()));
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* imageElement, float x, float y, ExceptionCode& ec)
{
    if (!imageElement) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    LayoutSize destRectSize = size(imageElement, ImageSizeAfterDevicePixelRatio);
    drawImage(imageElement, x, y, destRectSize.width(), destRectSize.height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* imageElement,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    if (!imageElement) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    LayoutSize sourceRectSize = size(imageElement, ImageSizeBeforeDevicePixelRatio);
    drawImage(imageElement, FloatRect(0, 0, sourceRectSize.width(), sourceRectSize.height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* imageElement,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    drawImage(imageElement, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* imageElement, const FloatRect& srcRect, const FloatRect& dstRect, ExceptionCode& ec)
{
    drawImage(imageElement, srcRect, dstRect, state().globalComposite, state().globalBlend, ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* imageElement, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator& op, const BlendMode& blendMode, ExceptionCode& ec)
{
    if (!imageElement) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    ec = 0;

    if (!std::isfinite(dstRect.x()) || !std::isfinite(dstRect.y()) || !std::isfinite(dstRect.width()) || !std::isfinite(dstRect.height())
        || !std::isfinite(srcRect.x()) || !std::isfinite(srcRect.y()) || !std::isfinite(srcRect.width()) || !std::isfinite(srcRect.height()))
        return;

    if (!dstRect.width() || !dstRect.height())
        return;

    if (!imageElement->complete())
        return;

    FloatRect normalizedSrcRect = normalizeRect(srcRect);
    FloatRect normalizedDstRect = normalizeRect(dstRect);

    FloatRect imageRect = FloatRect(FloatPoint(), size(imageElement, ImageSizeBeforeDevicePixelRatio));
    if (!srcRect.width() || !srcRect.height()) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    if (!imageRect.contains(normalizedSrcRect))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    CachedImage* cachedImage = imageElement->cachedImage();
    if (!cachedImage)
        return;

    Image* image = cachedImage->imageForRenderer(imageElement->renderer());
    if (!image)
        return;
    
    ImageObserver* observer = image->imageObserver();

    if (image->isSVGImage()) {
        image->setImageObserver(nullptr);
        image->setContainerSize(normalizedSrcRect.size());
    }

    if (rectContainsCanvas(normalizedDstRect)) {
        c->drawImage(*image, normalizedDstRect, normalizedSrcRect, ImagePaintingOptions(op, blendMode));
        didDrawEntireCanvas();
    } else if (isFullCanvasCompositeMode(op)) {
        fullCanvasCompositedDrawImage(*image, normalizedDstRect, normalizedSrcRect, op);
        didDrawEntireCanvas();
    } else if (op == CompositeCopy) {
        clearCanvas();
        c->drawImage(*image, normalizedDstRect, normalizedSrcRect, ImagePaintingOptions(op, blendMode));
        didDrawEntireCanvas();
    } else {
        c->drawImage(*image, normalizedDstRect, normalizedSrcRect, ImagePaintingOptions(op, blendMode));
        didDraw(normalizedDstRect);
    }
    
    if (image->isSVGImage())
        image->setImageObserver(observer);

    checkOrigin(imageElement);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas, float x, float y, ExceptionCode& ec)
{
    drawImage(sourceCanvas, 0, 0, sourceCanvas->width(), sourceCanvas->height(), x, y, sourceCanvas->width(), sourceCanvas->height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    drawImage(sourceCanvas, FloatRect(0, 0, sourceCanvas->width(), sourceCanvas->height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    drawImage(sourceCanvas, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas, const FloatRect& srcRect,
    const FloatRect& dstRect, ExceptionCode& ec)
{
    if (!sourceCanvas) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    FloatRect srcCanvasRect = FloatRect(FloatPoint(), sourceCanvas->size());

    if (!srcCanvasRect.width() || !srcCanvasRect.height()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!srcRect.width() || !srcRect.height()) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    ec = 0;

    if (!srcCanvasRect.contains(normalizeRect(srcRect)) || !dstRect.width() || !dstRect.height())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    // FIXME: Do this through platform-independent GraphicsContext API.
    ImageBuffer* buffer = sourceCanvas->buffer();
    if (!buffer)
        return;

    checkOrigin(sourceCanvas);

#if ENABLE(ACCELERATED_2D_CANVAS)
    // If we're drawing from one accelerated canvas 2d to another, avoid calling sourceCanvas->makeRenderingResultsAvailable()
    // as that will do a readback to software.
    CanvasRenderingContext* sourceContext = sourceCanvas->renderingContext();
    // FIXME: Implement an accelerated path for drawing from a WebGL canvas to a 2d canvas when possible.
    if (!isAccelerated() || !sourceContext || !sourceContext->isAccelerated() || !sourceContext->is2d())
        sourceCanvas->makeRenderingResultsAvailable();
#else
    sourceCanvas->makeRenderingResultsAvailable();
#endif

    if (rectContainsCanvas(dstRect)) {
        c->drawImageBuffer(*buffer, dstRect, srcRect, ImagePaintingOptions(state().globalComposite, state().globalBlend));
        didDrawEntireCanvas();
    } else if (isFullCanvasCompositeMode(state().globalComposite)) {
        fullCanvasCompositedDrawImage(*buffer, dstRect, srcRect, state().globalComposite);
        didDrawEntireCanvas();
    } else if (state().globalComposite == CompositeCopy) {
        clearCanvas();
        c->drawImageBuffer(*buffer, dstRect, srcRect, ImagePaintingOptions(state().globalComposite, state().globalBlend));
        didDrawEntireCanvas();
    } else {
        c->drawImageBuffer(*buffer, dstRect, srcRect, ImagePaintingOptions(state().globalComposite, state().globalBlend));
        didDraw(dstRect);
    }
}

#if ENABLE(VIDEO)
void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video, float x, float y, ExceptionCode& ec)
{
    if (!video) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    FloatSize videoSize = size(video);
    drawImage(video, x, y, videoSize.width(), videoSize.height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video,
                                         float x, float y, float width, float height, ExceptionCode& ec)
{
    if (!video) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    FloatSize videoSize = size(video);
    drawImage(video, FloatRect(0, 0, videoSize.width(), videoSize.height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    drawImage(video, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video, const FloatRect& srcRect, const FloatRect& dstRect,
                                         ExceptionCode& ec)
{
    if (!video) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    ec = 0;

    if (video->readyState() == HTMLMediaElement::HAVE_NOTHING || video->readyState() == HTMLMediaElement::HAVE_METADATA)
        return;

    FloatRect videoRect = FloatRect(FloatPoint(), size(video));
    if (!srcRect.width() || !srcRect.height()) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!videoRect.contains(normalizeRect(srcRect)) || !dstRect.width() || !dstRect.height())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

    checkOrigin(video);

#if USE(CG) || (ENABLE(ACCELERATED_2D_CANVAS) && USE(GSTREAMER_GL) && USE(CAIRO))
    if (PassNativeImagePtr image = video->nativeImageForCurrentTime()) {
        c->drawNativeImage(image, FloatSize(video->videoWidth(), video->videoHeight()), dstRect, srcRect);
        if (rectContainsCanvas(dstRect))
            didDrawEntireCanvas();
        else
            didDraw(dstRect);

        return;
    }
#endif

    GraphicsContextStateSaver stateSaver(*c);
    c->clip(dstRect);
    c->translate(dstRect.x(), dstRect.y());
    c->scale(FloatSize(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height()));
    c->translate(-srcRect.x(), -srcRect.y());
    video->paintCurrentFrameInContext(*c, FloatRect(FloatPoint(), size(video)));
    stateSaver.restore();
    didDraw(dstRect);
}
#endif

void CanvasRenderingContext2D::drawImageFromRect(HTMLImageElement* imageElement,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh,
    const String& compositeOperation)
{
    CompositeOperator op;
    BlendMode blendOp = BlendModeNormal;
    if (!parseCompositeAndBlendOperator(compositeOperation, op, blendOp) || blendOp != BlendModeNormal)
        op = CompositeSourceOver;

    drawImage(imageElement, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), op, BlendModeNormal, IGNORE_EXCEPTION);
}

void CanvasRenderingContext2D::setAlpha(float alpha)
{
    setGlobalAlpha(alpha);
}

void CanvasRenderingContext2D::setCompositeOperation(const String& operation)
{
    setGlobalCompositeOperation(operation);
}

void CanvasRenderingContext2D::clearCanvas()
{
    FloatRect canvasRect(0, 0, canvas()->width(), canvas()->height());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->save();
    c->setCTM(canvas()->baseTransform());
    c->clearRect(canvasRect);
    c->restore();
}

Path CanvasRenderingContext2D::transformAreaToDevice(const Path& path) const
{
    Path transformed(path);
    transformed.transform(state().transform);
    transformed.transform(canvas()->baseTransform());
    return transformed;
}

Path CanvasRenderingContext2D::transformAreaToDevice(const FloatRect& rect) const
{
    Path path;
    path.addRect(rect);
    return transformAreaToDevice(path);
}

bool CanvasRenderingContext2D::rectContainsCanvas(const FloatRect& rect) const
{
    FloatQuad quad(rect);
    FloatQuad canvasQuad(FloatRect(0, 0, canvas()->width(), canvas()->height()));
    return state().transform.mapQuad(quad).containsQuad(canvasQuad);
}

template<class T> IntRect CanvasRenderingContext2D::calculateCompositingBufferRect(const T& area, IntSize* croppedOffset)
{
    IntRect canvasRect(0, 0, canvas()->width(), canvas()->height());
    canvasRect = canvas()->baseTransform().mapRect(canvasRect);
    Path path = transformAreaToDevice(area);
    IntRect bufferRect = enclosingIntRect(path.fastBoundingRect());
    IntPoint originalLocation = bufferRect.location();
    bufferRect.intersect(canvasRect);
    if (croppedOffset)
        *croppedOffset = originalLocation - bufferRect.location();
    return bufferRect;
}

std::unique_ptr<ImageBuffer> CanvasRenderingContext2D::createCompositingBuffer(const IntRect& bufferRect)
{
    return ImageBuffer::create(bufferRect.size(), isAccelerated() ? Accelerated : Unaccelerated);
}

void CanvasRenderingContext2D::compositeBuffer(ImageBuffer& buffer, const IntRect& bufferRect, CompositeOperator op)
{
    IntRect canvasRect(0, 0, canvas()->width(), canvas()->height());
    canvasRect = canvas()->baseTransform().mapRect(canvasRect);

    GraphicsContext* c = drawingContext();
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

static void drawImageToContext(Image& image, GraphicsContext& context, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    context.drawImage(image, dest, src, op);
}

static void drawImageToContext(ImageBuffer& imageBuffer, GraphicsContext& context, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    context.drawImageBuffer(imageBuffer, dest, src, op);
}

template<class T> void CanvasRenderingContext2D::fullCanvasCompositedDrawImage(T& image, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    ASSERT(isFullCanvasCompositeMode(op));

    IntSize croppedOffset;
    IntRect bufferRect = calculateCompositingBufferRect(dest, &croppedOffset);
    if (bufferRect.isEmpty()) {
        clearCanvas();
        return;
    }

    std::unique_ptr<ImageBuffer> buffer = createCompositingBuffer(bufferRect);
    if (!buffer)
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    FloatRect adjustedDest = dest;
    adjustedDest.setLocation(FloatPoint(0, 0));
    AffineTransform effectiveTransform = c->getCTM();
    IntRect transformedAdjustedRect = enclosingIntRect(effectiveTransform.mapRect(adjustedDest));
    buffer->context().translate(-transformedAdjustedRect.location().x(), -transformedAdjustedRect.location().y());
    buffer->context().translate(croppedOffset.width(), croppedOffset.height());
    buffer->context().concatCTM(effectiveTransform);
    drawImageToContext(image, buffer->context(), adjustedDest, src, CompositeSourceOver);

    compositeBuffer(*buffer, bufferRect, op);
}

void CanvasRenderingContext2D::prepareGradientForDashboard(CanvasGradient& gradient) const
{
#if ENABLE(DASHBOARD_SUPPORT)
    if (m_usesDashboardCompatibilityMode)
        gradient.setDashboardCompatibilityMode();
#else
    UNUSED_PARAM(gradient);
#endif
}

RefPtr<CanvasGradient> CanvasRenderingContext2D::createLinearGradient(float x0, float y0, float x1, float y1, ExceptionCode& ec)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1)) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    Ref<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
    prepareGradientForDashboard(gradient.get());
    return WTFMove(gradient);
}

RefPtr<CanvasGradient> CanvasRenderingContext2D::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, ExceptionCode& ec)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(r0) || !std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(r1)) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    if (r0 < 0 || r1 < 0) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }

    Ref<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
    prepareGradientForDashboard(gradient.get());
    return WTFMove(gradient);
}

RefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLImageElement* imageElement,
    const String& repetitionType, ExceptionCode& ec)
{
    if (!imageElement) {
        ec = TYPE_MISMATCH_ERR;
        return nullptr;
    }
    bool repeatX, repeatY;
    ec = 0;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return nullptr;

    CachedImage* cachedImage = imageElement->cachedImage();
    // If the image loading hasn't started or the image is not complete, it is not fully decodable.
    if (!cachedImage || !imageElement->complete())
        return nullptr;

    if (cachedImage->status() == CachedResource::LoadError) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (!imageElement->cachedImage()->imageForRenderer(imageElement->renderer()))
        return CanvasPattern::create(Image::nullImage(), repeatX, repeatY, true);

    bool originClean = cachedImage->isOriginClean(canvas()->securityOrigin());

    // FIXME: SVG images with animations can switch between clean and dirty (leaking cross-origin
    // data). We should either:
    //   1) Take a fixed snapshot of an SVG image when creating a pattern and determine then whether
    //      the origin is clean.
    //   2) Dynamically verify the origin checks at draw time, and dirty the canvas accordingly.
    // To be on the safe side, taint the origin for all patterns containing SVG images for now.
    if (cachedImage->image()->isSVGImage())
        originClean = false;

    return CanvasPattern::create(cachedImage->imageForRenderer(imageElement->renderer()), repeatX, repeatY, originClean);
}

RefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLCanvasElement* canvas,
    const String& repetitionType, ExceptionCode& ec)
{
    if (!canvas) {
        ec = TYPE_MISMATCH_ERR;
        return nullptr;
    }
    if (!canvas->width() || !canvas->height() || !canvas->buffer()) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    bool repeatX, repeatY;
    ec = 0;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return nullptr;
    return CanvasPattern::create(canvas->copiedImage(), repeatX, repeatY, canvas->originClean());
}

void CanvasRenderingContext2D::didDrawEntireCanvas()
{
    didDraw(FloatRect(FloatPoint::zero(), canvas()->size()), CanvasDidDrawApplyClip);
}

void CanvasRenderingContext2D::didDraw(const FloatRect& r, unsigned options)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;

#if ENABLE(ACCELERATED_2D_CANVAS)
    // If we are drawing to hardware and we have a composited layer, just call contentChanged().
    if (isAccelerated()) {
        RenderBox* renderBox = canvas()->renderBox();
        if (renderBox && renderBox->hasAcceleratedCompositing()) {
            renderBox->contentChanged(CanvasPixelsChanged);
            canvas()->clearCopiedImage();
            canvas()->notifyObserversCanvasChanged(r);
            return;
        }
    }
#endif

    FloatRect dirtyRect = r;
    if (options & CanvasDidDrawApplyTransform) {
        AffineTransform ctm = state().transform;
        dirtyRect = ctm.mapRect(r);
    }

    if (options & CanvasDidDrawApplyShadow && alphaChannel(state().shadowColor)) {
        // The shadow gets applied after transformation
        FloatRect shadowRect(dirtyRect);
        shadowRect.move(state().shadowOffset);
        shadowRect.inflate(state().shadowBlur);
        dirtyRect.unite(shadowRect);
    }

    if (options & CanvasDidDrawApplyClip) {
        // FIXME: apply the current clip to the rectangle. Unfortunately we can't get the clip
        // back out of the GraphicsContext, so to take clip into account for incremental painting,
        // we'd have to keep the clip path around.
    }

    canvas()->didDraw(dirtyRect);
}

void CanvasRenderingContext2D::setTracksDisplayListReplay(bool tracksDisplayListReplay)
{
    if (tracksDisplayListReplay == m_tracksDisplayListReplay)
        return;

    m_tracksDisplayListReplay = tracksDisplayListReplay;
    if (!m_tracksDisplayListReplay)
        contextDisplayListMap().remove(this);
}

String CanvasRenderingContext2D::displayListAsText(DisplayList::AsTextFlags flags) const
{
    if (m_recordingContext)
        return m_recordingContext->displayList.asText(flags);
    
    return String();
}

String CanvasRenderingContext2D::replayDisplayListAsText(DisplayList::AsTextFlags flags) const
{
    auto it = contextDisplayListMap().find(this);
    if (it != contextDisplayListMap().end()) {
        TextStream stream;
        stream << it->value->asText(flags);
        return stream.release();
    }

    return String();
}

void CanvasRenderingContext2D::paintRenderingResultsToCanvas()
{
    if (UNLIKELY(m_usesDisplayListDrawing)) {
        if (!m_recordingContext)
            return;
        
        FloatRect clip(FloatPoint::zero(), canvas()->size());
        DisplayList::Replayer replayer(*canvas()->drawingContext(), m_recordingContext->displayList);

        if (UNLIKELY(m_tracksDisplayListReplay)) {
            auto replayList = replayer.replay(clip, m_tracksDisplayListReplay);
            contextDisplayListMap().add(this, WTFMove(replayList));
        } else
            replayer.replay(clip);

        m_recordingContext->displayList.clear();
    }
}

GraphicsContext* CanvasRenderingContext2D::drawingContext() const
{
    if (UNLIKELY(m_usesDisplayListDrawing)) {
        if (!m_recordingContext)
            m_recordingContext = std::make_unique<DisplayListDrawingContext>(FloatRect(FloatPoint::zero(), canvas()->size()));

        return &m_recordingContext->context;
    }

    return canvas()->drawingContext();
}

static RefPtr<ImageData> createEmptyImageData(const IntSize& size)
{
    if (RefPtr<ImageData> data = ImageData::create(size)) {
        data->data()->zeroFill();
        return data;
    }

    return nullptr;
}

RefPtr<ImageData> CanvasRenderingContext2D::createImageData(RefPtr<ImageData>&& imageData, ExceptionCode& ec) const
{
    if (!imageData) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    return createEmptyImageData(imageData->size());
}

RefPtr<ImageData> CanvasRenderingContext2D::createImageData(float sw, float sh, ExceptionCode& ec) const
{
    ec = 0;
    if (!sw || !sh) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }
    if (!std::isfinite(sw) || !std::isfinite(sh)) {
        ec = TypeError;
        return nullptr;
    }

    FloatSize logicalSize(fabs(sw), fabs(sh));
    if (!logicalSize.isExpressibleAsIntSize())
        return nullptr;

    IntSize size = expandedIntSize(logicalSize);
    if (size.width() < 1)
        size.setWidth(1);
    if (size.height() < 1)
        size.setHeight(1);

    return createEmptyImageData(size);
}

RefPtr<ImageData> CanvasRenderingContext2D::getImageData(float sx, float sy, float sw, float sh, ExceptionCode& ec) const
{
    return getImageData(ImageBuffer::LogicalCoordinateSystem, sx, sy, sw, sh, ec);
}

RefPtr<ImageData> CanvasRenderingContext2D::webkitGetImageDataHD(float sx, float sy, float sw, float sh, ExceptionCode& ec) const
{
    return getImageData(ImageBuffer::BackingStoreCoordinateSystem, sx, sy, sw, sh, ec);
}

RefPtr<ImageData> CanvasRenderingContext2D::getImageData(ImageBuffer::CoordinateSystem coordinateSystem, float sx, float sy, float sw, float sh, ExceptionCode& ec) const
{
    if (!canvas()->originClean()) {
        static NeverDestroyed<String> consoleMessage(ASCIILiteral("Unable to get image data from canvas because the canvas has been tainted by cross-origin data."));
        canvas()->document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage);
        ec = SECURITY_ERR;
        return nullptr;
    }

    if (!sw || !sh) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }
    if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sw) || !std::isfinite(sh)) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }    
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }

    FloatRect logicalRect(sx, sy, sw, sh);
    if (logicalRect.width() < 1)
        logicalRect.setWidth(1);
    if (logicalRect.height() < 1)
        logicalRect.setHeight(1);
    if (!logicalRect.isExpressibleAsIntRect())
        return nullptr;

    IntRect imageDataRect = enclosingIntRect(logicalRect);
    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return createEmptyImageData(imageDataRect.size());

    RefPtr<Uint8ClampedArray> byteArray = buffer->getUnmultipliedImageData(imageDataRect, coordinateSystem);
    if (!byteArray)
        return nullptr;

    return ImageData::create(imageDataRect.size(), byteArray.release());
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, ExceptionCode& ec)
{
    if (!data) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    putImageData(data, dx, dy, 0, 0, data->width(), data->height(), ec);
}

void CanvasRenderingContext2D::webkitPutImageDataHD(ImageData* data, float dx, float dy, ExceptionCode& ec)
{
    if (!data) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    webkitPutImageDataHD(data, dx, dy, 0, 0, data->width(), data->height(), ec);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, float dirtyX, float dirtyY,
                                            float dirtyWidth, float dirtyHeight, ExceptionCode& ec)
{
    putImageData(data, ImageBuffer::LogicalCoordinateSystem, dx, dy, dirtyX, dirtyY, dirtyWidth, dirtyHeight, ec);
}

void CanvasRenderingContext2D::webkitPutImageDataHD(ImageData* data, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight, ExceptionCode& ec)
{
    putImageData(data, ImageBuffer::BackingStoreCoordinateSystem, dx, dy, dirtyX, dirtyY, dirtyWidth, dirtyHeight, ec);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Element* element)
{
    drawFocusIfNeededInternal(m_path, element);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(DOMPath* path, Element* element)
{
    drawFocusIfNeededInternal(path->path(), element);
}

void CanvasRenderingContext2D::drawFocusIfNeededInternal(const Path& path, Element* element)
{
    GraphicsContext* context = drawingContext();

    if (!element || !element->focused() || !state().hasInvertibleTransform || path.isEmpty()
        || !element->isDescendantOf(canvas()) || !context)
        return;

    context->drawFocusRing(path, 1, 1, RenderTheme::focusRingColor());
}

void CanvasRenderingContext2D::putImageData(ImageData* data, ImageBuffer::CoordinateSystem coordinateSystem, float dx, float dy, float dirtyX, float dirtyY,
                                            float dirtyWidth, float dirtyHeight, ExceptionCode& ec)
{
    if (!data) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dirtyX) || !std::isfinite(dirtyY) || !std::isfinite(dirtyWidth) || !std::isfinite(dirtyHeight)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect destRect = enclosingIntRect(clipRect);
    destRect.move(destOffset);
    destRect.intersect(IntRect(IntPoint(), coordinateSystem == ImageBuffer::LogicalCoordinateSystem ? buffer->logicalSize() : buffer->internalSize()));
    if (destRect.isEmpty())
        return;
    IntRect sourceRect(destRect);
    sourceRect.move(-destOffset);

    buffer->putByteArray(Unmultiplied, data->data(), IntSize(data->width(), data->height()), sourceRect, IntPoint(destOffset), coordinateSystem);

    didDraw(destRect, CanvasDidDrawApplyNone); // ignore transform, shadow and clip
}

String CanvasRenderingContext2D::font() const
{
    if (!state().font.realized())
        return defaultFont;

    StringBuilder serializedFont;
    const auto& fontDescription = state().font.fontDescription();

    if (fontDescription.italic())
        serializedFont.appendLiteral("italic ");
    if (fontDescription.variantCaps() == FontVariantCaps::Small)
        serializedFont.appendLiteral("small-caps ");

    serializedFont.appendNumber(fontDescription.computedPixelSize());
    serializedFont.appendLiteral("px");

    for (unsigned i = 0; i < fontDescription.familyCount(); ++i) {
        if (i)
            serializedFont.append(',');
        // FIXME: We should append family directly to serializedFont rather than building a temporary string.
        String family = fontDescription.familyAt(i);
        if (family.startsWith("-webkit-"))
            family = family.substring(8);
        if (family.contains(' '))
            family = makeString('"', family, '"');

        serializedFont.append(' ');
        serializedFont.append(family);
    }

    return serializedFont.toString();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    if (newFont == state().unparsedFont && state().font.realized())
        return;

    RefPtr<MutableStyleProperties> parsedStyle = MutableStyleProperties::create();
    CSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, newFont, true, strictToCSSParserMode(!m_usesCSSCompatibilityParseMode), nullptr);
    if (parsedStyle->isEmpty())
        return;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);

    // According to http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit" and "initial" values must be ignored.
    if (fontValue == "inherit" || fontValue == "initial")
        return;

    // The parse succeeded.
    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    realizeSaves();
    modifiableState().unparsedFont = newFontSafeCopy;

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    Ref<RenderStyle> newStyle = RenderStyle::create();

    Document& document = canvas()->document();
    document.updateStyleIfNeeded();

    if (RenderStyle* computedStyle = canvas()->computedStyle())
        newStyle->setFontDescription(computedStyle->fontDescription());
    else {
        FontCascadeDescription defaultFontDescription;
        defaultFontDescription.setOneFamily(defaultFontFamily);
        defaultFontDescription.setSpecifiedSize(defaultFontSize);
        defaultFontDescription.setComputedSize(defaultFontSize);

        newStyle->setFontDescription(defaultFontDescription);
    }

    newStyle->fontCascade().update(&document.fontSelector());

    // Now map the font property longhands into the style.
    StyleResolver& styleResolver = canvas()->styleResolver();
    styleResolver.applyPropertyToStyle(CSSPropertyFontFamily, parsedStyle->getPropertyCSSValue(CSSPropertyFontFamily).get(), &newStyle.get());
    styleResolver.applyPropertyToCurrentStyle(CSSPropertyFontStyle, parsedStyle->getPropertyCSSValue(CSSPropertyFontStyle).get());
    styleResolver.applyPropertyToCurrentStyle(CSSPropertyFontVariantCaps, parsedStyle->getPropertyCSSValue(CSSPropertyFontVariantCaps).get());
    styleResolver.applyPropertyToCurrentStyle(CSSPropertyFontWeight, parsedStyle->getPropertyCSSValue(CSSPropertyFontWeight).get());

    // As described in BUG66291, setting font-size and line-height on a font may entail a CSSPrimitiveValue::computeLengthDouble call,
    // which assumes the fontMetrics are available for the affected font, otherwise a crash occurs (see http://trac.webkit.org/changeset/96122).
    // The updateFont() calls below update the fontMetrics and ensure the proper setting of font-size and line-height.
    styleResolver.updateFont();
    styleResolver.applyPropertyToCurrentStyle(CSSPropertyFontSize, parsedStyle->getPropertyCSSValue(CSSPropertyFontSize).get());
    styleResolver.updateFont();
    styleResolver.applyPropertyToCurrentStyle(CSSPropertyLineHeight, parsedStyle->getPropertyCSSValue(CSSPropertyLineHeight).get());

    modifiableState().font.initialize(document.fontSelector(), newStyle);
}

String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().textAlign);
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    if (state().textAlign == align)
        return;
    realizeSaves();
    modifiableState().textAlign = align;
}

String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().textBaseline);
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    if (state().textBaseline == baseline)
        return;
    realizeSaves();
    modifiableState().textBaseline = baseline;
}

inline TextDirection CanvasRenderingContext2D::toTextDirection(Direction direction, RenderStyle** computedStyle) const
{
    RenderStyle* style = (computedStyle || direction == Direction::Inherit) ? canvas()->computedStyle() : nullptr;
    if (computedStyle)
        *computedStyle = style;
    switch (direction) {
    case Direction::Inherit:
        return style ? style->direction() : LTR;
    case Direction::RTL:
        return RTL;
    case Direction::LTR:
        return LTR;
    }
    ASSERT_NOT_REACHED();
    return LTR;
}

String CanvasRenderingContext2D::direction() const
{
    if (state().direction == Direction::Inherit)
        canvas()->document().updateStyleIfNeeded();
    return toTextDirection(state().direction) == RTL ? ASCIILiteral("rtl") : ASCIILiteral("ltr");
}

void CanvasRenderingContext2D::setDirection(const String& directionString)
{
    Direction direction;
    if (directionString == "inherit")
        direction = Direction::Inherit;
    else if (directionString == "rtl")
        direction = Direction::RTL;
    else if (directionString == "ltr")
        direction = Direction::LTR;
    else
        return;

    if (state().direction == direction)
        return;

    realizeSaves();
    modifiableState().direction = direction;
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, true);
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth, true);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, false);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth, true);
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

static void normalizeSpaces(String& text)
{
    size_t i = text.find(isSpaceThatNeedsReplacing);

    if (i == notFound)
        return;

    unsigned textLength = text.length();
    Vector<UChar> charVector(textLength);
    StringView(text).getCharactersWithUpconvert(charVector.data());

    charVector[i++] = ' ';

    for (; i < textLength; ++i) {
        if (isSpaceThatNeedsReplacing(charVector[i]))
            charVector[i] = ' ';
    }
    text = String::adopt(charVector);
}

Ref<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    Ref<TextMetrics> metrics = TextMetrics::create();

    String normalizedText = text;
    normalizeSpaces(normalizedText);

    metrics->setWidth(fontProxy().width(TextRun(normalizedText)));

    return metrics;
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, float maxWidth, bool useMaxWidth)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;
    if (!std::isfinite(x) | !std::isfinite(y))
        return;
    if (useMaxWidth && (!std::isfinite(maxWidth) || maxWidth <= 0))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (!fill && gradient && gradient->isZeroSize())
        return;

    gradient = c->fillGradient();
    if (fill && gradient && gradient->isZeroSize())
        return;

    const auto& fontProxy = this->fontProxy();
    const FontMetrics& fontMetrics = fontProxy.fontMetrics();

    String normalizedText = text;
    normalizeSpaces(normalizedText);

    // FIXME: Need to turn off font smoothing.

    RenderStyle* computedStyle;
    canvas()->document().updateStyleIfNeeded();
    TextDirection direction = toTextDirection(state().direction, &computedStyle);
    bool isRTL = direction == RTL;
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(normalizedText, 0, 0, AllowTrailingExpansion, direction, override, true);
    // Draw the item text at the correct point.
    FloatPoint location(x, y);
    switch (state().textBaseline) {
    case TopTextBaseline:
    case HangingTextBaseline:
        location.setY(y + fontMetrics.ascent());
        break;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        location.setY(y - fontMetrics.descent());
        break;
    case MiddleTextBaseline:
        location.setY(y - fontMetrics.descent() + fontMetrics.height() / 2);
        break;
    case AlphabeticTextBaseline:
    default:
         // Do nothing.
        break;
    }

    float fontWidth = fontProxy.width(TextRun(normalizedText, 0, 0, AllowTrailingExpansion, direction, override));

    useMaxWidth = (useMaxWidth && maxWidth < fontWidth);
    float width = useMaxWidth ? maxWidth : fontWidth;

    TextAlign align = state().textAlign;
    if (align == StartTextAlign)
        align = isRTL ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = isRTL ? LeftTextAlign : RightTextAlign;

    switch (align) {
    case CenterTextAlign:
        location.setX(location.x() - width / 2);
        break;
    case RightTextAlign:
        location.setX(location.x() - width);
        break;
    default:
        break;
    }

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

            FloatSize offset = FloatSize(0, 2 * maskRect.height());

            FloatSize shadowOffset;
            float shadowRadius;
            Color shadowColor;
            c->getShadow(shadowOffset, shadowRadius, shadowColor);

            FloatRect shadowRect(maskRect);
            shadowRect.inflate(shadowRadius * 1.4);
            shadowRect.move(shadowOffset * -1);
            c->clip(shadowRect);

            shadowOffset += offset;

            c->setLegacyShadow(shadowOffset, shadowRadius, shadowColor);

            if (fill)
                c->setFillColor(Color::black);
            else
                c->setStrokeColor(Color::black);

            fontProxy.drawBidiText(*c, textRun, location + offset, FontCascade::UseFallbackIfFontNotReady);
        }

        std::unique_ptr<ImageBuffer> maskImage = c->createCompatibleBuffer(maskRect.size());
        if (!maskImage)
            return;

        GraphicsContext& maskImageContext = maskImage->context();

        if (fill)
            maskImageContext.setFillColor(Color::black);
        else {
            maskImageContext.setStrokeColor(Color::black);
            maskImageContext.setStrokeThickness(c->strokeThickness());
        }

        maskImageContext.setTextDrawingMode(fill ? TextModeFill : TextModeStroke);

        if (useMaxWidth) {
            maskImageContext.translate(location.x() - maskRect.x(), location.y() - maskRect.y());
            // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
            maskImageContext.scale(FloatSize((fontWidth > 0 ? (width / fontWidth) : 0), 1));
            fontProxy.drawBidiText(maskImageContext, textRun, FloatPoint(0, 0), FontCascade::UseFallbackIfFontNotReady);
        } else {
            maskImageContext.translate(-maskRect.x(), -maskRect.y());
            fontProxy.drawBidiText(maskImageContext, textRun, location, FontCascade::UseFallbackIfFontNotReady);
        }

        GraphicsContextStateSaver stateSaver(*c);
        c->clipToImageBuffer(*maskImage, maskRect);
        drawStyle.applyFillColor(c);
        c->fillRect(maskRect);
        return;
    }
#endif

    c->setTextDrawingMode(fill ? TextModeFill : TextModeStroke);

    GraphicsContextStateSaver stateSaver(*c);
    if (useMaxWidth) {
        c->translate(location.x(), location.y());
        // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
        c->scale(FloatSize((fontWidth > 0 ? (width / fontWidth) : 0), 1));
        location = FloatPoint();
    }

    if (isFullCanvasCompositeMode(state().globalComposite)) {
        beginCompositeLayer();
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::UseFallbackIfFontNotReady);
        endCompositeLayer();
        didDrawEntireCanvas();
    } else if (state().globalComposite == CompositeCopy) {
        clearCanvas();
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::UseFallbackIfFontNotReady);
        didDrawEntireCanvas();
    } else {
        fontProxy.drawBidiText(*c, textRun, location, FontCascade::UseFallbackIfFontNotReady);
        didDraw(textRect);
    }
}

void CanvasRenderingContext2D::inflateStrokeRect(FloatRect& rect) const
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

auto CanvasRenderingContext2D::fontProxy() -> const FontProxy&
{
    canvas()->document().updateStyleIfNeeded();

    if (!state().font.realized())
        setFont(state().unparsedFont);
    return state().font;
}

#if ENABLE(ACCELERATED_2D_CANVAS)
PlatformLayer* CanvasRenderingContext2D::platformLayer() const
{
    return canvas()->buffer() ? canvas()->buffer()->platformLayer() : 0;
}
#endif

static InterpolationQuality smoothingToInterpolationQuality(CanvasRenderingContext2D::SmoothingQuality quality)
{
    switch (quality) {
    case CanvasRenderingContext2D::SmoothingQuality::Low:
        return InterpolationLow;
    case CanvasRenderingContext2D::SmoothingQuality::Medium:
        return InterpolationMedium;
    case CanvasRenderingContext2D::SmoothingQuality::High:
        return InterpolationHigh;
    }

    ASSERT_NOT_REACHED();
    return InterpolationLow;
};

String CanvasRenderingContext2D::imageSmoothingQuality() const
{
    switch (state().imageSmoothingQuality) {
    case SmoothingQuality::Low:
        return ASCIILiteral("low");
    case SmoothingQuality::Medium:
        return ASCIILiteral("medium");
    case SmoothingQuality::High:
        return ASCIILiteral("high");
    }

    ASSERT_NOT_REACHED();
    return ASCIILiteral("low");
}

void CanvasRenderingContext2D::setImageSmoothingQuality(const String& smoothingQualityString)
{
    SmoothingQuality quality;
    if (smoothingQualityString == "low")
        quality = SmoothingQuality::Low;
    else if (smoothingQualityString == "medium")
        quality = SmoothingQuality::Medium;
    else if (smoothingQualityString == "high")
        quality = SmoothingQuality::High;
    else {
        ASSERT_NOT_REACHED();
        return;
    }

    if (quality == state().imageSmoothingQuality)
        return;

    realizeSaves();
    modifiableState().imageSmoothingQuality = quality;

    if (!state().imageSmoothingEnabled)
        return;

    if (auto* context = drawingContext())
        context->setImageInterpolationQuality(smoothingToInterpolationQuality(quality));
}

bool CanvasRenderingContext2D::imageSmoothingEnabled() const
{
    return state().imageSmoothingEnabled;
}

void CanvasRenderingContext2D::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().imageSmoothingEnabled)
        return;

    realizeSaves();
    modifiableState().imageSmoothingEnabled = enabled;
    GraphicsContext* c = drawingContext();
    if (c)
        c->setImageInterpolationQuality(enabled ? smoothingToInterpolationQuality(state().imageSmoothingQuality) : InterpolationNone);
}

} // namespace WebCore
