/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "HTMLFormControlsCollection.h"

#include "FormAssociatedElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "ScriptDisallowedScope.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

// Since the collections are to be "live", we have to do the
// calculation every time if anything has changed.

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLFormControlsCollection);

HTMLFormControlsCollection::HTMLFormControlsCollection(ContainerNode& ownerNode)
    : CachedHTMLCollection(ownerNode, FormControls)
    , m_cachedElement(nullptr)
    , m_cachedElementOffsetInArray(0)
{
    ASSERT(is<HTMLFormElement>(ownerNode));
}

Ref<HTMLFormControlsCollection> HTMLFormControlsCollection::create(ContainerNode& ownerNode, CollectionType)
{
    return adoptRef(*new HTMLFormControlsCollection(ownerNode));
}

HTMLFormControlsCollection::~HTMLFormControlsCollection() = default;

Optional<Variant<RefPtr<RadioNodeList>, RefPtr<Element>>> HTMLFormControlsCollection::namedItemOrItems(const String& name) const
{
    auto namedItems = this->namedItems(name);

    if (namedItems.isEmpty())
        return WTF::nullopt;
    if (namedItems.size() == 1)
        return Variant<RefPtr<RadioNodeList>, RefPtr<Element>> { RefPtr<Element> { WTFMove(namedItems[0]) } };

    return Variant<RefPtr<RadioNodeList>, RefPtr<Element>> { RefPtr<RadioNodeList> { ownerNode().radioNodeList(name) } };
}

static unsigned findFormAssociatedElement(const Vector<WeakPtr<HTMLElement>>& elements, const Element& element)
{
    for (unsigned i = 0; i < elements.size(); ++i) {
        auto currentElement = makeRefPtr(elements[i].get());
        ASSERT(currentElement);
        auto* associatedElement = currentElement->asFormAssociatedElement();
        ASSERT(associatedElement);
        if (associatedElement->isEnumeratable() && currentElement == &element)
            return i;
    }
    return elements.size();
}

HTMLElement* HTMLFormControlsCollection::customElementAfter(Element* current) const
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    auto& elements = ownerNode().unsafeAssociatedElements();
    unsigned start;
    if (!current)
        start = 0;
    else if (m_cachedElement == current)
        start = m_cachedElementOffsetInArray + 1;
    else
        start = findFormAssociatedElement(elements, *current) + 1;

    for (unsigned i = start; i < elements.size(); ++i) {
        auto element = makeRefPtr(elements[i].get());
        ASSERT(element);
        ASSERT(element->asFormAssociatedElement());
        if (element->asFormAssociatedElement()->isEnumeratable()) {
            m_cachedElement = element.get();
            m_cachedElementOffsetInArray = i;
            return element.get();
        }
    }
    return nullptr;
}

HTMLFormElement& HTMLFormControlsCollection::ownerNode() const
{
    return downcast<HTMLFormElement>(CachedHTMLCollection::ownerNode());
}

void HTMLFormControlsCollection::updateNamedElementCache() const
{
    if (hasNamedElementCache())
        return;

    auto cache = makeUnique<CollectionNamedElementCache>();

    HashSet<AtomStringImpl*> foundInputElements;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    for (auto& weakElement : ownerNode().unsafeAssociatedElements()) {
        auto element = makeRefPtr(weakElement.get());
        ASSERT(element);
        auto* associatedElement = element->asFormAssociatedElement();
        ASSERT(associatedElement);
        if (associatedElement->isEnumeratable()) {
            const AtomString& id = element->getIdAttribute();
            if (!id.isEmpty()) {
                cache->appendToIdCache(id, *element);
                foundInputElements.add(id.impl());
            }
            const AtomString& name = element->getNameAttribute();
            if (!name.isEmpty() && id != name) {
                cache->appendToNameCache(name, *element);
                foundInputElements.add(name.impl());
            }
        }
    }

    for (auto& elementPtr : ownerNode().imageElements()) {
        if (!elementPtr)
            continue;
        HTMLImageElement& element = *elementPtr;
        const AtomString& id = element.getIdAttribute();
        if (!id.isEmpty() && !foundInputElements.contains(id.impl()))
            cache->appendToIdCache(id, element);
        const AtomString& name = element.getNameAttribute();
        if (!name.isEmpty() && id != name && !foundInputElements.contains(name.impl()))
            cache->appendToNameCache(name, element);
    }

    setNamedItemCache(WTFMove(cache));
}

void HTMLFormControlsCollection::invalidateCacheForDocument(Document& document)
{
    CachedHTMLCollection::invalidateCacheForDocument(document);
    m_cachedElement = nullptr;
    m_cachedElementOffsetInArray = 0;
}

}
