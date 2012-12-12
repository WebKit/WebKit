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
#include "ElementShadow.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "PseudoElement.h"
#include "StyleInheritedData.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class ElementRareData : public NodeRareData {
public:
    ElementRareData();
    virtual ~ElementRareData();

    void setPseudoElement(PseudoId, PassRefPtr<PseudoElement>);
    PseudoElement* pseudoElement(PseudoId) const;

    void resetComputedStyle();
    void resetDynamicRestyleObservations();

    bool needsFocusAppearanceUpdateSoonAfterAttach() const { return m_needsFocusAppearanceUpdateSoonAfterAttach; }
    void setNeedsFocusAppearanceUpdateSoonAfterAttach(bool needs) { m_needsFocusAppearanceUpdateSoonAfterAttach = needs; }

    bool styleAffectedByEmpty() const { return m_styleAffectedByEmpty; }
    void setStyleAffectedByEmpty(bool value) { m_styleAffectedByEmpty = value; }

    bool isInCanvasSubtree() const { return m_isInCanvasSubtree; }
    void setIsInCanvasSubtree(bool value) { m_isInCanvasSubtree = value; }

#if ENABLE(FULLSCREEN_API)
    bool containsFullScreenElement() { return m_containsFullScreenElement; }
    void setContainsFullScreenElement(bool value) { m_containsFullScreenElement = value; }
#endif

#if ENABLE(DIALOG_ELEMENT)
    bool isInTopLayer() const { return m_isInTopLayer; }
    void setIsInTopLayer(bool value) { m_isInTopLayer = value; }
#endif

    bool childrenAffectedByHover() const { return m_childrenAffectedByHover; }
    void setChildrenAffectedByHover(bool value) { m_childrenAffectedByHover = value; }
    bool childrenAffectedByActive() const { return m_childrenAffectedByActive; }
    void setChildrenAffectedByActive(bool value) { m_childrenAffectedByActive = value; }
    bool childrenAffectedByDrag() const { return m_childrenAffectedByDrag; }
    void setChildrenAffectedByDrag(bool value) { m_childrenAffectedByDrag = value; }

    bool childrenAffectedByFirstChildRules() const { return m_childrenAffectedByFirstChildRules; }
    void setChildrenAffectedByFirstChildRules(bool value) { m_childrenAffectedByFirstChildRules = value; }
    bool childrenAffectedByLastChildRules() const { return m_childrenAffectedByLastChildRules; }
    void setChildrenAffectedByLastChildRules(bool value) { m_childrenAffectedByLastChildRules = value; }
    bool childrenAffectedByDirectAdjacentRules() const { return m_childrenAffectedByDirectAdjacentRules; }
    void setChildrenAffectedByDirectAdjacentRules(bool value) { m_childrenAffectedByDirectAdjacentRules = value; }
    bool childrenAffectedByForwardPositionalRules() const { return m_childrenAffectedByForwardPositionalRules; }
    void setChildrenAffectedByForwardPositionalRules(bool value) { m_childrenAffectedByForwardPositionalRules = value; }
    bool childrenAffectedByBackwardPositionalRules() const { return m_childrenAffectedByBackwardPositionalRules; }
    void setChildrenAffectedByBackwardPositionalRules(bool value) { m_childrenAffectedByBackwardPositionalRules = value; }
    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

    ElementShadow* shadow() const { return m_shadow.get(); }
    void setShadow(PassOwnPtr<ElementShadow> shadow) { m_shadow = shadow; }

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    void setAttributeMap(PassOwnPtr<NamedNodeMap> attributeMap) { m_attributeMap = attributeMap; }

    RenderStyle* computedStyle() const { return m_computedStyle.get(); }
    void setComputedStyle(PassRefPtr<RenderStyle> computedStyle) { m_computedStyle = computedStyle; }

    ClassList* classList() const { return m_classList.get(); }
    void setClassList(PassOwnPtr<ClassList> classList) { m_classList = classList; }

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    void setDataset(PassOwnPtr<DatasetDOMStringMap> dataset) { m_dataset = dataset; }

    LayoutSize minimumSizeForResizing() const { return m_minimumSizeForResizing; }
    void setMinimumSizeForResizing(LayoutSize size) { m_minimumSizeForResizing = size; }

    IntSize savedLayerScrollOffset() const { return m_savedLayerScrollOffset; }
    void setSavedLayerScrollOffset(IntSize size) { m_savedLayerScrollOffset = size; }

private:
    // Many fields are in NodeRareData for better packing.
    LayoutSize m_minimumSizeForResizing;
    RefPtr<RenderStyle> m_computedStyle;

    OwnPtr<DatasetDOMStringMap> m_dataset;
    OwnPtr<ClassList> m_classList;
    OwnPtr<ElementShadow> m_shadow;
    OwnPtr<NamedNodeMap> m_attributeMap;

    RefPtr<PseudoElement> m_generatedBefore;
    RefPtr<PseudoElement> m_generatedAfter;

    IntSize m_savedLayerScrollOffset;

private:
    void releasePseudoElement(PseudoElement*);
};

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(LayoutUnit::max(), LayoutUnit::max());
}

inline ElementRareData::ElementRareData()
    : m_minimumSizeForResizing(defaultMinimumSizeForResizing())
    , m_generatedBefore(0)
    , m_generatedAfter(0)
{
}

inline ElementRareData::~ElementRareData()
{
    ASSERT(!m_shadow);
    ASSERT(!m_generatedBefore);
    ASSERT(!m_generatedAfter);
}

inline void ElementRareData::setPseudoElement(PseudoId pseudoId, PassRefPtr<PseudoElement> element)
{
    switch (pseudoId) {
    case BEFORE:
        releasePseudoElement(m_generatedBefore.get());
        m_generatedBefore = element;
        break;
    case AFTER:
        releasePseudoElement(m_generatedAfter.get());
        m_generatedAfter = element;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

inline PseudoElement* ElementRareData::pseudoElement(PseudoId pseudoId) const
{
    switch (pseudoId) {
    case BEFORE:
        return m_generatedBefore.get();
    case AFTER:
        return m_generatedAfter.get();
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

inline void ElementRareData::releasePseudoElement(PseudoElement* element)
{
    if (!element)
        return;

    if (element->attached())
        element->detach();

    ASSERT(!element->nextSibling());
    ASSERT(!element->previousSibling());

    element->setParentOrHostNode(0);
}

inline void ElementRareData::resetComputedStyle()
{
    setComputedStyle(0);
    setStyleAffectedByEmpty(false);
    setChildIndex(0);
}

inline void ElementRareData::resetDynamicRestyleObservations()
{
    setChildrenAffectedByHover(false);
    setChildrenAffectedByActive(false);
    setChildrenAffectedByDrag(false);
    setChildrenAffectedByFirstChildRules(false);
    setChildrenAffectedByLastChildRules(false);
    setChildrenAffectedByDirectAdjacentRules(false);
    setChildrenAffectedByForwardPositionalRules(false);
    setChildrenAffectedByBackwardPositionalRules(false);
}

} // namespace

#endif // ElementRareData_h
