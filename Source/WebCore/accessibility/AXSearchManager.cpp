/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXSearchManager.h"

#include "AccessibilityObjectInlines.h"
#include "AXLogger.h"
#include "AXLoggerBase.h"
#include "AXObjectCacheInlines.h"
#include "AXTreeStoreInlines.h"
#include "AXUtilities.h"
#include "AccessibilityObject.h"
#include "FrameDestructionObserverInlines.h"
#include "Logging.h"
#include "LocalFrameInlines.h"
#include "TextIterator.h"
#include <ranges>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXSearchManager);

// This function determines if the given `axObject` is a radio button part of a different ad-hoc radio group
// than `referenceObject`, where ad-hoc radio group membership is determined by comparing `name` attributes.
static bool isRadioButtonInDifferentAdhocGroup(Ref<AXCoreObject> axObject, AXCoreObject* referenceObject)
{
    if (!axObject->isRadioButton())
        return false;

    // If the `referenceObject` is not a radio button and this `axObject` is, their radio group membership is different because
    // `axObject` belongs to a group and `referenceObject` doesn't.
    if (!referenceObject || !referenceObject->isRadioButton())
        return true;

    return axObject->nameAttribute() != referenceObject->nameAttribute();
}

bool AXSearchManager::matchForSearchKeyAtIndex(Ref<AXCoreObject> axObject, const AccessibilitySearchCriteria& criteria, size_t index)
{
    RefPtr startObject = criteria.startObject.get();
    switch (criteria.searchKeys[index]) {
    case AccessibilitySearchKey::AnyType:
        // The AccessibilitySearchKey::AnyType matches any non-null AccessibilityObject.
        return true;
    case AccessibilitySearchKey::Article:
        return axObject->role() == AccessibilityRole::DocumentArticle;
    case AccessibilitySearchKey::BlockquoteSameLevel:
        return startObject
            && axObject->isBlockquote()
            && axObject->blockquoteLevel() == startObject->blockquoteLevel();
    case AccessibilitySearchKey::Blockquote:
        return axObject->isBlockquote();
    case AccessibilitySearchKey::BoldFont:
        return axObject->hasBoldFont();
    case AccessibilitySearchKey::Button:
        return axObject->isButton();
    case AccessibilitySearchKey::Checkbox:
        return axObject->isCheckbox();
    case AccessibilitySearchKey::Control:
        return axObject->isControl() || axObject->isSummary();
    case AccessibilitySearchKey::DifferentType:
        return startObject
            && axObject->role() != startObject->role();
    case AccessibilitySearchKey::FontChange:
        return startObject
            && !axObject->hasSameFont(*startObject);
    case AccessibilitySearchKey::FontColorChange:
        return startObject
            && !axObject->hasSameFontColor(*startObject);
    case AccessibilitySearchKey::Frame:
        return axObject->isWebArea();
    case AccessibilitySearchKey::Graphic:
        return axObject->isImage() && !axObject->isInImage();
    case AccessibilitySearchKey::HeadingLevel1:
        return axObject->headingLevel() == 1;
    case AccessibilitySearchKey::HeadingLevel2:
        return axObject->headingLevel() == 2;
    case AccessibilitySearchKey::HeadingLevel3:
        return axObject->headingLevel() == 3;
    case AccessibilitySearchKey::HeadingLevel4:
        return axObject->headingLevel() == 4;
    case AccessibilitySearchKey::HeadingLevel5:
        return axObject->headingLevel() == 5;
    case AccessibilitySearchKey::HeadingLevel6:
        return axObject->headingLevel() == 6;
    case AccessibilitySearchKey::HeadingSameLevel:
        return startObject
            && axObject->isHeading()
            && axObject->headingLevel() == startObject->headingLevel();
    case AccessibilitySearchKey::Heading:
        return axObject->isHeading();
    case AccessibilitySearchKey::Highlighted:
        return axObject->hasHighlighting();
    case AccessibilitySearchKey::KeyboardFocusable:
        return axObject->isKeyboardFocusable();
    case AccessibilitySearchKey::ItalicFont:
        return axObject->hasItalicFont();
    case AccessibilitySearchKey::Landmark:
        return axObject->isLandmark();
    case AccessibilitySearchKey::Link: {
        bool isLink = axObject->isLink();
#if PLATFORM(IOS_FAMILY)
        if (!isLink)
            isLink = axObject->isDescendantOfRole(AccessibilityRole::Link);
#endif
        return isLink;
    }
    case AccessibilitySearchKey::List:
        return axObject->isList();
    case AccessibilitySearchKey::LiveRegion:
        return axObject->supportsLiveRegion();
    case AccessibilitySearchKey::MisspelledWord: {
        auto ranges = axObject->misspellingRanges();
        bool hasMisspelling = !ranges.isEmpty();
        if (hasMisspelling)
            m_misspellingRanges.set(axObject->objectID(), WTF::move(ranges));
        return hasMisspelling;
    }
    case AccessibilitySearchKey::Outline:
        return axObject->isTree();
    case AccessibilitySearchKey::PlainText:
        return axObject->hasPlainText();
    case AccessibilitySearchKey::RadioGroup:
        return axObject->isRadioGroup() || isRadioButtonInDifferentAdhocGroup(axObject, startObject.get());
    case AccessibilitySearchKey::SameType:
        return startObject
            && axObject->role() == startObject->role();
    case AccessibilitySearchKey::StaticText:
        return axObject->isStaticText();
    case AccessibilitySearchKey::StyleChange:
        return startObject
            && !axObject->hasSameStyle(*startObject);
    case AccessibilitySearchKey::TableSameLevel:
        return startObject
            && axObject->isExposableTable()
            && axObject->tableLevel() == startObject->tableLevel();
    case AccessibilitySearchKey::Table:
        return axObject->isExposableTable();
    case AccessibilitySearchKey::TextField:
        return axObject->isTextControl();
    case AccessibilitySearchKey::Underline:
        return axObject->hasUnderline();
    case AccessibilitySearchKey::UnvisitedLink:
        return axObject->isUnvisitedLink();
    case AccessibilitySearchKey::VisitedLink:
        return axObject->isVisitedLink();
    default:
        return false;
    }
}

bool AXSearchManager::match(Ref<AXCoreObject> axObject, const AccessibilitySearchCriteria& criteria)
{
    for (size_t i = 0; i < criteria.searchKeys.size(); ++i) {
        if (matchForSearchKeyAtIndex(axObject, criteria, i))
            return criteria.visibleOnly ? axObject->isOnScreen() : true;
    }
    return false;
}

bool AXSearchManager::matchText(Ref<AXCoreObject> axObject, const String& searchText)
{
    // If text is empty we return true.
    if (searchText.isEmpty())
        return true;

    return containsPlainText(axObject->title(), searchText, FindOption::CaseInsensitive)
        || containsPlainText(axObject->description(), searchText, FindOption::CaseInsensitive)
        || containsPlainText(axObject->stringValue(), searchText, FindOption::CaseInsensitive);
}

static void appendAccessibilityObject(Ref<AXCoreObject> object, AccessibilityObject::AccessibilityChildrenVector& results)
{
    if (!object->isAttachment()) [[likely]]
        results.append(WTF::move(object));
    else if (RefPtr axObject = dynamicDowncast<AccessibilityObject>(object)) {
        // Find the next descendant of this attachment object so search can continue through frames.
        RefPtr widget = axObject->widgetForAttachmentView();
        RefPtr frameView = dynamicDowncast<LocalFrameView>(widget);
        if (!frameView)
            return;
        RefPtr document = frameView->frame().document();
        if (!document || !document->hasLivingRenderTree())
            return;

        CheckedPtr cache = axObject->axObjectCache();
        if (RefPtr axDocument = cache ? cache->getOrCreate(*document) : nullptr)
            results.append(axDocument.releaseNonNull());
    }
}

static void appendChildrenToArray(AXCoreObject& object, bool isForward, RefPtr<AXCoreObject> startObject, AXCoreObject::AccessibilityChildrenVector& results)
{
    // A table's children includes elements whose own children are also the table's children (due to the way the Mac exposes tables).
    // The rows from the table should be queried, since those are direct descendants of the table, and they contain content.
    // FIXME: Unlike AXCoreObject::children(), AXCoreObject::rows() returns a copy, not a const-reference. This can be wasteful
    // for tables with lots of rows and probably should be changed.
    const auto& searchChildren = object.isExposableTable() ? object.rows() : object.crossFrameUnignoredChildren();

    size_t childrenSize = searchChildren.size();

    size_t startIndex = isForward ? childrenSize : 0;
    size_t endIndex = isForward ? 0 : childrenSize;

    // If the startObject is ignored, we should use an accessible sibling as a start element instead.
    if (startObject && startObject->isIgnored() && startObject->crossFrameIsDescendantOfObject(object)) {
        RefPtr<AXCoreObject> parentObject = startObject->parentObjectIncludingCrossFrame();
        // Go up the parent chain to find the highest ancestor that's also being ignored.
        while (parentObject && parentObject->isIgnored()) {
            if (parentObject == &object)
                break;
            startObject = parentObject;
            parentObject = parentObject->parentObjectIncludingCrossFrame();
        }

        // We should only ever hit this case with a live object (not an isolated object), as it would require startObject to be ignored,
        // and we should never have created an isolated object from an ignored live object.
        // FIXME: This is not true for ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE), fix this before shipping it.
        // FIXME: We hit this ASSERT on google.com. https://bugs.webkit.org/show_bug.cgi?id=293263
        AX_BROKEN_ASSERT(is<AccessibilityObject>(startObject));
        RefPtr newStartObject = dynamicDowncast<AccessibilityObject>(startObject);
        // Get the un-ignored sibling based on the search direction, and update the searchPosition.
        if (newStartObject && newStartObject->isIgnored())
            newStartObject = isForward ? newStartObject->previousSiblingUnignored() : newStartObject->nextSiblingUnignored();
        startObject = newStartObject;
    }

    size_t searchPosition = notFound;
    if (startObject) {
        searchPosition = searchChildren.findIf([&](const Ref<AXCoreObject>& object) {
            return startObject == object.ptr();
        });
    }

    if (searchPosition != notFound) {
        if (isForward)
            endIndex = searchPosition + 1;
        else
            endIndex = searchPosition;
    }

    // This is broken into two statements so that it's easier read.
    if (isForward) {
        for (size_t i = startIndex; i > endIndex; i--)
            appendAccessibilityObject(searchChildren.at(i - 1), results);
    } else {
        for (size_t i = startIndex; i < endIndex; i++)
            appendAccessibilityObject(searchChildren.at(i), results);
    }
}

DidTimeout AXSearchManager::revealHiddenMatchWithTimeout(AXCoreObject& matchedObject, Seconds timeout)
{
    auto revealAndUpdateAccessibilityTrees = [axID = matchedObject.objectID(), treeID = matchedObject.treeID()] {
        WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID);
        RefPtr object = cache ? cache->objectForID(axID) : nullptr;
        if (!object)
            return;
        object->revealAncestors();
        for (RefPtr ancestor = object; ancestor; ancestor = downcast<AccessibilityObject>(ancestor->parentObjectIncludingCrossFrame())) {
            if (RefPtr document = ancestor->document(); document && needsLayoutOrStyleRecalc(*document)) {
                document->updateLayoutIgnorePendingStylesheets();
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
                cache->scheduleObjectRegionsUpdate(/* scheduleImmediately */ true);
#endif
            }
            ancestor->recomputeIsIgnored();
        }

        cache->performDeferredCacheUpdate(ForceLayout::Yes);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (RefPtr tree = AXTreeStore<AXIsolatedTree>::isolatedTreeForID(treeID))
            tree->processQueuedNodeUpdates();
#endif
    };

    if (lastRevealAttemptTimedOut()) {
        // If the last reveal attempt timed out because the main-thread is busy, don't delay this search any further.
        // We should still expand the collapsed content to increase the chance the user discovers it later when the
        // main-thread has stopped being busy and can perform the expansion.
        Accessibility::performFunctionOnMainThread(revealAndUpdateAccessibilityTrees);
        return DidTimeout::Yes;
    }

    auto didTimeout = Accessibility::performFunctionOnMainThreadAndWaitWithTimeout(revealAndUpdateAccessibilityTrees, timeout);
    if (didTimeout == DidTimeout::Yes)
        setLastRevealAttemptTimedOut(true);
    return didTimeout;
}

AccessibilitySearchResultStream AXSearchManager::findMatchingObjectsAsStream(AccessibilitySearchCriteria&& criteria, RemoteFrameSearchCallback&& remoteFrameCallback)
{
    return findMatchingObjectsInternalAsStream(criteria, remoteFrameCallback);
}

AccessibilitySearchResultStream AXSearchManager::findMatchingObjectsInternalAsStream(const AccessibilitySearchCriteria& criteria, const RemoteFrameSearchCallback& remoteFrameCallback)
{
    AXTRACE("AXSearchManager::findMatchingObjectsInternalAsStream"_s);
    AXLOG(criteria);

    AccessibilitySearchResultStream stream;
    stream.setResultsLimit(criteria.resultsLimit);

    if (!criteria.searchKeys.size())
        return stream;

    RefPtr anchorObject = criteria.anchorObject.get();
    if (!anchorObject)
        return stream;

    // Track how many local results we've found to determine when to stop searching.
    unsigned localResultCount = 0;

    bool shouldCheckForRevealableText = !criteria.visibleOnly && !criteria.immediateDescendantsOnly && !criteria.searchText.isEmpty();
    auto matchWithinRevealableContainer = [&] (AXCoreObject& object) -> bool {
        if (!shouldCheckForRevealableText)
            return false;

        for (const auto& revealableContainer : object.revealableContainers()) {
            RefPtr descendant = revealableContainer.get();
            while ((descendant = descendant ? descendant->nextInPreOrder(/* updateChildren */ true, /* stayWithin */ revealableContainer.ptr(), /* crossFrame */ true) : nullptr)) {
                if (match(*descendant, criteria) && containsPlainText(descendant->revealableText(), criteria.searchText, FindOption::CaseInsensitive)) {
                    if (revealHiddenMatchWithTimeout(*descendant, 100_ms) == DidTimeout::No) {
                        stream.appendLocalResult(*descendant);
                        ++localResultCount;
                        return true;
                    }
                }
            }
        }
        return false;
    };

    auto addMatchToStream = [&](Ref<AXCoreObject> matchObject) -> bool {
        if (match(matchObject, criteria) && matchText(matchObject, criteria.searchText)) {
            stream.appendLocalResult(matchObject);
            ++localResultCount;
            return localResultCount >= criteria.resultsLimit;
        }
        return false;
    };

    // This search algorithm only searches the elements before/after the starting object.
    // It does this by stepping up the parent chain and at each level doing a DFS.

    // If there's no start object, it means we want to search everything.
    RefPtr startObject = criteria.startObject.get();
    if (!startObject)
        startObject = anchorObject;

    bool isForward = criteria.searchDirection == AccessibilitySearchDirection::Next;

#if PLATFORM(MAC)
    // For backward search starting from a remote frame, we need to dispatch to that frame first
    // so it can search backward from its current focus position. Without this, the backward search
    // would skip the remote frame entirely and only search elements before it in the parent.
    if (!isForward && startObject != anchorObject && startObject->isRemoteFrame()) {
        if (auto frameID = startObject->remoteFrameID(); frameID && startObject->remoteFramePID()) {
            stream.appendRemoteFrame(*frameID);
            if (remoteFrameCallback)
                remoteFrameCallback(*frameID, stream.entryCount(), localResultCount);
        }
    }
#endif // PLATFORM(MAC)

    // The first iteration of the outer loop will examine the children of the start object for matches. However, when
    // iterating backwards, the start object children should not be considered, so the loop is skipped ahead. We make an
    // exception when no start object was specified because we want to search everything regardless of search direction.
    RefPtr<AXCoreObject> previousObject;
    if (!isForward && startObject != anchorObject) {
        previousObject = startObject;
        startObject = startObject->crossFrameParentObjectUnignored();
    }

    if (startObject && matchWithinRevealableContainer(*startObject) && localResultCount >= criteria.resultsLimit)
        return stream;

    // The outer loop steps up the parent chain each time (unignored is important here because otherwise elements would be searched twice)
    for (RefPtr stopSearchElement = anchorObject->crossFrameParentObjectUnignored(); startObject && startObject != stopSearchElement; startObject = startObject->crossFrameParentObjectUnignored()) {

        // Only append the children after/before the previous element, so that the search does not check elements that are
        // already behind/ahead of start element.
        AXCoreObject::AccessibilityChildrenVector searchStack;
        if (!criteria.immediateDescendantsOnly || startObject == anchorObject)
            appendChildrenToArray(*startObject, isForward, previousObject, searchStack);

        // This now does a DFS at the current level of the parent.
        while (!searchStack.isEmpty()) {
            Ref searchObject = searchStack.takeLast();

#if PLATFORM(MAC)
            // Check if this is a remote frame - if so, record it in the stream for cross-process search.
            if (searchObject->isRemoteFrame()) {
                auto frameID = searchObject->remoteFrameID();
                auto pid = searchObject->remoteFramePID();
                if (frameID && pid) {
                    // Always record remote frames in the stream to maintain tree order.
                    stream.appendRemoteFrame(*frameID);
                    // Invoke callback to allow eager IPC dispatch while search continues.
                    if (remoteFrameCallback)
                        remoteFrameCallback(*frameID, stream.entryCount(), localResultCount);
                }
                // Don't descend into remote frames - we'll forward the search to them
                // via IPC in |remoteFrameCallback|.
                continue;
            }
#else
            UNUSED_PARAM(remoteFrameCallback);
#endif // PLATFORM(MAC)

            if (addMatchToStream(searchObject))
                break;

            if (matchWithinRevealableContainer(searchObject.get()) && localResultCount >= criteria.resultsLimit)
                break;

            if (!criteria.immediateDescendantsOnly)
                appendChildrenToArray(searchObject, isForward, nullptr, searchStack);
        }

        if (localResultCount >= criteria.resultsLimit)
            break;

        // When moving backwards, the parent object needs to be checked, because technically it's "before" the starting element.
        if (!isForward && startObject != anchorObject && addMatchToStream(*startObject))
            break;

        previousObject = startObject;
    }

    AXLOG(makeString("Stream total entries count: %zu. Local result count: %u"_s, stream.entryCount(), localResultCount));
    return stream;
}

std::optional<AXTextMarkerRange> AXSearchManager::findMatchingRange(AccessibilitySearchCriteria&& criteria)
{
    AXTRACE("AXSearchManager::findMatchingRange"_s);

    // Currently, this method only supports searching for the next/previous misspelling.
    // FIXME: support other types of ranges, like italicized.
    if (criteria.searchKeys.size() != 1 || criteria.searchKeys[0] != AccessibilitySearchKey::MisspelledWord || criteria.resultsLimit != 1) {
        AX_ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    // If there's no start object, it means we want to search everything.
    RefPtr startObject = criteria.startObject.get();
    if (!startObject)
        startObject = criteria.anchorObject.get();
    AXLOG(startObject);

    bool forward = criteria.searchDirection == AccessibilitySearchDirection::Next;
    if (match(*startObject, criteria)) {
        AX_ASSERT(m_misspellingRanges.contains(startObject->objectID()));
        const auto& ranges = m_misspellingRanges.get(startObject->objectID());
        AX_ASSERT(!ranges.isEmpty());

        AXTextMarkerRange startRange { startObject->treeID(), startObject->objectID(), criteria.startRange };
        if (forward) {
            for (auto& range : ranges) {
                if (range > startRange)
                    return range;
            }
        } else {
            for (auto& range : ranges | std::views::reverse) {
                if (range < startRange)
                    return range;
            }
        }
    }

    // Didn't find a matching range for startObject, thus move to the next/previous object.
    auto stream = findMatchingObjectsInternalAsStream(criteria, /* remoteFrameSearchCallback */ { });
    // Misspelling search is local-only, so just get the first local result from the stream.
    for (const auto& entry : stream.entries()) {
        if (RefPtr object = entry.objectIfLocalResult()) {
            auto axID = object->objectID();
            AX_ASSERT(m_misspellingRanges.contains(axID));
            const auto& ranges = m_misspellingRanges.get(axID);
            if (ranges.isEmpty()) {
                AX_ASSERT_NOT_REACHED();
                return std::nullopt;
            }
            return forward ? ranges[0] : ranges.last();
        }
    }
    return std::nullopt;
}

} // namespace WebCore
