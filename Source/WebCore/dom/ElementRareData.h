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

#ifndef ElementRareData_h
#define ElementRareData_h

#include "DOMTokenList.h"
#include "DatasetDOMStringMap.h"
#include "JSCustomElementInterface.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "PseudoElement.h"
#include "RenderElement.h"
#include "ShadowRoot.h"
#include "StyleInheritedData.h"

namespace WebCore {

class ElementRareData : public NodeRareData {
public:
    explicit ElementRareData(RenderElement*);
    ~ElementRareData();

    void setBeforePseudoElement(RefPtr<PseudoElement>&&);
    void setAfterPseudoElement(RefPtr<PseudoElement>&&);

    PseudoElement* beforePseudoElement() const { return m_beforePseudoElement.get(); }
    PseudoElement* afterPseudoElement() const { return m_afterPseudoElement.get(); }

    void resetComputedStyle();
    void resetDynamicRestyleObservations();
    
    int tabIndex() const { return m_tabIndex; }
    void setTabIndexExplicitly(int index) { m_tabIndex = index; m_tabIndexWasSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexWasSetExplicitly; }
    void clearTabIndexExplicitly() { m_tabIndex = 0; m_tabIndexWasSetExplicitly = false; }

    bool needsFocusAppearanceUpdateSoonAfterAttach() const { return m_needsFocusAppearanceUpdateSoonAfterAttach; }
    void setNeedsFocusAppearanceUpdateSoonAfterAttach(bool needs) { m_needsFocusAppearanceUpdateSoonAfterAttach = needs; }

    bool styleAffectedByActive() const { return m_styleAffectedByActive; }
    void setStyleAffectedByActive(bool value) { m_styleAffectedByActive = value; }

    bool styleAffectedByEmpty() const { return m_styleAffectedByEmpty; }
    void setStyleAffectedByEmpty(bool value) { m_styleAffectedByEmpty = value; }

    bool styleAffectedByFocusWithin() const { return m_styleAffectedByFocusWithin; }
    void setStyleAffectedByFocusWithin(bool value) { m_styleAffectedByFocusWithin = value; }

    RegionOversetState regionOversetState() const { return m_regionOversetState; }
    void setRegionOversetState(RegionOversetState state) { m_regionOversetState = state; }

    bool isNamedFlowContentElement() const { return m_isNamedFlowContentElement; }
    void setIsNamedFlowContentElement(bool value) { m_isNamedFlowContentElement = value; }

#if ENABLE(FULLSCREEN_API)
    bool containsFullScreenElement() { return m_containsFullScreenElement; }
    void setContainsFullScreenElement(bool value) { m_containsFullScreenElement = value; }
#endif

    bool childrenAffectedByDrag() const { return m_childrenAffectedByDrag; }
    void setChildrenAffectedByDrag(bool value) { m_childrenAffectedByDrag = value; }

    bool childrenAffectedByLastChildRules() const { return m_childrenAffectedByLastChildRules; }
    void setChildrenAffectedByLastChildRules(bool value) { m_childrenAffectedByLastChildRules = value; }
    bool childrenAffectedByBackwardPositionalRules() const { return m_childrenAffectedByBackwardPositionalRules; }
    void setChildrenAffectedByBackwardPositionalRules(bool value) { m_childrenAffectedByBackwardPositionalRules = value; }
    bool childrenAffectedByPropertyBasedBackwardPositionalRules() const { return m_childrenAffectedByPropertyBasedBackwardPositionalRules; }
    void setChildrenAffectedByPropertyBasedBackwardPositionalRules(bool value) { m_childrenAffectedByPropertyBasedBackwardPositionalRules = value; }

    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }
    static ptrdiff_t childIndexMemoryOffset() { return OBJECT_OFFSETOF(ElementRareData, m_childIndex); }

    void clearShadowRoot() { m_shadowRoot = nullptr; }
    ShadowRoot* shadowRoot() const { return m_shadowRoot.get(); }
    void setShadowRoot(RefPtr<ShadowRoot>&& shadowRoot) { m_shadowRoot = WTFMove(shadowRoot); }

#if ENABLE(CUSTOM_ELEMENTS)
    JSCustomElementInterface* customElementInterface() { return m_customElementInterface.get(); }
    void setCustomElementInterface(JSCustomElementInterface& customElementInterface) { m_customElementInterface = &customElementInterface; }
#endif

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

    bool hasDisplayContents() const { return m_hasDisplayContents; }
    void setHasDisplayContents(bool value) { m_hasDisplayContents = value; }

private:
    int m_tabIndex;
    unsigned short m_childIndex;
    unsigned m_tabIndexWasSetExplicitly : 1;
    unsigned m_needsFocusAppearanceUpdateSoonAfterAttach : 1;
    unsigned m_styleAffectedByActive : 1;
    unsigned m_styleAffectedByEmpty : 1;
    unsigned m_styleAffectedByFocusWithin : 1;
#if ENABLE(FULLSCREEN_API)
    unsigned m_containsFullScreenElement : 1;
#endif
    unsigned m_hasPendingResources : 1;
    unsigned m_childrenAffectedByHover : 1;
    unsigned m_childrenAffectedByDrag : 1;
    // Bits for dynamic child matching.
    // We optimize for :first-child and :last-child. The other positional child selectors like nth-child or
    // *-child-of-type, we will just give up and re-evaluate whenever children change at all.
    unsigned m_childrenAffectedByLastChildRules : 1;
    unsigned m_childrenAffectedByBackwardPositionalRules : 1;
    unsigned m_childrenAffectedByPropertyBasedBackwardPositionalRules : 1;
    unsigned m_hasDisplayContents : 1;
    unsigned m_isNamedFlowContentElement : 1;

    RegionOversetState m_regionOversetState;

    LayoutSize m_minimumSizeForResizing;
    IntPoint m_savedLayerScrollPosition;
    std::unique_ptr<RenderStyle> m_computedStyle;

    std::unique_ptr<DatasetDOMStringMap> m_dataset;
    std::unique_ptr<DOMTokenList> m_classList;
    RefPtr<ShadowRoot> m_shadowRoot;
#if ENABLE(CUSTOM_ELEMENTS)
    RefPtr<JSCustomElementInterface> m_customElementInterface;
#endif
    std::unique_ptr<NamedNodeMap> m_attributeMap;

    RefPtr<PseudoElement> m_beforePseudoElement;
    RefPtr<PseudoElement> m_afterPseudoElement;

    void releasePseudoElement(PseudoElement*);
};

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(LayoutUnit::max(), LayoutUnit::max());
}

inline ElementRareData::ElementRareData(RenderElement* renderer)
    : NodeRareData(renderer)
    , m_tabIndex(0)
    , m_childIndex(0)
    , m_tabIndexWasSetExplicitly(false)
    , m_needsFocusAppearanceUpdateSoonAfterAttach(false)
    , m_styleAffectedByActive(false)
    , m_styleAffectedByEmpty(false)
    , m_styleAffectedByFocusWithin(false)
#if ENABLE(FULLSCREEN_API)
    , m_containsFullScreenElement(false)
#endif
    , m_hasPendingResources(false)
    , m_childrenAffectedByHover(false)
    , m_childrenAffectedByDrag(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_childrenAffectedByPropertyBasedBackwardPositionalRules(false)
    , m_hasDisplayContents(false)
    , m_isNamedFlowContentElement(false)
    , m_regionOversetState(RegionUndefined)
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
    m_hasDisplayContents = false;
    setStyleAffectedByEmpty(false);
    setStyleAffectedByFocusWithin(false);
    setChildIndex(0);
}

inline void ElementRareData::resetDynamicRestyleObservations()
{
    setStyleAffectedByActive(false);
    setChildrenAffectedByDrag(false);
    setChildrenAffectedByLastChildRules(false);
    setChildrenAffectedByBackwardPositionalRules(false);
    setChildrenAffectedByPropertyBasedBackwardPositionalRules(false);
}

} // namespace

#endif // ElementRareData_h
