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
#include "FrameSelection.h"
#include "LocalFrameView.h"
#include "Page.h"
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXIsolatedTree);

static const Seconds CreationFeedbackInterval { 3_s };

HashMap<PageIdentifier, Ref<AXIsolatedTree>>& AXIsolatedTree::treePageCache()
{
    static NeverDestroyed<HashMap<PageIdentifier, Ref<AXIsolatedTree>>> map;
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
    root->setProperty(AXPropertyName::ScreenRelativePosition, axRoot.screenRelativePosition());
    NodeChange rootAppend { root, axRoot.wrapper(), AttachWrapper::OnMainThread };

    RefPtr axWebArea = Accessibility::findChild(axRoot, [] (auto& object) {
        return object->isWebArea();
    });
    if (!axWebArea) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto webArea = AXIsolatedObject::create(*axWebArea, this);
    webArea->setProperty(AXPropertyName::ScreenRelativePosition, axWebArea->screenRelativePosition());
    NodeChange webAreaAppend { webArea, axWebArea->wrapper(), AttachWrapper::OnMainThread };

    m_nodeMap.set(root->objectID(), ParentChildrenIDs { { }, { webArea->objectID() } });
    m_nodeMap.set(webArea->objectID(), ParentChildrenIDs { root->objectID(), { } });

    {
        Locker locker { m_changeLogLock };
        setRootNode(root.ptr());
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
    if (RefPtr existingTree = isolatedTreeForID(tree->treeID())) {
        tree->m_replacingTree = existingTree;
        tree->reportCreationProgress(axObjectCache, 0);
    }

    auto& document = axObjectCache.document();
    if (!document.view()->layoutContext().isInRenderTreeLayout() && !document.inRenderTreeUpdate() && !document.inStyleRecalc())
        document.updateLayoutIgnorePendingStylesheets();

    tree->m_maxTreeDepth = document.settings().maximumHTMLParserDOMTreeDepth();
    ASSERT(tree->m_maxTreeDepth);

    // Generate the nodes of the tree and set its root and focused objects.
    // For this, we need the root and focused objects of the AXObject tree.
    auto* axRoot = axObjectCache.getOrCreate(document.view());
    if (axRoot)
        tree->generateSubtree(*axRoot);
    auto* axFocus = axObjectCache.focusedObjectForPage(document.page());
    if (axFocus)
        tree->setFocusedNodeID(axFocus->objectID());

    tree->updateLoadingProgress(axObjectCache.loadingProgress());

    tree->updateRelations(axObjectCache.relations());

    for (auto& relatedObjectID : axObjectCache.relations().keys()) {
        RefPtr axObject = axObjectCache.objectForID(relatedObjectID);
        if (axObject && axObject->accessibilityIsIgnored())
            tree->addUnconnectedNode(axObject.releaseNonNull());
    }

    // Now that the tree is ready to take client requests, add it to the tree maps so that it can be found.
    storeTree(axObjectCache, tree);
    return tree;
}

void AXIsolatedTree::storeTree(AXObjectCache& axObjectCache, const Ref<AXIsolatedTree>& tree)
{
    AXTreeStore::set(tree->treeID(), tree.ptr());
    tree->m_replacingTree = nullptr;
    Locker locker { s_storeLock };
    treePageCache().set(*axObjectCache.pageID(), tree.copyRef());
}

void AXIsolatedTree::reportCreationProgress(AXObjectCache& cache, unsigned percentComplete)
{
    ASSERT(m_replacingTree->isEmptyContentTree());

    String percent = String::number(percentComplete) + "%"_s;
    String title = AXProcessingPage() + " "_s + percent;
    cache.announce(percent);
    if (RefPtr axRoot = cache.get(cache.document().view())) {
        m_replacingTree->overrideNodeProperties(axRoot->objectID(), {
            { AXPropertyName::TitleAttributeValue, title },
        });
    }
    if (RefPtr axWebarea = cache.rootWebArea()) {
        m_replacingTree->overrideNodeProperties(axWebarea->objectID(), {
            { AXPropertyName::TitleAttributeValue, WTFMove(title) },
        });
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

RefPtr<AXIsolatedObject> AXIsolatedTree::objectForID(const AXID axID) const
{
    // In isolated tree mode, only access m_readerThreadNodeMap on the AX thread.
    if (isMainThread()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    return axID.isValid() ? m_readerThreadNodeMap.get(axID) : nullptr;
}

template<typename U>
Vector<RefPtr<AXCoreObject>> AXIsolatedTree::objectsForIDs(const U& axIDs)
{
    AXTRACE("AXIsolatedTree::objectsForIDs"_s);
    ASSERT(!isMainThread());

    Vector<RefPtr<AXCoreObject>> result;
    result.reserveInitialCapacity(axIDs.size());
    for (auto& axID : axIDs) {
        RefPtr object = objectForID(axID);
        if (object) {
            result.append(object);
            continue;
        }

        if (object) {
            m_readerThreadNodeMap.add(axID, *object);
            result.append(object);
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

    // We're about to a lot of read-only work, so start the attribute cache.
    AXAttributeCacheEnabler enableCache(axObject.axObjectCache());
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
    ASSERT(axObject->wrapper());
    NodeChange nodeChange { object, axObject->wrapper(), attachWrapper };

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

    if (m_unconnectedNodes.contains(axObject->objectID())) {
        AXLOG(makeString("AXIsolatedTree::addUnconnectedNode exiting because an isolated object for ", axObject->objectID().loggingString(), " already exists."));
        return;
    }

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
    m_unconnectedNodes.add(axObject->objectID());

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
    m_pendingProtectedFromDeletionIDs.formUnion(std::exchange(m_protectedFromDeletionIDs, { }));
}

void AXIsolatedTree::queueRemovalsAndUnresolvedChanges(Vector<AXID>&& subtreeRemovals)
{
    ASSERT(isMainThread());

    queueAppendsAndRemovals(resolveAppends(), WTFMove(subtreeRemovals));
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
    for (const auto& unresolvedAppend : m_unresolvedPendingAppends) {
        if (m_replacingTree) {
            ++counter;
            if (MonotonicTime::now() - lastFeedbackTime > CreationFeedbackInterval) {
                reportCreationProgress(*cache, std::ceil((counter / m_unresolvedPendingAppends.size()) * 100));
                lastFeedbackTime = MonotonicTime::now();
            }
        }

        if (auto* axObject = cache->objectForID(unresolvedAppend.key)) {
            if (auto nodeChange = nodeChangeForObject(*axObject, unresolvedAppend.value))
                resolvedAppends.append(*nodeChange);
        }
    }
    resolvedAppends.shrinkToFit();
    m_unresolvedPendingAppends.clear();

    if (m_replacingTree)
        reportCreationProgress(*cache, 100);
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
        m_pendingParentUpdates.set(axID, m_nodeMap.get(axID).parentID);
    }

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

    auto* axParent = axObject.parentObjectUnignored();
    auto iterator = m_nodeMap.find(axObject.objectID());
    if (iterator == m_nodeMap.end())
        m_unresolvedPendingAppends.set(axObject.objectID(), AttachWrapper::OnMainThread);
    else {
        // This object is already in the isolated tree, so there's no need to create full node change for it (doing so is expensive).
        // Protect this object from being deleted. This is important when |axObject| was a child of some other object,
        // but no longer is, and thus the other object will try to queue it for removal. But the fact that we're here
        // indicates this object isn't ready to be removed, just a child of a different parent, so prevent this removal.
        m_protectedFromDeletionIDs.add(axObject.objectID());
        // Update the object's parent if it has changed (but only if we aren't going to create a node change for it,
        // as the act of creating a new node change will correct this as part of creating the new AXIsolatedObject).
        if (axParent && iterator->value.parentID != axParent->objectID() && !m_unresolvedPendingAppends.contains(axObject.objectID()))
            m_needsParentUpdate.add(axObject.objectID());
    }

    auto axChildrenCopy = axObject.children();
    Vector<AXID> axChildrenIDs;
    axChildrenIDs.reserveInitialCapacity(axChildrenCopy.size());
    for (const auto& axChild : axChildrenCopy) {
        if (!axChild || axChild.get() == &axObject) {
            ASSERT_NOT_REACHED();
            continue;
        }

        axChildrenIDs.append(axChild->objectID());
        collectNodeChangesForSubtree(*axChild);
    }
    axChildrenIDs.shrinkToFit();

    // Update the m_nodeMap.
    m_nodeMap.set(axObject.objectID(), ParentChildrenIDs { axParent ? axParent->objectID() : AXID(), WTFMove(axChildrenIDs) });
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

    auto* axParent = axObject.parentObjectUnignored();
    if (!axParent)
        return;

    if (auto change = nodeChangeForObject(*axParent, AttachWrapper::OnAXThread)) {
        Locker locker { m_changeLogLock };
        queueChange(WTFMove(*change));
    }
}

void AXIsolatedTree::updatePropertiesForSelfAndDescendants(AccessibilityObject& axObject, const AXPropertyNameSet& properties)
{
    AXTRACE("AXIsolatedTree::updatePropertiesForSelfAndDescendants"_s);
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    AXPropertyNameSet propertySet;
    for (const auto& property : properties)
        propertySet.add(property);

    Accessibility::enumerateDescendants<AXCoreObject>(axObject, true, [&propertySet, this] (auto& descendant) {
        queueNodeUpdate(descendant, { propertySet });
    });
}

void AXIsolatedTree::updateNodeProperties(AXCoreObject& axObject, const AXPropertyNameSet& properties)
{
    AXTRACE("AXIsolatedTree::updateNodeProperties"_s);
    AXLOG(makeString("Updating properties for objectID ", axObject.objectID().loggingString(), ": "));
    ASSERT(isMainThread());

    if (isUpdatingSubtree())
        return;

    AXPropertyMap propertyMap;
    for (const auto& property : properties) {
        AXLOG(makeString("Property: ", property));
        switch (property) {
        case AXPropertyName::AccessKey:
            propertyMap.set(AXPropertyName::AccessKey, axObject.accessKey().isolatedCopy());
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
        case AXPropertyName::IsNonNativeTextControl:
            propertyMap.set(AXPropertyName::IsNonNativeTextControl, axObject.isNonNativeTextControl());
            break;
        case AXPropertyName::CanSetFocusAttribute:
            propertyMap.set(AXPropertyName::CanSetFocusAttribute, axObject.canSetFocusAttribute());
            break;
        case AXPropertyName::CanSetValueAttribute:
            propertyMap.set(AXPropertyName::CanSetValueAttribute, axObject.canSetValueAttribute());
            break;
        case AXPropertyName::CellSlots:
            propertyMap.set(AXPropertyName::CellSlots, dynamicDowncast<AccessibilityObject>(axObject)->cellSlots());
            break;
        case AXPropertyName::ColumnIndexRange:
            propertyMap.set(AXPropertyName::ColumnIndexRange, axObject.columnIndexRange());
            break;
        case AXPropertyName::CurrentState:
            propertyMap.set(AXPropertyName::CurrentState, static_cast<int>(axObject.currentState()));
            break;
        case AXPropertyName::DisclosedRows:
            propertyMap.set(AXPropertyName::DisclosedRows, axIDs(axObject.disclosedRows()));
            break;
        case AXPropertyName::ExtendedDescription:
            propertyMap.set(AXPropertyName::ExtendedDescription, axObject.extendedDescription().isolatedCopy());
            break;
        case AXPropertyName::IdentifierAttribute:
            propertyMap.set(AXPropertyName::IdentifierAttribute, axObject.identifierAttribute().isolatedCopy());
            break;
        case AXPropertyName::IsChecked:
            ASSERT(axObject.supportsCheckedState());
            propertyMap.set(AXPropertyName::IsChecked, axObject.isChecked());
            propertyMap.set(AXPropertyName::ButtonState, axObject.checkboxOrRadioValue());
            break;
        case AXPropertyName::IsColumnHeader:
            propertyMap.set(AXPropertyName::IsColumnHeader, axObject.isColumnHeader());
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
        case AXPropertyName::IsRowHeader:
            propertyMap.set(AXPropertyName::IsRowHeader, axObject.isRowHeader());
            break;
        case AXPropertyName::IsVisible:
            propertyMap.set(AXPropertyName::IsVisible, axObject.isVisible());
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
        case AXPropertyName::NameAttribute:
            propertyMap.set(AXPropertyName::NameAttribute, axObject.nameAttribute().isolatedCopy());
            break;
        case AXPropertyName::PosInSet:
            propertyMap.set(AXPropertyName::PosInSet, axObject.posInSet());
            break;
        case AXPropertyName::RoleDescription:
            propertyMap.set(AXPropertyName::RoleDescription, axObject.roleDescription().isolatedCopy());
            break;
        case AXPropertyName::RowIndexRange:
            propertyMap.set(AXPropertyName::RowIndexRange, axObject.rowIndexRange());
            break;
        case AXPropertyName::AXRowIndex:
            propertyMap.set(AXPropertyName::AXRowIndex, axObject.axRowIndex());
            break;
        case AXPropertyName::CellScope:
            propertyMap.set(AXPropertyName::CellScope, axObject.cellScope().isolatedCopy());
            break;
        case AXPropertyName::SelectedTextRange:
            propertyMap.set(AXPropertyName::SelectedTextRange, axObject.selectedTextRange());
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
        case AXPropertyName::SelectedChildren:
            propertyMap.set(AXPropertyName::SelectedChildren, axIDs(axObject.selectedChildren()));
            break;
        case AXPropertyName::SupportsARIAOwns:
            propertyMap.set(AXPropertyName::SupportsARIAOwns, axObject.supportsARIAOwns());
            break;
        case AXPropertyName::SupportsExpanded:
            propertyMap.set(AXPropertyName::SupportsExpanded, axObject.supportsExpanded());
            break;
        case AXPropertyName::SupportsPosInSet:
            propertyMap.set(AXPropertyName::SupportsPosInSet, axObject.supportsPosInSet());
            break;
        case AXPropertyName::SupportsSetSize:
            propertyMap.set(AXPropertyName::SupportsSetSize, axObject.supportsSetSize());
            break;
        case AXPropertyName::TextInputMarkedTextMarkerRange: {
            std::pair<AXID, CharacterRange> value;
            auto range = axObject.textInputMarkedTextMarkerRange();
            if (auto characterRange = range.characterRange(); range && characterRange)
                value = { range.start().objectID(), *characterRange };
            propertyMap.set(AXPropertyName::TextInputMarkedTextMarkerRange, value);
            break;
        }
        case AXPropertyName::TitleUIElement: {
            auto* titleUIElement = axObject.titleUIElement();
            propertyMap.set(AXPropertyName::TitleUIElement, titleUIElement ? titleUIElement->objectID() : AXID());
            break;
        }
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

void AXIsolatedTree::overrideNodeProperties(AXID axID, AXPropertyMap&& propertyMap)
{
    ASSERT(isMainThread());

    if (propertyMap.isEmpty())
        return;

    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axID, WTFMove(propertyMap) });
}

void AXIsolatedTree::updateNodeAndDependentProperties(AccessibilityObject& axObject)
{
    ASSERT(isMainThread());

    updateNode(axObject);

    if (RefPtr correspondingControl = axObject.isLabel() ? axObject.correspondingControlForLabelElement() : nullptr)
        updateNode(*correspondingControl);

    // When a row gains or loses cells, the column count of the table can change.
    bool updateTableAncestorColumns = is<AccessibilityTableRow>(axObject);
    for (auto* ancestor = axObject.parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        if (ancestor->isTree()) {
            queueNodeUpdate(*ancestor, { AXPropertyName::ARIATreeRows });
            if (!updateTableAncestorColumns)
                break;
        }

        if (updateTableAncestorColumns && ancestor->isAccessibilityTableInstance()) {
            // Only `updateChildren` if the table is unignored, because otherwise `updateChildren` will ascend and update the next highest unignored ancestor, which doesn't accomplish our goal of updating table columns.
            if (ancestor->accessibilityIsIgnored())
                break;
            // Use `updateChildren` rather than `updateNodeProperty` because `updateChildren` will ensure the columns (which are children) will have associated isolated objects created.
            updateChildren(*ancestor, ResolveNodeChanges::No);
            break;
        }
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

    // We're about to a lot of read-only work, so start the attribute cache.
    AXAttributeCacheEnabler enableCache(axObject.axObjectCache());

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

    bool childrenChanged = oldChildrenIDs.size() != newChildrenIDs.size();
    for (size_t i = 0; i < newChildren.size(); ++i) {
        ASSERT(newChildren[i]->objectID() == newChildrenIDs[i]);
        ASSERT(newChildrenIDs[i].isValid());
        size_t index = oldChildrenIDs.find(newChildrenIDs[i]);
        if (index != notFound) {
            // Prevent deletion of this object below by removing it from oldChildrenIDs.
            oldChildrenIDs.remove(index);

            // Propagate any subtree updates downwards for this already-existing child.
            if (auto* liveChild = dynamicDowncast<AccessibilityObject>(newChildren[i].get()); liveChild && liveChild->hasDirtySubtree())
                updateChildren(*liveChild, ResolveNodeChanges::No);
        }
        else {
            // This is a new child, add it to the tree.
            childrenChanged = true;
            AXLOG(makeString("AXID ", axAncestor->objectID().loggingString(), " gaining new subtree, starting at ID ", newChildren[i]->objectID().loggingString(), ":"));
            AXLOG(newChildren[i]);
            collectNodeChangesForSubtree(*newChildren[i]);
        }
    }
    m_nodeMap.set(axAncestor->objectID(), ParentChildrenIDs { oldIDs.parentID, WTFMove(newChildrenIDs) });
    // Since axAncestor is definitively part of the AX tree by way of getting here, protect it from being
    // deleted in case it has been re-parented.
    m_protectedFromDeletionIDs.add(axAncestor->objectID());

    // What is left in oldChildrenIDs are the IDs that are no longer children of axAncestor.
    // Thus, remove them from m_nodeMap and queue them to be removed from the tree.
    for (const AXID& axID : oldChildrenIDs)
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
    if (childrenChanged || unconditionallyUpdate(axAncestor->roleValue()))
        updateNodeAndDependentProperties(*axAncestor);

    if (resolveNodeChanges == ResolveNodeChanges::Yes)
        queueRemovalsAndUnresolvedChanges(WTFMove(oldChildrenIDs));
    else
        queueRemovals(WTFMove(oldChildrenIDs));
}

void AXIsolatedTree::updateChildrenForObjects(const ListHashSet<RefPtr<AccessibilityObject>>& axObjects)
{
    AXTRACE("AXIsolatedTree::updateChildrenForObjects"_s);

    if (isUpdatingSubtree())
        return;

    AXAttributeCacheEnabler enableCache(axObjectCache());
    for (auto& axObject : axObjects)
        updateChildren(*axObject, ResolveNodeChanges::No);

    queueRemovalsAndUnresolvedChanges({ });
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

AXID AXIsolatedTree::focusedNodeID()
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

    Locker locker { m_changeLogLock };
    m_pendingFocusedNodeID = axID;
}

void AXIsolatedTree::updateRelations(const HashMap<AXID, AXRelations>& relations)
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

void AXIsolatedTree::labelCreated(AccessibilityObject& axObject)
{
    ASSERT(axObject.isLabel());
    if (RefPtr correspondingControl = axObject.isLabel() ? axObject.correspondingControlForLabelElement() : nullptr)
        queueNodeUpdate(*correspondingControl, NodeUpdateOptions::nodeUpdate());
}

void AXIsolatedTree::updateLoadingProgress(double newProgressValue)
{
    AXTRACE("AXIsolatedTree::updateLoadingProgress"_s);
    AXLOG(makeString("Updating loading progress to ", newProgressValue, " for treeID ", treeID().loggingString()));
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
    propertyMap.set(AXPropertyName::RelativeFrame, WTFMove(newFrame));
    Locker locker { m_changeLogLock };
    m_pendingPropertyChanges.append({ axID, WTFMove(propertyMap) });
}

void AXIsolatedTree::removeNode(const AccessibilityObject& axObject)
{
    AXTRACE("AXIsolatedTree::removeNode"_s);
    AXLOG(makeString("objectID ", axObject.objectID().loggingString()));
    ASSERT(isMainThread());

    if (RefPtr correspondingControl = axObject.isLabel() ? axObject.correspondingControlForLabelElement() : nullptr) {
        // If a label has been removed from the AX tree, the control associated it may need to change
        // its title UI element. Use callOnMainThread to spin the runloop before re-computing this,
        // as we need to wait for the backing DOM element to actually be destroyed to compute the
        // correct title UI element.
        callOnMainThread([correspondingControl, protectedThis = Ref { *this }] {
            protectedThis->updateNode(*correspondingControl);
        });
    }

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

    if (m_unconnectedNodes.remove(objectID))
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
        if (!axID.isValid() || m_unresolvedPendingAppends.contains(axID) || m_protectedFromDeletionIDs.contains(axID))
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

    // In isolated tree mode, only apply pending changes on the AX thread.
    if (isMainThread()) {
        ASSERT_NOT_REACHED();
        return;
    }

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
        m_focusedNodeID = m_pendingFocusedNodeID;
    }

    while (m_pendingSubtreeRemovals.size()) {
        auto axID = m_pendingSubtreeRemovals.takeLast();
        if (m_pendingProtectedFromDeletionIDs.contains(axID))
            continue;
        AXLOG(makeString("removing subtree axID ", axID.loggingString()));
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
        AXID axID = item.isolatedObject->objectID();
        AXLOG(makeString("appending axID ", axID.loggingString()));
        if (!axID.isValid())
            continue;

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
        AXLOG(makeString("updating children for axID ", update.first.loggingString()));
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

void AXIsolatedTree::queueNodeUpdate(AXCoreObject& object, const NodeUpdateOptions& options)
{
    ASSERT(isMainThread());

    if (!options.shouldUpdateNode && options.properties.size()) {
        // If we're going to recompute all properties for the node (i.e., the node is in m_needsUpdateNode),
        // don't bother queueing any individual property updates.
        if (m_needsUpdateNode.contains(object.objectID()))
            return;

        auto addResult = m_needsPropertyUpdates.add(object.objectID(), options.properties);
        if (!addResult.isNewEntry)
            addResult.iterator->value.formUnion(options.properties);
    }

    if (options.shouldUpdateChildren)
        m_needsUpdateChildren.add(object.objectID());

    if (options.shouldUpdateNode)
        m_needsUpdateNode.add(object.objectID());

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
        if (!m_unresolvedPendingAppends.contains(objectID)) {
            m_unresolvedPendingAppends.ensure(objectID, [] {
                return AttachWrapper::OnAXThread;
            });
        }
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

    queueRemovalsAndUnresolvedChanges({ });
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
