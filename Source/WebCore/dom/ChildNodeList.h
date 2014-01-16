/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007, 2013 Apple Inc. All rights reserved.
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

#ifndef ChildNodeList_h
#define ChildNodeList_h

#include "CollectionIndexCache.h"
#include "NodeList.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ContainerNode;

class EmptyNodeList FINAL : public NodeList {
public:
    static PassRefPtr<EmptyNodeList> create(Node& owner)
    {
        return adoptRef(new EmptyNodeList(owner));
    }
    virtual ~EmptyNodeList();

    Node& ownerNode() { return m_owner.get(); }

private:
    explicit EmptyNodeList(Node& owner) : m_owner(owner) { }

    virtual unsigned length() const override { return 0; }
    virtual Node* item(unsigned) const override { return nullptr; }
    virtual Node* namedItem(const AtomicString&) const override { return nullptr; }

    virtual bool isEmptyNodeList() const override { return true; }

    Ref<Node> m_owner;
};

class ChildNodeList FINAL : public NodeList {
public:
    static PassRefPtr<ChildNodeList> create(ContainerNode& parent)
    {
        return adoptRef(new ChildNodeList(parent));
    }

    virtual ~ChildNodeList();

    ContainerNode& ownerNode() { return m_parent.get(); }

    void invalidateCache();

    // For CollectionIndexCache
    Node* collectionFirst() const;
    Node* collectionLast() const;
    Node* collectionTraverseForward(Node&, unsigned count, unsigned& traversedCount) const;
    Node* collectionTraverseBackward(Node&, unsigned count) const;
    bool collectionCanTraverseBackward() const { return true; }

private:
    explicit ChildNodeList(ContainerNode& parent);

    virtual unsigned length() const override;
    virtual Node* item(unsigned index) const override;
    virtual Node* namedItem(const AtomicString&) const override;

    virtual bool isChildNodeList() const override { return true; }

    Ref<ContainerNode> m_parent;
    mutable CollectionIndexCache<ChildNodeList, Node> m_indexCache;
};

} // namespace WebCore

#endif // ChildNodeList_h
