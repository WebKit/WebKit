/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "ClassList.h"
#include "DatasetDOMStringMap.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "PseudoElement.h"
#include "RenderElement.h"
#include "ShadowRoot.h"
#include "StyleInheritedData.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class ElementRareData : public NodeRareData {
public:
    static PassOwnPtr<ElementRareData> create(RenderElement* renderer) { return adoptPtr(new ElementRareData(renderer)); }

    ~ElementRareData();

    void setBeforePseudoElement(PassRefPtr<PseudoElement>);
    void setAfterPseudoElement(PassRefPtr<PseudoElement>);

    PseudoElement* beforePseudoElement() const { return m_beforePseudoElement.get(); }
    PseudoElement* afterPseudoElement() const { return m_afterPseudoElement.get(); }

    void resetComputedStyle();
    void resetDynamicRestyleObservations();
    
    short tabIndex() const { return m_tabIndex; }
    void setTabIndexExplicitly(short index) { m_tabIndex = index; m_tabIndexWasSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexWasSetExplicitly; }
    void clearTabIndexExplicitly() { m_tabIndex = 0; m_tabIndexWasSetExplicitly = false; }

    bool needsFocusAppearanceUpdateSoonAfterAttach() const { return m_needsFocusAppearanceUpdateSoonAfterAttach; }
    void setNeedsFocusAppearanceUpdateSoonAfterAttach(bool needs) { m_needsFocusAppearanceUpdateSoonAfterAttach = needs; }

    bool styleAffectedByEmpty() const { return m_styleAffectedByEmpty; }
    void setStyleAffectedByEmpty(bool value) { m_styleAffectedByEmpty = value; }

    bool isInCanvasSubtree() const { return m_isInCanvasSubtree; }
    void setIsInCanvasSubtree(bool value) { m_isInCanvasSubtree = value; }

    RegionOversetState regionOversetState() const { return m_regionOversetState; }
    void setRegionOversetState(RegionOversetState state) { m_regionOversetState = state; }

#if ENABLE(FULLSCREEN_API)
    bool containsFullScreenElement() { return m_containsFullScreenElement; }
    void setContainsFullScreenElement(bool value) { m_containsFullScreenElement = value; }
#endif

    bool childrenAffectedByActive() const { return m_childrenAffectedByActive; }
    void setChildrenAffectedByActive(bool value) { m_childrenAffectedByActive = value; }
    bool childrenAffectedByDrag() const { return m_childrenAffectedByDrag; }
    void setChildrenAffectedByDrag(bool value) { m_childrenAffectedByDrag = value; }

    bool childrenAffectedByLastChildRules() const { return m_childrenAffectedByLastChildRules; }
    void setChildrenAffectedByLastChildRules(bool value) { m_childrenAffectedByLastChildRules = value; }
    bool childrenAffectedByForwardPositionalRules() const { return m_childrenAffectedByForwardPositionalRules; }
    void setChildrenAffectedByForwardPositionalRules(bool value) { m_childrenAffectedByForwardPositionalRules = value; }
    bool childrenAffectedByBackwardPositionalRules() const { return m_childrenAffectedByBackwardPositionalRules; }
    void setChildrenAffectedByBackwardPositionalRules(bool value) { m_childrenAffectedByBackwardPositionalRules = value; }

    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }
    static ptrdiff_t childIndexMemoryOffset() { return OBJECT_OFFSETOF(ElementRareData, m_childIndex); }

    void clearShadowRoot() { m_shadowRoot = nullptr; }
    ShadowRoot* shadowRoot() const { return m_shadowRoot.get(); }
    void setShadowRoot(PassRefPtr<ShadowRoot> shadowRoot) { m_shadowRoot = shadowRoot; }

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    void setAttributeMap(PassOwnPtr<NamedNodeMap> attributeMap) { m_attributeMap = attributeMap; }

    RenderStyle* computedStyle() const { return m_computedStyle.get(); }
    void setComputedStyle(PassRef<RenderStyle> computedStyle) { m_computedStyle = WTF::move(computedStyle); }

    ClassList* classList() const { return m_classList.get(); }
    void setClassList(std::unique_ptr<ClassList> classList) { m_classList = WTF::move(classList); }
    void clearClassListValueForQuirksMode()
    {
        if (!m_classList)
            return;
        m_classList->clearValueForQuirksMode();
    }

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    void setDataset(std::unique_ptr<DatasetDOMStringMap> dataset) { m_dataset = WTF::move(dataset); }

    LayoutSize minimumSizeForResizing() const { return m_minimumSizeForResizing; }
    void setMinimumSizeForResizing(LayoutSize size) { m_minimumSizeForResizing = size; }

    IntSize savedLayerScrollOffset() const { return m_savedLayerScrollOffset; }
    void setSavedLayerScrollOffset(IntSize size) { m_savedLayerScrollOffset = size; }

    bool hasPendingResources() const { return m_hasPendingResources; }
    void setHasPendingResources(bool has) { m_hasPendingResources = has; }

private:
    short m_tabIndex;
    unsigned short m_childIndex;
    unsigned m_tabIndexWasSetExplicitly : 1;
    unsigned m_needsFocusAppearanceUpdateSoonAfterAttach : 1;
    unsigned m_styleAffectedByEmpty : 1;
    unsigned m_isInCanvasSubtree : 1;
#if ENABLE(FULLSCREEN_API)
    unsigned m_containsFullScreenElement : 1;
#endif
    unsigned m_hasPendingResources : 1;
    unsigned m_childrenAffectedByHover : 1;
    unsigned m_childrenAffectedByActive : 1;
    unsigned m_childrenAffectedByDrag : 1;
    // Bits for dynamic child matching.
    // We optimize for :first-child and :last-child. The other positional child selectors like nth-child or
    // *-child-of-type, we will just give up and re-evaluate whenever children change at all.
    unsigned m_childrenAffectedByLastChildRules : 1;
    unsigned m_childrenAffectedByForwardPositionalRules : 1;
    unsigned m_childrenAffectedByBackwardPositionalRules : 1;

    RegionOversetState m_regionOversetState;

    LayoutSize m_minimumSizeForResizing;
    IntSize m_savedLayerScrollOffset;
    RefPtr<RenderStyle> m_computedStyle;

    std::unique_ptr<DatasetDOMStringMap> m_dataset;
    std::unique_ptr<ClassList> m_classList;
    RefPtr<ShadowRoot> m_shadowRoot;
    OwnPtr<NamedNodeMap> m_attributeMap;

    RefPtr<PseudoElement> m_beforePseudoElement;
    RefPtr<PseudoElement> m_afterPseudoElement;

    explicit ElementRareData(RenderElement*);
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
    , m_styleAffectedByEmpty(false)
    , m_isInCanvasSubtree(false)
#if ENABLE(FULLSCREEN_API)
    , m_containsFullScreenElement(false)
#endif
    , m_hasPendingResources(false)
    , m_childrenAffectedByHover(false)
    , m_childrenAffectedByActive(false)
    , m_childrenAffectedByDrag(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
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

inline void ElementRareData::setBeforePseudoElement(PassRefPtr<PseudoElement> pseudoElement)
{
    ASSERT(!m_beforePseudoElement || !pseudoElement);
    m_beforePseudoElement = pseudoElement;
}

inline void ElementRareData::setAfterPseudoElement(PassRefPtr<PseudoElement> pseudoElement)
{
    ASSERT(!m_afterPseudoElement || !pseudoElement);
    m_afterPseudoElement = pseudoElement;
}

inline void ElementRareData::resetComputedStyle()
{
    m_computedStyle = nullptr;
    setStyleAffectedByEmpty(false);
    setChildIndex(0);
}

inline void ElementRareData::resetDynamicRestyleObservations()
{
    setChildrenAffectedByActive(false);
    setChildrenAffectedByDrag(false);
    setChildrenAffectedByLastChildRules(false);
    setChildrenAffectedByForwardPositionalRules(false);
    setChildrenAffectedByBackwardPositionalRules(false);
}

} // namespace

#endif // ElementRareData_h
