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
#include "StyleInheritedData.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class ElementRareData : public NodeRareData {
public:
    ElementRareData();
    virtual ~ElementRareData();

    void resetComputedStyle();
    void resetDynamicRestyleObservations();

    using NodeRareData::needsFocusAppearanceUpdateSoonAfterAttach;
    using NodeRareData::setNeedsFocusAppearanceUpdateSoonAfterAttach;
    using NodeRareData::styleAffectedByEmpty;
    using NodeRareData::setStyleAffectedByEmpty;
    using NodeRareData::isInCanvasSubtree;
    using NodeRareData::setIsInCanvasSubtree;
#if ENABLE(FULLSCREEN_API)
    using NodeRareData::containsFullScreenElement;
    using NodeRareData::setContainsFullScreenElement;
#endif
#if ENABLE(DIALOG_ELEMENT)
    using NodeRareData::isInTopLayer;
    using NodeRareData::setIsInTopLayer;
#endif
    using NodeRareData::childrenAffectedByHover;
    using NodeRareData::setChildrenAffectedByHover;
    using NodeRareData::childrenAffectedByActive;
    using NodeRareData::setChildrenAffectedByActive;
    using NodeRareData::childrenAffectedByDrag;
    using NodeRareData::setChildrenAffectedByDrag;
    using NodeRareData::childrenAffectedByFirstChildRules;
    using NodeRareData::setChildrenAffectedByFirstChildRules;
    using NodeRareData::childrenAffectedByLastChildRules;
    using NodeRareData::setChildrenAffectedByLastChildRules;
    using NodeRareData::childrenAffectedByDirectAdjacentRules;
    using NodeRareData::setChildrenAffectedByDirectAdjacentRules;
    using NodeRareData::childrenAffectedByForwardPositionalRules;
    using NodeRareData::setChildrenAffectedByForwardPositionalRules;
    using NodeRareData::childrenAffectedByBackwardPositionalRules;
    using NodeRareData::setChildrenAffectedByBackwardPositionalRules;
    using NodeRareData::childIndex;
    using NodeRareData::setChildIndex;

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

    LayoutSize m_minimumSizeForResizing;
    RefPtr<RenderStyle> m_computedStyle;

    OwnPtr<DatasetDOMStringMap> m_datasetDOMStringMap;
    OwnPtr<ClassList> m_classList;
    OwnPtr<ElementShadow> m_shadow;
    OwnPtr<NamedNodeMap> m_attributeMap;

    IntSize m_savedLayerScrollOffset;
};

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(LayoutUnit::max(), LayoutUnit::max());
}

inline ElementRareData::ElementRareData()
    : m_minimumSizeForResizing(defaultMinimumSizeForResizing())
{
}

inline ElementRareData::~ElementRareData()
{
    ASSERT(!m_shadow);
}

inline void ElementRareData::resetComputedStyle()
{
    m_computedStyle.clear();
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
