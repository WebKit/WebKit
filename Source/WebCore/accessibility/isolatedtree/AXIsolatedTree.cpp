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

Lock AXIsolatedTree::s_cacheLock;

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

AXIsolatedTree::AXIsolatedTree(AXObjectCache* axObjectCache)
    : m_treeID(newTreeID())
    , m_axObjectCache(axObjectCache)
    , m_usedOnAXThread(axObjectCache->usedOnAXThread())
{
    AXTRACE("AXIsolatedTree::AXIsolatedTree");
    ASSERT(isMainThread());
}

AXIsolatedTree::~AXIsolatedTree()
{
    AXTRACE("AXIsolatedTree::~AXIsolatedTree");
}

void AXIsolatedTree::clear()
{
    AXTRACE("AXIsolatedTree::clear");
    ASSERT(isMainThread());
    m_axObjectCache = nullptr;

    Locker locker { m_changeLogLock };
    m_pendingSubtreeRemovals.append(m_rootNode->objectID());
    m_rootNode = nullptr;
    m_nodeMap.clear();
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForID(AXIsolatedTreeID treeID)
{
    AXTRACE("AXIsolatedTree::treeForID");
    Locker locker { s_cacheLock };
    return treeIDCache().get(treeID);
}

Ref<AXIsolatedTree> AXIsolatedTree::create(AXObjectCache* axObjectCache)
{
    AXTRACE("AXIsolatedTree::create");
    ASSERT(isMainThread());
    ASSERT(axObjectCache && axObjectCache->pageID());

    auto tree = adoptRef(*new AXIsolatedTree(axObjectCache));

    // Generate the nodes of the tree and set its root and focused objects.
    // For this, we need the root and focused objects of the AXObject tree.
    auto* axRoot = axObjectCache->getOrCreate(axObjectCache->document().view());
    if (axRoot)
        tree->generateSubtree(*axRoot, nullptr, true);
    auto* axFocus = axObjectCache->focusedObjectForPage(axObjectCache->document().page());
    if (axFocus)
        tree->setFocusedNodeID(axFocus->objectID());

    // Now that the tree is ready to take client requests, add it to the tree
    // maps so that it can be found.
    auto pageID = axObjectCache->pageID();
    Locker locker { s_cacheLock };
    ASSERT(!treePageCache().contains(*pageID));
    treePageCache().set(*pageID, tree.copyRef());
    treeIDCache().set(tree->treeID(), tree.copyRef());

    return tree;
}

void AXIsolatedTree::removeTreeForPageID(PageIdentifier pageID)
{
    AXTRACE("AXIsolatedTree::removeTreeForPageID");
    ASSERT(isMainThread());
    Locker locker { s_cacheLock };

    if (auto tree = treePageCache().take(pageID)) {
        tree->clear();
        treeIDCache().remove(tree->treeID());
    }
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForPageID(PageIdentifier pageID)
{
    Locker locker { s_cacheLock };

    if (auto tree = treePageCache().get(pageID))
        return makeRefPtr(tree);

    return nullptr;
}

RefPtr<AXIsolatedObject> AXIsolatedTree::nodeForID(AXID axID) const
{
    // In isolated tree mode 2, only access m_readerThreadNodeMap on the AX thread.
    if (m_usedOnAXThread && isMainThread())
        return nullptr;

    return axID != InvalidAXID ? m_readerThreadNodeMap.get(axID) : nullptr;
}

Vector<RefPtr<AXCoreObject>> AXIsolatedTree::objectsForIDs(const Vector<AXID>& axIDs) const
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

Vector<AXID> AXIsolatedTree::idsForObjects(const Vector<RefPtr<AXCoreObject>>& objects) const
{
    return objects.map([] (const RefPtr<AXCoreObject>& object) -> AXID {
        return object ? object->objectID() : InvalidAXID;
    });
}

void AXIsolatedTree::updateChildrenIDs(AXID axID, Vector<AXID>&& childrenIDs)
{
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    if (axID != InvalidAXID) {
        m_nodeMap.set(axID, childrenIDs);
        m_pendingChildrenUpdates.append(std::make_pair(axID, WTFMove(childrenIDs)));
    }
}

void AXIsolatedTree::generateSubtree(AXCoreObject& axObject, AXCoreObject* axParent, bool attachWrapper)
{
    AXTRACE("AXIsolatedTree::generateSubtree");
    ASSERT(isMainThread());

    if (axObject.objectID() == InvalidAXID)
        return;

    auto object = createSubtree(axObject, axParent ? axParent->objectID() : InvalidAXID, attachWrapper);
    Locker locker { m_changeLogLock };
    if (!axParent)
        setRootNode(object.ptr());
    else if (axParent->objectID() != InvalidAXID) // Need to check for the objectID of axParent again because it may have been detached while traversing the tree.
        updateChildrenIDs(axParent->objectID(), axParent->childrenIDs());
}

Ref<AXIsolatedObject> AXIsolatedTree::createSubtree(AXCoreObject& axObject, AXID parentID, bool attachWrapper)
{
    AXTRACE("AXIsolatedTree::createSubtree");
    ASSERT(isMainThread());

    auto object = AXIsolatedObject::create(axObject, this, parentID);
    if (object->objectID() == InvalidAXID) {
        // Either the axObject has an invalid ID or something else went terribly wrong. Don't bother doing anything else.
        ASSERT_NOT_REACHED();
        return object;
    }

    NodeChange nodeChange { object, nullptr };
    if (attachWrapper)
        object->attachPlatformWrapper(axObject.wrapper());
    else {
        // Set the wrapper in the NodeChange so that it is set on the AX thread.
        nodeChange.wrapper = axObject.wrapper();
    }

    auto axChildren = axObject.children();
    Vector<AXID> childrenIDs;
    childrenIDs.reserveCapacity(axChildren.size());
    for (const auto& axChild : axChildren) {
        auto child = createSubtree(*axChild, axObject.objectID(), attachWrapper);
        childrenIDs.uncheckedAppend(child->objectID());
    }

    {
        Locker locker { m_changeLogLock };
        m_pendingAppends.append(WTFMove(nodeChange));
        updateChildrenIDs(object->objectID(), WTFMove(childrenIDs));
    }

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

    auto newObject = AXIsolatedObject::create(axObject, this, parentID);
    newObject->m_childrenIDs = axObject.childrenIDs();

    Locker locker { m_changeLogLock };
    // Remove the old object and set the new one to be updated on the AX thread.
    m_pendingNodeRemovals.append(axID);
    m_pendingAppends.append({ newObject, axObject.wrapper() });
}

void AXIsolatedTree::updateNodeProperty(const AXCoreObject& axObject, AXPropertyName property)
{
    AXTRACE("AXIsolatedTree::updateNodeProperty");
    ASSERT(isMainThread());

    AXPropertyMap propertyMap;
    switch (property) {
    case AXPropertyName::IsChecked:
        propertyMap.set(AXPropertyName::IsChecked, axObject.isChecked());
        break;
    case AXPropertyName::SortDirection:
        propertyMap.set(AXPropertyName::SortDirection, static_cast<int>(axObject.sortDirection()));
        break;
    case AXPropertyName::IdentifierAttribute:
        propertyMap.set(AXPropertyName::IdentifierAttribute, axObject.identifierAttribute().isolatedCopy());
        break;
    default:
        return;
    }

    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axObject.objectID(), propertyMap });
}

void AXIsolatedTree::updateSubtree(AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateSubtree");
    AXLOG(&axObject);
    ASSERT(isMainThread());

    removeSubtree(axObject.objectID());
    generateSubtree(axObject, axObject.parentObject(), false);
}

void AXIsolatedTree::updateChildren(AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateChildren");
    AXLOG("For AXObject:");
    AXLOG(&axObject);
    ASSERT(isMainThread());

    if (m_nodeMap.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

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
    if (!axAncestor || axAncestor->objectID() == InvalidAXID || iterator == m_nodeMap.end()) {
        // This update triggered before the isolated tree has been repopulated.
        // Return here since there is nothing to update.
        return;
    }

    // iterator is pointing to the m_nodeMap entry corresponding to axAncestor->objectID().
    ASSERT(iterator->key == axAncestor->objectID());
    auto removals = iterator->value;

    const auto& axChildren = axAncestor->children();
    auto axChildrenIDs = axAncestor->childrenIDs();

    bool updatedChild = false; // Set to true if at least one child's subtree is updated.
    for (size_t i = 0; i < axChildren.size() && i < axChildrenIDs.size(); ++i) {
        size_t index = removals.find(axChildrenIDs[i]);
        if (index != notFound)
            removals.remove(index);
        else {
            ASSERT(axChildren[i]->objectID() == axChildrenIDs[i]);
            // This is a new child, add it to the tree.
            AXLOG("Adding a new child for:");
            AXLOG(axChildren[i]);
            generateSubtree(*axChildren[i], axAncestor, true);
            updatedChild = true;
        }
    }

    // What is left in removals are the IDs that are no longer children of
    // axObject. Thus, remove them from the tree.
    for (const AXID& childID : removals)
        removeSubtree(childID);

    if (updatedChild || removals.size()) {
        // Make the children IDs of the isolated object to be the same as the AXObject's.
        Locker locker { m_changeLogLock };
        updateChildrenIDs(axAncestor->objectID(), WTFMove(axChildrenIDs));
    } else {
        // Nothing was updated. As a last resort, update the subtree.
        updateSubtree(*axAncestor);
    }
}

RefPtr<AXIsolatedObject> AXIsolatedTree::focusedNode()
{
    AXTRACE("AXIsolatedTree::focusedNode");
    // Apply pending changes in case focus has changed and hasn't been updated.
    applyPendingChanges();
    Locker locker { m_changeLogLock };
    AXLOG(makeString("focusedNodeID ", m_focusedNodeID));
    AXLOG("focused node:");
    AXLOG(nodeForID(m_focusedNodeID));
    return nodeForID(m_focusedNodeID);
}

RefPtr<AXIsolatedObject> AXIsolatedTree::rootNode()
{
    AXTRACE("AXIsolatedTree::rootNode");
    Locker locker { m_changeLogLock };
    return m_rootNode;
}

void AXIsolatedTree::setRootNode(AXIsolatedObject* root)
{
    AXTRACE("AXIsolatedTree::setRootNode");
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());
    ASSERT(!m_rootNode);
    ASSERT(root);

    m_rootNode = root;
}

void AXIsolatedTree::setFocusedNodeID(AXID axID)
{
    AXTRACE("AXIsolatedTree::setFocusedNodeID");
    AXLOG(makeString("axID ", axID));
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_pendingFocusedNodeID = axID;

    AXPropertyMap propertyMap;
    propertyMap.set(AXPropertyName::IsFocused, true);
    m_pendingPropertyChanges.append({ axID, propertyMap });
}

void AXIsolatedTree::removeNode(AXID axID)
{
    AXTRACE("AXIsolatedTree::removeNode");
    AXLOG(makeString("AXID ", axID));
    ASSERT(isMainThread());

    m_nodeMap.remove(axID);
    Locker locker { m_changeLogLock };
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
        if (axID == InvalidAXID)
            continue;

        auto it = m_nodeMap.find(axID);
        if (it != m_nodeMap.end()) {
            removals.appendVector(it->value);
            m_nodeMap.remove(axID);
        }
    }

    Locker locker { m_changeLogLock };
    m_pendingSubtreeRemovals.append(axID);
}

void AXIsolatedTree::applyPendingChanges()
{
    AXTRACE("AXIsolatedTree::applyPendingChanges");

    // In isolated tree mode 2, only apply pending changes on the AX thread.
    if (m_usedOnAXThread && isMainThread())
        return;

    Locker locker { m_changeLogLock };

    if (m_pendingFocusedNodeID != m_focusedNodeID) {
        AXLOG(makeString("focusedNodeID ", m_focusedNodeID, " pendingFocusedNodeID ", m_pendingFocusedNodeID));

        if (m_focusedNodeID != InvalidAXID) {
            // Set the old focused object's IsFocused property to false.
            AXPropertyMap propertyMap;
            propertyMap.set(AXPropertyName::IsFocused, false);
            m_pendingPropertyChanges.append({ m_focusedNodeID, propertyMap });
        }
        m_focusedNodeID = m_pendingFocusedNodeID;
    }

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
        AXID axID = item.isolatedObject->objectID();
        AXLOG(makeString("appending axID ", axID));
        if (axID == InvalidAXID)
            continue;

        auto& wrapper = item.wrapper ? item.wrapper : item.isolatedObject->wrapper();
        if (!wrapper)
            continue;

        if (auto object = m_readerThreadNodeMap.get(axID)) {
            if (object != &item.isolatedObject.get()
                && object->wrapper() == wrapper.get()) {
                // The new IsolatedObject is a replacement for an existing object
                // as the result of an update. Thus detach the wrapper from the
                // existing object and attach it to the new one.
                object->detachWrapper(AccessibilityDetachmentType::ElementChanged);
                item.isolatedObject->attachPlatformWrapper(wrapper.get());
            }
            m_readerThreadNodeMap.remove(axID);
        }

        if (!item.isolatedObject->wrapper()) {
            // The new object hasn't been attached a wrapper yet, so attach it.
            item.isolatedObject->attachPlatformWrapper(wrapper.get());
        }

        auto addResult = m_readerThreadNodeMap.add(axID, item.isolatedObject.get());
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
            object->m_childrenIDs = WTFMove(update.second);
    }
    m_pendingChildrenUpdates.clear();

    for (auto& change : m_pendingPropertyChanges) {
        if (auto object = nodeForID(change.axID)) {
            for (auto& property : change.properties)
                object->setProperty(property.key, WTFMove(property.value));
        }
    }
    m_pendingPropertyChanges.clear();
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
