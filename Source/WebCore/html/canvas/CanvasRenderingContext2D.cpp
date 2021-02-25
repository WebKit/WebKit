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
#include "CSSPropertyParserHelpers.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "NodeRenderStyle.h"
#include "Path2D.h"
#include "RenderTheme.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "StyleBuilder.h"
#include "StyleFontSizeFunctions.h"
#include "StyleProperties.h"
#include "StyleResolveForFontRaw.h"
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

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    if (newFont.isEmpty())
        return;

    if (newFont == state().unparsedFont && state().font.realized())
        return;

    // According to http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit" and "initial" values must be ignored. parseFontWorkerSafe() ignores these.
    auto fontRaw = CSSParser::parseFontWorkerSafe(newFont, strictToCSSParserMode(!m_usesCSSCompatibilityParseMode));
    if (!fontRaw)
        return;

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    Document& document = canvas().document();
    document.updateStyleIfNeeded();

    FontCascadeDescription fontDescription;
    if (auto* computedStyle = canvas().computedStyle())
        fontDescription = FontCascadeDescription { computedStyle->fontDescription() };
    else {
        fontDescription.setOneFamily(DefaultFontFamily);
        fontDescription.setSpecifiedSize(DefaultFontSize);
        fontDescription.setComputedSize(DefaultFontSize);
    }

    auto fontStyle = Style::resolveForFontRaw(*fontRaw, WTFMove(fontDescription), document);
    if (!fontStyle)
        return;

    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    realizeSaves();
    modifiableState().unparsedFont = newFontSafeCopy;

    modifiableState().font.initialize(document.fontSelector(), *fontStyle);
    ASSERT(state().font.realized());
    ASSERT(state().font.isPopulated());
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

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, Optional<float> maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, Optional<float> maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth);
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

    TextRun textRun(normalizedText, 0, 0, AllowRightExpansion, direction, override, true);
    return measureTextInternal(textRun);
}

auto CanvasRenderingContext2D::fontProxy() -> const FontProxy* {
    auto& canvas = downcast<HTMLCanvasElement>(canvasBase());
    canvas.document().updateStyleIfNeeded();
    if (!state().font.realized())
        setFont(state().unparsedFont);
    return &state().font;
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, Optional<float> maxWidth)
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logCanvasWriteOrMeasure(this->canvas().document(), text);

    if (!canDrawTextWithParams(x, y, fill, maxWidth))
        return;

    String normalizedText = text;
    normalizeSpaces(normalizedText);

    const RenderStyle* computedStyle;
    auto direction = toTextDirection(state().direction, &computedStyle);
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(normalizedText, 0, 0, AllowRightExpansion, direction, override, true);
    drawTextUnchecked(textRun, x, y, fill, maxWidth);
}

} // namespace WebCore
