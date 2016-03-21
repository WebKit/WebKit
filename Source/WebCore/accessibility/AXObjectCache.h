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

#ifndef AXObjectCache_h
#define AXObjectCache_h

#include "AXTextStateChangeIntent.h"
#include "AccessibilityObject.h"
#include "Range.h"
#include "Timer.h"
#include "VisibleUnits.h"
#include <limits.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class HTMLAreaElement;
class Node;
class Page;
class RenderObject;
class ScrollView;
class VisiblePosition;
class Widget;

struct TextMarkerData {
    AXID axID;
    Node* node;
    int offset;
    int characterStartIndex;
    int characterOffset;
    bool ignored;
    EAffinity affinity;
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
    bool isEqual(CharacterOffset& other) const
    {
        if (isNull() || other.isNull())
            return false;
        return node == other.node && startIndex == other.startIndex && offset == other.offset;
    }
};

class AXComputedObjectAttributeCache {
public:
    AccessibilityObjectInclusion getIgnored(AXID) const;
    void setIgnored(AXID, AccessibilityObjectInclusion);

private:
    struct CachedAXObjectAttributes {
        CachedAXObjectAttributes() : ignored(DefaultBehavior) { }

        AccessibilityObjectInclusion ignored;
    };

    HashMap<AXID, CachedAXObjectAttributes> m_idMapping;
};

#if !PLATFORM(COCOA)
enum AXTextChange { AXTextInserted, AXTextDeleted, AXTextAttributesChanged };
#endif

enum PostTarget { TargetElement, TargetObservableParent };

enum PostType { PostSynchronously, PostAsynchronously };

class AXObjectCache {
    WTF_MAKE_NONCOPYABLE(AXObjectCache); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AXObjectCache(Document&);
    ~AXObjectCache();

    WEBCORE_EXPORT static AccessibilityObject* focusedUIElementForPage(const Page*);

    // Returns the root object for the entire document.
    WEBCORE_EXPORT AccessibilityObject* rootObject();
    // Returns the root object for a specific frame.
    WEBCORE_EXPORT AccessibilityObject* rootObjectForFrame(Frame*);
    
    // For AX objects with elements that back them.
    AccessibilityObject* getOrCreate(RenderObject*);
    AccessibilityObject* getOrCreate(Widget*);
    AccessibilityObject* getOrCreate(Node*);

    // used for objects without backing elements
    AccessibilityObject* getOrCreate(AccessibilityRole);
    
    // will only return the AccessibilityObject if it already exists
    AccessibilityObject* get(RenderObject*);
    AccessibilityObject* get(Widget*);
    AccessibilityObject* get(Node*);
    
    void remove(RenderObject*);
    void remove(Node*);
    void remove(Widget*);
    void remove(AXID);

    void detachWrapper(AccessibilityObject*, AccessibilityDetachmentType);
    void attachWrapper(AccessibilityObject*);
    void childrenChanged(Node*, Node* newChild = nullptr);
    void childrenChanged(RenderObject*, RenderObject* newChild = nullptr);
    void childrenChanged(AccessibilityObject*);
    void checkedStateChanged(Node*);
    void selectedChildrenChanged(Node*);
    void selectedChildrenChanged(RenderObject*);
    // Called by a node when text or a text equivalent (e.g. alt) attribute is changed.
    void textChanged(Node*);
    void textChanged(RenderObject*);
    // Called when a node has just been attached, so we can make sure we have the right subclass of AccessibilityObject.
    void updateCacheAfterNodeIsAttached(Node*);

    void handleActiveDescendantChanged(Node*);
    void handleAriaRoleChanged(Node*);
    void handleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode);
    void handleScrolledToAnchor(const Node* anchorNode);
    void handleAriaExpandedChange(Node*);
    void handleScrollbarUpdate(ScrollView*);
    
    void handleAriaModalChange(Node*);
    Node* ariaModalNode();

    void handleAttributeChanged(const QualifiedName& attrName, Element*);
    void recomputeIsIgnored(RenderObject* renderer);

#if HAVE(ACCESSIBILITY)
    WEBCORE_EXPORT static void enableAccessibility();
    WEBCORE_EXPORT static void disableAccessibility();

    // Enhanced user interface accessibility can be toggled by the assistive technology.
    WEBCORE_EXPORT static void setEnhancedUserInterfaceAccessibility(bool flag);
    
    static bool accessibilityEnabled() { return gAccessibilityEnabled; }
    static bool accessibilityEnhancedUserInterfaceEnabled() { return gAccessibilityEnhancedUserInterfaceEnabled; }
#else
    static void enableAccessibility() { }
    static void disableAccessibility() { }
    static void setEnhancedUserInterfaceAccessibility(bool) { }
    static bool accessibilityEnabled() { return false; }
    static bool accessibilityEnhancedUserInterfaceEnabled() { return false; }
#endif

    void removeAXID(AccessibilityObject*);
    bool isIDinUse(AXID id) const { return m_idsInUse.contains(id); }

    const Element* rootAXEditableElement(const Node*);
    bool nodeIsTextControl(const Node*);

    AXID platformGenerateAXID() const;
    AccessibilityObject* objectFromAXID(AXID id) const { return m_objects.get(id); }

    // Text marker utilities.
    void textMarkerDataForVisiblePosition(TextMarkerData&, const VisiblePosition&);
    void textMarkerDataForCharacterOffset(TextMarkerData&, const CharacterOffset&);
    void textMarkerDataForNextCharacterOffset(TextMarkerData&, const CharacterOffset&);
    void textMarkerDataForPreviousCharacterOffset(TextMarkerData&, const CharacterOffset&);
    VisiblePosition visiblePositionForTextMarkerData(TextMarkerData&);
    CharacterOffset characterOffsetForTextMarkerData(TextMarkerData&);
    // Use ignoreNextNodeStart/ignorePreviousNodeEnd to determine the behavior when we are at node boundary. 
    CharacterOffset nextCharacterOffset(const CharacterOffset&, bool ignoreNextNodeStart = true);
    CharacterOffset previousCharacterOffset(const CharacterOffset&, bool ignorePreviousNodeEnd = true);
    void startOrEndTextMarkerDataForRange(TextMarkerData&, RefPtr<Range>, bool);
    CharacterOffset startOrEndCharacterOffsetForRange(RefPtr<Range>, bool);
    AccessibilityObject* accessibilityObjectForTextMarkerData(TextMarkerData&);
    RefPtr<Range> rangeForUnorderedCharacterOffsets(const CharacterOffset&, const CharacterOffset&);
    static RefPtr<Range> rangeForNodeContents(Node*);
    static int lengthForRange(Range*);
    
    // Word boundary
    CharacterOffset nextWordEndCharacterOffset(const CharacterOffset&);
    CharacterOffset previousWordStartCharacterOffset(const CharacterOffset&);
    RefPtr<Range> leftWordRange(const CharacterOffset&);
    RefPtr<Range> rightWordRange(const CharacterOffset&);
    
    // Paragraph
    RefPtr<Range> paragraphForCharacterOffset(const CharacterOffset&);
    CharacterOffset nextParagraphEndCharacterOffset(const CharacterOffset&);
    CharacterOffset previousParagraphStartCharacterOffset(const CharacterOffset&);
    
    // Sentence
    RefPtr<Range> sentenceForCharacterOffset(const CharacterOffset&);
    CharacterOffset nextSentenceEndCharacterOffset(const CharacterOffset&);
    CharacterOffset previousSentenceStartCharacterOffset(const CharacterOffset&);
    
    // Bounds
    CharacterOffset characterOffsetForPoint(const IntPoint&, AccessibilityObject*);
    IntRect absoluteCaretBoundsForCharacterOffset(const CharacterOffset&);
    CharacterOffset characterOffsetForBounds(const IntRect&, bool);
    
    // Lines
    CharacterOffset endCharacterOffsetOfLine(const CharacterOffset&);
    CharacterOffset startCharacterOffsetOfLine(const CharacterOffset&);
    
    // Index
    CharacterOffset characterOffsetForIndex(int, const AccessibilityObject*);
    int indexForCharacterOffset(const CharacterOffset&, AccessibilityObject*);

    enum AXNotification {
        AXActiveDescendantChanged,
        AXAutocorrectionOccured,
        AXCheckedStateChanged,
        AXChildrenChanged,
        AXFocusedUIElementChanged,
        AXLayoutComplete,
        AXLoadComplete,
        AXNewDocumentLoadComplete,
        AXSelectedChildrenChanged,
        AXSelectedTextChanged,
        AXValueChanged,
        AXScrolledToAnchor,
        AXLiveRegionCreated,
        AXLiveRegionChanged,
        AXMenuListItemSelected,
        AXMenuListValueChanged,
        AXMenuClosed,
        AXMenuOpened,
        AXRowCountChanged,
        AXRowCollapsed,
        AXRowExpanded,
        AXExpandedChanged,
        AXInvalidStatusChanged,
        AXTextChanged,
        AXAriaAttributeChanged,
        AXElementBusyChanged
    };

    void postNotification(RenderObject*, AXNotification, PostTarget = TargetElement, PostType = PostAsynchronously);
    void postNotification(Node*, AXNotification, PostTarget = TargetElement, PostType = PostAsynchronously);
    void postNotification(AccessibilityObject*, Document*, AXNotification, PostTarget = TargetElement, PostType = PostAsynchronously);

#ifndef NDEBUG
    void showIntent(const AXTextStateChangeIntent&);
#endif

    void setTextSelectionIntent(const AXTextStateChangeIntent&);
    void setIsSynchronizingSelection(bool);

    void postTextStateChangeNotification(Node*, AXTextEditType, const String&, const VisiblePosition&);
    void postTextReplacementNotification(Node*, AXTextEditType deletionType, const String& deletedText, AXTextEditType insertionType, const String& insertedText, const VisiblePosition&);
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

    void clearTextMarkerNodesInUse(Document*);

    void startCachingComputedObjectAttributesUntilTreeMutates();
    void stopCachingComputedObjectAttributes();

    AXComputedObjectAttributeCache* computedObjectAttributeCache() { return m_computedObjectAttributeCache.get(); }

    Document& document() const { return m_document; }

#if PLATFORM(MAC)
    static void setShouldRepostNotificationsForTests(bool value);
#endif

protected:
    void postPlatformNotification(AccessibilityObject*, AXNotification);
    void platformHandleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode);

#if PLATFORM(COCOA)
    void postTextStateChangePlatformNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&);
    void postTextStateChangePlatformNotification(AccessibilityObject*, AXTextEditType, const String&, const VisiblePosition&);
    void postTextReplacementPlatformNotification(AccessibilityObject*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&);
#else
    static AXTextChange textChangeForEditType(AXTextEditType);
    void nodeTextChangePlatformNotification(AccessibilityObject*, AXTextChange, unsigned, const String&);
#endif

    void frameLoadingEventPlatformNotification(AccessibilityObject*, AXLoadingEvent);
    void textChanged(AccessibilityObject*);
    void labelChanged(Element*);

    // This is a weak reference cache for knowing if Nodes used by TextMarkers are valid.
    void setNodeInUse(Node* n) { m_textMarkerNodes.add(n); }
    void removeNodeForUse(Node* n) { m_textMarkerNodes.remove(n); }
    bool isNodeInUse(Node* n) { return m_textMarkerNodes.contains(n); }
    
    // CharacterOffset functions.
    enum TraverseOption { TraverseOptionDefault = 1 << 0, TraverseOptionToNodeEnd = 1 << 1, TraverseOptionIncludeStart = 1 << 2 };
    Node* nextNode(Node*) const;
    Node* previousNode(Node*) const;
    CharacterOffset traverseToOffsetInRange(RefPtr<Range>, int, TraverseOption = TraverseOptionDefault, bool stayWithinRange = false);
    VisiblePosition visiblePositionFromCharacterOffset(const CharacterOffset&);
    CharacterOffset characterOffsetFromVisiblePosition(const VisiblePosition&);
    void setTextMarkerDataWithCharacterOffset(TextMarkerData&, const CharacterOffset&);
    UChar32 characterAfter(const CharacterOffset&);
    UChar32 characterBefore(const CharacterOffset&);
    CharacterOffset characterOffsetForNodeAndOffset(Node&, int, TraverseOption = TraverseOptionDefault);
    CharacterOffset previousBoundary(const CharacterOffset&, BoundarySearchFunction);
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

    void postTextStateChangeNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&);

    bool enqueuePasswordValueChangeNotification(AccessibilityObject*);
    void passwordNotificationPostTimerFired();

    void handleMenuOpened(Node*);
    void handleLiveRegionCreated(Node*);
    void handleMenuItemSelected(Node*);
    
    // aria-modal related
    void findAriaModalNodes();
    void updateCurrentAriaModalNode();
    bool isNodeVisible(Node*) const;

    Document& m_document;
    HashMap<AXID, RefPtr<AccessibilityObject>> m_objects;
    HashMap<RenderObject*, AXID> m_renderObjectMapping;
    HashMap<Widget*, AXID> m_widgetObjectMapping;
    HashMap<Node*, AXID> m_nodeObjectMapping;
    HashSet<Node*> m_textMarkerNodes;
    std::unique_ptr<AXComputedObjectAttributeCache> m_computedObjectAttributeCache;
    WEBCORE_EXPORT static bool gAccessibilityEnabled;
    WEBCORE_EXPORT static bool gAccessibilityEnhancedUserInterfaceEnabled;

    HashSet<AXID> m_idsInUse;

    Timer m_notificationPostTimer;
    Vector<std::pair<RefPtr<AccessibilityObject>, AXNotification>> m_notificationsToPost;

    Timer m_passwordNotificationPostTimer;

    ListHashSet<RefPtr<AccessibilityObject>> m_passwordNotificationsToPost;
    
    Timer m_liveRegionChangedPostTimer;
    ListHashSet<RefPtr<AccessibilityObject>> m_liveRegionObjectsSet;
    
    Node* m_currentAriaModalNode;
    ListHashSet<Node*> m_ariaModalNodesSet;

    AXTextStateChangeIntent m_textSelectionIntent;
    bool m_isSynchronizingSelection { false };
};

class AXAttributeCacheEnabler
{
public:
    explicit AXAttributeCacheEnabler(AXObjectCache *cache);
    ~AXAttributeCacheEnabler();

#if HAVE(ACCESSIBILITY)
private:
    AXObjectCache* m_cache;
#endif
};

bool nodeHasRole(Node*, const String& role);
// This will let you know if aria-hidden was explicitly set to false.
bool isNodeAriaVisible(Node*);
    
#if !HAVE(ACCESSIBILITY)
inline AccessibilityObjectInclusion AXComputedObjectAttributeCache::getIgnored(AXID) const { return DefaultBehavior; }
inline void AXComputedObjectAttributeCache::setIgnored(AXID, AccessibilityObjectInclusion) { }
inline AXObjectCache::AXObjectCache(Document& document) : m_document(document), m_notificationPostTimer(nullptr), m_passwordNotificationPostTimer(nullptr), m_liveRegionChangedPostTimer(nullptr) { }
inline AXObjectCache::~AXObjectCache() { }
inline AccessibilityObject* AXObjectCache::focusedUIElementForPage(const Page*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::get(RenderObject*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::get(Node*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::get(Widget*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(AccessibilityRole) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(RenderObject*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(Node*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::getOrCreate(Widget*) { return nullptr; }
inline AccessibilityObject* AXObjectCache::rootObject() { return nullptr; }
inline AccessibilityObject* AXObjectCache::rootObjectForFrame(Frame*) { return nullptr; }
inline bool nodeHasRole(Node*, const String&) { return false; }
inline void AXObjectCache::startCachingComputedObjectAttributesUntilTreeMutates() { }
inline void AXObjectCache::stopCachingComputedObjectAttributes() { }
inline bool isNodeAriaVisible(Node*) { return true; }
inline const Element* AXObjectCache::rootAXEditableElement(const Node*) { return nullptr; }
inline Node* AXObjectCache::ariaModalNode() { return nullptr; }
inline void AXObjectCache::attachWrapper(AccessibilityObject*) { }
inline void AXObjectCache::checkedStateChanged(Node*) { }
inline void AXObjectCache::childrenChanged(RenderObject*, RenderObject*) { }
inline void AXObjectCache::childrenChanged(Node*, Node*) { }
inline void AXObjectCache::childrenChanged(AccessibilityObject*) { }
inline void AXObjectCache::textChanged(RenderObject*) { }
inline void AXObjectCache::textChanged(Node*) { }
inline void AXObjectCache::textChanged(AccessibilityObject*) { }
inline void AXObjectCache::updateCacheAfterNodeIsAttached(Node*) { }
inline void AXObjectCache::detachWrapper(AccessibilityObject*, AccessibilityDetachmentType) { }
inline void AXObjectCache::frameLoadingEventNotification(Frame*, AXLoadingEvent) { }
inline void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject*, AXLoadingEvent) { }
inline void AXObjectCache::handleActiveDescendantChanged(Node*) { }
inline void AXObjectCache::handleAriaExpandedChange(Node*) { }
inline void AXObjectCache::handleAriaRoleChanged(Node*) { }
inline void AXObjectCache::handleAriaModalChange(Node*) { }
inline void AXObjectCache::handleFocusedUIElementChanged(Node*, Node*) { }
inline void AXObjectCache::handleScrollbarUpdate(ScrollView*) { }
inline void AXObjectCache::handleAttributeChanged(const QualifiedName&, Element*) { }
inline void AXObjectCache::recomputeIsIgnored(RenderObject*) { }
inline void AXObjectCache::handleScrolledToAnchor(const Node*) { }
inline void AXObjectCache::postTextStateChangeNotification(Node*, const AXTextStateChangeIntent&, const VisibleSelection&) { }
inline void AXObjectCache::postTextStateChangeNotification(Node*, AXTextEditType, const String&, const VisiblePosition&) { }
inline void AXObjectCache::postTextReplacementNotification(Node*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&) { }
inline void AXObjectCache::postNotification(AccessibilityObject*, Document*, AXNotification, PostTarget, PostType) { }
inline void AXObjectCache::postNotification(RenderObject*, AXNotification, PostTarget, PostType) { }
inline void AXObjectCache::postNotification(Node*, AXNotification, PostTarget, PostType) { }
inline void AXObjectCache::postPlatformNotification(AccessibilityObject*, AXNotification) { }
inline void AXObjectCache::postLiveRegionChangeNotification(AccessibilityObject*) { }
inline RefPtr<Range> AXObjectCache::rangeForNodeContents(Node*) { return nullptr; }
inline void AXObjectCache::remove(AXID) { }
inline void AXObjectCache::remove(RenderObject*) { }
inline void AXObjectCache::remove(Node*) { }
inline void AXObjectCache::remove(Widget*) { }
inline void AXObjectCache::selectedChildrenChanged(RenderObject*) { }
inline void AXObjectCache::selectedChildrenChanged(Node*) { }
inline void AXObjectCache::setIsSynchronizingSelection(bool) { }
inline void AXObjectCache::setTextSelectionIntent(const AXTextStateChangeIntent&) { }
inline RefPtr<Range> AXObjectCache::rangeForUnorderedCharacterOffsets(const CharacterOffset&, const CharacterOffset&) { return nullptr; }
inline IntRect AXObjectCache::absoluteCaretBoundsForCharacterOffset(const CharacterOffset&) { return IntRect(); }
inline CharacterOffset AXObjectCache::characterOffsetForIndex(int, const AccessibilityObject*) { return CharacterOffset(); }
inline CharacterOffset AXObjectCache::startOrEndCharacterOffsetForRange(RefPtr<Range>, bool) { return CharacterOffset(); }
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

inline AXAttributeCacheEnabler::AXAttributeCacheEnabler(AXObjectCache*) { }
inline AXAttributeCacheEnabler::~AXAttributeCacheEnabler() { }

#endif

}

#endif
