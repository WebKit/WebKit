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
#include "FrameView.h"
#include "Page.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>

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
    m_nodeMap.clear();

    Locker locker { m_changeLogLock };
    m_pendingSubtreeRemovals.append(m_rootNode->objectID());
    m_rootNode = nullptr;
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

    auto& document = axObjectCache->document();
    if (!document.view()->layoutContext().isInRenderTreeLayout() && !document.inRenderTreeUpdate() && !document.inStyleRecalc())
        document.updateLayoutIgnorePendingStylesheets();

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
    tree->updateLoadingProgress(axObjectCache->loadingProgress());

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
        return RefPtr { tree };

    return nullptr;
}

RefPtr<AXIsolatedObject> AXIsolatedTree::nodeForID(AXID axID) const
{
    // In isolated tree mode 2, only access m_readerThreadNodeMap on the AX thread.
    ASSERT(m_usedOnAXThread ? !isMainThread() : isMainThread());
    if (m_usedOnAXThread && isMainThread())
        return nullptr;

    return axID.isValid() ? m_readerThreadNodeMap.get(axID) : nullptr;
}

Vector<RefPtr<AXCoreObject>> AXIsolatedTree::objectsForIDs(const Vector<AXID>& axIDs) const
{
    AXTRACE("AXIsolatedTree::objectsForIDs");
    Vector<RefPtr<AXCoreObject>> result;
    result.reserveInitialCapacity(axIDs.size());
    for (auto& axID : axIDs) {
        if (auto object = nodeForID(axID))
            result.uncheckedAppend(object);
    }
    result.shrinkToFit();

    return result;
}

Vector<AXID> AXIsolatedTree::idsForObjects(const Vector<RefPtr<AXCoreObject>>& objects) const
{
    return objects.map([] (const RefPtr<AXCoreObject>& object) -> AXID {
        return object ? object->objectID() : AXID();
    });
}

void AXIsolatedTree::generateSubtree(AXCoreObject& axObject, AXCoreObject* axParent, bool attachWrapper)
{
    AXTRACE("AXIsolatedTree::generateSubtree");
    ASSERT(isMainThread());

    if (!axObject.objectID().isValid())
        return;

    AXID parentID = axParent ? axParent->objectID() : AXID();
    Vector<NodeChange> changes;
    collectNodeChangesForSubtree(axObject, parentID, attachWrapper, changes);
    queueChangesAndRemovals(changes);
}

AXID AXIsolatedTree::parentIDForObject(AXCoreObject& axObject, AXID assumedParentID)
{
    ASSERT(isMainThread());

    auto role = axObject.roleValue();
    if (role == AccessibilityRole::Cell || role == AccessibilityRole::RowHeader || role == AccessibilityRole::ColumnHeader) {
        // Unfortunately, table relationships don't always follow the usual model we build the isolated tree with (a simple
        // live-tree walk, calling children() and setting those children to have a parent of the caller of children()).
        // Overwrite the parentID to be that of the actual parent.
        if (auto* actualParent = axObject.parentObjectUnignored()) {
            // Expect that the parent row has been created by now.
            // If we hit this m_nodeMap.contains ASSERT, we may need to create an isolated object for `actualParent` here or elsewhere.
            ASSERT(m_nodeMap.contains(actualParent->objectID()));
            ASSERT(actualParent->roleValue() == AccessibilityRole::Row);
            return actualParent->objectID();
        }
    }
    return assumedParentID;
}

AXIsolatedTree::NodeChange AXIsolatedTree::nodeChangeForObject(AXCoreObject& axObject, AXID parentID, bool attachWrapper)
{
    ASSERT(isMainThread());

    auto object = AXIsolatedObject::create(axObject, this, parentIDForObject(axObject, parentID));
    NodeChange nodeChange { object, nullptr };

    if (!object->objectID().isValid()) {
        // Either the axObject has an invalid ID or something else went terribly wrong. Don't bother doing anything else.
        ASSERT_NOT_REACHED();
        return nodeChange;
    }

    ASSERT(axObject.wrapper());
    if (attachWrapper)
        object->attachPlatformWrapper(axObject.wrapper());
    else {
        // Set the wrapper in the NodeChange so that it is set on the AX thread.
        nodeChange.wrapper = axObject.wrapper();
    }

    m_nodeMap.set(axObject.objectID(), ParentChildrenIDs { parentID, axObject.childrenIDs() });

    if (!parentID.isValid()) {
        Locker locker { m_changeLogLock };
        setRootNode(nodeChange.isolatedObject.ptr());
    }

    return nodeChange;
}

void AXIsolatedTree::queueChange(const NodeChange& nodeChange)
{
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    m_pendingAppends.append(nodeChange);

    AXID parentID = nodeChange.isolatedObject->parent();
    if (parentID.isValid()) {
        ASSERT_WITH_MESSAGE(m_nodeMap.contains(parentID), "node map should've contained parentID: %s", parentID.loggingString().utf8().data());
        auto siblingsIDs = m_nodeMap.get(parentID).childrenIDs;
        m_pendingChildrenUpdates.append({ parentID, WTFMove(siblingsIDs) });
    }

    AXID objectID = nodeChange.isolatedObject->objectID();
    ASSERT_WITH_MESSAGE(objectID != parentID, "object ID was the same as its parent ID (%s) when queueing a node change", objectID.loggingString().utf8().data());
    ASSERT_WITH_MESSAGE(m_nodeMap.contains(objectID), "node map should've contained objectID: %s", objectID.loggingString().utf8().data());
    auto childrenIDs = m_nodeMap.get(objectID).childrenIDs;
    m_pendingChildrenUpdates.append({ objectID, WTFMove(childrenIDs) });
}

void AXIsolatedTree::queueChangesAndRemovals(const Vector<NodeChange>& changes, const Vector<AXID>& subtreeRemovals)
{
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };

    for (const auto& axID : subtreeRemovals)
        m_pendingSubtreeRemovals.append(axID);

    for (const auto& change : changes)
        queueChange(change);
}

void AXIsolatedTree::collectNodeChangesForSubtree(AXCoreObject& axObject, AXID parentID, bool attachWrapper, Vector<NodeChange>& changes, HashSet<AXID>* idsBeingChanged)
{
    AXTRACE("AXIsolatedTree::collectNodeChangesForSubtree");
    ASSERT(isMainThread());

    auto nodeChange = nodeChangeForObject(axObject, parentID, attachWrapper);
    if (idsBeingChanged)
        idsBeingChanged->add(nodeChange.isolatedObject->objectID());
    changes.append(WTFMove(nodeChange));

    auto axChildrenIDs = axObject.children().map([&](auto& axChild) {
        collectNodeChangesForSubtree(*axChild, axObject.objectID(), attachWrapper, changes, idsBeingChanged);
        return axChild->objectID();
    });
    m_nodeMap.set(axObject.objectID(), ParentChildrenIDs { parentID, WTFMove(axChildrenIDs) });
}

void AXIsolatedTree::updateNode(AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateNode");
    AXLOG(&axObject);
    ASSERT(isMainThread());

    AXID axID = axObject.objectID();
    auto* axParent = axObject.parentObjectUnignored();
    AXID parentID = axParent ? axParent->objectID() : AXID();

    auto change = nodeChangeForObject(axObject, parentID, true);

    // Remove the old object and set the new one to be updated on the AX thread.
    Locker locker { m_changeLogLock };
    m_pendingNodeRemovals.append(axID);
    queueChange(change);
}

void AXIsolatedTree::updateNodeProperty(const AXCoreObject& axObject, AXPropertyName property)
{
    AXTRACE("AXIsolatedTree::updateNodeProperty");
    ASSERT(isMainThread());

    AXPropertyMap propertyMap;
    switch (property) {
    case AXPropertyName::CanSetFocusAttribute:
        propertyMap.set(AXPropertyName::CanSetFocusAttribute, axObject.canSetFocusAttribute());
        break;
    case AXPropertyName::IsChecked:
        propertyMap.set(AXPropertyName::IsChecked, axObject.isChecked());
        break;
    case AXPropertyName::IsEnabled:
        propertyMap.set(AXPropertyName::IsEnabled, axObject.isEnabled());
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
    // has added a new child. So find the closest ancestor of axObject that has
    // an associated isolated object and update its children.
    auto* axAncestor = Accessibility::findAncestor(axObject, true, [this] (auto& ancestor) {
        return m_nodeMap.find(ancestor.objectID()) != m_nodeMap.end();
    });

    if (!axAncestor || !axAncestor->objectID().isValid()) {
        // This update was triggered before the isolated tree has been repopulated.
        // Return here since there is nothing to update.
        AXLOG("Bailing because no ancestor could be found, or ancestor had an invalid objectID");
        return;
    }

#ifndef NDEBUG
    if (axAncestor != &axObject) {
        AXLOG(makeString("Original object with ID ", axObject.objectID().loggingString(), " wasn't in the isolated tree, so instead updating the closest in-isolated-tree ancestor:"));
        AXLOG(axAncestor);
    }
#endif

    auto oldIDs = m_nodeMap.get(axAncestor->objectID());
    auto& oldChildrenIDs = oldIDs.childrenIDs;

    const auto& newChildren = axAncestor->children();
    auto newChildrenIDs = axAncestor->childrenIDs(false);

    Vector<NodeChange> changes;
    HashSet<AXID> idsBeingChanged;
    for (size_t i = 0; i < newChildren.size(); ++i) {
        ASSERT(newChildren[i]->objectID() == newChildrenIDs[i]);
        ASSERT(newChildrenIDs[i].isValid());
        size_t index = oldChildrenIDs.find(newChildrenIDs[i]);
        if (index != notFound)
            oldChildrenIDs.remove(index);
        else {
            // This is a new child, add it to the tree.
            AXLOG(makeString("AXID ", axAncestor->objectID().loggingString(), " gaining new subtree, starting at ID ", newChildren[i]->objectID().loggingString(), ":"));
            AXLOG(newChildren[i]);
            collectNodeChangesForSubtree(*newChildren[i], axAncestor->objectID(), true, changes, &idsBeingChanged);
        }
    }
    m_nodeMap.set(axAncestor->objectID(), ParentChildrenIDs { oldIDs.parentID, newChildrenIDs });

    // What is left in oldChildrenIDs are the IDs that are no longer children of axAncestor.
    // Thus, remove them from m_nodeMap and queue them to be removed from the tree.
    for (AXID& axID : oldChildrenIDs) {
        // However, we don't want to remove subtrees from the nodemap that are part of the to-be-queued node changes (i.e those in `idsBeingChanged`).
        // This is important when a node moves to a different part of the tree rather than being deleted -- for example:
        //   1. Object 123 is slated to be a child of this object (i.e. in newChildren), and we collect node changes for it.
        //   2. Object 123 is currently a member of a subtree of some other object in oldChildrenIDs.
        //   3. Thus, we don't want to delete Object 123 from the nodemap, instead allowing it to be moved.
        removeSubtreeFromNodeMap(axID, axAncestor, idsBeingChanged);
    }
    queueChangesAndRemovals(changes, oldChildrenIDs);
}

RefPtr<AXIsolatedObject> AXIsolatedTree::focusedNode()
{
    AXTRACE("AXIsolatedTree::focusedNode");
    // Apply pending changes in case focus has changed and hasn't been updated.
    applyPendingChanges();
    Locker locker { m_changeLogLock };
    AXLOG(makeString("focusedNodeID ", m_focusedNodeID.loggingString()));
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
    AXLOG(makeString("axID ", axID.loggingString()));
    ASSERT(isMainThread());

    AXPropertyMap propertyMap;
    propertyMap.set(AXPropertyName::IsFocused, true);

    Locker locker { m_changeLogLock };
    m_pendingFocusedNodeID = axID;
    m_pendingPropertyChanges.append({ axID, propertyMap });
}

void AXIsolatedTree::updateLoadingProgress(double newProgressValue)
{
    AXTRACE("AXIsolatedTree::updateLoadingProgress");
    AXLOG(makeString("Queueing loading progress update to ", newProgressValue, " for treeID ", treeID()));
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_pendingLoadingProgress = newProgressValue;
}

void AXIsolatedTree::removeNode(const AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::removeNode");
    AXLOG(makeString("objectID ", axObject.objectID().loggingString()));
    ASSERT(isMainThread());

    removeSubtreeFromNodeMap(axObject.objectID(), axObject.parentObjectUnignored());
    queueChangesAndRemovals({ }, { axObject.objectID() });
}

void AXIsolatedTree::removeSubtreeFromNodeMap(AXID objectID, AXCoreObject* axParent, const HashSet<AXID>& idsToKeep)
{
    AXTRACE("AXIsolatedTree::removeSubtreeFromNodeMap");
    AXLOG(makeString("Removing subtree for objectID ", objectID.loggingString()));
    ASSERT(isMainThread());

    if (!m_nodeMap.contains(objectID)) {
        AXLOG("Tried to remove AXID that is no longer in m_nodeMap.");
        return;
    }

    AXID axParentID = axParent ? axParent->objectID() : AXID();
    if (axParentID != m_nodeMap.get(objectID).parentID) {
        AXLOG(makeString("Tried to remove object from a different parent ", axParentID.loggingString(), ", actual parent ", m_nodeMap.get(objectID).parentID.loggingString(), ", bailing out."));
        return;
    }

    Vector<AXID> removals = { objectID };
    while (removals.size()) {
        AXID axID = removals.takeLast();
        if (!axID.isValid() || idsToKeep.contains(axID))
            continue;

        auto it = m_nodeMap.find(axID);
        if (it != m_nodeMap.end()) {
            removals.appendVector(it->value.childrenIDs);
            m_nodeMap.remove(axID);
        }
    }

    // Update the childrenIDs of the parent since one of its children has been removed.
    if (axParent) {
        auto ids = m_nodeMap.get(axParentID);
        ids.childrenIDs = axParent->childrenIDs();
        m_nodeMap.set(axParentID, WTFMove(ids));
    }
}

void AXIsolatedTree::applyPendingChanges()
{
    AXTRACE("AXIsolatedTree::applyPendingChanges");

    // In isolated tree mode 2, only apply pending changes on the AX thread.
    ASSERT(m_usedOnAXThread ? !isMainThread() : isMainThread());
    if (m_usedOnAXThread && isMainThread())
        return;

    Locker locker { m_changeLogLock };

    m_loadingProgress = m_pendingLoadingProgress;

    if (m_pendingFocusedNodeID != m_focusedNodeID) {
        AXLOG(makeString("focusedNodeID ", m_focusedNodeID.loggingString(), " pendingFocusedNodeID ", m_pendingFocusedNodeID.loggingString()));

        if (m_focusedNodeID.isValid()) {
            // Set the old focused object's IsFocused property to false.
            AXPropertyMap propertyMap;
            propertyMap.set(AXPropertyName::IsFocused, false);
            m_pendingPropertyChanges.append({ m_focusedNodeID, propertyMap });
        }
        m_focusedNodeID = m_pendingFocusedNodeID;
    }

    while (m_pendingNodeRemovals.size()) {
        auto axID = m_pendingNodeRemovals.takeLast();
        AXLOG(makeString("removing axID ", axID.loggingString()));
        if (auto object = nodeForID(axID)) {
            object->detach(AccessibilityDetachmentType::ElementDestroyed);
            m_readerThreadNodeMap.remove(axID);
        }
    }

    while (m_pendingSubtreeRemovals.size()) {
        auto axID = m_pendingSubtreeRemovals.takeLast();
        AXLOG(makeString("removing subtree axID ", axID.loggingString()));
        if (auto object = nodeForID(axID)) {
            object->detach(AccessibilityDetachmentType::ElementDestroyed);
            m_pendingSubtreeRemovals.appendVector(object->m_childrenIDs);
            m_readerThreadNodeMap.remove(axID);
        }
    }

    for (const auto& item : m_pendingAppends) {
        AXID axID = item.isolatedObject->objectID();
        AXLOG(makeString("appending axID ", axID.loggingString()));
        if (!axID.isValid())
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

        // If the new object hasn't been attached to a wrapper yet, or if it was detached from
        // the wrapper when processing removals above, we must attach / re-attach it.
        if (item.isolatedObject->isDetached())
            item.isolatedObject->attachPlatformWrapper(wrapper.get());

        auto addResult = m_readerThreadNodeMap.add(axID, item.isolatedObject.get());
        // The newly added object must have a wrapper.
        ASSERT_UNUSED(addResult, addResult.iterator->value->wrapper());
        // The reference count of the just added IsolatedObject must be 2
        // because it is referenced by m_readerThreadNodeMap and m_pendingAppends.
        // When m_pendingAppends is cleared, the object will be held only by m_readerThreadNodeMap. The exception is the root node whose reference count is 3.
        ASSERT_WITH_MESSAGE(
            addResult.iterator->value->refCount() == 2 || (addResult.iterator->value.ptr() == m_rootNode.get() && m_rootNode->refCount() == 3),
            "unexpected ref count after adding object to m_readerThreadNodeMap: %d", addResult.iterator->value->refCount()
        );
    }
    m_pendingAppends.clear();

    for (auto& update : m_pendingChildrenUpdates) {
        AXLOG(makeString("updating children for axID ", update.first.loggingString()));
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
