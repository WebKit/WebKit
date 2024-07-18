/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#include "AXLogger.h"
#include "AXObjectCache.h"
#include "AXTextMarker.h"
#include "AccessibilityMockObject.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilityScrollView.h"
#include "AccessibilityTable.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CustomElementDefaultARIA.h"
#include "DOMTokenList.h"
#include "DocumentInlines.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "GeometryUtilities.h"
#include "HTMLBodyElement.h"
#include "HTMLDataListElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLModelElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "MathMLNames.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderImage.h"
#include "RenderInline.h"
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
#include "RuntimeApplicationChecks.h"
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

void AccessibilityObject::init()
{
    m_role = determineAccessibilityRole();
}

AXID AccessibilityObject::treeID() const
{
    auto* cache = axObjectCache();
    return cache ? cache->treeID() : AXID();
}

inline ProcessID AccessibilityObject::processID() const
{
    return presentingApplicationPID();
}

String AccessibilityObject::dbg() const
{
    String backingEntityDescription;
    if (auto* renderer = this->renderer())
        backingEntityDescription = makeString(", ", renderer->debugDescription());
    else if (auto* node = this->node())
        backingEntityDescription = makeString(", ", node->debugDescription());

    return makeString(
        "{role: ", accessibilityRoleToString(roleValue()),
        ", ID ", objectID().loggingString(),
        backingEntityDescription, "}"
    );
}

void AccessibilityObject::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    // Menu close events need to notify the platform. No element is used in the notification because it's a destruction event.
    if (detachmentType == AccessibilityDetachmentType::ElementDestroyed && roleValue() == AccessibilityRole::Menu) {
        if (auto* cache = axObjectCache())
            cache->postNotification(nullptr, &cache->document(), AXObjectCache::AXMenuClosed);
    }

    // Clear any children and call detachFromParent on them so that
    // no children are left with dangling pointers to their parent.
    clearChildren();
}

bool AccessibilityObject::isDetached() const
{
    return !wrapper();
}

OptionSet<AXAncestorFlag> AccessibilityObject::computeAncestorFlags() const
{
    OptionSet<AXAncestorFlag> computedFlags;

    if (hasAncestorFlag(AXAncestorFlag::HasDocumentRoleAncestor) || matchesAncestorFlag(AXAncestorFlag::HasDocumentRoleAncestor))
        computedFlags.set(AXAncestorFlag::HasDocumentRoleAncestor, 1);

    if (hasAncestorFlag(AXAncestorFlag::HasWebApplicationAncestor) || matchesAncestorFlag(AXAncestorFlag::HasWebApplicationAncestor))
        computedFlags.set(AXAncestorFlag::HasWebApplicationAncestor, 1);

    if (hasAncestorFlag(AXAncestorFlag::IsInDescriptionListDetail) || matchesAncestorFlag(AXAncestorFlag::IsInDescriptionListDetail))
        computedFlags.set(AXAncestorFlag::IsInDescriptionListDetail, 1);

    if (hasAncestorFlag(AXAncestorFlag::IsInDescriptionListTerm) || matchesAncestorFlag(AXAncestorFlag::IsInDescriptionListTerm))
        computedFlags.set(AXAncestorFlag::IsInDescriptionListTerm, 1);

    if (hasAncestorFlag(AXAncestorFlag::IsInCell) || matchesAncestorFlag(AXAncestorFlag::IsInCell))
        computedFlags.set(AXAncestorFlag::IsInCell, 1);

    if (hasAncestorFlag(AXAncestorFlag::IsInRow) || matchesAncestorFlag(AXAncestorFlag::IsInRow))
        computedFlags.set(AXAncestorFlag::IsInRow, 1);

    return computedFlags;
}

OptionSet<AXAncestorFlag> AccessibilityObject::computeAncestorFlagsWithTraversal() const
{
    // If this object's flags are initialized, this traversal is unnecessary. Use AccessibilityObject::ancestorFlags() instead.
    ASSERT(!ancestorFlagsAreInitialized());

    OptionSet<AXAncestorFlag> computedFlags;
    computedFlags.set(AXAncestorFlag::FlagsInitialized, true);
    Accessibility::enumerateAncestors<AccessibilityObject>(*this, false, [&] (const AccessibilityObject& ancestor) {
        computedFlags.add(ancestor.computeAncestorFlags());
    });
    return computedFlags;
}

void AccessibilityObject::initializeAncestorFlags(const OptionSet<AXAncestorFlag>& flags)
{
    m_ancestorFlags.set(AXAncestorFlag::FlagsInitialized, true);
    m_ancestorFlags.add(flags);
}

bool AccessibilityObject::matchesAncestorFlag(AXAncestorFlag flag) const
{
    auto role = roleValue();
    switch (flag) {
    case AXAncestorFlag::HasDocumentRoleAncestor:
        return role == AccessibilityRole::Document || role == AccessibilityRole::GraphicsDocument;
    case AXAncestorFlag::HasWebApplicationAncestor:
        return role == AccessibilityRole::WebApplication;
    case AXAncestorFlag::IsInDescriptionListDetail:
        return role == AccessibilityRole::DescriptionListDetail;
    case AXAncestorFlag::IsInDescriptionListTerm:
        return role == AccessibilityRole::DescriptionListTerm;
    case AXAncestorFlag::IsInCell:
        return role == AccessibilityRole::Cell;
    case AXAncestorFlag::IsInRow:
        return role == AccessibilityRole::Row;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool AccessibilityObject::hasAncestorMatchingFlag(AXAncestorFlag flag) const
{
    return Accessibility::findAncestor<AccessibilityObject>(*this, false, [flag] (const AccessibilityObject& object) {
        if (object.ancestorFlagsAreInitialized())
            return object.ancestorFlags().contains(flag);

        return object.matchesAncestorFlag(flag);
    }) != nullptr;
}

bool AccessibilityObject::hasDocumentRoleAncestor() const
{
    if (ancestorFlagsAreInitialized())
        return m_ancestorFlags.contains(AXAncestorFlag::HasDocumentRoleAncestor);

    return hasAncestorMatchingFlag(AXAncestorFlag::HasDocumentRoleAncestor);
}

bool AccessibilityObject::hasWebApplicationAncestor() const
{
    if (ancestorFlagsAreInitialized())
        return m_ancestorFlags.contains(AXAncestorFlag::HasWebApplicationAncestor);

    return hasAncestorMatchingFlag(AXAncestorFlag::HasWebApplicationAncestor);
}

bool AccessibilityObject::isInDescriptionListDetail() const
{
    if (ancestorFlagsAreInitialized())
        return m_ancestorFlags.contains(AXAncestorFlag::IsInDescriptionListDetail);

    return hasAncestorMatchingFlag(AXAncestorFlag::IsInDescriptionListDetail);
}

bool AccessibilityObject::isInDescriptionListTerm() const
{
    if (ancestorFlagsAreInitialized())
        return m_ancestorFlags.contains(AXAncestorFlag::IsInDescriptionListTerm);

    return hasAncestorMatchingFlag(AXAncestorFlag::IsInDescriptionListTerm);
}

bool AccessibilityObject::isInCell() const
{
    if (ancestorFlagsAreInitialized())
        return m_ancestorFlags.contains(AXAncestorFlag::IsInCell);

    return hasAncestorMatchingFlag(AXAncestorFlag::IsInCell);
}

bool AccessibilityObject::isInRow() const
{
    if (ancestorFlagsAreInitialized())
        return m_ancestorFlags.contains(AXAncestorFlag::IsInRow);

    return hasAncestorMatchingFlag(AXAncestorFlag::IsInRow);
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
    return { };
}

bool AccessibilityObject::isARIATextControl() const
{
    return ariaRoleAttribute() == AccessibilityRole::TextArea || ariaRoleAttribute() == AccessibilityRole::TextField || ariaRoleAttribute() == AccessibilityRole::SearchField;
}

bool AccessibilityObject::isNonNativeTextControl() const
{
    return (hasContentEditableAttributeSet() || isARIATextControl()) && !isNativeTextControl();
}

bool AccessibilityObject::hasMisspelling() const
{
    if (!node())
        return false;
    
    auto* frame = node()->document().frame();
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

std::optional<SimpleRange> AccessibilityObject::misspellingRange(const SimpleRange& start, AccessibilitySearchDirection direction) const
{
    auto node = this->node();
    if (!node)
        return std::nullopt;

    auto* frame = node->document().frame();
    if (!frame)
        return std::nullopt;

    if (!unifiedTextCheckerEnabled(frame))
        return std::nullopt;

    Editor& editor = frame->editor();

    TextCheckerClient* textChecker = editor.textChecker();
    if (!textChecker)
        return std::nullopt;

    Vector<TextCheckingResult> misspellings;
    checkTextOfParagraph(*textChecker, stringValue(), TextCheckingType::Spelling, misspellings, frame->selection().selection());

    // Find the first misspelling past the start.
    if (direction == AccessibilitySearchDirection::Next) {
        for (auto& misspelling : misspellings) {
            auto misspellingRange = editor.rangeForTextCheckingResult(misspelling);
            if (misspellingRange && is_gt(treeOrder<ComposedTree>(misspellingRange->end, start.end)))
                return *misspellingRange;
        }
    } else {
        for (auto& misspelling : makeReversedRange(misspellings)) {
            auto misspellingRange = editor.rangeForTextCheckingResult(misspelling);
            if (misspellingRange && is_lt(treeOrder<ComposedTree>(misspellingRange->start, start.start)))
                return *misspellingRange;
        }
    }

    return std::nullopt;
}

AXTextMarkerRange AccessibilityObject::textInputMarkedTextMarkerRange() const
{
    WeakPtr node = this->node();
    if (!node)
        return { };

    auto* frame = node->document().frame();
    if (!frame)
        return { };

    auto* cache = axObjectCache();
    if (!cache)
        return { };

    auto& editor = frame->editor();
    auto* object = cache->getOrCreate(editor.compositionNode());
    if (!object)
        return { };

    if (auto* observableObject = object->observableObject())
        object = observableObject;

    if (object->objectID() != objectID())
        return { };

    return { editor.compositionRange() };
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
    return Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const AccessibilityObject& object) {
        return !object.accessibilityIsIgnored();
    });
}

AccessibilityObject* AccessibilityObject::displayContentsParent() const
{
    auto* parentNode = node() ? node()->parentNode() : nullptr;
    if (RefPtr element = dynamicDowncast<Element>(parentNode); !element || !element->hasDisplayContents())
        return nullptr;

    auto* cache = axObjectCache();
    return cache ? cache->getOrCreate(parentNode) : nullptr;
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

AccessibilityObject* firstAccessibleObjectFromNode(const Node* node, const Function<bool(const AccessibilityObject&)>& isAccessible)
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

// FIXME: Usages of this function should be replaced by a new flag in AccessibilityObject::m_ancestorFlags.
bool AccessibilityObject::isDescendantOfRole(AccessibilityRole role) const
{
    return Accessibility::findAncestor<AccessibilityObject>(*this, false, [&role] (const AccessibilityObject& object) {
        return object.roleValue() == role;
    }) != nullptr;
}

static void appendAccessibilityObject(RefPtr<AXCoreObject> object, AccessibilityObject::AccessibilityChildrenVector& results)
{
    // Find the next descendant of this attachment object so search can continue through frames.
    if (object->isAttachment()) {
        Widget* widget = object->widgetForAttachmentView();
        auto* frameView = dynamicDowncast<LocalFrameView>(widget);
        if (!frameView)
            return;
        auto* document = frameView->frame().document();
        if (!document || !document->hasLivingRenderTree())
            return;
        
        object = object->axObjectCache()->getOrCreate(document);
    }

    if (object)
        results.append(object);
}

#if ASSERT_ENABLED
static bool isTableComponent(AXCoreObject& axObject)
{
    return axObject.isTable() || axObject.isTableColumn() || axObject.isTableRow() || axObject.isTableCell();
}
#endif

void AccessibilityObject::insertChild(AXCoreObject* newChild, unsigned index, DescendIfIgnored descendIfIgnored)
{
    if (!newChild)
        return;

    auto* child = dynamicDowncast<AccessibilityObject>(*newChild);
    ASSERT(child);
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

    auto* displayContentsParent = child->displayContentsParent();
    // To avoid double-inserting a child of a `display: contents` element, only insert if `this` is the rightful parent.
    if (displayContentsParent && displayContentsParent != this) {
        // Make sure the display:contents parent object knows it has a child it needs to add.
        displayContentsParent->setNeedsToUpdateChildren();
        // Don't exit early for certain table components, as they rely on inserting children for which they are not the rightful parent to behave correctly.
        if (!isTableColumn() && roleValue() != AccessibilityRole::TableHeaderContainer)
            return;
    }

    auto thisAncestorFlags = computeAncestorFlags();
    child->initializeAncestorFlags(thisAncestorFlags);
    setIsIgnoredFromParentDataForChild(child);
    if (child->accessibilityIsIgnored()) {
        if (descendIfIgnored == DescendIfIgnored::Yes) {
            unsigned insertionIndex = index;
            auto childAncestorFlags = child->computeAncestorFlags();
            for (auto grandchildCoreObject : child->children()) {
                if (grandchildCoreObject) {
                    ASSERT(is<AccessibilityObject>(*grandchildCoreObject));
                    RefPtr grandchild = dynamicDowncast<AccessibilityObject>(*grandchildCoreObject);
                    if (!grandchild)
                        continue;

                    // Even though `child` is ignored, we still need to set ancestry flags based on it.
                    grandchild->initializeAncestorFlags(childAncestorFlags);
                    grandchild->addAncestorFlags(thisAncestorFlags);
                    // Calls to `child->accessibilityIsIgnored()` or `child->children()` can cause layout, which in turn can cause this object to clear its m_children. This can cause `insertionIndex` to no longer be valid. Detect this and break early if necessary.
                    if (insertionIndex > m_children.size())
                        break;
                    m_children.insert(insertionIndex, grandchild);
                    ++insertionIndex;
                }
            }
        }
    } else {
        // Table component child-parent relationships often don't line up properly, hence the need for methods
        // like parentTable() and parentRow(). Exclude them from this ASSERT.
        ASSERT(isTableComponent(*child) || isTableComponent(*this) || child->parentObject() == this);
        m_children.insert(index, child);
    }
    
    // Reset the child's m_isIgnoredFromParentData since we are done adding that child and its children.
    child->clearIsIgnoredFromParentData();
}
    
void AccessibilityObject::addChild(AXCoreObject* child, DescendIfIgnored descendIfIgnored)
{
    insertChild(child, m_children.size(), descendIfIgnored);
}
    
void AccessibilityObject::findMatchingObjects(AccessibilitySearchCriteria* criteria, AccessibilityChildrenVector& results)
{
    ASSERT(criteria);
    if (!criteria)
        return;

    if (AXObjectCache* cache = axObjectCache())
        cache->startCachingComputedObjectAttributesUntilTreeMutates();

    criteria->anchorObject = this;
    Accessibility::findMatchingObjects(*criteria, results);
}

// Returns the range that is fewer positions away from the reference range.
// NOTE: The after range is expected to ACTUALLY be after the reference range and the before
// range is expected to ACTUALLY be before. These are not checked for performance reasons.
static std::optional<SimpleRange> rangeClosestToRange(const SimpleRange& referenceRange, std::optional<SimpleRange>&& afterRange, std::optional<SimpleRange>&& beforeRange)
{
    if (!beforeRange)
        return WTFMove(afterRange);
    if (!afterRange)
        return WTFMove(beforeRange);
    auto distanceBefore = characterCount({ beforeRange->end, referenceRange.start });
    auto distanceAfter = characterCount({ afterRange->start, referenceRange.end });
    return WTFMove(distanceBefore <= distanceAfter ? beforeRange : afterRange);
}

std::optional<SimpleRange> AccessibilityObject::rangeOfStringClosestToRangeInDirection(const SimpleRange& referenceRange, AccessibilitySearchDirection searchDirection, const Vector<String>& searchStrings) const
{
    auto* frame = this->frame();
    if (!frame)
        return std::nullopt;
    
    bool isBackwardSearch = searchDirection == AccessibilitySearchDirection::Previous;
    FindOptions findOptions { AtWordStarts, AtWordEnds, CaseInsensitive, StartInSelection };
    if (isBackwardSearch)
        findOptions.add(FindOptionFlag::Backwards);
    
    std::optional<SimpleRange> closestStringRange;
    for (auto& searchString : searchStrings) {
        if (auto foundStringRange = frame->editor().rangeOfString(searchString, referenceRange, findOptions)) {
            bool foundStringIsCloser;
            if (!closestStringRange)
                foundStringIsCloser = true;
            else {
                foundStringIsCloser = isBackwardSearch
                    ? is_gt(treeOrder<ComposedTree>(foundStringRange->end, closestStringRange->end))
                    : is_lt(treeOrder<ComposedTree>(foundStringRange->start, closestStringRange->start));
            }
            if (foundStringIsCloser)
                closestStringRange = *foundStringRange;
        }
    }
    return closestStringRange;
}

VisibleSelection AccessibilityObject::selection() const
{
    auto* document = this->document();
    auto* frame = document ? document->frame() : nullptr;
    return frame ? frame->selection().selection() : VisibleSelection();
}

// Returns an collapsed range preceding the document contents if there is no selection.
// FIXME: Why is that behavior more useful than returning null in that case?
std::optional<SimpleRange> AccessibilityObject::selectionRange() const
{
    auto frame = this->frame();
    if (!frame)
        return std::nullopt;

    if (auto range = frame->selection().selection().firstRange())
        return *range;

    auto& document = *frame->document();
    return { { { document, 0 }, { document, 0 } } };
}

std::optional<SimpleRange> AccessibilityObject::simpleRange() const
{
    auto* node = this->node();
    if (!node)
        return std::nullopt;
    return AXObjectCache::rangeForNodeContents(*node);
}

AXTextMarkerRange AccessibilityObject::textMarkerRange() const
{
    return simpleRange();
}

Vector<BoundaryPoint> AccessibilityObject::previousLineStartBoundaryPoints(const VisiblePosition& startingPosition, const SimpleRange& targetRange, unsigned positionsToRetrieve) const
{
    Vector<BoundaryPoint> boundaryPoints;
    boundaryPoints.reserveInitialCapacity(positionsToRetrieve);

    std::optional<VisiblePosition> lastPosition = startingPosition;
    for (unsigned i = 0; i < positionsToRetrieve; i++) {
        lastPosition = previousLineStartPositionInternal(*lastPosition);
        if (!lastPosition)
            break;

        auto boundaryPoint = makeBoundaryPoint(*lastPosition);
        if (!boundaryPoint || !contains(targetRange, *boundaryPoint))
            break;

        boundaryPoints.append(WTFMove(*boundaryPoint));
    }
    boundaryPoints.shrinkToFit();
    return boundaryPoints;
}

std::optional<BoundaryPoint> AccessibilityObject::lastBoundaryPointContainedInRect(const Vector<BoundaryPoint>& boundaryPoints, const BoundaryPoint& startBoundary, const FloatRect& rect, int leftIndex, int rightIndex) const
{
    if (leftIndex > rightIndex || boundaryPoints.isEmpty())
        return std::nullopt;

    auto indexIsValid = [&] (int index) {
        return index >= 0 && static_cast<size_t>(index) < boundaryPoints.size();
    };
    auto boundaryPointContainedInRect = [&] (const BoundaryPoint& boundary) {
        return boundaryPointsContainedInRect(startBoundary, boundary, rect);
    };

    int midIndex = leftIndex + (rightIndex - leftIndex) / 2;
    if (boundaryPointContainedInRect(boundaryPoints.at(midIndex))) {
        // We have a match if `midIndex` boundary point is contained in the rect, but the one at `midIndex - 1` isn't.
        if (indexIsValid(midIndex - 1) && !boundaryPointContainedInRect(boundaryPoints.at(midIndex - 1)))
            return boundaryPoints.at(midIndex);

        return lastBoundaryPointContainedInRect(boundaryPoints, startBoundary, rect, leftIndex, midIndex - 1);
    }
    // And vice versa, we have a match if the `midIndex` boundary point is not contained in the rect, but the one at `midIndex + 1` is.
    if (indexIsValid(midIndex + 1) && boundaryPointContainedInRect(boundaryPoints.at(midIndex + 1)))
        return boundaryPoints.at(midIndex + 1);

    return lastBoundaryPointContainedInRect(boundaryPoints, startBoundary, rect, midIndex + 1, rightIndex);
}

bool AccessibilityObject::boundaryPointsContainedInRect(const BoundaryPoint& startBoundary, const BoundaryPoint& endBoundary, const FloatRect& rect) const
{
    auto elementRect = boundsForRange({ startBoundary, endBoundary });
    return rect.contains(elementRect.location() + elementRect.size());
}

std::optional<SimpleRange> AccessibilityObject::visibleCharacterRange() const
{
    auto range = simpleRange();
    auto contentRect = unobscuredContentRect();
    auto elementRect = snappedIntRect(this->elementRect());
    auto inputs = std::make_tuple(range, contentRect, elementRect);
    if (m_cachedVisibleCharacterRangeInputs && *m_cachedVisibleCharacterRangeInputs == inputs)
        return m_cachedVisibleCharacterRange;

    auto computedRange = visibleCharacterRangeInternal(range, contentRect, elementRect);
    m_cachedVisibleCharacterRangeInputs = inputs;
    m_cachedVisibleCharacterRange = computedRange;
    return computedRange;
}

std::optional<SimpleRange> AccessibilityObject::visibleCharacterRangeInternal(const std::optional<SimpleRange>& range, const FloatRect& contentRect, const IntRect& startingElementRect) const
{
    if (!range || !contentRect.intersects(startingElementRect))
        return std::nullopt;

    auto elementRect = startingElementRect;
    auto startBoundary = range->start;
    auto endBoundary = range->end;

    // Origin isn't contained in visible rect, start moving forward by line.
    while (!contentRect.contains(elementRect.location())) {
        auto nextLinePosition = nextLineEndPosition(VisiblePosition(makeContainerOffsetPosition(startBoundary)));
        auto testStartBoundary = makeBoundaryPoint(nextLinePosition);
        if (!testStartBoundary || !contains(*range, *testStartBoundary))
            break;

        // testStartBoundary is valid, so commit it and update the elementRect.
        startBoundary = *testStartBoundary;
        elementRect = boundsForRange(SimpleRange(startBoundary, range->end));
        if (elementRect.isEmpty() || elementRect.x() < 0 || elementRect.y() < 0)
            break;
    }

    bool didCorrectStartBoundary = false;
    // Sometimes we shrink one line too far -- check the previous line start to see if it's in bounds.
    auto previousLineStartPosition = previousLineStartPositionInternal(VisiblePosition(makeContainerOffsetPosition(startBoundary)));
    if (previousLineStartPosition) {
        if (auto previousLineStartBoundaryPoint = makeBoundaryPoint(*previousLineStartPosition)) {
            auto lineStartRect = boundsForRange(SimpleRange(*previousLineStartBoundaryPoint, range->end));
            if (previousLineStartBoundaryPoint->container.ptr() == startBoundary.container.ptr() && contentRect.contains(lineStartRect.location())) {
                elementRect = lineStartRect;
                startBoundary = *previousLineStartBoundaryPoint;
                didCorrectStartBoundary = true;
            }
        }
    }

    if (!didCorrectStartBoundary) {
        // We iterated to a line-end position above. We must also check if the start of this line is in bounds.
        auto startBoundaryLineStartPosition = startOfLine(VisiblePosition(makeContainerOffsetPosition(startBoundary)));
        auto lineStartBoundaryPoint = makeBoundaryPoint(startBoundaryLineStartPosition);
        if (lineStartBoundaryPoint && lineStartBoundaryPoint->container.ptr() == startBoundary.container.ptr()) {
            auto lineStartRect = boundsForRange(SimpleRange(*lineStartBoundaryPoint, range->end));
            if (contentRect.contains(lineStartRect.location())) {
                elementRect = lineStartRect;
                startBoundary = *lineStartBoundaryPoint;
            } else if (lineStartBoundaryPoint->offset < lineStartBoundaryPoint->container->length()) {
                // Sometimes we're one character off from being in-bounds. Check for this too.
                lineStartBoundaryPoint->offset = lineStartBoundaryPoint->offset + 1;
                lineStartRect = boundsForRange(SimpleRange(*lineStartBoundaryPoint, range->end));
                lineStartBoundaryPoint->offset = lineStartBoundaryPoint->offset - 1;
                if (contentRect.contains(lineStartRect.location())) {
                    elementRect = lineStartRect;
                    startBoundary = *lineStartBoundaryPoint;
                }
            }
        }
    }

    // Computing previous line start positions is cheap relative to computing boundsForRange, so compute the end boundary by
    // grabbing batches of lines and binary searching within them to minimize calls to boundsForRange.
    Vector<BoundaryPoint> boundaryPoints = { endBoundary };
    do {
        // If the first boundary point is contained in contentRect, then it's a match because we know everything in the last batch
        // of lines was not contained in contentRect.
        if (boundaryPointsContainedInRect(startBoundary, boundaryPoints.at(0), contentRect)) {
            endBoundary = boundaryPoints.at(0);
            break;
        }

        auto lastBoundaryPoint = boundaryPoints.last();
        elementRect = boundsForRange({ startBoundary, lastBoundaryPoint });
        if (elementRect.isEmpty())
            break;
        // Otherwise if the last boundary point is contained in contentRect, then we know some boundary point in this batch is
        // our target end boundary point.
        if (contentRect.contains(elementRect.location() + elementRect.size())) {
            endBoundary = lastBoundaryPointContainedInRect(boundaryPoints, startBoundary, contentRect).value_or(lastBoundaryPoint);
            break;
        }
        boundaryPoints = previousLineStartBoundaryPoints(VisiblePosition(makeContainerOffsetPosition(lastBoundaryPoint)), *range, 64);
    } while (!boundaryPoints.isEmpty());

    // Sometimes we shrink one line too far. Check the next line end to see if it's in bounds.
    auto nextLineEndPosition = this->nextLineEndPosition(VisiblePosition(makeContainerOffsetPosition(endBoundary)));
    auto nextLineEndBoundaryPoint = makeBoundaryPoint(nextLineEndPosition);
    if (nextLineEndBoundaryPoint && nextLineEndBoundaryPoint->container.ptr() == endBoundary.container.ptr()) {
        auto lineEndRect = boundsForRange(SimpleRange(startBoundary, *nextLineEndBoundaryPoint));

        if (contentRect.contains(lineEndRect.location() + lineEndRect.size()))
            endBoundary = *nextLineEndBoundaryPoint;
    }

    return { { startBoundary, endBoundary } };
}

std::optional<SimpleRange> AccessibilityObject::findTextRange(const Vector<String>& searchStrings, const SimpleRange& start, AccessibilitySearchTextDirection direction) const
{
    std::optional<SimpleRange> found;
    if (direction == AccessibilitySearchTextDirection::Forward)
        found = rangeOfStringClosestToRangeInDirection(start, AccessibilitySearchDirection::Next, searchStrings);
    else if (direction == AccessibilitySearchTextDirection::Backward)
        found = rangeOfStringClosestToRangeInDirection(start, AccessibilitySearchDirection::Previous, searchStrings);
    else if (direction == AccessibilitySearchTextDirection::Closest) {
        auto foundAfter = rangeOfStringClosestToRangeInDirection(start, AccessibilitySearchDirection::Next, searchStrings);
        auto foundBefore = rangeOfStringClosestToRangeInDirection(start, AccessibilitySearchDirection::Previous, searchStrings);
        found = rangeClosestToRange(start, WTFMove(foundAfter), WTFMove(foundBefore));
    }
    if (found) {
        // If the search started within a text control, ensure that the result is inside that element.
        if (element() && element()->isTextField()) {
            if (!found->startContainer().isDescendantOrShadowDescendantOf(element())
                || !found->endContainer().isDescendantOrShadowDescendantOf(element()))
                return std::nullopt;
        }
    }
    return found;
}

Vector<SimpleRange> AccessibilityObject::findTextRanges(const AccessibilitySearchTextCriteria& criteria) const
{
    std::optional<SimpleRange> range;
    if (criteria.start == AccessibilitySearchTextStartFrom::Selection)
        range = selectionRange();
    else
        range = simpleRange();
    if (!range)
        return { };

    if (criteria.start == AccessibilitySearchTextStartFrom::Begin)
        range->end = range->start;
    else if (criteria.start == AccessibilitySearchTextStartFrom::End)
        range->start = range->end;
    else if (criteria.direction == AccessibilitySearchTextDirection::Backward)
        range->start = range->end;
    else
        range->end = range->start;

    Vector<SimpleRange> result;
    switch (criteria.direction) {
    case AccessibilitySearchTextDirection::Forward:
    case AccessibilitySearchTextDirection::Backward:
    case AccessibilitySearchTextDirection::Closest:
        if (auto foundRange = findTextRange(criteria.searchStrings, *range, criteria.direction))
            result.append(*foundRange);
        break;
    case AccessibilitySearchTextDirection::All:
        auto appendFoundRanges = [&](AccessibilitySearchTextDirection direction) {
            for (auto foundRange = range; (foundRange = findTextRange(criteria.searchStrings, *foundRange, direction)); )
                result.append(*foundRange);
        };
        appendFoundRanges(AccessibilitySearchTextDirection::Forward);
        appendFoundRanges(AccessibilitySearchTextDirection::Backward);
        break;
    }
    return result;
}

Vector<String> AccessibilityObject::performTextOperation(const AccessibilityTextOperation& operation)
{
    Vector<String> result;

    if (operation.textRanges.isEmpty())
        return result;

    auto* frame = this->frame();
    if (!frame)
        return result;

    for (const auto& textRange : operation.textRanges) {
        if (!frame->selection().setSelectedRange(textRange, Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes))
            continue;

        String text = plainText(textRange);
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
    switch (ariaRole) {
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TextField:
        return true;
    default:
        return false;
    }
}    

bool AccessibilityObject::isARIAControl(AccessibilityRole ariaRole)
{
    if (isARIAInput(ariaRole))
        return true;

    switch (ariaRole) {
    case AccessibilityRole::Button:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::Slider:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
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

    RenderObject* renderer = this->renderer();
    return renderer && renderer->isRenderMeter();
}

static IntRect boundsForRects(const LayoutRect& rect1, const LayoutRect& rect2, const SimpleRange& dataRange)
{
    LayoutRect ourRect = rect1;
    ourRect.unite(rect2);

    // If the rectangle spans lines and contains multiple text characters, use the range's bounding box intead.
    if (rect1.maxY() != rect2.maxY() && characterCount(dataRange) > 1) {
        if (auto boundingBox = unionRect(RenderObject::absoluteTextRects(dataRange)); !boundingBox.isEmpty())
            ourRect = boundingBox;
    }

    return snappedIntRect(ourRect);
}

IntRect AccessibilityRenderObject::boundsForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    if (visiblePositionRange.isNull())
        return IntRect();

    // Create a mutable VisiblePositionRange.
    VisiblePositionRange range(visiblePositionRange);
    LayoutRect rect1 = range.start.absoluteCaretBounds();
    LayoutRect rect2 = range.end.absoluteCaretBounds();

    // Readjust for position at the edge of a line. This is to exclude line rect that doesn't need to be accounted in the range bounds
    if (rect2.y() != rect1.y()) {
        VisiblePosition endOfFirstLine = endOfLine(range.start);
        if (range.start == endOfFirstLine) {
            range.start.setAffinity(Affinity::Downstream);
            rect1 = range.start.absoluteCaretBounds();
        }
        if (range.end == endOfFirstLine) {
            range.end.setAffinity(Affinity::Upstream);
            rect2 = range.end.absoluteCaretBounds();
        }
    }

    return boundsForRects(rect1, rect2, *makeSimpleRange(range));
}

IntRect AccessibilityObject::boundsForRange(const SimpleRange& range) const
{
    auto cache = axObjectCache();
    if (!cache)
        return { };

    auto start = cache->startOrEndCharacterOffsetForRange(range, true);
    auto end = cache->startOrEndCharacterOffsetForRange(range, false);

    auto rect1 = cache->absoluteCaretBoundsForCharacterOffset(start);
    auto rect2 = cache->absoluteCaretBoundsForCharacterOffset(end);

    // Readjust for position at the edge of a line. This is to exclude line rect that doesn't need to be accounted in the range bounds.
    if (rect2.y() != rect1.y()) {
        auto endOfFirstLine = cache->endCharacterOffsetOfLine(start);
        if (start.isEqual(endOfFirstLine)) {
            start = cache->nextCharacterOffset(start, false);
            rect1 = cache->absoluteCaretBoundsForCharacterOffset(start);
        }
        if (end.isEqual(endOfFirstLine)) {
            end = cache->previousCharacterOffset(end, false);
            rect2 = cache->absoluteCaretBoundsForCharacterOffset(end);
        }
    }

    return boundsForRects(rect1, rect2, range);
}

IntPoint AccessibilityObject::linkClickPoint()
{
    ASSERT(isLink());
    /* A link bounding rect can contain points that are not part of the link.
     For instance, a link that starts at the end of a line and finishes at the
     beginning of the next line will have a bounding rect that includes the
     entire two lines. In such a case, the middle point of the bounding rect
     may not belong to the link element and thus may not activate the link.
     Hence, return the middle point of the first character in the link if exists.
     */
    if (auto range = simpleRange()) {
        auto start = VisiblePosition { makeContainerOffsetPosition(range->start) };
        auto end = start.next();
        if (contains<ComposedTree>(*range, makeBoundaryPoint(end)))
            return { boundsForRange(*makeSimpleRange(start, end)).center() };
    }
    return clickPointFromElementRect();
}

IntPoint AccessibilityObject::clickPoint()
{
    // Headings are usually much wider than their textual content. If the mid point is used, often it can be wrong.
    if (isHeading() && children().size() == 1)
        return children().first()->clickPoint();

    if (isLink())
        return linkClickPoint();

    // use the default position unless this is an editable web area, in which case we use the selection bounds.
    if (!isWebArea() || !canSetValueAttribute())
        return clickPointFromElementRect();

    return boundsForVisiblePositionRange(selection()).center();
}

IntPoint AccessibilityObject::clickPointFromElementRect() const
{
    return roundedIntPoint(elementRect().center());
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
            if (obj->style().hasEffectiveAppearance())
                obj->theme().inflateRectForControlRenderer(*obj, r);
            result.unite(r);
        }
    }
    return snappedIntRect(LayoutRect(result));
}
    
bool AccessibilityObject::press()
{
    // The presence of the actionElement will confirm whether we should even attempt a press.
    RefPtr actionElem = actionElement();
    if (!actionElem)
        return false;
    if (auto* frame = actionElem->document().frame())
        frame->loader().resetMultipleFormSubmissionProtection();
    
    // Hit test at this location to determine if there is a sub-node element that should act
    // as the target of the action.
    RefPtr<Element> hitTestElement;
    RefPtr document = this->document();
    if (document) {
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AccessibilityHitTest };
        HitTestResult hitTestResult { clickPoint() };
        document->hitTest(hitType, hitTestResult);
        if (RefPtr innerNode = hitTestResult.innerNode()) {
            if (RefPtr shadowHost = innerNode->shadowHost())
                hitTestElement = WTFMove(shadowHost);
            else if (RefPtr element = dynamicDowncast<Element>(*innerNode))
                hitTestElement = WTFMove(element);
            else
                hitTestElement = innerNode->parentElement();
        }
    }

    // Prefer the actionElement instead of this node, if the actionElement is inside this node.
    RefPtr pressElement = this->element();
    if (!pressElement || actionElem->isDescendantOf(*pressElement))
        pressElement = WTFMove(actionElem);

    ASSERT(pressElement);
    // Prefer the hit test element, if it is inside the target element.
    if (hitTestElement && hitTestElement->isDescendantOf(*pressElement))
        pressElement = WTFMove(hitTestElement);

    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document.get());

    bool dispatchedEvent = false;
#if PLATFORM(IOS_FAMILY)
    if (hasTouchEventListener())
        dispatchedEvent = dispatchTouchEvent();
#endif

    return dispatchedEvent || pressElement->accessKeyAction(true) || pressElement->dispatchSimulatedClick(nullptr, SendMouseUpDownEvents);
}
    
bool AccessibilityObject::dispatchTouchEvent()
{
#if ENABLE(IOS_TOUCH_EVENTS)
    if (auto* frame = mainFrame())
        return frame->eventHandler().dispatchSimulatedTouchEvent(clickPoint());
#endif
    return false;
}

LocalFrame* AccessibilityObject::frame() const
{
    Node* node = this->node();
    return node ? node->document().frame() : nullptr;
}

LocalFrame* AccessibilityObject::mainFrame() const
{
    auto* document = topDocument();
    if (!document)
        return nullptr;
    
    auto* frame = document->frame();
    if (!frame)
        return nullptr;
    
    auto* localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localFrame)
        return nullptr;

    return localFrame;
}

Document* AccessibilityObject::topDocument() const
{
    if (!document())
        return nullptr;
    return &document()->topDocument();
}

RenderView* AccessibilityObject::topRenderer() const
{
    if (auto* topDocument = this->topDocument())
        return topDocument->renderView();
    return nullptr;
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

VisiblePosition AccessibilityObject::visiblePositionForPoint(const IntPoint& point) const
{
    // convert absolute point to view coordinates
    RenderView* renderView = topRenderer();
    if (!renderView)
        return VisiblePosition();

#if PLATFORM(MAC)
    auto* frameView = &renderView->frameView();
#endif

    Node* innerNode = nullptr;

    // Locate the node containing the point
    // FIXME: Remove this loop and instead add HitTestRequest::Type::AllowVisibleChildFrameContentOnly to the hit test request type.
    LayoutPoint pointResult;
    while (1) {
        LayoutPoint pointToUse;
#if PLATFORM(MAC)
        pointToUse = frameView->screenToContents(point);
#else
        pointToUse = point;
#endif
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active };
        HitTestResult result { pointToUse };
        renderView->document().hitTest(hitType, result);
        innerNode = result.innerNode();
        if (!innerNode)
            return VisiblePosition();

        RenderObject* renderer = innerNode->renderer();
        if (!renderer)
            return VisiblePosition();

        pointResult = result.localPoint();

        // done if hit something other than a widget
        auto* renderWidget = dynamicDowncast<RenderWidget>(*renderer);
        if (!renderWidget)
            break;

        // descend into widget (FRAME, IFRAME, OBJECT...)
        auto* widget = renderWidget->widget();
        auto* frameView = dynamicDowncast<LocalFrameView>(widget);
        if (!frameView)
            break;
        auto* document = frameView->frame().document();
        if (!document)
            break;

        renderView = document->renderView();
#if PLATFORM(MAC)
        // FIXME: Can this be removed? This seems like a redundant assignment.
        frameView = downcast<LocalFrameView>(widget);
#endif
    }
    
    return innerNode->renderer()->positionForPoint(pointResult, nullptr);
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
    if (visiblePos1 == visiblePos2 && visiblePos2.affinity() == Affinity::Upstream)
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

    return { startPos, endPos };
}

VisiblePositionRange AccessibilityObject::positionOfLeftWord(const VisiblePosition& visiblePos) const
{
    auto start = startOfWord(visiblePos, WordSide::LeftWordIfOnBoundary);
    return { start, endOfWord(start) };
}

VisiblePositionRange AccessibilityObject::positionOfRightWord(const VisiblePosition& visiblePos) const
{
    auto start = startOfWord(visiblePos, WordSide::RightWordIfOnBoundary);
    return { start, endOfWord(start) };
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

    return { startPosition, endOfLine(prevVisiblePos) };
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

    return { startPosition, endPosition };
}

VisiblePositionRange AccessibilityObject::sentenceForPosition(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    auto startPosition = startOfSentence(visiblePos);
    return { startPosition, endOfSentence(startPosition) };
}

VisiblePositionRange AccessibilityObject::paragraphForPosition(const VisiblePosition& visiblePos) const
{
    auto startPosition = startOfParagraph(visiblePos);
    return { startPosition, endOfParagraph(startPosition) };
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
        return { };

    return { startOfStyleRange(visiblePos), endOfStyleRange(visiblePos) };
}

// NOTE: Consider providing this utility method as AX API
VisiblePositionRange AccessibilityObject::visiblePositionRangeForRange(const CharacterRange& range) const
{
    if (range.location + range.length > getLengthForTextRange())
        return { };

    auto startPosition = visiblePositionForIndex(range.location);
    startPosition.setAffinity(Affinity::Downstream);
    return { startPosition, visiblePositionForIndex(range.location + range.length) };
}

std::optional<SimpleRange> AccessibilityObject::rangeForCharacterRange(const CharacterRange& range) const
{
    unsigned textLength = getLengthForTextRange();
    if (range.location + range.length > textLength)
        return std::nullopt;

    // Avoid setting selection to uneditable parent node in FrameSelection::setSelectedRange. See webkit.org/b/206093.
    if (!range.location && !range.length && !textLength)
        return std::nullopt;

    if (auto* cache = axObjectCache()) {
        auto start = cache->characterOffsetForIndex(range.location, this);
        auto end = cache->characterOffsetForIndex(range.location + range.length, this);
        return cache->rangeForUnorderedCharacterOffsets(start, end);
    }
    return std::nullopt;
}

VisiblePositionRange AccessibilityObject::lineRangeForPosition(const VisiblePosition& visiblePosition) const
{
    return { startOfLine(visiblePosition), endOfLine(visiblePosition) };
}

#if PLATFORM(MAC)
AXTextMarkerRange AccessibilityObject::selectedTextMarkerRange()
{
    auto visibleRange = selectedVisiblePositionRange();
    if (visibleRange.isNull())
        return { };
    return { visibleRange };
}
#endif

bool AccessibilityObject::replacedNodeNeedsCharacter(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !isRendererReplacedElement(replacedNode->renderer()) || replacedNode->isTextNode())
        return false;

    // create an AX object, but skip it if it is not supposed to be seen
    if (auto* cache = replacedNode->renderer()->document().axObjectCache()) {
        if (auto* axObject = cache->getOrCreate(replacedNode))
            return !axObject->accessibilityIsIgnored();
    }

    return true;
}

#if PLATFORM(COCOA) && ENABLE(MODEL_ELEMENT)
Vector<RetainPtr<id>> AccessibilityObject::modelElementChildren()
{
    RefPtr model = dynamicDowncast<HTMLModelElement>(node());
    if (!model)
        return { };

    return model->accessibilityChildren();
}
#endif

// Finds a RenderListItem parent give a node.
static RenderListItem* renderListItemContainer(Node* node)
{
    for (; node; node = node->parentNode()) {
        if (auto* listItem = dynamicDowncast<RenderListItem>(node->renderBoxModelObject()))
            return listItem;
    }
    return nullptr;
}

// Returns the text representing a list marker taking into account the position of the text in the line of text.
static StringView listMarkerText(RenderListItem* listItem, const VisiblePosition& startVisiblePosition, std::optional<StringView> markerText = std::nullopt)
{
    if (!listItem)
        return { };

    if (!markerText)
        markerText = listItem->markerTextWithSuffix();
    if (markerText->isEmpty())
        return { };

    // Only include the list marker if the range includes the line start (where the marker would be), and is in the same line as the marker.
    if (!isStartOfLine(startVisiblePosition) || !inSameLine(startVisiblePosition, firstPositionInNode(&listItem->element())))
        return { };
    return *markerText;
}

StringView AccessibilityObject::listMarkerTextForNodeAndPosition(Node* node, Position&& startPosition)
{
    auto* listItem = renderListItemContainer(node);
    if (!listItem)
        return { };
    // Creating a VisiblePosition and determining its relationship to a line of text can be expensive.
    // Thus perform that determination only if we have some text to return.
    auto markerText = listItem->markerTextWithSuffix();
    if (markerText.isEmpty())
        return { };
    return listMarkerText(listItem, startPosition, markerText);
}

String AccessibilityObject::stringForRange(const SimpleRange& range) const
{
    TextIterator it(range);
    if (it.atEnd())
        return String();

    StringBuilder builder;
    for (; !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length()) {
            // If this is in a list item, we need to add the text for the list marker
            // because a RenderListMarker does not have a Node equivalent and thus does not appear
            // when iterating text.
            // Don't add list marker text for new line character.
            if (it.text().length() != 1 || !deprecatedIsSpaceOrNewline(it.text()[0]))
                builder.append(listMarkerTextForNodeAndPosition(it.node(), makeDeprecatedLegacyPosition(it.range().start)));
            it.appendTextToStringBuilder(builder);
        } else {
            if (replacedNodeNeedsCharacter(it.node()))
                builder.append(objectReplacementCharacter);
        }
    }

    return builder.toString();
}

String AccessibilityObject::stringForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange)
{
    auto range = makeSimpleRange(visiblePositionRange);
    if (!range)
        return { };

    StringBuilder builder;
    for (TextIterator it(*range); !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length()) {
            // Add a textual representation for list marker text.
            builder.append(listMarkerText(renderListItemContainer(it.node()), visiblePositionRange.start));
            it.appendTextToStringBuilder(builder);
        } else {
            // locate the node and starting offset for this replaced range
            if (replacedNodeNeedsCharacter(it.node()))
                builder.append(objectReplacementCharacter);
        }
    }
    return builder.toString();
}

VisiblePosition AccessibilityObject::nextLineEndPosition(const VisiblePosition& startPosition) const
{
    if (startPosition.isNull())
        return { };

    // Move to the next position to ensure we move off a line end.
    auto nextPosition = startPosition.next();
    if (nextPosition.isNull())
        return { };

    auto lineEndPosition = endOfLine(nextPosition);
    // As long as the position hasn't reached the end of the document, keep searching for a valid line
    // end position. Skip past null positions, as there are cases like when the position is next to a
    // floating object that'll return null for end of line. Also, in certain scenarios, like when one
    // position is editable and the other isn't (e.g. in mixed-contenteditable-visible-character-range-hang.html),
    // we may end up back at the same position we started at. This is never valid, so keep moving forward
    // trying to find the next line end.
    while ((lineEndPosition.isNull() || lineEndPosition == startPosition) && nextPosition.isNotNull()) {
        nextPosition = nextPosition.next();
        lineEndPosition = endOfLine(nextPosition);
    }
    return lineEndPosition;
}

std::optional<VisiblePosition> AccessibilityObject::previousLineStartPositionInternal(const VisiblePosition& visiblePosition) const
{
    if (visiblePosition.isNull())
        return std::nullopt;

    // Make sure we move off of a line start.
    auto previousVisiblePosition = visiblePosition.previous();
    if (previousVisiblePosition.isNull())
        return std::nullopt;

    auto startPosition = startOfLine(previousVisiblePosition);
    // As long as the position hasn't reached the beginning of the document, keep searching for a valid line start position.
    // This avoids returning a null position when we shouldn't, like when a position is next to a floating object.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && previousVisiblePosition.isNotNull()) {
            previousVisiblePosition = previousVisiblePosition.previous();
            startPosition = startOfLine(previousVisiblePosition);
        }
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    return startPosition;
}

VisiblePosition AccessibilityObject::nextSentenceEndPosition(const VisiblePosition& position) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336

    // Make sure we move off of a sentence end.
    auto nextPosition = position.next();
    auto range = makeSimpleRange(startOfLine(nextPosition), endOfLine(nextPosition));
    if (!range)
        return { };

    // An empty line is considered a sentence. If it's skipped, then the sentence parser will not
    // see this empty line. Instead, return the end position of the empty line.
    return hasAnyPlainText(*range) ? endOfSentence(nextPosition) : nextPosition;
}

VisiblePosition AccessibilityObject::previousSentenceStartPosition(const VisiblePosition& position) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336

    // Make sure we move off of a sentence start.
    auto previousPosition = position.previous();
    auto range = makeSimpleRange(startOfLine(previousPosition), endOfLine(previousPosition));
    if (!range)
        return { };

    // Treat empty line as a separate sentence.
    return hasAnyPlainText(*range) ? startOfSentence(previousPosition) : previousPosition;
}

VisiblePosition AccessibilityObject::nextParagraphEndPosition(const VisiblePosition& position) const
{
    return endOfParagraph(position.next());
}

VisiblePosition AccessibilityObject::previousParagraphStartPosition(const VisiblePosition& position) const
{
    return startOfParagraph(position.previous());
}

OptionSet<SpeakAs> AccessibilityObject::speakAsProperty() const
{
    if (auto* style = this->style())
        return style->speakAs();
    return { };
}

InsideLink AccessibilityObject::insideLink() const
{
    auto* style = this->style();
    if (!style || !style->isLink())
        return InsideLink::NotInside;
    return style->insideLink();
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

    return role == AccessibilityRole::Checkbox
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
        || isSecureField();
}

String AccessibilityObject::readOnlyValue() const
{
    if (!hasAttribute(aria_readonlyAttr))
        return ariaRoleAttribute() != AccessibilityRole::Unknown && supportsReadOnly() ? "false"_s : String();

    return getAttribute(aria_readonlyAttr).string().convertToASCIILowercase();
}

bool AccessibilityObject::supportsCheckedState() const
{
    auto role = roleValue();
    return isCheckboxOrRadio()
    || role == AccessibilityRole::MenuItemCheckbox
    || role == AccessibilityRole::MenuItemRadio
    || role == AccessibilityRole::Switch
    || isToggleButton();
}

bool AccessibilityObject::supportsAutoComplete() const
{
    return (isComboBox() || isARIATextControl()) && hasAttribute(aria_autocompleteAttr);
}

String AccessibilityObject::autoCompleteValue() const
{
    const AtomString& autoComplete = getAttribute(aria_autocompleteAttr);
    if (equalLettersIgnoringASCIICase(autoComplete, "inline"_s)
        || equalLettersIgnoringASCIICase(autoComplete, "list"_s)
        || equalLettersIgnoringASCIICase(autoComplete, "both"_s))
        return autoComplete;

    return "none"_s;
}

bool AccessibilityObject::contentEditableAttributeIsEnabled(Element* element)
{
    if (!element)
        return false;
    
    const AtomString& contentEditableValue = element->attributeWithoutSynchronization(contenteditableAttr);
    if (contentEditableValue.isNull())
        return false;
    
    // Both "true" (case-insensitive) and the empty string count as true.
    return contentEditableValue.isEmpty() || equalLettersIgnoringASCIICase(contentEditableValue, "true"_s);
}

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
        currentVisiblePos = previousLinePosition(currentVisiblePos, 0, HasEditableAXRole);
        ++lineCount;
    } while (currentVisiblePos.isNotNull() && !(inSameLine(currentVisiblePos, savedVisiblePos)));

    return lineCount;
}

// NOTE: Consider providing this utility method as AX API
CharacterRange AccessibilityObject::plainTextRangeForVisiblePositionRange(const VisiblePositionRange& positionRange) const
{
    int index1 = index(positionRange.start);
    int index2 = index(positionRange.end);
    if (index1 < 0 || index2 < 0 || index1 > index2)
        return { };

    return CharacterRange(index1, index2 - index1);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given screen coordinates. This parameterized attribute returns the
// complete range of characters (including surrogate pairs of multi-byte glyphs) at the given
// screen coordinates.
// NOTE: This varies from AppKit when the point is below the last line. AppKit returns an
// an error in that case. We return textControl->text().length(), 1. Does this matter?
CharacterRange AccessibilityObject::characterRangeForPoint(const IntPoint& point) const
{
    int i = index(visiblePositionForPoint(point));
    if (i < 0)
        return { };
    return { static_cast<uint64_t>(i), 1 };
}

// Given a character index, the range of text associated with this accessibility object
// over which the style in effect at that character index applies.
CharacterRange AccessibilityObject::doAXStyleRangeForIndex(unsigned index) const
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

void AccessibilityObject::updateBackingStore()
{
    if (!axObjectCache())
        return;
    
    // Updating the layout may delete this object.
    RefPtr<AccessibilityObject> protectedThis(this);
    if (auto* document = this->document()) {
        if (!Accessibility::inRenderTreeOrStyleUpdate(*document))
            document->updateLayoutIgnorePendingStylesheets();
    }

    if (auto* cache = axObjectCache())
        cache->performDeferredCacheUpdate(ForceLayout::Yes);

    updateChildrenIfNecessary();
}

const AccessibilityScrollView* AccessibilityObject::ancestorAccessibilityScrollView(bool includeSelf) const
{
    return downcast<AccessibilityScrollView>(Accessibility::findAncestor<AccessibilityObject>(*this, includeSelf, [] (const auto& object) {
        return is<AccessibilityScrollView>(object);
    }));
}

#if PLATFORM(COCOA)
RemoteAXObjectRef AccessibilityObject::remoteParentObject() const
{
    if (auto* document = this->document()) {
        if (auto* frame = document->frame())
            return frame->loader().client().accessibilityRemoteObject();
    }
    return nullptr;
}
#endif

Document* AccessibilityObject::document() const
{
    auto* frameView = documentFrameView();
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

LocalFrameView* AccessibilityObject::documentFrameView() const 
{ 
    const AccessibilityObject* object = this;
    while (object && !object->isAccessibilityRenderObject()) 
        object = object->parentObject();
        
    if (!object)
        return nullptr;

    return object->documentFrameView();
}

const AccessibilityObject::AccessibilityChildrenVector& AccessibilityObject::children(bool updateChildrenIfNeeded)
{
    if (updateChildrenIfNeeded)
        updateChildrenIfNecessary();

    return m_children;
}

void AccessibilityObject::updateChildrenIfNecessary()
{
    if (!childrenInitialized()) {
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
    m_childrenInitialized = false;
}

AccessibilityObject* AccessibilityObject::anchorElementForNode(Node& node)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return nullptr;

    WeakPtr cache = renderer->document().axObjectCache();
    RefPtr axObject = cache ? cache->getOrCreate(renderer.get()) : nullptr;
    auto* anchor = axObject ? axObject->anchorElement() : nullptr;
    return anchor ? cache->getOrCreate(anchor->renderer()) : nullptr;
}

AccessibilityObject* AccessibilityObject::headingElementForNode(Node* node)
{
    if (!node)
        return nullptr;

    RenderObject* renderObject = node->renderer();
    if (!renderObject)
        return nullptr;

    AccessibilityObject* axObject = renderObject->document().axObjectCache()->getOrCreate(renderObject);

    return Accessibility::findAncestor<AccessibilityObject>(*axObject, true, [] (const AccessibilityObject& object) {
        return object.roleValue() == AccessibilityRole::Heading;
    });
}

void AccessibilityObject::ariaTreeRows(AccessibilityChildrenVector& rows, AccessibilityChildrenVector& ancestors)
{
    auto ownedObjects = this->ownedObjects();

    ancestors.append(this);

    // The ordering of rows is first DOM children *not* in aria-owns, followed by all specified
    // in aria-owns.
    for (const auto& child : children()) {
        // Add tree items as the rows.
        if (child->roleValue() == AccessibilityRole::TreeItem) {
            // Child appears both as a direct child and aria-owns, we should use the ordering as
            // described in aria-owns for this child.
            if (ownedObjects.contains(child))
                continue;

            // The result set may already contain the child through aria-owns. For example,
            // a treeitem sitting under the tree root, which is owned elsewhere in the tree.
            if (rows.contains(child))
                continue;

            rows.append(child);
        }

        // Now see if this item also has rows hiding inside of it.
        if (auto* accessibilityObject = dynamicDowncast<AccessibilityObject>(*child))
            accessibilityObject->ariaTreeRows(rows, ancestors);
    }

    // Now go through the aria-owns elements.
    for (const auto& child : ownedObjects) {
        // Avoid a circular reference via aria-owns by checking if our parent
        // path includes this child. Currently, looking up the aria-owns parent
        // path itself could be expensive, so we track it separately.
        if (ancestors.contains(child))
            continue;

        // Add tree items as the rows.
        if (child->roleValue() == AccessibilityRole::TreeItem) {
            // Hopefully a flow that does not occur often in practice, but if someone were to include
            // the owned child ealier in the top level of the tree, then reference via aria-owns later,
            // move it to the right place.
            if (rows.contains(child))
                rows.removeFirst(child);

            rows.append(child);
        }

        // Now see if this item also has rows hiding inside of it.
        if (auto* accessibilityObject = dynamicDowncast<AccessibilityObject>(*child))
            accessibilityObject->ariaTreeRows(rows, ancestors);
    }

    ancestors.removeLast();
}

void AccessibilityObject::ariaTreeRows(AccessibilityChildrenVector& rows)
{
    AccessibilityChildrenVector ancestors;
    ariaTreeRows(rows, ancestors);
}
    
AXCoreObject::AccessibilityChildrenVector AccessibilityObject::disclosedRows()
{
    AccessibilityChildrenVector result;

    for (const auto& obj : children()) {
        // Add tree items as the rows.
        if (obj->roleValue() == AccessibilityRole::TreeItem)
            result.append(obj);
        // If it's not a tree item, then descend into the group to find more tree items.
        else 
            obj->ariaTreeRows(result);
    }

    return result;
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

String AccessibilityObject::localizedActionVerb() const
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Need to add verbs for select elements.
    static NeverDestroyed<const String> buttonAction(AXButtonActionVerb());
    static NeverDestroyed<const String> textFieldAction(AXTextFieldActionVerb());
    static NeverDestroyed<const String> radioButtonAction(AXRadioButtonActionVerb());
    static NeverDestroyed<const String> checkedCheckboxAction(AXCheckedCheckboxActionVerb());
    static NeverDestroyed<const String> uncheckedCheckboxAction(AXUncheckedCheckboxActionVerb());
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
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::Switch:
        return isChecked() ? checkedCheckboxAction : uncheckedCheckboxAction;
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

String AccessibilityObject::actionVerb() const
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Need to add verbs for select elements.
    switch (roleValue()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::ToggleButton:
        return "press"_s;
    case AccessibilityRole::TextField:
    case AccessibilityRole::TextArea:
        return "activate"_s;
    case AccessibilityRole::RadioButton:
        return "select"_s;
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::Switch:
        return isChecked() ? "uncheck"_s : "check"_s;
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
        return "jump"_s;
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::MenuListPopup:
    case AccessibilityRole::ListItem:
        return "select"_s;
    default:
        break;
    }
#endif
    return { };
}

bool AccessibilityObject::ariaIsMultiline() const
{
    return equalLettersIgnoringASCIICase(getAttribute(aria_multilineAttr), "true"_s);
}

String AccessibilityObject::invalidStatus() const
{
    String grammarValue = "grammar"_s;
    String falseValue = "false"_s;
    String spellingValue = "spelling"_s;
    String trueValue = "true"_s;
    String undefinedValue = "undefined"_s;

    // aria-invalid can return false (default), grammar, spelling, or true.
    auto ariaInvalid = getAttribute(aria_invalidAttr).string().trim(isASCIIWhitespace);

    if (ariaInvalid.isEmpty()) {
        auto* htmlElement = dynamicDowncast<HTMLElement>(this->node());
        if (auto* validatedFormListedElement = htmlElement ? htmlElement->asValidatedFormListedElement() : nullptr) {
            // "willValidate" is true if the element is able to be validated.
            if (validatedFormListedElement->willValidate() && !validatedFormListedElement->isValidFormControlElement())
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
    auto currentStateValue = getAttribute(aria_currentAttr).string().trim(isASCIIWhitespace);

    // If "false", empty, or missing, return false state.
    if (currentStateValue.isEmpty() || currentStateValue == "false"_s)
        return AccessibilityCurrentState::False;
    
    if (currentStateValue == "page"_s)
        return AccessibilityCurrentState::Page;
    if (currentStateValue == "step"_s)
        return AccessibilityCurrentState::Step;
    if (currentStateValue == "location"_s)
        return AccessibilityCurrentState::Location;
    if (currentStateValue == "date"_s)
        return AccessibilityCurrentState::Date;
    if (currentStateValue == "time"_s)
        return AccessibilityCurrentState::Time;
    
    // Any value not included in the list of allowed values should be treated as "true".
    return AccessibilityCurrentState::True;
}

bool AccessibilityObject::isModalDescendant(Node* modalNode) const
{
    Node* node = this->node();
    if (!modalNode || !node)
        return false;
    
    // ARIA 1.1 aria-modal, indicates whether an element is modal when displayed.
    // For the decendants of the modal object, they should also be considered as aria-modal=true.
    // Determine descendancy by iterating the composed tree which inherently accounts for shadow roots and slots.
    for (auto* ancestor = this->node(); ancestor; ancestor = ancestor->parentInComposedTree()) {
        if (ancestor == modalNode)
            return true;
    }
    return false;
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
    RefPtr element = dynamicDowncast<Element>(node());
    return element && element->hasTagName(tagName);
}

bool AccessibilityObject::hasAttribute(const QualifiedName& attribute) const
{
    RefPtr element = this->element();
    if (!element)
        return false;

    if (element->hasAttributeWithoutSynchronization(attribute))
        return true;

    if (auto* defaultARIA = element->customElementDefaultARIAIfExists())
        return defaultARIA->hasAttribute(attribute);

    return false;
}

const AtomString& AccessibilityObject::getAttribute(const QualifiedName& attribute) const
{
    if (RefPtr element = this->element()) {
        auto& value = element->attributeWithoutSynchronization(attribute);
        if (!value.isNull())
            return value;
        if (auto* defaultARIA = element->customElementDefaultARIAIfExists())
            return defaultARIA->valueForAttribute(*element, attribute);
    }
    return nullAtom();
}

String AccessibilityObject::nameAttribute() const
{
    return getAttribute(nameAttr);
}

int AccessibilityObject::getIntegralAttribute(const QualifiedName& attributeName) const
{
    return parseHTMLInteger(getAttribute(attributeName)).value_or(0);
}

bool AccessibilityObject::replaceTextInRange(const String& replacementString, const CharacterRange& range)
{
    // If this is being called on the web area, redirect it to be on the body, which will have a renderer associated with it.
    if (RefPtr document = dynamicDowncast<Document>(node())) {
        if (auto bodyObject = axObjectCache()->getOrCreate(document->body()))
            return bodyObject->replaceTextInRange(replacementString, range);
        return false;
    }

    // FIXME: This checks node() is an Element, but below we assume that means renderer()->node() is an element.
    if (!renderer() || !is<Element>(node()))
        return false;

    auto& element = downcast<Element>(*renderer()->node());

    // We should use the editor's insertText to mimic typing into the field.
    // Also only do this when the field is in editing mode.
    auto& frame = renderer()->frame();
    if (element.shouldUseInputMethod()) {
        frame.selection().setSelectedRange(rangeForCharacterRange(range), Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes);
        frame.editor().replaceSelectionWithText(replacementString, Editor::SelectReplacement::No, Editor::SmartReplace::No);
        return true;
    }
    
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
        input->setRangeText(replacementString, range.location, range.length, emptyString());
        return true;
    }
    if (RefPtr textarea = dynamicDowncast<HTMLTextAreaElement>(element)) {
        textarea->setRangeText(replacementString, range.location, range.length, emptyString());
        return true;
    }

    return false;
}

bool AccessibilityObject::insertText(const String& text)
{
    AXTRACE(makeString("AccessibilityObject::insertText text = ", text));

    if (!renderer())
        return false;

    RefPtr element = dynamicDowncast<Element>(node());
    if (!element)
        return false;

    // Only try to insert text if the field is in editing mode (excluding secure fields, which we do still want to try to insert into).
    if (!isSecureField() && !element->shouldUseInputMethod())
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

using ARIARoleMap = HashMap<String, AccessibilityRole, ASCIICaseInsensitiveHash>;
using ARIAReverseRoleMap = HashMap<AccessibilityRole, String, DefaultHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>>;

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
        { "alert"_s, AccessibilityRole::ApplicationAlert },
        { "alertdialog"_s, AccessibilityRole::ApplicationAlertDialog },
        { "application"_s, AccessibilityRole::WebApplication },
        { "article"_s, AccessibilityRole::DocumentArticle },
        { "banner"_s, AccessibilityRole::LandmarkBanner },
        { "blockquote"_s, AccessibilityRole::Blockquote },
        { "button"_s, AccessibilityRole::Button },
        { "caption"_s, AccessibilityRole::Caption },
        { "code"_s, AccessibilityRole::Code },
        { "checkbox"_s, AccessibilityRole::Checkbox },
        { "complementary"_s, AccessibilityRole::LandmarkComplementary },
        { "contentinfo"_s, AccessibilityRole::LandmarkContentInfo },
        { "deletion"_s, AccessibilityRole::Deletion },
        { "dialog"_s, AccessibilityRole::ApplicationDialog },
        { "directory"_s, AccessibilityRole::Directory },
        // The 'doc-*' roles are defined the ARIA DPUB mobile: https://www.w3.org/TR/dpub-aam-1.0
        // Editor's draft is currently at https://w3c.github.io/dpub-aam
        { "doc-abstract"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-acknowledgments"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-afterword"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-appendix"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-backlink"_s, AccessibilityRole::WebCoreLink },
        { "doc-biblioentry"_s, AccessibilityRole::ListItem },
        { "doc-bibliography"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-biblioref"_s, AccessibilityRole::WebCoreLink },
        { "doc-chapter"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-colophon"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-conclusion"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-cover"_s, AccessibilityRole::Image },
        { "doc-credit"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-credits"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-dedication"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-endnote"_s, AccessibilityRole::ListItem },
        { "doc-endnotes"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-epigraph"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-epilogue"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-errata"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-example"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-footnote"_s, AccessibilityRole::Footnote },
        { "doc-foreword"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-glossary"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-glossref"_s, AccessibilityRole::WebCoreLink },
        { "doc-index"_s, AccessibilityRole::LandmarkNavigation },
        { "doc-introduction"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-noteref"_s, AccessibilityRole::WebCoreLink },
        { "doc-notice"_s, AccessibilityRole::DocumentNote },
        { "doc-pagebreak"_s, AccessibilityRole::Splitter },
        { "doc-pagelist"_s, AccessibilityRole::LandmarkNavigation },
        { "doc-part"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-preface"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-prologue"_s, AccessibilityRole::LandmarkDocRegion },
        { "doc-pullquote"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-qna"_s, AccessibilityRole::ApplicationTextGroup },
        { "doc-subtitle"_s, AccessibilityRole::Heading },
        { "doc-tip"_s, AccessibilityRole::DocumentNote },
        { "doc-toc"_s, AccessibilityRole::LandmarkNavigation },
        { "figure"_s, AccessibilityRole::Figure },
        { "generic"_s, AccessibilityRole::Generic },
        // The mappings for 'graphics-*' roles are defined in this spec: https://w3c.github.io/graphics-aam/
        { "graphics-document"_s, AccessibilityRole::GraphicsDocument },
        { "graphics-object"_s, AccessibilityRole::GraphicsObject },
        { "graphics-symbol"_s, AccessibilityRole::GraphicsSymbol },
        { "grid"_s, AccessibilityRole::Grid },
        { "gridcell"_s, AccessibilityRole::GridCell },
        { "table"_s, AccessibilityRole::Table },
        { "cell"_s, AccessibilityRole::Cell },
        { "columnheader"_s, AccessibilityRole::ColumnHeader },
        { "combobox"_s, AccessibilityRole::ComboBox },
        { "definition"_s, AccessibilityRole::Definition },
        { "document"_s, AccessibilityRole::Document },
        { "feed"_s, AccessibilityRole::Feed },
        { "form"_s, AccessibilityRole::Form },
        { "rowheader"_s, AccessibilityRole::RowHeader },
        { "group"_s, AccessibilityRole::ApplicationGroup },
        { "heading"_s, AccessibilityRole::Heading },
        // The "image" role is synonymous with the "img" role. https://w3c.github.io/aria/#image
        { "image"_s, AccessibilityRole::Image },
        { "img"_s, AccessibilityRole::Image },
        { "insertion"_s, AccessibilityRole::Insertion },
        { "link"_s, AccessibilityRole::WebCoreLink },
        { "list"_s, AccessibilityRole::List },
        { "listitem"_s, AccessibilityRole::ListItem },
        { "listbox"_s, AccessibilityRole::ListBox },
        { "log"_s, AccessibilityRole::ApplicationLog },
        { "main"_s, AccessibilityRole::LandmarkMain },
        { "marquee"_s, AccessibilityRole::ApplicationMarquee },
        { "math"_s, AccessibilityRole::DocumentMath },
        { "mark"_s, AccessibilityRole::Mark },
        { "menu"_s, AccessibilityRole::Menu },
        { "menubar"_s, AccessibilityRole::MenuBar },
        { "menuitem"_s, AccessibilityRole::MenuItem },
        { "menuitemcheckbox"_s, AccessibilityRole::MenuItemCheckbox },
        { "menuitemradio"_s, AccessibilityRole::MenuItemRadio },
        { "meter"_s, AccessibilityRole::Meter },
        { "none"_s, AccessibilityRole::Presentational },
        { "note"_s, AccessibilityRole::DocumentNote },
        { "navigation"_s, AccessibilityRole::LandmarkNavigation },
        { "option"_s, AccessibilityRole::ListBoxOption },
        { "paragraph"_s, AccessibilityRole::Paragraph },
        { "presentation"_s, AccessibilityRole::Presentational },
        { "progressbar"_s, AccessibilityRole::ProgressIndicator },
        { "radio"_s, AccessibilityRole::RadioButton },
        { "radiogroup"_s, AccessibilityRole::RadioGroup },
        { "region"_s, AccessibilityRole::LandmarkRegion },
        { "row"_s, AccessibilityRole::Row },
        { "rowgroup"_s, AccessibilityRole::RowGroup },
        { "scrollbar"_s, AccessibilityRole::ScrollBar },
        { "search"_s, AccessibilityRole::LandmarkSearch },
        { "searchbox"_s, AccessibilityRole::SearchField },
        { "separator"_s, AccessibilityRole::Splitter },
        { "slider"_s, AccessibilityRole::Slider },
        { "spinbutton"_s, AccessibilityRole::SpinButton },
        { "status"_s, AccessibilityRole::ApplicationStatus },
        { "subscript"_s, AccessibilityRole::Subscript },
        { "suggestion"_s, AccessibilityRole::Suggestion },
        { "superscript"_s, AccessibilityRole::Superscript },
        { "switch"_s, AccessibilityRole::Switch },
        { "tab"_s, AccessibilityRole::Tab },
        { "tablist"_s, AccessibilityRole::TabList },
        { "tabpanel"_s, AccessibilityRole::TabPanel },
        { "text"_s, AccessibilityRole::StaticText },
        { "textbox"_s, AccessibilityRole::TextField },
        { "term"_s, AccessibilityRole::Term },
        { "time"_s, AccessibilityRole::Time },
        { "timer"_s, AccessibilityRole::ApplicationTimer },
        { "toolbar"_s, AccessibilityRole::Toolbar },
        { "tooltip"_s, AccessibilityRole::UserInterfaceTooltip },
        { "tree"_s, AccessibilityRole::Tree },
        { "treegrid"_s, AccessibilityRole::TreeGrid },
        { "treeitem"_s, AccessibilityRole::TreeItem }
    };

    gAriaRoleMap = new ARIARoleMap;
    gAriaReverseRoleMap = new ARIAReverseRoleMap;
    size_t roleLength = std::size(roles);
    for (size_t i = 0; i < roleLength; ++i) {
        gAriaRoleMap->set(roles[i].ariaRole, roles[i].webcoreRole);
        gAriaReverseRoleMap->set(enumToUnderlyingType(roles[i].webcoreRole), roles[i].ariaRole);
    }

    // Create specific synonyms for the computedRole which is used in WPT tests and the accessibility inspector.
    gAriaReverseRoleMap->set(enumToUnderlyingType(AccessibilityRole::Image), "image"_s);
    gAriaReverseRoleMap->set(enumToUnderlyingType(AccessibilityRole::TextArea), "textbox"_s);
    gAriaReverseRoleMap->set(enumToUnderlyingType(AccessibilityRole::DateTime), "textbox"_s);
    gAriaReverseRoleMap->set(enumToUnderlyingType(AccessibilityRole::Presentational), "none"_s);
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
    auto simplifiedValue = value.simplifyWhiteSpace(isASCIIWhitespace);
    for (auto roleName : StringView(simplifiedValue).split(' ')) {
        AccessibilityRole role = ariaRoleMap().get<ASCIICaseInsensitiveStringViewHashTranslator>(roleName);
        if (enumToUnderlyingType(role))
            return role;
    }
    return AccessibilityRole::Unknown;
}

String AccessibilityObject::computedRoleString() const
{
    // FIXME: Need a few special cases that aren't in the RoleMap: option, etc. http://webkit.org/b/128296
    AccessibilityRole role = roleValue();

    if (role == AccessibilityRole::Image && accessibilityIsIgnored())
        return reverseAriaRoleMap().get(enumToUnderlyingType(AccessibilityRole::Presentational));

    // We do not compute a role string for generic block elements with user-agent assigned roles.
    if (role == AccessibilityRole::Group || role == AccessibilityRole::TextGroup)
        return emptyString();

    // We do compute a role string for block elements with author-provided roles.
    if (role == AccessibilityRole::ApplicationTextGroup
        || role == AccessibilityRole::Footnote
        || role == AccessibilityRole::GraphicsObject)
        return reverseAriaRoleMap().get(enumToUnderlyingType(AccessibilityRole::ApplicationGroup));

    if (role == AccessibilityRole::GraphicsDocument)
        return reverseAriaRoleMap().get(enumToUnderlyingType(AccessibilityRole::Document));

    if (role == AccessibilityRole::GraphicsSymbol)
        return reverseAriaRoleMap().get(enumToUnderlyingType(AccessibilityRole::Image));

    if (role == AccessibilityRole::HorizontalRule)
        return reverseAriaRoleMap().get(enumToUnderlyingType(AccessibilityRole::Splitter));

    if (role == AccessibilityRole::PopUpButton || role == AccessibilityRole::ToggleButton)
        return reverseAriaRoleMap().get(enumToUnderlyingType(AccessibilityRole::Button));

    if (role == AccessibilityRole::LandmarkDocRegion)
        return reverseAriaRoleMap().get(enumToUnderlyingType(AccessibilityRole::LandmarkRegion));

    return reverseAriaRoleMap().get(enumToUnderlyingType(role));
}

void AccessibilityObject::updateRole()
{
    auto previousRole = m_role;
    m_role = determineAccessibilityRole();
    if (previousRole != m_role) {
        if (auto* cache = axObjectCache())
            cache->handleRoleChanged(this);
    }
}

bool AccessibilityObject::hasHighlighting() const
{
    for (Node* node = this->node(); node; node = node->parentNode()) {
        if (node->hasTagName(markTag))
            return true;
    }
    
    return false;
}

SRGBA<uint8_t> AccessibilityObject::colorValue() const
{
    return Color::transparentBlack;
}

#if !PLATFORM(MAC)
String AccessibilityObject::rolePlatformString() const
{
    return Accessibility::roleToPlatformString(roleValue());
}

String AccessibilityObject::rolePlatformDescription() const
{
    // FIXME: implement in other platforms.
    return String();
}

String AccessibilityObject::subrolePlatformString() const
{
    return String();
}
#endif

String AccessibilityObject::embeddedImageDescription() const
{
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer());
    if (!renderImage)
        return { };

    return renderImage->accessibilityDescription();
}

// ARIA spec: User agents must not expose the aria-roledescription property if the element to which aria-roledescription is applied does not have a valid WAI-ARIA role or does not have an implicit WAI-ARIA role semantic.
bool AccessibilityObject::supportsARIARoleDescription() const
{
    auto role = this->roleValue();
    switch (role) {
    case AccessibilityRole::Generic:
    case AccessibilityRole::Unknown:
        return false;
    default:
        return true;
    }
}

String AccessibilityObject::roleDescription() const
{
    // aria-roledescription takes precedence over any other rule.
    if (supportsARIARoleDescription()) {
        auto roleDescription = getAttribute(aria_roledescriptionAttr).string().trim(isASCIIWhitespace);
        if (!roleDescription.isEmpty())
            return roleDescription;
    }

    auto roleDescription = rolePlatformDescription();
    if (!roleDescription.isEmpty())
        return roleDescription;

    if (roleValue() == AccessibilityRole::Figure)
        return AXFigureText();
    
    if (roleValue() == AccessibilityRole::Suggestion)
        return AXSuggestionRoleDescriptionText();

    return roleDescription;
}

bool nodeHasPresentationRole(Node* node)
{
    return nodeHasRole(node, "presentation"_s) || nodeHasRole(node, "none"_s);
}
    
bool AccessibilityObject::supportsPressAction() const
{
    if (isButton())
        return true;
    if (roleValue() == AccessibilityRole::Details)
        return true;
    
    RefPtr actionElement = this->actionElement();
    if (!actionElement)
        return false;

    // [Bug: 133613] Heuristic: If the action element is presentational, we shouldn't expose press as a supported action.
    if (nodeHasPresentationRole(actionElement.get()))
        return false;

    auto* cache = axObjectCache();
    if (!cache)
        return true;

    auto* axObject = actionElement != element() ? cache->getOrCreate(actionElement.get()) : nullptr;
    if (!axObject)
        return true;

    // [Bug: 136247] Heuristic: element handlers that have more than one accessible descendant should not be exposed as supporting press.
    constexpr unsigned searchLimit = 512;
    constexpr unsigned halfSearchLimit = searchLimit / 2;
    unsigned candidatesChecked = 0;
    unsigned matches = 0;

    Vector<RefPtr<AXCoreObject>> candidates;
    candidates.reserveInitialCapacity(std::min(static_cast<size_t>(searchLimit), axObject->children().size() + 1));
    candidates.append(axObject);

    while (!candidates.isEmpty()) {
        RefPtr candidate = candidates.takeLast();
        if (!candidate)
            continue;

        if (candidate->isStaticText() || candidate->isControl() || candidate->isImage() || candidate->isHeading() || candidate->isLink()) {
            matches += 1;
            if (matches >= 2)
                return false;
        }

        candidatesChecked += 1;
        // For performance, cut the search off at the limit and return false because
        // we cannot reasonably determine whether this should support a press action.
        if (candidatesChecked >= searchLimit)
            return false;

        size_t candidateCount = candidatesChecked + candidates.size();
        size_t candidatesLeftUntilLimit = searchLimit > candidateCount ? searchLimit - candidateCount : 0;
        // Limit the amount of children we take. Some objects can have tens of thousands of children, so we don't want to unconditionally append `children()` to the candidate pool.
        const auto& children = candidate->children();
        if (size_t maxChildrenToTake = std::min(static_cast<size_t>(halfSearchLimit), candidatesLeftUntilLimit))
            candidates.append(children.span().subspan(0, std::min(children.size(), maxChildrenToTake)));

        // `candidates` should never be allowed to grow past `searchLimit`.
        ASSERT(searchLimit >= candidates.size());
    }

    return true;
}

bool AccessibilityObject::supportsDatetimeAttribute() const
{
    return hasTagName(insTag) || hasTagName(delTag) || hasTagName(timeTag);
}

String AccessibilityObject::datetimeAttributeValue() const
{
    return getAttribute(datetimeAttr);
}
    
String AccessibilityObject::linkRelValue() const
{
    return getAttribute(relAttr);
}

bool AccessibilityObject::isLoaded() const
{
    auto* document = this->document();
    return document && !document->parser();
}

bool AccessibilityObject::isInlineText() const
{
    return is<RenderInline>(renderer());
}

bool AccessibilityObject::supportsKeyShortcuts() const
{
    return hasAttribute(aria_keyshortcutsAttr);
}

String AccessibilityObject::keyShortcuts() const
{
    return getAttribute(aria_keyshortcutsAttr);
}

Element* AccessibilityObject::element() const
{
    return dynamicDowncast<Element>(node());
}

const RenderStyle* AccessibilityObject::style() const
{
    const RenderStyle* style = nullptr;
    if (auto* renderer = this->renderer())
        style = &renderer->style();
    if (!style) {
        if (auto* element = this->element())
            style = element->computedStyle();
    }
    return style;
}

bool AccessibilityObject::isValueAutofillAvailable() const
{
    if (!isNativeTextControl())
        return false;

    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    return input && (input->isAutoFillAvailable() || input->autoFillButtonType() != AutoFillButtonType::None);
}

AutoFillButtonType AccessibilityObject::valueAutofillButtonType() const
{
    if (!isValueAutofillAvailable())
        return AutoFillButtonType::None;
    
    return downcast<HTMLInputElement>(*this->node()).autoFillButtonType();
}

bool AccessibilityObject::isSelected() const
{
    if (!renderer() && !node())
        return false;

    if (equalLettersIgnoringASCIICase(getAttribute(aria_selectedAttr), "true"_s))
        return true;

    if (isTabItem() && isTabItemSelected())
        return true;

    // Menu items are considered selectable by assistive technologies
    if (isMenuItem()) {
        if (isFocused())
            return true;
        WeakPtr parent = parentObjectUnignored();
        return parent && parent->activeDescendant() == this;
    }

    return false;
}

bool AccessibilityObject::isTabItemSelected() const
{
    if (!isTabItem() || (!renderer() && !node()))
        return false;

    WeakPtr node = this->node();
    if (!node || !node->isElementNode())
        return false;

    // The ARIA spec says a tab item can also be selected if it is aria-labeled by a tabpanel
    // that has keyboard focus inside of it, or if a tabpanel in its aria-controls list has KB
    // focus inside of it.
    auto* focusedElement = focusedUIElement();
    if (!focusedElement)
        return false;

    auto* cache = axObjectCache();
    if (!cache)
        return false;

    auto elements = elementsFromAttribute(aria_controlsAttr);
    for (auto& element : elements) {
        auto* tabPanel = cache->getOrCreate(element.ptr());

        // A tab item should only control tab panels.
        if (!tabPanel || tabPanel->roleValue() != AccessibilityRole::TabPanel)
            continue;

        auto* checkFocusElement = focusedElement;
        // Check if the focused element is a descendant of the element controlled by the tab item.
        while (checkFocusElement) {
            if (tabPanel == checkFocusElement)
                return true;
            checkFocusElement = checkFocusElement->parentObject();
        }
    }
    return false;
}

unsigned AccessibilityObject::textLength() const
{
    ASSERT(isTextControl());
    return text().length();
}

std::optional<String> AccessibilityObject::textContent() const
{
    if (!hasTextContent())
        return std::nullopt;

    std::optional<SimpleRange> range;
    if (isTextControl())
        range = rangeForCharacterRange({ 0, text().length() });
    else
        range = simpleRange();
    if (range)
        return stringForRange(*range);
    return std::nullopt;
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
    
bool AccessibilityObject::supportsARIAAttributes() const
{
    // This returns whether the element supports any global ARIA attributes.
    return supportsLiveRegion()
        || supportsDragging()
        || supportsDropping()
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
    
AccessibilityObject* AccessibilityObject::elementAccessibilityHitTest(const IntPoint& point) const
{ 
    // Send the hit test back into the sub-frame if necessary.
    if (isAttachment()) {
        Widget* widget = widgetForAttachmentView();
        // Normalize the point for the widget's bounds.
        if (widget && widget->isLocalFrameView()) {
            if (AXObjectCache* cache = axObjectCache())
                return cache->getOrCreate(widget)->accessibilityHitTest(IntPoint(point - widget->frameRect().location()));
        }
    }
    
    // Check if there are any mock elements that need to be handled.
    for (const auto& child : m_children) {
        if (auto* mockChild = dynamicDowncast<AccessibilityMockObject>(child.get()); mockChild && mockChild->elementRect().contains(point))
            return mockChild->elementAccessibilityHitTest(point);
    }

    return const_cast<AccessibilityObject*>(this);
}
    
AXObjectCache* AccessibilityObject::axObjectCache() const
{
    auto* document = this->document();
    return document ? document->axObjectCache() : nullptr;
}

AccessibilityObject* AccessibilityObject::focusedUIElement() const
{
    auto* page = this->page();
    auto* axObjectCache = this->axObjectCache();
    return page && axObjectCache ? axObjectCache->focusedObjectForPage(page) : nullptr;
}

void AccessibilityObject::setSelectedRows(AccessibilityChildrenVector&& selectedRows)
{
    // Setting selected only makes sense in trees and tables (and tree-tables).
    AccessibilityRole role = roleValue();
    if (role != AccessibilityRole::Tree && role != AccessibilityRole::TreeGrid && role != AccessibilityRole::Table && role != AccessibilityRole::Grid)
        return;

    bool isMulti = isMultiSelectable();
    for (const auto& selectedRow : selectedRows) {
        // FIXME: At the time of writing, setSelected is only implemented for AccessibilityListBoxOption and AccessibilityMenuListOption which are unlikely to be "rows", so this function probably isn't doing anything useful.
        selectedRow->setSelected(true);
        if (isMulti)
            break;
    }
}

void AccessibilityObject::setFocused(bool focus)
{
    if (focus) {
        // Ensure that the view is focused and active, otherwise, any attempt to set focus to an object inside it will fail.
        auto* frame = document() ? document()->frame() : nullptr;
        if (frame && frame->selection().isFocusedAndActive())
            return; // Nothing to do, already focused and active.

        auto* page = document() ? document()->page() : nullptr;
        if (!page)
            return;

        page->chrome().client().focus();
        // Reset the page pointer in case ChromeClient::focus() caused a side effect that invalidated our old one.
        page = document() ? document()->page() : nullptr;
        if (!page)
            return;

#if PLATFORM(IOS_FAMILY)
        // Mark the page as focused so the focus ring can be drawn immediately. The page is also marked
        // as focused as part assistiveTechnologyMakeFirstResponder, but that requires some back-and-forth
        // IPC between the web and UI processes, during which we can miss the drawing of the focus ring for the
        // first focused element. Making the page focused is a requirement for making the page selection focused.
        // This is iOS only until there's a demonstrated need for this preemptive focus on other platforms.
        if (!page->focusController().isFocused())
            page->focusController().setFocused(true);

        // Reset the page pointer in case FocusController::setFocused(true) caused a side effect that invalidated our old one.
        page = document() ? document()->page() : nullptr;
        if (!page)
            return;
#endif

#if PLATFORM(COCOA)
        auto* frameView = documentFrameView();
        if (!frameView)
            return;

        // Legacy WebKit1 case.
        if (frameView->platformWidget())
            page->chrome().client().makeFirstResponder((NSResponder *)frameView->platformWidget());
#endif
#if PLATFORM(MAC)
        else
            page->chrome().client().assistiveTechnologyMakeFirstResponder();
        // WebChromeClient::assistiveTechnologyMakeFirstResponder (the WebKit2 codepath) is intentionally
        // not called on iOS because stealing first-respondership causes issues such as:
        //   1. VoiceOver Speak Screen focus erroneously jumping to the top of the page when encountering an embedded WKWebView
        //   2. Third-party apps relying on WebKit to not steal first-respondership (https://bugs.webkit.org/show_bug.cgi?id=249976)
#endif
    }
}

AccessibilitySortDirection AccessibilityObject::sortDirection() const
{
    // Only objects that are descendant of column or row headers are allowed to have sort direction.
    auto* header = Accessibility::findAncestor<AccessibilityObject>(*this, true, [] (const AccessibilityObject& object) {
        auto role = object.roleValue();
        return role == AccessibilityRole::ColumnHeader || role == AccessibilityRole::RowHeader;
    });
    if (!header)
        return AccessibilitySortDirection::Invalid;

    auto& sortAttribute = header->getAttribute(aria_sortAttr);
    if (sortAttribute.isNull())
        return AccessibilitySortDirection::None;

    if (equalLettersIgnoringASCIICase(sortAttribute, "ascending"_s))
        return AccessibilitySortDirection::Ascending;
    if (equalLettersIgnoringASCIICase(sortAttribute, "descending"_s))
        return AccessibilitySortDirection::Descending;
    if (equalLettersIgnoringASCIICase(sortAttribute, "other"_s))
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
    auto& hasPopup = getAttribute(aria_haspopupAttr);
    if (hasPopup.isEmpty()) {
        // In ARIA 1.1, the implicit value for combobox became "listbox."
        if (isComboBox() || hasDatalist())
            return "listbox"_s;
        return "false"_s;
    }

    for (auto& value : { "menu"_s, "listbox"_s, "tree"_s, "grid"_s, "dialog"_s }) {
        // FIXME: Should fix ambiguity so we don't have to write "characters", but also don't create/destroy a String when passing an ASCIILiteral to equalIgnoringASCIICase.
        if (equalIgnoringASCIICase(hasPopup, value))
            return value;
    }

    // aria-haspopup specification states that true must be treated as menu.
    if (equalLettersIgnoringASCIICase(hasPopup, "true"_s))
        return "menu"_s;

    // The spec states that "User agents must treat any value of aria-haspopup that is not
    // included in the list of allowed values, including an empty string, as if the value
    // false had been provided."
    return "false"_s;
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
    return getIntegralAttribute(aria_setsizeAttr);
}

int AccessibilityObject::posInSet() const
{
    return getIntegralAttribute(aria_posinsetAttr);
}

String AccessibilityObject::identifierAttribute() const
{
    return getAttribute(idAttr);
}

Vector<String> AccessibilityObject::classList() const
{
    auto* element = this->element();
    if (!element)
        return { };

    auto& domClassList = element->classList();
    Vector<String> classList;
    unsigned length = domClassList.length();
    classList.reserveInitialCapacity(length);
    for (unsigned k = 0; k < length; k++)
        classList.append(domClassList.item(k).string());
    return classList;
}

String AccessibilityObject::extendedDescription() const
{
    auto describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;

    return getAttribute(HTMLNames::aria_descriptionAttr);
}

bool AccessibilityObject::supportsPressed() const
{
    const AtomString& expanded = getAttribute(aria_pressedAttr);
    return equalLettersIgnoringASCIICase(expanded, "true"_s) || equalLettersIgnoringASCIICase(expanded, "false"_s);
}

bool AccessibilityObject::supportsExpanded() const
{
    // If this object can toggle an HTML popover, it supports the reporting of its expanded state (which is based on the expanded / collapsed state of that popover).
    if (popoverTargetElement())
        return true;

    switch (roleValue()) {
    case AccessibilityRole::Details:
        return true;
    case AccessibilityRole::Button:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ColumnHeader:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::GridCell:
    case AccessibilityRole::Link:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::Row:
    case AccessibilityRole::RowHeader:
    case AccessibilityRole::Switch:
    case AccessibilityRole::Tab:
    case AccessibilityRole::TreeItem:
    case AccessibilityRole::WebApplication: {
        // Undefined values should not result in this attribute being exposed to ATs according to ARIA.
        const AtomString& expanded = getAttribute(aria_expandedAttr);
        return equalLettersIgnoringASCIICase(expanded, "true"_s) || equalLettersIgnoringASCIICase(expanded, "false"_s);
    }
    default:
        return false;
    }
}

double AccessibilityObject::loadingProgress() const
{
    if (isLoaded())
        return 1.0;

    auto* page = this->page();
    if (!page)
        return 0.0;

    return page->progress().estimatedProgress();
}

bool AccessibilityObject::isExpanded() const
{
    if (RefPtr details = dynamicDowncast<HTMLDetailsElement>(node()))
        return details->hasAttribute(openAttr);
    
    // Summary element should use its details parent's expanded status.
    if (isSummary()) {
        if (const AccessibilityObject* parent = Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const AccessibilityObject& object) {
            return is<HTMLDetailsElement>(object.node());
        }))
            return parent->isExpanded();
    }

    if (supportsExpanded()) {
        if (RefPtr popoverTargetElement = this->popoverTargetElement())
            return popoverTargetElement->isPopoverShowing();
        return equalLettersIgnoringASCIICase(getAttribute(aria_expandedAttr), "true"_s);
    }

    return false;  
}

bool AccessibilityObject::supportsChecked() const
{
    switch (roleValue()) {
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Switch:
        return true;
    default:
        return false;
    }
}

bool AccessibilityObject::supportsRowCountChange() const
{
    switch (roleValue()) {
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
    case AccessibilityRole::Grid:
    case AccessibilityRole::Table:
        return true;
    default:
        return false;
    }
}

AccessibilityButtonState AccessibilityObject::checkboxOrRadioValue() const
{
    // If this is a real checkbox or radio button, AccessibilityNodeObject will handle.
    // If it's an ARIA checkbox, radio, or switch the aria-checked attribute should be used.
    // If it's a toggle button, the aria-pressed attribute is consulted.

    if (isToggleButton()) {
        const AtomString& ariaPressed = getAttribute(aria_pressedAttr);
        if (equalLettersIgnoringASCIICase(ariaPressed, "true"_s))
            return AccessibilityButtonState::On;
        if (equalLettersIgnoringASCIICase(ariaPressed, "mixed"_s))
            return AccessibilityButtonState::Mixed;
        return AccessibilityButtonState::Off;
    }
    
    const AtomString& result = getAttribute(aria_checkedAttr);
    if (equalLettersIgnoringASCIICase(result, "true"_s))
        return AccessibilityButtonState::On;
    if (equalLettersIgnoringASCIICase(result, "mixed"_s)) {
        // ARIA says that radio, menuitemradio, and switch elements must NOT expose button state mixed.
        AccessibilityRole ariaRole = ariaRoleAttribute();
        if (ariaRole == AccessibilityRole::RadioButton || ariaRole == AccessibilityRole::MenuItemRadio || ariaRole == AccessibilityRole::Switch)
            return AccessibilityButtonState::Off;
        return AccessibilityButtonState::Mixed;
    }
    
    return AccessibilityButtonState::Off;
}

HashMap<String, AXEditingStyleValueVariant> AccessibilityObject::resolvedEditingStyles() const
{
    auto document = this->document();
    if (!document)
        return { };
    
    auto selectionStyle = EditingStyle::styleAtSelectionStart(document->selection().selection());
    if (!selectionStyle)
        return { };

    HashMap<String, AXEditingStyleValueVariant> styles;
    styles.add("bold"_s, selectionStyle->hasStyle(CSSPropertyFontWeight, "bold"_s));
    styles.add("italic"_s, selectionStyle->hasStyle(CSSPropertyFontStyle, "italic"_s));
    styles.add("underline"_s, selectionStyle->hasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"_s));
    styles.add("fontsize"_s, selectionStyle->legacyFontSize(*document));
    return styles;
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

bool AccessibilityObject::isOnScreen() const
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
        const IntRect innerRect = snappedIntRect(inner->isScrollView() ? inner->parentObject()->boundingBoxRect() : inner->boundingBoxRect());
        
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
        LocalFrameView::scrollRectToVisible(boundingBoxRect(), *renderer, false, options);
}

void AccessibilityObject::scrollToMakeVisibleWithSubFocus(IntRect&& subfocus) const
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
    IntRect newElementRect = snappedIntRect(elementRect());
    IntRect scrollParentRect = snappedIntRect(scrollParent->elementRect());
    subfocus.move(newElementRect.x(), newElementRect.y());
    subfocus.move(-scrollParentRect.x(), -scrollParentRect.y());

    // Recursively make sure the scroll parent itself is visible.
    if (scrollParent->parentObject())
        scrollParent->scrollToMakeVisibleWithSubFocus(WTFMove(subfocus));
}

FloatRect AccessibilityObject::unobscuredContentRect() const
{
    auto document = this->document();
    if (!document || !document->view())
        return { };
    return FloatRect(snappedIntRect(document->view()->unobscuredContentRect()));
}

void AccessibilityObject::scrollToGlobalPoint(IntPoint&& point) const
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
    size_t levels = objects.size() - 1;
    for (size_t i = 0; i < levels; i++) {
        const AccessibilityObject* outer = objects[i];
        const AccessibilityObject* inner = objects[i + 1];

        ScrollableArea* scrollableArea = outer->getScrollableAreaIfScrollable();

        LayoutRect innerRect = inner->isScrollView() ? inner->parentObject()->boundingBoxRect() : inner->boundingBoxRect();
        LayoutRect objectRect = innerRect;
        IntPoint scrollPosition = scrollableArea->scrollPosition();

        // Convert the object rect into local coordinates.
        objectRect.move(offsetX, offsetY);
        if (!outer->isScrollView())
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

        if (outer->isScrollView() && !inner->isScrollView()) {
            // If outer object we just scrolled is a scroll view (main window or iframe) but the
            // inner object is not, keep track of the coordinate transformation to apply to
            // future nested calculations.
            scrollPosition = scrollableArea->scrollPosition();
            offsetX -= (scrollPosition.x() + point.x());
            offsetY -= (scrollPosition.y() + point.y());
            point.move(scrollPosition.x() - innerRect.x(),
                       scrollPosition.y() - innerRect.y());
        } else if (inner->isScrollView()) {
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

void AccessibilityObject::setLastKnownIsIgnoredValue(bool isIgnored)
{
    m_lastKnownIsIgnoredValue = isIgnored ? AccessibilityObjectInclusion::IgnoreObject : AccessibilityObjectInclusion::IncludeObject;
}

bool AccessibilityObject::ignoredFromPresentationalRole() const
{
    return roleValue() == AccessibilityRole::Presentational || inheritsPresentationalRole();
}

bool AccessibilityObject::pressedIsPresent() const
{
    return !getAttribute(aria_pressedAttr).isEmpty();
}

TextIteratorBehaviors AccessibilityObject::textIteratorBehaviorForTextRange() const
{
    TextIteratorBehaviors behaviors { TextIteratorBehavior::IgnoresStyleVisibility };

#if USE(ATSPI)
    // We need to emit replaced elements for ATSPI, and present
    // them with the 'object replacement character' (0xFFFC).
    behaviors.add(TextIteratorBehavior::EmitsObjectReplacementCharacters);
#endif
    
    return behaviors;
}
    
AccessibilityRole AccessibilityObject::buttonRoleType() const
{
    // If aria-pressed is present, then it should be exposed as a toggle button.
    // https://www.w3.org/TR/wai-aria#aria-pressed
    if (pressedIsPresent())
        return AccessibilityRole::ToggleButton;
    if (hasPopup())
        return AccessibilityRole::PopUpButton;
    // We don't contemplate AccessibilityRole::RadioButton, as it depends on the input
    // type.

    return AccessibilityRole::Button;
}

bool AccessibilityObject::isFileUploadButton() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    return input && input->isFileUpload();
}

bool AccessibilityObject::accessibilityIsIgnoredByDefault() const
{
    return defaultObjectInclusion() == AccessibilityObjectInclusion::IgnoreObject;
}

bool AccessibilityObject::isARIAHidden() const
{
    return equalLettersIgnoringASCIICase(getAttribute(aria_hiddenAttr), "true"_s) && !isFocused();
}

// ARIA component of hidden definition.
// https://www.w3.org/TR/wai-aria/#dfn-hidden
bool AccessibilityObject::isAXHidden() const
{
    if (isFocused())
        return false;
    
    return Accessibility::findAncestor<AccessibilityObject>(*this, true, [] (const AccessibilityObject& object) {
        return object.isARIAHidden();
    }) != nullptr;
}

// DOM component of hidden definition.
// https://www.w3.org/TR/wai-aria/#dfn-hidden
bool AccessibilityObject::isDOMHidden() const
{
    if (auto* style = this->style())
        return style->display() == DisplayType::None || style->visibility() != Visibility::Visible;
    return true;
}

bool AccessibilityObject::isShowingValidationMessage() const
{
    if (RefPtr element = this->element()) {
        if (auto* listedElement = element->asValidatedFormListedElement())
            return listedElement->isShowingValidationMessage();
    }
    return false;
}

String AccessibilityObject::validationMessage() const
{
    if (RefPtr element = this->element()) {
        if (auto* listedElement = element->asValidatedFormListedElement())
            return listedElement->validationMessage();
    }
    return String();
}

AccessibilityObjectInclusion AccessibilityObject::defaultObjectInclusion() const
{
    if (auto* style = this->style()) {
        if (style->effectiveInert())
            return AccessibilityObjectInclusion::IgnoreObject;
        if (style->visibility() != Visibility::Visible) {
            // aria-hidden is meant to override visibility as the determinant in AX hierarchy inclusion.
            if (equalLettersIgnoringASCIICase(getAttribute(aria_hiddenAttr), "false"_s))
                return AccessibilityObjectInclusion::DefaultBehavior;

            return AccessibilityObjectInclusion::IgnoreObject;
        }
    }

    bool useParentData = !m_isIgnoredFromParentData.isNull();
    if (useParentData && (m_isIgnoredFromParentData.isAXHidden || m_isIgnoredFromParentData.isPresentationalChildOfAriaRole))
        return AccessibilityObjectInclusion::IgnoreObject;

    if (isARIAHidden())
        return AccessibilityObjectInclusion::IgnoreObject;

    bool ignoreARIAHidden = isFocused();
    if (Accessibility::findAncestor<AccessibilityObject>(*this, false, [&] (const auto& object) {
        return (!ignoreARIAHidden && object.isARIAHidden()) || object.ariaRoleHasPresentationalChildren() || !object.canHaveChildren();
    }))
        return AccessibilityObjectInclusion::IgnoreObject;

    // Include <dialog> elements and elements with role="dialog".
    if (roleValue() == AccessibilityRole::ApplicationDialog)
        return AccessibilityObjectInclusion::IncludeObject;

    return accessibilityPlatformIncludesObject();
}
    
bool AccessibilityObject::accessibilityIsIgnored() const
{
    AXComputedObjectAttributeCache* attributeCache = nullptr;
    auto* axObjectCache = this->axObjectCache();
    if (axObjectCache)
        attributeCache = axObjectCache->computedObjectAttributeCache();
    
    if (attributeCache) {
        AccessibilityObjectInclusion ignored = attributeCache->getIgnored(objectID());
        switch (ignored) {
        case AccessibilityObjectInclusion::IgnoreObject:
            return true;
        case AccessibilityObjectInclusion::IncludeObject:
            return false;
        case AccessibilityObjectInclusion::DefaultBehavior:
            break;
        }
    }

    bool ignored = accessibilityIsIgnoredWithoutCache(axObjectCache);

    // Refetch the attribute cache in case it was enabled as part of computing accessibilityIsIgnored.
    if (axObjectCache && (attributeCache = axObjectCache->computedObjectAttributeCache()))
        attributeCache->setIgnored(objectID(), ignored ? AccessibilityObjectInclusion::IgnoreObject : AccessibilityObjectInclusion::IncludeObject);

    return ignored;
}

bool AccessibilityObject::accessibilityIsIgnoredWithoutCache(AXObjectCache* cache) const
{
    // If we are in the midst of retrieving the current modal node, we only need to consider whether the object
    // is inherently ignored via computeAccessibilityIsIgnored. Also, calling ignoredFromModalPresence
    // in this state would cause infinite recursion.
    bool ignored = cache && cache->isRetrievingCurrentModalNode() ? false : ignoredFromModalPresence();
    if (!ignored)
        ignored = computeAccessibilityIsIgnored();

    auto previousLastKnownIsIgnoredValue = m_lastKnownIsIgnoredValue;
    const_cast<AccessibilityObject*>(this)->setLastKnownIsIgnoredValue(ignored);

    if (cache
        && ((previousLastKnownIsIgnoredValue == AccessibilityObjectInclusion::IgnoreObject && !ignored)
        || (previousLastKnownIsIgnoredValue == AccessibilityObjectInclusion::IncludeObject && ignored)))
        cache->childrenChanged(parentObject());

    return ignored;
}

Vector<Ref<Element>> AccessibilityObject::elementsFromAttribute(const QualifiedName& attribute) const
{
    RefPtr element = dynamicDowncast<Element>(node());
    if (!element)
        return { };

    if (document()->settings().ariaReflectionForElementReferencesEnabled()) {
        if (Element::isElementReflectionAttribute(document()->settings(), attribute)) {
            if (RefPtr reflectedElement = element->getElementAttribute(attribute)) {
                Vector<Ref<Element>> elements;
                elements.append(reflectedElement.releaseNonNull());
                return elements;
            }
        } else if (Element::isElementsArrayReflectionAttribute(document()->settings(), attribute)) {
            if (auto reflectedElements = element->getElementsArrayAttribute(attribute)) {
                return WTF::map(reflectedElements.value(), [](RefPtr<Element> element) -> Ref<Element> {
                    return *element;
                });
            }
        }
    }

    auto& idsString = getAttribute(attribute);
    if (idsString.isEmpty()) {
        if (auto* defaultARIA = element->customElementDefaultARIAIfExists()) {
            return WTF::map(defaultARIA->elementsForAttribute(*element, attribute), [](RefPtr<Element> element) -> Ref<Element> {
                return *element;
            });
        }
        return { };
    }

    Vector<Ref<Element>> elements;
    auto& treeScope = element->treeScope();
    SpaceSplitString spaceSplitString(idsString, SpaceSplitString::ShouldFoldCase::No);
    size_t length = spaceSplitString.size();
    for (size_t i = 0; i < length; ++i) {
        if (RefPtr element = treeScope.getElementById(spaceSplitString[i]))
            elements.append(element.releaseNonNull());
    }
    return elements;
}

#if PLATFORM(COCOA)
bool AccessibilityObject::preventKeyboardDOMEventDispatch() const
{
    auto* frame = this->frame();
    return frame && frame->settings().preventKeyboardDOMEventDispatch();
}

void AccessibilityObject::setPreventKeyboardDOMEventDispatch(bool on)
{
    auto* frame = this->frame();
    if (!frame)
        return;
    frame->settings().setPreventKeyboardDOMEventDispatch(on);
}
#endif

AccessibilityObject* AccessibilityObject::radioGroupAncestor() const
{
    return Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const AccessibilityObject& object) {
        return object.isRadioGroup();
    });
}

AtomString AccessibilityObject::tagName() const
{
    if (Element* element = this->element())
        return element->localName();

    return nullAtom();
}

bool AccessibilityObject::isStyleFormatGroup() const
{
    if (isCode())
        return true;

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
    
bool AccessibilityObject::isContainedBySecureField() const
{
    Node* node = this->node();
    if (!node)
        return false;
    
    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    RefPtr input = dynamicDowncast<HTMLInputElement>(node->shadowHost());
    return input && input->isSecureField();
}

AXCoreObject::AccessibilityChildrenVector AccessibilityObject::ariaSelectedRows()
{
    bool isMulti = isMultiSelectable();

    AccessibilityChildrenVector result;
    // Prefer active descendant over aria-selected.
    auto* activeDescendant = this->activeDescendant();
    if (activeDescendant && (activeDescendant->isTreeItem() || activeDescendant->isTableRow())) {
        result.append(activeDescendant);
        if (!isMulti)
            return result;
    }

    auto rowsIteration = [&](const auto& rows) {
        for (auto& rowCoreObject : rows) {
            auto* row = dynamicDowncast<AccessibilityObject>(rowCoreObject.get());
            if (!row)
                continue;

            if (row->isSelected() || row->isActiveDescendantOfFocusedContainer()) {
                result.append(row);
                if (!isMulti)
                    break;
            }
        }
    };

    if (isTree()) {
        AccessibilityChildrenVector allRows;
        ariaTreeRows(allRows);
        rowsIteration(allRows);
    } else if (auto* axTable = dynamicDowncast<AccessibilityTable>(this)) {
        if (axTable->supportsSelectedRows() && axTable->isExposable())
            rowsIteration(axTable->rows());
    }
    return result;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityObject::ariaListboxSelectedChildren()
{
    bool isMulti = isMultiSelectable();

    AccessibilityChildrenVector result;
    for (const auto& child : children()) {
        auto* axChild = dynamicDowncast<AccessibilityObject>(child.get());
        // Every child should have aria-role option, and if so, check for selected attribute/state.
        if (!axChild || axChild->ariaRoleAttribute() != AccessibilityRole::ListBoxOption)
            continue;

        if (axChild->isSelected() || axChild->isActiveDescendantOfFocusedContainer()) {
            result.append(axChild);
            if (!isMulti)
                return result;
        }
    }
    return result;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityObject::selectedChildren()
{
    if (!canHaveSelectedChildren())
        return { };

    AccessibilityChildrenVector result;
    switch (roleValue()) {
    case AccessibilityRole::ListBox:
        // native list boxes would be AccessibilityListBoxes, so only check for aria list boxes
        result = ariaListboxSelectedChildren();
        break;
    case AccessibilityRole::Grid:
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
        result = ariaSelectedRows();
        break;
    case AccessibilityRole::TabList:
        if (auto* selectedTab = selectedTabItem())
            result = { selectedTab };
        break;
    case AccessibilityRole::List:
        if (auto* selectedListItemChild = selectedListItem())
            result = { selectedListItemChild };
        break;
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
        if (auto* descendant = activeDescendant())
            result = { descendant };
        else if (auto* focusedElement = focusedUIElement())
            result = { focusedElement };
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return result;
}

AccessibilityObject* AccessibilityObject::selectedListItem()
{
    for (const auto& child : children()) {
        if (!child->isListItem())
            continue;

        auto* axObject = dynamicDowncast<AccessibilityObject>(child.get());
        if (!axObject)
            continue;

        if (axObject->isSelected() || axObject->isActiveDescendantOfFocusedContainer())
            return axObject;
    }
    
    return nullptr;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityObject::relatedObjects(AXRelationType relationType) const
{
    auto* cache = axObjectCache();
    if (!cache)
        return { };

    auto relatedObjectIDs = cache->relatedObjectIDsFor(*this, relationType);
    if (!relatedObjectIDs)
        return { };
    return cache->objectsForIDs(*relatedObjectIDs);
}

bool AccessibilityObject::shouldFocusActiveDescendant() const
{
    switch (ariaRoleAttribute()) {
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::Row:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::Meter:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::Toolbar:
    case AccessibilityRole::Tree:
    case AccessibilityRole::Grid:
    /* FIXME: replace these with actual roles when they are added to AccessibilityRole
    composite
    alert
    alertdialog
    status
    timer
    */
        return true;
    default:
        return false;
    }
}

bool AccessibilityObject::isActiveDescendantOfFocusedContainer() const
{
    auto containers = activeDescendantOfObjects();
    for (auto& container : containers) {
        if (container->isFocused())
            return true;
    }

    return false;
}

bool AccessibilityObject::ariaRoleHasPresentationalChildren() const
{
    switch (ariaRoleAttribute()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::Slider:
    case AccessibilityRole::Image:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::SpinButton:
        return true;
    default:
        return false;
    }
}

void AccessibilityObject::setIsIgnoredFromParentDataForChild(AccessibilityObject* child)
{
    if (!child)
        return;

    AccessibilityIsIgnoredFromParentData result = AccessibilityIsIgnoredFromParentData(this);
    if (!m_isIgnoredFromParentData.isNull()) {
        result.isAXHidden = (m_isIgnoredFromParentData.isAXHidden || equalLettersIgnoringASCIICase(child->getAttribute(aria_hiddenAttr), "true"_s)) && !child->isFocused();
        result.isPresentationalChildOfAriaRole = m_isIgnoredFromParentData.isPresentationalChildOfAriaRole || ariaRoleHasPresentationalChildren();
        result.isDescendantOfBarrenParent = m_isIgnoredFromParentData.isDescendantOfBarrenParent || !canHaveChildren();
    } else {
        if (child->isARIAHidden())
            result.isAXHidden = true;

        bool ignoreARIAHidden = child->isFocused();
        for (auto* object = child->parentObject(); object; object = object->parentObject()) {
            if (!result.isAXHidden && !ignoreARIAHidden && object->isARIAHidden())
                result.isAXHidden = true;

            if (!result.isPresentationalChildOfAriaRole && object->ariaRoleHasPresentationalChildren())
                result.isPresentationalChildOfAriaRole = true;

            if (!result.isDescendantOfBarrenParent && !object->canHaveChildren())
                result.isDescendantOfBarrenParent = true;
        }
    }

    child->setIsIgnoredFromParentData(result);
}

String AccessibilityObject::innerHTML() const
{
    auto* element = this->element();
    return element ? element->innerHTML() : String();
}

String AccessibilityObject::outerHTML() const
{
    auto* element = this->element();
    return element ? element->outerHTML() : String();
}

bool AccessibilityObject::ignoredByRowAncestor() const
{
    auto* ancestor = Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const AccessibilityObject& ancestor) {
        // If an object has a table cell ancestor (before a table row), that is a cell's contents, so don't ignore it.
        // Similarly, if an object has a table ancestor (before a row), that could be a row, row group or other container, so don't ignore it.
        return ancestor.isTableCell() || ancestor.isTableRow() || ancestor.isTable();
    });

    return ancestor && ancestor->isTableRow();
}

namespace Accessibility {

#if !PLATFORM(MAC) && !USE(ATSPI)
// FIXME: implement in other platforms.
PlatformRoleMap createPlatformRoleMap() { return PlatformRoleMap(); }
#endif

String roleToPlatformString(AccessibilityRole role)
{
    static NeverDestroyed<PlatformRoleMap> roleMap = createPlatformRoleMap();
    return roleMap->get(enumToUnderlyingType(role));
}

// This function determines if the given `axObject` is a radio button part of a different ad-hoc radio group
// than `referenceObject`, where ad-hoc radio group membership is determined by comparing `name` attributes.
static bool isRadioButtonInDifferentAdhocGroup(RefPtr<AXCoreObject> axObject, AXCoreObject* referenceObject)
{
    if (!axObject || !axObject->isRadioButton())
        return false;

    // If the `referenceObject` is not a radio button and this `axObject` is, their radio group membership is different because
    // `axObject` belongs to a group and `referenceObject` doesn't.
    if (!referenceObject || !referenceObject->isRadioButton())
        return true;

    return axObject->nameAttribute() != referenceObject->nameAttribute();
}

static bool isAccessibilityObjectSearchMatchAtIndex(RefPtr<AXCoreObject> axObject, const AccessibilitySearchCriteria& criteria, size_t index)
{
    switch (criteria.searchKeys[index]) {
    case AccessibilitySearchKey::AnyType:
        // The AccessibilitySearchKey::AnyType matches any non-null AccessibilityObject.
        return true;
    case AccessibilitySearchKey::Article:
        return axObject->roleValue() == AccessibilityRole::DocumentArticle;
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
            && axObject->roleValue() != criteria.startObject->roleValue();
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
#if ENABLE(AX_THREAD_TEXT_APIS)
    case AccessibilitySearchKey::HasTextRuns:
        return axObject->hasTextRuns();
#endif
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
        return axObject->isRadioGroup() || isRadioButtonInDifferentAdhocGroup(axObject, criteria.startObject);
    case AccessibilitySearchKey::SameType:
        return criteria.startObject
            && axObject->roleValue() == criteria.startObject->roleValue();
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
        return axObject->isUnvisited();
    case AccessibilitySearchKey::VisitedLink:
        return axObject->isVisited();
    default:
        return false;
    }
}

static bool isAccessibilityObjectSearchMatch(RefPtr<AXCoreObject> axObject, const AccessibilitySearchCriteria& criteria)
{
    if (!axObject)
        return false;

    size_t length = criteria.searchKeys.size();
    for (size_t i = 0; i < length; ++i) {
        if (isAccessibilityObjectSearchMatchAtIndex(axObject, criteria, i)) {
            if (criteria.visibleOnly && !axObject->isOnScreen())
                return false;
            return true;
        }
    }
    return false;
}

static bool isAccessibilityTextSearchMatch(RefPtr<AXCoreObject> axObject, const AccessibilitySearchCriteria& criteria)
{
    if (!axObject)
        return false;

    // If text is empty we return true.
    if (criteria.searchText.isEmpty())
        return true;

    return containsPlainText(axObject->title(), criteria.searchText, CaseInsensitive)
        || containsPlainText(axObject->description(), criteria.searchText, CaseInsensitive)
        || containsPlainText(axObject->stringValue(), criteria.searchText, CaseInsensitive);
}

static bool objectMatchesSearchCriteriaWithResultLimit(RefPtr<AXCoreObject> object, const AccessibilitySearchCriteria& criteria, AXCoreObject::AccessibilityChildrenVector& results)
{
    if (isAccessibilityObjectSearchMatch(object, criteria) && isAccessibilityTextSearchMatch(object, criteria)) {
        results.append(object);

        // Enough results were found to stop searching.
        if (results.size() >= criteria.resultsLimit)
            return true;
    }

    return false;
}

static void appendChildrenToArray(RefPtr<AXCoreObject> object, bool isForward, RefPtr<AXCoreObject> startObject, AccessibilityObject::AccessibilityChildrenVector& results)
{
    // A table's children includes elements whose own children are also the table's children (due to the way the Mac exposes tables).
    // The rows from the table should be queried, since those are direct descendants of the table, and they contain content.
    const auto& searchChildren = object->isTable() && object->isExposable() ? object->rows() : object->children();

    size_t childrenSize = searchChildren.size();

    size_t startIndex = isForward ? childrenSize : 0;
    size_t endIndex = isForward ? 0 : childrenSize;

    // If the startObject is ignored, we should use an accessible sibling as a start element instead.
    if (startObject && startObject->accessibilityIsIgnored() && startObject->isDescendantOfObject(object.get())) {
        RefPtr<AXCoreObject> parentObject = startObject->parentObject();
        // Go up the parent chain to find the highest ancestor that's also being ignored.
        while (parentObject && parentObject->accessibilityIsIgnored()) {
            if (parentObject == object)
                break;
            startObject = parentObject;
            parentObject = parentObject->parentObject();
        }

        // We should only ever hit this case with a live object (not an isolated object), as it would require startObject to be ignored,
        // and we should never have created an isolated object from an ignored live object.
        ASSERT(is<AccessibilityObject>(startObject));
        auto* newStartObject = dynamicDowncast<AccessibilityObject>(startObject.get());
        // Get the un-ignored sibling based on the search direction, and update the searchPosition.
        while (newStartObject && newStartObject->accessibilityIsIgnored())
            newStartObject = isForward ? newStartObject->previousSibling() : newStartObject->nextSibling();
        startObject = newStartObject;
    }

    size_t searchPosition = startObject ? searchChildren.find(startObject) : notFound;

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

void findMatchingObjects(const AccessibilitySearchCriteria& criteria, AXCoreObject::AccessibilityChildrenVector& results)
{
    AXTRACE("Accessibility::findMatchingObjects"_s);
    AXLOG(criteria);

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
    for (auto* stopSearchElement = criteria.anchorObject->parentObjectUnignored(); startObject && startObject != stopSearchElement; startObject = startObject->parentObjectUnignored()) {
        // Only append the children after/before the previous element, so that the search does not check elements that are
        // already behind/ahead of start element.
        AXCoreObject::AccessibilityChildrenVector searchStack;
        if (!criteria.immediateDescendantsOnly || startObject == criteria.anchorObject)
            appendChildrenToArray(startObject, isForward, previousObject, searchStack);

        // This now does a DFS at the current level of the parent.
        while (!searchStack.isEmpty()) {
            auto searchObject = searchStack.last();
            searchStack.removeLast();

            if (criteria.stopAtID.isValid() && searchObject->objectID() == criteria.stopAtID) {
                AXLOG(results);
                return;
            }

            if (objectMatchesSearchCriteriaWithResultLimit(searchObject, criteria, results))
                break;

            if (!criteria.immediateDescendantsOnly)
                appendChildrenToArray(searchObject, isForward, nullptr, searchStack);
        }

        if (results.size() >= criteria.resultsLimit)
            break;

        // When moving backwards, the parent object needs to be checked, because technically it's "before" the starting element.
        if (!isForward && startObject != criteria.anchorObject && objectMatchesSearchCriteriaWithResultLimit(startObject, criteria, results))
            break;

        previousObject = startObject;
    }

    AXLOG(results);
}

} // namespace Accessibility

} // namespace WebCore
