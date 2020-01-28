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

#include "config.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include "AXIsolatedTree.h"

#include "AXIsolatedObject.h"
#include "Page.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static Lock s_cacheLock;

static unsigned newTreeID()
{
    static unsigned s_currentTreeID = 0;
    return ++s_currentTreeID;
}

HashMap<PageIdentifier, Ref<AXIsolatedTree>>& AXIsolatedTree::treePageCache()
{
    static NeverDestroyed<HashMap<PageIdentifier, Ref<AXIsolatedTree>>> map;
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

RefPtr<AXIsolatedObject> AXIsolatedTree::nodeInTreeForID(AXIsolatedTreeID treeID, AXID axID)
{
    return treeForID(treeID)->nodeForID(axID);
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForID(AXIsolatedTreeID treeID)
{
    return treeIDCache().get(treeID);
}

Ref<AXIsolatedTree> AXIsolatedTree::createTreeForPageID(PageIdentifier pageID)
{
    LockHolder locker(s_cacheLock);
    ASSERT(!treePageCache().contains(pageID));

    auto newTree = AXIsolatedTree::create();
    treePageCache().set(pageID, newTree.copyRef());
    treeIDCache().set(newTree->treeIdentifier(), newTree.copyRef());
    return newTree;
}

void AXIsolatedTree::removeTreeForPageID(PageIdentifier pageID)
{
    LockHolder locker(s_cacheLock);

    if (auto optionalTree = treePageCache().take(pageID)) {
        auto& tree { *optionalTree };
        LockHolder treeLocker { tree->m_changeLogLock };
        for (const auto& axID : tree->m_readerThreadNodeMap.keys()) {
            if (auto object = tree->nodeForID(axID))
                object->detach(AccessibilityDetachmentType::CacheDestroyed);
        }
        tree->m_pendingAppends.clear();
        tree->m_pendingRemovals.clear();
        tree->m_readerThreadNodeMap.clear();
        treeLocker.unlockEarly();

        treeIDCache().remove(tree->treeIdentifier());
    }
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForPageID(PageIdentifier pageID)
{
    LockHolder locker(s_cacheLock);

    if (auto tree = treePageCache().get(pageID))
        return makeRefPtr(tree);

    return nullptr;
}

RefPtr<AXIsolatedObject> AXIsolatedTree::nodeForID(AXID axID) const
{
    if (!axID)
        return nullptr;
    return m_readerThreadNodeMap.get(axID);
}

RefPtr<AXIsolatedObject> AXIsolatedTree::focusedUIElement()
{
    m_focusedNodeID = m_pendingFocusedNodeID;
    return nodeForID(m_focusedNodeID);
}
    
RefPtr<AXIsolatedObject> AXIsolatedTree::rootNode()
{
    return nodeForID(m_rootNodeID);
}

void AXIsolatedTree::setRootNode(Ref<AXIsolatedObject>& root)
{
    LockHolder locker { m_changeLogLock };
    m_rootNodeID = root->objectID();
    m_readerThreadNodeMap.add(root->objectID(), WTFMove(root));
}
    
void AXIsolatedTree::setFocusedNodeID(AXID axID)
{
    LockHolder locker { m_changeLogLock };
    m_pendingFocusedNodeID = axID;
}
    
void AXIsolatedTree::removeNode(AXID axID)
{
    LockHolder locker { m_changeLogLock };
    m_pendingRemovals.append(axID);
}

void AXIsolatedTree::appendNodeChanges(Vector<Ref<AXIsolatedObject>>& log)
{
    LockHolder locker { m_changeLogLock };
    for (auto& node : log)
        m_pendingAppends.append(node.copyRef());
}

void AXIsolatedTree::applyPendingChanges()
{
    RELEASE_ASSERT(!isMainThread());
    LockHolder locker { m_changeLogLock };

    // We don't clear the pending IDs beacause if the next round of updates does not modify them, then they stay the same
    // value without extra bookkeeping.
    m_focusedNodeID = m_pendingFocusedNodeID;

    for (auto& item : m_pendingAppends)
        m_readerThreadNodeMap.add(item->objectID(), WTFMove(item));
    m_pendingAppends.clear();

    for (auto& item : m_pendingRemovals) {
        if (auto object = nodeForID(item))
            object->detach(AccessibilityDetachmentType::ElementDestroyed);
        m_readerThreadNodeMap.remove(item);
    }
    m_pendingRemovals.clear();
}
    
} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
