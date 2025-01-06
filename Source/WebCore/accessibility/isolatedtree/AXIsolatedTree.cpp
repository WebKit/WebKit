/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableRow.h"
#include "FrameSelection.h"
#include "LocalFrameView.h"
#include "Page.h"
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXIsolatedTree);
WTF_MAKE_TZONE_ALLOCATED_IMPL(AXIsolatedTree);

static const Seconds CreationFeedbackInterval { 3_s };

UncheckedKeyHashMap<PageIdentifier, Ref<AXIsolatedTree>>& AXIsolatedTree::treePageCache()
{
    static NeverDestroyed<UncheckedKeyHashMap<PageIdentifier, Ref<AXIsolatedTree>>> map;
    return map;
}

AXIsolatedTree::AXIsolatedTree(AXObjectCache& axObjectCache)
    : AXTreeStore(axObjectCache.treeID())
    , m_axObjectCache(&axObjectCache)
    , m_pageActivityState(axObjectCache.pageActivityState())
    , m_geometryManager(axObjectCache.m_geometryManager.ptr())
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
    ASSERT(axObjectCache.pageID());

    auto tree = adoptRef(*new AXIsolatedTree(axObjectCache));

    RefPtr axRoot = axObjectCache.getOrCreate(axObjectCache.document().view());
    if (axRoot) {
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
    auto root = AXIsolatedObject::create(axRoot, this);
    root->setProperty(AXProperty::ScreenRelativePosition, axRoot.screenRelativePosition());
    NodeChange rootAppend { root, axRoot.wrapper(), AttachWrapper::OnMainThread };

    RefPtr axWebArea = Accessibility::findUnignoredChild(axRoot, [] (auto& object) {
        return object->isWebArea();
    });
    if (!axWebArea) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto webArea = AXIsolatedObject::create(*axWebArea, this);
    webArea->setProperty(AXProperty::ScreenRelativePosition, axWebArea->screenRelativePosition());
    NodeChange webAreaAppend { webArea, axWebArea->wrapper(), AttachWrapper::OnMainThread };

    m_nodeMap.set(root->objectID(), ParentChildrenIDs { std::nullopt, { webArea->objectID() } });
    m_nodeMap.set(webArea->objectID(), ParentChildrenIDs { root->objectID(), { } });

    {
        Locker locker { m_changeLogLock };
        // Typically, m_rootNode is only allowed to be read or written on the AX thread.
        // However, we have not called `storeTree` with `this` yet, so there's no way
        // for this tree to actually be in use by the AX thread.
        m_rootNode = root.ptr();
        m_pendingFocusedNodeID = webArea->objectID();
    }
    queueAppendsAndRemovals({ rootAppend, webAreaAppend }, { });
}

Ref<AXIsolatedTree> AXIsolatedTree::create(AXObjectCache& axObjectCache)
{
    AXTRACE("AXIsolatedTree::create"_s);
    ASSERT(isMainThread());
    ASSERT(axObjectCache.pageID());

    auto tree = adoptRef(*new AXIsolatedTree(axObjectCache));
    if (RefPtr existingTree = isolatedTreeForID(tree->treeID()))
        tree->m_replacingTree = existingTree;

    auto& document = axObjectCache.document();
    if (!Accessibility::inRenderTreeOrStyleUpdate(document))
        document.updateLayoutIgnorePendingStylesheets();

    tree->m_maxTreeDepth = document.settings().maximumHTMLParserDOMTreeDepth();
    ASSERT(tree->m_maxTreeDepth);

    // Generate the nodes of the tree and set its root and focused objects.
    // For this, we need the root and focused objects of the AXObject tree.
    auto* axRoot = axObjectCache.getOrCreate(document.view());
    if (axRoot)
        tree->generateSubtree(*axRoot);
    tree->applyPendingRootNode();

    auto* axFocus = axObjectCache.focusedObjectForPage(document.page());
    if (axFocus)
        tree->setFocusedNodeID(axFocus->objectID());
    tree->setSelectedTextMarkerRange(document.selection().selection());
    tree->updateLoadingProgress(axObjectCache.loadingProgress());

    const auto relations = axObjectCache.relations();
    tree->updateRelations(relations);

    for (auto& relatedObjectID : relations.keys()) {
        RefPtr axObject = axObjectCache.objectForID(relatedObjectID);
        if (axObject && axObject->isIgnored())
            tree->addUnconnectedNode(axObject.releaseNonNull());
    }

    // Now that the tree is ready to take client requests, add it to the tree maps so that it can be found.
    storeTree(axObjectCache, tree);
    return tree;
}

void AXIsolatedTree::applyPendingRootNode()
{
    Locker locker { m_changeLogLock };
    if (m_pendingRootNode)
        m_rootNode = std::exchange(m_pendingRootNode, { });
}

void AXIsolatedTree::storeTree(AXObjectCache& cache, const Ref<AXIsolatedTree>& tree)
{
    ASSERT(isMainThread());

    // Once we set this tree in the AXTreeStore, the secondary thread can start using it,
    // and we can no longer access AXIsolatedTree::rootNode off the main-thread. Set the
    // root now while we still can.
    cache.setIsolatedTreeRoot(tree->rootNode());
    AXTreeStore::set(tree->treeID(), tree.ptr());
    tree->m_replacingTree = nullptr;
    Locker locker { s_storeLock };
    treePageCache().set(*cache.pageID(), tree.copyRef());
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
        overrideNodeProperties(axWebArea->objectID(), {
            { AXProperty::TitleAttributeValue, WTFMove(title) },
        });
        if (cache)
            cache->postPlatformNotification(*axWebArea, AXNotification::LayoutComplete);
    }
}

void AXIsolatedTree::removeTreeForPageID(PageIdentifier pageID)
{
    AXTRACE("AXIsolatedTree::removeTreeForPageID"_s);
    ASSERT(isMainThread());

    Locker locker { s_storeLock };
    if (auto tree = treePageCache().take(pageID)) {
        tree->m_geometryManager = nullptr;
        tree->queueForDestruction();
    }
}

RefPtr<AXIsolatedTree> AXIsolatedTree::treeForPageID(PageIdentifier pageID)
{
    Locker locker { s_storeLock };
    if (auto tree = treePageCache().get(pageID))
        return tree;
    return nullptr;
}

AXIsolatedObject* AXIsolatedTree::objectForID(AXID axID) const
{
    ASSERT(!isMainThread());
    return m_readerThreadNodeMap.get(axID);
}

void AXIsolatedTree::generateSubtree(AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::generateSubtree"_s);
    ASSERT(isMainThread());

    if (axObject.isDetached())
        return;

    // We're about to a lot of read-only work, so start the attribute cache.
    AXAttributeCacheEnabler enableCache(axObject.axObjectCache());
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

std::optional<AXIsolatedTree::NodeChange> AXIsolatedTree::nodeChangeForObject(Ref<AccessibilityObject> axObject, AttachWrapper attachWrapper)
{
    ASSERT(isMainThread());
    ASSERT(!axObject->isDetached());

    if (!shouldCreateNodeChange(axObject.get()))
        return std::nullopt;

    auto object = AXIsolatedObject::create(axObject, this);
    ASSERT(axObject->wrapper());
    NodeChange nodeChange { object, axObject->wrapper(), attachWrapper };

    m_nodeMap.set(axObject->objectID(), ParentChildrenIDs { nodeChange.isolatedObject->parent(), axObject->childrenIDs() });

    if (!nodeChange.isolatedObject->parent() && nodeChange.isolatedObject->isScrollView()) {
        Locker locker { m_changeLogLock };
        setPendingRootNodeLocked(nodeChange.isolatedObject.get());
    }

    return nodeChange;
}

void AXIsolatedTree::queueChange(const NodeChange& nodeChange)
{
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    m_pendingAppends.append(nodeChange);

    auto parentID = nodeChange.isolatedObject->parent();
    if (parentID) {
        auto siblingsIDs = m_nodeMap.get(*parentID).childrenIDs;
        m_pendingChildrenUpdates.append({ *parentID, WTFMove(siblingsIDs) });
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
    auto object = AXIsolatedObject::create(axObject, this);
    object->attachPlatformWrapper(axObject->wrapper());

    NodeChange nodeChange { object, nullptr };
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

    m_pendingSubtreeRemovals.appendVector(WTFMove(subtreeRemovals));
    m_pendingProtectedFromDeletionIDs.formUnion(std::exchange(m_protectedFromDeletionIDs, { }));
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
    for (const auto& unresolvedAppend : unresolvedPendingAppends) {
        if (m_replacingTree) {
            ++counter;
            if (MonotonicTime::now() - lastFeedbackTime > CreationFeedbackInterval) {
                m_replacingTree->reportLoadingProgress(counter / m_unresolvedPendingAppends.size());
                lastFeedbackTime = MonotonicTime::now();
            }
        }

        if (auto* axObject = cache->objectForID(unresolvedAppend.key)) {
            if (auto nodeChange = nodeChangeForObject(*axObject, unresolvedAppend.value))
                resolvedAppends.append(*nodeChange);
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

    Locker locker { m_changeLogLock };
    for (const auto& append : appends) {
        if (append.attachWrapper == AttachWrapper::OnMainThread)
            append.isolatedObject->attachPlatformWrapper(append.wrapper.get());
        queueChange(append);
    }

    auto parentUpdateIDs = std::exchange(m_needsParentUpdate, { });
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

    SetForScope collectingAtTreeLevel(m_collectingNodeChangesAtTreeLevel, m_collectingNodeChangesAtTreeLevel + 1);
    if (m_collectingNodeChangesAtTreeLevel >= m_maxTreeDepth)
        return;

    auto* axParent = axObject.parentInCoreTree();
    auto parentID = axParent ? std::optional { axParent->objectID() } : std::nullopt;
    auto axChildrenCopy = axObject.children();

    auto iterator = m_nodeMap.find(axObject.objectID());
    if (iterator == m_nodeMap.end()) {
        m_unresolvedPendingAppends.set(axObject.objectID(), AttachWrapper::OnMainThread);

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
        m_unresolvedPendingAppends.ensure(axObject.objectID(), [] {
            return AttachWrapper::OnAXThread;
        });
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

    auto* axParent = axObject.parentInCoreTree();
    if (!axParent)
        return;

    if (auto change = nodeChangeForObject(*axParent, AttachWrapper::OnAXThread)) {
        Locker locker { m_changeLogLock };
        queueChange(WTFMove(*change));
    }
}

void AXIsolatedTree::objectChangedIgnoredState(const AccessibilityObject& object)
{
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    ASSERT(isMainThread());

    if (auto* cell = dynamicDowncast<AccessibilityTableCell>(object)) {
        if (auto* parentTable = cell->parentTable()) {
            // FIXME: This should be as simple as:
            //     queueNodeUpdate(*parentTable->objectID(), { { AXProperty::Cells, AXProperty::CellSlots, AXProperty::Columns } });
            // As these are the table properties that depend on cells. But we can't do that, because we compute "new" column accessibility objects
            // every time we clearChildren() and addChildren(), so just re-computing AXProperty::Columns means that we won't have AXIsolatedObjects
            // for the columns. Instead we have to do a significantly more wasteful children update.
            queueNodeUpdate(parentTable->objectID(), NodeUpdateOptions::childrenUpdate());
            queueNodeUpdate(parentTable->objectID(), { { AXProperty::Cells, AXProperty::CellSlots } });
        }
    }

    if (object.isLink()) {
        if (RefPtr webArea = m_axObjectCache ? m_axObjectCache->rootWebArea() : nullptr)
            queueNodeUpdate(webArea->objectID(), { AXProperty::DocumentLinks });
    }
#else
    UNUSED_PARAM(object);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
}

void AXIsolatedTree::updatePropertiesForSelfAndDescendants(AccessibilityObject& axObject, const AXPropertySet& properties)
{
    AXTRACE("AXIsolatedTree::updatePropertiesForSelfAndDescendants"_s);
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    AXPropertySet propertySet;
    for (const auto& property : properties)
        propertySet.add(property);

    Accessibility::enumerateDescendantsIncludingIgnored<AXCoreObject>(axObject, true, [&propertySet, this] (auto& descendant) {
        queueNodeUpdate(descendant.objectID(), { propertySet });
    });
}

void AXIsolatedTree::updateNodeProperties(AccessibilityObject& axObject, const AXPropertySet& properties)
{
    AXTRACE("AXIsolatedTree::updateNodeProperties"_s);
    AXLOG(makeString("Updating properties for objectID "_s, axObject.objectID().loggingString(), ": "_s));
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    AXPropertyMap propertyMap;
    for (const auto& property : properties) {
        AXLOG(makeString("Property: "_s, property));
        switch (property) {
        case AXProperty::AccessKey:
            propertyMap.set(AXProperty::AccessKey, axObject.accessKey().isolatedCopy());
            break;
        case AXProperty::AccessibilityText: {
            Vector<AccessibilityText> texts;
            axObject.accessibilityText(texts);
            auto axTextValue = texts.map([] (const auto& text) -> AccessibilityText {
                return { text.text.isolatedCopy(), text.textSource };
            });
            propertyMap.set(AXProperty::AccessibilityText, axTextValue);
            break;
        }
        case AXProperty::ValueAutofillButtonType:
            propertyMap.set(AXProperty::ValueAutofillButtonType, static_cast<int>(axObject.valueAutofillButtonType()));
            propertyMap.set(AXProperty::IsValueAutofillAvailable, axObject.isValueAutofillAvailable());
            break;
        case AXProperty::AXColumnCount:
            propertyMap.set(AXProperty::AXColumnCount, axObject.axColumnCount());
            break;
        case AXProperty::BrailleLabel:
            propertyMap.set(AXProperty::BrailleLabel, axObject.brailleLabel().isolatedCopy());
            break;
        case AXProperty::BrailleRoleDescription:
            propertyMap.set(AXProperty::BrailleRoleDescription, axObject.brailleRoleDescription().isolatedCopy());
            break;
        case AXProperty::AXColumnIndex:
            propertyMap.set(AXProperty::AXColumnIndex, axObject.axColumnIndex());
            break;
        case AXProperty::CanSetFocusAttribute:
            propertyMap.set(AXProperty::CanSetFocusAttribute, axObject.canSetFocusAttribute());
            break;
        case AXProperty::CanSetSelectedAttribute:
            propertyMap.set(AXProperty::CanSetSelectedAttribute, axObject.canSetSelectedAttribute());
            break;
        case AXProperty::CanSetValueAttribute:
            propertyMap.set(AXProperty::CanSetValueAttribute, axObject.canSetValueAttribute());
            break;
        case AXProperty::Cells:
            propertyMap.set(AXProperty::Cells, axIDs(axObject.cells()));
            break;
        case AXProperty::CellSlots:
            propertyMap.set(AXProperty::CellSlots, axObject.cellSlots());
            break;
        case AXProperty::ColumnIndex:
            propertyMap.set(AXProperty::ColumnIndex, axObject.columnIndex());
            break;
        case AXProperty::ColumnIndexRange:
            propertyMap.set(AXProperty::ColumnIndexRange, axObject.columnIndexRange());
            break;
        case AXProperty::CurrentState:
            propertyMap.set(AXProperty::CurrentState, static_cast<int>(axObject.currentState()));
            break;
        case AXProperty::DatetimeAttributeValue:
            propertyMap.set(AXProperty::DatetimeAttributeValue, axObject.datetimeAttributeValue().isolatedCopy());
            break;
        case AXProperty::DisclosedRows:
            propertyMap.set(AXProperty::DisclosedRows, axIDs(axObject.disclosedRows()));
            break;
        case AXProperty::DocumentLinks:
            propertyMap.set(AXProperty::DocumentLinks, axIDs(axObject.documentLinks()));
            break;
        case AXProperty::ExtendedDescription:
            propertyMap.set(AXProperty::ExtendedDescription, axObject.extendedDescription().isolatedCopy());
            break;
        case AXProperty::HasClickHandler:
            propertyMap.set(AXProperty::HasClickHandler, axObject.hasClickHandler());
            break;
        case AXProperty::IdentifierAttribute:
            propertyMap.set(AXProperty::IdentifierAttribute, axObject.identifierAttribute().isolatedCopy());
            break;
        case AXProperty::InsideLink:
            propertyMap.set(AXProperty::InsideLink, axObject.insideLink());
            break;
        case AXProperty::InternalLinkElement: {
            auto* linkElement = axObject.internalLinkElement();
            propertyMap.set(AXProperty::InternalLinkElement, linkElement ? std::optional { linkElement->objectID() } : std::nullopt);
            break;
        }
        case AXProperty::IsChecked:
            ASSERT(axObject.supportsCheckedState());
            propertyMap.set(AXProperty::IsChecked, axObject.isChecked());
            propertyMap.set(AXProperty::ButtonState, axObject.checkboxOrRadioValue());
            break;
        case AXProperty::IsColumnHeader:
            propertyMap.set(AXProperty::IsColumnHeader, axObject.isColumnHeader());
            break;
        case AXProperty::IsEnabled:
            propertyMap.set(AXProperty::IsEnabled, axObject.isEnabled());
            break;
        case AXProperty::IsExpanded:
            propertyMap.set(AXProperty::IsExpanded, axObject.isExpanded());
            break;
        case AXProperty::IsIgnored:
            propertyMap.set(AXProperty::IsIgnored, axObject.isIgnored());
            break;
        case AXProperty::IsRequired:
            propertyMap.set(AXProperty::IsRequired, axObject.isRequired());
            break;
        case AXProperty::IsSelected:
            propertyMap.set(AXProperty::IsSelected, axObject.isSelected());
            break;
        case AXProperty::IsRowHeader:
            propertyMap.set(AXProperty::IsRowHeader, axObject.isRowHeader());
            break;
        case AXProperty::IsVisible:
            propertyMap.set(AXProperty::IsVisible, axObject.isVisible());
            break;
        case AXProperty::MaxValueForRange:
            propertyMap.set(AXProperty::MaxValueForRange, axObject.maxValueForRange());
            break;
        case AXProperty::MinValueForRange:
            propertyMap.set(AXProperty::MinValueForRange, axObject.minValueForRange());
            break;
        case AXProperty::Orientation:
            propertyMap.set(AXProperty::Orientation, static_cast<int>(axObject.orientation()));
            break;
        case AXProperty::NameAttribute:
            propertyMap.set(AXProperty::NameAttribute, axObject.nameAttribute().isolatedCopy());
            break;
        case AXProperty::PosInSet:
            propertyMap.set(AXProperty::PosInSet, axObject.posInSet());
            break;
        case AXProperty::RemoteFramePlatformElement:
            propertyMap.set(AXProperty::RemoteFramePlatformElement, axObject.remoteFramePlatformElement());
            break;
        case AXProperty::StringValue:
            propertyMap.set(AXProperty::StringValue, axObject.stringValue().isolatedCopy());
            break;
        case AXProperty::HasRemoteFrameChild:
            propertyMap.set(AXProperty::HasRemoteFrameChild, axObject.hasRemoteFrameChild());
            break;
        case AXProperty::RoleDescription:
            propertyMap.set(AXProperty::RoleDescription, axObject.roleDescription().isolatedCopy());
            break;
        case AXProperty::RowIndex:
            propertyMap.set(AXProperty::RowIndex, axObject.rowIndex());
            break;
        case AXProperty::RowIndexRange:
            propertyMap.set(AXProperty::RowIndexRange, axObject.rowIndexRange());
            break;
        case AXProperty::AXRowIndex:
            propertyMap.set(AXProperty::AXRowIndex, axObject.axRowIndex());
            break;
        case AXProperty::CellScope:
            propertyMap.set(AXProperty::CellScope, axObject.cellScope().isolatedCopy());
            break;
        case AXProperty::ScreenRelativePosition:
            propertyMap.set(AXProperty::ScreenRelativePosition, axObject.screenRelativePosition());
            break;
        case AXProperty::SelectedTextRange:
            propertyMap.set(AXProperty::SelectedTextRange, axObject.selectedTextRange());
            break;
        case AXProperty::SetSize:
            propertyMap.set(AXProperty::SetSize, axObject.setSize());
            break;
        case AXProperty::SortDirection:
            propertyMap.set(AXProperty::SortDirection, static_cast<int>(axObject.sortDirection()));
            break;
        case AXProperty::SpeechHint:
            propertyMap.set(AXProperty::SpeechHint, axObject.speechHintAttributeValue().isolatedCopy());
            break;
        case AXProperty::KeyShortcuts:
            propertyMap.set(AXProperty::SupportsKeyShortcuts, axObject.supportsKeyShortcuts());
            propertyMap.set(AXProperty::KeyShortcuts, axObject.keyShortcuts().isolatedCopy());
            break;
        case AXProperty::SupportsARIAOwns:
            propertyMap.set(AXProperty::SupportsARIAOwns, axObject.supportsARIAOwns());
            break;
        case AXProperty::SupportsExpanded:
            propertyMap.set(AXProperty::SupportsExpanded, axObject.supportsExpanded());
            break;
        case AXProperty::SupportsDragging:
            propertyMap.set(AXProperty::SupportsDragging, axObject.supportsDragging());
            break;
        case AXProperty::SupportsPosInSet:
            propertyMap.set(AXProperty::SupportsPosInSet, axObject.supportsPosInSet());
            break;
        case AXProperty::SupportsSetSize:
            propertyMap.set(AXProperty::SupportsSetSize, axObject.supportsSetSize());
            break;
        case AXProperty::TextInputMarkedTextMarkerRange: {
            std::pair<Markable<AXID>, CharacterRange> value;
            auto range = axObject.textInputMarkedTextMarkerRange();
            if (auto characterRange = range.characterRange(); range && characterRange)
                value = { range.start().objectID(), *characterRange };
            propertyMap.set(AXProperty::TextInputMarkedTextMarkerRange, value);
            break;
        }
#if ENABLE(AX_THREAD_TEXT_APIS)
        case AXProperty::BackgroundColor:
            propertyMap.set(AXProperty::BackgroundColor, axObject.backgroundColor());
            break;
        case AXProperty::Font:
            propertyMap.set(AXProperty::Font, axObject.font());
            break;
        case AXProperty::HasLinethrough:
            propertyMap.set(AXProperty::HasLinethrough, axObject.lineDecorationStyle().hasLinethrough);
            break;
        case AXProperty::HasTextShadow:
            propertyMap.set(AXProperty::HasTextShadow, axObject.hasTextShadow());
            break;
        case AXProperty::HasUnderline:
            propertyMap.set(AXProperty::HasUnderline, axObject.lineDecorationStyle().hasUnderline);
            break;
        case AXProperty::IsSubscript:
            propertyMap.set(AXProperty::IsSubscript, axObject.isSubscript());
            break;
        case AXProperty::IsSuperscript:
            propertyMap.set(AXProperty::IsSuperscript, axObject.isSuperscript());
            break;
        case AXProperty::LinethroughColor:
            propertyMap.set(AXProperty::LinethroughColor, axObject.lineDecorationStyle().linethroughColor);
            break;
        case AXProperty::TextColor:
            propertyMap.set(AXProperty::TextColor, axObject.textColor());
            break;
        case AXProperty::TextRuns:
            propertyMap.set(AXProperty::TextRuns, axObject.textRuns());
            break;
        case AXProperty::UnderlineColor:
            propertyMap.set(AXProperty::UnderlineColor, axObject.lineDecorationStyle().underlineColor);
            break;
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        case AXProperty::Title:
            propertyMap.set(AXProperty::Title, axObject.title().isolatedCopy());
            break;
        case AXProperty::URL:
            propertyMap.set(AXProperty::URL, std::make_shared<URL>(axObject.url().isolatedCopy()));
            break;
        case AXProperty::ValueForRange:
            propertyMap.set(AXProperty::ValueForRange, axObject.valueForRange());
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

void AXIsolatedTree::overrideNodeProperties(AXID axID, AXPropertyMap&& propertyMap)
{
    ASSERT(isMainThread());

    if (propertyMap.isEmpty())
        return;

    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axID, WTFMove(propertyMap) });
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
    bool updateTableAncestorColumns = is<AccessibilityTableRow>(axObject);
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    updateTableAncestorColumns = updateTableAncestorColumns || isRowGroup(axObject.node());
#endif
    for (RefPtr ancestor = axObject.parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        if (updateTableAncestorColumns && is<AccessibilityTable>(*ancestor)) {
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
    AXAttributeCacheEnabler enableCache(axObject.axObjectCache());

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
                auto* parent = axObject.parentInCoreTree();
                m_nodeMap.set(liveChild->objectID(), ParentChildrenIDs { parent ? std::optional { parent->objectID() } : std::nullopt, liveChild->childrenIDs() });
                m_unresolvedPendingAppends.set(liveChild->objectID(), AttachWrapper::OnMainThread);
            }

            AXLOG(makeString(
                "Child ID "_s, liveChild->objectID().loggingString(), " of original object ID "_s, axObject.objectID().loggingString(), " was found in the isolated tree with uninitialized live children. Updating its isolated children."_s
            ));
            // Don't immediately resolve node changes in these recursive calls to updateChildren. This avoids duplicate node change creation in this scenario:
            //   1. Some subtree is updated in the below call to updateChildren.
            //   2. Later in this function, when updating axAncestor, we update some higher subtree that includes the updated subtree from step 1.
            updateChildren(liveChild, ResolveNodeChanges::No);
        }
    }
#endif // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    auto oldIDs = m_nodeMap.get(axAncestor->objectID());
    auto& oldChildrenIDs = oldIDs.childrenIDs;

    const auto& newChildren = axAncestor->children();
    auto newChildrenIDs = axAncestor->childrenIDs(false);

    bool childrenChanged = oldChildrenIDs.size() != newChildrenIDs.size();
    for (size_t i = 0; i < newChildren.size(); ++i) {
        ASSERT(newChildren[i]->objectID() == newChildrenIDs[i]);
        size_t index = oldChildrenIDs.find(newChildrenIDs[i]);
        if (index != notFound) {
            // Prevent deletion of this object below by removing it from oldChildrenIDs.
            oldChildrenIDs.remove(index);

            // Propagate any subtree updates downwards for this already-existing child.
            if (auto* liveChild = dynamicDowncast<AccessibilityObject>(newChildren[i].get()); liveChild && liveChild->hasDirtySubtree())
                updateChildren(*liveChild, ResolveNodeChanges::No);
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
        removeSubtreeFromNodeMap(axID, axAncestor);

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
    if (childrenChanged || unconditionallyUpdate(axAncestor->roleValue())) {
        updateNode(*axAncestor);
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

    AXAttributeCacheEnabler enableCache(axObjectCache());
    for (auto& axObject : axObjects)
        updateChildren(axObject.get(), ResolveNodeChanges::No);

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

void AXIsolatedTree::setPendingRootNodeLocked(AXIsolatedObject& root)
{
    AXTRACE("AXIsolatedTree::setRootNode"_s);
    ASSERT(isMainThread());
    ASSERT(m_changeLogLock.isLocked());

    m_pendingRootNode = &root;
}

void AXIsolatedTree::setFocusedNodeID(std::optional<AXID> axID)
{
    AXTRACE("AXIsolatedTree::setFocusedNodeID"_s);
    AXLOG(makeString("axID "_s, axID ? axID->loggingString() : ""_str));
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_pendingFocusedNodeID = axID;
}

void AXIsolatedTree::updateRelations(const UncheckedKeyHashMap<AXID, AXRelations>& relations)
{
    AXTRACE("AXIsolatedTree::updateRelations"_s);
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_relations = relations;
    m_relationsNeedUpdate = false;
}

AXTextMarkerRange AXIsolatedTree::selectedTextMarkerRange()
{
    AXTRACE("AXIsolatedTree::selectedTextMarkerRange"_s);
    Locker locker { m_changeLogLock };
    return m_selectedTextMarkerRange;
}

void AXIsolatedTree::setSelectedTextMarkerRange(AXTextMarkerRange&& range)
{
    AXTRACE("AXIsolatedTree::setSelectedTextMarkerRange"_s);
    ASSERT(isMainThread());

    Locker locker { m_changeLogLock };
    m_selectedTextMarkerRange = range;
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

    AXPropertyMap propertyMap;
    propertyMap.set(AXProperty::RelativeFrame, WTFMove(newFrame));
    // We can clear the initially-cached rough frame, since the object's frame has been cached
    propertyMap.set(AXProperty::InitialFrameRect, FloatRect());
    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axID, WTFMove(propertyMap) });
}

void AXIsolatedTree::updateRootScreenRelativePosition()
{
    AXTRACE("AXIsolatedTree::updateRootScreenRelativePosition"_s);
    ASSERT(isMainThread());

    CheckedPtr cache = m_axObjectCache.get();
    if (auto* axRoot = cache ? cache->getOrCreate(cache->document().view()) : nullptr)
        updateNodeProperties(*axRoot, { AXProperty::ScreenRelativePosition });
}

void AXIsolatedTree::removeNode(const AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::removeNode"_s);
    AXLOG(makeString("objectID "_s, axObject.objectID().loggingString()));
    ASSERT(isMainThread());

    auto labeledObjectIDs = axObjectCache() ? axObjectCache()->relatedObjectIDsFor(axObject, AXRelationType::LabelFor, AXObjectCache::UpdateRelations::No) : std::nullopt;
    if (labeledObjectIDs) {
        // Update the labeled objects since axObject is one of their labels and it is being removed.
        for (AXID labeledObjectID : *labeledObjectIDs) {
            // The label/title of an isolated object is computed based on its AccessibilityText property, thus update it.
            queueNodeUpdate(labeledObjectID, { AXProperty::AccessibilityText });
        }
    }

    m_unresolvedPendingAppends.remove(axObject.objectID());
    removeSubtreeFromNodeMap(axObject.objectID(), axObject.parentInCoreTree());
    queueRemovals({ axObject.objectID() });
}

void AXIsolatedTree::removeSubtreeFromNodeMap(std::optional<AXID> objectID, AccessibilityObject* axParent)
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

    auto axParentID = axParent ? std::optional { axParent->objectID() } : std::nullopt;
    auto actualParentID = m_nodeMap.get(*objectID).parentID;
    if (axParentID != actualParentID) {
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

std::optional<ListHashSet<AXID>> AXIsolatedTree::relatedObjectIDsFor(const AXIsolatedObject& object, AXRelationType relationType)
{
    ASSERT(!isMainThread());
    Locker locker { m_changeLogLock };

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
    ASSERT(!isMainThread());

    Locker locker { m_changeLogLock };

    if (UNLIKELY(m_queuedForDestruction)) {
        for (const auto& object : m_readerThreadNodeMap.values())
            object->detach(AccessibilityDetachmentType::CacheDestroyed);

        // Because each AXIsolatedObject holds a RefPtr to this tree, clear out any member variable
        // that holds an AXIsolatedObject so the ref-cycle is broken and this tree can be destroyed.
        m_readerThreadNodeMap.clear();
        m_rootNode = nullptr;
        m_pendingRootNode = nullptr;
        m_pendingAppends.clear();
        // We don't need to bother clearing out any other non-cycle-causing member variables as they
        // will be cleaned up automatically when the tree is destroyed.

        ASSERT(AXTreeStore::contains(treeID()));
        AXTreeStore::remove(treeID());
        return;
    }

    if (m_pendingRootNode)
        m_rootNode = std::exchange(m_pendingRootNode, { });

    if (m_pendingFocusedNodeID != m_focusedNodeID) {
        AXLOG(makeString("focusedNodeID "_s, m_focusedNodeID ? m_focusedNodeID->loggingString() : ""_str, " pendingFocusedNodeID "_s, m_pendingFocusedNodeID ? m_pendingFocusedNodeID->loggingString() : ""_str));
        m_focusedNodeID = m_pendingFocusedNodeID;
    }

    while (m_pendingSubtreeRemovals.size()) {
        auto axID = m_pendingSubtreeRemovals.takeLast();
        if (m_pendingProtectedFromDeletionIDs.contains(axID))
            continue;
        AXLOG(makeString("removing subtree axID "_s, axID.loggingString()));
        if (RefPtr object = objectForID(axID)) {
            // There's no need to call the more comprehensive AXCoreObject::detach here since
            // we're deleting the entire subtree of this object and thus don't need to `detachRemoteParts`.
            object->detachWrapper(AccessibilityDetachmentType::ElementDestroyed);
            m_pendingSubtreeRemovals.appendVector(object->m_childrenIDs);
            m_readerThreadNodeMap.remove(axID);
        }
    }
    m_pendingProtectedFromDeletionIDs.clear();

    for (const auto& item : m_pendingAppends) {
        auto axID = item.isolatedObject->objectID();
        AXLOG(makeString("appending axID "_s, axID.loggingString()));

        auto& wrapper = item.attachWrapper == AttachWrapper::OnAXThread ? item.wrapper : item.isolatedObject->wrapper();
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
                object->setProperty(property.key, WTFMove(property.value));
        }
    }
    m_pendingPropertyChanges.clear();
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
    ASSERT(isMainThread());

    if (!options.shouldUpdateNode && options.properties.size()) {
        // If we're going to recompute all properties for the node (i.e., the node is in m_needsUpdateNode),
        // don't bother queueing any individual property updates.
        if (m_needsUpdateNode.contains(objectID))
            return;

        auto addResult = m_needsPropertyUpdates.add(objectID, options.properties);
        if (!addResult.isNewEntry)
            addResult.iterator->value.formUnion(options.properties);
    }

    if (options.shouldUpdateChildren)
        m_needsUpdateChildren.add(objectID);

    if (options.shouldUpdateNode)
        m_needsUpdateNode.add(objectID);

    if (auto* cache = axObjectCache())
        cache->startUpdateTreeSnapshotTimer();
}

void AXIsolatedTree::processQueuedNodeUpdates()
{
    ASSERT(isMainThread());

    WeakPtr cache = axObjectCache();
    if (!cache)
        return;

    for (AXID nodeID : m_needsUpdateChildren) {
        if (!cache)
            break;

        if (RefPtr axObject = cache->objectForID(nodeID))
            updateChildren(*axObject, ResolveNodeChanges::No);
    }
    m_needsUpdateChildren.clear();

    for (AXID objectID : m_needsUpdateNode) {
        m_unresolvedPendingAppends.ensure(objectID, [] {
            return AttachWrapper::OnAXThread;
        });
    }
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

    queueRemovalsAndUnresolvedChanges();
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

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
