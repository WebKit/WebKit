/*
 * Copyright (C) 2008-2009, 2011, 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityObject.h"

#include "AXObjectCache.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilityScrollView.h"
#include "AccessibilityTable.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMTokenList.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "HTMLDataListElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "MathMLNames.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RenderedPosition.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "TextCheckerClient.h"
#include "TextCheckingHelper.h"
#include "TextIterator.h"
#include "UserGestureIndicator.h"
#include "VisibleUnits.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

AccessibilityObject::~AccessibilityObject()
{
    ASSERT(isDetached());
}

void AccessibilityObject::detach(AccessibilityDetachmentType detachmentType, AXObjectCache* cache)
{
    // Menu close events need to notify the platform. No element is used in the notification because it's a destruction event.
    if (detachmentType == AccessibilityDetachmentType::ElementDestroyed && roleValue() == AccessibilityRole::Menu && cache)
        cache->postNotification(nullptr, &cache->document(), AXObjectCache::AXMenuClosed);
    
    // Clear any children and call detachFromParent on them so that
    // no children are left with dangling pointers to their parent.
    clearChildren();

#if ENABLE(ACCESSIBILITY)
    setWrapper(nullptr);
#endif
}

bool AccessibilityObject::isDetached() const
{
#if ENABLE(ACCESSIBILITY)
    return !wrapper();
#else
    return true;
#endif
}

bool AccessibilityObject::isAccessibilityObjectSearchMatchAtIndex(AccessibilityObject* axObject, AccessibilitySearchCriteria* criteria, size_t index)
{
    switch (criteria->searchKeys[index]) {
    // The AccessibilitySearchKey::AnyType matches any non-null AccessibilityObject.
    case AccessibilitySearchKey::AnyType:
        return true;
        
    case AccessibilitySearchKey::Article:
        return axObject->roleValue() == AccessibilityRole::DocumentArticle;
            
    case AccessibilitySearchKey::BlockquoteSameLevel:
        return criteria->startObject
            && axObject->isBlockquote()
            && axObject->blockquoteLevel() == criteria->startObject->blockquoteLevel();
        
    case AccessibilitySearchKey::Blockquote:
        return axObject->isBlockquote();
        
    case AccessibilitySearchKey::BoldFont:
        return axObject->hasBoldFont();
        
    case AccessibilitySearchKey::Button:
        return axObject->isButton();
        
    case AccessibilitySearchKey::CheckBox:
        return axObject->isCheckbox();
        
    case AccessibilitySearchKey::Control:
        return axObject->isControl();
        
    case AccessibilitySearchKey::DifferentType:
        return criteria->startObject
            && axObject->roleValue() != criteria->startObject->roleValue();
        
    case AccessibilitySearchKey::FontChange:
        return criteria->startObject
            && !axObject->hasSameFont(criteria->startObject->renderer());
        
    case AccessibilitySearchKey::FontColorChange:
        return criteria->startObject
            && !axObject->hasSameFontColor(criteria->startObject->renderer());
        
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
        return criteria->startObject
            && axObject->isHeading()
            && axObject->headingLevel() == criteria->startObject->headingLevel();
        
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
            isLink = axObject->isDescendantOfRole(AccessibilityRole::WebCoreLink);
#endif
        return isLink;
    }
        
    case AccessibilitySearchKey::List:
        return axObject->isList();
        
    case AccessibilitySearchKey::LiveRegion:
        return axObject->supportsLiveRegion();
        
    case AccessibilitySearchKey::MisspelledWord:
        return axObject->hasMisspelling();
        
    case AccessibilitySearchKey::Outline:
        return axObject->isTree();
        
    case AccessibilitySearchKey::PlainText:
        return axObject->hasPlainText();
        
    case AccessibilitySearchKey::RadioGroup:
        return axObject->isRadioGroup();
        
    case AccessibilitySearchKey::SameType:
        return criteria->startObject
            && axObject->roleValue() == criteria->startObject->roleValue();
        
    case AccessibilitySearchKey::StaticText:
        return axObject->isStaticText();
        
    case AccessibilitySearchKey::StyleChange:
        return criteria->startObject
            && !axObject->hasSameStyle(criteria->startObject->renderer());
        
    case AccessibilitySearchKey::TableSameLevel:
        return criteria->startObject
            && is<AccessibilityTable>(*axObject) && downcast<AccessibilityTable>(*axObject).isExposableThroughAccessibility()
            && downcast<AccessibilityTable>(*axObject).tableLevel() == criteria->startObject->tableLevel();
        
    case AccessibilitySearchKey::Table:
        return is<AccessibilityTable>(*axObject) && downcast<AccessibilityTable>(*axObject).isExposableThroughAccessibility();
        
    case AccessibilitySearchKey::TextField:
        return axObject->isTextControl();
        
    case AccessibilitySearchKey::Underline:
        return axObject->hasUnderline();
        
    case AccessibilitySearchKey::UnvisitedLink:
        return axObject->isUnvisited();
        
    case AccessibilitySearchKey::VisitedLink:
        return axObject->isVisited();
        
    default:
        return false;
    }
}

bool AccessibilityObject::isAccessibilityObjectSearchMatch(AccessibilityObject* axObject, AccessibilitySearchCriteria* criteria)
{
    if (!axObject || !criteria)
        return false;
    
    size_t length = criteria->searchKeys.size();
    for (size_t i = 0; i < length; ++i) {
        if (isAccessibilityObjectSearchMatchAtIndex(axObject, criteria, i)) {
            if (criteria->visibleOnly && !axObject->isOnscreen())
                return false;
            return true;
        }
    }
    return false;
}

bool AccessibilityObject::isAccessibilityTextSearchMatch(AccessibilityObject* axObject, AccessibilitySearchCriteria* criteria)
{
    if (!axObject || !criteria)
        return false;
    
    return axObject->accessibilityObjectContainsText(&criteria->searchText);
}

bool AccessibilityObject::accessibilityObjectContainsText(String* text) const
{
    // If text is null or empty we return true.
    return !text
        || text->isEmpty()
        || findPlainText(title(), *text, CaseInsensitive)
        || findPlainText(accessibilityDescription(), *text, CaseInsensitive)
        || findPlainText(stringValue(), *text, CaseInsensitive);
}

// ARIA marks elements as having their accessible name derive from either their contents, or their author provide name.
bool AccessibilityObject::accessibleNameDerivesFromContent() const
{
    // First check for objects specifically identified by ARIA.
    switch (ariaRoleAttribute()) {
    case AccessibilityRole::ApplicationAlert:
    case AccessibilityRole::ApplicationAlertDialog:
    case AccessibilityRole::ApplicationDialog:
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::ApplicationLog:
    case AccessibilityRole::ApplicationMarquee:
    case AccessibilityRole::ApplicationStatus:
    case AccessibilityRole::ApplicationTimer:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::Definition:
    case AccessibilityRole::Document:
    case AccessibilityRole::DocumentArticle:
    case AccessibilityRole::DocumentMath:
    case AccessibilityRole::DocumentNote:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::Form:
    case AccessibilityRole::Grid:
    case AccessibilityRole::Group:
    case AccessibilityRole::Image:
    case AccessibilityRole::List:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkSearch:
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::Meter:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::ScrollBar:
    case AccessibilityRole::Slider:
    case AccessibilityRole::SpinButton:
    case AccessibilityRole::Splitter:
    case AccessibilityRole::Table:
    case AccessibilityRole::TabList:
    case AccessibilityRole::TabPanel:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
    case AccessibilityRole::Toolbar:
    case AccessibilityRole::TreeGrid:
    case AccessibilityRole::Tree:
    case AccessibilityRole::WebApplication:
        return false;
    default:
        break;
    }
    
    // Now check for generically derived elements now that we know the element does not match a specific ARIA role.
    switch (roleValue()) {
    case AccessibilityRole::Slider:
    case AccessibilityRole::ListBox:
        return false;
    default:
        break;
    }
    
    return true;
}
    
String AccessibilityObject::computedLabel()
{
    // This method is being called by WebKit inspector, which may happen at any time, so we need to update our backing store now.
    // Also hold onto this object in case updateBackingStore deletes this node.
    RefPtr<AccessibilityObject> protectedThis(this);
    updateBackingStore();
    Vector<AccessibilityText> text;
    accessibilityText(text);
    if (text.size())
        return text[0].text;
    return String();
}

bool AccessibilityObject::isBlockquote() const
{
    return roleValue() == AccessibilityRole::Blockquote;
}

bool AccessibilityObject::isTextControl() const
{
    switch (roleValue()) {
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
        return true;
    default:
        return false;
    }
}
    
bool AccessibilityObject::isARIATextControl() const
{
    return ariaRoleAttribute() == AccessibilityRole::TextArea || ariaRoleAttribute() == AccessibilityRole::TextField || ariaRoleAttribute() == AccessibilityRole::SearchField;
}

bool AccessibilityObject::isNonNativeTextControl() const
{
    return (isARIATextControl() || hasContentEditableAttributeSet()) && !isNativeTextControl();
}

bool AccessibilityObject::isLandmark() const
{
    switch (roleValue()) {
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkSearch:
        return true;
    default:
        return false;
    }
}

bool AccessibilityObject::hasMisspelling() const
{
    if (!node())
        return false;
    
    Frame* frame = node()->document().frame();
    if (!frame)
        return false;
    
    Editor& editor = frame->editor();
    
    TextCheckerClient* textChecker = editor.textChecker();
    if (!textChecker)
        return false;
    
    bool isMisspelled = false;

    if (unifiedTextCheckerEnabled(frame)) {
        Vector<TextCheckingResult> results;
        checkTextOfParagraph(*textChecker, stringValue(), TextCheckingType::Spelling, results, frame->selection().selection());
        if (!results.isEmpty())
            isMisspelled = true;
        return isMisspelled;
    }

    int misspellingLength = 0;
    int misspellingLocation = -1;
    textChecker->checkSpellingOfString(stringValue(), &misspellingLocation, &misspellingLength);
    if (misspellingLength || misspellingLocation != -1)
        isMisspelled = true;
    
    return isMisspelled;
}

unsigned AccessibilityObject::blockquoteLevel() const
{
    unsigned level = 0;
    for (Node* elementNode = node(); elementNode; elementNode = elementNode->parentNode()) {
        if (elementNode->hasTagName(blockquoteTag))
            ++level;
    }
    
    return level;
}

AccessibilityObject* AccessibilityObject::parentObjectUnignored() const
{
    return const_cast<AccessibilityObject*>(AccessibilityObject::matchedParent(*this, false, [] (const AccessibilityObject& object) {
        return !object.accessibilityIsIgnored();
    }));
}

AccessibilityObject* AccessibilityObject::previousSiblingUnignored(int limit) const
{
    AccessibilityObject* previous;
    ASSERT(limit >= 0);
    for (previous = previousSibling(); previous && previous->accessibilityIsIgnored(); previous = previous->previousSibling()) {
        limit--;
        if (limit <= 0)
            break;
    }
    return previous;
}
    
FloatRect AccessibilityObject::convertFrameToSpace(const FloatRect& frameRect, AccessibilityConversionSpace conversionSpace) const
{
    ASSERT(isMainThread());
    
    // Find the appropriate scroll view to use to convert the contents to the window.
    const auto parentAccessibilityScrollView = ancestorAccessibilityScrollView(false /* includeSelf */);
    auto* parentScrollView = parentAccessibilityScrollView ? parentAccessibilityScrollView->scrollView() : nullptr;

    auto snappedFrameRect = snappedIntRect(IntRect(frameRect));
    if (parentScrollView)
        snappedFrameRect = parentScrollView->contentsToRootView(snappedFrameRect);
    
    if (conversionSpace == AccessibilityConversionSpace::Screen) {
        auto page = this->page();
        if (!page)
            return snappedFrameRect;

        // If we have an empty chrome client (like SVG) then we should use the page
        // of the scroll view parent to help us get to the screen rect.
        if (parentAccessibilityScrollView && page->chrome().client().isEmptyChromeClient())
            page = parentAccessibilityScrollView->page();
        
        snappedFrameRect = page->chrome().rootViewToAccessibilityScreen(snappedFrameRect);
    }
    
    return snappedFrameRect;
}
    
FloatRect AccessibilityObject::relativeFrame() const
{
    return convertFrameToSpace(elementRect(), AccessibilityConversionSpace::Page);
}

AccessibilityObject* AccessibilityObject::nextSiblingUnignored(int limit) const
{
    AccessibilityObject* next;
    ASSERT(limit >= 0);
    for (next = nextSibling(); next && next->accessibilityIsIgnored(); next = next->nextSibling()) {
        limit--;
        if (limit <= 0)
            break;
    }
    return next;
}

AccessibilityObject* AccessibilityObject::firstAccessibleObjectFromNode(const Node* node)
{
    return WebCore::firstAccessibleObjectFromNode(node, [] (const AccessibilityObject& accessible) {
        return !accessible.accessibilityIsIgnored();
    });
}

AccessibilityObject* firstAccessibleObjectFromNode(const Node* node, const WTF::Function<bool(const AccessibilityObject&)>& isAccessible)
{
    if (!node)
        return nullptr;

    AXObjectCache* cache = node->document().axObjectCache();
    if (!cache)
        return nullptr;

    AccessibilityObject* accessibleObject = cache->getOrCreate(node->renderer());
    while (accessibleObject && !isAccessible(*accessibleObject)) {
        node = NodeTraversal::next(*node);

        while (node && !node->renderer())
            node = NodeTraversal::nextSkippingChildren(*node);

        if (!node)
            return nullptr;

        accessibleObject = cache->getOrCreate(node->renderer());
    }

    return accessibleObject;
}

bool AccessibilityObject::isDescendantOfRole(AccessibilityRole role) const
{
    return AccessibilityObject::matchedParent(*this, false, [&role] (const AccessibilityObject& object) {
        return object.roleValue() == role;
    }) != nullptr;
}

static void appendAccessibilityObject(AccessibilityObject* object, AccessibilityObject::AccessibilityChildrenVector& results)
{
    // Find the next descendant of this attachment object so search can continue through frames.
    if (object->isAttachment()) {
        Widget* widget = object->widgetForAttachmentView();
        if (!is<FrameView>(widget))
            return;
        
        Document* document = downcast<FrameView>(*widget).frame().document();
        if (!document || !document->hasLivingRenderTree())
            return;
        
        object = object->axObjectCache()->getOrCreate(document);
    }

    if (object)
        results.append(object);
}
    
void AccessibilityObject::insertChild(AccessibilityObject* child, unsigned index)
{
    if (!child)
        return;
    
    // If the parent is asking for this child's children, then either it's the first time (and clearing is a no-op),
    // or its visibility has changed. In the latter case, this child may have a stale child cached.
    // This can prevent aria-hidden changes from working correctly. Hence, whenever a parent is getting children, ensure data is not stale.
    // Only clear the child's children when we know it's in the updating chain in order to avoid unnecessary work.
    if (child->needsToUpdateChildren() || m_subtreeDirty) {
        child->clearChildren();
        // Pass m_subtreeDirty flag down to the child so that children cache gets reset properly.
        if (m_subtreeDirty)
            child->setNeedsToUpdateSubtree();
    } else {
        // For some reason the grand children might be detached so that we need to regenerate the
        // children list of this child.
        for (const auto& grandChild : child->children(false)) {
            if (grandChild->isDetachedFromParent()) {
                child->clearChildren();
                break;
            }
        }
    }
    
    setIsIgnoredFromParentDataForChild(child);
    if (child->accessibilityIsIgnored()) {
        const auto& children = child->children();
        size_t length = children.size();
        for (size_t i = 0; i < length; ++i)
            m_children.insert(index + i, children[i]);
    } else {
        ASSERT(child->parentObject() == this);
        m_children.insert(index, child);
    }
    
    // Reset the child's m_isIgnoredFromParentData since we are done adding that child and its children.
    child->clearIsIgnoredFromParentData();
}
    
void AccessibilityObject::addChild(AccessibilityObject* child)
{
    insertChild(child, m_children.size());
}
    
static void appendChildrenToArray(AccessibilityObject* object, bool isForward, AccessibilityObject* startObject, AccessibilityObject::AccessibilityChildrenVector& results)
{
    // A table's children includes elements whose own children are also the table's children (due to the way the Mac exposes tables).
    // The rows from the table should be queried, since those are direct descendants of the table, and they contain content.
    const auto& searchChildren = is<AccessibilityTable>(*object) && downcast<AccessibilityTable>(*object).isExposableThroughAccessibility() ? downcast<AccessibilityTable>(*object).rows() : object->children();

    size_t childrenSize = searchChildren.size();

    size_t startIndex = isForward ? childrenSize : 0;
    size_t endIndex = isForward ? 0 : childrenSize;

    // If the startObject is ignored, we should use an accessible sibling as a start element instead.
    if (startObject && startObject->accessibilityIsIgnored() && startObject->isDescendantOfObject(object)) {
        AccessibilityObject* parentObject = startObject->parentObject();
        // Go up the parent chain to find the highest ancestor that's also being ignored.
        while (parentObject && parentObject->accessibilityIsIgnored()) {
            if (parentObject == object)
                break;
            startObject = parentObject;
            parentObject = parentObject->parentObject();
        }
        // Get the un-ignored sibling based on the search direction, and update the searchPosition.
        while (startObject && startObject->accessibilityIsIgnored())
            startObject = isForward ? startObject->previousSibling() : startObject->nextSibling();
    }
    
    size_t searchPosition = startObject ? searchChildren.find(startObject) : WTF::notFound;
    
    if (searchPosition != WTF::notFound) {
        if (isForward)
            endIndex = searchPosition + 1;
        else
            endIndex = searchPosition;
    }

    // This is broken into two statements so that it's easier read.
    if (isForward) {
        for (size_t i = startIndex; i > endIndex; i--)
            appendAccessibilityObject(searchChildren.at(i - 1).get(), results);
    } else {
        for (size_t i = startIndex; i < endIndex; i++)
            appendAccessibilityObject(searchChildren.at(i).get(), results);
    }
}

// Returns true if the number of results is now >= the number of results desired.
bool AccessibilityObject::objectMatchesSearchCriteriaWithResultLimit(AccessibilityObject* object, AccessibilitySearchCriteria* criteria, AccessibilityChildrenVector& results)
{
    if (isAccessibilityObjectSearchMatch(object, criteria) && isAccessibilityTextSearchMatch(object, criteria)) {
        results.append(object);
        
        // Enough results were found to stop searching.
        if (results.size() >= criteria->resultsLimit)
            return true;
    }
    
    return false;
}

void AccessibilityObject::findMatchingObjects(AccessibilitySearchCriteria* criteria, AccessibilityChildrenVector& results)
{
    ASSERT(criteria);
    
    if (!criteria)
        return;

    if (AXObjectCache* cache = axObjectCache())
        cache->startCachingComputedObjectAttributesUntilTreeMutates();

    // This search mechanism only searches the elements before/after the starting object.
    // It does this by stepping up the parent chain and at each level doing a DFS.
    
    // If there's no start object, it means we want to search everything.
    AccessibilityObject* startObject = criteria->startObject;
    if (!startObject)
        startObject = this;
    
    bool isForward = criteria->searchDirection == AccessibilitySearchDirection::Next;
    
    // The first iteration of the outer loop will examine the children of the start object for matches. However, when
    // iterating backwards, the start object children should not be considered, so the loop is skipped ahead. We make an
    // exception when no start object was specified because we want to search everything regardless of search direction.
    AccessibilityObject* previousObject = nullptr;
    if (!isForward && startObject != this) {
        previousObject = startObject;
        startObject = startObject->parentObjectUnignored();
    }
    
    // The outer loop steps up the parent chain each time (unignored is important here because otherwise elements would be searched twice)
    for (AccessibilityObject* stopSearchElement = parentObjectUnignored(); startObject && startObject != stopSearchElement; startObject = startObject->parentObjectUnignored()) {

        // Only append the children after/before the previous element, so that the search does not check elements that are 
        // already behind/ahead of start element.
        AccessibilityChildrenVector searchStack;
        if (!criteria->immediateDescendantsOnly || startObject == this)
            appendChildrenToArray(startObject, isForward, previousObject, searchStack);

        // This now does a DFS at the current level of the parent.
        while (!searchStack.isEmpty()) {
            AccessibilityObject* searchObject = searchStack.last().get();
            searchStack.removeLast();
            
            if (objectMatchesSearchCriteriaWithResultLimit(searchObject, criteria, results))
                break;
            
            if (!criteria->immediateDescendantsOnly)
                appendChildrenToArray(searchObject, isForward, 0, searchStack);
        }
        
        if (results.size() >= criteria->resultsLimit)
            break;

        // When moving backwards, the parent object needs to be checked, because technically it's "before" the starting element.
        if (!isForward && startObject != this && objectMatchesSearchCriteriaWithResultLimit(startObject, criteria, results))
            break;

        previousObject = startObject;
    }
}

// Returns the range that is fewer positions away from the reference range.
// NOTE: The after range is expected to ACTUALLY be after the reference range and the before
// range is expected to ACTUALLY be before. These are not checked for performance reasons.
static RefPtr<Range> rangeClosestToRange(RefPtr<Range> const& referenceRange, RefPtr<Range>&& afterRange, RefPtr<Range>&& beforeRange)
{
    if (!referenceRange)
        return nullptr;

    // The treeScope for shadow nodes may not be the same scope as another element in a document.
    // Comparisons may fail in that case, which are expected behavior and should not assert.
    if (afterRange && (referenceRange->endPosition().isNull() || ((afterRange->startPosition().anchorNode()->compareDocumentPosition(*referenceRange->endPosition().anchorNode()) & Node::DOCUMENT_POSITION_DISCONNECTED) == Node::DOCUMENT_POSITION_DISCONNECTED)))
        return nullptr;
    ASSERT(!afterRange || afterRange->compareBoundaryPoints(Range::START_TO_START, *referenceRange).releaseReturnValue() >= 0);

    if (beforeRange && (referenceRange->startPosition().isNull() || ((beforeRange->endPosition().anchorNode()->compareDocumentPosition(*referenceRange->startPosition().anchorNode()) & Node::DOCUMENT_POSITION_DISCONNECTED) == Node::DOCUMENT_POSITION_DISCONNECTED)))
        return nullptr;
    ASSERT(!beforeRange || beforeRange->compareBoundaryPoints(Range::START_TO_START, *referenceRange).releaseReturnValue() <= 0);

    if (!afterRange && !beforeRange)
        return nullptr;
    if (afterRange && !beforeRange)
        return WTFMove(afterRange);
    if (!afterRange && beforeRange)
        return WTFMove(beforeRange);
    
    unsigned positionsToAfterRange = Position::positionCountBetweenPositions(afterRange->startPosition(), referenceRange->endPosition());
    unsigned positionsToBeforeRange = Position::positionCountBetweenPositions(beforeRange->endPosition(), referenceRange->startPosition());
    
    return positionsToAfterRange < positionsToBeforeRange ? afterRange : beforeRange;
}

RefPtr<Range> AccessibilityObject::rangeOfStringClosestToRangeInDirection(Range* referenceRange, AccessibilitySearchDirection searchDirection, Vector<String> const& searchStrings) const
{
    Frame* frame = this->frame();
    if (!frame)
        return nullptr;
    
    if (!referenceRange)
        return nullptr;
    
    bool isBackwardSearch = searchDirection == AccessibilitySearchDirection::Previous;
    FindOptions findOptions { AtWordStarts, AtWordEnds, CaseInsensitive, StartInSelection };
    if (isBackwardSearch)
        findOptions.add(FindOptionFlag::Backwards);
    
    RefPtr<Range> closestStringRange = nullptr;
    for (const auto& searchString : searchStrings) {
        if (RefPtr<Range> searchStringRange = frame->editor().rangeOfString(searchString, referenceRange, findOptions)) {
            if (!closestStringRange)
                closestStringRange = searchStringRange;
            else {
                // If searching backward, use the trailing range edges to correctly determine which
                // range is closest. Similarly, if searching forward, use the leading range edges.
                Position closestStringPosition = isBackwardSearch ? closestStringRange->endPosition() : closestStringRange->startPosition();
                Position searchStringPosition = isBackwardSearch ? searchStringRange->endPosition() : searchStringRange->startPosition();
                
                int closestPositionOffset = closestStringPosition.computeOffsetInContainerNode();
                int searchPositionOffset = searchStringPosition.computeOffsetInContainerNode();
                Node* closestContainerNode = closestStringPosition.containerNode();
                Node* searchContainerNode = searchStringPosition.containerNode();
                
                short result = Range::compareBoundaryPoints(closestContainerNode, closestPositionOffset, searchContainerNode, searchPositionOffset).releaseReturnValue();
                if ((!isBackwardSearch && result > 0) || (isBackwardSearch && result < 0))
                    closestStringRange = searchStringRange;
            }
        }
    }
    return closestStringRange;
}

// Returns the range of the entire document if there is no selection.
RefPtr<Range> AccessibilityObject::selectionRange() const
{
    Frame* frame = this->frame();
    if (!frame)
        return nullptr;
    
    const VisibleSelection& selection = frame->selection().selection();
    if (!selection.isNone())
        return selection.firstRange();
    
    return Range::create(*frame->document());
}

RefPtr<Range> AccessibilityObject::elementRange() const
{    
    return AXObjectCache::rangeForNodeContents(node());
}

RefPtr<Range> AccessibilityObject::findTextRange(Vector<String> const& searchStrings, RefPtr<Range> const& start, AccessibilitySearchTextDirection direction) const
{
    RefPtr<Range> found;
    if (direction == AccessibilitySearchTextDirection::Forward)
        found = rangeOfStringClosestToRangeInDirection(start.get(), AccessibilitySearchDirection::Next, searchStrings);
    else if (direction == AccessibilitySearchTextDirection::Backward)
        found = rangeOfStringClosestToRangeInDirection(start.get(), AccessibilitySearchDirection::Previous, searchStrings);
    else if (direction == AccessibilitySearchTextDirection::Closest) {
        auto foundAfter = rangeOfStringClosestToRangeInDirection(start.get(), AccessibilitySearchDirection::Next, searchStrings);
        auto foundBefore = rangeOfStringClosestToRangeInDirection(start.get(), AccessibilitySearchDirection::Previous, searchStrings);
        found = rangeClosestToRange(start.get(), WTFMove(foundAfter), WTFMove(foundBefore));
    }

    if (found) {
        // If the search started within a text control, ensure that the result is inside that element.
        if (element() && element()->isTextField()) {
            if (!found->startContainer().isDescendantOrShadowDescendantOf(element())
                || !found->endContainer().isDescendantOrShadowDescendantOf(element()))
                return nullptr;
        }
    }
    return found;
}

Vector<RefPtr<Range>> AccessibilityObject::findTextRanges(AccessibilitySearchTextCriteria const& criteria) const
{
    Vector<RefPtr<Range>> result;

    // Determine start range.
    RefPtr<Range> startRange;
    if (criteria.start == AccessibilitySearchTextStartFrom::Selection)
        startRange = selectionRange();
    else
        startRange = elementRange();

    if (startRange) {
        // Collapse the range to the start unless searching from the end of the doc or searching backwards.
        if (criteria.start == AccessibilitySearchTextStartFrom::Begin)
            startRange->collapse(true);
        else if (criteria.start == AccessibilitySearchTextStartFrom::End)
            startRange->collapse(false);
        else
            startRange->collapse(criteria.direction != AccessibilitySearchTextDirection::Backward);
    } else
        return result;

    RefPtr<Range> found;
    switch (criteria.direction) {
    case AccessibilitySearchTextDirection::Forward:
    case AccessibilitySearchTextDirection::Backward:
    case AccessibilitySearchTextDirection::Closest:
        found = findTextRange(criteria.searchStrings, startRange, criteria.direction);
        if (found)
            result.append(found);
        break;
    case AccessibilitySearchTextDirection::All: {
        auto findAll = [&](AccessibilitySearchTextDirection dir) {
            found = findTextRange(criteria.searchStrings, startRange, dir);
            while (found) {
                result.append(found);
                found = findTextRange(criteria.searchStrings, found, dir);
            }
        };
        findAll(AccessibilitySearchTextDirection::Forward);
        findAll(AccessibilitySearchTextDirection::Backward);
        break;
    }
    }

    return result;
}

Vector<String> AccessibilityObject::performTextOperation(AccessibilityTextOperation const& operation)
{
    Vector<String> result;

    if (operation.textRanges.isEmpty())
        return result;

    Frame* frame = this->frame();
    if (!frame)
        return result;

    for (auto textRange : operation.textRanges) {
        if (!frame->selection().setSelectedRange(textRange.get(), DOWNSTREAM, FrameSelection::ShouldCloseTyping::Yes))
            continue;

        String text = textRange->text();
        String replacementString = operation.replacementText;
        bool replaceSelection = false;
        switch (operation.type) {
        case AccessibilityTextOperationType::Capitalize:
            replacementString = capitalize(text, ' '); // FIXME: Needs to take locale into account to work correctly.
            replaceSelection = true;
            break;
        case AccessibilityTextOperationType::Uppercase:
            replacementString = text.convertToUppercaseWithoutLocale(); // FIXME: Needs locale to work correctly.
            replaceSelection = true;
            break;
        case AccessibilityTextOperationType::Lowercase:
            replacementString = text.convertToLowercaseWithoutLocale(); // FIXME: Needs locale to work correctly.
            replaceSelection = true;
            break;
        case AccessibilityTextOperationType::Replace: {
            replaceSelection = true;
            // When applying find and replace activities, we want to match the capitalization of the replaced text,
            // (unless we're replacing with an abbreviation.)
            if (text.length() > 0
                && replacementString.length() > 2
                && replacementString != replacementString.convertToUppercaseWithoutLocale()) {
                if (text[0] == u_toupper(text[0]))
                    replacementString = capitalize(replacementString, ' '); // FIXME: Needs to take locale into account to work correctly.
                else
                    replacementString = replacementString.convertToLowercaseWithoutLocale(); // FIXME: Needs locale to work correctly.
            }
            break;
        }
        case AccessibilityTextOperationType::Select:
            break;
        }

        // A bit obvious, but worth noting the API contract for this method is that we should
        // return the replacement string when replacing, but the selected string if not.
        if (replaceSelection) {
            frame->editor().replaceSelectionWithText(replacementString, Editor::SelectReplacement::Yes, Editor::SmartReplace::Yes);
            result.append(replacementString);
        } else
            result.append(text);
    }

    return result;
}

bool AccessibilityObject::hasAttributesRequiredForInclusion() const
{
    // These checks are simplified in the interest of execution speed.
    if (!getAttribute(aria_helpAttr).isEmpty()
        || !getAttribute(aria_describedbyAttr).isEmpty()
        || !getAttribute(altAttr).isEmpty()
        || !getAttribute(titleAttr).isEmpty())
        return true;

#if ENABLE(MATHML)
    if (!getAttribute(MathMLNames::alttextAttr).isEmpty())
        return true;
#endif

    return false;
}

bool AccessibilityObject::isARIAInput(AccessibilityRole ariaRole)
{
    return ariaRole == AccessibilityRole::RadioButton || ariaRole == AccessibilityRole::CheckBox || ariaRole == AccessibilityRole::TextField || ariaRole == AccessibilityRole::Switch || ariaRole == AccessibilityRole::SearchField;
}    
    
bool AccessibilityObject::isARIAControl(AccessibilityRole ariaRole)
{
    return isARIAInput(ariaRole) || ariaRole == AccessibilityRole::TextArea || ariaRole == AccessibilityRole::Button || ariaRole == AccessibilityRole::ComboBox || ariaRole == AccessibilityRole::Slider || ariaRole == AccessibilityRole::ListBox;
}
    
bool AccessibilityObject::isRangeControl() const
{
    switch (roleValue()) {
    case AccessibilityRole::Meter:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::Slider:
    case AccessibilityRole::ScrollBar:
    case AccessibilityRole::SpinButton:
        return true;
    case AccessibilityRole::Splitter:
        return canSetFocusAttribute();
    default:
        return false;
    }
}

bool AccessibilityObject::isMeter() const
{
    if (ariaRoleAttribute() == AccessibilityRole::Meter)
        return true;

#if ENABLE(METER_ELEMENT)
    RenderObject* renderer = this->renderer();
    return renderer && renderer->isMeter();
#else
    return false;
#endif
}

IntPoint AccessibilityObject::clickPoint()
{
    LayoutRect rect = elementRect();
    return roundedIntPoint(LayoutPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2));
}

IntRect AccessibilityObject::boundingBoxForQuads(RenderObject* obj, const Vector<FloatQuad>& quads)
{
    ASSERT(obj);
    if (!obj)
        return IntRect();
    
    FloatRect result;
    for (const auto& quad : quads) {
        FloatRect r = quad.enclosingBoundingBox();
        if (!r.isEmpty()) {
            if (obj->style().hasAppearance())
                obj->theme().adjustRepaintRect(*obj, r);
            result.unite(r);
        }
    }
    return snappedIntRect(LayoutRect(result));
}
    
bool AccessibilityObject::press()
{
    // The presence of the actionElement will confirm whether we should even attempt a press.
    Element* actionElem = actionElement();
    if (!actionElem)
        return false;
    if (Frame* f = actionElem->document().frame())
        f->loader().resetMultipleFormSubmissionProtection();
    
    // Hit test at this location to determine if there is a sub-node element that should act
    // as the target of the action.
    Element* hitTestElement = nullptr;
    Document* document = this->document();
    if (document) {
        HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AccessibilityHitTest);
        HitTestResult hitTestResult(clickPoint());
        document->hitTest(request, hitTestResult);
        if (auto* innerNode = hitTestResult.innerNode()) {
            if (auto* shadowHost = innerNode->shadowHost())
                hitTestElement = shadowHost;
            else if (is<Element>(*innerNode))
                hitTestElement = &downcast<Element>(*innerNode);
            else
                hitTestElement = innerNode->parentElement();
        }
    }

    // Prefer the actionElement instead of this node, if the actionElement is inside this node.
    Element* pressElement = this->element();
    if (!pressElement || actionElem->isDescendantOf(*pressElement))
        pressElement = actionElem;
    
    ASSERT(pressElement);
    // Prefer the hit test element, if it is inside the target element.
    if (hitTestElement && hitTestElement->isDescendantOf(*pressElement))
        pressElement = hitTestElement;
    
    UserGestureIndicator gestureIndicator(ProcessingUserGesture, document);
    
    bool dispatchedTouchEvent = false;
#if PLATFORM(IOS_FAMILY)
    if (hasTouchEventListener())
        dispatchedTouchEvent = dispatchTouchEvent();
#endif
    if (!dispatchedTouchEvent)
        pressElement->accessKeyAction(true);
    
    return true;
}
    
bool AccessibilityObject::dispatchTouchEvent()
{
#if ENABLE(IOS_TOUCH_EVENTS)
    if (auto* frame = mainFrame())
        return frame->eventHandler().dispatchSimulatedTouchEvent(clickPoint());
#endif
    return false;
}

Frame* AccessibilityObject::frame() const
{
    Node* node = this->node();
    return node ? node->document().frame() : nullptr;
}

Frame* AccessibilityObject::mainFrame() const
{
    Document* document = topDocument();
    if (!document)
        return nullptr;
    
    Frame* frame = document->frame();
    if (!frame)
        return nullptr;
    
    return &frame->mainFrame();
}

Document* AccessibilityObject::topDocument() const
{
    if (!document())
        return nullptr;
    return &document()->topDocument();
}

String AccessibilityObject::language() const
{
    const AtomString& lang = getAttribute(langAttr);
    if (!lang.isEmpty())
        return lang;

    AccessibilityObject* parent = parentObject();
    
    // as a last resort, fall back to the content language specified in the meta tag
    if (!parent) {
        Document* doc = document();
        if (doc)
            return doc->contentLanguage();
        return nullAtom();
    }
    
    return parent->language();
}
    
VisiblePositionRange AccessibilityObject::visiblePositionRangeForUnorderedPositions(const VisiblePosition& visiblePos1, const VisiblePosition& visiblePos2) const
{
    if (visiblePos1.isNull() || visiblePos2.isNull())
        return VisiblePositionRange();

    // If there's no common tree scope between positions, return early.
    if (!commonTreeScope(visiblePos1.deepEquivalent().deprecatedNode(), visiblePos2.deepEquivalent().deprecatedNode()))
        return VisiblePositionRange();
    
    VisiblePosition startPos;
    VisiblePosition endPos;
    bool alreadyInOrder;

    // upstream is ordered before downstream for the same position
    if (visiblePos1 == visiblePos2 && visiblePos2.affinity() == UPSTREAM)
        alreadyInOrder = false;

    // use selection order to see if the positions are in order
    else
        alreadyInOrder = VisibleSelection(visiblePos1, visiblePos2).isBaseFirst();

    if (alreadyInOrder) {
        startPos = visiblePos1;
        endPos = visiblePos2;
    } else {
        startPos = visiblePos2;
        endPos = visiblePos1;
    }

    return VisiblePositionRange(startPos, endPos);
}

VisiblePositionRange AccessibilityObject::positionOfLeftWord(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfWord(visiblePos, LeftWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::positionOfRightWord(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfWord(visiblePos, RightWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

static VisiblePosition updateAXLineStartForVisiblePosition(const VisiblePosition& visiblePosition)
{
    // A line in the accessibility sense should include floating objects, such as aligned image, as part of a line.
    // So let's update the position to include that.
    VisiblePosition tempPosition;
    VisiblePosition startPosition = visiblePosition;
    while (true) {
        tempPosition = startPosition.previous();
        if (tempPosition.isNull())
            break;
        Position p = tempPosition.deepEquivalent();
        RenderObject* renderer = p.deprecatedNode()->renderer();
        if (!renderer || (renderer->isRenderBlock() && !p.deprecatedEditingOffset()))
            break;
        if (!RenderedPosition(tempPosition).isNull())
            break;
        startPosition = tempPosition;
    }

    return startPosition;
}

VisiblePositionRange AccessibilityObject::leftLineVisiblePositionRange(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    // make a caret selection for the position before marker position (to make sure
    // we move off of a line start)
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePositionRange();

    VisiblePosition startPosition = startOfLine(prevVisiblePos);

    // keep searching for a valid line start position.  Unless the VisiblePosition is at the very beginning, there should
    // always be a valid line range.  However, startOfLine will return null for position next to a floating object,
    // since floating object doesn't really belong to any line.
    // This check will reposition the marker before the floating object, to ensure we get a line start.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    VisiblePosition endPosition = endOfLine(prevVisiblePos);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::rightLineVisiblePositionRange(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    // make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePositionRange();

    VisiblePosition startPosition = startOfLine(nextVisiblePos);

    // fetch for a valid line start position
    if (startPosition.isNull()) {
        startPosition = visiblePos;
        nextVisiblePos = nextVisiblePos.next();
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // Unless the VisiblePosition is at the very end, there should always be a valid line range.  However, endOfLine will
    // return null for position by a floating object, since floating object doesn't really belong to any line.
    // This check will reposition the marker after the floating object, to ensure we get a line end.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }

    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::sentenceForPosition(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition startPosition = startOfSentence(visiblePos);
    VisiblePosition endPosition = endOfSentence(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::paragraphForPosition(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfParagraph(visiblePos);
    VisiblePosition endPosition = endOfParagraph(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

static VisiblePosition startOfStyleRange(const VisiblePosition& visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().deprecatedNode()->renderer();
    RenderObject* startRenderer = renderer;
    auto* style = &renderer->style();

    // traverse backward by renderer to look for style change
    for (RenderObject* r = renderer->previousInPreOrder(); r; r = r->previousInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChildSlow())
            continue;

        // stop at style change
        if (&r->style() != style)
            break;

        // remember match
        startRenderer = r;
    }

    return firstPositionInOrBeforeNode(startRenderer->node());
}

static VisiblePosition endOfStyleRange(const VisiblePosition& visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().deprecatedNode()->renderer();
    RenderObject* endRenderer = renderer;
    const RenderStyle& style = renderer->style();

    // traverse forward by renderer to look for style change
    for (RenderObject* r = renderer->nextInPreOrder(); r; r = r->nextInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChildSlow())
            continue;

        // stop at style change
        if (&r->style() != &style)
            break;

        // remember match
        endRenderer = r;
    }

    return lastPositionInOrAfterNode(endRenderer->node());
}

VisiblePositionRange AccessibilityObject::styleRangeForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    return VisiblePositionRange(startOfStyleRange(visiblePos), endOfStyleRange(visiblePos));
}

// NOTE: Consider providing this utility method as AX API
VisiblePositionRange AccessibilityObject::visiblePositionRangeForRange(const PlainTextRange& range) const
{
    unsigned textLength = getLengthForTextRange();
    if (range.start + range.length > textLength)
        return VisiblePositionRange();

    VisiblePosition startPosition = visiblePositionForIndex(range.start);
    startPosition.setAffinity(DOWNSTREAM);
    VisiblePosition endPosition = visiblePositionForIndex(range.start + range.length);
    return VisiblePositionRange(startPosition, endPosition);
}

RefPtr<Range> AccessibilityObject::rangeForPlainTextRange(const PlainTextRange& range) const
{
    unsigned textLength = getLengthForTextRange();
    if (range.start + range.length > textLength)
        return nullptr;
    
    if (AXObjectCache* cache = axObjectCache()) {
        CharacterOffset start = cache->characterOffsetForIndex(range.start, this);
        CharacterOffset end = cache->characterOffsetForIndex(range.start + range.length, this);
        return cache->rangeForUnorderedCharacterOffsets(start, end);
    }
    return nullptr;
}

VisiblePositionRange AccessibilityObject::lineRangeForPosition(const VisiblePosition& visiblePosition) const
{
    VisiblePosition startPosition = startOfLine(visiblePosition);
    VisiblePosition endPosition = endOfLine(visiblePosition);
    return VisiblePositionRange(startPosition, endPosition);
}

bool AccessibilityObject::replacedNodeNeedsCharacter(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !isRendererReplacedElement(replacedNode->renderer()) || replacedNode->isTextNode())
        return false;

    // create an AX object, but skip it if it is not supposed to be seen
    AccessibilityObject* object = replacedNode->renderer()->document().axObjectCache()->getOrCreate(replacedNode);
    if (object->accessibilityIsIgnored())
        return false;

    return true;
}

// Finds a RenderListItem parent give a node.
static RenderListItem* renderListItemContainerForNode(Node* node)
{
    for (; node; node = node->parentNode()) {
        RenderBoxModelObject* renderer = node->renderBoxModelObject();
        if (is<RenderListItem>(renderer))
            return downcast<RenderListItem>(renderer);
    }
    return nullptr;
}

static String listMarkerTextForNode(Node* node)
{
    RenderListItem* listItem = renderListItemContainerForNode(node);
    if (!listItem)
        return String();
    
    // If this is in a list item, we need to manually add the text for the list marker
    // because a RenderListMarker does not have a Node equivalent and thus does not appear
    // when iterating text.
    return listItem->markerTextWithSuffix();
}

// Returns the text associated with a list marker if this node is contained within a list item.
String AccessibilityObject::listMarkerTextForNodeAndPosition(Node* node, const VisiblePosition& visiblePositionStart)
{
    // If the range does not contain the start of the line, the list marker text should not be included.
    if (!isStartOfLine(visiblePositionStart))
        return String();

    // We should speak the list marker only for the first line.
    RenderListItem* listItem = renderListItemContainerForNode(node);
    if (!listItem)
        return String();
    if (!inSameLine(visiblePositionStart, firstPositionInNode(&listItem->element())))
        return String();
    
    return listMarkerTextForNode(node);
}

String AccessibilityObject::stringForRange(RefPtr<Range> range) const
{
    if (!range)
        return String();
    
    TextIterator it(range.get());
    if (it.atEnd())
        return String();
    
    StringBuilder builder;
    for (; !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length()) {
            // Add a textual representation for list marker text.
            // Don't add list marker text for new line character.
            if (it.text().length() != 1 || !isSpaceOrNewline(it.text()[0]))
                builder.append(listMarkerTextForNodeAndPosition(it.node(), VisiblePosition(range->startPosition())));
            it.appendTextToStringBuilder(builder);
        } else {
            // locate the node and starting offset for this replaced range
            Node& node = it.range()->startContainer();
            ASSERT(&node == &it.range()->endContainer());
            int offset = it.range()->startOffset();
            if (replacedNodeNeedsCharacter(node.traverseToChildAt(offset)))
                builder.append(objectReplacementCharacter);
        }
    }
    
    return builder.toString();
}

String AccessibilityObject::stringForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange)
{
    if (visiblePositionRange.isNull())
        return String();

    StringBuilder builder;
    RefPtr<Range> range = makeRange(visiblePositionRange.start, visiblePositionRange.end);
    for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length()) {
            // Add a textual representation for list marker text.
            builder.append(listMarkerTextForNodeAndPosition(it.node(), visiblePositionRange.start));
            it.appendTextToStringBuilder(builder);
        } else {
            // locate the node and starting offset for this replaced range
            Node& node = it.range()->startContainer();
            ASSERT(&node == &it.range()->endContainer());
            int offset = it.range()->startOffset();
            if (replacedNodeNeedsCharacter(node.traverseToChildAt(offset)))
                builder.append(objectReplacementCharacter);
        }
    }

    return builder.toString();
}

int AccessibilityObject::lengthForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    // FIXME: Multi-byte support
    if (visiblePositionRange.isNull())
        return -1;
    
    int length = 0;
    RefPtr<Range> range = makeRange(visiblePositionRange.start, visiblePositionRange.end);
    for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length())
            length += it.text().length();
        else {
            // locate the node and starting offset for this replaced range
            Node& node = it.range()->startContainer();
            ASSERT(&node == &it.range()->endContainer());
            int offset = it.range()->startOffset();

            if (replacedNodeNeedsCharacter(node.traverseToChildAt(offset)))
                ++length;
        }
    }
    
    return length;
}

VisiblePosition AccessibilityObject::visiblePositionForBounds(const IntRect& rect, AccessibilityVisiblePositionForBounds visiblePositionForBounds) const
{
    if (rect.isEmpty())
        return VisiblePosition();
    
    auto* mainFrame = this->mainFrame();
    if (!mainFrame)
        return VisiblePosition();
    
    // FIXME: Add support for right-to-left languages.
    IntPoint corner = (visiblePositionForBounds == AccessibilityVisiblePositionForBounds::First) ? rect.minXMinYCorner() : rect.maxXMaxYCorner();
    VisiblePosition position = mainFrame->visiblePositionForPoint(corner);
    
    if (rect.contains(position.absoluteCaretBounds().center()))
        return position;
    
    // If the initial position is located outside the bounds adjust it incrementally as needed.
    VisiblePosition nextPosition = position.next();
    VisiblePosition previousPosition = position.previous();
    while (nextPosition.isNotNull() || previousPosition.isNotNull()) {
        if (rect.contains(nextPosition.absoluteCaretBounds().center()))
            return nextPosition;
        if (rect.contains(previousPosition.absoluteCaretBounds().center()))
            return previousPosition;
        
        nextPosition = nextPosition.next();
        previousPosition = previousPosition.previous();
    }
    
    return VisiblePosition();
}

VisiblePosition AccessibilityObject::nextWordEnd(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a word end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    return endOfWord(nextVisiblePos, LeftWordIfOnBoundary);
}

VisiblePosition AccessibilityObject::previousWordStart(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a word start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePosition();

    return startOfWord(prevVisiblePos, RightWordIfOnBoundary);
}

VisiblePosition AccessibilityObject::nextLineEndPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // to make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // There are cases like when the position is next to a floating object that'll return null for end of line. This code will avoid returning null.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }

    return endPosition;
}

VisiblePosition AccessibilityObject::previousLineStartPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a line start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePosition();

    VisiblePosition startPosition = startOfLine(prevVisiblePos);

    // as long as the position hasn't reached the beginning of the doc,  keep searching for a valid line start position
    // There are cases like when the position is next to a floating object that'll return null for start of line. This code will avoid returning null.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    return startPosition;
}

VisiblePosition AccessibilityObject::nextSentenceEndPosition(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a sentence end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    // an empty line is considered a sentence. If it's skipped, then the sentence parser will not
    // see this empty line.  Instead, return the end position of the empty line.
    VisiblePosition endPosition;
    
    String lineString = plainText(makeRange(startOfLine(nextVisiblePos), endOfLine(nextVisiblePos)).get());
    if (lineString.isEmpty())
        endPosition = nextVisiblePos;
    else
        endPosition = endOfSentence(nextVisiblePos);

    return endPosition;
}

VisiblePosition AccessibilityObject::previousSentenceStartPosition(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a sentence start
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull())
        return VisiblePosition();

    // treat empty line as a separate sentence.
    VisiblePosition startPosition;
    
    String lineString = plainText(makeRange(startOfLine(previousVisiblePos), endOfLine(previousVisiblePos)).get());
    if (lineString.isEmpty())
        startPosition = previousVisiblePos;
    else
        startPosition = startOfSentence(previousVisiblePos);

    return startPosition;
}

VisiblePosition AccessibilityObject::nextParagraphEndPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a paragraph end
    VisiblePosition nextPos = visiblePos.next();
    if (nextPos.isNull())
        return VisiblePosition();

    return endOfParagraph(nextPos);
}

VisiblePosition AccessibilityObject::previousParagraphStartPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a paragraph start
    VisiblePosition previousPos = visiblePos.previous();
    if (previousPos.isNull())
        return VisiblePosition();

    return startOfParagraph(previousPos);
}

AccessibilityObject* AccessibilityObject::accessibilityObjectForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return nullptr;

    RenderObject* obj = visiblePos.deepEquivalent().deprecatedNode()->renderer();
    if (!obj)
        return nullptr;

    return obj->document().axObjectCache()->getOrCreate(obj);
}
    
// If you call node->hasEditableStyle() since that will return true if an ancestor is editable.
// This only returns true if this is the element that actually has the contentEditable attribute set.
bool AccessibilityObject::hasContentEditableAttributeSet() const
{
    return contentEditableAttributeIsEnabled(element());
}

bool AccessibilityObject::supportsReadOnly() const
{
    AccessibilityRole role = roleValue();

    return role == AccessibilityRole::CheckBox
        || role == AccessibilityRole::ColumnHeader
        || role == AccessibilityRole::ComboBox
        || role == AccessibilityRole::Grid
        || role == AccessibilityRole::GridCell
        || role == AccessibilityRole::ListBox
        || role == AccessibilityRole::MenuItemCheckbox
        || role == AccessibilityRole::MenuItemRadio
        || role == AccessibilityRole::RadioGroup
        || role == AccessibilityRole::RowHeader
        || role == AccessibilityRole::SearchField
        || role == AccessibilityRole::Slider
        || role == AccessibilityRole::SpinButton
        || role == AccessibilityRole::Switch
        || role == AccessibilityRole::TextField
        || role == AccessibilityRole::TreeGrid
        || isPasswordField();
}

String AccessibilityObject::readOnlyValue() const
{
    if (!hasAttribute(aria_readonlyAttr))
        return ariaRoleAttribute() != AccessibilityRole::Unknown && supportsReadOnly() ? "false" : String();

    return getAttribute(aria_readonlyAttr).string().convertToASCIILowercase();
}

bool AccessibilityObject::supportsAutoComplete() const
{
    return (isComboBox() || isARIATextControl()) && hasAttribute(aria_autocompleteAttr);
}

String AccessibilityObject::autoCompleteValue() const
{
    const AtomString& autoComplete = getAttribute(aria_autocompleteAttr);
    if (equalLettersIgnoringASCIICase(autoComplete, "inline")
        || equalLettersIgnoringASCIICase(autoComplete, "list")
        || equalLettersIgnoringASCIICase(autoComplete, "both"))
        return autoComplete;

    return "none";
}

bool AccessibilityObject::contentEditableAttributeIsEnabled(Element* element)
{
    if (!element)
        return false;
    
    const AtomString& contentEditableValue = element->attributeWithoutSynchronization(contenteditableAttr);
    if (contentEditableValue.isNull())
        return false;
    
    // Both "true" (case-insensitive) and the empty string count as true.
    return contentEditableValue.isEmpty() || equalLettersIgnoringASCIICase(contentEditableValue, "true");
}
    
#if ENABLE(ACCESSIBILITY)
int AccessibilityObject::lineForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull() || !node())
        return -1;

    // If the position is not in the same editable region as this AX object, return -1.
    Node* containerNode = visiblePos.deepEquivalent().containerNode();
    if (!containerNode->containsIncludingShadowDOM(node()) && !node()->containsIncludingShadowDOM(containerNode))
        return -1;

    int lineCount = -1;
    VisiblePosition currentVisiblePos = visiblePos;
    VisiblePosition savedVisiblePos;

    // move up until we get to the top
    // FIXME: This only takes us to the top of the rootEditableElement, not the top of the
    // top document.
    do {
        savedVisiblePos = currentVisiblePos;
        VisiblePosition prevVisiblePos = previousLinePosition(currentVisiblePos, 0, HasEditableAXRole);
        currentVisiblePos = prevVisiblePos;
        ++lineCount;
    }  while (currentVisiblePos.isNotNull() && !(inSameLine(currentVisiblePos, savedVisiblePos)));

    return lineCount;
}
#endif

// NOTE: Consider providing this utility method as AX API
PlainTextRange AccessibilityObject::plainTextRangeForVisiblePositionRange(const VisiblePositionRange& positionRange) const
{
    int index1 = index(positionRange.start);
    int index2 = index(positionRange.end);
    if (index1 < 0 || index2 < 0 || index1 > index2)
        return PlainTextRange();

    return PlainTextRange(index1, index2 - index1);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given screen coordinates. This parameterized attribute returns the
// complete range of characters (including surrogate pairs of multi-byte glyphs) at the given
// screen coordinates.
// NOTE: This varies from AppKit when the point is below the last line. AppKit returns an
// an error in that case. We return textControl->text().length(), 1. Does this matter?
PlainTextRange AccessibilityObject::doAXRangeForPosition(const IntPoint& point) const
{
    int i = index(visiblePositionForPoint(point));
    if (i < 0)
        return PlainTextRange();

    return PlainTextRange(i, 1);
}

// Given a character index, the range of text associated with this accessibility object
// over which the style in effect at that character index applies.
PlainTextRange AccessibilityObject::doAXStyleRangeForIndex(unsigned index) const
{
    VisiblePositionRange range = styleRangeForPosition(visiblePositionForIndex(index, false));
    return plainTextRangeForVisiblePositionRange(range);
}

// Given an indexed character, the line number of the text associated with this accessibility
// object that contains the character.
unsigned AccessibilityObject::doAXLineForIndex(unsigned index)
{
    return lineForPosition(visiblePositionForIndex(index, false));
}

#if ENABLE(ACCESSIBILITY)
void AccessibilityObject::updateBackingStore()
{
    if (!axObjectCache())
        return;
    
    // Updating the layout may delete this object.
    RefPtr<AccessibilityObject> protectedThis(this);
    if (auto* document = this->document()) {
        if (!document->view()->layoutContext().isInRenderTreeLayout() && !document->inRenderTreeUpdate() && !document->inStyleRecalc())
            document->updateLayoutIgnorePendingStylesheets();
    }

    if (auto cache = axObjectCache())
        cache->performDeferredCacheUpdate();
    
    updateChildrenIfNecessary();
}
#endif

const AccessibilityScrollView* AccessibilityObject::ancestorAccessibilityScrollView(bool includeSelf) const
{
    return downcast<AccessibilityScrollView>(AccessibilityObject::matchedParent(*this, includeSelf, [] (const auto& object) {
        return is<AccessibilityScrollView>(object);
    }));
}

ScrollView* AccessibilityObject::scrollViewAncestor() const
{
    if (auto parentScrollView = ancestorAccessibilityScrollView(true/* includeSelf */))
        return parentScrollView->scrollView();
    
    return nullptr;
}
    
Document* AccessibilityObject::document() const
{
    FrameView* frameView = documentFrameView();
    if (!frameView)
        return nullptr;
    
    return frameView->frame().document();
}
    
Page* AccessibilityObject::page() const
{
    Document* document = this->document();
    if (!document)
        return nullptr;
    return document->page();
}

FrameView* AccessibilityObject::documentFrameView() const 
{ 
    const AccessibilityObject* object = this;
    while (object && !object->isAccessibilityRenderObject()) 
        object = object->parentObject();
        
    if (!object)
        return nullptr;

    return object->documentFrameView();
}

#if ENABLE(ACCESSIBILITY)
const AccessibilityObject::AccessibilityChildrenVector& AccessibilityObject::children(bool updateChildrenIfNeeded)
{
    if (updateChildrenIfNeeded)
        updateChildrenIfNecessary();

    return m_children;
}
#endif

void AccessibilityObject::updateChildrenIfNecessary()
{
    if (!hasChildren()) {
        // Enable the cache in case we end up adding a lot of children, we don't want to recompute axIsIgnored each time.
        AXAttributeCacheEnabler enableCache(axObjectCache());
        addChildren();
    }
}
    
void AccessibilityObject::clearChildren()
{
    // Some objects have weak pointers to their parents and those associations need to be detached.
    for (const auto& child : m_children)
        child->detachFromParent();
    
    m_children.clear();
    m_haveChildren = false;
}

AccessibilityObject* AccessibilityObject::anchorElementForNode(Node* node)
{
    RenderObject* obj = node->renderer();
    if (!obj)
        return nullptr;
    
    RefPtr<AccessibilityObject> axObj = obj->document().axObjectCache()->getOrCreate(obj);
    Element* anchor = axObj->anchorElement();
    if (!anchor)
        return nullptr;
    
    RenderObject* anchorRenderer = anchor->renderer();
    if (!anchorRenderer)
        return nullptr;
    
    return anchorRenderer->document().axObjectCache()->getOrCreate(anchorRenderer);
}

AccessibilityObject* AccessibilityObject::headingElementForNode(Node* node)
{
    if (!node)
        return nullptr;
    
    RenderObject* renderObject = node->renderer();
    if (!renderObject)
        return nullptr;
    
    AccessibilityObject* axObject = renderObject->document().axObjectCache()->getOrCreate(renderObject);
    
    return const_cast<AccessibilityObject*>(AccessibilityObject::matchedParent(*axObject, true, [] (const AccessibilityObject& object) {
        return object.roleValue() == AccessibilityRole::Heading;
    }));
}

const AccessibilityObject* AccessibilityObject::matchedParent(const AccessibilityObject& object, bool includeSelf, const WTF::Function<bool(const AccessibilityObject&)>& matches)
{
    const AccessibilityObject* parent = includeSelf ? &object : object.parentObject();
    for (; parent; parent = parent->parentObject()) {
        if (matches(*parent))
            return parent;
    }
    return nullptr;
}

void AccessibilityObject::ariaTreeRows(AccessibilityChildrenVector& result)
{
    for (const auto& child : children()) {
        // Add tree items as the rows.
        if (child->roleValue() == AccessibilityRole::TreeItem)
            result.append(child);

        // Now see if this item also has rows hiding inside of it.
        child->ariaTreeRows(result);
    }
}
    
void AccessibilityObject::ariaTreeItemContent(AccessibilityChildrenVector& result)
{
    // The ARIA tree item content are the item that are not other tree items or their containing groups.
    for (const auto& child : children()) {
        if (!child->isGroup() && child->roleValue() != AccessibilityRole::TreeItem)
            result.append(child);
    }
}

void AccessibilityObject::ariaTreeItemDisclosedRows(AccessibilityChildrenVector& result)
{
    for (const auto& obj : children()) {
        // Add tree items as the rows.
        if (obj->roleValue() == AccessibilityRole::TreeItem)
            result.append(obj);
        // If it's not a tree item, then descend into the group to find more tree items.
        else 
            obj->ariaTreeRows(result);
    }    
}
    
const String AccessibilityObject::defaultLiveRegionStatusForRole(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::ApplicationAlertDialog:
    case AccessibilityRole::ApplicationAlert:
        return "assertive"_s;
    case AccessibilityRole::ApplicationLog:
    case AccessibilityRole::ApplicationStatus:
        return "polite"_s;
    case AccessibilityRole::ApplicationTimer:
    case AccessibilityRole::ApplicationMarquee:
        return "off"_s;
    default:
        return nullAtom();
    }
}
    
#if ENABLE(ACCESSIBILITY)
const String& AccessibilityObject::actionVerb() const
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Need to add verbs for select elements.
    static NeverDestroyed<const String> buttonAction(AXButtonActionVerb());
    static NeverDestroyed<const String> textFieldAction(AXTextFieldActionVerb());
    static NeverDestroyed<const String> radioButtonAction(AXRadioButtonActionVerb());
    static NeverDestroyed<const String> checkedCheckBoxAction(AXCheckedCheckBoxActionVerb());
    static NeverDestroyed<const String> uncheckedCheckBoxAction(AXUncheckedCheckBoxActionVerb());
    static NeverDestroyed<const String> linkAction(AXLinkActionVerb());
    static NeverDestroyed<const String> menuListAction(AXMenuListActionVerb());
    static NeverDestroyed<const String> menuListPopupAction(AXMenuListPopupActionVerb());
    static NeverDestroyed<const String> listItemAction(AXListItemActionVerb());

    switch (roleValue()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::ToggleButton:
        return buttonAction;
    case AccessibilityRole::TextField:
    case AccessibilityRole::TextArea:
        return textFieldAction;
    case AccessibilityRole::RadioButton:
        return radioButtonAction;
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::Switch:
        return isChecked() ? checkedCheckBoxAction : uncheckedCheckBoxAction;
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
        return linkAction;
    case AccessibilityRole::PopUpButton:
        return menuListAction;
    case AccessibilityRole::MenuListPopup:
        return menuListPopupAction;
    case AccessibilityRole::ListItem:
        return listItemAction;
    default:
        return nullAtom();
    }
#else
    return nullAtom();
#endif
}
#endif

bool AccessibilityObject::ariaIsMultiline() const
{
    return equalLettersIgnoringASCIICase(getAttribute(aria_multilineAttr), "true");
}

String AccessibilityObject::invalidStatus() const
{
    String grammarValue = "grammar"_s;
    String falseValue = "false"_s;
    String spellingValue = "spelling"_s;
    String trueValue = "true"_s;
    String undefinedValue = "undefined"_s;

    // aria-invalid can return false (default), grammar, spelling, or true.
    String ariaInvalid = stripLeadingAndTrailingHTMLSpaces(getAttribute(aria_invalidAttr));
    
    if (ariaInvalid.isEmpty()) {
        // We should expose invalid status for input types.
        Node* node = this->node();
        if (node && is<HTMLInputElement>(*node)) {
            HTMLInputElement& input = downcast<HTMLInputElement>(*node);
            if (input.hasBadInput() || input.typeMismatch())
                return trueValue;
        }
        return falseValue;
    }
    
    // If "false", "undefined" [sic, string value], empty, or missing, return "false".
    if (ariaInvalid == falseValue || ariaInvalid == undefinedValue)
        return falseValue;
    // Besides true/false/undefined, the only tokens defined by WAI-ARIA 1.0...
    // ...for @aria-invalid are "grammar" and "spelling".
    if (ariaInvalid == grammarValue)
        return grammarValue;
    if (ariaInvalid == spellingValue)
        return spellingValue;
    // Any other non empty string should be treated as "true".
    return trueValue;
}

bool AccessibilityObject::supportsCurrent() const
{
    return hasAttribute(aria_currentAttr);
}
 
AccessibilityCurrentState AccessibilityObject::currentState() const
{
    // aria-current can return false (default), true, page, step, location, date or time.
    String currentStateValue = stripLeadingAndTrailingHTMLSpaces(getAttribute(aria_currentAttr));
    
    // If "false", empty, or missing, return false state.
    if (currentStateValue.isEmpty() || currentStateValue == "false")
        return AccessibilityCurrentState::False;
    
    if (currentStateValue == "page")
        return AccessibilityCurrentState::Page;
    if (currentStateValue == "step")
        return AccessibilityCurrentState::Step;
    if (currentStateValue == "location")
        return AccessibilityCurrentState::Location;
    if (currentStateValue == "date")
        return AccessibilityCurrentState::Date;
    if (currentStateValue == "time")
        return AccessibilityCurrentState::Time;
    
    // Any value not included in the list of allowed values should be treated as "true".
    return AccessibilityCurrentState::True;
}

String AccessibilityObject::currentValue() const
{
    switch (currentState()) {
    case AccessibilityCurrentState::False:
        return "false";
    case AccessibilityCurrentState::Page:
        return "page";
    case AccessibilityCurrentState::Step:
        return "step";
    case AccessibilityCurrentState::Location:
        return "location";
    case AccessibilityCurrentState::Time:
        return "time";
    case AccessibilityCurrentState::Date:
        return "date";
    default:
    case AccessibilityCurrentState::True:
        return "true";
    }
}

bool AccessibilityObject::isModalDescendant(Node* modalNode) const
{
    Node* node = this->node();
    if (!modalNode || !node)
        return false;
    
    if (node == modalNode)
        return true;
    
    // ARIA 1.1 aria-modal, indicates whether an element is modal when displayed.
    // For the decendants of the modal object, they should also be considered as aria-modal=true.
    return node->isDescendantOf(*modalNode);
}

bool AccessibilityObject::isModalNode() const
{
    if (AXObjectCache* cache = axObjectCache())
        return node() && cache->modalNode() == node();

    return false;
}

bool AccessibilityObject::ignoredFromModalPresence() const
{
    // We shouldn't ignore the top node.
    if (!node() || !node()->parentNode())
        return false;
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return false;
    
    // modalNode is the current displayed modal dialog.
    Node* modalNode = cache->modalNode();
    if (!modalNode)
        return false;
    
    // We only want to ignore the objects within the same frame as the modal dialog.
    if (modalNode->document().frame() != this->frame())
        return false;
    
    return !isModalDescendant(modalNode);
}

bool AccessibilityObject::hasTagName(const QualifiedName& tagName) const
{
    Node* node = this->node();
    return is<Element>(node) && downcast<Element>(*node).hasTagName(tagName);
}
    
bool AccessibilityObject::hasAttribute(const QualifiedName& attribute) const
{
    Node* node = this->node();
    if (!is<Element>(node))
        return false;
    
    return downcast<Element>(*node).hasAttributeWithoutSynchronization(attribute);
}
    
const AtomString& AccessibilityObject::getAttribute(const QualifiedName& attribute) const
{
    if (auto* element = this->element())
        return element->attributeWithoutSynchronization(attribute);
    return nullAtom();
}

bool AccessibilityObject::replaceTextInRange(const String& replacementString, const PlainTextRange& range)
{
    if (!renderer() || !is<Element>(node()))
        return false;

    auto& element = downcast<Element>(*renderer()->node());

    // We should use the editor's insertText to mimic typing into the field.
    // Also only do this when the field is in editing mode.
    auto& frame = renderer()->frame();
    if (element.shouldUseInputMethod()) {
        frame.selection().setSelectedRange(rangeForPlainTextRange(range).get(), DOWNSTREAM, FrameSelection::ShouldCloseTyping::Yes);
        frame.editor().replaceSelectionWithText(replacementString, Editor::SelectReplacement::No, Editor::SmartReplace::No);
        return true;
    }
    
    if (is<HTMLInputElement>(element)) {
        downcast<HTMLInputElement>(element).setRangeText(replacementString, range.start, range.length, "");
        return true;
    }
    if (is<HTMLTextAreaElement>(element)) {
        downcast<HTMLTextAreaElement>(element).setRangeText(replacementString, range.start, range.length, "");
        return true;
    }

    return false;
}

bool AccessibilityObject::insertText(const String& text)
{
    if (!renderer() || !is<Element>(node()))
        return false;

    auto& element = downcast<Element>(*renderer()->node());

    // Only try to insert text if the field is in editing mode.
    if (!element.shouldUseInputMethod())
        return false;

    // Use Editor::insertText to mimic typing into the field.
    auto& editor = renderer()->frame().editor();
    return editor.insertText(text, nullptr);
}

// Lacking concrete evidence of orientation, horizontal means width > height. vertical is height > width;
AccessibilityOrientation AccessibilityObject::orientation() const
{
    LayoutRect bounds = elementRect();
    if (bounds.size().width() > bounds.size().height())
        return AccessibilityOrientation::Horizontal;
    if (bounds.size().height() > bounds.size().width())
        return AccessibilityOrientation::Vertical;

    return AccessibilityOrientation::Undefined;
}    

bool AccessibilityObject::isDescendantOfObject(const AccessibilityObject* axObject) const
{
    if (!axObject || !axObject->hasChildren())
        return false;
    
    return AccessibilityObject::matchedParent(*this, false, [axObject] (const AccessibilityObject& object) {
        return &object == axObject;
    }) != nullptr;
}

bool AccessibilityObject::isAncestorOfObject(const AccessibilityObject* axObject) const
{
    if (!axObject)
        return false;

    return this == axObject || axObject->isDescendantOfObject(this);
}

AccessibilityObject* AccessibilityObject::firstAnonymousBlockChild() const
{
    for (AccessibilityObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->renderer() && child->renderer()->isAnonymousBlock())
            return child;
    }
    return nullptr;
}

using ARIARoleMap = HashMap<String, AccessibilityRole, ASCIICaseInsensitiveHash>;
using ARIAReverseRoleMap = HashMap<AccessibilityRole, String, DefaultHash<int>::Hash, WTF::UnsignedWithZeroKeyHashTraits<int>>;

static ARIARoleMap* gAriaRoleMap = nullptr;
static ARIAReverseRoleMap* gAriaReverseRoleMap = nullptr;

struct RoleEntry {
    String ariaRole;
    AccessibilityRole webcoreRole;
};

static void initializeRoleMap()
{
    if (gAriaRoleMap)
        return;
    ASSERT(!gAriaReverseRoleMap);

    const RoleEntry roles[] = {
        { "alert", AccessibilityRole::ApplicationAlert },
        { "alertdialog", AccessibilityRole::ApplicationAlertDialog },
        { "application", AccessibilityRole::WebApplication },
        { "article", AccessibilityRole::DocumentArticle },
        { "banner", AccessibilityRole::LandmarkBanner },
        { "blockquote", AccessibilityRole::Blockquote },
        { "button", AccessibilityRole::Button },
        { "caption", AccessibilityRole::Caption },
        { "checkbox", AccessibilityRole::CheckBox },
        { "complementary", AccessibilityRole::LandmarkComplementary },
        { "contentinfo", AccessibilityRole::LandmarkContentInfo },
        { "deletion", AccessibilityRole::Deletion },
        { "dialog", AccessibilityRole::ApplicationDialog },
        { "directory", AccessibilityRole::Directory },
        // The 'doc-*' roles are defined the ARIA DPUB mobile: https://www.w3.org/TR/dpub-aam-1.0/ 
        // Editor's draft is currently at https://rawgit.com/w3c/aria/master/dpub-aam/dpub-aam.html 
        { "doc-abstract", AccessibilityRole::ApplicationTextGroup },
        { "doc-acknowledgments", AccessibilityRole::LandmarkDocRegion },
        { "doc-afterword", AccessibilityRole::LandmarkDocRegion },
        { "doc-appendix", AccessibilityRole::LandmarkDocRegion },
        { "doc-backlink", AccessibilityRole::WebCoreLink },
        { "doc-biblioentry", AccessibilityRole::ListItem },
        { "doc-bibliography", AccessibilityRole::LandmarkDocRegion },
        { "doc-biblioref", AccessibilityRole::WebCoreLink },
        { "doc-chapter", AccessibilityRole::LandmarkDocRegion },
        { "doc-colophon", AccessibilityRole::ApplicationTextGroup },
        { "doc-conclusion", AccessibilityRole::LandmarkDocRegion },
        { "doc-cover", AccessibilityRole::Image },
        { "doc-credit", AccessibilityRole::ApplicationTextGroup },
        { "doc-credits", AccessibilityRole::LandmarkDocRegion },
        { "doc-dedication", AccessibilityRole::ApplicationTextGroup },
        { "doc-endnote", AccessibilityRole::ListItem },
        { "doc-endnotes", AccessibilityRole::LandmarkDocRegion },
        { "doc-epigraph", AccessibilityRole::ApplicationTextGroup },
        { "doc-epilogue", AccessibilityRole::LandmarkDocRegion },
        { "doc-errata", AccessibilityRole::LandmarkDocRegion },
        { "doc-example", AccessibilityRole::ApplicationTextGroup },
        { "doc-footnote", AccessibilityRole::Footnote },
        { "doc-foreword", AccessibilityRole::LandmarkDocRegion },
        { "doc-glossary", AccessibilityRole::LandmarkDocRegion },
        { "doc-glossref", AccessibilityRole::WebCoreLink },
        { "doc-index", AccessibilityRole::LandmarkNavigation },
        { "doc-introduction", AccessibilityRole::LandmarkDocRegion },
        { "doc-noteref", AccessibilityRole::WebCoreLink },
        { "doc-notice", AccessibilityRole::DocumentNote },
        { "doc-pagebreak", AccessibilityRole::Splitter },
        { "doc-pagelist", AccessibilityRole::LandmarkNavigation },
        { "doc-part", AccessibilityRole::LandmarkDocRegion },
        { "doc-preface", AccessibilityRole::LandmarkDocRegion },
        { "doc-prologue", AccessibilityRole::LandmarkDocRegion },
        { "doc-pullquote", AccessibilityRole::ApplicationTextGroup },
        { "doc-qna", AccessibilityRole::ApplicationTextGroup },
        { "doc-subtitle", AccessibilityRole::Heading },
        { "doc-tip", AccessibilityRole::DocumentNote },
        { "doc-toc", AccessibilityRole::LandmarkNavigation },
        { "figure", AccessibilityRole::Figure },
        // The mappings for 'graphics-*' roles are defined in this spec: https://w3c.github.io/graphics-aam/
        { "graphics-document", AccessibilityRole::GraphicsDocument },
        { "graphics-object", AccessibilityRole::GraphicsObject },
        { "graphics-symbol", AccessibilityRole::GraphicsSymbol },
        { "grid", AccessibilityRole::Grid },
        { "gridcell", AccessibilityRole::GridCell },
        { "table", AccessibilityRole::Table },
        { "cell", AccessibilityRole::Cell },
        { "columnheader", AccessibilityRole::ColumnHeader },
        { "combobox", AccessibilityRole::ComboBox },
        { "definition", AccessibilityRole::Definition },
        { "document", AccessibilityRole::Document },
        { "feed", AccessibilityRole::Feed },
        { "form", AccessibilityRole::Form },
        { "rowheader", AccessibilityRole::RowHeader },
        { "group", AccessibilityRole::ApplicationGroup },
        { "heading", AccessibilityRole::Heading },
        { "img", AccessibilityRole::Image },
        { "insertion", AccessibilityRole::Insertion },
        { "link", AccessibilityRole::WebCoreLink },
        { "list", AccessibilityRole::List },
        { "listitem", AccessibilityRole::ListItem },
        { "listbox", AccessibilityRole::ListBox },
        { "log", AccessibilityRole::ApplicationLog },
        { "main", AccessibilityRole::LandmarkMain },
        { "marquee", AccessibilityRole::ApplicationMarquee },
        { "math", AccessibilityRole::DocumentMath },
        { "menu", AccessibilityRole::Menu },
        { "menubar", AccessibilityRole::MenuBar },
        { "menuitem", AccessibilityRole::MenuItem },
        { "menuitemcheckbox", AccessibilityRole::MenuItemCheckbox },
        { "menuitemradio", AccessibilityRole::MenuItemRadio },
        { "meter", AccessibilityRole::Meter },
        { "none", AccessibilityRole::Presentational },
        { "note", AccessibilityRole::DocumentNote },
        { "navigation", AccessibilityRole::LandmarkNavigation },
        { "option", AccessibilityRole::ListBoxOption },
        { "paragraph", AccessibilityRole::Paragraph },
        { "presentation", AccessibilityRole::Presentational },
        { "progressbar", AccessibilityRole::ProgressIndicator },
        { "radio", AccessibilityRole::RadioButton },
        { "radiogroup", AccessibilityRole::RadioGroup },
        { "region", AccessibilityRole::LandmarkRegion },
        { "row", AccessibilityRole::Row },
        { "rowgroup", AccessibilityRole::RowGroup },
        { "scrollbar", AccessibilityRole::ScrollBar },
        { "search", AccessibilityRole::LandmarkSearch },
        { "searchbox", AccessibilityRole::SearchField },
        { "separator", AccessibilityRole::Splitter },
        { "slider", AccessibilityRole::Slider },
        { "spinbutton", AccessibilityRole::SpinButton },
        { "status", AccessibilityRole::ApplicationStatus },
        { "subscript", AccessibilityRole::Subscript },
        { "superscript", AccessibilityRole::Superscript },
        { "switch", AccessibilityRole::Switch },
        { "tab", AccessibilityRole::Tab },
        { "tablist", AccessibilityRole::TabList },
        { "tabpanel", AccessibilityRole::TabPanel },
        { "text", AccessibilityRole::StaticText },
        { "textbox", AccessibilityRole::TextArea },
        { "term", AccessibilityRole::Term },
        { "time", AccessibilityRole::Time },
        { "timer", AccessibilityRole::ApplicationTimer },
        { "toolbar", AccessibilityRole::Toolbar },
        { "tooltip", AccessibilityRole::UserInterfaceTooltip },
        { "tree", AccessibilityRole::Tree },
        { "treegrid", AccessibilityRole::TreeGrid },
        { "treeitem", AccessibilityRole::TreeItem }
    };

    gAriaRoleMap = new ARIARoleMap;
    gAriaReverseRoleMap = new ARIAReverseRoleMap;
    size_t roleLength = WTF_ARRAY_LENGTH(roles);
    for (size_t i = 0; i < roleLength; ++i) {
        gAriaRoleMap->set(roles[i].ariaRole, roles[i].webcoreRole);
        gAriaReverseRoleMap->set(static_cast<int>(roles[i].webcoreRole), roles[i].ariaRole);
    }
}

static ARIARoleMap& ariaRoleMap()
{
    initializeRoleMap();
    return *gAriaRoleMap;
}

static ARIAReverseRoleMap& reverseAriaRoleMap()
{
    initializeRoleMap();
    return *gAriaReverseRoleMap;
}

AccessibilityRole AccessibilityObject::ariaRoleToWebCoreRole(const String& value)
{
    if (value.isNull() || value.isEmpty())
        return AccessibilityRole::Unknown;

    for (auto roleName : StringView(value).split(' ')) {
        AccessibilityRole role = ariaRoleMap().get<ASCIICaseInsensitiveStringViewHashTranslator>(roleName);
        if (static_cast<int>(role))
            return role;
    }
    return AccessibilityRole::Unknown;
}

String AccessibilityObject::computedRoleString() const
{
    // FIXME: Need a few special cases that aren't in the RoleMap: option, etc. http://webkit.org/b/128296
    AccessibilityRole role = roleValue();

    if (role == AccessibilityRole::Image && accessibilityIsIgnored())
        return reverseAriaRoleMap().get(static_cast<int>(AccessibilityRole::Presentational));

    // We do not compute a role string for generic block elements with user-agent assigned roles.
    if (role == AccessibilityRole::Group || role == AccessibilityRole::TextGroup)
        return "";

    // We do compute a role string for block elements with author-provided roles.
    if (role == AccessibilityRole::ApplicationTextGroup
        || role == AccessibilityRole::Footnote
        || role == AccessibilityRole::GraphicsObject)
        return reverseAriaRoleMap().get(static_cast<int>(AccessibilityRole::ApplicationGroup));

    if (role == AccessibilityRole::GraphicsDocument)
        return reverseAriaRoleMap().get(static_cast<int>(AccessibilityRole::Document));

    if (role == AccessibilityRole::GraphicsSymbol)
        return reverseAriaRoleMap().get(static_cast<int>(AccessibilityRole::Image));

    if (role == AccessibilityRole::HorizontalRule)
        return reverseAriaRoleMap().get(static_cast<int>(AccessibilityRole::Splitter));

    if (role == AccessibilityRole::PopUpButton || role == AccessibilityRole::ToggleButton)
        return reverseAriaRoleMap().get(static_cast<int>(AccessibilityRole::Button));

    if (role == AccessibilityRole::LandmarkDocRegion)
        return reverseAriaRoleMap().get(static_cast<int>(AccessibilityRole::LandmarkRegion));

    return reverseAriaRoleMap().get(static_cast<int>(role));
}

bool AccessibilityObject::hasHighlighting() const
{
    for (Node* node = this->node(); node; node = node->parentNode()) {
        if (node->hasTagName(markTag))
            return true;
    }
    
    return false;
}

String AccessibilityObject::roleDescription() const
{
    return stripLeadingAndTrailingHTMLSpaces(getAttribute(aria_roledescriptionAttr));
}
    
bool nodeHasPresentationRole(Node* node)
{
    return nodeHasRole(node, "presentation") || nodeHasRole(node, "none");
}
    
bool AccessibilityObject::supportsPressAction() const
{
    if (isButton())
        return true;
    if (roleValue() == AccessibilityRole::Details)
        return true;
    
    Element* actionElement = this->actionElement();
    if (!actionElement)
        return false;
    
    // [Bug: 136247] Heuristic: element handlers that have more than one accessible descendant should not be exposed as supporting press.
    if (actionElement != element()) {
        if (AccessibilityObject* axObj = axObjectCache()->getOrCreate(actionElement)) {
            AccessibilityChildrenVector results;
            // Search within for immediate descendants that are static text. If we find more than one
            // then this is an event delegator actionElement and we should expose the press action.
            Vector<AccessibilitySearchKey> keys({ AccessibilitySearchKey::StaticText, AccessibilitySearchKey::Control, AccessibilitySearchKey::Graphic, AccessibilitySearchKey::Heading, AccessibilitySearchKey::Link });
            AccessibilitySearchCriteria criteria(axObj, AccessibilitySearchDirection::Next, emptyString(), 2, false, false);
            criteria.searchKeys = keys;
            axObj->findMatchingObjects(&criteria, results);
            if (results.size() > 1)
                return false;
        }
    }
    
    // [Bug: 133613] Heuristic: If the action element is presentational, we shouldn't expose press as a supported action.
    return !nodeHasPresentationRole(actionElement);
}

bool AccessibilityObject::supportsDatetimeAttribute() const
{
    return hasTagName(insTag) || hasTagName(delTag) || hasTagName(timeTag);
}

const AtomString& AccessibilityObject::datetimeAttributeValue() const
{
    return getAttribute(datetimeAttr);
}
    
const AtomString& AccessibilityObject::linkRelValue() const
{
    return getAttribute(relAttr);
}
    
const String AccessibilityObject::keyShortcutsValue() const
{
    return getAttribute(aria_keyshortcutsAttr);
}

Element* AccessibilityObject::element() const
{
    Node* node = this->node();
    if (is<Element>(node))
        return downcast<Element>(node);
    return nullptr;
}
    
bool AccessibilityObject::isValueAutofillAvailable() const
{
    if (!isNativeTextControl())
        return false;
    
    Node* node = this->node();
    if (!is<HTMLInputElement>(node))
        return false;
    
    return downcast<HTMLInputElement>(*node).isAutoFillAvailable() || downcast<HTMLInputElement>(*node).autoFillButtonType() != AutoFillButtonType::None;
}

AutoFillButtonType AccessibilityObject::valueAutofillButtonType() const
{
    if (!isValueAutofillAvailable())
        return AutoFillButtonType::None;
    
    return downcast<HTMLInputElement>(*this->node()).autoFillButtonType();
}
    
bool AccessibilityObject::isValueAutofilled() const
{
    if (!isNativeTextControl())
        return false;
    
    Node* node = this->node();
    if (!is<HTMLInputElement>(node))
        return false;
    
    return downcast<HTMLInputElement>(*node).isAutoFilled();
}

const String AccessibilityObject::placeholderValue() const
{
    const AtomString& placeholder = getAttribute(placeholderAttr);
    if (!placeholder.isEmpty())
        return placeholder;
    
    const AtomString& ariaPlaceholder = getAttribute(aria_placeholderAttr);
    if (!ariaPlaceholder.isEmpty())
        return ariaPlaceholder;
    
    return nullAtom();
}
    
bool AccessibilityObject::isInsideLiveRegion(bool excludeIfOff) const
{
    return liveRegionAncestor(excludeIfOff);
}
    
AccessibilityObject* AccessibilityObject::liveRegionAncestor(bool excludeIfOff) const
{
    return const_cast<AccessibilityObject*>(AccessibilityObject::matchedParent(*this, true, [excludeIfOff] (const AccessibilityObject& object) {
        return object.supportsLiveRegion(excludeIfOff);
    }));
}

bool AccessibilityObject::supportsARIAAttributes() const
{
    // This returns whether the element supports any global ARIA attributes.
    return supportsLiveRegion()
        || supportsARIADragging()
        || supportsARIADropping()
        || supportsARIAOwns()
        || hasAttribute(aria_atomicAttr)
        || hasAttribute(aria_busyAttr)
        || hasAttribute(aria_controlsAttr)
        || hasAttribute(aria_currentAttr)
        || hasAttribute(aria_describedbyAttr)
        || hasAttribute(aria_detailsAttr)
        || hasAttribute(aria_disabledAttr)
        || hasAttribute(aria_errormessageAttr)
        || hasAttribute(aria_flowtoAttr)
        || hasAttribute(aria_haspopupAttr)
        || hasAttribute(aria_invalidAttr)
        || hasAttribute(aria_labelAttr)
        || hasAttribute(aria_labelledbyAttr)
        || hasAttribute(aria_relevantAttr);
}
    
bool AccessibilityObject::liveRegionStatusIsEnabled(const AtomString& liveRegionStatus)
{
    return equalLettersIgnoringASCIICase(liveRegionStatus, "polite") || equalLettersIgnoringASCIICase(liveRegionStatus, "assertive");
}
    
bool AccessibilityObject::supportsLiveRegion(bool excludeIfOff) const
{
    const AtomString& liveRegionStatusValue = liveRegionStatus();
    return excludeIfOff ? liveRegionStatusIsEnabled(liveRegionStatusValue) : !liveRegionStatusValue.isEmpty();
}

AccessibilityObjectInterface* AccessibilityObject::elementAccessibilityHitTest(const IntPoint& point) const
{ 
    // Send the hit test back into the sub-frame if necessary.
    if (isAttachment()) {
        Widget* widget = widgetForAttachmentView();
        // Normalize the point for the widget's bounds.
        if (widget && widget->isFrameView()) {
            if (AXObjectCache* cache = axObjectCache())
                return cache->getOrCreate(widget)->accessibilityHitTest(IntPoint(point - widget->frameRect().location()));
        }
    }
    
    // Check if there are any mock elements that need to be handled.
    for (const auto& child : m_children) {
        if (child->isMockObject() && child->elementRect().contains(point))
            return child->elementAccessibilityHitTest(point);
    }

    return const_cast<AccessibilityObject*>(this);
}
    
AXObjectCache* AccessibilityObject::axObjectCache() const
{
    auto* document = this->document();
    return document ? document->axObjectCache() : nullptr;
}
    
AccessibilityObjectInterface* AccessibilityObject::focusedUIElement() const
{
    auto* page = this->page();
    return page ? AXObjectCache::focusedUIElementForPage(page) : nullptr;
}
    
AccessibilitySortDirection AccessibilityObject::sortDirection() const
{
    AccessibilityRole role = roleValue();
    if (role != AccessibilityRole::RowHeader && role != AccessibilityRole::ColumnHeader)
        return AccessibilitySortDirection::Invalid;

    const AtomString& sortAttribute = getAttribute(aria_sortAttr);
    if (equalLettersIgnoringASCIICase(sortAttribute, "ascending"))
        return AccessibilitySortDirection::Ascending;
    if (equalLettersIgnoringASCIICase(sortAttribute, "descending"))
        return AccessibilitySortDirection::Descending;
    if (equalLettersIgnoringASCIICase(sortAttribute, "other"))
        return AccessibilitySortDirection::Other;
    
    return AccessibilitySortDirection::None;
}

bool AccessibilityObject::supportsRangeValue() const
{
    return isProgressIndicator()
        || isSlider()
        || isScrollbar()
        || isSpinButton()
        || (isSplitter() && canSetFocusAttribute())
        || isAttachmentElement();
}
    
bool AccessibilityObject::supportsHasPopup() const
{
    return hasAttribute(aria_haspopupAttr) || isComboBox();
}

String AccessibilityObject::popupValue() const
{
    static const NeverDestroyed<HashSet<String>> allowedPopupValues(std::initializer_list<String> {
        "menu", "listbox", "tree", "grid", "dialog"
    });

    auto hasPopup = getAttribute(aria_haspopupAttr).convertToASCIILowercase();
    if (hasPopup.isNull() || hasPopup.isEmpty()) {
        // In ARIA 1.1, the implicit value for combobox became "listbox."
        if (isComboBox() || hasDatalist())
            return "listbox";
        return "false";
    }

    if (allowedPopupValues->contains(hasPopup))
        return hasPopup;

    // aria-haspopup specification states that true must be treated as menu.
    if (hasPopup == "true")
        return "menu";

    // The spec states that "User agents must treat any value of aria-haspopup that is not
    // included in the list of allowed values, including an empty string, as if the value
    // false had been provided."
    return "false";
}

bool AccessibilityObject::hasDatalist() const
{
#if ENABLE(DATALIST_ELEMENT)
    auto datalistId = getAttribute(listAttr);
    if (datalistId.isNull() || datalistId.isEmpty())
        return false;

    auto element = this->element();
    if (!element)
        return false;

    auto datalist = element->treeScope().getElementById(datalistId);
    return is<HTMLDataListElement>(datalist);
#else
    return false;
#endif
}

bool AccessibilityObject::supportsSetSize() const
{
    return hasAttribute(aria_setsizeAttr);
}

bool AccessibilityObject::supportsPosInSet() const
{
    return hasAttribute(aria_posinsetAttr);
}
    
int AccessibilityObject::setSize() const
{
    return getAttribute(aria_setsizeAttr).toInt();
}

int AccessibilityObject::posInSet() const
{
    return getAttribute(aria_posinsetAttr).toInt();
}
    
const AtomString& AccessibilityObject::identifierAttribute() const
{
    return getAttribute(idAttr);
}
    
void AccessibilityObject::classList(Vector<String>& classList) const
{
    Node* node = this->node();
    if (!is<Element>(node))
        return;
    
    Element* element = downcast<Element>(node);
    DOMTokenList& list = element->classList();
    unsigned length = list.length();
    for (unsigned k = 0; k < length; k++)
        classList.append(list.item(k).string());
}

bool AccessibilityObject::supportsPressed() const
{
    const AtomString& expanded = getAttribute(aria_pressedAttr);
    return equalLettersIgnoringASCIICase(expanded, "true") || equalLettersIgnoringASCIICase(expanded, "false");
}
    
bool AccessibilityObject::supportsExpanded() const
{
    // Undefined values should not result in this attribute being exposed to ATs according to ARIA.
    const AtomString& expanded = getAttribute(aria_expandedAttr);
    if (equalLettersIgnoringASCIICase(expanded, "true") || equalLettersIgnoringASCIICase(expanded, "false"))
        return true;
    switch (roleValue()) {
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::DisclosureTriangle:
    case AccessibilityRole::Details:
        return true;
    default:
        return false;
    }
}
    
bool AccessibilityObject::isExpanded() const
{
    if (equalLettersIgnoringASCIICase(getAttribute(aria_expandedAttr), "true"))
        return true;
    
    if (is<HTMLDetailsElement>(node()))
        return downcast<HTMLDetailsElement>(node())->isOpen();
    
    // Summary element should use its details parent's expanded status.
    if (isSummary()) {
        if (const AccessibilityObject* parent = AccessibilityObject::matchedParent(*this, false, [] (const AccessibilityObject& object) {
            return is<HTMLDetailsElement>(object.node());
        }))
            return parent->isExpanded();
    }
    
    return false;  
}

bool AccessibilityObject::supportsChecked() const
{
    switch (roleValue()) {
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Switch:
        return true;
    default:
        return false;
    }
}

AccessibilityButtonState AccessibilityObject::checkboxOrRadioValue() const
{
    // If this is a real checkbox or radio button, AccessibilityRenderObject will handle.
    // If it's an ARIA checkbox, radio, or switch the aria-checked attribute should be used.
    // If it's a toggle button, the aria-pressed attribute is consulted.

    if (isToggleButton()) {
        const AtomString& ariaPressed = getAttribute(aria_pressedAttr);
        if (equalLettersIgnoringASCIICase(ariaPressed, "true"))
            return AccessibilityButtonState::On;
        if (equalLettersIgnoringASCIICase(ariaPressed, "mixed"))
            return AccessibilityButtonState::Mixed;
        return AccessibilityButtonState::Off;
    }
    
    const AtomString& result = getAttribute(aria_checkedAttr);
    if (equalLettersIgnoringASCIICase(result, "true"))
        return AccessibilityButtonState::On;
    if (equalLettersIgnoringASCIICase(result, "mixed")) {
        // ARIA says that radio, menuitemradio, and switch elements must NOT expose button state mixed.
        AccessibilityRole ariaRole = ariaRoleAttribute();
        if (ariaRole == AccessibilityRole::RadioButton || ariaRole == AccessibilityRole::MenuItemRadio || ariaRole == AccessibilityRole::Switch)
            return AccessibilityButtonState::Off;
        return AccessibilityButtonState::Mixed;
    }
    
    if (isIndeterminate())
        return AccessibilityButtonState::Mixed;
    
    return AccessibilityButtonState::Off;
}

// This is a 1-dimensional scroll offset helper function that's applied
// separately in the horizontal and vertical directions, because the
// logic is the same. The goal is to compute the best scroll offset
// in order to make an object visible within a viewport.
//
// If the object is already fully visible, returns the same scroll
// offset.
//
// In case the whole object cannot fit, you can specify a
// subfocus - a smaller region within the object that should
// be prioritized. If the whole object can fit, the subfocus is
// ignored.
//
// If possible, the object and subfocus are centered within the
// viewport.
//
// Example 1: the object is already visible, so nothing happens.
//   +----------Viewport---------+
//                 +---Object---+
//                 +--SubFocus--+
//
// Example 2: the object is not fully visible, so it's centered
// within the viewport.
//   Before:
//   +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
//   After:
//                 +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
// Example 3: the object is larger than the viewport, so the
// viewport moves to show as much of the object as possible,
// while also trying to center the subfocus.
//   Before:
//   +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
//   After:
//             +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
// When constraints cannot be fully satisfied, the min
// (left/top) position takes precedence over the max (right/bottom).
//
// Note that the return value represents the ideal new scroll offset.
// This may be out of range - the calling function should clip this
// to the available range.
static int computeBestScrollOffset(int currentScrollOffset, int subfocusMin, int subfocusMax, int objectMin, int objectMax, int viewportMin, int viewportMax)
{
    int viewportSize = viewportMax - viewportMin;
    
    // If the object size is larger than the viewport size, consider
    // only a portion that's as large as the viewport, centering on
    // the subfocus as much as possible.
    if (objectMax - objectMin > viewportSize) {
        // Since it's impossible to fit the whole object in the
        // viewport, exit now if the subfocus is already within the viewport.
        if (subfocusMin - currentScrollOffset >= viewportMin && subfocusMax - currentScrollOffset <= viewportMax)
            return currentScrollOffset;
        
        // Subfocus must be within focus.
        subfocusMin = std::max(subfocusMin, objectMin);
        subfocusMax = std::min(subfocusMax, objectMax);
        
        // Subfocus must be no larger than the viewport size; favor top/left.
        if (subfocusMax - subfocusMin > viewportSize)
            subfocusMax = subfocusMin + viewportSize;
        
        // Compute the size of an object centered on the subfocus, the size of the viewport.
        int centeredObjectMin = (subfocusMin + subfocusMax - viewportSize) / 2;
        int centeredObjectMax = centeredObjectMin + viewportSize;

        objectMin = std::max(objectMin, centeredObjectMin);
        objectMax = std::min(objectMax, centeredObjectMax);
    }

    // Exit now if the focus is already within the viewport.
    if (objectMin - currentScrollOffset >= viewportMin
        && objectMax - currentScrollOffset <= viewportMax)
        return currentScrollOffset;
    
    // Center the object in the viewport.
    return (objectMin + objectMax - viewportMin - viewportMax) / 2;
}

bool AccessibilityObject::isOnscreen() const
{   
    bool isOnscreen = true;

    // To figure out if the element is onscreen, we start by building of a stack starting with the
    // element, and then include every scrollable parent in the hierarchy.
    Vector<const AccessibilityObject*> objects;
    
    objects.append(this);
    for (AccessibilityObject* parentObject = this->parentObject(); parentObject; parentObject = parentObject->parentObject()) {
        if (parentObject->getScrollableAreaIfScrollable())
            objects.append(parentObject);
    }

    // Now, go back through that chain and make sure each inner object is within the
    // visible bounds of the outer object.
    size_t levels = objects.size() - 1;
    
    for (size_t i = levels; i >= 1; i--) {
        const AccessibilityObject* outer = objects[i];
        const AccessibilityObject* inner = objects[i - 1];
        // FIXME: unclear if we need LegacyIOSDocumentVisibleRect.
        const IntRect outerRect = i < levels ? snappedIntRect(outer->boundingBoxRect()) : outer->getScrollableAreaIfScrollable()->visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
        const IntRect innerRect = snappedIntRect(inner->isAccessibilityScrollView() ? inner->parentObject()->boundingBoxRect() : inner->boundingBoxRect());
        
        if (!outerRect.intersects(innerRect)) {
            isOnscreen = false;
            break;
        }
    }
    
    return isOnscreen;
}

void AccessibilityObject::scrollToMakeVisible() const
{
    scrollToMakeVisible({ SelectionRevealMode::Reveal, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, ShouldAllowCrossOriginScrolling::Yes });
}

void AccessibilityObject::scrollToMakeVisible(const ScrollRectToVisibleOptions& options) const
{
    if (isScrollView() && parentObject())
        parentObject()->scrollToMakeVisible();

    if (auto* renderer = this->renderer())
        renderer->scrollRectToVisible(boundingBoxRect(), false, options);
}

void AccessibilityObject::scrollToMakeVisibleWithSubFocus(const IntRect& subfocus) const
{
    // Search up the parent chain until we find the first one that's scrollable.
    AccessibilityObject* scrollParent = parentObject();
    ScrollableArea* scrollableArea;
    for (scrollableArea = nullptr;
         scrollParent && !(scrollableArea = scrollParent->getScrollableAreaIfScrollable());
         scrollParent = scrollParent->parentObject()) { }
    if (!scrollableArea)
        return;

    LayoutRect objectRect = boundingBoxRect();
    IntPoint scrollPosition = scrollableArea->scrollPosition();
    // FIXME: unclear if we need LegacyIOSDocumentVisibleRect.
    IntRect scrollVisibleRect = scrollableArea->visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);

    if (!scrollParent->isScrollView()) {
        objectRect.moveBy(scrollPosition);
        objectRect.moveBy(-snappedIntRect(scrollParent->elementRect()).location());
    }
    
    int desiredX = computeBestScrollOffset(
        scrollPosition.x(),
        objectRect.x() + subfocus.x(), objectRect.x() + subfocus.maxX(),
        objectRect.x(), objectRect.maxX(),
        0, scrollVisibleRect.width());
    int desiredY = computeBestScrollOffset(
        scrollPosition.y(),
        objectRect.y() + subfocus.y(), objectRect.y() + subfocus.maxY(),
        objectRect.y(), objectRect.maxY(),
        0, scrollVisibleRect.height());

    scrollParent->scrollTo(IntPoint(desiredX, desiredY));

    // Convert the subfocus into the coordinates of the scroll parent.
    IntRect newSubfocus = subfocus;
    IntRect newElementRect = snappedIntRect(elementRect());
    IntRect scrollParentRect = snappedIntRect(scrollParent->elementRect());
    newSubfocus.move(newElementRect.x(), newElementRect.y());
    newSubfocus.move(-scrollParentRect.x(), -scrollParentRect.y());
    
    // Recursively make sure the scroll parent itself is visible.
    if (scrollParent->parentObject())
        scrollParent->scrollToMakeVisibleWithSubFocus(newSubfocus);
}

void AccessibilityObject::scrollToGlobalPoint(const IntPoint& globalPoint) const
{
    // Search up the parent chain and create a vector of all scrollable parent objects
    // and ending with this object itself.
    Vector<const AccessibilityObject*> objects;

    objects.append(this);
    for (AccessibilityObject* parentObject = this->parentObject(); parentObject; parentObject = parentObject->parentObject()) {
        if (parentObject->getScrollableAreaIfScrollable())
            objects.append(parentObject);
    }

    objects.reverse();

    // Start with the outermost scrollable (the main window) and try to scroll the
    // next innermost object to the given point.
    int offsetX = 0, offsetY = 0;
    IntPoint point = globalPoint;
    size_t levels = objects.size() - 1;
    for (size_t i = 0; i < levels; i++) {
        const AccessibilityObject* outer = objects[i];
        const AccessibilityObject* inner = objects[i + 1];

        ScrollableArea* scrollableArea = outer->getScrollableAreaIfScrollable();

        LayoutRect innerRect = inner->isAccessibilityScrollView() ? inner->parentObject()->boundingBoxRect() : inner->boundingBoxRect();
        LayoutRect objectRect = innerRect;
        IntPoint scrollPosition = scrollableArea->scrollPosition();

        // Convert the object rect into local coordinates.
        objectRect.move(offsetX, offsetY);
        if (!outer->isAccessibilityScrollView())
            objectRect.move(scrollPosition.x(), scrollPosition.y());

        int desiredX = computeBestScrollOffset(
            0,
            objectRect.x(), objectRect.maxX(),
            objectRect.x(), objectRect.maxX(),
            point.x(), point.x());
        int desiredY = computeBestScrollOffset(
            0,
            objectRect.y(), objectRect.maxY(),
            objectRect.y(), objectRect.maxY(),
            point.y(), point.y());
        outer->scrollTo(IntPoint(desiredX, desiredY));

        if (outer->isAccessibilityScrollView() && !inner->isAccessibilityScrollView()) {
            // If outer object we just scrolled is a scroll view (main window or iframe) but the
            // inner object is not, keep track of the coordinate transformation to apply to
            // future nested calculations.
            scrollPosition = scrollableArea->scrollPosition();
            offsetX -= (scrollPosition.x() + point.x());
            offsetY -= (scrollPosition.y() + point.y());
            point.move(scrollPosition.x() - innerRect.x(),
                       scrollPosition.y() - innerRect.y());
        } else if (inner->isAccessibilityScrollView()) {
            // Otherwise, if the inner object is a scroll view, reset the coordinate transformation.
            offsetX = 0;
            offsetY = 0;
        }
    }
}
    
void AccessibilityObject::scrollAreaAndAncestor(std::pair<ScrollableArea*, AccessibilityObject*>& scrollers) const
{
    // Search up the parent chain until we find the first one that's scrollable.
    scrollers.first = nullptr;
    for (scrollers.second = parentObject(); scrollers.second; scrollers.second = scrollers.second->parentObject()) {
        if ((scrollers.first = scrollers.second->getScrollableAreaIfScrollable()))
            break;
    }
}
    
ScrollableArea* AccessibilityObject::scrollableAreaAncestor() const
{
    std::pair<ScrollableArea*, AccessibilityObject*> scrollers;
    scrollAreaAndAncestor(scrollers);
    return scrollers.first;
}
    
IntPoint AccessibilityObject::scrollPosition() const
{
    if (auto scroller = scrollableAreaAncestor())
        return scroller->scrollPosition();

    return IntPoint();
}

IntRect AccessibilityObject::scrollVisibleContentRect() const
{
    if (auto scroller = scrollableAreaAncestor())
        return scroller->visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
    
    return IntRect();
}
    
IntSize AccessibilityObject::scrollContentsSize() const
{
    if (auto scroller = scrollableAreaAncestor())
        return scroller->contentsSize();

    return IntSize();
}
    
bool AccessibilityObject::scrollByPage(ScrollByPageDirection direction) const
{
    std::pair<ScrollableArea*, AccessibilityObject*> scrollers;
    scrollAreaAndAncestor(scrollers);
    ScrollableArea* scrollableArea = scrollers.first;
    AccessibilityObject* scrollParent = scrollers.second;
    
    if (!scrollableArea)
        return false;
    
    IntPoint scrollPosition = scrollableArea->scrollPosition();
    IntPoint newScrollPosition = scrollPosition;
    IntSize scrollSize = scrollableArea->contentsSize();
    IntRect scrollVisibleRect = scrollableArea->visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
    switch (direction) {
    case ScrollByPageDirection::Right: {
        int scrollAmount = scrollVisibleRect.size().width();
        int newX = scrollPosition.x() - scrollAmount;
        newScrollPosition.setX(std::max(newX, 0));
        break;
    }
    case ScrollByPageDirection::Left: {
        int scrollAmount = scrollVisibleRect.size().width();
        int newX = scrollAmount + scrollPosition.x();
        int maxX = scrollSize.width() - scrollAmount;
        newScrollPosition.setX(std::min(newX, maxX));
        break;
    }
    case ScrollByPageDirection::Up: {
        int scrollAmount = scrollVisibleRect.size().height();
        int newY = scrollPosition.y() - scrollAmount;
        newScrollPosition.setY(std::max(newY, 0));
        break;
    }
    case ScrollByPageDirection::Down: {
        int scrollAmount = scrollVisibleRect.size().height();
        int newY = scrollAmount + scrollPosition.y();
        int maxY = scrollSize.height() - scrollAmount;
        newScrollPosition.setY(std::min(newY, maxY));
        break;
    }
    }
    
    if (newScrollPosition != scrollPosition) {
        scrollParent->scrollTo(newScrollPosition);
        document()->updateLayoutIgnorePendingStylesheets();
        return true;
    }
    
    return false;
}


bool AccessibilityObject::lastKnownIsIgnoredValue()
{
    if (m_lastKnownIsIgnoredValue == AccessibilityObjectInclusion::DefaultBehavior)
        m_lastKnownIsIgnoredValue = accessibilityIsIgnored() ? AccessibilityObjectInclusion::IgnoreObject : AccessibilityObjectInclusion::IncludeObject;

    return m_lastKnownIsIgnoredValue == AccessibilityObjectInclusion::IgnoreObject;
}

void AccessibilityObject::setLastKnownIsIgnoredValue(bool isIgnored)
{
    m_lastKnownIsIgnoredValue = isIgnored ? AccessibilityObjectInclusion::IgnoreObject : AccessibilityObjectInclusion::IncludeObject;
}

void AccessibilityObject::notifyIfIgnoredValueChanged()
{
    bool isIgnored = accessibilityIsIgnored();
    if (lastKnownIsIgnoredValue() != isIgnored) {
        if (AXObjectCache* cache = axObjectCache())
            cache->childrenChanged(parentObject());
        setLastKnownIsIgnoredValue(isIgnored);
    }
}

bool AccessibilityObject::pressedIsPresent() const
{
    return !getAttribute(aria_pressedAttr).isEmpty();
}

TextIteratorBehavior AccessibilityObject::textIteratorBehaviorForTextRange() const
{
    TextIteratorBehavior behavior = TextIteratorIgnoresStyleVisibility;
    
#if USE(ATK)
    // We need to emit replaced elements for GTK, and present
    // them with the 'object replacement character' (0xFFFC).
    behavior = static_cast<TextIteratorBehavior>(behavior | TextIteratorEmitsObjectReplacementCharacters);
#endif
    
    return behavior;
}
    
AccessibilityRole AccessibilityObject::buttonRoleType() const
{
    // If aria-pressed is present, then it should be exposed as a toggle button.
    // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
    if (pressedIsPresent())
        return AccessibilityRole::ToggleButton;
    if (hasPopup())
        return AccessibilityRole::PopUpButton;
    // We don't contemplate AccessibilityRole::RadioButton, as it depends on the input
    // type.

    return AccessibilityRole::Button;
}

bool AccessibilityObject::isButton() const
{
    AccessibilityRole role = roleValue();

    return role == AccessibilityRole::Button || role == AccessibilityRole::PopUpButton || role == AccessibilityRole::ToggleButton;
}

bool AccessibilityObject::accessibilityIsIgnoredByDefault() const
{
    return defaultObjectInclusion() == AccessibilityObjectInclusion::IgnoreObject;
}

// ARIA component of hidden definition.
// http://www.w3.org/TR/wai-aria/terms#def_hidden
bool AccessibilityObject::isAXHidden() const
{
    return AccessibilityObject::matchedParent(*this, true, [] (const AccessibilityObject& object) {
        return equalLettersIgnoringASCIICase(object.getAttribute(aria_hiddenAttr), "true");
    }) != nullptr;
}

// DOM component of hidden definition.
// http://www.w3.org/TR/wai-aria/terms#def_hidden
bool AccessibilityObject::isDOMHidden() const
{
    RenderObject* renderer = this->renderer();
    if (!renderer)
        return true;
    
    const RenderStyle& style = renderer->style();
    return style.display() == DisplayType::None || style.visibility() != Visibility::Visible;
}

bool AccessibilityObject::isShowingValidationMessage() const
{
    if (is<HTMLFormControlElement>(node()))
        return downcast<HTMLFormControlElement>(*node()).isShowingValidationMessage();
    return false;
}

String AccessibilityObject::validationMessage() const
{
    if (is<HTMLFormControlElement>(node()))
        return downcast<HTMLFormControlElement>(*node()).validationMessage();
    return String();
}

AccessibilityObjectInclusion AccessibilityObject::defaultObjectInclusion() const
{
    bool useParentData = !m_isIgnoredFromParentData.isNull();
    
    if (useParentData ? m_isIgnoredFromParentData.isAXHidden : isAXHidden())
        return AccessibilityObjectInclusion::IgnoreObject;
    
    if (ignoredFromModalPresence())
        return AccessibilityObjectInclusion::IgnoreObject;
    
    if (useParentData ? m_isIgnoredFromParentData.isPresentationalChildOfAriaRole : isPresentationalChildOfAriaRole())
        return AccessibilityObjectInclusion::IgnoreObject;
    
    return accessibilityPlatformIncludesObject();
}
    
bool AccessibilityObject::accessibilityIsIgnored() const
{
    AXComputedObjectAttributeCache* attributeCache = nullptr;
    AXObjectCache* cache = axObjectCache();
    if (cache)
        attributeCache = cache->computedObjectAttributeCache();
    
    if (attributeCache) {
        AccessibilityObjectInclusion ignored = attributeCache->getIgnored(axObjectID());
        switch (ignored) {
        case AccessibilityObjectInclusion::IgnoreObject:
            return true;
        case AccessibilityObjectInclusion::IncludeObject:
            return false;
        case AccessibilityObjectInclusion::DefaultBehavior:
            break;
        }
    }

    bool result = computeAccessibilityIsIgnored();

    // In case computing axIsIgnored disables attribute caching, we should refetch the object to see if it exists.
    if (cache && (attributeCache = cache->computedObjectAttributeCache()))
        attributeCache->setIgnored(axObjectID(), result ? AccessibilityObjectInclusion::IgnoreObject : AccessibilityObjectInclusion::IncludeObject);

    return result;
}

void AccessibilityObject::elementsFromAttribute(Vector<Element*>& elements, const QualifiedName& attribute) const
{
    Node* node = this->node();
    if (!node || !node->isElementNode())
        return;

    TreeScope& treeScope = node->treeScope();

    const AtomString& idList = getAttribute(attribute);
    if (idList.isEmpty())
        return;

    auto spaceSplitString = SpaceSplitString(idList, false);
    size_t length = spaceSplitString.size();
    for (size_t i = 0; i < length; ++i) {
        if (auto* idElement = treeScope.getElementById(spaceSplitString[i]))
            elements.append(idElement);
    }
}

#if PLATFORM(COCOA)
bool AccessibilityObject::preventKeyboardDOMEventDispatch() const
{
    Frame* frame = this->frame();
    return frame && frame->settings().preventKeyboardDOMEventDispatch();
}

void AccessibilityObject::setPreventKeyboardDOMEventDispatch(bool on)
{
    Frame* frame = this->frame();
    if (!frame)
        return;
    frame->settings().setPreventKeyboardDOMEventDispatch(on);
}
#endif

AccessibilityObject* AccessibilityObject::focusableAncestor()
{
    return const_cast<AccessibilityObject*>(AccessibilityObject::matchedParent(*this, true, [] (const AccessibilityObject& object) {
        return object.canSetFocusAttribute();
    }));
}

AccessibilityObject* AccessibilityObject::editableAncestor()
{
    return const_cast<AccessibilityObject*>(AccessibilityObject::matchedParent(*this, true, [] (const AccessibilityObject& object) {
        return object.isTextControl();
    }));
}

AccessibilityObject* AccessibilityObject::highestEditableAncestor()
{
    AccessibilityObject* editableAncestor = this->editableAncestor();
    AccessibilityObject* previousEditableAncestor = nullptr;
    while (editableAncestor) {
        if (editableAncestor == previousEditableAncestor) {
            if (AccessibilityObject* parent = editableAncestor->parentObject()) {
                editableAncestor = parent->editableAncestor();
                continue;
            }
            break;
        }
        previousEditableAncestor = editableAncestor;
        editableAncestor = editableAncestor->editableAncestor();
    }
    return previousEditableAncestor;
}

AccessibilityObject* AccessibilityObject::radioGroupAncestor() const
{
    return const_cast<AccessibilityObject*>(AccessibilityObject::matchedParent(*this, false, [] (const AccessibilityObject& object) {
        return object.isRadioGroup();
    }));
}

bool AccessibilityObject::isStyleFormatGroup() const
{
    Node* node = this->node();
    if (!node)
        return false;
    
    return node->hasTagName(kbdTag) || node->hasTagName(codeTag)
    || node->hasTagName(preTag) || node->hasTagName(sampTag)
    || node->hasTagName(varTag) || node->hasTagName(citeTag)
    || node->hasTagName(insTag) || node->hasTagName(delTag)
    || node->hasTagName(supTag) || node->hasTagName(subTag);
}

bool AccessibilityObject::isFigureElement() const
{
    Node* node = this->node();
    return node && node->hasTagName(figureTag);
}

bool AccessibilityObject::isKeyboardFocusable() const
{
    if (auto element = this->element())
        return element->isFocusable();
    return false;
}

bool AccessibilityObject::isOutput() const
{
    Node* node = this->node();
    return node && node->hasTagName(outputTag);
}
    
bool AccessibilityObject::isContainedByPasswordField() const
{
    Node* node = this->node();
    if (!node)
        return false;
    
    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    Element* element = node->shadowHost();
    return is<HTMLInputElement>(element) && downcast<HTMLInputElement>(*element).isPasswordField();
}
    
AccessibilityObject* AccessibilityObject::selectedListItem()
{
    for (const auto& child : children()) {
        if (child->isListItem() && (child->isSelected() || child->isActiveDescendantOfFocusedContainer()))
            return child.get();
    }
    
    return nullptr;
}

void AccessibilityObject::ariaElementsFromAttribute(AccessibilityChildrenVector& children, const QualifiedName& attributeName) const
{
    Vector<Element*> elements;
    elementsFromAttribute(elements, attributeName);
    AXObjectCache* cache = axObjectCache();
    for (const auto& element : elements) {
        if (AccessibilityObject* axObject = cache->getOrCreate(element))
            children.append(axObject);
    }
}

void AccessibilityObject::ariaElementsReferencedByAttribute(AccessibilityChildrenVector& elements, const QualifiedName& attribute) const
{
    auto id = identifierAttribute();
    if (id.isEmpty())
        return;

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return;

    for (auto& element : descendantsOfType<Element>(node()->treeScope().rootNode())) {
        const AtomString& idList = element.attributeWithoutSynchronization(attribute);
        if (!SpaceSplitString(idList, false).contains(id))
            continue;

        if (AccessibilityObject* axObject = cache->getOrCreate(&element))
            elements.append(axObject);
    }
}

bool AccessibilityObject::isActiveDescendantOfFocusedContainer() const
{
    AccessibilityChildrenVector containers;
    ariaActiveDescendantReferencingElements(containers);
    for (auto& container : containers) {
        if (container->isFocused())
            return true;
    }

    return false;
}

void AccessibilityObject::ariaActiveDescendantReferencingElements(AccessibilityChildrenVector& containers) const
{
    ariaElementsReferencedByAttribute(containers, aria_activedescendantAttr);
}

void AccessibilityObject::ariaControlsElements(AccessibilityChildrenVector& ariaControls) const
{
    ariaElementsFromAttribute(ariaControls, aria_controlsAttr);
}

void AccessibilityObject::ariaControlsReferencingElements(AccessibilityChildrenVector& controllers) const
{
    ariaElementsReferencedByAttribute(controllers, aria_controlsAttr);
}

void AccessibilityObject::ariaDescribedByElements(AccessibilityChildrenVector& ariaDescribedBy) const
{
    ariaElementsFromAttribute(ariaDescribedBy, aria_describedbyAttr);
}

void AccessibilityObject::ariaDescribedByReferencingElements(AccessibilityChildrenVector& describers) const
{
    ariaElementsReferencedByAttribute(describers, aria_describedbyAttr);
}

void AccessibilityObject::ariaDetailsElements(AccessibilityChildrenVector& ariaDetails) const
{
    ariaElementsFromAttribute(ariaDetails, aria_detailsAttr);
}

void AccessibilityObject::ariaDetailsReferencingElements(AccessibilityChildrenVector& detailsFor) const
{
    ariaElementsReferencedByAttribute(detailsFor, aria_detailsAttr);
}

void AccessibilityObject::ariaErrorMessageElements(AccessibilityChildrenVector& ariaErrorMessage) const
{
    ariaElementsFromAttribute(ariaErrorMessage, aria_errormessageAttr);
}

void AccessibilityObject::ariaErrorMessageReferencingElements(AccessibilityChildrenVector& errorMessageFor) const
{
    ariaElementsReferencedByAttribute(errorMessageFor, aria_errormessageAttr);
}

void AccessibilityObject::ariaFlowToElements(AccessibilityChildrenVector& flowTo) const
{
    ariaElementsFromAttribute(flowTo, aria_flowtoAttr);
}

void AccessibilityObject::ariaFlowToReferencingElements(AccessibilityChildrenVector& flowFrom) const
{
    ariaElementsReferencedByAttribute(flowFrom, aria_flowtoAttr);
}

void AccessibilityObject::ariaLabelledByElements(AccessibilityChildrenVector& ariaLabelledBy) const
{
    ariaElementsFromAttribute(ariaLabelledBy, aria_labelledbyAttr);
    if (!ariaLabelledBy.size())
        ariaElementsFromAttribute(ariaLabelledBy, aria_labeledbyAttr);
}

void AccessibilityObject::ariaLabelledByReferencingElements(AccessibilityChildrenVector& labels) const
{
    ariaElementsReferencedByAttribute(labels, aria_labelledbyAttr);
    if (!labels.size())
        ariaElementsReferencedByAttribute(labels, aria_labeledbyAttr);
}

void AccessibilityObject::ariaOwnsElements(AccessibilityChildrenVector& axObjects) const
{
    ariaElementsFromAttribute(axObjects, aria_ownsAttr);
}

void AccessibilityObject::ariaOwnsReferencingElements(AccessibilityChildrenVector& owners) const
{
    ariaElementsReferencedByAttribute(owners, aria_ownsAttr);
}

void AccessibilityObject::setIsIgnoredFromParentDataForChild(AccessibilityObject* child)
{
    if (!child)
        return;
    
    if (child->parentObject() != this) {
        child->clearIsIgnoredFromParentData();
        return;
    }
    
    AccessibilityIsIgnoredFromParentData result = AccessibilityIsIgnoredFromParentData(this);
    if (!m_isIgnoredFromParentData.isNull()) {
        result.isAXHidden = m_isIgnoredFromParentData.isAXHidden || equalLettersIgnoringASCIICase(child->getAttribute(aria_hiddenAttr), "true");
        result.isPresentationalChildOfAriaRole = m_isIgnoredFromParentData.isPresentationalChildOfAriaRole || ariaRoleHasPresentationalChildren();
        result.isDescendantOfBarrenParent = m_isIgnoredFromParentData.isDescendantOfBarrenParent || !canHaveChildren();
    } else {
        result.isAXHidden = child->isAXHidden();
        result.isPresentationalChildOfAriaRole = child->isPresentationalChildOfAriaRole();
        result.isDescendantOfBarrenParent = child->isDescendantOfBarrenParent();
    }
    
    child->setIsIgnoredFromParentData(result);
}

} // namespace WebCore
