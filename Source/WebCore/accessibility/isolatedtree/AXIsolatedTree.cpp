/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include "config.h"
#include "AXIsolatedTree.h"

#include "AXIsolatedTreeNode.h"
#include "Page.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static Lock s_cacheLock;

static unsigned newTreeID()
{
    static unsigned s_currentTreeID = 0;
    return ++s_currentTreeID;
}

HashMap<AXIsolatedTreeID, Ref<AXIsolatedTree>>& AXIsolatedTree::treePageCache()
{
    static NeverDestroyed<HashMap<AXIsolatedTreeID, Ref<AXIsolatedTree>>> map;
    return map;
}

HashMap<AXIsolatedTreeID, Ref<AXIsolatedTree>>& AXIsolatedTree::treeIDCache()
{
    static NeverDestroyed<HashMap<AXIsolatedTreeID, Ref<AXIsolatedTree>>> map;
    return map;
}

AXIsolatedTree::AXIsolatedTree()
    : m_treeID(newTreeID())
{
}

AXIsolatedTree::~AXIsolatedTree() = default;

Ref<AXIsolatedTree> AXIsolatedTree::create()
{
    ASSERT(isMainThread());
    return adoptRef(*new AXIsolatedTree());
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForID(AXIsolatedTreeID treeID)
{
    return treeIDCache().get(treeID);
}

Ref<AXIsolatedTree> AXIsolatedTree::createTreeForPageID(uint64_t pageID)
{
    LockHolder locker(s_cacheLock);

    auto newTree = AXIsolatedTree::create();
    treePageCache().set(pageID, newTree.copyRef());
    treeIDCache().set(newTree->treeIdentifier(), newTree.copyRef());
    return newTree;
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForPageID(uint64_t pageID)
{
    LockHolder locker(s_cacheLock);

    if (auto tree = treePageCache().get(pageID))
        return makeRefPtr(tree);

    return nullptr;
}

RefPtr<AXIsolatedTreeNode> AXIsolatedTree::nodeForID(AXID axID) const
{
    ASSERT(!isMainThread());
    if (!axID)
        return nullptr;
    return m_readerThreadNodeMap.get(axID);
}

RefPtr<AXIsolatedTreeNode> AXIsolatedTree::rootNode()
{
    return nodeForID(m_rootNodeID);
}

void AXIsolatedTree::removeNode(AXID axID)
{
    LockHolder locker(m_changeLogLock);
    m_pendingRemovals.append(axID);
}

void AXIsolatedTree::appendNodeChanges(Vector<Ref<AXIsolatedTreeNode>>& log)
{
    LockHolder locker(m_changeLogLock);
    for (auto& node : log)
        m_pendingAppends.append(node.copyRef());
}

void AXIsolatedTree::applyPendingChanges()
{
    RELEASE_ASSERT(!isMainThread());

    LockHolder locker(m_changeLogLock);
    Vector<Ref<AXIsolatedTreeNode>> appendCopy;
    std::swap(appendCopy, m_pendingAppends);
    Vector<AXID> removeCopy({ WTFMove(m_pendingRemovals) });
    locker.unlockEarly();

    for (auto& item : appendCopy) {
        m_readerThreadNodeMap.add(item->identifier(), item.copyRef());
        if (item->isRootNode())
            m_rootNodeID = item->identifier();
    }

    for (auto item : removeCopy)
        m_readerThreadNodeMap.remove(item);
}
    
} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
