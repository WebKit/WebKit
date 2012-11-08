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
#include "ElementShadow.h"
#include "HTMLCollection.h"
#include "NamedNodeMap.h"
#include "NodeRareData.h"
#include "StyleInheritedData.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLCollection;

class ElementRareData : public NodeRareData {
public:
    ElementRareData();
    virtual ~ElementRareData();

    void resetComputedStyle();

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

    bool hasCachedHTMLCollections() const
    {
        return m_cachedCollections;
    }

    PassRefPtr<HTMLCollection> ensureCachedHTMLCollection(Element*, CollectionType);
    HTMLCollection* cachedHTMLCollection(CollectionType type)
    {
        if (!m_cachedCollections)
            return 0;

        return (*m_cachedCollections)[type - FirstNodeCollectionType];
    }

    void removeCachedHTMLCollection(HTMLCollection* collection, CollectionType type)
    {
        ASSERT(m_cachedCollections);
        ASSERT_UNUSED(collection, (*m_cachedCollections)[type - FirstNodeCollectionType] == collection);
        (*m_cachedCollections)[type - FirstNodeCollectionType] = 0;
    }

    void clearHTMLCollectionCaches(const QualifiedName* attrName)
    {
        if (!m_cachedCollections)
            return;

        bool shouldIgnoreType = !attrName || *attrName == HTMLNames::idAttr || *attrName == HTMLNames::nameAttr;

        for (unsigned i = 0; i < (*m_cachedCollections).size(); i++) {
            if (HTMLCollection* collection = (*m_cachedCollections)[i]) {
                if (shouldIgnoreType || DynamicNodeListCacheBase::shouldInvalidateTypeOnAttributeChange(collection->invalidationType(), *attrName))
                    collection->invalidateCache();
            }
        }
    }

    void adoptTreeScope(Document* oldDocument, Document* newDocument)
    {
        if (!m_cachedCollections)
            return;

        for (unsigned i = 0; i < (*m_cachedCollections).size(); i++) {
            HTMLCollection* collection = (*m_cachedCollections)[i];
            if (!collection)
                continue;
            collection->invalidateCache();
            if (oldDocument != newDocument) {
                oldDocument->unregisterNodeListCache(collection);
                newDocument->registerNodeListCache(collection);
            }
        }
    }

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

    typedef FixedArray<HTMLCollection*, NumNodeCollectionTypes> CachedHTMLCollectionArray;
    OwnPtr<CachedHTMLCollectionArray> m_cachedCollections;

    LayoutSize m_minimumSizeForResizing;
    RefPtr<RenderStyle> m_computedStyle;
    AtomicString m_shadowPseudoId;

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
}

}
#endif // ElementRareData_h
