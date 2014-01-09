/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef DocumentOrderedMap_h
#define DocumentOrderedMap_h

#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringImpl.h>

namespace WebCore {

class Element;
class HTMLImageElement;
class HTMLLabelElement;
class HTMLMapElement;
class TreeScope;

class DocumentOrderedMap {
public:
    void add(const AtomicStringImpl&, Element&, const TreeScope&);
    void remove(const AtomicStringImpl&, Element&);
    void clear();

    bool contains(const AtomicStringImpl&) const;
    bool containsSingle(const AtomicStringImpl&) const;
    bool containsMultiple(const AtomicStringImpl&) const;

    // concrete instantiations of the get<>() method template
    Element* getElementById(const AtomicStringImpl&, const TreeScope&) const;
    Element* getElementByName(const AtomicStringImpl&, const TreeScope&) const;
    HTMLMapElement* getElementByMapName(const AtomicStringImpl&, const TreeScope&) const;
    HTMLMapElement* getElementByLowercasedMapName(const AtomicStringImpl&, const TreeScope&) const;
    HTMLImageElement* getElementByLowercasedUsemap(const AtomicStringImpl&, const TreeScope&) const;
    HTMLLabelElement* getElementByLabelForAttribute(const AtomicStringImpl&, const TreeScope&) const;
    Element* getElementByWindowNamedItem(const AtomicStringImpl&, const TreeScope&) const;
    Element* getElementByDocumentNamedItem(const AtomicStringImpl&, const TreeScope&) const;

    const Vector<Element*>* getAllElementsById(const AtomicStringImpl&, const TreeScope&) const;

private:
    template<bool keyMatches(const AtomicStringImpl&, const Element&)> Element* get(const AtomicStringImpl&, const TreeScope&) const;

    struct MapEntry {
        MapEntry()
            : element(0)
            , count(0)
        { }
        explicit MapEntry(Element* firstElement)
            : element(firstElement)
            , count(1)
        { }

        Element* element;
        unsigned count;
        Vector<Element*> orderedList;
    };

    typedef HashMap<const AtomicStringImpl*, MapEntry> Map;

    mutable Map m_map;
};

inline bool DocumentOrderedMap::containsSingle(const AtomicStringImpl& id) const
{
    auto it = m_map.find(&id);
    return it != m_map.end() && it->value.count == 1;
}

inline bool DocumentOrderedMap::contains(const AtomicStringImpl& id) const
{
    return m_map.contains(&id);
}

inline bool DocumentOrderedMap::containsMultiple(const AtomicStringImpl& id) const
{
    auto it = m_map.find(&id);
    return it != m_map.end() && it->value.count > 1;
}

} // namespace WebCore

#endif // DocumentOrderedMap_h
