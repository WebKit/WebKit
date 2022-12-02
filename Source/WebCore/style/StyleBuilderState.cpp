/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "StyleBuilderState.h"

#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontSelector.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "CSSShadowValue.h"
#include "ColorFromPrimitiveValue.h"
#include "Document.h"
#include "ElementInlines.h"
#include "FilterOperationsBuilder.h"
#include "FontCache.h"
#include "HTMLElement.h"
#include "RenderTheme.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "StyleBuilder.h"
#include "StyleCachedImage.h"
#include "StyleCanvasImage.h"
#include "StyleCrossfadeImage.h"
#include "StyleCursorImage.h"
#include "StyleFilterImage.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleGradientImage.h"
#include "StyleImageSet.h"
#include "StyleNamedImage.h"
#include "StylePaintImage.h"
#include "TransformFunctions.h"

namespace WebCore {
namespace Style {

BuilderState::BuilderState(Builder& builder, RenderStyle& style, BuilderContext&& context)
    : m_builder(builder)
    , m_styleMap(*this)
    , m_style(style)
    , m_context(WTFMove(context))
    , m_cssToLengthConversionData(style, m_context)
{
}

// SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
// of each individual length value in the render style / tree. CSSPrimitiveValue::computeLength*()
// multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
// Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
// need to respect the scaling. RenderBox (the parent class of LegacyRenderSVGRoot) grabs values like
// width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
// if we'd pass a 1.0 zoom factor everyhwere. So we only pass a zoom factor of 1.0 for specific
// properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
bool BuilderState::useSVGZoomRules() const
{
    return is<SVGElement>(element());
}

bool BuilderState::useSVGZoomRulesForLength() const
{
    return is<SVGElement>(element()) && !(is<SVGSVGElement>(*element()) && element()->parentNode());
}

RefPtr<StyleImage> BuilderState::createStyleImage(const CSSValue& value)
{
    if (is<CSSImageValue>(value))
        return downcast<CSSImageValue>(value).createStyleImage(*this);
    if (is<CSSImageSetValue>(value))
        return downcast<CSSImageSetValue>(value).createStyleImage(*this);
    if (is<CSSCursorImageValue>(value))
        return downcast<CSSCursorImageValue>(value).createStyleImage(*this);
    if (is<CSSNamedImageValue>(value))
        return downcast<CSSNamedImageValue>(value).createStyleImage(*this);
    if (is<CSSCanvasValue>(value))
        return downcast<CSSCanvasValue>(value).createStyleImage(*this);
    if (is<CSSCrossfadeValue>(value))
        return downcast<CSSCrossfadeValue>(value).createStyleImage(*this);
    if (is<CSSFilterImageValue>(value))
        return downcast<CSSFilterImageValue>(value).createStyleImage(*this);
    if (is<CSSLinearGradientValue>(value))
        return downcast<CSSLinearGradientValue>(value).createStyleImage(*this);
    if (is<CSSPrefixedLinearGradientValue>(value))
        return downcast<CSSPrefixedLinearGradientValue>(value).createStyleImage(*this);
    if (is<CSSDeprecatedLinearGradientValue>(value))
        return downcast<CSSDeprecatedLinearGradientValue>(value).createStyleImage(*this);
    if (is<CSSRadialGradientValue>(value))
        return downcast<CSSRadialGradientValue>(value).createStyleImage(*this);
    if (is<CSSPrefixedRadialGradientValue>(value))
        return downcast<CSSPrefixedRadialGradientValue>(value).createStyleImage(*this);
    if (is<CSSDeprecatedRadialGradientValue>(value))
        return downcast<CSSDeprecatedRadialGradientValue>(value).createStyleImage(*this);
    if (is<CSSConicGradientValue>(value))
        return downcast<CSSConicGradientValue>(value).createStyleImage(*this);
#if ENABLE(CSS_PAINTING_API)
    if (is<CSSPaintImageValue>(value))
        return downcast<CSSPaintImageValue>(value).createStyleImage(*this);
#endif
    return nullptr;
}

std::optional<FilterOperations> BuilderState::createFilterOperations(const CSSValue& inValue)
{
    return WebCore::Style::createFilterOperations(document(), m_style, m_cssToLengthConversionData, inValue);
}

bool BuilderState::isColorFromPrimitiveValueDerivedFromElement(const CSSPrimitiveValue& value)
{
    switch (value.valueID()) {
    case CSSValueInternalDocumentTextColor:
    case CSSValueWebkitLink:
    case CSSValueWebkitActivelink:
    case CSSValueCurrentcolor:
        return true;
    default:
        return false;
    }
}

StyleColor BuilderState::colorFromPrimitiveValue(const CSSPrimitiveValue& value, ForVisitedLink forVisitedLink) const
{
    if (!element() || !element()->isLink())
        forVisitedLink = ForVisitedLink::No;
    return { WebCore::Style::colorFromPrimitiveValue(document(), m_style, value, forVisitedLink) };
}

Color BuilderState::colorFromPrimitiveValueWithResolvedCurrentColor(const CSSPrimitiveValue& value) const
{
    return WebCore::Style::colorFromPrimitiveValueWithResolvedCurrentColor(document(), m_style, value);
}

void BuilderState::registerContentAttribute(const AtomString& attributeLocalName)
{
    if (style().styleType() == PseudoId::Before || style().styleType() == PseudoId::After)
        m_registeredContentAttributes.append(attributeLocalName);
}

void BuilderState::adjustStyleForInterCharacterRuby()
{
    if (m_style.rubyPosition() != RubyPosition::InterCharacter || !element() || !element()->hasTagName(HTMLNames::rtTag))
        return;

    m_style.setTextAlign(TextAlignMode::Center);
    if (m_style.isHorizontalWritingMode())
        m_style.setWritingMode(WritingMode::LeftToRight);
}

void BuilderState::updateFont()
{
    auto& fontSelector = const_cast<Document&>(document()).fontSelector();

    auto needsUpdate = [&] {
        if (m_fontDirty)
            return true;
        auto* fonts = m_style.fontCascade().fonts();
        if (!fonts)
            return true;
        return false;
    };

    if (!needsUpdate())
        return;

#if ENABLE(TEXT_AUTOSIZING)
    updateFontForTextSizeAdjust();
#endif
    updateFontForGenericFamilyChange();
    updateFontForZoomChange();
    updateFontForOrientationChange();

    m_style.fontCascade().update(&fontSelector);

    m_fontDirty = false;
}

#if ENABLE(TEXT_AUTOSIZING)
void BuilderState::updateFontForTextSizeAdjust()
{
    if (m_style.textSizeAdjust().isAuto()
        || !document().settings().textAutosizingEnabled()
        || (document().settings().textAutosizingUsesIdempotentMode()
            && !m_style.textSizeAdjust().isNone()
            && !document().settings().idempotentModeAutosizingOnlyHonorsPercentages()))
        return;

    auto newFontDescription = m_style.fontDescription();
    if (!m_style.textSizeAdjust().isNone())
        newFontDescription.setComputedSize(newFontDescription.specifiedSize() * m_style.textSizeAdjust().multiplier());
    else
        newFontDescription.setComputedSize(newFontDescription.specifiedSize());

    m_style.setFontDescription(WTFMove(newFontDescription));
}
#endif

void BuilderState::updateFontForZoomChange()
{
    if (m_style.effectiveZoom() == parentStyle().effectiveZoom() && m_style.textZoom() == parentStyle().textZoom())
        return;

    const auto& childFont = m_style.fontDescription();
    auto newFontDescription = childFont;
    setFontSize(newFontDescription, childFont.specifiedSize());

    m_style.setFontDescription(WTFMove(newFontDescription));
}

void BuilderState::updateFontForGenericFamilyChange()
{
    const auto& childFont = m_style.fontDescription();

    if (childFont.isAbsoluteSize())
        return;

    const auto& parentFont = parentStyle().fontDescription();
    if (childFont.useFixedDefaultSize() == parentFont.useFixedDefaultSize())
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified. We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size = [&] {
        if (CSSValueID sizeIdentifier = childFont.keywordSizeAsIdentifier())
            return Style::fontSizeForKeyword(sizeIdentifier, childFont.useFixedDefaultSize(), document());

        auto fixedSize =  document().settings().defaultFixedFontSize();
        auto defaultSize =  document().settings().defaultFontSize();
        float fixedScaleFactor = (fixedSize && defaultSize) ? static_cast<float>(fixedSize) / defaultSize : 1;
        return parentFont.useFixedDefaultSize() ? childFont.specifiedSize() / fixedScaleFactor : childFont.specifiedSize() * fixedScaleFactor;
    }();

    auto newFontDescription = childFont;
    setFontSize(newFontDescription, size);
    m_style.setFontDescription(WTFMove(newFontDescription));
}

void BuilderState::updateFontForOrientationChange()
{
    auto [fontOrientation, glyphOrientation] = m_style.fontAndGlyphOrientation();

    const auto& fontDescription = m_style.fontDescription();
    if (fontDescription.orientation() == fontOrientation && fontDescription.nonCJKGlyphOrientation() == glyphOrientation)
        return;

    auto newFontDescription = fontDescription;
    newFontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    newFontDescription.setOrientation(fontOrientation);
    m_style.setFontDescription(WTFMove(newFontDescription));
}

void BuilderState::setFontSize(FontCascadeDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), useSVGZoomRules(), &style(), document()));
}

}
}
