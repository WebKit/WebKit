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

#include "AXLogger.h"
#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "TextIterator.h"

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
    switch (criteria.searchKeys[index]) {
    case AccessibilitySearchKey::AnyType:
        // The AccessibilitySearchKey::AnyType matches any non-null AccessibilityObject.
        return true;
    case AccessibilitySearchKey::Article:
        return axObject->role() == AccessibilityRole::DocumentArticle;
    case AccessibilitySearchKey::BlockquoteSameLevel:
        return criteria.startObject
            && axObject->isBlockquote()
            && axObject->blockquoteLevel() == criteria.startObject->blockquoteLevel();
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
        return criteria.startObject
            && axObject->role() != criteria.startObject->role();
    case AccessibilitySearchKey::FontChange:
        return criteria.startObject
            && !axObject->hasSameFont(*criteria.startObject);
    case AccessibilitySearchKey::FontColorChange:
        return criteria.startObject
            && !axObject->hasSameFontColor(*criteria.startObject);
    case AccessibilitySearchKey::Frame:
        return axObject->isWebArea();
    case AccessibilitySearchKey::Graphic:
        return axObject->isImage();
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
        return criteria.startObject
            && axObject->isHeading()
            && axObject->headingLevel() == criteria.startObject->headingLevel();
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
            m_misspellingRanges.set(axObject->objectID(), WTFMove(ranges));
        return hasMisspelling;
    }
    case AccessibilitySearchKey::Outline:
        return axObject->isTree();
    case AccessibilitySearchKey::PlainText:
        return axObject->hasPlainText();
    case AccessibilitySearchKey::RadioGroup:
        return axObject->isRadioGroup() || isRadioButtonInDifferentAdhocGroup(axObject, criteria.startObject);
    case AccessibilitySearchKey::SameType:
        return criteria.startObject
            && axObject->role() == criteria.startObject->role();
    case AccessibilitySearchKey::StaticText:
        return axObject->isStaticText();
    case AccessibilitySearchKey::StyleChange:
        return criteria.startObject
            && !axObject->hasSameStyle(*criteria.startObject);
    case AccessibilitySearchKey::TableSameLevel:
        return criteria.startObject
            && axObject->isTable() && axObject->isExposable()
            && axObject->tableLevel() == criteria.startObject->tableLevel();
    case AccessibilitySearchKey::Table:
        return axObject->isTable() && axObject->isExposable();
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

bool AXSearchManager::matchWithResultsLimit(Ref<AXCoreObject> object, const AccessibilitySearchCriteria& criteria, AXCoreObject::AccessibilityChildrenVector& results)
{
    if (match(object, criteria) && matchText(object, criteria.searchText)) {
        results.append(object);

        // Enough results were found to stop searching.
        if (results.size() >= criteria.resultsLimit)
            return true;
    }

    return false;
}

static void appendAccessibilityObject(Ref<AXCoreObject> object, AccessibilityObject::AccessibilityChildrenVector& results)
{
    if (!object->isAttachment()) [[likely]]
        results.append(WTFMove(object));
    else {
        // Find the next descendant of this attachment object so search can continue through frames.
        RefPtr widget = object->widgetForAttachmentView();
        RefPtr frameView = dynamicDowncast<LocalFrameView>(widget);
        if (!frameView)
            return;
        RefPtr document = frameView->frame().document();
        if (!document || !document->hasLivingRenderTree())
            return;

        CheckedPtr cache = object->axObjectCache();
        if (RefPtr axDocument = cache ? cache->getOrCreate(*document) : nullptr)
            results.append(*axDocument);
    }
}

static void appendChildrenToArray(Ref<AXCoreObject> object, bool isForward, RefPtr<AXCoreObject> startObject, AXCoreObject::AccessibilityChildrenVector& results)
{
    // A table's children includes elements whose own children are also the table's children (due to the way the Mac exposes tables).
    // The rows from the table should be queried, since those are direct descendants of the table, and they contain content.
    // FIXME: Unlike AXCoreObject::children(), AXCoreObject::rows() returns a copy, not a const-reference. This can be wasteful
    // for tables with lots of rows and probably should be changed.
    const auto& searchChildren = object->isTable() && object->isExposable() ? object->rows() : object->unignoredChildren();

    size_t childrenSize = searchChildren.size();

    size_t startIndex = isForward ? childrenSize : 0;
    size_t endIndex = isForward ? 0 : childrenSize;

    // If the startObject is ignored, we should use an accessible sibling as a start element instead.
    if (startObject && startObject->isIgnored() && startObject->isDescendantOfObject(object.get())) {
        RefPtr<AXCoreObject> parentObject = startObject->parentObject();
        // Go up the parent chain to find the highest ancestor that's also being ignored.
        while (parentObject && parentObject->isIgnored()) {
            if (parentObject == object.ptr())
                break;
            startObject = parentObject;
            parentObject = parentObject->parentObject();
        }

        // We should only ever hit this case with a live object (not an isolated object), as it would require startObject to be ignored,
        // and we should never have created an isolated object from an ignored live object.
        // FIXME: This is not true for ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE), fix this before shipping it.
        // FIXME: We hit this ASSERT on google.com. https://bugs.webkit.org/show_bug.cgi?id=293263
        ASSERT(is<AccessibilityObject>(startObject));
        RefPtr newStartObject = dynamicDowncast<AccessibilityObject>(startObject.get());
        // Get the un-ignored sibling based on the search direction, and update the searchPosition.
        if (newStartObject && newStartObject->isIgnored())
            newStartObject = isForward ? newStartObject->previousSiblingUnignored() : newStartObject->nextSiblingUnignored();
        startObject = newStartObject;
    }

    size_t searchPosition = notFound;
    if (startObject) {
        searchPosition = searchChildren.findIf([&] (const Ref<AXCoreObject>& object) {
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

AXCoreObject::AccessibilityChildrenVector AXSearchManager::findMatchingObjectsInternal(const AccessibilitySearchCriteria& criteria)
{
    AXTRACE("AXSearchManager::findMatchingObjectsInternal"_s);
    AXLOG(criteria);

    if (!criteria.searchKeys.size())
        return { };

#if PLATFORM(MAC)
    if (criteria.searchKeys.size() == 1) {
        // Only perform these optimizations if we aren't expected to start from somewhere mid-tree.
        // We could probably implement these optimizations when we do have a startObject and get
        // performance benefits, but no known assistive technology needs this right now.
        if (!criteria.startObject) {
            if (criteria.searchKeys[0] == AccessibilitySearchKey::LiveRegion) {
                if (criteria.anchorObject->isRootWebArea()) {
                    // All live regions will be descendants of the root webarea, so we don't need to do
                    // any ancestry walks as `sortedDescendants` does.
                    auto liveRegions = criteria.anchorObject->allSortedLiveRegions();
                    return liveRegions.subvector(0, std::min(liveRegions.size(), static_cast<size_t>(criteria.resultsLimit)));
                }
                return criteria.anchorObject->sortedDescendants(criteria.resultsLimit, PreSortedObjectType::LiveRegion);
            }

            if (criteria.searchKeys[0] == AccessibilitySearchKey::Frame) {
                if (criteria.anchorObject->isRootWebArea()) {
                    auto webAreas = criteria.anchorObject->allSortedNonRootWebAreas();
                    return webAreas.subvector(0, std::min(webAreas.size(), static_cast<size_t>(criteria.resultsLimit)));
                }
                return criteria.anchorObject->sortedDescendants(criteria.resultsLimit, PreSortedObjectType::WebArea);
            }
        }
    }
#endif // PLATFORM(MAC)

    AXCoreObject::AccessibilityChildrenVector results;
    // This search algorithm only searches the elements before/after the starting object.
    // It does this by stepping up the parent chain and at each level doing a DFS.

    // If there's no start object, it means we want to search everything.
    RefPtr<AXCoreObject> startObject = criteria.startObject;
    if (!startObject)
        startObject = criteria.anchorObject;

    bool isForward = criteria.searchDirection == AccessibilitySearchDirection::Next;

    // The first iteration of the outer loop will examine the children of the start object for matches. However, when
    // iterating backwards, the start object children should not be considered, so the loop is skipped ahead. We make an
    // exception when no start object was specified because we want to search everything regardless of search direction.
    RefPtr<AXCoreObject> previousObject;
    if (!isForward && startObject != criteria.anchorObject) {
        previousObject = startObject;
        startObject = startObject->parentObjectUnignored();
    }

    // The outer loop steps up the parent chain each time (unignored is important here because otherwise elements would be searched twice)
    for (RefPtr stopSearchElement = criteria.anchorObject->parentObjectUnignored(); startObject && startObject != stopSearchElement; startObject = startObject->parentObjectUnignored()) {
        // Only append the children after/before the previous element, so that the search does not check elements that are
        // already behind/ahead of start element.
        AXCoreObject::AccessibilityChildrenVector searchStack;
        if (!criteria.immediateDescendantsOnly || startObject == criteria.anchorObject)
            appendChildrenToArray(*startObject, isForward, previousObject, searchStack);

        // This now does a DFS at the current level of the parent.
        while (!searchStack.isEmpty()) {
            auto searchObject = searchStack.last();
            searchStack.removeLast();

            if (matchWithResultsLimit(searchObject, criteria, results))
                break;

            if (!criteria.immediateDescendantsOnly)
                appendChildrenToArray(searchObject, isForward, nullptr, searchStack);
        }

        if (results.size() >= criteria.resultsLimit)
            break;

        // When moving backwards, the parent object needs to be checked, because technically it's "before" the starting element.
        if (!isForward && startObject != criteria.anchorObject && matchWithResultsLimit(*startObject, criteria, results))
            break;

        previousObject = startObject;
    }

    AXLOG(results);
    return results;
}

std::optional<AXTextMarkerRange> AXSearchManager::findMatchingRange(AccessibilitySearchCriteria&& criteria)
{
    AXTRACE("AXSearchManager::findMatchingRange"_s);

    // Currently, this method only supports searching for the next/previous misspelling.
    // FIXME: support other types of ranges, like italicized.
    if (criteria.searchKeys.size() != 1 || criteria.searchKeys[0] != AccessibilitySearchKey::MisspelledWord || criteria.resultsLimit != 1) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    // If there's no start object, it means we want to search everything.
    RefPtr startObject = criteria.startObject;
    if (!startObject)
        startObject = criteria.anchorObject;
    AXLOG(startObject);

    bool forward = criteria.searchDirection == AccessibilitySearchDirection::Next;
    if (match(*startObject, criteria)) {
        ASSERT(m_misspellingRanges.contains(startObject->objectID()));
        const auto& ranges = m_misspellingRanges.get(startObject->objectID());
        ASSERT(!ranges.isEmpty());

        AXTextMarkerRange startRange { startObject->treeID(), startObject->objectID(), criteria.startRange };
        if (forward) {
            for (auto& range : ranges) {
                if (range > startRange)
                    return range;
            }
        } else {
            for (auto& range : makeReversedRange(ranges)) {
                if (range < startRange)
                    return range;
            }
        }
    }

    // Didn't find a matching range for startObject, thus move to the next/previous object.
    auto objects = findMatchingObjectsInternal(criteria);
    if (!objects.isEmpty()) {
        Ref object = objects[0];
        ASSERT(m_misspellingRanges.contains(object->objectID()));
        const auto& ranges = m_misspellingRanges.get(object->objectID());
        ASSERT(!ranges.isEmpty());
        return forward ? ranges[0] : ranges.last();
    }
    return std::nullopt;
}

} // namespace WebCore
