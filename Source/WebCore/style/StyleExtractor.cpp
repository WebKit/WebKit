/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"
#include "StyleExtractor.h"

#include "CSSProperty.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSSerializationContext.h"
#include "CSSValuePool.h"
#include "ComposedTreeAncestorIterator.h"
#include "ContainerNodeInlines.h"
#include "CustomPropertyRegistry.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "HTMLFrameOwnerElement.h"
#include "NodeRenderStyle.h"
#include "PseudoElementIdentifier.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "SVGElement.h"
#include "StyleExtractorGenerated.h"
#include "StyleInterpolation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "Styleable.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Extractor);

enum class AdjustPixelValuesForComputedStyle : bool { No, Yes };
enum class ForcedLayout : uint8_t { No, Yes, ParentDocument };

using PhysicalDirection = BoxSide;
using FlowRelativeDirection = LogicalBoxSide;

static Element* styleElementForNode(Node* node)
{
    if (!node)
        return nullptr;
    if (auto* element = dynamicDowncast<Element>(*node))
        return element;
    return composedTreeAncestors(*node).first();
}

Extractor::Extractor(Node* node, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
    : Extractor(styleElementForNode(node), allowVisitedStyle, pseudoElementIdentifier)
{
}

Extractor::Extractor(Node* node, bool allowVisitedStyle)
    : Extractor(node, allowVisitedStyle, std::nullopt)
{
}

Extractor::Extractor(Element* element, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
    : m_element(element)
    , m_pseudoElementIdentifier(pseudoElementIdentifier)
    , m_allowVisitedStyle(allowVisitedStyle)
{
}

Extractor::Extractor(Element* element, bool allowVisitedStyle)
    : Extractor(element, allowVisitedStyle, std::nullopt)
{
}

RefPtr<CSSPrimitiveValue> Extractor::getFontSizeCSSValuePreferringKeyword() const
{
    RefPtr element = m_element;
    if (!element)
        return nullptr;

    element->protectedDocument()->updateLayoutIgnorePendingStylesheets();

    auto* style = element->computedStyle(m_pseudoElementIdentifier);
    if (!style)
        return nullptr;

    if (auto sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier())
        return CSSPrimitiveValue::create(sizeIdentifier);

    return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(style->fontDescription().computedSize(), *style), CSSUnitType::CSS_PX);
}

bool Extractor::useFixedFontDefaultSize() const
{
    RefPtr element = m_element;
    if (!element)
        return false;

    auto* style = element->computedStyle(m_pseudoElementIdentifier);
    if (!style)
        return false;

    return style->fontDescription().useFixedDefaultSize();
}

RenderElement* Extractor::styledRenderer() const
{
    RefPtr element = m_element;
    if (!element)
        return nullptr;
    if (m_pseudoElementIdentifier)
        return Styleable(*element, m_pseudoElementIdentifier).renderer();
    if (element->hasDisplayContents())
        return nullptr;
    return element->renderer();
}

static inline bool hasValidStyleForProperty(Element& element, CSSPropertyID propertyID)
{
    if (element.styleValidity() != Style::Validity::Valid)
        return false;
    if (element.document().hasPendingFullStyleRebuild())
        return false;
    if (!element.document().childNeedsStyleRecalc())
        return true;

    if (auto* keyframeEffectStack = Styleable(element, { }).keyframeEffectStack()) {
        if (keyframeEffectStack->containsProperty(propertyID))
            return false;
    }

    auto isQueryContainer = [&](Element& element) {
        auto* style = element.renderStyle();
        return style && style->containerType() != ContainerType::Normal;
    };

    if (isQueryContainer(element))
        return false;

    const auto* currentElement = &element;
    for (auto& ancestor : composedTreeAncestors(element)) {
        if (ancestor.styleValidity() != Style::Validity::Valid)
            return false;

        if (isQueryContainer(ancestor))
            return false;

        if (ancestor.directChildNeedsStyleRecalc() && currentElement->styleIsAffectedByPreviousSibling())
            return false;

        currentElement = &ancestor;
    }

    return true;
}

bool Extractor::updateStyleIfNeededForProperty(Element& element, CSSPropertyID propertyID)
{
    Ref document = element.document();

    document->styleScope().flushPendingUpdate();

    auto hasValidStyle = [&] {
        auto shorthand = shorthandForProperty(propertyID);
        if (shorthand.length()) {
            for (auto longhand : shorthand) {
                if (!hasValidStyleForProperty(element, longhand))
                    return false;
            }
            return true;
        }
        return hasValidStyleForProperty(element, propertyID);
    }();

    if (hasValidStyle)
        return false;

    document->updateStyleIfNeeded();
    return true;
}

static inline const RenderStyle* computeRenderStyleForProperty(Element& element, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, CSSPropertyID propertyID, std::unique_ptr<RenderStyle>& ownedStyle, SingleThreadWeakPtr<RenderElement> renderer)
{
    if (!renderer)
        renderer = element.renderer();

    if (renderer && renderer->isComposited() && Style::Interpolation::isAccelerated(propertyID, element.document().settings())) {
        ownedStyle = renderer->animatedStyle();
        if (pseudoElementIdentifier) {
            // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
            return ownedStyle->getCachedPseudoStyle(*pseudoElementIdentifier);
        }
        return ownedStyle.get();
    }

    return element.computedStyle(pseudoElementIdentifier);
}

RefPtr<CSSValue> Extractor::customPropertyValue(const AtomString& propertyName) const
{
    RefPtr element = m_element;
    if (!element)
        return nullptr;

    updateStyleIfNeededForProperty(*element, CSSPropertyCustom);

    std::unique_ptr<RenderStyle> ownedStyle;
    auto* style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, CSSPropertyCustom, ownedStyle, nullptr);
    if (!style)
        return nullptr;

    Ref document = element->document();

    if (document->hasStyleWithViewportUnits()) {
        if (RefPtr owner = document->ownerElement()) {
            owner->document().updateLayout();
            style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, CSSPropertyCustom, ownedStyle, nullptr);
        }
    }

    return const_cast<CSSCustomPropertyValue*>(style->customPropertyValue(propertyName));
}

String Extractor::customPropertyText(const AtomString& propertyName) const
{
    RefPtr propertyValue = customPropertyValue(propertyName);
    return propertyValue ? propertyValue->cssText(CSS::defaultSerializationContext()) : emptyString();
}

static bool isLayoutDependent(CSSPropertyID propertyID, const RenderStyle* style, RenderObject* renderer)
{
    auto isNonReplacedInline = [](auto& renderer) {
        return renderer.isInline() && !renderer.isReplacedOrAtomicInline();
    };

    auto formattingContextRootStyle = [](auto& renderer) -> const RenderStyle& {
        if (auto* ancestorToUse = (renderer.isFlexItem() || renderer.isGridItem()) ? renderer.parent() : renderer.containingBlock())
            return ancestorToUse->style();
        ASSERT_NOT_REACHED();
        return renderer.style();
    };

    auto mapLogicalToPhysicalPaddingProperty = [&](auto direction, auto& renderer) -> CSSPropertyID {
        switch (mapSideLogicalToPhysical(formattingContextRootStyle(renderer).writingMode(), direction)) {
        case PhysicalDirection::Top:
            return CSSPropertyPaddingTop;
        case PhysicalDirection::Right:
            return CSSPropertyPaddingRight;
        case PhysicalDirection::Bottom:
            return CSSPropertyPaddingBottom;
        case PhysicalDirection::Left:
            return CSSPropertyPaddingLeft;
        default:
            ASSERT_NOT_REACHED();
            return CSSPropertyInvalid;
        }
    };

    auto paddingIsLayoutDependent = []<auto lengthGetter>(auto* style, auto* renderer) -> bool {
        return renderer && style && renderer->isRenderBox() && !(style->*lengthGetter)().isFixed();
    };

    switch (propertyID) {
    case CSSPropertyTop:
    case CSSPropertyBottom:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:
        return renderer && style && renderer->isRenderBox();
    case CSSPropertyWidth:
    case CSSPropertyHeight:
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
        return renderer && !renderer->isRenderOrLegacyRenderSVGModelObject() && !isNonReplacedInline(*renderer);
    case CSSPropertyMargin:
    case CSSPropertyMarginBlock:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginBlockEnd:
    case CSSPropertyMarginInline:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
        return renderer && renderer->isRenderBox();
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyTransformOrigin:
    case CSSPropertyTransform:
    case CSSPropertyFilter: // Why are filters layout-dependent?
    case CSSPropertyBackdropFilter: // Why are backdrop-filters layout-dependent?
    case CSSPropertyWebkitBackdropFilter: // Why are backdrop-filters layout-dependent?
        return true;
    case CSSPropertyPadding:
        return isLayoutDependent(CSSPropertyPaddingBlock, style, renderer) || isLayoutDependent(CSSPropertyPaddingInline, style, renderer);
    case CSSPropertyPaddingBlock:
        return isLayoutDependent(CSSPropertyPaddingBlockStart, style, renderer) || isLayoutDependent(CSSPropertyPaddingBlockEnd, style, renderer);
    case CSSPropertyPaddingInline:
        return isLayoutDependent(CSSPropertyPaddingInlineStart, style, renderer) || isLayoutDependent(CSSPropertyPaddingInlineEnd, style, renderer);
    case CSSPropertyPaddingBlockStart:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::BlockStart, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingBlockEnd:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::BlockEnd, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingInlineStart:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::InlineStart, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingInlineEnd:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::InlineEnd, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingTop:
        return paddingIsLayoutDependent.template operator()<&RenderStyle::paddingTop>(style, renderer);
    case CSSPropertyPaddingRight:
        return paddingIsLayoutDependent.template operator()<&RenderStyle::paddingRight>(style, renderer);
    case CSSPropertyPaddingBottom:
        return paddingIsLayoutDependent.template operator()<&RenderStyle::paddingBottom>(style, renderer);
    case CSSPropertyPaddingLeft:
        return paddingIsLayoutDependent.template operator()<&RenderStyle::paddingLeft>(style, renderer);
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyGridTemplate:
    case CSSPropertyGrid:
        return renderer && renderer->isRenderGrid();
    default:
        return false;
    }
}

RefPtr<CSSValue> Extractor::propertyValue(CSSPropertyID propertyID, UpdateLayout updateLayout, ExtractorState::PropertyValueType valueType) const
{
    RefPtr element = m_element.get();
    if (!element)
        return nullptr;

    if (!isExposed(propertyID, element->document().settings())) {
        // Exit quickly, and avoid us ever having to update layout in this case.
        return nullptr;
    }

    std::unique_ptr<RenderStyle> ownedStyle;
    const RenderStyle* style = nullptr;
    auto forcedLayout = ForcedLayout::No;

    if (updateLayout == UpdateLayout::Yes) {
        Ref document = element->document();

        updateStyleIfNeededForProperty(*element, propertyID);
        if (propertyID == CSSPropertyDisplay && !styledRenderer()) {
            RefPtr svgElement = dynamicDowncast<SVGElement>(*element);
            if (svgElement && !svgElement->isValid())
                return nullptr;
        }

        style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, propertyID, ownedStyle, styledRenderer());

        forcedLayout = [&] {
            // FIXME: Some of these cases could be narrowed down or optimized better.
            if (isLayoutDependent(propertyID, style, styledRenderer()))
                return ForcedLayout::Yes;
            // FIXME: Why?
            if (element->isInShadowTree())
                return ForcedLayout::Yes;
            if (!document->ownerElement())
                return ForcedLayout::No;
            if (!document->styleScope().resolverIfExists())
                return ForcedLayout::No;
            if (auto& ruleSets = document->styleScope().resolverIfExists()->ruleSets(); ruleSets.hasViewportDependentMediaQueries() || ruleSets.hasContainerQueries())
                return ForcedLayout::Yes;
            // FIXME: Can we limit this to properties whose computed length value derived from a viewport unit?
            if (document->hasStyleWithViewportUnits())
                return ForcedLayout::ParentDocument;
            return ForcedLayout::No;
        }();

        if (forcedLayout == ForcedLayout::Yes)
            document->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, element.get());
        else if (forcedLayout == ForcedLayout::ParentDocument) {
            if (RefPtr owner = document->ownerElement())
                owner->protectedDocument()->updateLayout();
            else
                forcedLayout = ForcedLayout::No;
        }
    }

    if (updateLayout == UpdateLayout::No || forcedLayout != ForcedLayout::No)
        style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, propertyID, ownedStyle, styledRenderer());

    if (!style)
        return nullptr;

    return valueForPropertyInStyle(*style, propertyID, CSSValuePool::singleton(), valueType == ExtractorState::PropertyValueType::Resolved ? styledRenderer() : nullptr, valueType);
}

bool Extractor::hasProperty(CSSPropertyID propertyID) const
{
    return propertyValue(propertyID);
}

RefPtr<CSSValue> Extractor::valueForPropertyInStyle(const RenderStyle& style, CSSPropertyID propertyID, CSSValuePool& cssValuePool, RenderElement* renderer, ExtractorState::PropertyValueType valueType) const
{
    ASSERT(isExposed(propertyID, m_element->document().settings()));

    ExtractorState state {
        .valueType = valueType,
        .style = style,
        .element = *m_element,
        .pseudoElementIdentifier = m_pseudoElementIdentifier,
        .renderer = renderer,
        .allowVisitedStyle = m_allowVisitedStyle,
        .pool = cssValuePool,
    };
    return ExtractorGenerated::extractValue(state, propertyID);
}

bool Extractor::propertyMatches(CSSPropertyID propertyID, const CSSValue* value) const
{
    if (!m_element)
        return false;
    if (propertyID == CSSPropertyFontSize) {
        if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(*value)) {
            m_element->protectedDocument()->updateLayoutIgnorePendingStylesheets();
            if (auto* style = m_element->computedStyle(m_pseudoElementIdentifier)) {
                if (CSSValueID sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier()) {
                    if (primitiveValue->isValueID() && primitiveValue->valueID() == sizeIdentifier)
                        return true;
                }
            }
        }
    }
    RefPtr<CSSValue> computedValue = propertyValue(propertyID);
    return computedValue && value && computedValue->equals(*value);
}

Ref<MutableStyleProperties> Extractor::copyProperties(std::span<const CSSPropertyID> properties) const
{
    auto vector = WTF::compactMap(properties, [&](auto& property) -> std::optional<CSSProperty> {
        if (auto value = propertyValue(property))
            return CSSProperty(property, value.releaseNonNull());
        return std::nullopt;
    });
    return MutableStyleProperties::create(WTFMove(vector));
}

Ref<MutableStyleProperties> Extractor::copyProperties() const
{
    return MutableStyleProperties::create(WTF::compactMap(allLonghandCSSProperties(), [this] (auto property) -> std::optional<CSSProperty> {
        auto value = propertyValue(property);
        if (!value)
            return std::nullopt;
        return { { property, value.releaseNonNull() } };
    }).span());
}

} // namespace Style
} // namespace WebCore
