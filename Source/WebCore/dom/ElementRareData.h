/*
 * Copyright (C) 2008, 2009, 2010, 2014, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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
 *
 */

#pragma once

#include "CSSCalcRandomCachingKeyMap.h"
#include "CustomElementDefaultARIA.h"
#include "CustomElementReactionQueue.h"
#include "CustomStateSet.h"
#include "DOMTokenList.h"
#include "DatasetDOMStringMap.h"
#include "Element.h"
#include "ElementAnimationRareData.h"
#include "EventTarget.h"
#include "FormAssociatedCustomElement.h"
#include "IntersectionObserver.h"
#include "KeyframeEffectStack.h"
#include "LargestContentfulPaintData.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "PopoverData.h"
#include "PseudoElement.h"
#include "PseudoElementIdentifier.h"
#include "RenderElement.h"
#include "ResizeObserver.h"
#include "ShadowRoot.h"
#include "SpaceSplitString.h"
#include "StylePropertyMap.h"
#include "StylePropertyMapReadOnly.h"
#include "VisibilityAdjustment.h"
#include <wtf/HashMap.h>
#include <wtf/Markable.h>

namespace WebCore {

class ElementRareData : public NodeRareData {
public:
    explicit ElementRareData();
    ~ElementRareData();

    void setBeforePseudoElement(RefPtr<PseudoElement>&&);
    void setAfterPseudoElement(RefPtr<PseudoElement>&&);

    PseudoElement* beforePseudoElement() const { return m_beforePseudoElement.get(); }
    PseudoElement* afterPseudoElement() const { return m_afterPseudoElement.get(); }

    void resetComputedStyle();

    int unusualTabIndex() const;
    void setUnusualTabIndex(int);

    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }
    static constexpr ptrdiff_t childIndexMemoryOffset() { return OBJECT_OFFSETOF(ElementRareData, m_childIndex); }

    void clearShadowRoot() { m_shadowRoot = nullptr; }
    ShadowRoot* shadowRoot() const { return m_shadowRoot.get(); }
    void setShadowRoot(RefPtr<ShadowRoot>&& shadowRoot) { m_shadowRoot = WTF::move(shadowRoot); }

    CustomElementReactionQueue* customElementReactionQueue() { return m_customElementReactionQueue.get(); }
    void setCustomElementReactionQueue(std::unique_ptr<CustomElementReactionQueue>&& queue) { m_customElementReactionQueue = WTF::move(queue); }

    CustomElementDefaultARIA* customElementDefaultARIA() { return m_customElementDefaultARIA.get(); }
    void setCustomElementDefaultARIA(std::unique_ptr<CustomElementDefaultARIA>&& defaultARIA) { m_customElementDefaultARIA = WTF::move(defaultARIA); }

    FormAssociatedCustomElement* formAssociatedCustomElement() { return m_formAssociatedCustomElement.get(); }
    void setFormAssociatedCustomElement(const std::unique_ptr<FormAssociatedCustomElement>&& element) { lazyInitialize(m_formAssociatedCustomElement, std::move(element)); }

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    void setAttributeMap(const std::unique_ptr<NamedNodeMap>&& attributeMap) { lazyInitialize(m_attributeMap, std::move(attributeMap)); }

    String userInfo() const { return m_userInfo; }
    void setUserInfo(String&& userInfo) { m_userInfo = WTF::move(userInfo); }

    RenderStyle* computedStyle() const { return m_computedStyle.get(); }
    void setComputedStyle(std::unique_ptr<RenderStyle>&& computedStyle) { m_computedStyle = WTF::move(computedStyle); }

    RenderStyle* displayContentsOrNoneStyle() const { return m_displayContentsOrNoneStyle.get(); }
    void setDisplayContentsOrNoneStyle(std::unique_ptr<RenderStyle> style) { m_displayContentsOrNoneStyle = WTF::move(style); }

    const AtomString& effectiveLang() const { return m_effectiveLang; }
    void setEffectiveLang(const AtomString& lang) { m_effectiveLang = lang; }

    DOMTokenList* classList() const { return m_classList.get(); }
    void setClassList(const std::unique_ptr<DOMTokenList>&& classList) { lazyInitialize(m_classList, std::move(classList)); }

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    void setDataset(const std::unique_ptr<DatasetDOMStringMap>&& dataset) { lazyInitialize(m_dataset, std::move(dataset)); }

    ScrollPosition savedLayerScrollPosition() const { return m_savedLayerScrollPosition; }
    void setSavedLayerScrollPosition(ScrollPosition position) { m_savedLayerScrollPosition = position; }

    bool hasAnimationRareData() const { return !m_animationRareData.isEmpty(); }
    ElementAnimationRareData* animationRareData(const std::optional<Style::PseudoElementIdentifier>&) const;
    ElementAnimationRareData& ensureAnimationRareData(const std::optional<Style::PseudoElementIdentifier>&);

    AtomString viewTransitionCapturedName(const std::optional<Style::PseudoElementIdentifier>&) const;
    void setViewTransitionCapturedName(const std::optional<Style::PseudoElementIdentifier>&, AtomString);

    DOMTokenList* partList() const { return m_partList.get(); }
    void setPartList(const std::unique_ptr<DOMTokenList>&& partList) { lazyInitialize(m_partList, std::move(partList)); }

    const SpaceSplitString& partNames() const { return m_partNames; }
    void setPartNames(SpaceSplitString&& partNames) { m_partNames = WTF::move(partNames); }

    IntersectionObserverData* intersectionObserverData() { return m_intersectionObserverData.get(); }
    void setIntersectionObserverData(std::unique_ptr<IntersectionObserverData>&& data) { m_intersectionObserverData = WTF::move(data); }

    ResizeObserverData* resizeObserverData() { return m_resizeObserverData.get(); }
    void setResizeObserverData(std::unique_ptr<ResizeObserverData>&& data) { m_resizeObserverData = WTF::move(data); }

    ElementLargestContentfulPaintData* largestContentfulPaintData() { return m_largestContentfulPaintData.get(); }
    void setLargestContentfulPaintData(std::unique_ptr<ElementLargestContentfulPaintData>&& data) { m_largestContentfulPaintData = WTF::move(data); }

    std::optional<LayoutUnit> lastRememberedLogicalWidth() const { return m_lastRememberedLogicalWidth; }
    std::optional<LayoutUnit> lastRememberedLogicalHeight() const { return m_lastRememberedLogicalHeight; }
    void setLastRememberedLogicalWidth(LayoutUnit width) { m_lastRememberedLogicalWidth = width; }
    void setLastRememberedLogicalHeight(LayoutUnit height) { m_lastRememberedLogicalHeight = height; }
    void clearLastRememberedLogicalWidth() { m_lastRememberedLogicalWidth.reset(); }
    void clearLastRememberedLogicalHeight() { m_lastRememberedLogicalHeight.reset(); }

    const AtomString& nonce() const { return m_nonce; }
    void setNonce(const AtomString& value) { m_nonce = value; }

    StylePropertyMap* attributeStyleMap() { return m_attributeStyleMap.get(); }
    void setAttributeStyleMap(Ref<StylePropertyMap>&& map) { m_attributeStyleMap = WTF::move(map); }

    StylePropertyMapReadOnly* computedStyleMap() { return m_computedStyleMap.get(); }
    void setComputedStyleMap(Ref<StylePropertyMapReadOnly>&& map) { m_computedStyleMap = WTF::move(map); }

    ExplicitlySetAttrElementsMap& explicitlySetAttrElementsMap() { return m_explicitlySetAttrElementsMap; }

    PopoverData* popoverData() { return m_popoverData.get(); }
    void setPopoverData(std::unique_ptr<PopoverData>&& popoverData) { m_popoverData = WTF::move(popoverData); }

    Element* invokedPopover() const { return m_invokedPopover.get(); }
    void setInvokedPopover(RefPtr<Element>&& element) { m_invokedPopover = WTF::move(element); }

    const std::optional<OptionSet<ContentRelevancy>>& contentRelevancy() const { return m_contentRelevancy; }
    void setContentRelevancy(OptionSet<ContentRelevancy>& contentRelevancy) { m_contentRelevancy = contentRelevancy; }

    CustomStateSet* customStateSet() { return m_customStateSet.get(); }
    void setCustomStateSet(Ref<CustomStateSet>&& customStateSet) { m_customStateSet = WTF::move(customStateSet); }

    OptionSet<VisibilityAdjustment> visibilityAdjustment() const { return m_visibilityAdjustment; }
    void setVisibilityAdjustment(OptionSet<VisibilityAdjustment> adjustment) { m_visibilityAdjustment = adjustment; }

    Ref<CSSCalc::RandomCachingKeyMap> ensureRandomCachingKeyMap(const std::optional<Style::PseudoElementIdentifier>&);
    bool hasRandomCachingKeyMap() const;

#if DUMP_NODE_STATISTICS
    OptionSet<UseType> useTypes() const
    {
        auto result = NodeRareData::useTypes();
        if (m_unusualTabIndex)
            result.add(UseType::TabIndex);
        if (!m_savedLayerScrollPosition.isZero())
            result.add(UseType::ScrollingPosition);
        if (m_computedStyle)
            result.add(UseType::ComputedStyle);
        if (m_displayContentsOrNoneStyle)
            result.add(UseType::DisplayContentsOrNoneStyle);
        if (!m_effectiveLang.isEmpty())
            result.add(UseType::EffectiveLang);
        if (m_dataset)
            result.add(UseType::Dataset);
        if (m_classList)
            result.add(UseType::ClassList);
        if (m_shadowRoot)
            result.add(UseType::ShadowRoot);
        if (m_customElementReactionQueue)
            result.add(UseType::CustomElementReactionQueue);
        if (m_customElementDefaultARIA)
            result.add(UseType::CustomElementDefaultARIA);
        if (m_formAssociatedCustomElement)
            result.add(UseType::FormAssociatedCustomElement);
        if (m_attributeMap)
            result.add(UseType::AttributeMap);
        if (m_intersectionObserverData)
            result.add(UseType::InteractionObserver);
        if (m_resizeObserverData || m_lastRememberedLogicalWidth || m_lastRememberedLogicalHeight)
            result.add(UseType::ResizeObserver);
        if (!m_animationRareData.isEmpty())
            result.add(UseType::Animations);
        if (m_beforePseudoElement || m_afterPseudoElement)
            result.add(UseType::PseudoElements);
        if (m_attributeStyleMap)
            result.add(UseType::AttributeStyleMap);
        if (m_computedStyleMap)
            result.add(UseType::ComputedStyleMap);
        if (m_partList)
            result.add(UseType::PartList);
        if (!m_partNames.isEmpty())
            result.add(UseType::PartNames);
        if (!m_nonce.isEmpty())
            result.add(UseType::Nonce);
        if (!m_explicitlySetAttrElementsMap.isEmpty())
            result.add(UseType::ExplicitlySetAttrElementsMap);
        if (m_popoverData)
            result.add(UseType::Popover);
        if (m_childIndex)
            result.add(UseType::ChildIndex);
        if (!m_customStateSet.isEmpty())
            result.add(UseType::CustomStateSet);
        if (m_userInfo)
            result.add(UseType::UserInfo);
        if (m_invokedPopover)
            result.add(UseType::InvokedPopover);
        return result;
    }
#endif

private:
    unsigned short m_childIndex { 0 }; // Keep on top for better bit packing with NodeRareData.
    int m_unusualTabIndex { 0 }; // Keep on top for better bit packing with NodeRareData.

    std::optional<OptionSet<ContentRelevancy>> m_contentRelevancy;
    ScrollPosition m_savedLayerScrollPosition;

    String m_userInfo;

    std::unique_ptr<RenderStyle> m_computedStyle;
    std::unique_ptr<RenderStyle> m_displayContentsOrNoneStyle;

    AtomString m_effectiveLang;
    const std::unique_ptr<DatasetDOMStringMap> m_dataset;
    const std::unique_ptr<DOMTokenList> m_classList;
    RefPtr<ShadowRoot> m_shadowRoot;
    std::unique_ptr<CustomElementReactionQueue> m_customElementReactionQueue;
    std::unique_ptr<CustomElementDefaultARIA> m_customElementDefaultARIA;
    const std::unique_ptr<FormAssociatedCustomElement> m_formAssociatedCustomElement;
    const std::unique_ptr<NamedNodeMap> m_attributeMap;

    std::unique_ptr<IntersectionObserverData> m_intersectionObserverData;
    std::unique_ptr<ResizeObserverData> m_resizeObserverData;
    std::unique_ptr<ElementLargestContentfulPaintData> m_largestContentfulPaintData;

    Markable<LayoutUnit> m_lastRememberedLogicalWidth;
    Markable<LayoutUnit> m_lastRememberedLogicalHeight;

    HashMap<std::optional<Style::PseudoElementIdentifier>, std::unique_ptr<ElementAnimationRareData>> m_animationRareData;

    HashMap<std::optional<Style::PseudoElementIdentifier>, AtomString> m_viewTransitionCapturedName;

    RefPtr<PseudoElement> m_beforePseudoElement;
    RefPtr<PseudoElement> m_afterPseudoElement;

    RefPtr<StylePropertyMap> m_attributeStyleMap;
    RefPtr<StylePropertyMapReadOnly> m_computedStyleMap;

    const std::unique_ptr<DOMTokenList> m_partList;
    SpaceSplitString m_partNames;

    AtomString m_nonce;

    ExplicitlySetAttrElementsMap m_explicitlySetAttrElementsMap;

    std::unique_ptr<PopoverData> m_popoverData;

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_invokedPopover;

    RefPtr<CustomStateSet> m_customStateSet;

    OptionSet<VisibilityAdjustment> m_visibilityAdjustment;

    HashMap<std::optional<Style::PseudoElementIdentifier>, Ref<CSSCalc::RandomCachingKeyMap>> m_randomCachingKeyMaps;
};

inline ElementRareData::ElementRareData()
    : NodeRareData(Type::Element)
{
}

inline ElementRareData::~ElementRareData()
{
    ASSERT(!m_shadowRoot);
    ASSERT(!m_beforePseudoElement);
    ASSERT(!m_afterPseudoElement);
}

inline void ElementRareData::setBeforePseudoElement(RefPtr<PseudoElement>&& pseudoElement)
{
    ASSERT(!m_beforePseudoElement || !pseudoElement);
    m_beforePseudoElement = WTF::move(pseudoElement);
}

inline void ElementRareData::setAfterPseudoElement(RefPtr<PseudoElement>&& pseudoElement)
{
    ASSERT(!m_afterPseudoElement || !pseudoElement);
    m_afterPseudoElement = WTF::move(pseudoElement);
}

inline void ElementRareData::resetComputedStyle()
{
    m_computedStyle = nullptr;
}

inline int ElementRareData::unusualTabIndex() const
{
    ASSERT(m_unusualTabIndex); // setUnusualTabIndex must have been called before this.
    return m_unusualTabIndex;
}

inline void ElementRareData::setUnusualTabIndex(int tabIndex)
{
    ASSERT(tabIndex && tabIndex != -1); // Common values of 0 and -1 are stored as TabIndexState in Node.
    m_unusualTabIndex = tabIndex;
}

inline ElementAnimationRareData* ElementRareData::animationRareData(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    return m_animationRareData.get(pseudoElementIdentifier);
}

inline ElementAnimationRareData& ElementRareData::ensureAnimationRareData(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    if (auto* animationRareData = this->animationRareData(pseudoElementIdentifier))
        return *animationRareData;

    auto result = m_animationRareData.add(pseudoElementIdentifier, makeUnique<ElementAnimationRareData>());
    ASSERT(result.isNewEntry);
    return *result.iterator->value.get();
}

inline AtomString ElementRareData::viewTransitionCapturedName(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier) const
{
    return m_viewTransitionCapturedName.get(pseudoElementIdentifier);
}

inline void ElementRareData::setViewTransitionCapturedName(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, AtomString captureName)
{
    m_viewTransitionCapturedName.set(pseudoElementIdentifier, captureName);
}

inline Ref<CSSCalc::RandomCachingKeyMap> ElementRareData::ensureRandomCachingKeyMap(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return m_randomCachingKeyMaps.ensure(pseudoElementIdentifier, [] {
        return CSSCalc::RandomCachingKeyMap::create();
    }).iterator->value;
}

inline bool ElementRareData::hasRandomCachingKeyMap() const
{
    return !m_randomCachingKeyMaps.isEmpty();
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(hasRareData());
    return downcast<ElementRareData>(rareData());
}

inline ShadowRoot* Node::shadowRoot() const
{
    if (auto* element = dynamicDowncast<Element>(*this))
        return element->shadowRoot();
    return nullptr;
}

inline ShadowRoot* Element::shadowRoot() const
{
    return hasRareData() ? elementRareData()->shadowRoot() : nullptr;
}

inline void Element::removeShadowRoot()
{
    RefPtr shadowRoot = this->shadowRoot();
    if (!shadowRoot) [[likely]]
        return;
    removeShadowRootSlow(*shadowRoot);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ElementRareData)
    static bool isType(const WebCore::NodeRareData& rareData) { return rareData.isElementRareData(); }
SPECIALIZE_TYPE_TRAITS_END()
