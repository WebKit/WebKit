/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
 */

#ifndef TagNodeList_h
#define TagNodeList_h

#include "Element.h"
#include "LiveNodeList.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {

// NodeList that limits to a particular tag.
class TagNodeList final : public CachedLiveNodeList<TagNodeList> {
public:
    static PassRef<TagNodeList> create(ContainerNode& rootNode, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        ASSERT(namespaceURI != starAtom);
        return adoptRef(*new TagNodeList(rootNode, namespaceURI, localName));
    }

    static PassRef<TagNodeList> create(ContainerNode& rootNode, const AtomicString& localName)
    {
        return adoptRef(*new TagNodeList(rootNode, starAtom, localName));
    }

    virtual ~TagNodeList();

    virtual bool nodeMatches(Element*) const override;
    virtual bool isRootedAtDocument() const override { return false; }

protected:
    TagNodeList(ContainerNode& rootNode, const AtomicString& namespaceURI, const AtomicString& localName);

    AtomicString m_namespaceURI;
    AtomicString m_localName;
};

inline bool TagNodeList::nodeMatches(Element* element) const
{
    // Implements http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#concept-getelementsbytagnamens
    if (m_localName != starAtom && m_localName != element->localName())
        return false;

    return m_namespaceURI == starAtom || m_namespaceURI == element->namespaceURI();
}

class HTMLTagNodeList final : public CachedLiveNodeList<HTMLTagNodeList> {
public:
    static PassRef<HTMLTagNodeList> create(ContainerNode& rootNode, const AtomicString& localName)
    {
        return adoptRef(*new HTMLTagNodeList(rootNode, localName));
    }

    virtual ~HTMLTagNodeList();

    virtual bool nodeMatches(Element*) const override;
    virtual bool isRootedAtDocument() const override { return false; }

private:
    HTMLTagNodeList(ContainerNode& rootNode, const AtomicString& localName);

    AtomicString m_localName;
    AtomicString m_loweredLocalName;
};

inline bool HTMLTagNodeList::nodeMatches(Element* element) const
{
    // Implements http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#concept-getelementsbytagname
    if (m_localName == starAtom)
        return true;
    const AtomicString& localName = element->isHTMLElement() ? m_loweredLocalName : m_localName;
    return localName == element->localName();
}

} // namespace WebCore

#endif // TagNodeList_h
