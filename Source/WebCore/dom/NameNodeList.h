/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007-2008, 2014 Apple Inc. All rights reserved.
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

#ifndef NameNodeList_h
#define NameNodeList_h

#include "LiveNodeList.h"
#include <wtf/Forward.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

// NodeList which lists all Nodes in a Element with a given "name" attribute
class NameNodeList final : public CachedLiveNodeList<NameNodeList> {
public:
    static PassRef<NameNodeList> create(ContainerNode& rootNode, const AtomicString& name)
    {
        return adoptRef(*new NameNodeList(rootNode, name));
    }

    virtual ~NameNodeList();

    virtual bool nodeMatches(Element*) const override;
    virtual bool isRootedAtDocument() const override { return false; }

private:
    NameNodeList(ContainerNode& rootNode, const AtomicString& name);

    AtomicString m_name;
};

inline bool NameNodeList::nodeMatches(Element* element) const
{
    return element->getNameAttribute() == m_name;
}

} // namespace WebCore

#endif // NameNodeList_h
