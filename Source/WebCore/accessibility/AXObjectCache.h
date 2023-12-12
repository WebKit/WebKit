/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AXGeometryManager.h"
#include "AXIsolatedTree.h"
#include "AXTextMarker.h"
#include "AXTextStateChangeIntent.h"
#include "AXTreeStore.h"
#include "AccessibilityObject.h"
#include "SimpleRange.h"
#include "Timer.h"
#include "VisibleUnits.h"
#include <limits.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class AccessibilityTable;
class AccessibilityTableCell;
class Document;
class HTMLAreaElement;
class HTMLTableElement;
class HTMLTextFormControlElement;
class Node;
class Page;
class RenderBlock;
class RenderObject;
class RenderText;
class RenderWidget;
class Scrollbar;
class ScrollView;
class VisiblePosition;
class Widget;

struct CharacterOffset {
    Node* node;
    int startIndex;
    int offset;
    int remainingOffset;
    
    CharacterOffset(Node* n = nullptr, int startIndex = 0, int offset = 0, int remaining = 0)
        : node(n)
        , startIndex(startIndex)
        , offset(offset)
        , remainingOffset(remaining)
    { }
    
    int remaining() const { return remainingOffset; }
    bool isNull() const { return !node; }
    bool isEqual(const CharacterOffset& other) const
    {
        if (isNull() || other.isNull())
            return false;
        return node == other.node && startIndex == other.startIndex && offset == other.offset;
    }

    String debugDescription()
    {
        return makeString("CharacterOffset {node: ", node ? node->debugDescription() : "null"_s, ", startIndex: ", startIndex, ", offset: ", offset, ", remainingOffset: ", remainingOffset, "}");
    }
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXComputedObjectAttributeCache);
class AXComputedObjectAttributeCache {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXComputedObjectAttributeCache);
public:
    AccessibilityObjectInclusion getIgnored(AXID) const;
    void setIgnored(AXID, AccessibilityObjectInclusion);

private:
    struct CachedAXObjectAttributes {
        CachedAXObjectAttributes()
            : ignored(AccessibilityObjectInclusion::DefaultBehavior)
        { }

        AccessibilityObjectInclusion ignored;
    };

    HashMap<AXID, CachedAXObjectAttributes> m_idMapping;
};

struct VisiblePositionIndex {
    int value = -1;
    RefPtr<ContainerNode> scope;
};

struct VisiblePositionIndexRange {
    VisiblePositionIndex startIndex;
    VisiblePositionIndex endIndex;
    bool isNull() const { return startIndex.value == -1 || endIndex.value == -1; }
};

struct AXTreeData {
    String liveTree;
    String isolatedTree;
};

class AccessibilityReplacedText {
public:
    AccessibilityReplacedText() = default;
    AccessibilityReplacedText(const VisibleSelection&);
    void postTextStateChangeNotification(AXObjectCache*, AXTextEditType, const String&, const VisibleSelection&);
    const VisiblePositionIndexRange& replacedRange() { return m_replacedRange; }
protected:
    String m_replacedText;
    VisiblePositionIndexRange m_replacedRange;
};

#if !PLATFORM(COCOA)
enum AXTextChange { AXTextInserted, AXTextDeleted, AXTextAttributesChanged };
#endif

enum class PostTarget { Element, ObservableParent };

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXObjectCache);
class AXObjectCache : public CanMakeWeakPtr<AXObjectCache>, public CanMakeCheckedPtr
    , public AXTreeStore<AXObjectCache> {
    WTF_MAKE_NONCOPYABLE(AXObjectCache);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXObjectCache);
    friend class AXIsolatedTree;
    friend class AXTextMarker;
    friend WTF::TextStream& operator<<(WTF::TextStream&, AXObjectCache&);
public:
    explicit AXObjectCache(Document&);
    ~AXObjectCache();

    // Returns the root object for the entire document.
    WEBCORE_EXPORT AXCoreObject* rootObject();
    // Returns the root object for a specific frame.
    WEBCORE_EXPORT AccessibilityObject* rootObjectForFrame(LocalFrame*);

    // Creation/retrieval of AX objects associated with a DOM or RenderTree object.
    AccessibilityObject* getOrCreate(RenderObject*);
    AccessibilityObject* getOrCreate(Widget*);
    enum class IsPartOfRelation : bool { No, Yes };
    WEBCORE_EXPORT AccessibilityObject* getOrCreate(Node*, IsPartOfRelation = IsPartOfRelation::No);

    // used for objects without backing elements
    AccessibilityObject* create(AccessibilityRole);
    
    // will only return the AccessibilityObject if it already exists
    AccessibilityObject* get(RenderObject*);
    AccessibilityObject* get(Widget*);
    AccessibilityObject* get(Node*);

    void remove(RenderObject*);
    void remove(Node&);
    void remove(Widget*);
    void remove(AXID);

#if !PLATFORM(COCOA) && !USE(ATSPI)
    void detachWrapper(AXCoreObject*, AccessibilityDetachmentType);
#endif
private:
    using DOMObjectVariant = std::variant<std::nullptr_t, RenderObject*, Node*, Widget*>;
    void cacheAndInitializeWrapper(AccessibilityObject*, DOMObjectVariant = nullptr);
    void attachWrapper(AccessibilityObject*);

public:
    void onPageActivityStateChange(OptionSet<ActivityState>);
    void setPageActivityState(OptionSet<ActivityState> state) { m_pageActivityState = state; }
    OptionSet<ActivityState> pageActivityState() const { return m_pageActivityState; }

    void childrenChanged(Node*, Node* newChild = nullptr);
    void childrenChanged(RenderObject*, RenderObject* newChild = nullptr);
    void childrenChanged(AccessibilityObject*);
    void onFocusChange(Node* oldFocusedNode, Node* newFocusedNode);
    void onPopoverToggle(const HTMLElement&);
    void onScrollbarFrameRectChange(const Scrollbar&);
    void onSelectedChanged(Node*);
    void onTextSecurityChanged(HTMLInputElement&);
    void onTitleChange(Document&);
    void onValidityChange(Element&);
    void onTextCompositionChange(Node&, CompositionState, bool, const String&, size_t, bool);
    void onWidgetVisibilityChanged(RenderWidget*);
    void valueChanged(Element*);
    void checkedStateChanged(Node*);
    void autofillTypeChanged(Node*);
    void handleRoleChanged(AccessibilityObject*);
    // Called when a RenderObject is created for an Element. Depending on the
    // presence of a RenderObject, we may have instatiated an AXRenderObject or
    // an AXNodeObject. This occurs when an Element with no renderer is
    // re-parented into a subtree that does have a renderer.
    void onRendererCreated(Element&);
#if PLATFORM(MAC)
    void onSelectedTextChanged(const VisiblePositionRange&);
#endif

    void updateLoadingProgress(double);
    void loadingFinished() { updateLoadingProgress(1); }
    double loadingProgress() const { return m_loadingProgress; }

    struct AttributeChange {
        WeakPtr<Element, WeakPtrImplWithEventTargetData> element { nullptr };
        QualifiedName attrName;
        AtomString oldValue;
        AtomString newValue;
    };
    using DeferredCollection = std::variant<HashMap<Element*, String>
        , HashSet<AXID>
        , ListHashSet<Node*>
        , ListHashSet<RefPtr<AccessibilityObject>>
        , Vector<AttributeChange>
        , Vector<std::pair<Node*, Node*>>
        , WeakHashSet<Element, WeakPtrImplWithEventTargetData>
        , WeakHashSet<HTMLTableElement, WeakPtrImplWithEventTargetData>
        , WeakHashSet<AccessibilityTable>
        , WeakHashSet<AccessibilityTableCell>
        , WeakListHashSet<Node, WeakPtrImplWithEventTargetData>
        , WeakHashMap<Element, String, WeakPtrImplWithEventTargetData>>;
    void deferFocusedUIElementChangeIfNeeded(Node* oldFocusedNode, Node* newFocusedNode);
    void deferModalChange(Element*);
    void deferMenuListValueChange(Element*);
    void deferNodeAddedOrRemoved(Node*);
    void handleScrolledToAnchor(const Node* anchorNode);
    void onScrollbarUpdate(ScrollView*);

    bool isRetrievingCurrentModalNode() { return m_isRetrievingCurrentModalNode; }
    Node* modalNode();

    void deferAttributeChangeIfNeeded(Element*, const QualifiedName&, const AtomString&, const AtomString&);
    void recomputeIsIgnored(RenderObject*);
    void recomputeIsIgnored(Node*);

    WEBCORE_EXPORT static void enableAccessibility();
    WEBCORE_EXPORT static void disableAccessibility();
    static bool forceDeferredSpellChecking() { return gForceDeferredSpellChecking; }
    WEBCORE_EXPORT static void setForceDeferredSpellChecking(bool);
#if PLATFORM(MAC)
    static bool shouldSpellCheck();
#else
    static bool shouldSpellCheck() { return true; }
#endif

    WEBCORE_EXPORT AccessibilityObject* focusedObjectForPage(const Page*);

    // Enhanced user interface accessibility can be toggled by the assistive technology.
    WEBCORE_EXPORT static void setEnhancedUserInterfaceAccessibility(bool flag);

    // Note: these may be called from a non-main thread concurrently as other readers.
    static bool accessibilityEnabled() { return gAccessibilityEnabled; }
    static bool accessibilityEnhancedUserInterfaceEnabled() { return gAccessibilityEnhancedUserInterfaceEnabled; }

    const Element* rootAXEditableElement(const Node*);
    bool nodeIsTextControl(const Node*);

    AccessibilityObject* objectForID(const AXID id) const { return m_objects.get(id); }
    template<typename U> Vector<RefPtr<AXCoreObject>> objectsForIDs(const U&) const;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void onPaint(const RenderObject&, IntRect&&) const;
    void onPaint(const Widget&, IntRect&&) const;
#else
    NO_RETURN_DUE_TO_ASSERT void onPaint(const RenderObject&, IntRect&&) const { ASSERT_NOT_REACHED(); }
    NO_RETURN_DUE_TO_ASSERT void onPaint(const Widget&, IntRect&&) const { ASSERT_NOT_REACHED(); }
#endif

    // Text marker utilities.
    std::optional<TextMarkerData> textMarkerDataForVisiblePosition(const VisiblePosition&);
    std::optional<TextMarkerData> textMarkerDataForFirstPositionInTextControl(HTMLTextFormControlElement&);
    TextMarkerData textMarkerDataForCharacterOffset(const CharacterOffset&);
    TextMarkerData textMarkerDataForNextCharacterOffset(const CharacterOffset&);
    AXTextMarker nextTextMarker(const AXTextMarker&);
    TextMarkerData textMarkerDataForPreviousCharacterOffset(const CharacterOffset&);
    AXTextMarker previousTextMarker(const AXTextMarker&);
    VisiblePosition visiblePositionForTextMarkerData(const TextMarkerData&);
    CharacterOffset characterOffsetForTextMarkerData(TextMarkerData&);
    // Use ignoreNextNodeStart/ignorePreviousNodeEnd to determine the behavior when we are at node boundary.
    CharacterOffset nextCharacterOffset(const CharacterOffset&, bool ignoreNextNodeStart = true);
    CharacterOffset previousCharacterOffset(const CharacterOffset&, bool ignorePreviousNodeEnd = true);
    TextMarkerData startOrEndTextMarkerDataForRange(const SimpleRange&, bool);
    CharacterOffset startOrEndCharacterOffsetForRange(const SimpleRange&, bool, bool enterTextControls = false);
    AccessibilityObject* accessibilityObjectForTextMarkerData(TextMarkerData&);
    std::optional<SimpleRange> rangeForUnorderedCharacterOffsets(const CharacterOffset&, const CharacterOffset&);
    static SimpleRange rangeForNodeContents(Node&);
    static unsigned lengthForRange(const SimpleRange&);

    // Word boundary
    CharacterOffset nextWordEndCharacterOffset(const CharacterOffset&);
    CharacterOffset previousWordStartCharacterOffset(const CharacterOffset&);
    std::optional<SimpleRange> leftWordRange(const CharacterOffset&);
    std::optional<SimpleRange> rightWordRange(const CharacterOffset&);
    
    // Paragraph
    std::optional<SimpleRange> paragraphForCharacterOffset(const CharacterOffset&);
    CharacterOffset nextParagraphEndCharacterOffset(const CharacterOffset&);
    CharacterOffset previousParagraphStartCharacterOffset(const CharacterOffset&);
    
    // Sentence
    std::optional<SimpleRange> sentenceForCharacterOffset(const CharacterOffset&);
    CharacterOffset nextSentenceEndCharacterOffset(const CharacterOffset&);
    CharacterOffset previousSentenceStartCharacterOffset(const CharacterOffset&);
    
    // Bounds
    CharacterOffset characterOffsetForPoint(const IntPoint&, AXCoreObject*);
    IntRect absoluteCaretBoundsForCharacterOffset(const CharacterOffset&);
    CharacterOffset characterOffsetForBounds(const IntRect&, bool);
    
    // Lines
    CharacterOffset endCharacterOffsetOfLine(const CharacterOffset&);
    CharacterOffset startCharacterOffsetOfLine(const CharacterOffset&);
    
    // Index
    CharacterOffset characterOffsetForIndex(int, const AXCoreObject*);

    enum AXNotification {
        AXAccessKeyChanged,
        AXActiveDescendantChanged,
        AXAnnouncementRequested,
        AXAutocorrectionOccured,
        AXAutofillTypeChanged,
        AXCellSlotsChanged,
        AXCheckedStateChanged,
        AXChildrenChanged,
        AXColumnCountChanged,
        AXColumnIndexChanged,
        AXColumnSpanChanged,
        AXContentEditableAttributeChanged,
        AXControlledObjectsChanged,
        AXCurrentStateChanged,
        AXDescribedByChanged,
        AXDisabledStateChanged,
        AXDropEffectChanged,
        AXExtendedDescriptionChanged,
        AXFlowToChanged,
        AXFocusableStateChanged,
        AXFocusedUIElementChanged,
        AXFrameLoadComplete,
        AXGrabbedStateChanged,
        AXHasPopupChanged,
        AXIdAttributeChanged,
        AXImageOverlayChanged,
        AXIsAtomicChanged,
        AXKeyShortcutsChanged,
        AXLanguageChanged,
        AXLayoutComplete,
        AXLevelChanged,
        AXLoadComplete,
        AXNameChanged,
        AXNewDocumentLoadComplete,
        AXPageScrolled,
        AXPlaceholderChanged,
        AXPopoverTargetChanged,
        AXPositionInSetChanged,
        AXRoleChanged,
        AXRoleDescriptionChanged,
        AXRowIndexChanged,
        AXRowSpanChanged,
        AXCellScopeChanged,
        AXSelectedChildrenChanged,
        AXSelectedCellsChanged,
        AXSelectedStateChanged,
        AXSelectedTextChanged,
        AXSetSizeChanged,
        AXTableHeadersChanged,
        AXTextCompositionBegan,
        AXTextCompositionEnded,
        AXURLChanged,
        AXValueChanged,
        AXVisibilityChanged,
        AXScrolledToAnchor,
        AXLabelCreated,
        AXLiveRegionCreated,
        AXLiveRegionChanged,
        AXLiveRegionRelevantChanged,
        AXLiveRegionStatusChanged,
        AXMaximumValueChanged,
        AXMenuListItemSelected,
        AXMenuListValueChanged,
        AXMenuClosed,
        AXMenuOpened,
        AXMinimumValueChanged,
        AXMultiSelectableStateChanged,
        AXOrientationChanged,
        AXRowCountChanged,
        AXRowCollapsed,
        AXRowExpanded,
        AXExpandedChanged,
        AXInvalidStatusChanged,
        AXPressDidSucceed,
        AXPressDidFail,
        AXPressedStateChanged,
        AXReadOnlyStatusChanged,
        AXRequiredStatusChanged,
        AXSortDirectionChanged,
        AXTextChanged,
        AXTextCompositionChanged,
        AXTextSecurityChanged,
        AXElementBusyChanged,
        AXDraggingStarted,
        AXDraggingEnded,
        AXDraggingEnteredDropZone,
        AXDraggingDropped,
        AXDraggingExitedDropZone
    };

    void postNotification(RenderObject*, AXNotification, PostTarget = PostTarget::Element);
    void postNotification(Node*, AXNotification, PostTarget = PostTarget::Element);
    void postNotification(AccessibilityObject*, Document*, AXNotification, PostTarget = PostTarget::Element);
    // Requests clients to announce to the user the given message in the way they deem appropriate.
    WEBCORE_EXPORT void announce(const String&);

#ifndef NDEBUG
    void showIntent(const AXTextStateChangeIntent&);
#endif

    void setTextSelectionIntent(const AXTextStateChangeIntent&);
    void setIsSynchronizingSelection(bool);

    void postTextStateChangeNotification(Node*, AXTextEditType, const String&, const VisiblePosition&);
    void postTextReplacementNotification(Node*, AXTextEditType deletionType, const String& deletedText, AXTextEditType insertionType, const String& insertedText, const VisiblePosition&);
    void postTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String& deletedText, const String& insertedText);
    void postTextStateChangeNotification(Node*, const AXTextStateChangeIntent&, const VisibleSelection&);
    void postTextStateChangeNotification(const Position&, const AXTextStateChangeIntent&, const VisibleSelection&);
    void postLiveRegionChangeNotification(AccessibilityObject*);

    enum AXLoadingEvent {
        AXLoadingStarted,
        AXLoadingReloaded,
        AXLoadingFailed,
        AXLoadingFinished
    };

    void frameLoadingEventNotification(LocalFrame*, AXLoadingEvent);

    void prepareForDocumentDestruction(const Document&);

    void startCachingComputedObjectAttributesUntilTreeMutates();
    void stopCachingComputedObjectAttributes();

    AXComputedObjectAttributeCache* computedObjectAttributeCache() { return m_computedObjectAttributeCache.get(); }

    Document& document() const { return const_cast<Document&>(m_document.get()); }
    constexpr const std::optional<PageIdentifier>& pageID() const { return m_pageID; }

#if PLATFORM(MAC)
    static void setShouldRepostNotificationsForTests(bool);
#endif
    void deferRecomputeIsIgnoredIfNeeded(Element*);
    void deferRecomputeIsIgnored(Element*);
    void deferRecomputeTableIsExposed(Element*);
    void deferRecomputeTableCellSlots(AccessibilityTable&);
    void deferTextChangedIfNeeded(Node*);
    void deferSelectedChildrenChangedIfNeeded(Element&);
    void performDeferredCacheUpdate();
    void deferTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String& previousValue);

    std::optional<SimpleRange> rangeMatchesTextNearRange(const SimpleRange&, const String&);

    static ASCIILiteral notificationPlatformName(AXNotification);

    AXTreeData treeData();

    // Returns the IDs of the objects that relate to the given object with the specified relationship.
    std::optional<ListHashSet<AXID>> relatedObjectIDsFor(const AXCoreObject&, AXRelationType);
    void updateRelations(Element&, const QualifiedName&);

#if PLATFORM(IOS_FAMILY)
    void relayNotification(const String&, RetainPtr<NSData>);
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void scheduleObjectRegionsUpdate(bool scheduleImmediately = false) { m_geometryManager->scheduleObjectRegionsUpdate(scheduleImmediately); }
    void willUpdateObjectRegions() { m_geometryManager->willUpdateObjectRegions(); }
    WEBCORE_EXPORT static bool isIsolatedTreeEnabled();
    WEBCORE_EXPORT static void initializeAXThreadIfNeeded();
private:
    static bool clientSupportsIsolatedTree();
    static bool isTestClient();
    AXCoreObject* isolatedTreeRootObject();
    // Propagates the root of the isolated tree back into the Core and WebKit.
    void setIsolatedTreeRoot(AXCoreObject*);
    void setIsolatedTreeFocusedObject(AccessibilityObject*);
    RefPtr<AXIsolatedTree> getOrCreateIsolatedTree();
    void buildIsolatedTree();
    void updateIsolatedTree(AccessibilityObject&, AXNotification);
    void updateIsolatedTree(AccessibilityObject*, AXNotification);
    void updateIsolatedTree(const Vector<std::pair<RefPtr<AccessibilityObject>, AXNotification>>&);
    void updateIsolatedTree(AccessibilityObject*, AXPropertyName) const;
    void updateIsolatedTree(AccessibilityObject&, AXPropertyName) const;
    void startUpdateTreeSnapshotTimer();
#endif

protected:
    void postPlatformNotification(AXCoreObject*, AXNotification);
    void platformHandleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode);

    void platformPerformDeferredCacheUpdate();

#if PLATFORM(COCOA) || USE(ATSPI)
    void postTextStateChangePlatformNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&);
    void postTextStateChangePlatformNotification(AccessibilityObject*, AXTextEditType, const String&, const VisiblePosition&);
    void postTextReplacementPlatformNotificationForTextControl(AccessibilityObject*, const String& deletedText, const String& insertedText, HTMLTextFormControlElement&);
    void postTextReplacementPlatformNotification(AccessibilityObject*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&);
#else // PLATFORM(COCOA) || USE(ATSPI)
    static AXTextChange textChangeForEditType(AXTextEditType);
    void nodeTextChangePlatformNotification(AccessibilityObject*, AXTextChange, unsigned, const String&);
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT void postPlatformAnnouncementNotification(const String&);
#else
    void postPlatformAnnouncementNotification(const String&) { }
#endif

    void frameLoadingEventPlatformNotification(AccessibilityObject*, AXLoadingEvent);
    void handleLabelForChanged(HTMLLabelElement&, const AtomString& /* oldValue */);

    // This is a weak reference cache for knowing if Nodes used by TextMarkers are valid.
    void setNodeInUse(Node* n) { m_textMarkerNodes.add(n); }
    void removeNodeForUse(Node& n) { m_textMarkerNodes.remove(&n); }
    bool isNodeInUse(Node* n) { return m_textMarkerNodes.contains(n); }
    
    // CharacterOffset functions.
    enum TraverseOption { TraverseOptionDefault = 1 << 0, TraverseOptionToNodeEnd = 1 << 1, TraverseOptionIncludeStart = 1 << 2, TraverseOptionValidateOffset = 1 << 3, TraverseOptionDoNotEnterTextControls = 1 << 4 };
    Node* nextNode(Node*) const;
    Node* previousNode(Node*) const;
    CharacterOffset traverseToOffsetInRange(const SimpleRange&, int, TraverseOption = TraverseOptionDefault, bool stayWithinRange = false);
public:
    VisiblePosition visiblePositionFromCharacterOffset(const CharacterOffset&);
protected:
    CharacterOffset characterOffsetFromVisiblePosition(const VisiblePosition&);
    char32_t characterAfter(const CharacterOffset&);
    char32_t characterBefore(const CharacterOffset&);
    CharacterOffset characterOffsetForNodeAndOffset(Node&, int, TraverseOption = TraverseOptionDefault);

    enum class NeedsContextAtParagraphStart : bool { No, Yes };
    CharacterOffset previousBoundary(const CharacterOffset&, BoundarySearchFunction, NeedsContextAtParagraphStart = NeedsContextAtParagraphStart::No);
    CharacterOffset nextBoundary(const CharacterOffset&, BoundarySearchFunction);
    CharacterOffset startCharacterOffsetOfWord(const CharacterOffset&, WordSide = WordSide::RightWordIfOnBoundary);
    CharacterOffset endCharacterOffsetOfWord(const CharacterOffset&, WordSide = WordSide::RightWordIfOnBoundary);
    CharacterOffset startCharacterOffsetOfParagraph(const CharacterOffset&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
    CharacterOffset endCharacterOffsetOfParagraph(const CharacterOffset&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
    CharacterOffset startCharacterOffsetOfSentence(const CharacterOffset&);
    CharacterOffset endCharacterOffsetOfSentence(const CharacterOffset&);
    CharacterOffset characterOffsetForPoint(const IntPoint&);
    LayoutRect localCaretRectForCharacterOffset(RenderObject*&, const CharacterOffset&);
    bool shouldSkipBoundary(const CharacterOffset&, const CharacterOffset&);
private:
    AccessibilityObject* rootWebArea();

    // The AX focus is more finegrained than the notion of focused Node. This method handles those cases where the focused AX object is a descendant or a sub-part of the focused Node.
    AccessibilityObject* focusedObjectForNode(Node*);
    static AccessibilityObject* focusedImageMapUIElement(HTMLAreaElement*);

    AXID getAXID(AccessibilityObject*);
    AXID generateNewObjectID() const;

    void notificationPostTimerFired();

    void liveRegionChangedNotificationPostTimerFired();

    void focusCurrentModal();
    
    void performCacheUpdateTimerFired();

    void postTextStateChangeNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&);

    bool enqueuePasswordValueChangeNotification(AccessibilityObject*);
    void passwordNotificationPostTimerFired();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void selectedTextRangeTimerFired();
    Seconds platformSelectedTextRangeDebounceInterval() const;
    void updateTreeSnapshotTimerFired();
    void processQueuedIsolatedNodeUpdates();
#endif

    void deferRowspanChange(AccessibilityObject*);
    void handleChildrenChanged(AccessibilityObject&);
    void handleAllDeferredChildrenChanged();
    void handleRoleChanged(Element*, const AtomString&, const AtomString&);
    void handleRoleDescriptionChanged(Element*);
    void handleMenuOpened(Node*);
    void handleLiveRegionCreated(Node*);
    void handleLabelCreated(HTMLLabelElement*);
    void handleMenuItemSelected(Node*);
    void handleTabPanelSelected(Node*, Node*);
    void handleRowCountChanged(AccessibilityObject*, Document*);
    void handleAttributeChange(Element*, const QualifiedName&, const AtomString&, const AtomString&);
    bool shouldProcessAttributeChange(Element*, const QualifiedName&);
    void selectedChildrenChanged(Node*);
    void selectedChildrenChanged(RenderObject*);
    void handleScrollbarUpdate(ScrollView&);
    void handleActiveDescendantChange(Element&, const AtomString&, const AtomString&);
    void handleAriaExpandedChange(Node*);
    enum class UpdateModal : bool { No, Yes };
    void handleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode, UpdateModal = UpdateModal::Yes);
    void handleMenuListValueChanged(Element&);
    void handleTextChanged(AccessibilityObject*);
    void handleRecomputeCellSlots(AccessibilityTable&);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void handleRowspanChanged(AccessibilityTableCell&);
#endif

    // aria-modal or modal <dialog> related
    bool isModalElement(Element&) const;
    void findModalNodes();
    enum class WillRecomputeFocus : bool { No, Yes };
    void updateCurrentModalNode(WillRecomputeFocus = WillRecomputeFocus::No);
    bool isNodeVisible(Node*) const;
    bool modalElementHasAccessibleContent(Element&);

    // Relationships between objects.
    static Vector<QualifiedName>& relationAttributes();
    static AXRelationType attributeToRelationType(const QualifiedName&);
    enum class AddSymmetricRelation : bool { No, Yes };
    static AXRelationType symmetricRelation(AXRelationType);
    void addRelation(Element*, Element*, AXRelationType);
    void addRelation(AccessibilityObject*, AccessibilityObject*, AXRelationType, AddSymmetricRelation = AddSymmetricRelation::Yes);
    void removeRelationByID(AXID originID, AXID targetID, AXRelationType);
    void addRelations(Element&, const QualifiedName&);
    void removeRelations(Element&, AXRelationType);
    void updateRelationsIfNeeded();
    void updateRelationsForTree(ContainerNode&);
    void relationsNeedUpdate(bool);
    HashMap<AXID, AXRelations> relations();
    const HashSet<AXID>& relationTargetIDs();
    bool isDescendantOfRelatedNode(Node&);

    // Object creation.
    Ref<AccessibilityObject> createObjectFromRenderer(RenderObject*);

    CheckedRef<Document> m_document;
    const std::optional<PageIdentifier> m_pageID; // constant for object's lifetime.
    OptionSet<ActivityState> m_pageActivityState;
    HashMap<AXID, RefPtr<AccessibilityObject>> m_objects;

    // The pointers in these mapping HashMaps should never be dereferenced.
    HashMap<RenderObject*, AXID> m_renderObjectMapping;
    HashMap<Widget*, AXID> m_widgetObjectMapping;
    HashMap<Node*, AXID> m_nodeObjectMapping;
    // The pointers in this HashSet should never be dereferenced.
    ListHashSet<Node*> m_textMarkerNodes;

    std::unique_ptr<AXComputedObjectAttributeCache> m_computedObjectAttributeCache;

#if ENABLE(ACCESSIBILITY)
    WEBCORE_EXPORT static bool gAccessibilityEnabled;
    WEBCORE_EXPORT static bool gAccessibilityEnhancedUserInterfaceEnabled;
    static bool gForceDeferredSpellChecking;
#else
    static constexpr bool gAccessibilityEnabled { false };
    static constexpr bool gAccessibilityEnhancedUserInterfaceEnabled { false };
    static constexpr bool gForceDeferredSpellChecking { false };
#endif

    HashSet<AXID> m_idsInUse;

    Timer m_notificationPostTimer;
    Vector<std::pair<RefPtr<AccessibilityObject>, AXNotification>> m_notificationsToPost;

    Timer m_passwordNotificationPostTimer;

    ListHashSet<RefPtr<AccessibilityObject>> m_passwordNotificationsToPost;
    
    Timer m_liveRegionChangedPostTimer;
    ListHashSet<RefPtr<AccessibilityObject>> m_liveRegionObjectsSet;

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_currentModalElement;
    // Multiple aria-modals behavior is undefined by spec. We keep them sorted based on DOM order here.
    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> m_modalElements;
    bool m_modalNodesInitialized { false };
    bool m_isRetrievingCurrentModalNode { false };

    Timer m_performCacheUpdateTimer;

    AXTextStateChangeIntent m_textSelectionIntent;
    // An object can be "replaced" when we create an AX object from the backing element before it has
    // attached a renderer, but then want to replace it with a new AX object after the renderer has been attached.
    HashSet<AXID> m_deferredReplacedObjects;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredRecomputeIsIgnoredList;
    WeakHashSet<HTMLTableElement, WeakPtrImplWithEventTargetData> m_deferredRecomputeTableIsExposedList;
    WeakHashSet<AccessibilityTable> m_deferredRecomputeTableCellSlotsList;
    WeakHashSet<AccessibilityTableCell> m_deferredRowspanChanges;
    WeakListHashSet<Node, WeakPtrImplWithEventTargetData> m_deferredTextChangedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredSelectedChildredChangedList;
    ListHashSet<RefPtr<AccessibilityObject>> m_deferredChildrenChangedList;
    WeakListHashSet<Node, WeakPtrImplWithEventTargetData> m_deferredNodeAddedOrRemovedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredModalChangedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredMenuListChange;
    WeakHashSet<ScrollView> m_deferredScrollbarUpdateChangeList;
    WeakHashMap<Element, String, WeakPtrImplWithEventTargetData> m_deferredTextFormControlValue;
    Vector<AttributeChange> m_deferredAttributeChange;
    std::optional<std::pair<WeakPtr<Node, WeakPtrImplWithEventTargetData>, WeakPtr<Node, WeakPtrImplWithEventTargetData>>> m_deferredFocusedNodeChange;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    Timer m_buildIsolatedTreeTimer;
    bool m_deferredRegenerateIsolatedTree { false };
    Ref<AXGeometryManager> m_geometryManager;
    DeferrableOneShotTimer m_selectedTextRangeTimer;
    AXID m_lastDebouncedTextRangeObject;

    Timer m_updateTreeSnapshotTimer;
#endif
    bool m_isSynchronizingSelection { false };
    bool m_performingDeferredCacheUpdate { false };
    double m_loadingProgress { 0 };

    // Relationships between objects.
    HashMap<AXID, AXRelations> m_relations;
    bool m_relationsNeedUpdate { true };
    HashSet<AXID> m_relationTargets;

#if USE(ATSPI)
    ListHashSet<RefPtr<AXCoreObject>> m_deferredParentChangedList;
#endif
};

template<typename U>
inline Vector<RefPtr<AXCoreObject>> AXObjectCache::objectsForIDs(const U& axIDs) const
{
    ASSERT(isMainThread());

    return WTF::compactMap(axIDs, [&](auto& axID) -> std::optional<RefPtr<AXCoreObject>> {
        if (auto* object = objectForID(axID))
            return RefPtr { object };
        return std::nullopt;
    });
}

class AXAttributeCacheEnabler
{
public:
    explicit AXAttributeCacheEnabler(AXObjectCache *cache);
    ~AXAttributeCacheEnabler();

#if ENABLE(ACCESSIBILITY)
private:
    AXObjectCache* m_cache;
    bool m_wasAlreadyCaching { false };
#endif
};

bool nodeHasRole(Node*, StringView role);
bool nodeHasGridRole(Node*);
bool nodeHasCellRole(Node*);
// This will let you know if aria-hidden was explicitly set to false.
bool isNodeAriaVisible(Node*);

#if !ENABLE(ACCESSIBILITY)
inline AccessibilityObjectInclusion AXComputedObjectAttributeCache::getIgnored(AXID) const { return AccessibilityObjectInclusion::DefaultBehavior; }
inline AccessibilityReplacedText::AccessibilityReplacedText(const VisibleSelection&) { }
inline void AccessibilityReplacedText::postTextStateChangeNotification(AXObjectCache*, AXTextEditType, const String&, const VisibleSelection&) { }
inline void AXComputedObjectAttributeCache::setIgnored(AXID, AccessibilityObjectInclusion) { }
inline AXObjectCache::AXObjectCache(Document& document) : m_document(document), m_notificationPostTimer(*this, &AXObjectCache::notificationPostTimerFired), m_passwordNotificationPostTimer(*this, &AXObjectCache::passwordNotificationPostTimerFired), m_liveRegionChangedPostTimer(*this, &AXObjectCache::liveRegionChangedNotificationPostTimerFired), m_performCacheUpdateTimer(*this, &AXObjectCache::performCacheUpdateTimerFired) { }
inline AXObjectCache::~AXObjectCache() { }
inline AccessibilityObject* AXObjectCache::get(RenderObject*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::get(Node*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::get(Widget*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(RenderObject*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::create(AccessibilityRole) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(Node*, IsPartOfRelation) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(Widget*) { return nullptr; }
inline AXCoreObject* AXObjectCache::rootObject() { return nullptr; }
inline AccessibilityObject* AXObjectCache::rootObjectForFrame(LocalFrame*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::focusedObjectForPage(const Page*) { return nullptr; }
inline void AXObjectCache::enableAccessibility() { }
inline void AXObjectCache::setForceDeferredSpellChecking(bool) { }
inline void AXObjectCache::disableAccessibility() { }
inline void AXObjectCache::setEnhancedUserInterfaceAccessibility(bool) { }
inline bool nodeHasRole(Node*, StringView) { return false; }
inline bool nodeHasGridRole(Node*) { return false; }
inline bool nodeHasCellRole(Node*) { return false; }
inline void AXObjectCache::startCachingComputedObjectAttributesUntilTreeMutates() { }
inline void AXObjectCache::stopCachingComputedObjectAttributes() { }
inline bool isNodeAriaVisible(Node*) { return true; }
inline const Element* AXObjectCache::rootAXEditableElement(const Node*) { return nullptr; }
inline Node* AXObjectCache::modalNode() { return nullptr; }
inline void AXObjectCache::attachWrapper(AccessibilityObject*) { }
inline void AXObjectCache::checkedStateChanged(Node*) { }
inline void AXObjectCache::autofillTypeChanged(Node*) { }
inline void AXObjectCache::childrenChanged(Node*, Node*) { }
inline void AXObjectCache::childrenChanged(RenderObject*, RenderObject*) { }
inline void AXObjectCache::childrenChanged(AccessibilityObject*) { }
inline void AXObjectCache::onSelectedChanged(Node*) { }
inline void AXObjectCache::onTextSecurityChanged(HTMLInputElement&) { }
inline void AXObjectCache::onTitleChange(Document&) { }
inline void AXObjectCache::onValidityChange(Element&) { }
inline void AXObjectCache::onTextCompositionChange(Node&, CompositionState, bool, const String&, size_t, bool) { }
inline void AXObjectCache::valueChanged(Element*) { }
inline void AXObjectCache::onFocusChange(Node*, Node*) { }
inline void AXObjectCache::onPageActivityStateChange(OptionSet<ActivityState>) { }
inline void AXObjectCache::onPopoverToggle(const HTMLElement&) { }
inline void AXObjectCache::onScrollbarFrameRectChange(const Scrollbar&) { }
inline void AXObjectCache::onWidgetVisibilityChanged(RenderWidget*) { }
inline void AXObjectCache::deferRecomputeIsIgnoredIfNeeded(Element*) { }
inline void AXObjectCache::deferRecomputeIsIgnored(Element*) { }
inline void AXObjectCache::deferTextChangedIfNeeded(Node*) { }
inline void AXObjectCache::deferSelectedChildrenChangedIfNeeded(Element&) { }
inline void AXObjectCache::deferTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String&) { }
inline void AXObjectCache::deferRecomputeTableCellSlots(AccessibilityTable&) { }
inline void AXObjectCache::deferRowspanChange(AccessibilityObject*) { }
#if !PLATFORM(COCOA) && !USE(ATSPI)
inline void AXObjectCache::detachWrapper(AXCoreObject*, AccessibilityDetachmentType) { }
#endif
inline void AXObjectCache::focusCurrentModal() { }
inline void AXObjectCache::performCacheUpdateTimerFired() { }
inline void AXObjectCache::frameLoadingEventNotification(LocalFrame*, AXLoadingEvent) { }
inline void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject*, AXLoadingEvent) { }
inline void AXObjectCache::handleAriaExpandedChange(Node*) { }
inline void AXObjectCache::deferModalChange(Element*) { }
inline void AXObjectCache::handleAllDeferredChildrenChanged() { }
inline void AXObjectCache::handleRoleChanged(AccessibilityObject*) { }
inline void AXObjectCache::deferAttributeChangeIfNeeded(Element*, const QualifiedName&, const AtomString&, const AtomString&) { }
inline void AXObjectCache::handleAttributeChange(Element*, const QualifiedName&, const AtomString&, const AtomString&) { }
inline bool AXObjectCache::shouldProcessAttributeChange(Element*, const QualifiedName&) { return false; }
inline void AXObjectCache::handleFocusedUIElementChanged(Node*, Node*, UpdateModal) { }
inline void AXObjectCache::handleScrollbarUpdate(ScrollView&) { }
inline void AXObjectCache::onScrollbarUpdate(ScrollView*) { }
inline void AXObjectCache::handleScrolledToAnchor(const Node*) { }
inline void AXObjectCache::liveRegionChangedNotificationPostTimerFired() { }
inline void AXObjectCache::notificationPostTimerFired() { }
inline void AXObjectCache::passwordNotificationPostTimerFired() { }
inline void AXObjectCache::performDeferredCacheUpdate() { }
inline void AXObjectCache::postLiveRegionChangeNotification(AccessibilityObject*) { }
inline void AXObjectCache::postNotification(AccessibilityObject*, Document*, AXNotification, PostTarget) { }
inline void AXObjectCache::postNotification(Node*, AXNotification, PostTarget) { }
inline void AXObjectCache::postNotification(RenderObject*, AXNotification, PostTarget) { }
inline void AXObjectCache::announce(const String&) { }
inline void AXObjectCache::postTextReplacementNotification(Node*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&) { }
inline void AXObjectCache::postTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String&, const String&) { }
inline void AXObjectCache::postTextStateChangeNotification(Node*, AXTextEditType, const String&, const VisiblePosition&) { }
inline void AXObjectCache::postTextStateChangeNotification(Node*, const AXTextStateChangeIntent&, const VisibleSelection&) { }
inline void AXObjectCache::recomputeIsIgnored(RenderObject*) { }
inline void AXObjectCache::handleTextChanged(AccessibilityObject*) { }
inline void AXObjectCache::handleRecomputeCellSlots(AccessibilityTable&) { }
inline void AXObjectCache::onRendererCreated(Element&) { }
inline void AXObjectCache::updateLoadingProgress(double) { }
inline SimpleRange AXObjectCache::rangeForNodeContents(Node& node) { return makeRangeSelectingNodeContents(node); }
inline std::optional<ListHashSet<AXID>> AXObjectCache::relatedObjectIDsFor(const AXCoreObject&, AXRelationType) { return std::nullopt; }
inline void AXObjectCache::updateRelations(Element&, const QualifiedName&) { }
inline void AXObjectCache::remove(AXID) { }
inline void AXObjectCache::remove(RenderObject*) { }
inline void AXObjectCache::remove(Node&) { }
inline void AXObjectCache::remove(Widget*) { }
inline void AXObjectCache::selectedChildrenChanged(RenderObject*) { }
inline void AXObjectCache::selectedChildrenChanged(Node*) { }
inline void AXObjectCache::setIsSynchronizingSelection(bool) { }
inline void AXObjectCache::setTextSelectionIntent(const AXTextStateChangeIntent&) { }
inline std::optional<SimpleRange> AXObjectCache::rangeForUnorderedCharacterOffsets(const CharacterOffset&, const CharacterOffset&) { return std::nullopt; }
inline IntRect AXObjectCache::absoluteCaretBoundsForCharacterOffset(const CharacterOffset&) { return IntRect(); }
inline CharacterOffset AXObjectCache::characterOffsetForIndex(int, const AXCoreObject*) { return CharacterOffset(); }
inline CharacterOffset AXObjectCache::startOrEndCharacterOffsetForRange(const SimpleRange&, bool, bool) { return CharacterOffset(); }
inline CharacterOffset AXObjectCache::endCharacterOffsetOfLine(const CharacterOffset&) { return CharacterOffset(); }
inline CharacterOffset AXObjectCache::nextCharacterOffset(const CharacterOffset&, bool) { return CharacterOffset(); }
inline CharacterOffset AXObjectCache::previousCharacterOffset(const CharacterOffset&, bool) { return CharacterOffset(); }
inline std::optional<TextMarkerData> AXObjectCache::textMarkerDataForVisiblePosition(const VisiblePosition&) { return std::nullopt; }
inline TextMarkerData AXObjectCache::textMarkerDataForCharacterOffset(const CharacterOffset&) { return { }; }
inline VisiblePosition AXObjectCache::visiblePositionForTextMarkerData(const TextMarkerData&) { return { }; }
inline VisiblePosition AXObjectCache::visiblePositionFromCharacterOffset(const CharacterOffset&) { return { }; }
#if PLATFORM(COCOA)
inline void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&) { }
inline void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject*, AXTextEditType, const String&, const VisiblePosition&) { }
inline void AXObjectCache::postTextReplacementPlatformNotification(AccessibilityObject*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&) { }
#else
inline AXTextChange AXObjectCache::textChangeForEditType(AXTextEditType) { return AXTextInserted; }
inline void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject*, AXTextChange, unsigned, const String&) { }
#endif
inline AXTreeData AXObjectCache::treeData() { return { }; }

inline AXAttributeCacheEnabler::AXAttributeCacheEnabler(AXObjectCache*) { }
inline AXAttributeCacheEnabler::~AXAttributeCacheEnabler() { }
#endif // !ENABLE(ACCESSIBILITY)

WTF::TextStream& operator<<(WTF::TextStream&, AXObjectCache::AXNotification);

} // namespace WebCore
