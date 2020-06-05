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
#include "AXLogger.h"
#include "Page.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static Lock s_cacheLock;

static unsigned newTreeID()
{
    static unsigned s_currentTreeID = 0;
    return ++s_currentTreeID;
}

AXIsolatedTree::NodeChange::NodeChange(AXIsolatedObject& isolatedObject, AccessibilityObjectWrapper* wrapper)
    : m_isolatedObject(isolatedObject)
    , m_wrapper(wrapper)
{
}

AXIsolatedTree::NodeChange::NodeChange(const NodeChange& other)
    : m_isolatedObject(other.m_isolatedObject.get())
    , m_wrapper(other.m_wrapper)
{
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
    AXTRACE("AXIsolatedTree::AXIsolatedTree");
}

AXIsolatedTree::~AXIsolatedTree()
{
    AXTRACE("AXIsolatedTree::~AXIsolatedTree");
}

void AXIsolatedTree::clear()
{
    AXTRACE("AXIsolatedTree::clear");
    ASSERT(isMainThread());

    LockHolder locker { m_changeLogLock };
    m_pendingSubtreeRemovals.append(m_rootNode->objectID());
    m_rootNode = nullptr;
    m_nodeMap.clear();
    m_axObjectCache = nullptr;
}

Ref<AXIsolatedTree> AXIsolatedTree::create()
{
    AXTRACE("AXIsolatedTree::create");
    ASSERT(isMainThread());
    return adoptRef(*new AXIsolatedTree());
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForID(AXIsolatedTreeID treeID)
{
    AXTRACE("AXIsolatedTree::treeForID");
    return treeIDCache().get(treeID);
}

Ref<AXIsolatedTree> AXIsolatedTree::createTreeForPageID(PageIdentifier pageID)
{
    AXTRACE("AXIsolatedTree::createTreeForPageID");
    LockHolder locker(s_cacheLock);
    ASSERT(!treePageCache().contains(pageID));

    auto newTree = AXIsolatedTree::create();
    treePageCache().set(pageID, newTree.copyRef());
    treeIDCache().set(newTree->treeID(), newTree.copyRef());
    return newTree;
}

void AXIsolatedTree::removeTreeForPageID(PageIdentifier pageID)
{
    AXTRACE("AXIsolatedTree::removeTreeForPageID");
    ASSERT(isMainThread());
    LockHolder locker(s_cacheLock);

    if (auto optionalTree = treePageCache().take(pageID)) {
        auto& tree { *optionalTree };
        tree->clear();
        treeIDCache().remove(tree->treeID());
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
    // FIXME: The following ASSERT should be met but it is commented out at the
    // moment because of <rdar://problem/63985646> After calling _AXUIElementUseSecondaryAXThread(true),
    // still receives client request on main thread.
    // ASSERT(axObjectCache()->canUseSecondaryAXThread() ? !isMainThread() : isMainThread());
    return axID != InvalidAXID ? m_readerThreadNodeMap.get(axID) : nullptr;
}

Vector<RefPtr<AXCoreObject>> AXIsolatedTree::objectsForIDs(Vector<AXID> axIDs) const
{
    AXTRACE("AXIsolatedTree::objectsForIDs");
    Vector<RefPtr<AXCoreObject>> result;
    result.reserveCapacity(axIDs.size());

    for (const auto& axID : axIDs) {
        if (auto object = nodeForID(axID))
            result.uncheckedAppend(object);
    }

    return result;
}

void AXIsolatedTree::generateSubtree(AXCoreObject& axObject, AXID parentID, bool attachWrapper)
{
    AXTRACE("AXIsolatedTree::generateSubtree");
    ASSERT(isMainThread());
    Vector<NodeChange> nodeChanges;
    auto object = createSubtree(axObject, parentID, attachWrapper, nodeChanges);
    LockHolder locker { m_changeLogLock };
    appendNodeChanges(nodeChanges);

    if (parentID == InvalidAXID)
        setRootNode(object.ptr());
    // FIXME: else attach the newly created subtree to its parent.
}

Ref<AXIsolatedObject> AXIsolatedTree::createSubtree(AXCoreObject& axObject, AXID parentID, bool attachWrapper, Vector<NodeChange>& nodeChanges)
{
    AXTRACE("AXIsolatedTree::createSubtree");
    ASSERT(isMainThread());

    auto object = AXIsolatedObject::create(axObject, m_treeID, parentID);
    if (attachWrapper) {
        object->attachPlatformWrapper(axObject.wrapper());
        // Since this object has already an attached wrapper, set the wrapper
        // in the NodeChange to null so that it is not re-attached.
        nodeChanges.append(NodeChange(object, nullptr));
    } else {
        // Set the wrapper in the NodeChange so that it is set on the AX thread.
        nodeChanges.append(NodeChange(object, axObject.wrapper()));
    }

    Vector<AXID> childrenIDs;
    for (const auto& axChild : axObject.children()) {
        auto child = createSubtree(*axChild, axObject.objectID(), attachWrapper, nodeChanges);
        childrenIDs.append(child->objectID());
    }
    m_nodeMap.set(object->objectID(), childrenIDs);
    object->setChildrenIDs(WTFMove(childrenIDs));

    return object;
}

void AXIsolatedTree::updateNode(AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateNode");
    AXLOG(&axObject);
    ASSERT(isMainThread());

    AXID axID = axObject.objectID();
    auto* axParent = axObject.parentObject();
    AXID parentID = axParent ? axParent->objectID() : InvalidAXID;

    auto newObject = AXIsolatedObject::create(axObject, m_treeID, parentID);
    newObject->m_childrenIDs = axObject.childrenIDs();

    {
        LockHolder locker { m_changeLogLock };
        // Remove the old object and set the new one to be updated on the AX thread.
        m_pendingNodeRemovals.append(axID);
        m_pendingAppends.append(NodeChange(newObject, axObject.wrapper()));
    }
}

void AXIsolatedTree::updateSubtree(AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateSubtree");
    AXLOG(&axObject);
    ASSERT(isMainThread());
    removeSubtree(axObject.objectID());
    auto* axParent = axObject.parentObject();
    AXID parentID = axParent ? axParent->objectID() : InvalidAXID;
    generateSubtree(axObject, parentID, false);
}

void AXIsolatedTree::updateChildren(AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateChildren");
    AXLOG("For AXObject:");
    AXLOG(&axObject);
    ASSERT(isMainThread());

    if (!axObject.document() || !axObject.document()->hasLivingRenderTree())
        return;

    // updateChildren may be called as the result of a children changed
    // notification for an axObject that has no associated isolated object.
    // An example of this is when an empty element such as a <canvas> or <div>
    // is added a new child. So find the closest ancestor of axObject that has
    // an associated isolated object and update its children.
    auto iterator = m_nodeMap.end();
    auto* axAncestor = Accessibility::findAncestor(axObject, true, [&iterator, this] (const AXCoreObject& ancestor) {
        auto it = m_nodeMap.find(ancestor.objectID());
        if (it != m_nodeMap.end()) {
            iterator = it;
            return true;
        }
        return false;
    });
    ASSERT(axAncestor && iterator != m_nodeMap.end());
    if (!axAncestor || iterator == m_nodeMap.end())
        return; // nothing to update.

    // iterator is pointing to the m_nodeMap entry corresponding to axAncestor->objectID().
    ASSERT(iterator->key == axAncestor->objectID());
    auto removals = iterator->value;

    const auto& axChildren = axAncestor->children();
    auto axChildrenIDs = axAncestor->childrenIDs();

    for (size_t i = 0; i < axChildrenIDs.size(); ++i) {
        size_t index = removals.find(axChildrenIDs[i]);
        if (index != notFound)
            removals.remove(index);
        else {
            // This is a new child, add it to the tree.
            AXLOG("Adding a new child for:");
            AXLOG(axChildren[i]);
            generateSubtree(*axChildren[i], axAncestor->objectID(), true);
        }
    }

    // What is left in removals are the IDs that are no longer children of
    // axObject. Thus, remove them from the tree.
    for (const AXID& childID : removals)
        removeSubtree(childID);

    // Lastly, make the children IDs of the isolated object to be the same as the AXObject's.
    m_nodeMap.set(axAncestor->objectID(), axChildrenIDs);
    {
        LockHolder locker { m_changeLogLock };
        m_pendingChildrenUpdates.append(std::make_pair(axAncestor->objectID(), axChildrenIDs));
    }
}

RefPtr<AXIsolatedObject> AXIsolatedTree::focusedNode()
{
    AXTRACE("AXIsolatedTree::focusedNode");
    // Apply pending changes in case focus has changed and hasn't been updated.
    applyPendingChanges();
    LockHolder locker { m_changeLogLock };
    AXLOG(makeString("focusedNodeID ", m_focusedNodeID));
    AXLOG("focused node:");
    AXLOG(nodeForID(m_focusedNodeID));
    return nodeForID(m_focusedNodeID);
}

RefPtr<AXIsolatedObject> AXIsolatedTree::rootNode()
{
    AXTRACE("AXIsolatedTree::rootNode");
    LockHolder locker { m_changeLogLock };
    return m_rootNode;
}

void AXIsolatedTree::setRootNode(AXIsolatedObject* root)
{
    AXTRACE("AXIsolatedTree::setRootNode");
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());
    ASSERT(root);

    m_rootNode = root;
}

void AXIsolatedTree::setFocusedNodeID(AXID axID)
{
    AXTRACE("AXIsolatedTree::setFocusedNodeID");
    AXLOG(makeString("axID ", axID));
    ASSERT(isMainThread());
    LockHolder locker { m_changeLogLock };
    m_pendingFocusedNodeID = axID;
}

void AXIsolatedTree::removeNode(AXID axID)
{
    AXTRACE("AXIsolatedTree::removeNode");
    AXLOG(makeString("AXID ", axID));

    LockHolder locker { m_changeLogLock };
    m_pendingNodeRemovals.append(axID);
}

void AXIsolatedTree::removeSubtree(AXID axID)
{
    AXTRACE("AXIsolatedTree::removeSubtree");
    AXLOG(makeString("Removing subtree for axID ", axID));
    ASSERT(isMainThread());

    Vector<AXID> removals = { axID };
    while (removals.size()) {
        AXID axID = removals.takeLast();
        auto it = m_nodeMap.find(axID);
        if (it != m_nodeMap.end()) {
            removals.appendVector(it->value);
            m_nodeMap.remove(axID);
        }
    }

    LockHolder locker { m_changeLogLock };
    m_pendingSubtreeRemovals.append(axID);
}

void AXIsolatedTree::appendNodeChanges(const Vector<NodeChange>& changes)
{
    AXTRACE("AXIsolatedTree::appendNodeChanges");
    ASSERT(isMainThread());
    m_pendingAppends.appendVector(changes);
}

void AXIsolatedTree::applyPendingChanges()
{
    AXTRACE("AXIsolatedTree::applyPendingChanges");
    LockHolder locker { m_changeLogLock };

    AXLOG(makeString("focusedNodeID ", m_focusedNodeID, " pendingFocusedNodeID ", m_pendingFocusedNodeID));
    m_focusedNodeID = m_pendingFocusedNodeID;

    while (m_pendingNodeRemovals.size()) {
        auto axID = m_pendingNodeRemovals.takeLast();
        AXLOG(makeString("removing axID ", axID));
        if (auto object = nodeForID(axID)) {
            object->detach(AccessibilityDetachmentType::ElementDestroyed);
            m_readerThreadNodeMap.remove(axID);
        }
    }

    while (m_pendingSubtreeRemovals.size()) {
        auto axID = m_pendingSubtreeRemovals.takeLast();
        AXLOG(makeString("removing subtree axID ", axID));
        if (auto object = nodeForID(axID)) {
            object->detach(AccessibilityDetachmentType::ElementDestroyed);
            m_pendingSubtreeRemovals.appendVector(object->m_childrenIDs);
            m_readerThreadNodeMap.remove(axID);
        }
    }

    for (const auto& item : m_pendingAppends) {
        AXID axID = item.m_isolatedObject->objectID();
        AXLOG(makeString("appending axID ", axID));
        if (axID == InvalidAXID)
            continue;

        auto& wrapper = item.m_wrapper ? item.m_wrapper : item.m_isolatedObject->wrapper();
        if (!wrapper)
            continue;

        if (auto object = m_readerThreadNodeMap.get(axID)) {
            if (object != &item.m_isolatedObject.get()
                && object->wrapper() == wrapper.get()) {
                // The new IsolatedObject is a replacement for an existing object
                // as the result of an update. Thus detach the wrapper from the
                // existing object and attach it to the new one.
                object->detachWrapper(AccessibilityDetachmentType::ElementDestroyed);
                item.m_isolatedObject->attachPlatformWrapper(wrapper.get());
            }
            m_readerThreadNodeMap.remove(axID);
        }

        if (!item.m_isolatedObject->wrapper()) {
            // The new object hasn't been attached a wrapper yet, so attach it.
            item.m_isolatedObject->attachPlatformWrapper(wrapper.get());
        }

        auto addResult = m_readerThreadNodeMap.add(axID, item.m_isolatedObject.get());
        // The newly added object must have a wrapper.
        ASSERT_UNUSED(addResult, addResult.iterator->value->wrapper());
        // The reference count of the just added IsolatedObject must be 2
        // because it is referenced by m_readerThreadNodeMap and m_pendingAppends.
        // When m_pendingAppends is cleared, the object will be held only by m_readerThreadNodeMap. The exception is the root node whose reference count is 3.
        ASSERT_UNUSED(addResult, addResult.iterator->value->refCount() == 2
            || (addResult.iterator->value.ptr() == m_rootNode.get() && m_rootNode->refCount() == 3));
    }
    m_pendingAppends.clear();

    for (auto& update : m_pendingChildrenUpdates) {
        AXLOG(makeString("updating children for axID ", update.first));
        if (auto object = nodeForID(update.first))
            object->setChildrenIDs(WTFMove(update.second));
    }
    m_pendingChildrenUpdates.clear();
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
