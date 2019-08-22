/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "Path2D.h"
#include "RenderTheme.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "TextMetrics.h"
#include "TextRun.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(CanvasRenderingContext2D);

std::unique_ptr<CanvasRenderingContext2D> CanvasRenderingContext2D::create(CanvasBase& canvas, bool usesCSSCompatibilityParseMode)
{
    auto renderingContext = std::unique_ptr<CanvasRenderingContext2D>(new CanvasRenderingContext2D(canvas, usesCSSCompatibilityParseMode));

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

CanvasRenderingContext2D::CanvasRenderingContext2D(CanvasBase& canvas, bool usesCSSCompatibilityParseMode)
    : CanvasRenderingContext2DBase(canvas, usesCSSCompatibilityParseMode)
{
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() = default;

void CanvasRenderingContext2D::drawFocusIfNeeded(Element& element)
{
    drawFocusIfNeededInternal(m_path, element);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Path2D& path, Element& element)
{
    drawFocusIfNeededInternal(path.path(), element);
}

void CanvasRenderingContext2D::drawFocusIfNeededInternal(const Path& path, Element& element)
{
    auto* context = drawingContext();
    if (!element.focused() || !state().hasInvertibleTransform || path.isEmpty() || !element.isDescendantOf(canvas()) || !context)
        return;
    context->drawFocusRing(path, 1, 1, RenderTheme::singleton().focusRingColor(element.document().styleColorOptions(canvas().computedStyle())));
}

String CanvasRenderingContext2D::font() const
{
    if (!state().font.realized())
        return DefaultFont;

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

        serializedFont.append(' ', family);
    }

    return serializedFont.toString();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    if (newFont == state().unparsedFont && state().font.realized())
        return;

    auto parsedStyle = MutableStyleProperties::create();
    CSSParser::parseValue(parsedStyle, CSSPropertyFont, newFont, true, strictToCSSParserMode(!m_usesCSSCompatibilityParseMode));
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
    auto newStyle = RenderStyle::createPtr();

    Document& document = canvas().document();
    document.updateStyleIfNeeded();

    if (auto* computedStyle = canvas().computedStyle())
        newStyle->setFontDescription(FontCascadeDescription { computedStyle->fontDescription() });
    else {
        FontCascadeDescription defaultFontDescription;
        defaultFontDescription.setOneFamily(DefaultFontFamily);
        defaultFontDescription.setSpecifiedSize(DefaultFontSize);
        defaultFontDescription.setComputedSize(DefaultFontSize);

        newStyle->setFontDescription(WTFMove(defaultFontDescription));
    }

    newStyle->fontCascade().update(&document.fontSelector());

    // Now map the font property longhands into the style.
    StyleResolver& styleResolver = canvas().styleResolver();
    styleResolver.applyPropertyToStyle(CSSPropertyFontFamily, parsedStyle->getPropertyCSSValue(CSSPropertyFontFamily).get(), WTFMove(newStyle));
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

    modifiableState().font.initialize(document.fontSelector(), *styleResolver.style());
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

CanvasTextAlign CanvasRenderingContext2D::textAlign() const
{
    return toCanvasTextAlign(state().textAlign);
}

void CanvasRenderingContext2D::setTextAlign(CanvasTextAlign canvasTextAlign)
{
    auto textAlign = fromCanvasTextAlign(canvasTextAlign);
    if (state().textAlign == textAlign)
        return;
    realizeSaves();
    modifiableState().textAlign = textAlign;
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

CanvasTextBaseline CanvasRenderingContext2D::textBaseline() const
{
    return toCanvasTextBaseline(state().textBaseline);
}

void CanvasRenderingContext2D::setTextBaseline(CanvasTextBaseline canvasTextBaseline)
{
    auto textBaseline = fromCanvasTextBaseline(canvasTextBaseline);
    if (state().textBaseline == textBaseline)
        return;
    realizeSaves();
    modifiableState().textBaseline = textBaseline;
}

inline TextDirection CanvasRenderingContext2D::toTextDirection(Direction direction, const RenderStyle** computedStyle) const
{
    auto* style = (computedStyle || direction == Direction::Inherit) ? canvas().computedStyle() : nullptr;
    if (computedStyle)
        *computedStyle = style;
    switch (direction) {
    case Direction::Inherit:
        return style ? style->direction() : TextDirection::LTR;
    case Direction::Rtl:
        return TextDirection::RTL;
    case Direction::Ltr:
        return TextDirection::LTR;
    }
    ASSERT_NOT_REACHED();
    return TextDirection::LTR;
}

CanvasDirection CanvasRenderingContext2D::direction() const
{
    if (state().direction == Direction::Inherit)
        canvas().document().updateStyleIfNeeded();
    return toTextDirection(state().direction) == TextDirection::RTL ? CanvasDirection::Rtl : CanvasDirection::Ltr;
}

void CanvasRenderingContext2D::setDirection(CanvasDirection direction)
{
    if (state().direction == direction)
        return;

    realizeSaves();
    modifiableState().direction = direction;
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, Optional<float> maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, Optional<float> maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth);
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
    text = String::adopt(WTFMove(charVector));
}

Ref<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled()) {
        auto& canvas = this->canvas();
        ResourceLoadObserver::shared().logCanvasWriteOrMeasure(canvas.document(), text);
        ResourceLoadObserver::shared().logCanvasRead(canvas.document());
    }
    
    Ref<TextMetrics> metrics = TextMetrics::create();

    String normalizedText = text;
    normalizeSpaces(normalizedText);

    const RenderStyle* computedStyle;
    auto direction = toTextDirection(state().direction, &computedStyle);
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(normalizedText, 0, 0, AllowTrailingExpansion, direction, override, true);
    auto& font = fontProxy();
    auto& fontMetrics = font.fontMetrics();

    GlyphOverflow glyphOverflow;
    glyphOverflow.computeBounds = true;
    float fontWidth = font.width(textRun, &glyphOverflow);
    metrics->setWidth(fontWidth);

    FloatPoint offset = textOffset(fontWidth, direction);

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

auto CanvasRenderingContext2D::fontProxy() -> const FontProxy& {
    auto& canvas = downcast<HTMLCanvasElement>(canvasBase());
    canvas.document().updateStyleIfNeeded();
    if (!state().font.realized())
        setFont(state().unparsedFont);
    return state().font;
}

FloatPoint CanvasRenderingContext2D::textOffset(float width, TextDirection direction)
{
    auto& fontMetrics = fontProxy().fontMetrics();
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

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, Optional<float> maxWidth)
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasWriteOrMeasure(this->canvas().document(), text);
    
    auto& fontProxy = this->fontProxy();
    const auto& fontMetrics = fontProxy.fontMetrics();

    auto* c = drawingContext();
    if (!c)
        return;
    if (!state().hasInvertibleTransform)
        return;
    if (!std::isfinite(x) | !std::isfinite(y))
        return;
    if (maxWidth && (!std::isfinite(maxWidth.value()) || maxWidth.value() <= 0))
        return;

    // If gradient size is zero, then paint nothing.
    auto gradient = c->strokeGradient();
    if (!fill && gradient && gradient->isZeroSize())
        return;

    gradient = c->fillGradient();
    if (fill && gradient && gradient->isZeroSize())
        return;

    String normalizedText = text;
    normalizeSpaces(normalizedText);

    // FIXME: Need to turn off font smoothing.

    const RenderStyle* computedStyle;
    auto direction = toTextDirection(state().direction, &computedStyle);
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(normalizedText, 0, 0, AllowTrailingExpansion, direction, override, true);
    float fontWidth = fontProxy.width(textRun);
    bool useMaxWidth = maxWidth && maxWidth.value() < fontWidth;
    float width = useMaxWidth ? maxWidth.value() : fontWidth;
    FloatPoint location(x, y);
    location += textOffset(width, direction);

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

            c->setLegacyShadow(shadowOffset, shadowRadius, shadowColor);

            if (fill)
                c->setFillColor(Color::black);
            else
                c->setStrokeColor(Color::black);

            fontProxy.drawBidiText(*c, textRun, location + offset, FontCascade::UseFallbackIfFontNotReady);
        }

        auto maskImage = ImageBuffer::createCompatibleBuffer(maskRect.size(), ColorSpaceSRGB, *c);
        if (!maskImage)
            return;

        auto& maskImageContext = maskImage->context();

        if (fill)
            maskImageContext.setFillColor(Color::black);
        else {
            maskImageContext.setStrokeColor(Color::black);
            maskImageContext.setStrokeThickness(c->strokeThickness());
        }

        maskImageContext.setTextDrawingMode(fill ? TextModeFill : TextModeStroke);

        if (useMaxWidth) {
            maskImageContext.translate(location - maskRect.location());
            // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
            maskImageContext.scale(FloatSize((fontWidth > 0 ? (width / fontWidth) : 0), 1));
            fontProxy.drawBidiText(maskImageContext, textRun, FloatPoint(0, 0), FontCascade::UseFallbackIfFontNotReady);
        } else {
            maskImageContext.translate(-maskRect.location());
            fontProxy.drawBidiText(maskImageContext, textRun, location, FontCascade::UseFallbackIfFontNotReady);
        }

        GraphicsContextStateSaver stateSaver(*c);
        c->clipToImageBuffer(*maskImage, maskRect);
        drawStyle.applyFillColor(*c);
        c->fillRect(maskRect);
        return;
    }
#endif

    c->setTextDrawingMode(fill ? TextModeFill : TextModeStroke);

    GraphicsContextStateSaver stateSaver(*c);
    if (useMaxWidth) {
        c->translate(location);
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

} // namespace WebCore
