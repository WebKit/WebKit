/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HTMLCollection.h"

#include "CachedHTMLCollectionInlines.h"
#include "HTMLNames.h"
#include "NodeRareDataInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLCollection);

inline auto HTMLCollection::rootTypeFromCollectionType(CollectionType type) -> RootType
{
    switch (type) {
    case CollectionType::DocImages:
    case CollectionType::DocEmpty:
    case CollectionType::DocEmbeds:
    case CollectionType::DocForms:
    case CollectionType::DocLinks:
    case CollectionType::DocAnchors:
    case CollectionType::DocScripts:
    case CollectionType::DocAll:
    case CollectionType::WindowNamedItems:
    case CollectionType::DocumentNamedItems:
    case CollectionType::DocumentAllNamedItems:
    case CollectionType::FormControls:
        return HTMLCollection::IsRootedAtTreeScope;
    case CollectionType::AllDescendants:
    case CollectionType::ByClass:
    case CollectionType::ByTag:
    case CollectionType::ByHTMLTag:
    case CollectionType::FieldSetElements:
    case CollectionType::NodeChildren:
    case CollectionType::TableTBodies:
    case CollectionType::TSectionRows:
    case CollectionType::TableRows:
    case CollectionType::TRCells:
    case CollectionType::SelectOptions:
    case CollectionType::SelectedOptions:
    case CollectionType::DataListOptions:
    case CollectionType::MapAreas:
        return HTMLCollection::IsRootedAtNode;
    }
    ASSERT_NOT_REACHED();
    return HTMLCollection::IsRootedAtNode;
}

static NodeListInvalidationType invalidationTypeExcludingIdAndNameAttributes(CollectionType type)
{
    switch (type) {
    case CollectionType::ByTag:
    case CollectionType::ByHTMLTag:
    case CollectionType::AllDescendants:
    case CollectionType::DocImages:
    case CollectionType::DocEmbeds:
    case CollectionType::DocForms:
    case CollectionType::DocScripts:
    case CollectionType::DocAll:
    case CollectionType::NodeChildren:
    case CollectionType::TableTBodies:
    case CollectionType::TSectionRows:
    case CollectionType::TableRows:
    case CollectionType::TRCells:
    case CollectionType::SelectOptions:
    case CollectionType::MapAreas:
    case CollectionType::DocEmpty:
        return NodeListInvalidationType::DoNotInvalidateOnAttributeChanges;
    case CollectionType::SelectedOptions:
    case CollectionType::DataListOptions:
        // FIXME: We can do better some day.
        return NodeListInvalidationType::InvalidateOnAnyAttrChange;
    case CollectionType::ByClass:
        return NodeListInvalidationType::InvalidateOnClassAttrChange;
    case CollectionType::DocAnchors:
        return NodeListInvalidationType::InvalidateOnNameAttrChange;
    case CollectionType::DocLinks:
        return NodeListInvalidationType::InvalidateOnHRefAttrChange;
    case CollectionType::WindowNamedItems:
    case CollectionType::DocumentNamedItems:
    case CollectionType::DocumentAllNamedItems:
        return NodeListInvalidationType::InvalidateOnIdNameAttrChange;
    case CollectionType::FieldSetElements:
    case CollectionType::FormControls:
        return NodeListInvalidationType::InvalidateForFormControls;
    }
    ASSERT_NOT_REACHED();
    return NodeListInvalidationType::DoNotInvalidateOnAttributeChanges;
}

HTMLCollection::HTMLCollection(ContainerNode& ownerNode, CollectionType type)
    : m_collectionType(static_cast<unsigned>(type))
    , m_invalidationType(static_cast<unsigned>(invalidationTypeExcludingIdAndNameAttributes(type)))
    , m_rootType(rootTypeFromCollectionType(type))
    , m_ownerNode(ownerNode)
{
    ASSERT(m_rootType == static_cast<unsigned>(rootTypeFromCollectionType(type)));
    ASSERT(m_invalidationType == static_cast<unsigned>(invalidationTypeExcludingIdAndNameAttributes(type)));
    ASSERT(m_collectionType == static_cast<unsigned>(type));
}

HTMLCollection::~HTMLCollection()
{
    if (hasNamedElementCache())
        document().collectionWillClearIdNameMap(*this);

    // HTMLNameCollection & ClassCollection remove cache by themselves.
    // FIXME: We need a cleaner way to handle this.
    switch (type()) {
    case CollectionType::ByClass:
    case CollectionType::ByTag:
    case CollectionType::ByHTMLTag:
    case CollectionType::WindowNamedItems:
    case CollectionType::DocumentNamedItems:
    case CollectionType::DocumentAllNamedItems:
        break;
    default:
        ownerNode().nodeLists()->removeCachedCollection(this);
    }
}

void HTMLCollection::invalidateCacheForDocument(Document& document)
{
    if (hasNamedElementCache())
        invalidateNamedElementCache(document);
}

void HTMLCollection::invalidateNamedElementCache(Document& document) const
{
    ASSERT(hasNamedElementCache());
    document.collectionWillClearIdNameMap(*this);
    {
        Locker locker { m_namedElementCacheAssignmentLock };
        m_namedElementCache = nullptr;
    }
}

Element* HTMLCollection::namedItemSlow(const AtomString& name) const
{
    // The pathological case. We need to walk the entire subtree.
    updateNamedElementCache();
    ASSERT(m_namedElementCache);

    if (auto* idResults = m_namedElementCache->findElementsWithId(name)) {
        if (idResults->size())
            return idResults->at(0).ptr();
    }

    if (auto* nameResults = m_namedElementCache->findElementsWithName(name)) {
        if (nameResults->size())
            return nameResults->at(0).ptr();
    }

    return nullptr;
}

// Documented in https://dom.spec.whatwg.org/#interface-htmlcollection.
const Vector<AtomString>& HTMLCollection::supportedPropertyNames()
{
    updateNamedElementCache();
    ASSERT(m_namedElementCache);

    return m_namedElementCache->propertyNames();
}

bool HTMLCollection::isSupportedPropertyName(const AtomString& name)
{
    updateNamedElementCache();
    ASSERT(m_namedElementCache);

    if (m_namedElementCache->findElementsWithId(name))
        return true;
    if (m_namedElementCache->findElementsWithName(name))
        return true;

    return false;
}

void HTMLCollection::updateNamedElementCache() const
{
    if (hasNamedElementCache())
        return;

    auto cache = makeUnique<CollectionNamedElementCache>();

    unsigned size = length();
    for (unsigned i = 0; i < size; ++i) {
        Element& element = *item(i);
        const AtomString& id = element.getIdAttribute();
        if (!id.isEmpty())
            cache->appendToIdCache(id, element);
        auto* htmlElement = dynamicDowncast<HTMLElement>(element);
        if (!htmlElement)
            continue;
        const AtomString& name = element.getNameAttribute();
        if (!name.isEmpty() && id != name && (type() != CollectionType::DocAll || nameShouldBeVisibleInDocumentAll(*htmlElement)))
            cache->appendToNameCache(name, element);
    }

    setNamedItemCache(WTFMove(cache));
}

Vector<Ref<Element>> HTMLCollection::namedItems(const AtomString& name) const
{
    // FIXME: This non-virtual function can't possibly be doing the correct thing for
    // any derived class that overrides the virtual namedItem function.

    Vector<Ref<Element>> elements;

    if (name.isEmpty())
        return elements;

    updateNamedElementCache();
    ASSERT(m_namedElementCache);

    auto* elementsWithId = m_namedElementCache->findElementsWithId(name);
    auto* elementsWithName = m_namedElementCache->findElementsWithName(name);

    elements.reserveInitialCapacity((elementsWithId ? elementsWithId->size() : 0) + (elementsWithName ? elementsWithName->size() : 0));

    if (elementsWithId) {
        elements.appendContainerWithMapping(*elementsWithId, [](auto& element) {
            return Ref { element.get() };
        });
    }
    if (elementsWithName) {
        elements.appendContainerWithMapping(*elementsWithName, [](auto& element) {
            return Ref { element.get() };
        });
    }

    return elements;
}

size_t HTMLCollection::memoryCost() const
{
    // memoryCost() may be invoked concurrently from a GC thread, and we need to be careful about what data we access here and how.
    // Hence, we need to guard m_namedElementCache from being replaced while accessing it.
    Locker locker { m_namedElementCacheAssignmentLock };
    return m_namedElementCache ? m_namedElementCache->memoryCost() : 0;
}

} // namespace WebCore
