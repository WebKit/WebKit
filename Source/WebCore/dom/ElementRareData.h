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
#include "ElementAnimationRareData.h"
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

    int unusualTabIndex() const;
    void setUnusualTabIndex(int);

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

    ElementAnimationRareData* elementAnimationRareData() { return m_animationRareData.get(); }
    ElementAnimationRareData& ensureAnimationRareData();

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
        if (m_unusualTabIndex)
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
        if (m_animationRareData)
            result.add(UseType::Animations);
        if (m_beforePseudoElement || m_afterPseudoElement)
            result.add(UseType::PseudoElements);
#if ENABLE(CSS_TYPED_OM)
        if (m_attributeStyleMap)
            result.add(UseType::StyleMap);
#endif
        if (m_partList)
            result.add(UseType::PartList);
        if (!m_partNames.isEmpty())
            result.add(UseType::PartNames);
        return result;
    }
#endif

private:
    int m_unusualTabIndex { 0 };
    unsigned short m_childIndex { 0 };

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

    std::unique_ptr<ElementAnimationRareData> m_animationRareData;

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

inline ElementAnimationRareData& ElementRareData::ensureAnimationRareData()
{
    if (!m_animationRareData)
        m_animationRareData = makeUnique<ElementAnimationRareData>();
    return *m_animationRareData.get();
}

} // namespace WebCore
