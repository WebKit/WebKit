/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DocumentOrderedMap.h"

#include "ElementIterator.h"
#include "HTMLImageElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HTMLNameCollection.h"

namespace WebCore {

using namespace HTMLNames;

inline bool keyMatchesId(const AtomicStringImpl& key, const Element& element)
{
    return element.getIdAttribute().impl() == &key;
}

inline bool keyMatchesName(const AtomicStringImpl& key, const Element& element)
{
    return element.getNameAttribute().impl() == &key;
}

inline bool keyMatchesMapName(const AtomicStringImpl& key, const Element& element)
{
    return isHTMLMapElement(element) && toHTMLMapElement(element).getName().impl() == &key;
}

inline bool keyMatchesLowercasedMapName(const AtomicStringImpl& key, const Element& element)
{
    return isHTMLMapElement(element) && toHTMLMapElement(element).getName().lower().impl() == &key;
}

inline bool keyMatchesLowercasedUsemap(const AtomicStringImpl& key, const Element& element)
{
    // FIXME: HTML5 specification says we should match both image and object elements.
    return isHTMLImageElement(element) && toHTMLImageElement(element).matchesLowercasedUsemap(key);
}

inline bool keyMatchesLabelForAttribute(const AtomicStringImpl& key, const Element& element)
{
    return isHTMLLabelElement(element) && element.getAttribute(forAttr).impl() == &key;
}

inline bool keyMatchesWindowNamedItem(const AtomicStringImpl& key, const Element& element)
{
    return WindowNameCollection::nodeMatches(const_cast<Element*>(&element), &key);
}

inline bool keyMatchesDocumentNamedItem(const AtomicStringImpl& key, const Element& element)
{
    return DocumentNameCollection::nodeMatches(const_cast<Element*>(&element), &key);
}

void DocumentOrderedMap::clear()
{
    m_map.clear();
}

void DocumentOrderedMap::add(const AtomicStringImpl& key, Element& element, const TreeScope& treeScope)
{
    UNUSED_PARAM(treeScope);
    ASSERT_WITH_SECURITY_IMPLICATION(element.isInTreeScope());
    ASSERT_WITH_SECURITY_IMPLICATION(treeScope.rootNode().containsIncludingShadowDOM(&element));
    if (!element.isInTreeScope())
        return;
    Map::AddResult addResult = m_map.add(&key, MapEntry(&element));
    if (addResult.isNewEntry)
        return;

    MapEntry& entry = addResult.iterator->value;
    ASSERT_WITH_SECURITY_IMPLICATION(entry.count);
    entry.element = nullptr;
    entry.count++;
    entry.orderedList.clear();
}

void DocumentOrderedMap::remove(const AtomicStringImpl& key, Element& element)
{
    m_map.checkConsistency();
    auto it = m_map.find(&key);
    ASSERT_WITH_SECURITY_IMPLICATION(it != m_map.end());
    if (it == m_map.end())
        return;
    MapEntry& entry = it->value;

    ASSERT_WITH_SECURITY_IMPLICATION(entry.count);
    if (entry.count == 1) {
        ASSERT_WITH_SECURITY_IMPLICATION(!entry.element || entry.element == &element);
        m_map.remove(it);
    } else {
        if (entry.element == &element)
            entry.element = nullptr;
        entry.count--;
        entry.orderedList.clear(); // FIXME: Remove the element instead if there are only few items left.
    }
}

template<bool keyMatches(const AtomicStringImpl&, const Element&)>
inline Element* DocumentOrderedMap::get(const AtomicStringImpl& key, const TreeScope& scope) const
{
    m_map.checkConsistency();

    auto it = m_map.find(&key);
    if (it == m_map.end())
        return nullptr;

    MapEntry& entry = it->value;
    ASSERT(entry.count);
    if (entry.element) {
        ASSERT_WITH_SECURITY_IMPLICATION(entry.element->isInTreeScope());
        ASSERT_WITH_SECURITY_IMPLICATION(&entry.element->treeScope() == &scope);
        return entry.element;
    }

    // We know there's at least one node that matches; iterate to find the first one.
    for (auto& element : descendantsOfType<Element>(scope.rootNode())) {
        if (!keyMatches(key, element))
            continue;
        entry.element = &element;
        ASSERT_WITH_SECURITY_IMPLICATION(element.isInTreeScope());
        ASSERT_WITH_SECURITY_IMPLICATION(&element.treeScope() == &scope);
        return &element;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

Element* DocumentOrderedMap::getElementById(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return get<keyMatchesId>(key, scope);
}

Element* DocumentOrderedMap::getElementByName(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return get<keyMatchesName>(key, scope);
}

HTMLMapElement* DocumentOrderedMap::getElementByMapName(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return toHTMLMapElement(get<keyMatchesMapName>(key, scope));
}

HTMLMapElement* DocumentOrderedMap::getElementByLowercasedMapName(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return toHTMLMapElement(get<keyMatchesLowercasedMapName>(key, scope));
}

HTMLImageElement* DocumentOrderedMap::getElementByLowercasedUsemap(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return toHTMLImageElement(get<keyMatchesLowercasedUsemap>(key, scope));
}

HTMLLabelElement* DocumentOrderedMap::getElementByLabelForAttribute(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return toHTMLLabelElement(get<keyMatchesLabelForAttribute>(key, scope));
}

Element* DocumentOrderedMap::getElementByWindowNamedItem(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return get<keyMatchesWindowNamedItem>(key, scope);
}

Element* DocumentOrderedMap::getElementByDocumentNamedItem(const AtomicStringImpl& key, const TreeScope& scope) const
{
    return get<keyMatchesDocumentNamedItem>(key, scope);
}

const Vector<Element*>* DocumentOrderedMap::getAllElementsById(const AtomicStringImpl& key, const TreeScope& scope) const
{
    m_map.checkConsistency();

    auto it = m_map.find(&key);
    if (it == m_map.end())
        return nullptr;

    MapEntry& entry = it->value;
    ASSERT_WITH_SECURITY_IMPLICATION(entry.count);
    if (!entry.count)
        return nullptr;

    if (entry.orderedList.isEmpty()) {
        entry.orderedList.reserveCapacity(entry.count);
        auto elementDescandents = descendantsOfType<Element>(scope.rootNode());
        auto it = entry.element ? elementDescandents.beginAt(*entry.element) : elementDescandents.begin();
        auto end = elementDescandents.end();
        for (; it != end; ++it) {
            auto& element = *it;
            if (!keyMatchesId(key, element))
                continue;
            entry.orderedList.append(&element);
        }
        ASSERT(entry.orderedList.size() == entry.count);
    }

    return &entry.orderedList;
}

} // namespace WebCore
