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
#include "CanvasRenderingContext2D.h"

#include "CSSFilter.h"
#include "CSSFontSelector.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParserConsumer+Filter.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "DocumentInlines.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "NodeRenderStyle.h"
#include "Path2D.h"
#include "PixelFormat.h"
#include "RenderTheme.h"
#include "RenderWidget.h"
#include "ResourceLoadObserver.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "StyleBuilder.h"
#include "StyleFontSizeFunctions.h"
#include "StyleProperties.h"
#include "StyleResolveForFont.h"
#include "StyleTreeResolver.h"
#include "TextMetrics.h"
#include "TextRun.h"
#include "UnicodeBidi.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CanvasRenderingContext2D);

std::unique_ptr<CanvasRenderingContext2D> CanvasRenderingContext2D::create(CanvasBase& canvas, CanvasRenderingContext2DSettings&& settings, bool usesCSSCompatibilityParseMode)
{
    auto renderingContext = std::unique_ptr<CanvasRenderingContext2D>(new CanvasRenderingContext2D(canvas, WTFMove(settings), usesCSSCompatibilityParseMode));

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

CanvasRenderingContext2D::CanvasRenderingContext2D(CanvasBase& canvas, CanvasRenderingContext2DSettings&& settings, bool usesCSSCompatibilityParseMode)
    : CanvasRenderingContext2DBase(canvas, Type::CanvasElement2D, WTFMove(settings), usesCSSCompatibilityParseMode)
{
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() = default;

std::optional<FilterOperations> CanvasRenderingContext2D::setFilterStringWithoutUpdatingStyle(const String& filterString)
{
    Ref document = canvas().document();
    if (!document->settings().canvasFiltersEnabled())
        return std::nullopt;

    document->updateStyleIfNeeded();

    const auto* style = canvas().computedStyle();
    if (!style)
        return std::nullopt;

    auto parserContext = CSSParserContext(strictToCSSParserMode(!usesCSSCompatibilityParseMode()));
    return CSSPropertyParserHelpers::parseFilterValueListOrNoneRaw(filterString, parserContext, document, const_cast<RenderStyle&>(*style));
}

RefPtr<Filter> CanvasRenderingContext2D::createFilter(const FloatRect& bounds) const
{
    if (bounds.isEmpty())
        return nullptr;

    auto* context = effectiveDrawingContext();
    if (!context)
        return nullptr;

    CheckedPtr renderer = canvas().renderer();
    if (!renderer)
        return nullptr;

    RefPtr page = canvas().document().page();
    if (!page)
        return nullptr;

    auto preferredFilterRenderingModes = page->preferredFilterRenderingModes();
    auto filter = CSSFilter::create(*renderer, state().filterOperations, preferredFilterRenderingModes, { 1, 1 }, bounds, *context);
    if (!filter)
        return nullptr;

    auto outsets = calculateFilterOutsets(bounds);

    filter->setFilterRegion(bounds + outsets);
    return filter;
}

IntOutsets CanvasRenderingContext2D::calculateFilterOutsets(const FloatRect& bounds) const
{
    if (state().filterOperations.isEmpty())
        return { };

    CheckedPtr renderer = canvas().renderer();
    if (!renderer)
        return { };

    return CSSFilter::calculateOutsets(*renderer, state().filterOperations, bounds);
}

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
    auto* context = effectiveDrawingContext();
    if (!element.focused() || !state().hasInvertibleTransform || path.isEmpty() || !element.isDescendantOf(canvas()) || !context)
        return;
    context->drawFocusRing(path, 1, RenderTheme::singleton().focusRingColor(element.document().styleColorOptions(canvas().computedStyle())));
    didDrawEntireCanvas();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    Ref document = canvas().document();
    document->updateStyleIfNeeded();

    setFontWithoutUpdatingStyle(newFont);
}

void CanvasRenderingContext2D::setFontWithoutUpdatingStyle(const String& newFont)
{
    // Intentionally don't update style here, because updating style can cause JS to run synchronously.
    // This function is called in the middle of processing, and running arbitrary JS in the middle of processing can cause unexpected behavior.
    // Instead, the relevant canvas entry points update style once when they begin running, and we won't touch the style after that.
    // This means that the style may end up being stale here, but that's at least better than running arbitrary JS in the middle of processing.

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (newFont.isEmpty())
        return;

    if (newFont == state().unparsedFont && state().font.realized())
        return;

    // According to http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit" and "initial" values must be ignored. CSSPropertyParserHelpers::parseUnresolvedFont() ignores these.
    auto unresolvedFont = CSSPropertyParserHelpers::parseUnresolvedFont(newFont, strictToCSSParserMode(!usesCSSCompatibilityParseMode()));
    if (!unresolvedFont)
        return;

    FontCascadeDescription fontDescription;
    if (auto* computedStyle = canvas().computedStyle())
        fontDescription = FontCascadeDescription { computedStyle->fontDescription() };
    else {
        static NeverDestroyed<AtomString> family = DefaultFontFamily;
        fontDescription.setOneFamily(family.get());
        fontDescription.setSpecifiedSize(DefaultFontSize);
        fontDescription.setComputedSize(DefaultFontSize);
    }

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    Document& document = canvas().document();
    auto fontCascade = Style::resolveForUnresolvedFont(*unresolvedFont, WTFMove(fontDescription), document);
    if (!fontCascade)
        return;

    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    realizeSaves();
    modifiableState().unparsedFont = newFontSafeCopy;

    modifiableState().font.initialize(document.fontSelector(), *fontCascade);
    ASSERT(state().font.realized());
    ASSERT(state().font.isPopulated());

    // Recompute the word and the letter spacing for the new font.
    String letterSpacing;
    setLetterSpacing(std::exchange(modifiableState().letterSpacing, letterSpacing));
    String wordSpacing;
    setWordSpacing(std::exchange(modifiableState().wordSpacing, wordSpacing));
}

inline TextDirection CanvasRenderingContext2D::toTextDirection(Direction direction, const RenderStyle** computedStyle) const
{
    auto* style = computedStyle || direction == Direction::Inherit ? canvas().existingComputedStyle() : nullptr;
    if (computedStyle)
        *computedStyle = style;
    switch (direction) {
    case Direction::Inherit:
        return style ? style->writingMode().computedTextDirection() : TextDirection::LTR;
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

void CanvasRenderingContext2D::fillText(const String& text, double x, double y, std::optional<double> maxWidth)
{
    canvasBase().recordLastFillText(text);
    drawTextInternal(text, x, y, true, maxWidth);
}

void CanvasRenderingContext2D::strokeText(const String& text, double x, double y, std::optional<double> maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth);
}

Ref<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    Ref document = canvas().document();
    document->updateStyleIfNeeded();

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (document->settings().webAPIStatisticsEnabled()) {
        ResourceLoadObserver::shared().logCanvasWriteOrMeasure(document, text);
        ResourceLoadObserver::shared().logCanvasRead(document);
    }

    String normalizedText = normalizeSpaces(text);
    const RenderStyle* computedStyle;
    auto direction = toTextDirection(state().direction, &computedStyle);
    bool override = computedStyle && isOverride(computedStyle->unicodeBidi());
    TextRun textRun(normalizedText, 0, 0, ExpansionBehavior::allowRightOnly(), direction, override, true);
    return measureTextInternal(textRun);
}

auto CanvasRenderingContext2D::fontProxy() -> const FontProxy*
{
    // Intentionally don't update style here, because updating style can cause JS to run synchronously.
    // This function is called in the middle of processing, and running arbitrary JS in the middle of processing can cause unexpected behavior.
    // Instead, the relevant canvas entry points update style once when they begin running, and we won't touch the style after that.
    // This means that the style may end up being stale here, but that's at least better than running arbitrary JS in the middle of processing.
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (!state().font.realized())
        setFontWithoutUpdatingStyle(state().unparsedFont);
    return &state().font;
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, double x, double y, bool fill, std::optional<double> maxWidth)
{
    Ref document = canvas().document();
    document->updateStyleIfNeeded();

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (document->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasWriteOrMeasure(document, text);

    if (!canDrawText(x, y, fill, maxWidth))
        return;

    String normalizedText = normalizeSpaces(text);
    const RenderStyle* computedStyle;
    auto direction = toTextDirection(state().direction, &computedStyle);
    bool override = computedStyle && isOverride(computedStyle->unicodeBidi());
    TextRun textRun(normalizedText, 0, 0, ExpansionBehavior::allowRightOnly(), direction, override, true);
    drawTextUnchecked(textRun, x, y, fill, maxWidth);
}

} // namespace WebCore
