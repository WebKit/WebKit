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
#include "Element.h"
#include "HTMLCollection.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "ShadowTree.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class ElementRareData : public NodeRareData {
public:
    ElementRareData();
    virtual ~ElementRareData();

    void resetComputedStyle();

    using NodeRareData::needsFocusAppearanceUpdateSoonAfterAttach;
    using NodeRareData::setNeedsFocusAppearanceUpdateSoonAfterAttach;

    typedef FixedArray<OwnPtr<HTMLCollection>, NumNodeCollectionTypes> CachedHTMLCollectionArray;

    bool hasCachedHTMLCollections() const
    {
        return m_cachedCollections;
    }

    HTMLCollection* ensureCachedHTMLCollection(Element* element, CollectionType type)
    {
        if (!m_cachedCollections)
            m_cachedCollections = adoptPtr(new CachedHTMLCollectionArray);

        OwnPtr<HTMLCollection>& collection = (*m_cachedCollections)[type - FirstNodeCollectionType];
        if (!collection)
            collection = HTMLCollection::create(element, type);
        return collection.get();
    }

    OwnPtr<CachedHTMLCollectionArray> m_cachedCollections;

    LayoutSize m_minimumSizeForResizing;
    RefPtr<RenderStyle> m_computedStyle;
    AtomicString m_shadowPseudoId;

    OwnPtr<DatasetDOMStringMap> m_datasetDOMStringMap;
    OwnPtr<ClassList> m_classList;
    OwnPtr<ShadowTree> m_shadowTree;
    OwnPtr<NamedNodeMap> m_attributeMap;

    bool m_styleAffectedByEmpty;

    IntSize m_savedLayerScrollOffset;

#if ENABLE(FULLSCREEN_API)
    bool m_containsFullScreenElement;
#endif
};

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(MAX_LAYOUT_UNIT, MAX_LAYOUT_UNIT);
}

inline ElementRareData::ElementRareData()
    : NodeRareData()
    , m_minimumSizeForResizing(defaultMinimumSizeForResizing())
    , m_styleAffectedByEmpty(false)
#if ENABLE(FULLSCREEN_API)
    , m_containsFullScreenElement(false)
#endif
{
}

inline ElementRareData::~ElementRareData()
{
    ASSERT(!m_shadowTree);
}

inline void ElementRareData::resetComputedStyle()
{
    m_computedStyle.clear();
}

}
#endif // ElementRareData_h
