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

HashMap<PageIdentifier, Ref<AXIsolatedTree>>& AXIsolatedTree::treePageCache()
{
    static NeverDestroyed<HashMap<PageIdentifier, Ref<AXIsolatedTree>>> map;
    return map;
}

AXIsolatedTree::AXIsolatedTree(AXObjectCache* axObjectCache)
    : m_axObjectCache(axObjectCache)
    , m_usedOnAXThread(axObjectCache->usedOnAXThread())
{
    AXTRACE("AXIsolatedTree::AXIsolatedTree"_s);
    ASSERT(isMainThread());
}

AXIsolatedTree::~AXIsolatedTree()
{
    AXTRACE("AXIsolatedTree::~AXIsolatedTree"_s);
}

void AXIsolatedTree::queueForDestruction()
{
    AXTRACE("AXIsolatedTree::queueForDestruction"_s);
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_queuedForDestruction = true;
}

Ref<AXIsolatedTree> AXIsolatedTree::create(AXObjectCache* axObjectCache)
{
    AXTRACE("AXIsolatedTree::create"_s);
    ASSERT(isMainThread());
    ASSERT(axObjectCache && axObjectCache->pageID());

    auto tree = adoptRef(*new AXIsolatedTree(axObjectCache));

    auto& document = axObjectCache->document();
    if (!document.view()->layoutContext().isInRenderTreeLayout() && !document.inRenderTreeUpdate() && !document.inStyleRecalc())
        document.updateLayoutIgnorePendingStylesheets();

    tree->m_maxTreeDepth = document.settings().maximumHTMLParserDOMTreeDepth();
    ASSERT(tree->m_maxTreeDepth);

    // Generate the nodes of the tree and set its root and focused objects.
    // For this, we need the root and focused objects of the AXObject tree.
    auto* axRoot = axObjectCache->getOrCreate(axObjectCache->document().view());
    if (axRoot)
        tree->generateSubtree(*axRoot);
    auto* axFocus = axObjectCache->focusedObjectForPage(axObjectCache->document().page());
    if (axFocus)
        tree->setFocusedNodeID(axFocus->objectID());

    // Now that the tree is ready to take client requests, add it to the tree
    // maps so that it can be found.
    AXTreeStore::add(tree->treeID(), tree.ptr());
    auto pageID = axObjectCache->pageID();
    Locker locker { s_storeLock };
    ASSERT(!treePageCache().contains(*pageID));
    treePageCache().set(*pageID, tree.copyRef());
    tree->updateLoadingProgress(axObjectCache->loadingProgress());

    return tree;
}

void AXIsolatedTree::removeTreeForPageID(PageIdentifier pageID)
{
    AXTRACE("AXIsolatedTree::removeTreeForPageID"_s);
    ASSERT(isMainThread());

    Locker locker { s_storeLock };
    if (auto tree = treePageCache().take(pageID))
        tree->queueForDestruction();
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForPageID(PageIdentifier pageID)
{
    Locker locker { s_storeLock };
    if (auto tree = treePageCache().get(pageID))
        return tree;
    return nullptr;
}

RefPtr<AXIsolatedObject> AXIsolatedTree::nodeForID(const AXID& axID) const
{
    // In isolated tree mode 2, only access m_readerThreadNodeMap on the AX thread.
    ASSERT(m_usedOnAXThread ? !isMainThread() : isMainThread());
    if (m_usedOnAXThread && isMainThread())
        return nullptr;

    return axID.isValid() ? m_readerThreadNodeMap.get(axID) : nullptr;
}

Vector<RefPtr<AXCoreObject>> AXIsolatedTree::objectsForIDs(const Vector<AXID>& axIDs)
{
    AXTRACE("AXIsolatedTree::objectsForIDs"_s);
    ASSERT(!isMainThread());

    Vector<RefPtr<AXCoreObject>> result;
    result.reserveInitialCapacity(axIDs.size());
    for (auto& axID : axIDs) {
        auto object = nodeForID(axID);
        if (object) {
            result.uncheckedAppend(object);
            continue;
        }

        // There is no isolated object for this AXID. This can happen if the corresponding live object is ignored.
        // If there is a live object for this ID and it is an ignored target of a relationship, create an isolated object for it.
        object = Accessibility::retrieveValueFromMainThread<RefPtr<AXIsolatedObject>>([&axID, this] () -> RefPtr<AXIsolatedObject> {
            auto* cache = axObjectCache();
            if (!cache || !cache->relationTargetIDs().contains(axID))
                return nullptr;

            RefPtr axObject = cache->objectForID(axID);
            if (!axObject || !axObject->accessibilityIsIgnored())
                return nullptr;

            auto object = AXIsolatedObject::create(*axObject, const_cast<AXIsolatedTree*>(this));
            ASSERT(axObject->wrapper());
            object->attachPlatformWrapper(axObject->wrapper());
            return object;
        });
        if (object) {
            m_readerThreadNodeMap.add(axID, *object);
            result.uncheckedAppend(object);
        }
    }
    result.shrinkToFit();
    return result;
}

void AXIsolatedTree::generateSubtree(AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::generateSubtree"_s);
    ASSERT(isMainThread());

    if (axObject.isDetached())
        return;

    collectNodeChangesForSubtree(axObject);
    queueRemovalsAndUnresolvedChanges({ });
}

static bool shouldCreateNodeChange(AXCoreObject& axObject)
{
    // We should never create an isolated object from a detached or ignored object.
    return !axObject.isDetached() && !axObject.accessibilityIsIgnored();
}

std::optional<AXIsolatedTree::NodeChange> AXIsolatedTree::nodeChangeForObject(Ref<AccessibilityObject> axObject, AttachWrapper attachWrapper)
{
    ASSERT(isMainThread());
    ASSERT(!axObject->isDetached());

    if (!shouldCreateNodeChange(axObject.get()))
        return std::nullopt;

    auto object = AXIsolatedObject::create(axObject, this);
    NodeChange nodeChange { object, nullptr };

    ASSERT(axObject->wrapper());
    if (attachWrapper == AttachWrapper::OnMainThread)
        object->attachPlatformWrapper(axObject->wrapper());
    else {
        // Set the wrapper in the NodeChange so that it is set on the AX thread.
        nodeChange.wrapper = axObject->wrapper();
    }

    m_nodeMap.set(axObject->objectID(), ParentChildrenIDs { nodeChange.isolatedObject->parent(), axObject->childrenIDs() });

    if (!nodeChange.isolatedObject->parent().isValid() && nodeChange.isolatedObject->isScrollView()) {
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
        auto siblingsIDs = m_nodeMap.get(parentID).childrenIDs;
        m_pendingChildrenUpdates.append({ parentID, WTFMove(siblingsIDs) });
    }

    AXID objectID = nodeChange.isolatedObject->objectID();
    ASSERT_WITH_MESSAGE(objectID != parentID, "object ID was the same as its parent ID (%s) when queueing a node change", objectID.loggingString().utf8().data());
    ASSERT_WITH_MESSAGE(m_nodeMap.contains(objectID), "node map should've contained objectID: %s", objectID.loggingString().utf8().data());
    auto childrenIDs = m_nodeMap.get(objectID).childrenIDs;
    m_pendingChildrenUpdates.append({ objectID, WTFMove(childrenIDs) });
}

void AXIsolatedTree::addUnconnectedNode(Ref<AccessibilityObject> axObject)
{
    AXTRACE("AXIsolatedTree::addUnconnectedNode"_s);
    ASSERT(isMainThread());

    if (axObject->isDetached() || !axObject->wrapper()) {
        AXLOG(makeString("AXIsolatedTree::addUnconnectedNode bailing because associated live object ID ", axObject->objectID().loggingString(), " had no wrapper or is detached. Object is:"));
        AXLOG(axObject.ptr());
        return;
    }
    AXLOG(makeString("AXIsolatedTree::addUnconnectedNode creating isolated object from live object ID ", axObject->objectID().loggingString()));

    // Because we are queuing a change for an object not intended to be connected to the rest of the tree,
    // we don't need to update m_nodeMap or m_pendingChildrenUpdates for this object or its parent as is
    // done in AXIsolatedTree::nodeChangeForObject and AXIsolatedTree::queueChange.
    //
    // Instead, just directly create and queue the node change so m_readerThreadNodeMap can hold a reference
    // to it. It will be removed from m_readerThreadNodeMap when the corresponding DOM element, renderer, or
    // other entity is removed from the page.
    auto object = AXIsolatedObject::create(axObject, this);
    object->attachPlatformWrapper(axObject->wrapper());

    NodeChange nodeChange { object, nullptr };
    Locker locker { m_changeLogLock };
    m_pendingAppends.append(WTFMove(nodeChange));
}

void AXIsolatedTree::queueRemovals(Vector<AXID>&& subtreeRemovals)
{
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    queueRemovalsLocked(WTFMove(subtreeRemovals));
}

void AXIsolatedTree::queueRemovalsLocked(Vector<AXID>&& subtreeRemovals)
{
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    m_pendingSubtreeRemovals.appendVector(WTFMove(subtreeRemovals));
}

void AXIsolatedTree::queueRemovalsAndUnresolvedChanges(Vector<AXID>&& subtreeRemovals)
{
    ASSERT(isMainThread());

    Vector<NodeChange> resolvedAppends;
    if (!m_unresolvedPendingAppends.isEmpty()) {
        if (auto* cache = axObjectCache()) {
            resolvedAppends.reserveInitialCapacity(m_unresolvedPendingAppends.size());
            for (const auto& unresolvedAppend : m_unresolvedPendingAppends) {
                if (auto* axObject = cache->objectForID(unresolvedAppend.key)) {
                    if (auto nodeChange = nodeChangeForObject(*axObject, unresolvedAppend.value))
                        resolvedAppends.uncheckedAppend(WTFMove(*nodeChange));
                }
            }
            m_unresolvedPendingAppends.clear();
        }
    }

    Locker locker { m_changeLogLock };
    for (const auto& resolvedAppend : resolvedAppends)
        queueChange(resolvedAppend);
    queueRemovalsLocked(WTFMove(subtreeRemovals));
}

void AXIsolatedTree::collectNodeChangesForSubtree(AXCoreObject& axObject)
{
    AXTRACE("AXIsolatedTree::collectNodeChangesForSubtree"_s);
    AXLOG(axObject);
    ASSERT(isMainThread());

    if (axObject.isDetached()) {
        AXLOG("Can't build an isolated tree branch rooted at a detached object.");
        return;
    }

    SetForScope collectingAtTreeLevel(m_collectingNodeChangesAtTreeLevel, m_collectingNodeChangesAtTreeLevel + 1);
    if (m_collectingNodeChangesAtTreeLevel >= m_maxTreeDepth)
        return;

    m_unresolvedPendingAppends.set(axObject.objectID(), AttachWrapper::OnMainThread);
    auto axChildrenCopy = axObject.children();
    Vector<AXID> axChildrenIDs;
    axChildrenIDs.reserveInitialCapacity(axChildrenCopy.size());
    for (const auto& axChild : axChildrenCopy) {
        if (!axChild || axChild.get() == &axObject) {
            ASSERT_NOT_REACHED();
            continue;
        }

        axChildrenIDs.uncheckedAppend(axChild->objectID());
        collectNodeChangesForSubtree(*axChild);
    }
    axChildrenIDs.shrinkToFit();

    // Update the m_nodeMap.
    auto* axParent = axObject.parentObjectUnignored();
    m_nodeMap.set(axObject.objectID(), ParentChildrenIDs { axParent ? axParent->objectID() : AXID(), WTFMove(axChildrenIDs) });
}

void AXIsolatedTree::updateNode(AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateNode"_s);
    AXLOG(&axObject);
    ASSERT(isMainThread());

    // If we update a node as the result of some side effect while collecting node changes (e.g. a role change from
    // AccessibilityRenderObject::updateRoleAfterChildrenCreation), queue the append up to be resolved with the rest
    // of the collected changes. This prevents us from creating two node changes for the same object.
    if (isCollectingNodeChanges()) {
        m_unresolvedPendingAppends.set(axObject.objectID(), AttachWrapper::OnAXThread);
        return;
    }

    // Otherwise, resolve the change immediately and queue it up.
    // In both cases, we can't attach the wrapper immediately on the main thread, since the wrapper could be in use
    // on the AX thread (because this function updates an existing node).
    if (auto change = nodeChangeForObject(axObject, AttachWrapper::OnAXThread)) {
        Locker locker { m_changeLogLock };
        queueChange(WTFMove(*change));
        return;
    }

    // Not able to update axObject. This may be because it is a descendant of a barren object such as a button. In that case, try to update its parent.
    if (!axObject.isDescendantOfBarrenParent())
        return;

    auto* axParent = axObject.parentObjectUnignored();
    if (!axParent)
        return;

    if (auto change = nodeChangeForObject(*axParent, AttachWrapper::OnAXThread)) {
        Locker locker { m_changeLogLock };
        queueChange(WTFMove(*change));
    }
}

void AXIsolatedTree::updatePropertiesForSelfAndDescendants(AccessibilityObject& axObject, const Vector<AXPropertyName>& properties)
{
    ASSERT(isMainThread());

    Accessibility::enumerateDescendants<AXCoreObject>(axObject, true, [&properties, this] (auto& descendant) {
        updateNodeProperties(descendant, properties);
    });
}

void AXIsolatedTree::updateNodeProperties(AXCoreObject& axObject, const Vector<AXPropertyName>& properties)
{
    AXTRACE("AXIsolatedTree::updateNodeProperties"_s);
    AXLOG(makeString("Updating properties ", properties, " for objectID ", axObject.objectID().loggingString()));
    ASSERT(isMainThread());

    AXPropertyMap propertyMap;
    for (const auto& property : properties) {
        switch (property) {
        case AXPropertyName::ARIATreeItemContent:
            propertyMap.set(AXPropertyName::ARIATreeItemContent, axIDs(axObject.ariaTreeItemContent()));
            break;
        case AXPropertyName::ARIATreeRows: {
            AXCoreObject::AccessibilityChildrenVector ariaTreeRows;
            axObject.ariaTreeRows(ariaTreeRows);
            propertyMap.set(AXPropertyName::ARIATreeRows, axIDs(ariaTreeRows));
            break;
        }
        case AXPropertyName::ValueAutofillButtonType:
            propertyMap.set(AXPropertyName::ValueAutofillButtonType, static_cast<int>(axObject.valueAutofillButtonType()));
            propertyMap.set(AXPropertyName::IsValueAutofillAvailable, axObject.isValueAutofillAvailable());
            break;
        case AXPropertyName::AXColumnCount:
            propertyMap.set(AXPropertyName::AXColumnCount, axObject.axColumnCount());
            break;
        case AXPropertyName::ColumnHeaders:
            propertyMap.set(AXPropertyName::ColumnHeaders, axIDs(axObject.columnHeaders()));
            break;
        case AXPropertyName::AXColumnIndex:
            propertyMap.set(AXPropertyName::AXColumnIndex, axObject.axColumnIndex());
            break;
        case AXPropertyName::CanSetFocusAttribute:
            propertyMap.set(AXPropertyName::CanSetFocusAttribute, axObject.canSetFocusAttribute());
            break;
        case AXPropertyName::CanSetValueAttribute:
            propertyMap.set(AXPropertyName::CanSetValueAttribute, axObject.canSetValueAttribute());
            break;
        case AXPropertyName::CurrentState:
            propertyMap.set(AXPropertyName::CurrentState, static_cast<int>(axObject.currentState()));
            break;
        case AXPropertyName::DisclosedRows:
            propertyMap.set(AXPropertyName::DisclosedRows, axIDs(axObject.disclosedRows()));
            break;
        case AXPropertyName::IdentifierAttribute:
            propertyMap.set(AXPropertyName::IdentifierAttribute, axObject.identifierAttribute().isolatedCopy());
            break;
        case AXPropertyName::IsChecked:
            ASSERT(axObject.supportsCheckedState());
            propertyMap.set(AXPropertyName::IsChecked, axObject.isChecked());
            propertyMap.set(AXPropertyName::ButtonState, axObject.checkboxOrRadioValue());
            break;
        case AXPropertyName::IsEnabled:
            propertyMap.set(AXPropertyName::IsEnabled, axObject.isEnabled());
            break;
        case AXPropertyName::IsExpanded:
            propertyMap.set(AXPropertyName::IsExpanded, axObject.isExpanded());
            break;
        case AXPropertyName::IsRequired:
            propertyMap.set(AXPropertyName::IsRequired, axObject.isRequired());
            break;
        case AXPropertyName::IsSelected:
            propertyMap.set(AXPropertyName::IsSelected, axObject.isSelected());
            break;
        case AXPropertyName::MaxValueForRange:
            propertyMap.set(AXPropertyName::MaxValueForRange, axObject.maxValueForRange());
            break;
        case AXPropertyName::MinValueForRange:
            propertyMap.set(AXPropertyName::MinValueForRange, axObject.minValueForRange());
            break;
        case AXPropertyName::Orientation:
            propertyMap.set(AXPropertyName::Orientation, static_cast<int>(axObject.orientation()));
            break;
        case AXPropertyName::PosInSet:
            propertyMap.set(AXPropertyName::PosInSet, axObject.posInSet());
            break;
        case AXPropertyName::ReadOnlyValue:
            propertyMap.set(AXPropertyName::ReadOnlyValue, axObject.readOnlyValue().isolatedCopy());
            break;
        case AXPropertyName::RoleDescription:
            propertyMap.set(AXPropertyName::RoleDescription, axObject.roleDescription().isolatedCopy());
            break;
        case AXPropertyName::AXRowIndex:
            propertyMap.set(AXPropertyName::AXRowIndex, axObject.axRowIndex());
            break;
        case AXPropertyName::SetSize:
            propertyMap.set(AXPropertyName::SetSize, axObject.setSize());
            break;
        case AXPropertyName::SortDirection:
            propertyMap.set(AXPropertyName::SortDirection, static_cast<int>(axObject.sortDirection()));
            break;
        case AXPropertyName::KeyShortcuts:
            propertyMap.set(AXPropertyName::SupportsKeyShortcuts, axObject.supportsKeyShortcuts());
            propertyMap.set(AXPropertyName::KeyShortcuts, axObject.keyShortcuts().isolatedCopy());
            break;
        case AXPropertyName::SupportsPosInSet:
            propertyMap.set(AXPropertyName::SupportsPosInSet, axObject.supportsPosInSet());
            break;
        case AXPropertyName::SupportsSetSize:
            propertyMap.set(AXPropertyName::SupportsSetSize, axObject.supportsSetSize());
            break;
        case AXPropertyName::URL:
            propertyMap.set(AXPropertyName::URL, axObject.url().isolatedCopy());
            break;
        case AXPropertyName::ValueForRange:
            propertyMap.set(AXPropertyName::ValueForRange, axObject.valueForRange());
            break;
        default:
            break;
        }
    }

    if (propertyMap.isEmpty())
        return;

    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axObject.objectID(), propertyMap });
}

void AXIsolatedTree::updateNodeAndDependentProperties(AccessibilityObject& axObject)
{
    ASSERT(isMainThread());

    updateNode(axObject);

    if (auto* treeAncestor = Accessibility::findAncestor(axObject, true, [] (const auto& object) { return object.isTree(); }))
        updateNodeProperty(*treeAncestor, AXPropertyName::ARIATreeRows);
}

void AXIsolatedTree::updateChildren(AccessibilityObject& axObject, ResolveNodeChanges resolveNodeChanges)
{
    AXTRACE("AXIsolatedTree::updateChildren"_s);
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

    if (!axAncestor || axAncestor->isDetached()) {
        // This update was triggered before the isolated tree has been repopulated.
        // Return here since there is nothing to update.
        AXLOG("Bailing because no ancestor could be found, or ancestor is detached");
        return;
    }

    if (axAncestor != &axObject) {
        AXLOG(makeString("Original object with ID ", axObject.objectID().loggingString(), " wasn't in the isolated tree, so instead updating the closest in-isolated-tree ancestor:"));
        AXLOG(axAncestor);

        // An explicit copy is necessary here because the nested calls to updateChildren
        // can cause this objects children to be invalidated as we iterate.
        auto childrenCopy = axObject.children();
        for (auto& child : childrenCopy) {
            Ref liveChild = downcast<AccessibilityObject>(*child);
            if (liveChild->childrenInitialized())
                continue;

            if (!m_nodeMap.contains(liveChild->objectID())) {
                if (!shouldCreateNodeChange(liveChild))
                    continue;

                // This child should be added to the isolated tree but hasn't been yet.
                // Add it to the nodemap so the recursive call to updateChildren below properly builds the subtree for this object.
                auto* parent = liveChild->parentObjectUnignored();
                m_nodeMap.set(liveChild->objectID(), ParentChildrenIDs { parent ? parent->objectID() : AXID(), liveChild->childrenIDs() });
                m_unresolvedPendingAppends.set(liveChild->objectID(), AttachWrapper::OnMainThread);
            }

            AXLOG(makeString(
                "Child ID ", liveChild->objectID().loggingString(), " of original object ID ", axObject.objectID().loggingString(), " was found in the isolated tree with uninitialized live children. Updating its isolated children."
            ));
            // Don't immediately resolve node changes in these recursive calls to updateChildren. This avoids duplicate node change creation in this scenario:
            //   1. Some subtree is updated in the below call to updateChildren.
            //   2. Later in this function, when updating axAncestor, we update some higher subtree that includes the updated subtree from step 1.
            updateChildren(liveChild, ResolveNodeChanges::No);
        }
    }

    auto oldIDs = m_nodeMap.get(axAncestor->objectID());
    auto& oldChildrenIDs = oldIDs.childrenIDs;

    const auto& newChildren = axAncestor->children();
    auto newChildrenIDs = axAncestor->childrenIDs(false);

    for (size_t i = 0; i < newChildren.size(); ++i) {
        ASSERT(newChildren[i]->objectID() == newChildrenIDs[i]);
        ASSERT(newChildrenIDs[i].isValid());
        size_t index = oldChildrenIDs.find(newChildrenIDs[i]);
        if (index != notFound) {
            // Prevent deletion of this object below by removing it from oldChildrenIDs.
            oldChildrenIDs.remove(index);

            // Propagate any subtree updates downwards for this already-existing child.
            if (auto* liveChild = dynamicDowncast<AccessibilityObject>(newChildren[i].get()); liveChild && liveChild->hasDirtySubtree())
                collectNodeChangesForSubtree(*liveChild);
        }
        else {
            // This is a new child, add it to the tree.
            AXLOG(makeString("AXID ", axAncestor->objectID().loggingString(), " gaining new subtree, starting at ID ", newChildren[i]->objectID().loggingString(), ":"));
            AXLOG(newChildren[i]);
            collectNodeChangesForSubtree(*newChildren[i]);
        }
    }
    m_nodeMap.set(axAncestor->objectID(), ParentChildrenIDs { oldIDs.parentID, WTFMove(newChildrenIDs) });

    // What is left in oldChildrenIDs are the IDs that are no longer children of axAncestor.
    // Thus, remove them from m_nodeMap and queue them to be removed from the tree.
    for (const AXID& axID : oldChildrenIDs)
        removeSubtreeFromNodeMap(axID, axAncestor);

    if (resolveNodeChanges == ResolveNodeChanges::Yes)
        queueRemovalsAndUnresolvedChanges(WTFMove(oldChildrenIDs));
    else
        queueRemovals(WTFMove(oldChildrenIDs));

    // Also queue updates to the target node itself and any properties that depend on children().
    updateNodeAndDependentProperties(*axAncestor);
}

RefPtr<AXIsolatedObject> AXIsolatedTree::focusedNode()
{
    AXTRACE("AXIsolatedTree::focusedNode"_s);
    ASSERT(!isMainThread());
    // applyPendingChanges can destroy `this` tree, so protect it until the end of this method.
    Ref protectedThis { *this };
    // Apply pending changes in case focus has changed and hasn't been updated.
    applyPendingChanges();
    AXLOG(makeString("focusedNodeID ", m_focusedNodeID.loggingString()));
    AXLOG("focused node:");
    AXLOG(nodeForID(m_focusedNodeID));
    return nodeForID(m_focusedNodeID);
}

RefPtr<AXIsolatedObject> AXIsolatedTree::rootNode()
{
    AXTRACE("AXIsolatedTree::rootNode"_s);
    Locker locker { m_changeLogLock };
    return m_rootNode;
}

void AXIsolatedTree::setRootNode(AXIsolatedObject* root)
{
    AXTRACE("AXIsolatedTree::setRootNode"_s);
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());
    ASSERT(!m_rootNode);
    ASSERT(root);

    m_rootNode = root;
}

void AXIsolatedTree::setFocusedNodeID(AXID axID)
{
    AXTRACE("AXIsolatedTree::setFocusedNodeID"_s);
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
    AXTRACE("AXIsolatedTree::updateLoadingProgress"_s);
    AXLOG(makeString("Updating loading progress to ", newProgressValue, " for treeID ", treeID().loggingString()));
    ASSERT(isMainThread());

    m_loadingProgress = newProgressValue;
}

void AXIsolatedTree::removeNode(const AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::removeNode"_s);
    AXLOG(makeString("objectID ", axObject.objectID().loggingString()));
    ASSERT(isMainThread());

    m_unresolvedPendingAppends.remove(axObject.objectID());
    removeSubtreeFromNodeMap(axObject.objectID(), axObject.parentObjectUnignored());
    queueRemovals({ axObject.objectID() });
}

void AXIsolatedTree::removeSubtreeFromNodeMap(AXID objectID, AccessibilityObject* axParent)
{
    AXTRACE("AXIsolatedTree::removeSubtreeFromNodeMap"_s);
    AXLOG(makeString("Removing subtree for objectID ", objectID.loggingString()));
    ASSERT(isMainThread());

    if (!objectID.isValid())
        return;

    if (!m_nodeMap.contains(objectID)) {
        AXLOG(makeString("Tried to remove AXID ", objectID.loggingString(), " that is no longer in m_nodeMap."));
        return;
    }

    AXID axParentID = axParent ? axParent->objectID() : AXID();
    if (axParentID != m_nodeMap.get(objectID).parentID) {
        AXLOG(makeString("Tried to remove object ID ", objectID.loggingString(), " from a different parent ", axParentID.loggingString(), ", actual parent ", m_nodeMap.get(objectID).parentID.loggingString(), ", bailing out."));
        return;
    }

    Vector<AXID> removals = { objectID };
    while (removals.size()) {
        AXID axID = removals.takeLast();
        if (!axID.isValid() || m_unresolvedPendingAppends.contains(axID))
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

std::optional<Vector<AXID>> AXIsolatedTree::relatedObjectIDsFor(const AXCoreObject& object, AXRelationType relationType)
{
    ASSERT(!isMainThread());

    if (m_relationsNeedUpdate) {
        m_relations = Accessibility::retrieveValueFromMainThread<HashMap<AXID, AXRelations>>([this] () -> HashMap<AXID, AXRelations> {
            if (auto* cache = axObjectCache())
                return cache->relations();
            return { };
        });
        m_relationsNeedUpdate = false;
    }

    auto relationsIterator = m_relations.find(object.objectID());
    if (relationsIterator == m_relations.end())
        return std::nullopt;

    auto targetsIterator = relationsIterator->value.find(static_cast<uint8_t>(relationType));
    if (targetsIterator == relationsIterator->value.end())
        return std::nullopt;
    return targetsIterator->value;
}

void AXIsolatedTree::applyPendingChanges()
{
    AXTRACE("AXIsolatedTree::applyPendingChanges"_s);

    // In isolated tree mode 2, only apply pending changes on the AX thread.
    ASSERT(m_usedOnAXThread ? !isMainThread() : isMainThread());
    if (m_usedOnAXThread && isMainThread())
        return;

    Locker locker { m_changeLogLock };

    if (UNLIKELY(m_queuedForDestruction)) {
        // Protect this until we have fully self-destructed.
        Ref protectedThis { *this };

        for (const auto& object : m_readerThreadNodeMap.values())
            object->detach(AccessibilityDetachmentType::CacheDestroyed);

        // Because each AXIsolatedObject holds a RefPtr to this tree, clear out any member variable
        // that holds an AXIsolatedObject so the ref-cycle is broken and this tree can be destroyed.
        m_readerThreadNodeMap.clear();
        m_rootNode = nullptr;
        m_pendingAppends.clear();
        // We don't need to bother clearing out any other non-cycle-causing member variables as they
        // will be cleaned up automatically when the tree is destroyed.

        ASSERT(AXTreeStore::contains(treeID()));
        AXTreeStore::remove(treeID());
        return;
    }

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

    while (m_pendingSubtreeRemovals.size()) {
        auto axID = m_pendingSubtreeRemovals.takeLast();
        AXLOG(makeString("removing subtree axID ", axID.loggingString()));
        if (RefPtr object = nodeForID(axID)) {
            // There's no need to call the more comprehensive AXCoreObject::detach here since
            // we're deleting the entire subtree of this object and thus don't need to `detachRemoteParts`.
            object->detachWrapper(AccessibilityDetachmentType::ElementDestroyed);
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

        if (auto existingObject = m_readerThreadNodeMap.get(axID)) {
            if (existingObject != &item.isolatedObject.get()
                && existingObject->wrapper() == wrapper.get()) {
                // The new IsolatedObject is a replacement for an existing object
                // as the result of an update. Thus detach the existing object
                // and attach the wrapper to the new one.
                existingObject->detach(AccessibilityDetachmentType::ElementChanged);
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
