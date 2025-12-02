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
#include "StyleBuilderStateInlines.h"

#include "CSSCalcRandomCachingKey.h"
#include "CSSCanvasValue.h"
#include "CSSColorValue.h"
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
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "ElementTraversal.h"
#include "FontCache.h"
#include "HTMLElement.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "StyleBuilder.h"
#include "StyleCachedImage.h"
#include "StyleCanvasImage.h"
#include "StyleColor.h"
#include "StyleCrossfadeImage.h"
#include "StyleCursorImage.h"
#include "StyleFilterImage.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleGradientImage.h"
#include "StyleImageSet.h"
#include "StyleNamedImage.h"
#include "StylePaintImage.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BuilderState);

BuilderState::BuilderState(RenderStyle& style)
    : m_style(style)
{
}

BuilderState::BuilderState(RenderStyle& style, BuilderContext&& context)
    : m_style(style)
    , m_context(WTFMove(context))
    , m_cssToLengthConversionData(style, *this)
{
}

// SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
// of each individual length value in the render style / tree. CSSPrimitiveValue::resolveAsLength*()
// multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
// Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
// need to respect the scaling. RenderBox (the parent class of LegacyRenderSVGRoot) grabs values like
// width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
// if we'd pass a 1.0 zoom factor everywhere. So we only pass a zoom factor of 1.0 for specific
// properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
bool BuilderState::useSVGZoomRules() const
{
    return is<SVGElement>(element());
}

bool BuilderState::useSVGZoomRulesForLength() const
{
    return is<SVGElement>(element()) && !(is<SVGSVGElement>(*element()) && element()->parentNode());
}

RefPtr<StyleImage> BuilderState::createStyleImage(const CSSValue& value) const
{
    if (auto* imageValue = dynamicDowncast<CSSImageValue>(value))
        return imageValue->createStyleImage(*this);
    if (auto* imageSetValue = dynamicDowncast<CSSImageSetValue>(value))
        return imageSetValue->createStyleImage(*this);
    if (auto* imageValue = dynamicDowncast<CSSCursorImageValue>(value))
        return imageValue->createStyleImage(*this);
    if (auto* imageValue = dynamicDowncast<CSSNamedImageValue>(value))
        return imageValue->createStyleImage(*this);
    if (auto* cssCanvasValue = dynamicDowncast<CSSCanvasValue>(value))
        return cssCanvasValue->createStyleImage(*this);
    if (auto* crossfadeValue = dynamicDowncast<CSSCrossfadeValue>(value))
        return crossfadeValue->createStyleImage(*this);
    if (auto* filterImageValue = dynamicDowncast<CSSFilterImageValue>(value))
        return filterImageValue->createStyleImage(*this);
    if (auto* gradientValue = dynamicDowncast<CSSGradientValue>(value))
        return gradientValue->createStyleImage(*this);
    if (auto* paintImageValue = dynamicDowncast<CSSPaintImageValue>(value))
        return paintImageValue->createStyleImage(*this);
    return nullptr;
}

void BuilderState::registerContentAttribute(const AtomString& attributeLocalName)
{
    if (style().pseudoElementType() == PseudoElementType::Before || style().pseudoElementType() == PseudoElementType::After)
        m_registeredContentAttributes.append(attributeLocalName);
}

void BuilderState::adjustStyleForInterCharacterRuby()
{
    if (!m_style.isInterCharacterRubyPosition() || !element() || !element()->hasTagName(HTMLNames::rtTag))
        return;

    m_style.setTextAlign(TextAlign::Center);
    if (!m_style.writingMode().isVerticalTypographic())
        m_style.setWritingMode(StyleWritingMode::VerticalLr);
}

void BuilderState::updateFont()
{
    auto& fontSelector = const_cast<Document&>(document()).fontSelector();

    auto needsUpdate = [&] {
        return m_fontDirty || !m_style.fontCascade().fonts();
    };

    if (!needsUpdate())
        return;

#if ENABLE(TEXT_AUTOSIZING)
    updateFontForTextSizeAdjust();
#endif
    updateFontForGenericFamilyChange();
    updateFontForZoomChange();
    updateFontForOrientationChange();
    updateFontForSizeChange();

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

    m_style.setFontDescriptionWithoutUpdate(WTFMove(newFontDescription));
}
#endif

void BuilderState::updateFontForZoomChange()
{
    if (m_style.usedZoom() == parentStyle().usedZoom() && m_style.textZoom() == parentStyle().textZoom())
        return;

    setFontDescriptionFontSize(m_style.fontDescription().specifiedSize());
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
    m_style.setFontDescriptionWithoutUpdate(WTFMove(newFontDescription));
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
    m_style.setFontDescriptionWithoutUpdate(WTFMove(newFontDescription));
}

void BuilderState::updateFontForSizeChange()
{
    auto& fontCascade = m_style.mutableFontCascadeWithoutUpdate();
    auto fontSize = fontCascade.size();

    auto newWordSpacing = evaluate<float>(m_style.computedWordSpacing(), fontSize, m_style.usedZoomForLength());
    auto newLetterSpacing = evaluate<float>(m_style.computedLetterSpacing(), fontSize, m_style.usedZoomForLength());

    if (newWordSpacing != fontCascade.wordSpacing())
        fontCascade.setWordSpacing(newWordSpacing);

    if (newLetterSpacing != fontCascade.letterSpacing()) {
        fontCascade.setLetterSpacing(newLetterSpacing);

        const auto& oldFontDescription = m_style.fontDescription();

        bool oldShouldDisableLigatures = oldFontDescription.shouldDisableLigaturesForSpacing();
        bool newShouldDisableLigatures = newLetterSpacing != 0;

        // Switching letter-spacing between zero and non-zero requires updating to enable/disable ligatures.
        if (oldShouldDisableLigatures != newShouldDisableLigatures) {
            auto newFontDescription = oldFontDescription;
            newFontDescription.setShouldDisableLigaturesForSpacing(newShouldDisableLigatures);
            m_style.setFontDescriptionWithoutUpdate(WTFMove(newFontDescription));
        }
    }
}

void BuilderState::setFontSize(FontCascadeDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    auto computedFontSize = Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), useSVGZoomRules(), &style(), document());
    fontDescription.setComputedSize(computedFontSize.size, computedFontSize.usedZoomFactor);
}

CSSPropertyID BuilderState::cssPropertyID() const
{
    return m_currentProperty ? m_currentProperty->id : CSSPropertyInvalid;
}

bool BuilderState::isCurrentPropertyInvalidAtComputedValueTime() const
{
    return m_invalidAtComputedValueTimeProperties.get(cssPropertyID());
}

void BuilderState::setCurrentPropertyInvalidAtComputedValueTime()
{
    m_invalidAtComputedValueTimeProperties.set(cssPropertyID());
}

void BuilderState::setUsesViewportUnits()
{
    m_style.setUsesViewportUnits();
}

void BuilderState::setUsesContainerUnits()
{
    m_style.setUsesContainerUnits();
}

double BuilderState::lookupCSSRandomBaseValue(const CSSCalc::RandomCachingKey& key, std::optional<CSS::Keyword::ElementShared> elementShared) const
{
    if (!elementShared)
        return element()->lookupCSSRandomBaseValue(style().pseudoElementIdentifier(), key);

    return document().lookupCSSRandomBaseValue(key);
}

// MARK: - Tree Counting Functions

unsigned BuilderState::siblingCount()
{
    // https://drafts.csswg.org/css-values-5/#funcdef-sibling-count

    ASSERT(element());

    RefPtr parent = element()->parentElement();
    if (!parent)
        return 1;

    m_style.setUsesTreeCountingFunctions();
    parent->setChildrenAffectedByBackwardPositionalRules();
    parent->setChildrenAffectedByForwardPositionalRules();

    unsigned count = 1;
    for (const auto* sibling = ElementTraversal::previousSibling(*element()); sibling; sibling = ElementTraversal::previousSibling(*sibling))
        ++count;
    for (const auto* sibling = ElementTraversal::nextSibling(*element()); sibling; sibling = ElementTraversal::nextSibling(*sibling))
        ++count;
    return count;
}

unsigned BuilderState::siblingIndex()
{
    // https://drafts.csswg.org/css-values-5/#funcdef-sibling-index

    ASSERT(element());

    RefPtr parent = element()->parentElement();
    if (!parent)
        return 1;

    m_style.setUsesTreeCountingFunctions();
    parent->setChildrenAffectedByBackwardPositionalRules();
    parent->setChildrenAffectedByForwardPositionalRules();

    unsigned count = 1;
    for (const auto* sibling = ElementTraversal::previousSibling(*element()); sibling; sibling = ElementTraversal::previousSibling(*sibling))
        ++count;
    return count;
}

void BuilderState::disableNativeAppearanceIfNeeded(CSSPropertyID propertyID, PropertyCascade::Origin origin)
{
    auto shouldDisable = [&] {
        if (origin != PropertyCascade::Origin::Author)
            return false;
        if (!CSSProperty::disablesNativeAppearance(propertyID))
            return false;
        if (!applyPropertyToRegularStyle())
            return false;
        return element()->isDevolvableWidget() || RenderTheme::hasAppearanceForElementTypeFromUAStyle(*element());
    };

    if (shouldDisable())
        style().setNativeAppearanceDisabled(true);
}


} // namespace Style
} // namespace WebCore
