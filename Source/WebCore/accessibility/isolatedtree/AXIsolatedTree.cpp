/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#include "AXAttributeCacheScope.h"
#include "AXGeometryManager.h"
#include "AXIsolatedObject.h"
#include "AXLocalFrame.h"
#include "AXLogger.h"
#include "AXNotifications.h"
#include "AXObjectCacheInlines.h"
#include "AXTreeStore.h"
#include "AXTreeStoreInlines.h"
#include "AXUtilities.h"
#include "AccessibilityObjectInlines.h"
#include "AccessibilityNodeObject.h"
#include "DocumentPage.h"
#include "DocumentView.h"
#include "FrameSelection.h"
#include "HTMLNames.h"
#include "LocalFrameView.h"
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

using namespace HTMLNames;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXIsolatedTree);
WTF_MAKE_TZONE_ALLOCATED_IMPL(AXIsolatedTree);
WTF_MAKE_TZONE_ALLOCATED_IMPL(AXIDAndCharacterRange);

static const Seconds CreationFeedbackInterval { 3_s };

HashMap<FrameIdentifier, Ref<AXIsolatedTree>>& AXIsolatedTree::treeFrameCache()
{
    static NeverDestroyed<HashMap<FrameIdentifier, Ref<AXIsolatedTree>>> map;
    return map;
}

AXIsolatedTree::AXIsolatedTree(AXObjectCache& axObjectCache)
    : AXTreeStore(axObjectCache.treeID())
    , m_axObjectCache(&axObjectCache)
    , m_geometryManager(axObjectCache.m_geometryManager.ptr())
    , m_pageActivityState(axObjectCache.pageActivityState())
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

Ref<AXIsolatedTree> AXIsolatedTree::createEmpty(AXObjectCache& axObjectCache)
{
    AXTRACE("AXIsolatedTree::createEmpty"_s);
    ASSERT(isMainThread());
    ASSERT(axObjectCache.frameID());

    auto tree = adoptRef(*new AXIsolatedTree(axObjectCache));

    if (RefPtr axRoot = axObjectCache.document() ? axObjectCache.getOrCreate(axObjectCache.document()->view()) : nullptr) {
        tree->updatingSubtree(axRoot.get());
        tree->createEmptyContent(*axRoot);
    }

    tree->updateLoadingProgress(axObjectCache.loadingProgress());
    tree->m_processingProgress = 0;

    // Now that the tree is ready to take client requests, add it to the tree maps so that it can be found.
    storeTree(axObjectCache, tree);

    return tree;
}

void AXIsolatedTree::createEmptyContent(AccessibilityObject& axRoot)
{
    ASSERT(isMainThread());
    ASSERT(!axRoot.isDetached());
    ASSERT(axRoot.isScrollView() && !axRoot.parentObject());

    // An empty content tree consists only of the ScrollView and WebArea objects.
    m_isEmptyContentTree = true;

    // Create the IsolatedObjects for the root/ScrollView and WebArea.
    auto rootData = createIsolatedObjectData(axRoot, *this);
    rootData.setProperty(AXProperty::ScreenRelativePosition, axRoot.screenRelativePosition());
    NodeChange rootAppend { WTFMove(rootData), axRoot.wrapper() };

    RefPtr axWebArea = Accessibility::findUnignoredChild(axRoot, [] (auto& object) {
        return object->isWebArea();
    });
    if (!axWebArea) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto webAreaData = createIsolatedObjectData(*axWebArea, *this);
    webAreaData.setProperty(AXProperty::ScreenRelativePosition, axWebArea->screenRelativePosition());
    NodeChange webAreaAppend { WTFMove(webAreaData), axWebArea->wrapper() };

    m_nodeMap.set(axRoot.objectID(), ParentChildrenIDs { std::nullopt, { axWebArea->objectID() } });
    m_nodeMap.set(axWebArea->objectID(), ParentChildrenIDs { axRoot.objectID(), { } });

    {
        Locker locker { m_changeLogLock };
        setPendingRootNodeIDLocked(axRoot.objectID());
        m_pendingFocusedNodeID = axWebArea->objectID();
    }
    Vector<NodeChange> appends;
    appends.reserveInitialCapacity(2);
    appends.append(WTFMove(rootAppend));
    appends.append(WTFMove(webAreaAppend));
    queueAppendsAndRemovals(WTFMove(appends), { });
}

RefPtr<AXIsolatedTree> AXIsolatedTree::create(AXObjectCache& axObjectCache)
{
    AXTRACE("AXIsolatedTree::create"_s);
    ASSERT(isMainThread());
    ASSERT(axObjectCache.frameID());

    auto tree = adoptRef(*new AXIsolatedTree(axObjectCache));
    if (RefPtr existingTree = isolatedTreeForID(tree->treeID()))
        tree->m_replacingTree = existingTree;

    RefPtr document = axObjectCache.document();
    if (!document)
        return nullptr;

    if (AXObjectCache::isAppleInternalInstall()) [[unlikely]]
        WTFBeginSignpostAlways(tree.ptr(), InitialAccessibilityIsolatedTreeBuild, "building isolated tree for AXObjectCache: %" PRIVATE_LOG_STRING ", is the top-document: %d", axObjectCache.debugDescription().utf8().data(), document->isTopDocument());

    if (!Accessibility::inRenderTreeOrStyleUpdate(*document))
        document->updateLayoutIgnorePendingStylesheets();

    // Generate the nodes of the tree and set its root and focused objects.
    // For this, we need the root and focused objects of the AXObject tree.
    RefPtr axRoot = axObjectCache.getOrCreate(document->view());
    if (axRoot)
        tree->generateSubtree(*axRoot);

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    RefPtr axFocus = axObjectCache.focusedObjectForLocalFrame();
#else
    RefPtr axFocus = axObjectCache.focusedObjectForPage(document->page());
#endif
    if (axFocus)
        tree->setFocusedNodeID(axFocus->objectID());
    tree->setSelectedTextMarkerRange(document->selection().selection());
    tree->setInitialSortedLiveRegions(axIDs(axObjectCache.sortedLiveRegions()));
    tree->setInitialSortedNonRootWebAreas(axIDs(axObjectCache.sortedNonRootWebAreas()));
    tree->updateLoadingProgress(axObjectCache.loadingProgress());

    auto relations = axObjectCache.relations();
    for (auto& relatedObjectID : relations.keys()) {
        RefPtr axObject = axObjectCache.objectForID(relatedObjectID);
        if (axObject && axObject->isIgnored())
            tree->addUnconnectedNode(axObject.releaseNonNull());
    }
    tree->updateRelations(WTFMove(relations));

    // Now that the tree is ready to take client requests, add it to the tree maps so that it can be found.
    storeTree(axObjectCache, tree);

    if (AXObjectCache::isAppleInternalInstall()) [[unlikely]]
        WTFEndSignpostAlways(tree.ptr(), InitialAccessibilityIsolatedTreeBuild);
    return tree;
}

void AXIsolatedTree::applyPendingRootNodeLocked()
{
    ASSERT(!isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    if (m_pendingRootNodeID) {
        if (RefPtr root = objectForID(m_pendingRootNodeID)) {
            m_rootNode = WTFMove(root);
            m_pendingRootNodeID = std::nullopt;

#if ASSERT_ENABLED
            auto markReachableNodes = [](AXCoreObject* object, HashSet<AXID>& reachableNodes, auto& self) -> void {
                reachableNodes.add(object->objectID());
                for (auto& child : object->children())
                    self(&child.get(), reachableNodes, self);
            };
            HashSet<AXID> reachableNodes;
            if (m_rootNode) {
                markReachableNodes(m_rootNode.get(), reachableNodes, markReachableNodes);
                ASSERT_WITH_MESSAGE(reachableNodes.size() == m_readerThreadNodeMap.size(), "AX: After applying pending root node, %u reachable nodes but %u are in the node map", reachableNodes.size(), m_readerThreadNodeMap.size());
            }
#endif
        }
    }
}

void AXIsolatedTree::storeTree(AXObjectCache& cache, const Ref<AXIsolatedTree>& tree)
{
    ASSERT(isMainThread());

    // Once we've added this new tree to the AXTreeStore, clients will be able to use
    // it off the main-thread. Make any final state mutations while we are the only thread
    // that can touch this tree.
    cache.setIsolatedTree(tree);
    AXTreeStore::set(tree->treeID(), tree.ptr());
    tree->m_replacingTree = nullptr;
    Locker locker { s_storeLock };
    treeFrameCache().set(*cache.frameID(), tree.copyRef());
}

double AXIsolatedTree::loadingProgress()
{
    return .50 * m_loadingProgress + .50 * m_processingProgress;
}

void AXIsolatedTree::reportLoadingProgress(double processingProgress)
{
    AXTRACE("AXIsolatedTree::reportLoadingProgress"_s);
    ASSERT(isMainThread());

    if (!isEmptyContentTree()) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_processingProgress = processingProgress;
    String title = AXProcessingPage(loadingProgress());
    AXLOG(title);

    WeakPtr cache = axObjectCache();
    if (RefPtr axWebArea = cache ? cache->rootWebArea() : nullptr) {
        AXPropertyVector properties;
        properties.append({ AXProperty::WebAreaTitle, WTFMove(title) });
        overrideNodeProperties(axWebArea->objectID(), WTFMove(properties));
        if (cache)
            cache->postPlatformNotification(*axWebArea, AXNotification::LayoutComplete);
    }
}

void AXIsolatedTree::removeTreeForFrameID(FrameIdentifier frameID)
{
    AXTRACE("AXIsolatedTree::removeTreeForFrameID"_s);
    ASSERT(isMainThread());

    Locker locker { s_storeLock };
    if (RefPtr tree = treeFrameCache().take(frameID)) {
        tree->m_geometryManager = nullptr;
        tree->queueForDestruction();
    }
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForFrameID(FrameIdentifier frameID)
{
    Locker locker { s_storeLock };
    return treeFrameCache().get(frameID);
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForFrameIDAlreadyLocked(FrameIdentifier frameID)
{
    return treeFrameCache().get(frameID);
}

void AXIsolatedTree::generateSubtree(AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::generateSubtree"_s);
    ASSERT(isMainThread());

    if (axObject.isDetached())
        return;

    // We're about to a lot of read-only work, so start the attribute cache.
    AXAttributeCacheScope enableCache(axObject.axObjectCache());
    collectNodeChangesForSubtree(axObject);
    queueRemovalsAndUnresolvedChanges();
}

bool AXIsolatedTree::shouldCreateNodeChange(AccessibilityObject& axObject)
{
    // We should never create an isolated object from a detached or ignored object, unless we aren't
    // enforcing ignored in the core accessibility tree.
    return !axObject.isDetached()
        && (axObject.includeIgnoredInCoreTree()
            || !axObject.isIgnored()
            || m_unconnectedNodes.contains(axObject.objectID()));
}

std::optional<AXIsolatedTree::NodeChange> AXIsolatedTree::nodeChangeForObject(Ref<AccessibilityObject> axObject)
{
    ASSERT(isMainThread());
    ASSERT(!axObject->isDetached());

    if (!shouldCreateNodeChange(axObject.get()))
        return std::nullopt;

    ASSERT(axObject->wrapper());
    auto data = createIsolatedObjectData(axObject, *this);
    Markable parentID = data.parentID;
    m_nodeMap.set(axObject->objectID(), ParentChildrenIDs { parentID, data.childrenIDs });
    NodeChange nodeChange { WTFMove(data), axObject->wrapper() };

    if (axObject->isRoot())
        setPendingRootNodeID(axObject->objectID());
    return nodeChange;
}

void AXIsolatedTree::queueChange(NodeChange&& nodeChange)
{
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    AXID objectID = nodeChange.data.axID;
    Markable parentID = nodeChange.data.parentID;
    m_pendingAppends.append(WTFMove(nodeChange));

    if (parentID) {
        auto siblingsIDs = m_nodeMap.get(*parentID).childrenIDs;
        m_pendingChildrenUpdates.append({ *parentID, WTFMove(siblingsIDs) });
    }

    ASSERT_WITH_MESSAGE(objectID != parentID, "object ID was the same as its parent ID (%s) when queueing a node change", objectID.loggingString().utf8().data());
    ASSERT_WITH_MESSAGE(m_nodeMap.contains(objectID), "node map should've contained objectID: %s", objectID.loggingString().utf8().data());
    auto childrenIDs = m_nodeMap.get(objectID).childrenIDs;
    m_pendingChildrenUpdates.append({ objectID, WTFMove(childrenIDs) });
}

void AXIsolatedTree::addUnconnectedNode(Ref<AccessibilityObject> axObject)
{
    AXTRACE("AXIsolatedTree::addUnconnectedNode"_s);
    ASSERT(isMainThread());

    auto objectID = axObject->objectID();
    if (m_unconnectedNodes.contains(objectID)) {
        AXLOG(makeString("AXIsolatedTree::addUnconnectedNode exiting because an isolated object for "_s, objectID.loggingString(), " already exists."_s));
        return;
    }

    if (axObject->isDetached() || !axObject->wrapper()) {
        AXLOG(makeString("AXIsolatedTree::addUnconnectedNode bailing because associated live object ID "_s, objectID.loggingString(), " had no wrapper or is detached. Object is:"_s));
        AXLOG(axObject.ptr());
        return;
    }
    AXLOG(makeString("AXIsolatedTree::addUnconnectedNode creating isolated object from live object ID "_s, objectID.loggingString()));

    // Because we are queuing a change for an object not intended to be connected to the rest of the tree,
    // we don't need to update m_nodeMap or m_pendingChildrenUpdates for this object or its parent as is
    // done in AXIsolatedTree::nodeChangeForObject and AXIsolatedTree::queueChange.
    //
    // Instead, just directly create and queue the node change so m_readerThreadNodeMap can hold a reference
    // to it. It will be removed from m_readerThreadNodeMap when the corresponding DOM element, renderer, or
    // other entity is removed from the page.
    NodeChange nodeChange { createIsolatedObjectData(axObject, *this), axObject->wrapper() };
    Locker locker { m_changeLogLock };
    m_pendingAppends.append(WTFMove(nodeChange));
    m_unconnectedNodes.add(objectID);
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

    m_pendingSubtreeRemovals.addAll(WTFMove(subtreeRemovals));
    m_pendingProtectedFromDeletionIDs.addAll(std::exchange(m_protectedFromDeletionIDs, { }));
}

void AXIsolatedTree::queueRemovalsAndUnresolvedChanges()
{
    ASSERT(isMainThread());

    queueAppendsAndRemovals(resolveAppends(), std::exchange(m_subtreesToRemove, { }));
}

Vector<AXIsolatedTree::NodeChange> AXIsolatedTree::resolveAppends()
{
    ASSERT(isMainThread());

    if (m_unresolvedPendingAppends.isEmpty())
        return { };

    auto* cache = axObjectCache();
    if (!cache)
        return { };

    auto lastFeedbackTime = MonotonicTime::now();
    double counter = 0;
    Vector<NodeChange> resolvedAppends;
    resolvedAppends.reserveInitialCapacity(m_unresolvedPendingAppends.size());
    // The process of resolving appends can add more IDs to m_unresolvedPendingAppends as we iterate over it, so
    // iterate over an exchanged map instead. Any late-appended IDs will get picked up in the next cycle.
    auto unresolvedPendingAppends = std::exchange(m_unresolvedPendingAppends, { });
    for (const auto& axID : unresolvedPendingAppends) {
        if (m_replacingTree) {
            ++counter;
            if (MonotonicTime::now() - lastFeedbackTime > CreationFeedbackInterval) {
                m_replacingTree->reportLoadingProgress(counter / unresolvedPendingAppends.size());
                lastFeedbackTime = MonotonicTime::now();
            }
        }

        if (RefPtr axObject = cache->objectForID(axID)) {
            if (std::optional nodeChange = nodeChangeForObject(*axObject))
                resolvedAppends.append(WTFMove(*nodeChange));
        }
    }
    resolvedAppends.shrinkToFit();

    if (m_replacingTree)
        m_replacingTree->reportLoadingProgress(1);
    return resolvedAppends;
}

void AXIsolatedTree::queueAppendsAndRemovals(Vector<NodeChange>&& appends, Vector<AXID>&& subtreeRemovals)
{
    ASSERT(isMainThread());

    auto parentUpdateIDs = std::exchange(m_needsParentUpdate, { });
    Locker locker { m_changeLogLock };
    for (auto&& append : appends)
        queueChange(WTFMove(append));

    for (const auto& axID : parentUpdateIDs) {
        ASSERT_WITH_MESSAGE(m_nodeMap.contains(axID), "An object marked as needing a parent update should've had an entry in the node map by now. ID was %s", axID.loggingString().utf8().data());
        m_pendingParentUpdates.set(axID, *m_nodeMap.get(axID).parentID);
    }

    queueRemovalsLocked(WTFMove(subtreeRemovals));
}

void AXIsolatedTree::collectNodeChangesForSubtree(AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::collectNodeChangesForSubtree"_s);
    AXLOG(axObject);
    ASSERT(isMainThread());

    if (axObject.isDetached()) {
        AXLOG("Can't build an isolated tree branch rooted at a detached object.");
        return;
    }

    SetForScope isCollectingNodeChanges(m_isCollectingNodeChanges, true);

    RefPtr axParent = axObject.parentInCoreTree();
    auto parentID = axParent ? std::optional { axParent->objectID() } : std::nullopt;
    auto axChildrenCopy = axObject.children();

    auto iterator = m_nodeMap.find(axObject.objectID());
    if (iterator == m_nodeMap.end()) {
        m_unresolvedPendingAppends.add(axObject.objectID());

        Vector<AXID> axChildrenIDs;
        axChildrenIDs.reserveInitialCapacity(axChildrenCopy.size());
        for (const auto& axChild : axChildrenCopy) {
            if (axChild.ptr() == &axObject) {
                ASSERT_NOT_REACHED();
                continue;
            }

            axChildrenIDs.append(axChild->objectID());
            collectNodeChangesForSubtree(downcast<AccessibilityObject>(axChild.get()));
        }
        axChildrenIDs.shrinkToFit();

        m_nodeMap.set(axObject.objectID(), ParentChildrenIDs { parentID, WTFMove(axChildrenIDs) });
    } else {
        // This object is already in the isolated tree, so there's no need to create full node change for it (doing so is expensive).
        // Protect this object from being deleted. This is important when |axObject| was a child of some other object,
        // but no longer is, and thus the other object will try to queue it for removal. But the fact that we're here
        // indicates this object isn't ready to be removed, just a child of a different parent, so prevent this removal.
        m_protectedFromDeletionIDs.add(axObject.objectID());
        // Update the object's parent if it has changed (but only if we aren't going to create a node change for it,
        // as the act of creating a new node change will correct this as part of creating the new AXIsolatedObject).
        if (parentID && iterator->value.parentID != *parentID && !m_unresolvedPendingAppends.contains(axObject.objectID()))
            m_needsParentUpdate.add(axObject.objectID());

        // Only update the parentID so that we have the right one set for when we process m_needsParentUpdate. We explicitly
        // don't want to update the children IDs in this case, as we need to keep the "old" children around in order for
        // `AXIsolatedTree::updateChildren` to behave correctly.
        iterator->value.parentID = parentID;

        for (const auto& axChild : axChildrenCopy) {
            if (axChild.ptr() == &axObject) {
                ASSERT_NOT_REACHED();
                continue;
            }
            collectNodeChangesForSubtree(downcast<AccessibilityObject>(axChild.get()));
        }
    }
}

void AXIsolatedTree::updateNode(AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::updateNode"_s);
    AXLOG(&axObject);
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    // If we update a node as the result of some side effect while collecting node changes (e.g. a role change from
    // AccessibilityRenderObject::updateRoleAfterChildrenCreation), queue the append up to be resolved with the rest
    // of the collected changes. This prevents us from creating two node changes for the same object.
    if (isCollectingNodeChanges() || !m_unresolvedPendingAppends.isEmpty()) {
        m_unresolvedPendingAppends.add(axObject.objectID());
        return;
    }

    // Otherwise, resolve the change immediately and queue it up.
    // In both cases, we can't attach the wrapper immediately on the main thread, since the wrapper could be in use
    // on the AX thread (because this function updates an existing node).
    if (auto change = nodeChangeForObject(axObject)) {
        Locker locker { m_changeLogLock };
        queueChange(WTFMove(*change));
        return;
    }

    // Not able to update axObject. This may be because it is a descendant of a barren object such as a button. In that case, try to update its parent.
    if (!axObject.isDescendantOfBarrenParent())
        return;

    RefPtr axParent = axObject.parentInCoreTree();
    if (!axParent)
        return;

    if (auto change = nodeChangeForObject(downcast<AccessibilityObject>(*axParent))) {
        Locker locker { m_changeLogLock };
        queueChange(WTFMove(*change));
    }
}

void AXIsolatedTree::objectChangedIgnoredState(const AccessibilityObject& object)
{
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    ASSERT(isMainThread());

    if (RefPtr parentTable = object.parentTableIfTableCell()) {
        // FIXME: This should be as simple as:
        //     queueNodeUpdate(*parentTable->objectID(), { { AXProperty::Cells, AXProperty::CellSlots, AXProperty::Columns } });
        // As these are the table properties that depend on cells. But we can't do that, because we compute "new" column accessibility objects
        // every time we clearChildren() and addChildren(), so just re-computing AXProperty::Columns means that we won't have AXIsolatedObjects
        // for the columns. Instead we have to do a significantly more wasteful children update.
        queueNodeUpdate(parentTable->objectID(), NodeUpdateOptions::childrenUpdate());
        queueNodeUpdate(parentTable->objectID(), { { AXProperty::Cells, AXProperty::CellSlots } });
    }

    if (object.isLink()) {
        CheckedPtr axObjectCache = m_axObjectCache.get();
        if (RefPtr webArea = axObjectCache ? axObjectCache->rootWebArea() : nullptr)
            queueNodeUpdate(webArea->objectID(), { AXProperty::DocumentLinks });
    }
#else
    UNUSED_PARAM(object);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
}

void AXIsolatedTree::updatePropertiesForSelfAndDescendants(AccessibilityObject& axObject, const AXPropertySet& propertySet)
{
    AXTRACE("AXIsolatedTree::updatePropertiesForSelfAndDescendants"_s);
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    Accessibility::enumerateDescendantsIncludingIgnored<AXCoreObject>(axObject, true, [&propertySet, this, protectedThis = Ref { *this }] (auto& descendant) {
        queueNodeUpdate(descendant.objectID(), { propertySet });
    });
}

void AXIsolatedTree::updateNodeProperties(AccessibilityObject& axObject, const AXPropertySet& propertySet)
{
    AXTRACE("AXIsolatedTree::updateNodeProperties"_s);
    AXLOG(makeString("Updating properties for objectID "_s, axObject.objectID().loggingString(), ": "_s));
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    AXPropertyVector properties;
    properties.reserveInitialCapacity(propertySet.size());
    for (const auto& property : propertySet) {
        AXLOG(makeString("Property: "_s, property));
        switch (property) {
        case AXProperty::Abbreviation:
            properties.append({ AXProperty::Abbreviation, axObject.abbreviation().isolatedCopy() });
            break;
        case AXProperty::AccessKey:
            properties.append({ AXProperty::AccessKey, axObject.accessKey().isolatedCopy() });
            break;
        case AXProperty::AccessibilityText: {
            Vector<AccessibilityText> texts;
            axObject.accessibilityText(texts);
            auto axTextValue = texts.map([] (const auto& text) -> AccessibilityText {
                return { text.text.isolatedCopy(), text.textSource };
            });
            properties.append({ AXProperty::AccessibilityText, WTFMove(axTextValue) });
            break;
        }
        case AXProperty::ARIARoleDescription:
            properties.append({ AXProperty::ARIARoleDescription, axObject.ariaRoleDescription().isolatedCopy() });
            break;
        case AXProperty::ARIALevel:
            properties.append({ AXProperty::ARIALevel, axObject.ariaLevel() });
            break;
        case AXProperty::ValueAutofillButtonType:
            properties.append({ AXProperty::ValueAutofillButtonType, static_cast<int>(axObject.valueAutofillButtonType()) });
            properties.append({ AXProperty::IsValueAutofillAvailable, axObject.isValueAutofillAvailable() });
            break;
        case AXProperty::AXColumnCount:
            properties.append({ AXProperty::AXColumnCount, axObject.axColumnCount() });
            break;
        case AXProperty::BrailleLabel:
            properties.append({ AXProperty::BrailleLabel, axObject.brailleLabel().isolatedCopy() });
            break;
        case AXProperty::BrailleRoleDescription:
            properties.append({ AXProperty::BrailleRoleDescription, axObject.brailleRoleDescription().isolatedCopy() });
            break;
        case AXProperty::AXColumnIndex: {
            if (std::optional<unsigned> columnIndex = axObject.axColumnIndex())
                properties.append({ AXProperty::AXColumnIndex, *columnIndex });
            break;
        }
        case AXProperty::AXColumnIndexText:
            properties.append({ AXProperty::AXColumnIndexText, axObject.axColumnIndexText().isolatedCopy() });
            break;
        case AXProperty::CanSetFocusAttribute:
            properties.append({ AXProperty::CanSetFocusAttribute, axObject.canSetFocusAttribute() });
            break;
        case AXProperty::CanSetSelectedAttribute:
            properties.append({ AXProperty::CanSetSelectedAttribute, axObject.canSetSelectedAttribute() });
            break;
        case AXProperty::CanSetValueAttribute:
            properties.append({ AXProperty::CanSetValueAttribute, axObject.canSetValueAttribute() });
            break;
        case AXProperty::Cells:
            properties.append({ AXProperty::Cells, axIDs(axObject.cells()) });
            break;
        case AXProperty::CellSlots:
            properties.append({ AXProperty::CellSlots, axObject.cellSlots() });
            break;
        case AXProperty::ColumnIndex:
            properties.append({ AXProperty::ColumnIndex, axObject.columnIndex() });
            break;
        case AXProperty::ColumnIndexRange:
            properties.append({ AXProperty::ColumnIndexRange, axObject.columnIndexRange() });
            break;
        case AXProperty::CurrentState:
            properties.append({ AXProperty::CurrentState, static_cast<int>(axObject.currentState()) });
            break;
        case AXProperty::DatetimeAttributeValue:
            properties.append({ AXProperty::DatetimeAttributeValue, axObject.datetimeAttributeValue().isolatedCopy() });
            break;
        case AXProperty::DisclosedRows:
            properties.append({ AXProperty::DisclosedRows, axIDs(axObject.disclosedRows()) });
            break;
        case AXProperty::DocumentLinks:
            properties.append({ AXProperty::DocumentLinks, axIDs(axObject.documentLinks()) });
            break;
        case AXProperty::ExplicitOrientation: {
            if (std::optional<AccessibilityOrientation> orientation = axObject.explicitOrientation())
                properties.append({ AXProperty::ExplicitOrientation, *orientation });
            break;
        }
        case AXProperty::ExtendedDescription:
            properties.append({ AXProperty::ExtendedDescription, axObject.extendedDescription().isolatedCopy() });
            break;
        case AXProperty::HasClickHandler:
            properties.append({ AXProperty::HasClickHandler, axObject.hasClickHandler() });
            break;
        case AXProperty::IdentifierAttribute:
            properties.append({ AXProperty::IdentifierAttribute, axObject.identifierAttribute().isolatedCopy() });
            break;
        case AXProperty::InputType:
            if (std::optional inputType = axObject.inputType())
                properties.append({ AXProperty::InputType, *inputType });
            break;
        case AXProperty::InternalLinkElement: {
            auto* linkElement = axObject.internalLinkElement();
            properties.append({ AXProperty::InternalLinkElement, linkElement ? std::optional { linkElement->objectID() } : std::nullopt });
            break;
        }
        case AXProperty::IsChecked:
            ASSERT(axObject.supportsCheckedState());
            properties.append({ AXProperty::IsChecked, axObject.isChecked() });
            properties.append({ AXProperty::ButtonState, axObject.checkboxOrRadioValue() });
            break;
        case AXProperty::IsColumnHeader:
            properties.append({ AXProperty::IsColumnHeader, axObject.isColumnHeader() });
            break;
        case AXProperty::IsEditableWebArea:
            properties.append({ AXProperty::IsEditableWebArea, axObject.isEditableWebArea() });
            break;
        case AXProperty::IsEnabled:
            properties.append({ AXProperty::IsEnabled, axObject.isEnabled() });
            break;
        case AXProperty::IsExpanded:
            properties.append({ AXProperty::IsExpanded, axObject.isExpanded() });
            break;
        case AXProperty::IsHiddenUntilFoundContainer:
            properties.append({ AXProperty::IsHiddenUntilFoundContainer, axObject.isHiddenUntilFoundContainer() });
            break;
        case AXProperty::IsIgnored:
            properties.append({ AXProperty::IsIgnored, axObject.isIgnored() });
            break;
        case AXProperty::IsRequired:
            properties.append({ AXProperty::IsRequired, axObject.isRequired() });
            break;
        case AXProperty::IsSelected:
            properties.append({ AXProperty::IsSelected, axObject.isSelected() });
            break;
        case AXProperty::IsRowHeader:
            properties.append({ AXProperty::IsRowHeader, axObject.isRowHeader() });
            break;
        case AXProperty::IsVisible:
            properties.append({ AXProperty::IsVisible, axObject.isVisible() });
            break;
        case AXProperty::IsVisited:
            properties.append({ AXProperty::IsVisited, axObject.isVisited() });
            break;
        case AXProperty::MaxValueForRange:
            properties.append({ AXProperty::MaxValueForRange, axObject.maxValueForRange() });
            break;
        case AXProperty::MinValueForRange:
            properties.append({ AXProperty::MinValueForRange, axObject.minValueForRange() });
            break;
        case AXProperty::NameAttribute:
            properties.append({ AXProperty::NameAttribute, axObject.nameAttribute().isolatedCopy() });
            break;
        case AXProperty::PosInSet:
            properties.append({ AXProperty::PosInSet, axObject.posInSet() });
            break;
        case AXProperty::RemoteFramePlatformElement:
            properties.append({ AXProperty::RemoteFramePlatformElement, axObject.remoteFramePlatformElement() });
            break;
        case AXProperty::StitchGroups:
            properties.append({ AXProperty::StitchGroups, axObject.stitchGroups() });
            break;
        case AXProperty::StringValue:
            properties.append({ AXProperty::StringValue, axObject.stringValue().isolatedCopy() });
            break;
        case AXProperty::HasRemoteFrameChild:
            properties.append({ AXProperty::HasRemoteFrameChild, axObject.hasRemoteFrameChild() });
            break;
        case AXProperty::RowIndex:
            properties.append({ AXProperty::RowIndex, axObject.rowIndex() });
            break;
        case AXProperty::RowIndexRange:
            properties.append({ AXProperty::RowIndexRange, axObject.rowIndexRange() });
            break;
        case AXProperty::AXRowIndex: {
            if (std::optional<unsigned> rowIndex = axObject.axRowIndex())
                properties.append({ AXProperty::AXRowIndex, *rowIndex });
            break;
        }
        case AXProperty::AXRowIndexText:
            properties.append({ AXProperty::AXRowIndexText, axObject.axRowIndexText().isolatedCopy() });
            break;
        case AXProperty::CellScope:
            properties.append({ AXProperty::CellScope, axObject.cellScope().isolatedCopy() });
            break;
        case AXProperty::HasCursorPointer:
            properties.append({ AXProperty::HasCursorPointer, axObject.hasCursorPointer() });
            break;
        case AXProperty::RadioButtonGroupMembers:
            properties.append({ AXProperty::RadioButtonGroupMembers, axIDs(axObject.radioButtonGroup()) });
            break;
        case AXProperty::ScreenRelativePosition:
            properties.append({ AXProperty::ScreenRelativePosition, axObject.screenRelativePosition() });
            break;
        case AXProperty::SelectedTextRange:
            properties.append({ AXProperty::SelectedTextRange, axObject.selectedTextRange() });
            break;
        case AXProperty::SetSize:
            properties.append({ AXProperty::SetSize, axObject.setSize() });
            break;
        case AXProperty::SortDirection:
            properties.append({ AXProperty::SortDirection, static_cast<int>(axObject.sortDirection()) });
            break;
        case AXProperty::SpeakAs:
            properties.append({ AXProperty::SpeakAs, axObject.speakAs() });
            break;
        case AXProperty::KeyShortcuts:
            properties.append({ AXProperty::SupportsKeyShortcuts, axObject.supportsKeyShortcuts() });
            properties.append({ AXProperty::KeyShortcuts, axObject.keyShortcuts().isolatedCopy() });
            break;
        case AXProperty::ShowsCursorOnHover:
            properties.append({ AXProperty::ShowsCursorOnHover, axObject.showsCursorOnHover() });
            break;
        case AXProperty::SupportsARIAOwns:
            properties.append({ AXProperty::SupportsARIAOwns, axObject.supportsARIAOwns() });
            break;
        case AXProperty::SupportsExpanded:
            properties.append({ AXProperty::SupportsExpanded, axObject.supportsExpanded() });
            break;
        case AXProperty::SupportsDragging:
            properties.append({ AXProperty::SupportsDragging, axObject.supportsDragging() });
            break;
        case AXProperty::SupportsPosInSet:
            properties.append({ AXProperty::SupportsPosInSet, axObject.supportsPosInSet() });
            break;
        case AXProperty::SupportsSetSize:
            properties.append({ AXProperty::SupportsSetSize, axObject.supportsSetSize() });
            break;
        case AXProperty::TextInputMarkedTextMarkerRange: {
            AXIDAndCharacterRange value;
            auto range = axObject.textInputMarkedTextMarkerRange();
            if (auto characterRange = range.characterRange(); range && characterRange)
                value = { range.start().objectID(), *characterRange };
            properties.append({ AXProperty::TextInputMarkedTextMarkerRange, WTF::makeUnique<AXIDAndCharacterRange>(value) });
            break;
        }
#if ENABLE(AX_THREAD_TEXT_APIS)
        case AXProperty::BackgroundColor:
            properties.append({ AXProperty::BackgroundColor, axObject.backgroundColor() });
            break;
        case AXProperty::Font: {
            if (RefPtr parent = axObject.parentObject()) {
                RetainPtr font = axObject.font();
                if (font != parent->font()) {
                    properties.append({ AXProperty::Font, WTFMove(font) });
                    break;
                }
            }
            properties.append({ AXProperty::Font, nullptr });
            break;
        }
        case AXProperty::HasLinethrough:
            properties.append({ AXProperty::HasLinethrough, axObject.lineDecorationStyle().hasLinethrough });
            break;
        case AXProperty::HasTextShadow:
            properties.append({ AXProperty::HasTextShadow, axObject.hasTextShadow() });
            break;
        case AXProperty::IsSubscript:
            properties.append({ AXProperty::IsSubscript, axObject.isSubscript() });
            break;
        case AXProperty::IsSuperscript:
            properties.append({ AXProperty::IsSuperscript, axObject.isSuperscript() });
            break;
        case AXProperty::LinethroughColor:
            properties.append({ AXProperty::LinethroughColor, axObject.lineDecorationStyle().linethroughColor });
            break;
        case AXProperty::RevealableText:
            // We should only cache this property for ignored objects.
            ASSERT(axObject.isIgnored());
            if (String text = axObject.revealableText(); !text.isEmpty())
                properties.append({ AXProperty::RevealableText, WTFMove(text).isolatedCopy() });
            break;
        case AXProperty::TextColor: {
            if (RefPtr parent = axObject.parentObject()) {
                auto color = axObject.textColor();
                if (color != parent->textColor()) {
                    properties.append({ AXProperty::TextColor, color });
                    break;
                }
            }
            // Setting text color to nullptr will remove it from the property map, and allow it to be inherited from an ancestor.
            properties.append({ AXProperty::TextColor, nullptr });
            break;
        }
        case AXProperty::TextRuns:
            properties.append({ AXProperty::TextRuns, WTF::makeUnique<AXTextRuns>(axObject.textRuns()) });
            break;
        case AXProperty::UnderlineColor: {
            if (axObject.hasUnderline())
                properties.append({ AXProperty::UnderlineColor, axObject.lineDecorationStyle().underlineColor });
            else {
                // Queue the default color to remove it from the property map.
                properties.append({ AXProperty::UnderlineColor, Accessibility::defaultColor() });
            }
            break;
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        case AXProperty::URL:
            properties.append({ AXProperty::URL, WTF::makeUnique<URL>(axObject.url().isolatedCopy()) });
            break;
        case AXProperty::ValueForRange:
            properties.append({ AXProperty::ValueForRange, axObject.valueForRange() });
            break;
        default:
            break;
        }
    }

    if (properties.isEmpty())
        return;

    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axObject.objectID(), WTFMove(properties) });
}

void AXIsolatedTree::overrideNodeProperties(AXID axID, AXPropertyVector&& properties)
{
    ASSERT(isMainThread());

    if (properties.isEmpty())
        return;

    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axID, WTFMove(properties) });
}

void AXIsolatedTree::updateDependentProperties(AccessibilityObject& axObject)
{
    ASSERT(isMainThread());

    auto updateRelatedObjects = [this] (const AccessibilityObject& object) {
        for (const auto& labeledObject : object.labelForObjects())
            queueNodeUpdate(labeledObject->objectID(), NodeUpdateOptions::nodeUpdate());

        for (const auto& describedByObject : object.descriptionForObjects())
            queueNodeUpdate(describedByObject->objectID(), { { AXProperty::AccessibilityText, AXProperty::ExtendedDescription } });
    };
    updateRelatedObjects(axObject);

    // When a row gains or loses cells, or a table changes rows in a row group, the column count of the table can change.
    bool updateTableAncestorColumns = axObject.isExposedTableRow();
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    updateTableAncestorColumns = updateTableAncestorColumns || isRowGroup(axObject.node());
#endif
    for (RefPtr ancestor = axObject.parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        if (updateTableAncestorColumns && ancestor->isTable()) {
            // Only `updateChildren` if the table is unignored, because otherwise `updateChildren` will ascend and update the next highest unignored ancestor, which doesn't accomplish our goal of updating table columns.
            if (ancestor->isIgnored())
                break;
            // Use `NodeUpdateOptions::childrenUpdate()` rather than `updateNodeProperty` because `childrenUpdate()` will ensure the columns (which are children) will have associated isolated objects created.
            queueNodeUpdate(ancestor->objectID(), NodeUpdateOptions::childrenUpdate());
            break;
        }

        updateRelatedObjects(*ancestor);
    }
}

void AXIsolatedTree::updateChildren(AccessibilityObject& axObject, ResolveNodeChanges resolveNodeChanges)
{
    AXTRACE("AXIsolatedTree::updateChildren"_s);
    AXLOG("For AXObject:");
    AXLOG(&axObject);
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    if (m_nodeMap.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!axObject.document() || !axObject.document()->hasLivingRenderTree())
        return;

    // We're about to do a lot of work, so start the attribute cache.
    AXAttributeCacheScope enableCache(axObject.axObjectCache());

    // updateChildren may be called as the result of a children changed
    // notification for an axObject that has no associated isolated object.
    // An example of this is when an empty element such as a <canvas> or <div>
    // has added a new child. So find the closest ancestor of axObject that has
    // an associated isolated object and update its children.
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    auto* axAncestor = &axObject;
#else
    auto* axAncestor = Accessibility::findAncestor(axObject, true, [this] (auto& ancestor) {
        return m_nodeMap.contains(ancestor.objectID());
    });
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    if (!axAncestor || axAncestor->isDetached()) {
        // This update was triggered before the isolated tree has been repopulated.
        // Return here since there is nothing to update.
        AXLOG("Bailing because no ancestor could be found, or ancestor is detached");
        return;
    }

#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (axAncestor != &axObject) {
        AXLOG(makeString("Original object with ID "_s, axObject.objectID().loggingString(), " wasn't in the isolated tree, so instead updating the closest in-isolated-tree ancestor:"_s));
        AXLOG(axAncestor);

        // An explicit copy is necessary here because the nested calls to updateChildren
        // can cause this objects children to be invalidated as we iterate.
        auto childrenCopy = axObject.children();
        for (auto& child : childrenCopy) {
            Ref liveChild = downcast<AccessibilityObject>(child.get());
            if (liveChild->childrenInitialized())
                continue;

            if (!m_nodeMap.contains(liveChild->objectID())) {
                if (!shouldCreateNodeChange(liveChild))
                    continue;

                // This child should be added to the isolated tree but hasn't been yet.
                // Add it to the nodemap so the recursive call to updateChildren below properly builds the subtree for this object.
                RefPtr parent = axObject.parentInCoreTree();
                m_nodeMap.set(liveChild->objectID(), ParentChildrenIDs { parent ? std::optional { parent->objectID() } : std::nullopt, liveChild->childrenIDs() });
                m_unresolvedPendingAppends.add(liveChild->objectID());
            }

            AXLOG(makeString(
                "Child ID "_s, liveChild->objectID().loggingString(), " of original object ID "_s, axObject.objectID().loggingString(), " was found in the isolated tree with uninitialized live children. Updating its isolated children."_s
            ));
            // Don't immediately resolve node changes in these recursive calls to updateChildren. This avoids duplicate node change creation in this scenario:
            //   1. Some subtree is updated in the below call to updateChildren.
            //   2. Later in this function, when updating axAncestor, we update some higher subtree that includes the updated subtree from step 1.
            queueNodeUpdate(liveChild->objectID(), NodeUpdateOptions::childrenUpdate());
        }
    }
#endif // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    // FIXME: This copy out of the hashmap seems unnecessary — can we use HashMap::find instead?
    auto oldIDs = m_nodeMap.get(axAncestor->objectID());
    auto& oldChildrenIDs = oldIDs.childrenIDs;

    const auto& newChildren = axAncestor->children();
    auto newChildrenIDs = axIDs(newChildren);

    bool childrenChanged = oldChildrenIDs.size() != newChildrenIDs.size();
    for (size_t i = 0; i < newChildren.size(); ++i) {
        ASSERT(newChildren[i]->objectID() == newChildrenIDs[i]);
        size_t index = oldChildrenIDs.find(newChildrenIDs[i]);
        if (index != notFound) {
            // Prevent deletion of this object below by removing it from oldChildrenIDs.
            oldChildrenIDs.removeAt(index);

            // Propagate any subtree updates downwards for this already-existing child.
            if (RefPtr liveChild = dynamicDowncast<AccessibilityObject>(newChildren[i].get()); liveChild && liveChild->hasDirtySubtree())
                queueNodeUpdate(liveChild->objectID(), NodeUpdateOptions::childrenUpdate());
        } else {
            // This is a new child, add it to the tree.
            childrenChanged = true;
            AXLOG(makeString("AXID "_s, axAncestor->objectID().loggingString(), " gaining new subtree, starting at ID "_s, newChildren[i]->objectID().loggingString(), ':'));
            AXLOG(newChildren[i]);
            collectNodeChangesForSubtree(downcast<AccessibilityObject>(newChildren[i].get()));
        }
    }
    m_nodeMap.set(axAncestor->objectID(), ParentChildrenIDs { oldIDs.parentID, WTFMove(newChildrenIDs) });
    // Since axAncestor is definitively part of the AX tree by way of getting here, protect it from being
    // deleted in case it has been re-parented.
    m_protectedFromDeletionIDs.add(axAncestor->objectID());

    // What is left in oldChildrenIDs are the IDs that are no longer children of axAncestor.
    // Thus, remove them from m_nodeMap and queue them to be removed from the tree.
    for (auto& axID : oldChildrenIDs)
        removeSubtreeFromNodeMap(axID, axAncestor->objectID());

    auto unconditionallyUpdate = [] (AccessibilityRole role) {
        // These are the roles that should be updated even if AX children don't change. This is necessary because
        // these roles are not allowed to have children according to accessibility semantics, but can have render
        // tree or DOM children, changes of which affect many properties (e.g. anything downstream of textUnderElement).
        // Note this is a subset of the roles in AccessibilityObject::canHaveChildren, deliberately only those that
        // could reasonably have meaningful-to-accessibility DOM / render tree children.
        switch (role) {
        case AccessibilityRole::Button:
        case AccessibilityRole::PopUpButton:
        case AccessibilityRole::Tab:
        case AccessibilityRole::ToggleButton:
        case AccessibilityRole::ListBoxOption:
        case AccessibilityRole::ProgressIndicator:
        case AccessibilityRole::Switch:
        case AccessibilityRole::MenuItemCheckbox:
        case AccessibilityRole::MenuItemRadio:
        case AccessibilityRole::Meter:
            return true;
        default:
            return false;
        }
    };

    // Also queue updates to the target node itself and any properties that depend on children().
    if (childrenChanged || unconditionallyUpdate(axAncestor->role())) {
        queueNodeUpdate(axAncestor->objectID(), NodeUpdateOptions::nodeUpdate());
        updateDependentProperties(*axAncestor);
    }

    m_subtreesToRemove.appendVector(WTFMove(oldChildrenIDs));
    if (resolveNodeChanges == ResolveNodeChanges::Yes)
        queueRemovalsAndUnresolvedChanges();
}

void AXIsolatedTree::updateChildrenForObjects(const ListHashSet<Ref<AccessibilityObject>>& axObjects)
{
    AXTRACE("AXIsolatedTree::updateChildrenForObjects"_s);

    if (isUpdatingSubtree())
        return;

    AXAttributeCacheScope enableCache(axObjectCache());
    for (auto& axObject : axObjects)
        queueNodeUpdate(axObject->objectID(), NodeUpdateOptions::childrenUpdate());

    queueRemovalsAndUnresolvedChanges();
}

void AXIsolatedTree::setPageActivityState(OptionSet<ActivityState> state)
{
    ASSERT(isMainThread());

    Locker locker { s_storeLock };
    m_pageActivityState = state;
}

OptionSet<ActivityState> AXIsolatedTree::pageActivityState() const
{
    Locker locker { s_storeLock };
    return m_pageActivityState;
}

OptionSet<ActivityState> AXIsolatedTree::lockedPageActivityState() const
{
    ASSERT(s_storeLock.isLocked());
    return m_pageActivityState;
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedTree::sortedLiveRegions()
{
    ASSERT(!isMainThread());
    return objectsForIDs(m_sortedLiveRegionIDs);
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedTree::sortedNonRootWebAreas()
{
    ASSERT(!isMainThread());
    return objectsForIDs(m_sortedNonRootWebAreaIDs);
}

void AXIsolatedTree::setInitialSortedLiveRegions(Vector<AXID> liveRegionIDs)
{
    ASSERT(isMainThread());

    m_sortedLiveRegionIDs = WTFMove(liveRegionIDs);
}

void AXIsolatedTree::setInitialSortedNonRootWebAreas(Vector<AXID> webAreaIDs)
{
    ASSERT(isMainThread());

    m_sortedNonRootWebAreaIDs = WTFMove(webAreaIDs);
}

std::optional<AXID> AXIsolatedTree::focusedNodeID()
{
    ASSERT(!isMainThread());
    // applyPendingChanges can destroy `this` tree, so protect it until the end of this method.
    Ref protectedThis { *this };
    // Apply pending changes in case focus has changed and hasn't been updated.
    applyPendingChanges();
    return m_focusedNodeID;
}

RefPtr<AXIsolatedObject> AXIsolatedTree::focusedNode()
{
    AXTRACE("AXIsolatedTree::focusedNode"_s);
    ASSERT(!isMainThread());
    AXLOG("focused node:");
    AXLOG(objectForID(focusedNodeID()));
    return objectForID(focusedNodeID());
}

RefPtr<AXIsolatedObject> AXIsolatedTree::rootWebArea()
{
    AXTRACE("AXIsolatedTree::rootWebArea"_s);
    ASSERT(!isMainThread());

    RefPtr root = rootNode();
    return root ? Accessibility::findUnignoredChild(*root, [] (auto& object) {
        return object->isWebArea();
    }) : nullptr;
}

void AXIsolatedTree::setPendingRootNodeID(AXID axID)
{
    Locker locker { m_changeLogLock };
    setPendingRootNodeIDLocked(axID);
}

void AXIsolatedTree::setPendingRootNodeIDLocked(AXID axID)
{
    AXTRACE("AXIsolatedTree::setPendingRootNodeIDLocked"_s);
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    m_pendingRootNodeID = axID;
}

void AXIsolatedTree::setFocusedNodeID(std::optional<AXID> axID)
{
    AXTRACE("AXIsolatedTree::setFocusedNodeID"_s);
    AXLOG(makeString("axID "_s, axID ? axID->loggingString() : ""_str));
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_pendingFocusedNodeID = axID;
}

void AXIsolatedTree::updateRelations(HashMap<AXID, AXRelations>&& relations)
{
    AXTRACE("AXIsolatedTree::updateRelations"_s);
    ASSERT(isMainThread());

    m_relationsNeedUpdate = false;
    Locker locker { m_changeLogLock };
    m_pendingRelations = WTFMove(relations);
}

void AXIsolatedTree::setSelectedTextMarkerRange(AXTextMarkerRange&& range)
{
    AXTRACE("AXIsolatedTree::setSelectedTextMarkerRange"_s);
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_pendingSelectedTextMarkerRange = WTFMove(range);
}

void AXIsolatedTree::updateLoadingProgress(double newProgressValue)
{
    AXTRACE("AXIsolatedTree::updateLoadingProgress"_s);
    AXLOG(makeString("Updating loading progress to "_s, newProgressValue, " for treeID "_s, treeID().loggingString()));
    ASSERT(isMainThread());

    m_loadingProgress = newProgressValue;
}

void AXIsolatedTree::updateFrame(AXID axID, IntRect&& newFrame)
{
    AXTRACE("AXIsolatedTree::updateFrame"_s);
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    AXPropertyVector properties;
    properties.append({ AXProperty::RelativeFrame, WTFMove(newFrame) });
    // We can clear the initially-cached rough frame, since the object's frame has been cached.
    properties.append({ AXProperty::InitialLocalRect, FloatRect() });
    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axID, WTFMove(properties) });
}

void AXIsolatedTree::updateRootScreenRelativePosition()
{
    AXTRACE("AXIsolatedTree::updateRootScreenRelativePosition"_s);
    ASSERT(isMainThread());

    CheckedPtr cache = m_axObjectCache.get();
    if (RefPtr axRoot = cache && cache->document() ? cache->getOrCreate(cache->document()->view()) : nullptr)
        queueNodeUpdate(axRoot->objectID(), { AXProperty::ScreenRelativePosition });
}

void AXIsolatedTree::removeNode(AXID axID, std::optional<AXID> parentID)
{
    AXTRACE("AXIsolatedTree::removeNode"_s);
    AXLOG(makeString("objectID "_s, axID.loggingString()));
    ASSERT(isMainThread());

    m_unresolvedPendingAppends.remove(axID);
    removeSubtreeFromNodeMap(axID, parentID);
    queueRemovals({ axID });
}

void AXIsolatedTree::removeSubtreeFromNodeMap(std::optional<AXID> objectID, std::optional<AXID> axParentID)
{
    AXTRACE("AXIsolatedTree::removeSubtreeFromNodeMap"_s);
    AXLOG(makeString("Removing subtree for objectID "_s,  objectID ? objectID->loggingString() : ""_str));
    ASSERT(isMainThread());

    if (!objectID)
        return;

    if (m_unconnectedNodes.remove(*objectID))
        return;

    if (!m_nodeMap.contains(*objectID)) {
        AXLOG(makeString("Tried to remove AXID "_s, objectID->loggingString(), " that is no longer in m_nodeMap."_s));
        return;
    }

    Markable<AXID> actualParentID = m_nodeMap.get(*objectID).parentID;
    // If the axParentID and actualParentID differ in whether they are null, or if the values differ, break early.
    // If both are null, we are likely at the parent.
    if (static_cast<bool>(axParentID) != static_cast<bool>(actualParentID) || axParentID != actualParentID) {
        AXLOG(makeString("Tried to remove object ID "_s, objectID->loggingString(), " from a different parent "_s, axParentID ? axParentID->loggingString() : ""_str, ", actual parent "_s, actualParentID ? actualParentID->loggingString() : ""_str, ", bailing out."_s));
        return;
    }

    Vector<std::optional<AXID>> removals = { *objectID };
    while (removals.size()) {
        auto axID = removals.takeLast();
        if (!axID || m_unresolvedPendingAppends.contains(*axID) || m_protectedFromDeletionIDs.contains(*axID))
            continue;

        auto it = m_nodeMap.find(*axID);
        if (it != m_nodeMap.end()) {
            removals.appendVector(it->value.childrenIDs);
            m_nodeMap.remove(*axID);
        }
    }
}

std::optional<ListHashSet<AXID>> AXIsolatedTree::relatedObjectIDsFor(const AXIsolatedObject& object, AXRelation relation)
{
    ASSERT(!isMainThread());

    auto relationsIterator = m_relations.find(object.objectID());
    if (relationsIterator == m_relations.end())
        return std::nullopt;

    auto targetsIterator = relationsIterator->value.find(static_cast<uint8_t>(relation));
    if (targetsIterator == relationsIterator->value.end())
        return std::nullopt;
    return targetsIterator->value;
}

void AXIsolatedTree::applyPendingChanges()
{
    Locker locker { m_changeLogLock };
    applyPendingChangesLocked();
}

void AXIsolatedTree::applyPendingChangesUnlessQueuedForDestruction()
{
    ASSERT(!isMainThread());

    Locker locker { m_changeLogLock };

    if (m_queuedForDestruction)
        return;
    applyPendingChangesLocked();
}

void AXIsolatedTree::applyPendingChangesLocked()
{
    AXTRACE("AXIsolatedTree::applyPendingChanges"_s);
    ASSERT(!isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    if (AXObjectCache::isAppleInternalInstall()) [[unlikely]]
        WTFBeginSignpostAlways(this, AccessibilityIsolatedTreeApplyPendingChanges, "tree ID: %" PRIVATE_LOG_STRING "", treeID().loggingString().utf8().data());

    if (m_queuedForDestruction) [[unlikely]] {
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

        if (AXObjectCache::isAppleInternalInstall()) [[unlikely]]
            WTFEndSignpostAlways(this, AccessibilityIsolatedTreeApplyPendingChanges, "tree ID: %" PRIVATE_LOG_STRING "", treeID().loggingString().utf8().data());
        return;
    }

    if (m_pendingFocusedNodeID != m_focusedNodeID) {
        AXLOG(makeString("focusedNodeID "_s, m_focusedNodeID ? m_focusedNodeID->loggingString() : ""_str, " pendingFocusedNodeID "_s, m_pendingFocusedNodeID ? m_pendingFocusedNodeID->loggingString() : ""_str));
        m_focusedNodeID = m_pendingFocusedNodeID;
    }

    while (m_pendingSubtreeRemovals.size()) {
        // WTF_IGNORES_THREAD_SAFETY_ANALYSIS because we _do_ hold the m_changeLogLock, but the thread-safety
        // analysis throws a false-positive compile error when we access m_pendingProtectedFromDeletionIDs in
        // this lambda.
        std::function<void(Ref<AXCoreObject>&&)> deleteSubtree = [this, protectedThis = Ref { *this }, &deleteSubtree] (Ref<AXCoreObject>&& coreObjectToDelete) WTF_IGNORES_THREAD_SAFETY_ANALYSIS {
            auto& objectToDelete = downcast<AXIsolatedObject>(coreObjectToDelete.get());
            while (objectToDelete.m_children.size()) {
                Ref child = objectToDelete.m_children.takeLast();
                if (!m_pendingProtectedFromDeletionIDs.contains(child->objectID()))
                    deleteSubtree(WTFMove(child));
            }

            // There's no need to call the more comprehensive AXCoreObject::detach here since
            // we're deleting the entire subtree of this object and thus don't need to `detachRemoteParts`.
            objectToDelete.detachWrapper(AccessibilityDetachmentType::ElementDestroyed);

            auto deleteAXID = objectToDelete.objectID();
            m_readerThreadNodeMap.remove(deleteAXID);
            m_pendingSubtreeRemovals.remove(deleteAXID);

            for (const AXID& childID : objectToDelete.m_unresolvedChildrenIDs) {
                // Ideally, assuming m_children has been initialized, there would be no unresolved children IDs.
                // But sometimes when initializing m_children, AXIsolatedTree::objectForID fails for an unknown
                // reason, and thus we are left with an entry in m_unresolvedChildrenIDs. See the ASSERT in
                // AXIsolatedObject::children. In case any of our unresolved IDs got populated with an object
                // later somehow, try to clean them up.
                if (!m_pendingProtectedFromDeletionIDs.contains(childID)) {
                    if (RefPtr child = m_readerThreadNodeMap.take(childID))
                        deleteSubtree(child.releaseNonNull());
                }
            }
        };

        // This dereference is safe because we checked m_pendingSubtreeRemovals.size() to get here.
        auto axID = *m_pendingSubtreeRemovals.takeAny();
        if (m_pendingProtectedFromDeletionIDs.contains(axID))
            continue;

        if (RefPtr object = m_readerThreadNodeMap.take(axID))
            deleteSubtree(object.releaseNonNull());
    }
    m_pendingProtectedFromDeletionIDs.clear();

    for (auto& item : m_pendingAppends) {
        auto axID = item.data.axID;
        AXLOG(makeString("appending axID "_s, axID.loggingString()));

        auto& wrapper = item.wrapper;
        if (!wrapper)
            continue;

        auto addResult = m_readerThreadNodeMap.ensure(axID, [&] {
            return AXIsolatedObject::create(WTFMove(item.data));
        });

        Ref<AXIsolatedObject>& object = addResult.iterator->value;
        if (!addResult.isNewEntry)
            object->updateFromData(WTFMove(item.data));

        // If the new object hasn't been attached to a wrapper yet, or if it was detached from
        // the wrapper when processing removals above, we must attach / re-attach it.
        if (object->isDetached())
            object->attachPlatformWrapper(wrapper.get());

        // The object must have a wrapper by this point.
        ASSERT(object->wrapper());
        // The reference count of the just added IsolatedObject must be 2
        // because it is referenced by m_readerThreadNodeMap and m_pendingAppends.
        // When m_pendingAppends is cleared, the object will be held only by m_readerThreadNodeMap. The exception is the root node whose reference count is 3.
    }
    m_pendingAppends.clear();

    for (const auto& parentUpdate : m_pendingParentUpdates) {
        if (RefPtr object = objectForID(parentUpdate.key))
            object->setParent(parentUpdate.value);
    }
    m_pendingParentUpdates.clear();

    for (auto& update : m_pendingChildrenUpdates) {
        AXLOG(makeString("updating children for axID "_s, update.first.loggingString()));
        if (RefPtr object = objectForID(update.first))
            object->setChildrenIDs(WTFMove(update.second));
    }
    m_pendingChildrenUpdates.clear();

    for (auto& change : m_pendingPropertyChanges) {
        if (RefPtr object = objectForID(change.axID)) {
            for (auto& property : change.properties)
                object->setProperty(property.first, WTFMove(property.second));
            object->shrinkPropertiesAfterUpdates();
        }
    }
    m_pendingPropertyChanges.clear();

    if (m_pendingSortedLiveRegionIDs)
        m_sortedLiveRegionIDs = std::exchange(m_pendingSortedLiveRegionIDs, std::nullopt).value();

    if (m_pendingSortedNonRootWebAreaIDs)
        m_sortedNonRootWebAreaIDs = std::exchange(m_pendingSortedNonRootWebAreaIDs, std::nullopt).value();

    if (m_pendingMostRecentlyPaintedText)
        m_mostRecentlyPaintedText = std::exchange(m_pendingMostRecentlyPaintedText, std::nullopt).value();

    if (m_pendingRelations)
        m_relations = std::exchange(m_pendingRelations, std::nullopt).value();

    if (m_pendingSelectedTextMarkerRange)
        m_selectedTextMarkerRange = std::exchange(m_pendingSelectedTextMarkerRange, std::nullopt).value();

    // Do this at the end because it requires looking up the root node by ID, so doing it at the end
    // ensures all additions to m_readerThreadNodeMap have been made by now.
    applyPendingRootNodeLocked();

    if (AXObjectCache::isAppleInternalInstall()) [[unlikely]]
        WTFEndSignpostAlways(this, AccessibilityIsolatedTreeApplyPendingChanges, "tree ID: %" PRIVATE_LOG_STRING "", treeID().loggingString().utf8().data());
}

void AXIsolatedTree::sortedLiveRegionsDidChange(Vector<AXID> liveRegionIDs)
{
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_pendingSortedLiveRegionIDs = WTFMove(liveRegionIDs);
}

void AXIsolatedTree::sortedNonRootWebAreasDidChange(Vector<AXID> webAreaIDs)
{
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    // FIXME: m_pendingSortedLiveRegionIDs and m_pendingSortedNonRootWebAreaIDs should be synced in AXIsolatedTree::processQueuedNodeUpdates(),
    // not ad-hoc whenever the main-thread changes them. Otherwise we could sync IDs to the accessibility thread that don't have isolated objects
    // until the next actual tree-update cycle.
    m_pendingSortedNonRootWebAreaIDs = WTFMove(webAreaIDs);
}

AXTreePtr findAXTree(Function<bool(AXTreePtr)>&& match)
{
    if (isMainThread()) {
        for (WeakPtr tree : AXTreeStore<AXObjectCache>::liveTreeMap().values()) {
            if (!tree)
                continue;

            if (match(tree))
                return tree;
        }
        return nullptr;
    }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    Locker locker { AXTreeStore<AXIsolatedTree>::s_storeLock };
    for (auto it = AXTreeStore<AXIsolatedTree>::isolatedTreeMap().begin(); it != AXTreeStore<AXIsolatedTree>::isolatedTreeMap().end(); ++it) {
        RefPtr tree = it->value.get();
        if (!tree)
            continue;

        if (match(tree))
            return tree;
    }
    return nullptr;
#endif
}

void AXIsolatedTree::queueNodeUpdate(AXID objectID, const NodeUpdateOptions& options)
{
    AX_DEBUG_ASSERT(isMainThread());

    if (!options.shouldUpdateNode && options.properties.size()) {
        // If we're going to recompute all properties for the node (i.e., the node is in m_needsUpdateNode),
        // don't bother queueing any individual property updates.
        if (m_needsUpdateNode.contains(objectID))
            return;

        auto addResult = m_needsPropertyUpdates.add(objectID, options.properties);
        if (!addResult.isNewEntry)
            addResult.iterator->value.addAll(options.properties);
    }

    if (options.shouldUpdateChildren)
        m_needsUpdateChildren.add(objectID);

    if (options.shouldUpdateNode)
        m_needsUpdateNode.add(objectID);

    if (auto* cache = axObjectCache())
        cache->startUpdateTreeSnapshotTimer();
}

void AXIsolatedTree::queueNodeRemoval(const AccessibilityObject& axObject)
{
    ASSERT(isMainThread());

    CheckedPtr axObjectCache = this->axObjectCache();
    std::optional labeledObjectIDs = axObjectCache ? axObjectCache->relatedObjectIDsFor(axObject, AXRelation::LabelFor, AXObjectCache::UpdateRelations::No) : std::nullopt;
    if (labeledObjectIDs) {
        // Update the labeled objects since axObject is one of their labels and it is being removed.
        for (AXID labeledObjectID : *labeledObjectIDs) {
            // The label/title of an isolated object is computed based on its AccessibilityText property, thus update it.
            queueNodeUpdate(labeledObjectID, { AXProperty::AccessibilityText });
        }
    }

    RefPtr parent = axObject.parentInCoreTree();
    std::optional<AXID> parentID = parent ? std::optional { parent->objectID() } : std::nullopt;

    m_needsNodeRemoval.add(axObject.objectID(), parentID);
    if (axObjectCache)
        axObjectCache->startUpdateTreeSnapshotTimer();
}

void AXIsolatedTree::processQueuedNodeUpdates()
{
    ASSERT(isMainThread());

    WeakPtr cache = axObjectCache();
    if (!cache)
        return;

    if (AXObjectCache::isAppleInternalInstall()) [[unlikely]]
        WTFBeginSignpostAlways(this, UpdateAccessibilityIsolatedTree, "updating isolated tree for AXObjectCache: %" PRIVATE_LOG_STRING "", cache ? CheckedPtr { cache.get() }->debugDescription().utf8().data() : "null");

    for (const auto& nodeIDs : m_needsNodeRemoval)
        removeNode(nodeIDs.key, nodeIDs.value);
    m_needsNodeRemoval.clear();

    for (AXID nodeID : m_needsUpdateChildren) {
        if (!cache)
            break;

        if (RefPtr axObject = cache->objectForID(nodeID))
            updateChildren(*axObject, ResolveNodeChanges::No);
    }
    m_needsUpdateChildren.clear();

    for (AXID objectID : m_needsUpdateNode)
        m_unresolvedPendingAppends.add(objectID);
    m_needsUpdateNode.clear();

    for (const auto& propertyUpdate : m_needsPropertyUpdates) {
        if (m_unresolvedPendingAppends.contains(propertyUpdate.key))
            continue;

        if (!cache)
            break;

        if (RefPtr axObject = cache->objectForID(propertyUpdate.key))
            updateNodeProperties(*axObject, propertyUpdate.value);
    }
    m_needsPropertyUpdates.clear();

    if (m_relationsNeedUpdate)
        updateRelations(cache->relations());

    if (m_mostRecentlyPaintedTextIsDirty) {
        m_mostRecentlyPaintedTextIsDirty = false;
        // Resolving `mostRecentlyPaintedText()` can result in this sequence:
        //   1. AXObjectCache::getOrCreate, which calls AccessibilityObject::recomputeIsIgnored
        //   2. If the ignored state changes, AXObjectCache::objectBecameUnignored may be called
        //   3. AXIsolatedTree::treeForFrameID() will be called to try to inform the isolated tree
        //      of this change, which requires taking AXTreeStore::s_storeLock.
        //
        // If we (the main-thread) held the m_changeLogLock when the above sequence happened, we would deadlock
        // if the accessibility thread was simultaneously running applyPendingChangesForAllIsolatedTrees(), which
        // holds the s_storeLock for the length of the function. The main-thread would be waiting on the storeLock,
        // and the accessibility thread would be waiting on the m_changeLogLock to run AXIsolatedTree::applyPendingChanges()
        // while holding the s_storeLock. Thus, a deadlock.
        //
        // So it's crucial to resolve the mostRecentlyPaintedText structure before the m_changeLogLock critical section,
        // and only perform a move or copy while in the critical section to avoid a deadlock.
        auto mostRecentlyPaintedText = cache->mostRecentlyPaintedText();
        Locker lock { m_changeLogLock };
        m_pendingMostRecentlyPaintedText = WTFMove(mostRecentlyPaintedText);
    }

    queueRemovalsAndUnresolvedChanges();

    if (AXObjectCache::isAppleInternalInstall()) [[unlikely]]
        WTFEndSignpostAlways(this, UpdateAccessibilityIsolatedTree);
}

#if ENABLE(AX_THREAD_TEXT_APIS)
AXTextMarker AXIsolatedTree::firstMarker()
{
    ASSERT(!isMainThread());
    // The first marker should be constructed from the root WebArea, not the true root of the tree
    // which is the ScrollView, so that when we convert the marker to a CharacterPosition, there
    // is an associated node. Otherwise, the CharacterPosition will be null.
    RefPtr webArea = rootWebArea();
    return webArea ? AXTextMarker { *webArea, 0 } : AXTextMarker();
}

AXTextMarker AXIsolatedTree::lastMarker()
{
    RefPtr root = rootNode();
    if (!root)
        return { };

    const auto& children = root->unignoredChildren();
    // Start the `findLast` traversal from the last child of the root to reduce the amount of traversal done.
    RefPtr endObject = children.isEmpty() ? root : dynamicDowncast<AXIsolatedObject>(children[children.size() - 1].get());
    return endObject ? AXTextMarker { *endObject, 0 }.findLast() : AXTextMarker();
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

std::optional<AXPropertyFlag> convertToPropertyFlag(AXProperty property)
{
    switch (property) {
    case AXProperty::CanSetFocusAttribute:
        return AXPropertyFlag::CanSetFocusAttribute;
    case AXProperty::CanSetSelectedAttribute:
        return AXPropertyFlag::CanSetSelectedAttribute;
    case AXProperty::CanSetValueAttribute:
        return AXPropertyFlag::CanSetValueAttribute;
    case AXProperty::HasBoldFont:
        return AXPropertyFlag::HasBoldFont;
    case AXProperty::HasClickHandler:
        return AXPropertyFlag::HasClickHandler;
    case AXProperty::HasCursorPointer:
        return AXPropertyFlag::HasCursorPointer;
    case AXProperty::HasItalicFont:
        return AXPropertyFlag::HasItalicFont;
    case AXProperty::HasPlainText:
        return AXPropertyFlag::HasPlainText;
    case AXProperty::HasPointerEventsNone:
        return AXPropertyFlag::HasPointerEventsNone;
    case AXProperty::IsBlockFlow:
        return AXPropertyFlag::IsBlockFlow;
    case AXProperty::IsEnabled:
        return AXPropertyFlag::IsEnabled;
    case AXProperty::IsExposedTableCell:
        return AXPropertyFlag::IsExposedTableCell;
    case AXProperty::IsGrabbed:
        return AXPropertyFlag::IsGrabbed;
    case AXProperty::IsHiddenUntilFoundContainer:
        return AXPropertyFlag::IsHiddenUntilFoundContainer;
    case AXProperty::IsIgnored:
        return AXPropertyFlag::IsIgnored;
    case AXProperty::IsInlineText:
        return AXPropertyFlag::IsInlineText;
    case AXProperty::IsKeyboardFocusable:
        return AXPropertyFlag::IsKeyboardFocusable;
    case AXProperty::IsNonLayerSVGObject:
        return AXPropertyFlag::IsNonLayerSVGObject;
    case AXProperty::IsExposedTableRow:
        return AXPropertyFlag::IsExposedTableRow;
    case AXProperty::IsVisited:
        return AXPropertyFlag::IsVisited;
    case AXProperty::ShowsCursorOnHover:
        return AXPropertyFlag::ShowsCursorOnHover;
    case AXProperty::SupportsCheckedState:
        return AXPropertyFlag::SupportsCheckedState;
    case AXProperty::SupportsDragging:
        return AXPropertyFlag::SupportsDragging;
    case AXProperty::SupportsExpanded:
        return AXPropertyFlag::SupportsExpanded;
    case AXProperty::SupportsPath:
        return AXPropertyFlag::SupportsPath;
    case AXProperty::SupportsPosInSet:
        return AXPropertyFlag::SupportsPosInSet;
    case AXProperty::SupportsSetSize:
        return AXPropertyFlag::SupportsSetSize;
    default:
        return std::nullopt;
    }
}

void setPropertyIn(AXProperty property, AXPropertyValueVariant&& value, AXPropertyVector& properties, OptionSet<AXPropertyFlag>& propertyFlags)
{
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        if (!*boolValue) {
            // Because this function is only for building the initial set of properties,
            // we don't need to handle a false value, as there never should be an attempt
            // to un-set (with false) a bool property that has already been set.
            // These asserts verify this.
            ASSERT(!properties.containsIf([&property] (const auto& propertyPair) {
                return propertyPair.first == property;
            }));
            ASSERT(!convertToPropertyFlag(property) || !propertyFlags.contains(*convertToPropertyFlag(property)));
            return;
        }

        if (std::optional propertyFlag = convertToPropertyFlag(property))
            propertyFlags.add(*propertyFlag);
        else
            properties.append(std::pair(property, WTFMove(value)));

        return;
    }

    if (!isDefaultValue(property, value))
        properties.append(std::pair(property, WTFMove(value)));
}

static bool shouldCacheElementName(ElementName name)
{
    switch (name) {
    case ElementName::HTML_area:
    case ElementName::HTML_abbr:
    case ElementName::HTML_acronym:
    case ElementName::HTML_body:
    case ElementName::HTML_del:
    case ElementName::HTML_h1:
    case ElementName::HTML_h2:
    case ElementName::HTML_h3:
    case ElementName::HTML_h4:
    case ElementName::HTML_h5:
    case ElementName::HTML_h6:
    case ElementName::HTML_ins:
    case ElementName::HTML_th:
    case ElementName::HTML_time:
#if ENABLE(AX_THREAD_TEXT_APIS)
    case ElementName::HTML_mark:
    case ElementName::HTML_attachment:
    case ElementName::HTML_thead:
    case ElementName::HTML_tbody:
    case ElementName::HTML_tfoot:
    case ElementName::HTML_output:
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        return true;
    default:
        return false;
    }
}

static bool canBeMultilineTextField(AccessibilityObject& object)
{
    if (object.isNonNativeTextControl())
        return !object.hasAttribute(aria_multilineAttr) || object.ariaIsMultiline();

    auto* renderer = object.renderer();
    if (renderer && renderer->isRenderTextControl())
        return renderer->isRenderTextControlMultiLine();

    // If we're not sure, return true, it means we can't use this as an optimization to avoid computing the line index.
    return true;
}

// Allocate a capacity based on the most common property count objects have (based on measurements from a real webpage).
// Based on said measurements, 59.6% objects have 2 or less properties. We'll shrink the vector at the end for objects
// that have less than 2.
static constexpr unsigned unignoredSizeToReserve = 2;
IsolatedObjectData createIsolatedObjectData(const Ref<AccessibilityObject>& axObject, Ref<AXIsolatedTree> tree)
{
    auto& object = axObject.get();

    bool getsGeometryFromChildren = false;
    AXPropertyVector properties;
    OptionSet<AXPropertyFlag> propertyFlags;
    auto setProperty = [&] (AXProperty property, AXPropertyValueVariant&& value) {
        setPropertyIn(property, WTFMove(value), properties, propertyFlags);
    };
    auto setObjectProperty = [&] (AXProperty property, AXCoreObject* object) {
        setProperty(property, object ? Markable { object->objectID() } : std::nullopt);
    };
    auto setObjectVectorProperty = [&] (AXProperty property, const AXCoreObject::AccessibilityChildrenVector& objects) {
        setProperty(property, axIDs(objects));
    };
    auto setMathscripts = [&] (AXProperty property, AccessibilityObject& object) {
        AXCoreObject::AccessibilityMathMultiscriptPairs pairs;
        if (property == AXProperty::MathPrescripts)
            object.mathPrescripts(pairs);
        else if (property == AXProperty::MathPostscripts)
            object.mathPostscripts(pairs);

        if (pairs.isEmpty())
            return;

        auto idPairs = pairs.map([](auto& mathPair) {
            return std::pair { mathPair.first ? Markable { mathPair.first->objectID() } : std::nullopt, mathPair.second ? Markable { mathPair.second->objectID() } : std::nullopt };
        });
        setProperty(property, WTFMove(idPairs));
    };

    auto reserveCapacityAndCacheBaseProperties = [&] (unsigned sizeToReserve) {
        if (sizeToReserve)
            properties.reserveInitialCapacity(sizeToReserve);

        // These properties are cached for all objects, ignored and unignored.
        setProperty(AXProperty::HasClickHandler, object.hasClickHandler());
        setProperty(AXProperty::HasCursorPointer, object.hasCursorPointer());
        setProperty(AXProperty::ShowsCursorOnHover, object.showsCursorOnHover());
        setProperty(AXProperty::HasPointerEventsNone, object.hasPointerEventsNone());
        auto elementName = object.elementName();
        if (shouldCacheElementName(elementName))
            setProperty(AXProperty::ElementName, elementName);
        setProperty(AXProperty::TitleAttribute, object.titleAttribute().isolatedCopy());

#if ENABLE(AX_THREAD_TEXT_APIS)
        setProperty(AXProperty::TextRuns, WTF::makeUnique<AXTextRuns>(object.textRuns()));
        switch (object.textEmissionBehavior()) {
        case TextEmissionBehavior::DoubleNewline:
            propertyFlags.add(AXPropertyFlag::IsTextEmissionBehaviorDoubleNewline);
            break;
        case TextEmissionBehavior::Newline:
            propertyFlags.add(AXPropertyFlag::IsTextEmissionBehaviorNewline);
            break;
        case TextEmissionBehavior::Tab:
            propertyFlags.add(AXPropertyFlag::IsTextEmissionBehaviorTab);
            break;
        case TextEmissionBehavior::None:
            break;
        }
        if (object.role() == AccessibilityRole::ListMarker) {
            setProperty(AXProperty::ListMarkerText, object.listMarkerText().isolatedCopy());
            setProperty(AXProperty::ListMarkerLineID, object.listMarkerLineID());
        }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

        String language = object.language();
        if (!language.isEmpty())
            setProperty(AXProperty::Language, WTFMove(language).isolatedCopy());
        setProperty(AXProperty::IsEnabled, object.isEnabled());
        setProperty(AXProperty::IsHiddenUntilFoundContainer, object.isHiddenUntilFoundContainer());
        if (object.isBlockFlow()) {
            setProperty(AXProperty::IsBlockFlow, true);
            setProperty(AXProperty::StitchGroups, object.stitchGroups());
        }
        appendBasePlatformProperties(properties, propertyFlags, axObject);

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
        if (object.isLocalFrame()) {
            if (auto* localFrame = dynamicDowncast<AXLocalFrame>(&object)) {
                if (std::optional frameID = localFrame->frameID())
                    setProperty(AXProperty::CrossFrameChildFrameID, *frameID);
            }
        }
#endif
    };

    bool needsAllProperties = true;
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    bool isIgnored = true;
    if (object.includeIgnoredInCoreTree()) {
        isIgnored = object.isIgnored();
        setProperty(AXProperty::IsIgnored, isIgnored);

        // Do not set any properties in this block, as this is before we reserve capacity for the property vector.

        // Maintain full properties for objects meeting this criteria:
        //   - Unconnected objects, which are involved in relations or outgoing notifications
        //   - Static text. We sometimes ignore static text (e.g. because it descends from a text field),
        //     but need full properties for proper text marker behavior.
        // FIXME: We shouldn't cache all properties for empty / non-rendered text?
        needsAllProperties = !isIgnored || tree->isUnconnectedNode(object.objectID()) || is<RenderText>(object.renderer());
    }
#else
    reserveCapacityAndCacheBaseProperties(unignoredSizeToReserve);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    if (!needsAllProperties)
        reserveCapacityAndCacheBaseProperties(0);
    else {
        reserveCapacityAndCacheBaseProperties(unignoredSizeToReserve);

        setProperty(AXProperty::IsAttachment, object.isAttachment());
        setProperty(AXProperty::IsBusy, object.isBusy());
        setProperty(AXProperty::IsExpanded, object.isExpanded());

        // FIXME: Caching isSecureField would require caching an additional property (on top of input type), so for now, let's still cache this.
        setProperty(AXProperty::IsSecureField, object.isSecureField());

        setProperty(AXProperty::IsIndeterminate, object.isIndeterminate());
        setProperty(AXProperty::IsInlineText, object.isInlineText());
        setProperty(AXProperty::IsMultiSelectable, object.isMultiSelectable());
        setProperty(AXProperty::IsRequired, object.isRequired());
        setProperty(AXProperty::IsSelected, object.isSelected());
        setProperty(AXProperty::IsVisited, object.isVisited());
        setProperty(AXProperty::IsValueAutofillAvailable, object.isValueAutofillAvailable());
        setProperty(AXProperty::ARIARoleDescription, object.ariaRoleDescription().isolatedCopy());
        setProperty(AXProperty::SubrolePlatformString, object.subrolePlatformString().isolatedCopy());
        setProperty(AXProperty::CanSetFocusAttribute, object.canSetFocusAttribute());
        setProperty(AXProperty::CanSetValueAttribute, object.canSetValueAttribute());
        setProperty(AXProperty::CanSetSelectedAttribute, object.canSetSelectedAttribute());
        setProperty(AXProperty::ValueDescription, object.valueDescription().isolatedCopy());
        setProperty(AXProperty::ValueForRange, object.valueForRange());
        setProperty(AXProperty::MaxValueForRange, object.maxValueForRange());
        setProperty(AXProperty::MinValueForRange, object.minValueForRange());
        setProperty(AXProperty::SupportsARIAOwns, object.supportsARIAOwns());
        setProperty(AXProperty::ExplicitPopupValue, object.explicitPopupValue().isolatedCopy());
        setProperty(AXProperty::ExplicitInvalidStatus, object.explicitInvalidStatus().isolatedCopy());
        setProperty(AXProperty::SupportsExpanded, object.supportsExpanded());
        setProperty(AXProperty::SortDirection, static_cast<int>(object.sortDirection()));
#if !LOG_DISABLED
        // Eagerly cache ID when logging is enabled so that we can log isolated objects without constant deadlocks.
        // Don't cache ID when logging is disabled because we don't expect non-test AX clients to actually request it.
        setProperty(AXProperty::IdentifierAttribute, object.identifierAttribute().isolatedCopy());
#endif
        // FIXME: We never update AXProperty::SupportsDropping.
        setProperty(AXProperty::SupportsDropping, object.supportsDropping());
        setProperty(AXProperty::SupportsDragging, object.supportsDragging());
        setProperty(AXProperty::IsGrabbed, object.isGrabbed());
        setProperty(AXProperty::PlaceholderValue, object.placeholderValue().isolatedCopy());
        setProperty(AXProperty::ValueAutofillButtonType, static_cast<int>(object.valueAutofillButtonType()));
        setProperty(AXProperty::URL, WTF::makeUnique<URL>(object.url().isolatedCopy()));
        setProperty(AXProperty::AccessKey, object.accessKey().isolatedCopy());
        setProperty(AXProperty::ExplicitAutoCompleteValue, object.explicitAutoCompleteValue().isolatedCopy());
        setProperty(AXProperty::ColorValue, object.colorValue());
        if (std::optional orientation = object.explicitOrientation())
            setProperty(AXProperty::ExplicitOrientation, *orientation);
        setProperty(AXProperty::ExplicitLiveRegionStatus, object.explicitLiveRegionStatus().isolatedCopy());
        setProperty(AXProperty::ExplicitLiveRegionRelevant, object.explicitLiveRegionRelevant().isolatedCopy());
        setProperty(AXProperty::LiveRegionAtomic, object.liveRegionAtomic());
        setProperty(AXProperty::HasBoldFont, object.hasBoldFont());
        setProperty(AXProperty::HasItalicFont, object.hasItalicFont());
        setProperty(AXProperty::HasPlainText, object.hasPlainText());
#if !ENABLE(AX_THREAD_TEXT_APIS)
        setProperty(AXProperty::TextContentPrefixFromListMarker, object.textContentPrefixFromListMarker());
#endif
        setProperty(AXProperty::IsKeyboardFocusable, object.isKeyboardFocusable());
        setProperty(AXProperty::BrailleRoleDescription, object.brailleRoleDescription().isolatedCopy());
        setProperty(AXProperty::BrailleLabel, object.brailleLabel().isolatedCopy());
        setProperty(AXProperty::IsNonLayerSVGObject, object.isNonLayerSVGObject());

        // Only cache input types on things that are an input.
        if (std::optional inputType = axObject->inputType())
            setProperty(AXProperty::InputType, *inputType);

        bool isWebArea = axObject->isWebArea();
        bool isScrollArea = axObject->isScrollView();
        if (isScrollArea && !axObject->parentObject()) {
            // Eagerly cache the screen relative position for the root. AXIsolatedObject::screenRelativePosition()
            // of non-root objects depend on the root object's screen relative position, so make sure it's there
            // from the start. We keep this up-to-date via AXIsolatedTree::updateRootScreenRelativePosition().
            setProperty(AXProperty::ScreenRelativePosition, axObject->screenRelativePosition());
            // FIXME: We never update this property, e.g. when the iframe is moved in the hosting web content process.
            setProperty(AXProperty::RemoteFrameOffset, object.remoteFrameOffset());

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
            RefPtr crossFrameParent = axObject->crossFrameParentObject();
            if (crossFrameParent) {
                WeakPtr parentCache = crossFrameParent->axObjectCache();
                if (parentCache && parentCache->frameID()) {
                    setProperty(AXProperty::CrossFrameParentFrameID, *parentCache->frameID());
                    setProperty(AXProperty::CrossFrameParentAXID, Markable { crossFrameParent->objectID() });
                }
            }
#endif // ENABLE_ACCESSIBILITY_LOCAL_FRAME
        }

        RefPtr geometryManager = tree->geometryManager();
        std::optional frame = geometryManager ? geometryManager->cachedRectForID(object.objectID()) : std::nullopt;
        if (frame)
            setProperty(AXProperty::RelativeFrame, WTFMove(*frame));
        else if (isScrollArea || isWebArea || object.isScrollbar()) {
            // The GeometryManager does not have a relative frame for ScrollViews, WebAreas, or scrollbars yet. We need to get it from the
            // live object so that we don't need to hit the main thread in the case a request comes in while the whole isolated tree is being built.
            setProperty(AXProperty::RelativeFrame, enclosingIntRect(object.relativeFrame()));
        } else if (!object.renderer() && object.node() && is<AccessibilityNodeObject>(object) && !object.isImageMapLink()) {
            // The frame of node-only AX objects is made up of their children.
            // This excludes image-map links (a.k.a. <area> elements), which will never have rendered children.
            getsGeometryFromChildren = true;
        } else if (object.isMenuListPopup()) {
            // AccessibilityMenuListPopup's elementRect is hardcoded to return an empty rect, so preserve that behavior.
            setProperty(AXProperty::RelativeFrame, IntRect());
        } else
            setProperty(AXProperty::InitialLocalRect, object.localRect());

        if (isWebArea)
            setProperty(AXProperty::IsEditableWebArea, object.isEditableWebArea());

        if (object.supportsPath()) {
            setProperty(AXProperty::SupportsPath, true);
            setProperty(AXProperty::Path, WTF::makeUnique<Path>(object.elementPath()));
        }

        if (object.supportsKeyShortcuts()) {
            setProperty(AXProperty::SupportsKeyShortcuts, true);
            setProperty(AXProperty::KeyShortcuts, object.keyShortcuts().isolatedCopy());
        }

        if (object.supportsCurrent()) {
            setProperty(AXProperty::SupportsCurrent, true);
            setProperty(AXProperty::CurrentState, static_cast<int>(object.currentState()));
        }

        if (object.supportsSetSize()) {
            setProperty(AXProperty::SupportsSetSize, true);
            setProperty(AXProperty::SetSize, object.setSize());
        }

        if (object.supportsPosInSet()) {
            setProperty(AXProperty::SupportsPosInSet, true);
            setProperty(AXProperty::PosInSet, object.posInSet());
        }

        if (object.supportsDatetimeAttribute())
            setProperty(AXProperty::DatetimeAttributeValue, object.datetimeAttributeValue().isolatedCopy());

        if (object.supportsCheckedState()) {
            setProperty(AXProperty::SupportsCheckedState, true);
            setProperty(AXProperty::IsChecked, object.isChecked());
            setProperty(AXProperty::ButtonState, object.checkboxOrRadioValue());
        }

        if (object.isTable()) {
            setProperty(AXProperty::IsTable, true);
            setProperty(AXProperty::IsExposableTable, object.isExposableTable());
            setObjectVectorProperty(AXProperty::Columns, object.columns());
            setObjectVectorProperty(AXProperty::Rows, object.rows());
            setObjectVectorProperty(AXProperty::Cells, object.cells());
            setObjectVectorProperty(AXProperty::VisibleRows, object.visibleRows());
            setProperty(AXProperty::AXColumnCount, object.axColumnCount());
            setProperty(AXProperty::AXRowCount, object.axRowCount());
            setProperty(AXProperty::CellSlots, object.cellSlots());
        }

        if (object.isExposedTableCell()) {
            setProperty(AXProperty::IsExposedTableCell, true);
            setProperty(AXProperty::ColumnIndexRange, object.columnIndexRange());
            setProperty(AXProperty::RowIndexRange, object.rowIndexRange());
            if (std::optional columnIndex = object.axColumnIndex())
                setProperty(AXProperty::AXColumnIndex, *columnIndex);
            if (std::optional rowIndex = object.axRowIndex())
                setProperty(AXProperty::AXRowIndex, *rowIndex);
            setProperty(AXProperty::AXColumnIndexText, object.axColumnIndexText().isolatedCopy());
            setProperty(AXProperty::AXRowIndexText, object.axRowIndexText().isolatedCopy());
            setProperty(AXProperty::IsColumnHeader, object.isColumnHeader());
            setProperty(AXProperty::IsRowHeader, object.isRowHeader());
            setProperty(AXProperty::CellScope, object.cellScope().isolatedCopy());
            setProperty(AXProperty::Abbreviation, object.abbreviation().isolatedCopy());
        }

        bool isExposedTableRow = object.isExposedTableRow();
        if (isExposedTableRow) {
            setProperty(AXProperty::IsExposedTableRow, true);
            setProperty(AXProperty::RowIndex, object.rowIndex());
        } else if (object.isTableColumn())
            setProperty(AXProperty::ColumnIndex, object.columnIndex());

        if (object.isARIATreeGridRow()) {
            setProperty(AXProperty::IsARIATreeGridRow, true);
            setObjectVectorProperty(AXProperty::DisclosedRows, object.disclosedRows());
            setObjectProperty(AXProperty::DisclosedByRow, object.disclosedByRow());
        } else if (object.isARIAGridRow())
            setProperty(AXProperty::IsARIAGridRow, true);

        bool isTreeItem = object.isTreeItem();
        if (isTreeItem) {
            setProperty(AXProperty::IsTreeItem, true);
            setObjectVectorProperty(AXProperty::DisclosedRows, object.disclosedRows());
        }

        setProperty(AXProperty::IsTree, object.isTree());
        if (object.isRadioButton()) {
            setProperty(AXProperty::NameAttribute, object.nameAttribute().isolatedCopy());
            setObjectVectorProperty(AXProperty::RadioButtonGroupMembers, object.radioButtonGroup());
        }

        if (object.isImage())
            setProperty(AXProperty::EmbeddedImageDescription, object.embeddedImageDescription().isolatedCopy());

        // On macOS, we only advertise support for the visible children attribute for lists and listboxes.
        if (object.isList() || object.isListBox())
            setObjectVectorProperty(AXProperty::VisibleChildren, object.visibleChildren());

        if (object.isDateTime()) {
            setProperty(AXProperty::DateTimeValue, object.dateTimeValue().isolatedCopy());
            setProperty(AXProperty::DateTimeComponentsType, object.dateTimeComponentsType());
        }

        if (object.isSpinButton()) {
            setObjectProperty(AXProperty::DecrementButton, object.decrementButton());
            setObjectProperty(AXProperty::IncrementButton, object.incrementButton());
        }

        if (object.isMathElement()) {
            setProperty(AXProperty::IsMathElement, true);
            setProperty(AXProperty::IsMathFraction, object.isMathFraction());
            setProperty(AXProperty::IsMathFenced, object.isMathFenced());
            setProperty(AXProperty::IsMathSubscriptSuperscript, object.isMathSubscriptSuperscript());
            setProperty(AXProperty::IsMathRow, object.isMathRow());
            setProperty(AXProperty::IsMathUnderOver, object.isMathUnderOver());
            setProperty(AXProperty::IsMathTable, object.isMathTable());
            setProperty(AXProperty::IsMathTableRow, object.isMathTableRow());
            setProperty(AXProperty::IsMathTableCell, object.isMathTableCell());
            setProperty(AXProperty::IsMathMultiscript, object.isMathMultiscript());
            setProperty(AXProperty::IsMathToken, object.isMathToken());
            setProperty(AXProperty::MathFencedOpenString, object.mathFencedOpenString().isolatedCopy());
            setProperty(AXProperty::MathFencedCloseString, object.mathFencedCloseString().isolatedCopy());
            setProperty(AXProperty::MathLineThickness, object.mathLineThickness());
            setProperty(AXProperty::IsAnonymousMathOperator, object.isAnonymousMathOperator());

            bool isMathRoot = object.isMathRoot();
            setProperty(AXProperty::IsMathRoot, isMathRoot);
            setProperty(AXProperty::IsMathSquareRoot, object.isMathSquareRoot());
            if (isMathRoot) {
                if (auto radicand = object.mathRadicand())
                    setObjectVectorProperty(AXProperty::MathRadicand, *radicand);

                setObjectProperty(AXProperty::MathRootIndexObject, object.mathRootIndexObject());
            }

            setObjectProperty(AXProperty::MathUnderObject, object.mathUnderObject());
            setObjectProperty(AXProperty::MathOverObject, object.mathOverObject());
            setObjectProperty(AXProperty::MathNumeratorObject, object.mathNumeratorObject());
            setObjectProperty(AXProperty::MathDenominatorObject, object.mathDenominatorObject());
            setObjectProperty(AXProperty::MathBaseObject, object.mathBaseObject());
            setObjectProperty(AXProperty::MathSubscriptObject, object.mathSubscriptObject());
            setObjectProperty(AXProperty::MathSuperscriptObject, object.mathSuperscriptObject());
            setMathscripts(AXProperty::MathPrescripts, object);
            setMathscripts(AXProperty::MathPostscripts, object);
        }

        Vector<AccessibilityText> texts;
        object.accessibilityText(texts);
        auto axTextValue = texts.map([] (const auto& text) -> AccessibilityText {
            return { text.text.isolatedCopy(), text.textSource };
        });
        setProperty(AXProperty::AccessibilityText, axTextValue);

        if (isScrollArea) {
            setObjectProperty(AXProperty::VerticalScrollBar, object.scrollBar(AccessibilityOrientation::Vertical));
            setObjectProperty(AXProperty::HorizontalScrollBar, object.scrollBar(AccessibilityOrientation::Horizontal));
            setProperty(AXProperty::HasRemoteFrameChild, object.hasRemoteFrameChild());
        } else if (isWebArea && !tree->isEmptyContentTree()) {
            // We expose DocumentLinks only for the web area objects when the tree is not an empty content tree. This property is expensive and makes no sense in an empty content tree.
            // FIXME: compute DocumentLinks on the AX thread instead of caching it.
            setObjectVectorProperty(AXProperty::DocumentLinks, object.documentLinks());
        }

        if (object.isWidget()) {
            if (object.isPlugin()) {
                // Plugins are a subclass of widget, so we only need to cache IsPlugin, and we implicitly know
                // this is also a widget (see AXIsolatedObject::isWidget).
                setProperty(AXProperty::IsPlugin, true);
            } else
                setProperty(AXProperty::IsWidget, true);

            setProperty(AXProperty::IsVisible, object.isVisible());
        }

        if (String description = object.description(); description.length())
            setProperty(AXProperty::Description, WTFMove(description).isolatedCopy());

        if (String extendedDescription = object.extendedDescription(); extendedDescription.length())
            setProperty(AXProperty::ExtendedDescription, WTFMove(extendedDescription).isolatedCopy());

        if (object.isTextControl()) {
            // FIXME: We don't keep this property up-to-date, and we can probably just compute it using
            // AXIsolatedObject::selectedTextMarkerRange() (which does stay up-to-date).
            setProperty(AXProperty::SelectedTextRange, object.selectedTextRange());

            auto range = object.textInputMarkedTextMarkerRange();
            if (auto characterRange = range.characterRange(); range && characterRange)
                setProperty(AXProperty::TextInputMarkedTextMarkerRange, WTF::makeUnique<AXIDAndCharacterRange>(AXIDAndCharacterRange(range.start().objectID(), *characterRange)));

            setProperty(AXProperty::CanBeMultilineTextField, canBeMultilineTextField(object));
        }

        if (object.isHeading() || isExposedTableRow || isTreeItem)
            setProperty(AXProperty::ARIALevel, object.ariaLevel());

        // These properties are only needed on the AXCoreObject interface due to their use in ATSPI,
        // so only cache them for ATSPI.
#if USE(ATSPI)
        // We cache IsVisible on all platforms just for Widgets above. In ATSPI, this should be cached on all objects.
        if (!object.isWidget())
            setProperty(AXProperty::IsVisible, object.isVisible());

        setProperty(AXProperty::ActionVerb, object.actionVerb().isolatedCopy());
        setProperty(AXProperty::IsFieldset, object.isFieldset());
        setProperty(AXProperty::IsPressed, object.isPressed());
        setProperty(AXProperty::IsSelectedOptionActive, object.isSelectedOptionActive());
        setProperty(AXProperty::LocalizedActionVerb, object.localizedActionVerb().isolatedCopy());
#endif // USE(ATSPI)
        setObjectProperty(AXProperty::InternalLinkElement, object.internalLinkElement());
    }

    appendPlatformProperties(properties, propertyFlags, axObject);

    RefPtr axParent = object.parentInCoreTree();
    Markable<AXID> parentID = axParent ? std::optional(axParent->objectID()) : std::nullopt;

#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (isIgnored) {
        if (String text = axObject->revealableText(); !text.isEmpty()) {
            // We only need to cache this for ignored objects, as unignored objects
            // have no need to be revealed.
            setProperty(AXProperty::RevealableText, WTFMove(text).isolatedCopy());
        }
    }
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    properties.shrinkToFit();
    return {
        object.childrenIDs(),
        WTFMove(properties),
        WTFMove(tree),
        parentID,
        object.objectID(),
        object.role(),
        propertyFlags,
        getsGeometryFromChildren
    };
}

} // namespace WebCore
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
