/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2026 Apple Inc. All rights reserved.
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

#include "CSSCalcRandomCachingKey.h"
#include "CSSCanvasValue.h"
#include "CSSColorImageValue.h"
#include "CSSColorValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontSelector.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSLightDarkImageValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "ElementTraversal.h"
#include "FontCache.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLElement.h"
#include "LocalFrame.h"
#include "RenderTheme.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "StyleBuilder.h"
#include "StyleCachedImage.h"
#include "StyleCanvasImage.h"
#include "StyleColor.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleCrossfadeImage.h"
#include "StyleCursorImage.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleFilterImage.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleGradientImage.h"
#include "StyleImageSet.h"
#include "StyleLocalPropertyRegistry.h"
#include "StyleNamedImage.h"
#include "StylePaintImage.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleScope.h"

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BuilderState);

BuilderState::BuilderState(ComputedStyle& style, BuilderContext&& context)
    : m_mutator { style, MutatorContext {
        .document = WTF::move(context.document),
        .element = WTF::move(context.element),
        .parentStyle = context.parentStyle,
        .rootElementStyle = context.rootElementStyle
    } }
    , m_treeResolutionState { WTF::move(context.treeResolutionState) }
    , m_positionTryFallback { WTF::move(context.positionTryFallback) }
    , m_localPropertyRegistry { context.localPropertyRegistry }
    , m_callingContextBuilder { context.callingContextBuilder }
    , m_cssToLengthConversionData(style, *this)
{
}

const CSSRegisteredCustomProperty* BuilderState::registeredProperty(const AtomString& name) const
{
    if (m_localPropertyRegistry)
        return m_localPropertyRegistry->get(name);
    return document().customPropertyRegistry().get(name);
}

float BuilderState::zoomWithTextZoomFactor()
{
    if (auto* frame = document().frame()) {
        float textZoomFactor = style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        float usedZoom = style().evaluationTimeZoomEnabled() ? 1.0f : style().usedZoom();
        return usedZoom * textZoomFactor;
    }
    return cssToLengthConversionData().zoom();
}

bool BuilderState::evaluationTimeZoomEnabled() const
{
    return style().evaluationTimeZoomEnabled();
}

RefPtr<Image> BuilderState::createStyleImage(const CSSValue& value) const
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
    if (auto* colorImageValue = dynamicDowncast<CSSColorImageValue>(value))
        return colorImageValue->createStyleImage(*this);
    if (auto* lightDarkImageValue = dynamicDowncast<CSSLightDarkImageValue>(value))
        return lightDarkImageValue->createStyleImage(*this);
    if (auto* paintImageValue = dynamicDowncast<CSSPaintImageValue>(value))
        return paintImageValue->createStyleImage(*this);
    return nullptr;
}

void BuilderState::registerSubstitutionAttribute(const AtomString& attributeLocalName, const Scope* targetScope)
{
    m_registeredSubstitutionAttributes.append({ attributeLocalName, targetScope });
}

void BuilderState::adjustStyleForInterCharacterRuby()
{
    if (!style().isInterCharacterRubyPosition() || !element() || !element()->hasTagName(HTMLNames::rtTag))
        return;

    style().setTextAlign(TextAlign::Center);
    if (!style().writingMode().isVerticalTypographic())
        style().setWritingMode(StyleWritingMode::VerticalLr);
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
    style().setUsesViewportUnits();
}

void BuilderState::setIsContainerDependent()
{
    style().setIsContainerDependent();
}

double BuilderState::lookupCSSRandomBaseValue(const CSSCalc::RandomCachingKey& key, std::optional<CSS::Keyword::ElementScoped> elementScoped) const
{
    if (elementScoped)
        return protect(element())->lookupCSSRandomBaseValue(style().pseudoElementIdentifier(), key);

    return document().lookupCSSRandomBaseValue(key);
}

// MARK: - Tree Counting Functions

unsigned BuilderState::siblingCount()
{
    // https://drafts.csswg.org/css-values-5/#funcdef-sibling-count

    ASSERT(element());

    // https://drafts.csswg.org/css-shadow-1/#tree-scoped-name-loosely-matched
    // "loosely-matched tree-scoped references" return 0 for cross-tree styling.
    if (m_currentProperty && m_currentProperty->styleScopeOrdinal <= ScopeOrdinal::ContainingHost)
        return 0;

    auto* parent = element()->parentElement();
    if (!parent)
        return 1;

    style().setUsesTreeCountingFunctions();
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

    // https://drafts.csswg.org/css-shadow-1/#tree-scoped-name-loosely-matched
    // "loosely-matched tree-scoped references" return 0 for cross-tree styling.
    if (m_currentProperty && m_currentProperty->styleScopeOrdinal <= ScopeOrdinal::ContainingHost)
        return 0;

    auto* parent = element()->parentElement();
    if (!parent)
        return 1;

    style().setUsesTreeCountingFunctions();
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
        SUPPRESS_UNCOUNTED_ARG return element()->isDevolvableWidget() || RenderTheme::hasAppearanceForElementTypeFromUAStyle(*element());
    };

    if (shouldDisable())
        style().setNativeAppearanceDisabled(true);
}


} // namespace Style
} // namespace WebCore
