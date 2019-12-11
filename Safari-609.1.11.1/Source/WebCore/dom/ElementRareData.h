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

#include "CustomElementReactionQueue.h"
#include "DOMTokenList.h"
#include "DatasetDOMStringMap.h"
#include "IntersectionObserver.h"
#include "KeyframeEffectStack.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "PseudoElement.h"
#include "RenderElement.h"
#include "ResizeObserver.h"
#include "ShadowRoot.h"
#include "SpaceSplitString.h"
#include "StylePropertyMap.h"

namespace WebCore {

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(LayoutUnit::max(), LayoutUnit::max());
}

class ElementRareData : public NodeRareData {
public:
    explicit ElementRareData();
    ~ElementRareData();

    void setBeforePseudoElement(RefPtr<PseudoElement>&&);
    void setAfterPseudoElement(RefPtr<PseudoElement>&&);

    PseudoElement* beforePseudoElement() const { return m_beforePseudoElement.get(); }
    PseudoElement* afterPseudoElement() const { return m_afterPseudoElement.get(); }

    void resetComputedStyle();
    void resetStyleRelations();
    
    Optional<int> tabIndex() const { return m_tabIndexWasSetExplicitly ? Optional<int> { m_tabIndex } : WTF::nullopt; }
    void setTabIndexExplicitly(int index) { m_tabIndex = index; m_tabIndexWasSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexWasSetExplicitly; }
    void clearTabIndexExplicitly() { m_tabIndex = 0; m_tabIndexWasSetExplicitly = false; }

#if ENABLE(FULLSCREEN_API)
    bool containsFullScreenElement() { return m_containsFullScreenElement; }
    void setContainsFullScreenElement(bool value) { m_containsFullScreenElement = value; }
#endif

    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }
    static ptrdiff_t childIndexMemoryOffset() { return OBJECT_OFFSETOF(ElementRareData, m_childIndex); }

    void clearShadowRoot() { m_shadowRoot = nullptr; }
    ShadowRoot* shadowRoot() const { return m_shadowRoot.get(); }
    void setShadowRoot(RefPtr<ShadowRoot>&& shadowRoot) { m_shadowRoot = WTFMove(shadowRoot); }

    CustomElementReactionQueue* customElementReactionQueue() { return m_customElementReactionQueue.get(); }
    void setCustomElementReactionQueue(std::unique_ptr<CustomElementReactionQueue>&& queue) { m_customElementReactionQueue = WTFMove(queue); }

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    void setAttributeMap(std::unique_ptr<NamedNodeMap> attributeMap) { m_attributeMap = WTFMove(attributeMap); }

    RenderStyle* computedStyle() const { return m_computedStyle.get(); }
    void setComputedStyle(std::unique_ptr<RenderStyle> computedStyle) { m_computedStyle = WTFMove(computedStyle); }

    DOMTokenList* classList() const { return m_classList.get(); }
    void setClassList(std::unique_ptr<DOMTokenList> classList) { m_classList = WTFMove(classList); }

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    void setDataset(std::unique_ptr<DatasetDOMStringMap> dataset) { m_dataset = WTFMove(dataset); }

    LayoutSize minimumSizeForResizing() const { return m_minimumSizeForResizing; }
    void setMinimumSizeForResizing(LayoutSize size) { m_minimumSizeForResizing = size; }

    IntPoint savedLayerScrollPosition() const { return m_savedLayerScrollPosition; }
    void setSavedLayerScrollPosition(IntPoint position) { m_savedLayerScrollPosition = position; }

    bool hasPendingResources() const { return m_hasPendingResources; }
    void setHasPendingResources(bool has) { m_hasPendingResources = has; }

    bool hasCSSAnimation() const { return m_hasCSSAnimation; }
    void setHasCSSAnimation(bool value) { m_hasCSSAnimation = value; }

    KeyframeEffectStack* keyframeEffectStack() { return m_keyframeEffectStack.get(); }
    void setKeyframeEffectStack(std::unique_ptr<KeyframeEffectStack>&& keyframeEffectStack) { m_keyframeEffectStack = WTFMove(keyframeEffectStack); }

    bool hasElementIdentifier() const { return m_hasElementIdentifier; }
    void setHasElementIdentifier(bool value) { m_hasElementIdentifier = value; }

    DOMTokenList* partList() const { return m_partList.get(); }
    void setPartList(std::unique_ptr<DOMTokenList> partList) { m_partList = WTFMove(partList); }

    const SpaceSplitString& partNames() const { return m_partNames; }
    void setPartNames(SpaceSplitString&& partNames) { m_partNames = WTFMove(partNames); }

#if ENABLE(INTERSECTION_OBSERVER)
    IntersectionObserverData* intersectionObserverData() { return m_intersectionObserverData.get(); }
    void setIntersectionObserverData(std::unique_ptr<IntersectionObserverData>&& data) { m_intersectionObserverData = WTFMove(data); }
#endif

#if ENABLE(RESIZE_OBSERVER)
    ResizeObserverData* resizeObserverData() { return m_resizeObserverData.get(); }
    void setResizeObserverData(std::unique_ptr<ResizeObserverData>&& data) { m_resizeObserverData = WTFMove(data); }
#endif

#if ENABLE(CSS_TYPED_OM)
    StylePropertyMap* attributeStyleMap() { return m_attributeStyleMap.get(); }
    void setAttributeStyleMap(Ref<StylePropertyMap>&& map) { m_attributeStyleMap = WTFMove(map); }
#endif

#if DUMP_NODE_STATISTICS
    OptionSet<UseType> useTypes() const
    {
        auto result = NodeRareData::useTypes();
        if (m_tabIndexWasSetExplicitly)
            result.add(UseType::TabIndex);
        if (m_minimumSizeForResizing != defaultMinimumSizeForResizing())
            result.add(UseType::MinimumSize);
        if (!m_savedLayerScrollPosition.isZero())
            result.add(UseType::ScrollingPosition);
        if (m_computedStyle)
            result.add(UseType::ComputedStyle);
        if (m_dataset)
            result.add(UseType::Dataset);
        if (m_classList)
            result.add(UseType::ClassList);
        if (m_shadowRoot)
            result.add(UseType::ShadowRoot);
        if (m_customElementReactionQueue)
            result.add(UseType::CustomElementQueue);
        if (m_attributeMap)
            result.add(UseType::AttributeMap);
        if (m_intersectionObserverData)
            result.add(UseType::InteractionObserver);
#if ENABLE(RESIZE_OBSERVER)
        if (m_resizeObserverData)
            result.add(UseType::ResizeObserver);
#endif
        if (m_beforePseudoElement || m_afterPseudoElement)
            result.add(UseType::PseudoElements);
        return result;
    }
#endif

private:
    int m_tabIndex;
    unsigned short m_childIndex;
    unsigned m_tabIndexWasSetExplicitly : 1;
#if ENABLE(FULLSCREEN_API)
    unsigned m_containsFullScreenElement : 1;
#endif
    unsigned m_hasPendingResources : 1;
    unsigned m_hasCSSAnimation : 1;
    unsigned m_hasElementIdentifier : 1;

    LayoutSize m_minimumSizeForResizing;
    IntPoint m_savedLayerScrollPosition;
    std::unique_ptr<RenderStyle> m_computedStyle;

    std::unique_ptr<DatasetDOMStringMap> m_dataset;
    std::unique_ptr<DOMTokenList> m_classList;
    RefPtr<ShadowRoot> m_shadowRoot;
    std::unique_ptr<CustomElementReactionQueue> m_customElementReactionQueue;
    std::unique_ptr<NamedNodeMap> m_attributeMap;
#if ENABLE(INTERSECTION_OBSERVER)
    std::unique_ptr<IntersectionObserverData> m_intersectionObserverData;
#endif

#if ENABLE(RESIZE_OBSERVER)
    std::unique_ptr<ResizeObserverData> m_resizeObserverData;
#endif

    std::unique_ptr<KeyframeEffectStack> m_keyframeEffectStack;

    RefPtr<PseudoElement> m_beforePseudoElement;
    RefPtr<PseudoElement> m_afterPseudoElement;

#if ENABLE(CSS_TYPED_OM)
    RefPtr<StylePropertyMap> m_attributeStyleMap;
#endif

    std::unique_ptr<DOMTokenList> m_partList;
    SpaceSplitString m_partNames;

    void releasePseudoElement(PseudoElement*);
};

inline ElementRareData::ElementRareData()
    : NodeRareData(Type::Element)
    , m_tabIndex(0)
    , m_childIndex(0)
    , m_tabIndexWasSetExplicitly(false)
#if ENABLE(FULLSCREEN_API)
    , m_containsFullScreenElement(false)
#endif
    , m_hasPendingResources(false)
    , m_hasCSSAnimation(false)
    , m_hasElementIdentifier(false)
    , m_minimumSizeForResizing(defaultMinimumSizeForResizing())
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
    m_beforePseudoElement = WTFMove(pseudoElement);
}

inline void ElementRareData::setAfterPseudoElement(RefPtr<PseudoElement>&& pseudoElement)
{
    ASSERT(!m_afterPseudoElement || !pseudoElement);
    m_afterPseudoElement = WTFMove(pseudoElement);
}

inline void ElementRareData::resetComputedStyle()
{
    m_computedStyle = nullptr;
}

inline void ElementRareData::resetStyleRelations()
{
    setChildIndex(0);
}

} // namespace WebCore
