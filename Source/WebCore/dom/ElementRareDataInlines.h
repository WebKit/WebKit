/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CustomElementDefaultARIA.h"
#include "CustomElementReactionQueue.h"
#include "DOMTokenList.h"
#include "DatasetDOMStringMap.h"
#include "ElementAnimationRareData.h"
#include "ElementRareData.h"
#include "FormAssociatedCustomElement.h"
#include "IntersectionObserver.h"
#include "KeyframeEffectStack.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "PopoverData.h"
#include "PseudoElement.h"
#include "RenderStyle.h"
#include "ResizeObserver.h"
#include "ShadowRoot.h"
#include "StylePropertyMap.h"
#include "StylePropertyMapReadOnly.h"

namespace WebCore {

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

inline void ElementRareData::setUnusualTabIndex(int tabIndex)
{
    ASSERT(tabIndex && tabIndex != -1); // Common values of 0 and -1 are stored as TabIndexState in Node.
    m_unusualTabIndex = tabIndex;
}

inline ElementAnimationRareData* ElementRareData::animationRareData(PseudoId pseudoId) const
{
    for (auto& animationRareData : m_animationRareData) {
        if (animationRareData->pseudoId() == pseudoId)
            return animationRareData.get();
    }
    return nullptr;
}

inline ElementAnimationRareData& ElementRareData::ensureAnimationRareData(PseudoId pseudoId)
{
    if (auto* animationRareData = this->animationRareData(pseudoId))
        return *animationRareData;

    m_animationRareData.append(makeUnique<ElementAnimationRareData>(pseudoId));
    return *m_animationRareData.last().get();
}

inline void ElementRareData::setChildIndex(unsigned index)
{
    m_childIndex = index;
}

inline void ElementRareData::clearShadowRoot()
{
    m_shadowRoot = nullptr;
}

inline void ElementRareData::setShadowRoot(RefPtr<ShadowRoot>&& shadowRoot)
{
    m_shadowRoot = WTFMove(shadowRoot);
}

inline void ElementRareData::setCustomElementReactionQueue(std::unique_ptr<CustomElementReactionQueue>&& queue)
{
    m_customElementReactionQueue = WTFMove(queue);
}

inline void ElementRareData::setCustomElementDefaultARIA(std::unique_ptr<CustomElementDefaultARIA>&& defaultARIA)
{
    m_customElementDefaultARIA = WTFMove(defaultARIA);
}

inline void ElementRareData::setFormAssociatedCustomElement(std::unique_ptr<FormAssociatedCustomElement>&& element)
{
    m_formAssociatedCustomElement = WTFMove(element);
}

inline void ElementRareData::setAttributeMap(std::unique_ptr<NamedNodeMap>&& attributeMap)
{
    m_attributeMap = WTFMove(attributeMap);
}

inline void ElementRareData::setComputedStyle(std::unique_ptr<RenderStyle>&& computedStyle)
{
    m_computedStyle = WTFMove(computedStyle);
}

inline void ElementRareData::setDisplayContentsStyle(std::unique_ptr<RenderStyle> style)
{
    m_displayContentsStyle = WTFMove(style);
}

inline void ElementRareData::setEffectiveLang(const AtomString& lang)
{
    m_effectiveLang = lang;
}

inline void ElementRareData::setClassList(std::unique_ptr<DOMTokenList>&& classList)
{
    m_classList = WTFMove(classList);
}

inline void ElementRareData::setDataset(std::unique_ptr<DatasetDOMStringMap>&& dataset)
{
    m_dataset = WTFMove(dataset);
}

inline void ElementRareData::setSavedLayerScrollPosition(IntPoint position)
{
    m_savedLayerScrollPosition = position;
}

inline void ElementRareData::setPartList(std::unique_ptr<DOMTokenList>&& partList)
{
    m_partList = WTFMove(partList);
}

inline void ElementRareData::setPartNames(SpaceSplitString&& partNames)
{
    m_partNames = WTFMove(partNames);
}

inline void ElementRareData::setIntersectionObserverData(std::unique_ptr<IntersectionObserverData>&& data)
{
    m_intersectionObserverData = WTFMove(data);
}

inline void ElementRareData::setResizeObserverData(std::unique_ptr<ResizeObserverData>&& data)
{
    m_resizeObserverData = WTFMove(data);
}

inline void ElementRareData::setLastRememberedLogicalWidth(LayoutUnit width)
{
    m_lastRememberedLogicalWidth = width;
}

inline void ElementRareData::setLastRememberedLogicalHeight(LayoutUnit height)
{
    m_lastRememberedLogicalHeight = height;
}

inline void ElementRareData::setNonce(const AtomString& value)
{
    m_nonce = value;
}

inline void ElementRareData::setAttributeStyleMap(Ref<StylePropertyMap>&& map)
{
    m_attributeStyleMap = WTFMove(map);
}

inline void ElementRareData::setComputedStyleMap(Ref<StylePropertyMapReadOnly>&& map)
{
    m_computedStyleMap = WTFMove(map);
}

inline void ElementRareData::setPopoverData(std::unique_ptr<PopoverData>&& popoverData)
{
    m_popoverData = WTFMove(popoverData);
}

#if DUMP_NODE_STATISTICS
inline OptionSet<UseType> ElementRareData::useTypes() const
{
    auto result = NodeRareData::useTypes();
    if (!m_savedLayerScrollPosition.isZero())
        result.add(UseType::ScrollingPosition);
    if (m_computedStyle)
        result.add(UseType::ComputedStyle);
    if (m_displayContentsStyle)
        result.add(UseType::DisplayContentsStyle);
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
    return result;
}
#endif

}
