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

#include "AXIsolatedTree.h"
#include "AXTextStateChangeIntent.h"
#include "AccessibilityObject.h"
#include "SimpleRange.h"
#include "Timer.h"
#include "VisibleUnits.h"
#include <limits.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/WeakHashSet.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Document;
class HTMLAreaElement;
class HTMLTableElement;
class HTMLTextFormControlElement;
class Node;
class Page;
class RenderBlock;
class RenderObject;
class RenderText;
class ScrollView;
class VisiblePosition;
class Widget;

struct TextMarkerData {
    AXID axID;

    Node* node { nullptr };
    unsigned offset { 0 };
    Position::AnchorType anchorType { Position::PositionIsOffsetInAnchor };
    Affinity affinity { Affinity::Downstream };

    int characterStartIndex { 0 };
    int characterOffset { 0 };
    bool ignored { false };
};

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
};

class AXComputedObjectAttributeCache {
    WTF_MAKE_FAST_ALLOCATED;
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

class AXObjectCache {
    WTF_MAKE_NONCOPYABLE(AXObjectCache); WTF_MAKE_FAST_ALLOCATED;
    friend class AXIsolatedTree;
    friend WTF::TextStream& operator<<(WTF::TextStream&, AXObjectCache&);
public:
    explicit AXObjectCache(Document&);
    ~AXObjectCache();

    // Returns the root object for the entire document.
    WEBCORE_EXPORT AXCoreObject* rootObject();
    // Returns the root object for a specific frame.
    WEBCORE_EXPORT AccessibilityObject* rootObjectForFrame(Frame*);
    
    // For AX objects with elements that back them.
    AccessibilityObject* getOrCreate(RenderObject*);
    AccessibilityObject* getOrCreate(Widget*);
    WEBCORE_EXPORT AccessibilityObject* getOrCreate(Node*);

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
    void childrenChanged(Node*, Node* newChild = nullptr);
    void childrenChanged(RenderObject*, RenderObject* newChild = nullptr);
    void childrenChanged(AccessibilityObject*);
    void onSelectedChanged(Node*);
    void valueChanged(Element*);
    void checkedStateChanged(Node*);
    void autofillTypeChanged(Node*);
    void handleRoleChanged(AccessibilityObject*);
    // Called when a node has just been attached, so we can make sure we have the right subclass of AccessibilityObject.
    void updateCacheAfterNodeIsAttached(Node*);
    void updateLoadingProgress(double);
    void loadingFinished() { updateLoadingProgress(1); }
    double loadingProgress() const { return m_loadingProgress; }

    void deferFocusedUIElementChangeIfNeeded(Node* oldFocusedNode, Node* newFocusedNode);
    void deferModalChange(Element*);
    void deferMenuListValueChange(Element*);
    void deferNodeAddedOrRemoved(Node*);
    void handleScrolledToAnchor(const Node* anchorNode);
    void handleScrollbarUpdate(ScrollView*);

    bool isRetrievingCurrentModalNode() { return m_isRetrievingCurrentModalNode; }
    Node* modalNode();

    void deferAttributeChangeIfNeeded(Element*, const QualifiedName&, const AtomString&, const AtomString&);
    void recomputeIsIgnored(RenderObject*);
    void recomputeIsIgnored(Node*);

    WEBCORE_EXPORT static void enableAccessibility();
    WEBCORE_EXPORT static void disableAccessibility();

    WEBCORE_EXPORT AccessibilityObject* focusedObjectForPage(const Page*);

    // Enhanced user interface accessibility can be toggled by the assistive technology.
    WEBCORE_EXPORT static void setEnhancedUserInterfaceAccessibility(bool flag);

    // Note: these may be called from a non-main thread concurrently as other readers.
    static bool accessibilityEnabled() { return gAccessibilityEnabled; }
    static bool accessibilityEnhancedUserInterfaceEnabled() { return gAccessibilityEnhancedUserInterfaceEnabled; }

    const Element* rootAXEditableElement(const Node*);
    bool nodeIsTextControl(const Node*);

    AXID platformGenerateAXID() const;
    AccessibilityObject* objectForID(const AXID& id) const { return m_objects.get(id); }
    Vector<RefPtr<AXCoreObject>> objectsForIDs(const Vector<AXID>&) const;

    // Text marker utilities.
    std::optional<TextMarkerData> textMarkerDataForVisiblePosition(const VisiblePosition&);
    std::optional<TextMarkerData> textMarkerDataForFirstPositionInTextControl(HTMLTextFormControlElement&);
    void textMarkerDataForCharacterOffset(TextMarkerData&, const CharacterOffset&);
    void textMarkerDataForNextCharacterOffset(TextMarkerData&, const CharacterOffset&);
    void textMarkerDataForPreviousCharacterOffset(TextMarkerData&, const CharacterOffset&);
    VisiblePosition visiblePositionForTextMarkerData(TextMarkerData&);
    CharacterOffset characterOffsetForTextMarkerData(TextMarkerData&);
    // Use ignoreNextNodeStart/ignorePreviousNodeEnd to determine the behavior when we are at node boundary. 
    CharacterOffset nextCharacterOffset(const CharacterOffset&, bool ignoreNextNodeStart = true);
    CharacterOffset previousCharacterOffset(const CharacterOffset&, bool ignorePreviousNodeEnd = true);
    void startOrEndTextMarkerDataForRange(TextMarkerData&, const SimpleRange&, bool);
    CharacterOffset startOrEndCharacterOffsetForRange(const SimpleRange&, bool, bool enterTextControls = false);
    AccessibilityObject* accessibilityObjectForTextMarkerData(TextMarkerData&);
    std::optional<SimpleRange> rangeForUnorderedCharacterOffsets(const CharacterOffset&, const CharacterOffset&);
    static SimpleRange rangeForNodeContents(Node&);
    static int lengthForRange(const std::optional<SimpleRange>&);
    
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
        AXActiveDescendantChanged,
        AXAutocorrectionOccured,
        AXAutofillTypeChanged,
        AXCheckedStateChanged,
        AXChildrenChanged,
        AXColumnCountChanged,
        AXColumnIndexChanged,
        AXColumnSpanChanged,
        AXCurrentStateChanged,
        AXDescribedByChanged,
        AXDisabledStateChanged,
        AXDropEffectChanged,
        AXFlowToChanged,
        AXFocusedUIElementChanged,
        AXFrameLoadComplete,
        AXGrabbedStateChanged,
        AXHasPopupChanged,
        AXIdAttributeChanged,
        AXImageOverlayChanged,
        AXIsAtomicChanged,
        AXLanguageChanged,
        AXLayoutComplete,
        AXLevelChanged,
        AXLoadComplete,
        AXNewDocumentLoadComplete,
        AXPageScrolled,
        AXPlaceholderChanged,
        AXPositionInSetChanged,
        AXRoleChanged,
        AXRoleDescriptionChanged,
        AXRowIndexChanged,
        AXRowSpanChanged,
        AXSelectedChildrenChanged,
        AXSelectedCellChanged,
        AXSelectedStateChanged,
        AXSelectedTextChanged,
        AXSetSizeChanged,
        AXTableHeadersChanged,
        AXURLChanged,
        AXValueChanged,
        AXScrolledToAnchor,
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

    void frameLoadingEventNotification(Frame*, AXLoadingEvent);

    void prepareForDocumentDestruction(const Document&);

    void startCachingComputedObjectAttributesUntilTreeMutates();
    void stopCachingComputedObjectAttributes();

    AXComputedObjectAttributeCache* computedObjectAttributeCache() { return m_computedObjectAttributeCache.get(); }

    Document& document() const { return m_document; }
    std::optional<PageIdentifier> pageID() const { return m_pageID; }

#if PLATFORM(MAC)
    static void setShouldRepostNotificationsForTests(bool value);
#endif
    void deferRecomputeIsIgnoredIfNeeded(Element*);
    void deferRecomputeIsIgnored(Element*);
    void deferRecomputeTableIsExposed(Element*);
    void deferTextChangedIfNeeded(Node*);
    void deferSelectedChildrenChangedIfNeeded(Element&);
    void performDeferredCacheUpdate();
    void deferTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String& previousValue);

    std::optional<SimpleRange> rangeMatchesTextNearRange(const SimpleRange&, const String&);

    static ASCIILiteral notificationPlatformName(AXNotification);

    AXTreeData treeData();

    // Returns the IDs of the objects that relate to the given object with the specified relationship.
    std::optional<Vector<AXID>> relatedObjectIDsFor(const AXCoreObject&, AXRelationType);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    WEBCORE_EXPORT static bool isIsolatedTreeEnabled();
    WEBCORE_EXPORT static bool usedOnAXThread();
private:
    static bool clientSupportsIsolatedTree();
    AXCoreObject* isolatedTreeRootObject();
    void setIsolatedTreeFocusedObject(Node*);
    RefPtr<AXIsolatedTree> getOrCreateIsolatedTree() const;
    void updateIsolatedTree(AccessibilityObject&, AXNotification);
    void updateIsolatedTree(AccessibilityObject*, AXNotification);
    void updateIsolatedTree(const Vector<std::pair<RefPtr<AccessibilityObject>, AXNotification>>&);
    static void initializeSecondaryAXThread();
#endif

protected:
    void postPlatformNotification(AXCoreObject*, AXNotification);
    void platformHandleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode);

    void platformPerformDeferredCacheUpdate();

#if PLATFORM(COCOA) || USE(ATSPI)
    void postTextStateChangePlatformNotification(AXCoreObject*, const AXTextStateChangeIntent&, const VisibleSelection&);
    void postTextStateChangePlatformNotification(AccessibilityObject*, AXTextEditType, const String&, const VisiblePosition&);
    void postTextReplacementPlatformNotificationForTextControl(AXCoreObject*, const String& deletedText, const String& insertedText, HTMLTextFormControlElement&);
    void postTextReplacementPlatformNotification(AXCoreObject*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&);
#else
    static AXTextChange textChangeForEditType(AXTextEditType);
    void nodeTextChangePlatformNotification(AccessibilityObject*, AXTextChange, unsigned, const String&);
#endif

    void frameLoadingEventPlatformNotification(AccessibilityObject*, AXLoadingEvent);
    void labelChanged(Element*);

    // This is a weak reference cache for knowing if Nodes used by TextMarkers are valid.
    void setNodeInUse(Node* n) { m_textMarkerNodes.add(n); }
    void removeNodeForUse(Node& n) { m_textMarkerNodes.remove(&n); }
    bool isNodeInUse(Node* n) { return m_textMarkerNodes.contains(n); }
    
    // CharacterOffset functions.
    enum TraverseOption { TraverseOptionDefault = 1 << 0, TraverseOptionToNodeEnd = 1 << 1, TraverseOptionIncludeStart = 1 << 2, TraverseOptionValidateOffset = 1 << 3, TraverseOptionDoNotEnterTextControls = 1 << 4 };
    Node* nextNode(Node*) const;
    Node* previousNode(Node*) const;
    CharacterOffset traverseToOffsetInRange(const SimpleRange&, int, TraverseOption = TraverseOptionDefault, bool stayWithinRange = false);
    VisiblePosition visiblePositionFromCharacterOffset(const CharacterOffset&);
    CharacterOffset characterOffsetFromVisiblePosition(const VisiblePosition&);
    void setTextMarkerDataWithCharacterOffset(TextMarkerData&, const CharacterOffset&);
    UChar32 characterAfter(const CharacterOffset&);
    UChar32 characterBefore(const CharacterOffset&);
    CharacterOffset characterOffsetForNodeAndOffset(Node&, int, TraverseOption = TraverseOptionDefault);

    enum class NeedsContextAtParagraphStart { Yes, No };
    CharacterOffset previousBoundary(const CharacterOffset&, BoundarySearchFunction, NeedsContextAtParagraphStart = NeedsContextAtParagraphStart::No);
    CharacterOffset nextBoundary(const CharacterOffset&, BoundarySearchFunction);
    CharacterOffset startCharacterOffsetOfWord(const CharacterOffset&, EWordSide = RightWordIfOnBoundary);
    CharacterOffset endCharacterOffsetOfWord(const CharacterOffset&, EWordSide = RightWordIfOnBoundary);
    CharacterOffset startCharacterOffsetOfParagraph(const CharacterOffset&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
    CharacterOffset endCharacterOffsetOfParagraph(const CharacterOffset&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
    CharacterOffset startCharacterOffsetOfSentence(const CharacterOffset&);
    CharacterOffset endCharacterOffsetOfSentence(const CharacterOffset&);
    CharacterOffset characterOffsetForPoint(const IntPoint&);
    LayoutRect localCaretRectForCharacterOffset(RenderObject*&, const CharacterOffset&);
    bool shouldSkipBoundary(const CharacterOffset&, const CharacterOffset&);
private:
    AccessibilityObject* rootWebArea();
    static AccessibilityObject* focusedImageMapUIElement(HTMLAreaElement*);

    AXID getAXID(AccessibilityObject*);

    void notificationPostTimerFired();

    void liveRegionChangedNotificationPostTimerFired();
    
    void focusCurrentModal();
    
    void performCacheUpdateTimerFired();

    void postTextStateChangeNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&);

    bool enqueuePasswordValueChangeNotification(AccessibilityObject*);
    void passwordNotificationPostTimerFired();

    void processDeferredChildrenChangedList();
    void handleChildrenChanged(AccessibilityObject&);
    void handleRoleChanged(Element*, const AtomString&, const AtomString&);
    void handleRoleDescriptionChanged(Element*);
    void handleMenuOpened(Node*);
    void handleLiveRegionCreated(Node*);
    void handleMenuItemSelected(Node*);
    void handleRowCountChanged(AccessibilityObject*, Document*);
    void handleAttributeChange(Element*, const QualifiedName&, const AtomString&, const AtomString&);
    bool shouldProcessAttributeChange(Element*, const QualifiedName&);
    void selectedChildrenChanged(Node*);
    void selectedChildrenChanged(RenderObject*);

    void handleActiveDescendantChanged(Element&);

    void handleAriaExpandedChange(Node*);
    enum class UpdateModal : bool { No, Yes };
    void handleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode, UpdateModal = UpdateModal::Yes);
    void handleMenuListValueChanged(Element&);
    void handleTextChanged(AccessibilityObject*);

    // aria-modal or modal <dialog> related
    bool isModalElement(Element&) const;
    void findModalNodes();
    void updateCurrentModalNode();
    Element* updateCurrentModalNodeInternal();
    bool isNodeVisible(Node*) const;
    bool modalElementHasAccessibleContent(Element&);

    // Relationships between objects.
    static Vector<QualifiedName>& relationAttributes();
    static AXRelationType attributeToRelationType(const QualifiedName&);
    enum class AddingSymmetricRelation : bool { No, Yes };
    static AXRelationType symmetricRelation(AXRelationType);
    void addRelation(Element*, Element*, AXRelationType);
    void addRelation(AccessibilityObject*, AccessibilityObject*, AXRelationType, AddingSymmetricRelation = AddingSymmetricRelation::No);
    void updateRelationsIfNeeded();
    void updateRelationsForTree(ContainerNode&);
    void relationsNeedUpdate(bool);
    HashMap<AXID, AXRelations> relations();
    const HashSet<AXID>& relationTargetIDs();

    Document& m_document;
    const std::optional<PageIdentifier> m_pageID; // constant for object's lifetime.
    HashMap<AXID, RefPtr<AccessibilityObject>> m_objects;
    HashMap<RenderObject*, AXID> m_renderObjectMapping;
    HashMap<Widget*, AXID> m_widgetObjectMapping;
    HashMap<Node*, AXID> m_nodeObjectMapping;
    ListHashSet<Node*> m_textMarkerNodes;
    std::unique_ptr<AXComputedObjectAttributeCache> m_computedObjectAttributeCache;

#if ENABLE(ACCESSIBILITY)
    WEBCORE_EXPORT static bool gAccessibilityEnabled;
    WEBCORE_EXPORT static bool gAccessibilityEnhancedUserInterfaceEnabled;
#else
    static constexpr bool gAccessibilityEnabled = false;
    static constexpr bool gAccessibilityEnhancedUserInterfaceEnabled = false;
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
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredRecomputeIsIgnoredList;
    WeakHashSet<HTMLTableElement, WeakPtrImplWithEventTargetData> m_deferredRecomputeTableIsExposedList;
    ListHashSet<Node*> m_deferredTextChangedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredSelectedChildredChangedList;
    ListHashSet<RefPtr<AccessibilityObject>> m_deferredChildrenChangedList;
    ListHashSet<Node*> m_deferredNodeAddedOrRemovedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredModalChangedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredMenuListChange;
    HashMap<Element*, String> m_deferredTextFormControlValue;

    struct AttributeChange {
        Element* element { nullptr };
        QualifiedName attrName;
        AtomString oldValue;
        AtomString newValue;
    };
    Vector<AttributeChange> m_deferredAttributeChange;

    Vector<std::pair<Node*, Node*>> m_deferredFocusedNodeChange;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool m_deferredRegenerateIsolatedTree { false };
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

class AXAttributeCacheEnabler
{
public:
    explicit AXAttributeCacheEnabler(AXObjectCache *cache);
    ~AXAttributeCacheEnabler();

#if ENABLE(ACCESSIBILITY)
private:
    AXObjectCache* m_cache;
#endif
};

bool nodeHasRole(Node*, StringView role);
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
inline AccessibilityObject* AXObjectCache::getOrCreate(Node*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(Widget*) { return nullptr; }
inline AXCoreObject* AXObjectCache::rootObject() { return nullptr; }
inline AccessibilityObject* AXObjectCache::rootObjectForFrame(Frame*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::focusedObjectForPage(const Page*) { return nullptr; }
inline void AXObjectCache::enableAccessibility() { }
inline void AXObjectCache::disableAccessibility() { }
inline void AXObjectCache::setEnhancedUserInterfaceAccessibility(bool) { }
inline bool nodeHasRole(Node*, StringView) { return false; }
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
inline void AXObjectCache::valueChanged(Element*) { }
inline void AXObjectCache::deferFocusedUIElementChangeIfNeeded(Node*, Node*) { }
inline void AXObjectCache::deferRecomputeIsIgnoredIfNeeded(Element*) { }
inline void AXObjectCache::deferRecomputeIsIgnored(Element*) { }
inline void AXObjectCache::deferTextChangedIfNeeded(Node*) { }
inline void AXObjectCache::deferSelectedChildrenChangedIfNeeded(Element&) { }
inline void AXObjectCache::deferTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String&) { }
#if !PLATFORM(COCOA) && !USE(ATSPI)
inline void AXObjectCache::detachWrapper(AXCoreObject*, AccessibilityDetachmentType) { }
#endif
inline void AXObjectCache::focusCurrentModal() { }
inline void AXObjectCache::performCacheUpdateTimerFired() { }
inline void AXObjectCache::frameLoadingEventNotification(Frame*, AXLoadingEvent) { }
inline void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject*, AXLoadingEvent) { }
inline void AXObjectCache::handleActiveDescendantChanged(Element&) { }
inline void AXObjectCache::handleAriaExpandedChange(Node*) { }
inline void AXObjectCache::deferModalChange(Element*) { }
inline void AXObjectCache::handleRoleChanged(AccessibilityObject*) { }
inline void AXObjectCache::deferAttributeChangeIfNeeded(Element*, const QualifiedName&, const AtomString&, const AtomString&) { }
inline void AXObjectCache::handleAttributeChange(Element*, const QualifiedName&, const AtomString&, const AtomString&) { }
inline bool AXObjectCache::shouldProcessAttributeChange(Element*, const QualifiedName&) { return false; }
inline void AXObjectCache::handleFocusedUIElementChanged(Node*, Node*, UpdateModal) { }
inline void AXObjectCache::handleScrollbarUpdate(ScrollView*) { }
inline void AXObjectCache::handleScrolledToAnchor(const Node*) { }
inline void AXObjectCache::liveRegionChangedNotificationPostTimerFired() { }
inline void AXObjectCache::notificationPostTimerFired() { }
inline Vector<RefPtr<AXCoreObject>> AXObjectCache::objectsForIDs(const Vector<AXID>&) const { return { }; }
inline void AXObjectCache::passwordNotificationPostTimerFired() { }
inline void AXObjectCache::performDeferredCacheUpdate() { }
inline void AXObjectCache::postLiveRegionChangeNotification(AccessibilityObject*) { }
inline void AXObjectCache::postNotification(AccessibilityObject*, Document*, AXNotification, PostTarget) { }
inline void AXObjectCache::postNotification(Node*, AXNotification, PostTarget) { }
inline void AXObjectCache::postNotification(RenderObject*, AXNotification, PostTarget) { }
inline void AXObjectCache::postPlatformNotification(AXCoreObject*, AXNotification) { }
inline void AXObjectCache::postTextReplacementNotification(Node*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&) { }
inline void AXObjectCache::postTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String&, const String&) { }
inline void AXObjectCache::postTextStateChangeNotification(Node*, AXTextEditType, const String&, const VisiblePosition&) { }
inline void AXObjectCache::postTextStateChangeNotification(Node*, const AXTextStateChangeIntent&, const VisibleSelection&) { }
inline void AXObjectCache::recomputeIsIgnored(RenderObject*) { }
inline void AXObjectCache::handleTextChanged(AccessibilityObject*) { }
inline void AXObjectCache::updateCacheAfterNodeIsAttached(Node*) { }
inline void AXObjectCache::updateLoadingProgress(double) { }
inline SimpleRange AXObjectCache::rangeForNodeContents(Node& node) { return makeRangeSelectingNodeContents(node); }
inline std::optional<Vector<AXID>> AXObjectCache::relatedObjectIDsFor(const AXCoreObject&, AXRelationType) { return std::nullopt; }
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
