/*
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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

#include <WebCore/AXTextMarker.h>
#include <WebCore/AXTreeStore.h>
#include <WebCore/Document.h>
#include <WebCore/RenderView.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/StyleChange.h>
#include <WebCore/Timer.h>
#include <WebCore/VisibleUnits.h>
#include <limits.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/Platform.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

#if PLATFORM(COCOA)
#include <WebCore/AttributedString.h>
#include <wtf/RetainPtr.h>
#endif

OBJC_CLASS NSMutableArray;
OBJC_CLASS NSString;

namespace WTF {
class TextStream;
}

namespace WebCore {

class AXComputedObjectAttributeCache;
class AXGeometryManager;
class AXIsolatedTree;
class AXLiveRegionManager;
class AXRemoteFrame;
class AccessibilityNodeObject;
class AccessibilityObject;
class AccessibilityRenderObject;
class Document;
class HTMLAreaElement;
class HTMLDetailsElement;
class HTMLTableElement;
class HTMLTextFormControlElement;
class Node;
class Page;
class RenderBlock;
class RenderBlockFlow;
class RenderImage;
class RenderObject;
class RenderStyle;
class RenderText;
class RenderWidget;
class Scrollbar;
class ScrollView;
class VisiblePosition;
class Widget;

struct AXTextStateChangeIntent;
struct AriaNotifyOptions;
struct TextMarkerData;

enum class AXNotification : uint8_t;
enum class AXStreamOptions : uint16_t;
enum class AXProperty : uint16_t;
enum class LiveRegionStatus: uint8_t;
enum class TextMarkerOrigin : uint16_t;

struct CharacterOffset {
    RefPtr<Node> node;
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
    inline bool isEqual(const CharacterOffset& other) const;
    inline String debugDescription();
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

// When this is updated, WebCoreArgumentCoders.serialization.in must be updated as well.
struct AXDebugInfo {
    bool isAccessibilityEnabled;
    bool isAccessibilityThreadInitialized;
    String liveTree;
    String isolatedTree;
    uint64_t remoteTokenHash;
    uint64_t webProcessLocalTokenHash;
};

enum class NotifyPriority : uint8_t { Normal, High };

enum class InterruptBehavior : uint8_t { None, All, Pending };

enum class LiveRegionStatus : uint8_t { Off, Polite, Assertive };

// When this is updated, WebCoreArgumentCoders.serialization.in must be updated as well.
struct AriaNotifyData {
    String message;
    NotifyPriority priority { NotifyPriority::Normal };
    InterruptBehavior interrupt { InterruptBehavior::None };
    String language;
};

#if PLATFORM(COCOA)
// When this is updated, WebCoreArgumentCoders.serialization.in must be updated as well.
struct LiveRegionAnnouncementData {
    AttributedString message;
    LiveRegionStatus status { LiveRegionStatus::Polite };
};

struct AXTextChangeContext {
    AXTextStateChangeIntent intent;
    String deletedText;
    String insertedText;
    VisibleSelection selection;
};
#endif // PLATFORM(COCOA)

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

enum class AXLoadingEvent : uint8_t {
    Started,
    Reloaded,
    Failed,
    Finished
};

#if !PLATFORM(COCOA)
enum class AXTextChange : uint8_t { Inserted, Deleted, Replaced, AttributesChanged };
#endif

enum class PostTarget { Element, ObservableParent };

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXObjectCache);
class AXObjectCache final : public CanMakeWeakPtr<AXObjectCache>, public CanMakeCheckedPtr<AXObjectCache>
    , public AXTreeStore<AXObjectCache> {
    WTF_MAKE_NONCOPYABLE(AXObjectCache);
    WTF_MAKE_TZONE_ALLOCATED(AXObjectCache);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AXObjectCache);
    friend class AXIsolatedTree;
    friend class AXTextMarker;
    friend WTF::TextStream& operator<<(WTF::TextStream&, AXObjectCache&);
public:
    explicit AXObjectCache(LocalFrame&, Document*);
    ~AXObjectCache();

    String debugDescription() const;

    // Returns the root object for a specific frame.
    WEBCORE_EXPORT AXCoreObject* rootObjectForFrame(LocalFrame&);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    WEBCORE_EXPORT void buildIsolatedTreeIfNeeded();
#endif

    // Creation/retrieval of AX objects associated with a DOM or RenderTree object.
    inline AccessibilityObject* getOrCreate(RenderObject* renderer)
    {
        return renderer ? getOrCreate(*renderer) : nullptr;
    }
    AccessibilityObject* getOrCreate(RenderObject&);

    inline AccessibilityObject* getOrCreate(Widget* widget)
    {
        return widget ? getOrCreate(*widget) : nullptr;
    }
    AccessibilityObject* getOrCreate(Widget&);

    enum class IsPartOfRelation : bool { No, Yes };
    inline AccessibilityObject* getOrCreate(Node* node, IsPartOfRelation isPartOfRelation = IsPartOfRelation::No)
    {
        return node ? getOrCreate(*node, isPartOfRelation) : nullptr;
    }
    AccessibilityObject* getOrCreate(Node&, IsPartOfRelation = IsPartOfRelation::No);
    AccessibilityObject* getOrCreate(Element&, IsPartOfRelation = IsPartOfRelation::No);
    // Out-of-line implementations (necessary for use outside of WebCore). Our inlined
    // implementations (important for performance inside WebCore) cannot be exported.
    WEBCORE_EXPORT AccessibilityObject* exportedGetOrCreate(Node&);
    WEBCORE_EXPORT AccessibilityObject* exportedGetOrCreate(Node*);

    // used for objects without backing elements
    AccessibilityObject* create(AccessibilityRole);

    // Will only return the AccessibilityObject if it already exists.
    inline AccessibilityObject* get(RenderObject* renderer) const
    {
        return renderer ? get(*renderer) : nullptr;
    }
    inline AccessibilityObject* get(RenderObject& renderer) const
    {
        std::optional axID = getAXID(renderer);
        return axID ? m_objects.get(*axID) : nullptr;
    }

    inline AccessibilityObject* get(Widget* widget) const
    {
        return widget ? get(*widget) : nullptr;
    }
    inline AccessibilityObject* get(Widget& widget) const
    {
        auto axID = m_widgetIdMapping.getOptional(widget);
        return axID ? m_objects.get(*axID) : nullptr;
    }

    inline AccessibilityObject* get(Node* node) const
    {
        return node ? get(*node) : nullptr;
    }
    inline AccessibilityObject* get(Node& node) const
    {
        if (CheckedPtr document = dynamicDowncast<Document>(node)) [[unlikely]]
            return get(document->renderView());
        return m_nodeObjectMapping.get(node);
    }
    inline AccessibilityObject* get(Element& element) const
    {
        return m_nodeObjectMapping.get(element);
    }
    inline std::optional<AXID> getAXID(RenderObject& renderer) const
    {
        if (RefPtr node = renderer.node())
            return m_nodeIdMapping.getOptional(*node);
        return m_renderObjectIdMapping.getOptional(const_cast<RenderObject&>(renderer));
    }

    void remove(RenderObject&);
    void remove(Node&);
    void remove(Widget&);
    void remove(std::optional<AXID> axID)
    {
        if (axID)
            remove(*axID);
    }
    void remove(AXID);

#if !PLATFORM(COCOA) && !USE(ATSPI)
    void detachWrapper(AXCoreObject*, AccessibilityDetachmentType);
#endif
private:
    using DOMObjectVariant = Variant<std::nullptr_t, RenderObject*, Node*, Widget*>;
    void cacheAndInitializeWrapper(AccessibilityObject&, DOMObjectVariant = nullptr);
    void attachWrapper(AccessibilityObject&);

    AccessibilityObject* getOrCreateSlow(Node&, IsPartOfRelation);

public:
    void onPageActivityStateChange(OptionSet<ActivityState>);
    void setPageActivityState(OptionSet<ActivityState> state) { m_pageActivityState = state; }
    OptionSet<ActivityState> pageActivityState() const { return m_pageActivityState; }

    inline void childrenChanged(Node& node)
    {
        if (!node.renderer()) {
            // We only need to handle DOM changes for things that don't have renderers.
            // If something does have a renderer, we would already get children-changed notifications
            // from the render tree.
            childrenChanged(get(node));
        }
    }
    void childrenChanged(RenderObject&, RenderObject* newChild = nullptr);
    void childrenChanged(AccessibilityObject*);
    void onDetailsSummarySlotChange(const HTMLDetailsElement&);
    void onDragElementChanged(Element* oldElement, Element* newElement);
    void onDraggingStarted(Element&);
    void onDraggingEnded(Element&);
    void onDraggingEnteredDropZone(Element&);
    void onDraggingExitedDropZone(Element&);
    void onDraggingDropped(Element&);
    void onEventListenerAdded(Node&, const AtomString& eventType);
    void onEventListenerRemoved(Node&, const AtomString& eventType);
    void onFocusChange(Element* oldElement, Element* newElement);
    void onInertOrVisibilityChange(RenderElement&);
    void onPopoverToggle(const HTMLElement&);
    void onRadioGroupMembershipChanged(HTMLElement&);
    void onScrollbarFrameRectChange(const Scrollbar&);
    void onSelectedOptionChanged(Element&);
    void onSelectedOptionChanged(RenderObject&, int);
    void onSelectedTextChanged(const VisiblePositionRange&, AccessibilityObject* = nullptr);
    void onSlottedContentChange(const HTMLSlotElement&);
    void onStyleChange(Element&, OptionSet<Style::Change>, const RenderStyle* oldStyle, const RenderStyle* newStyle);
    void onStyleChange(RenderText&, StyleDifference, const RenderStyle* oldStyle, const RenderStyle& newStyle);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void onAccessibilityPaintStarted();
    void onAccessibilityPaintFinished();
    // Returns true if the font changes, requiring all descendants to update the Font property.
    bool onFontChange(Element&, const RenderStyle*, const RenderStyle*);
    // Returns true if the text color changes, requiring all descendants to update the TextColor property.
    bool onTextColorChange(Element&, const RenderStyle*, const RenderStyle*);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void onTextSecurityChanged(HTMLInputElement&);
    void onTitleChange(Document&);
    void onValidityChange(Element&);
    void onTextCompositionChange(Node&, CompositionState, bool, const String&, size_t, bool);
    void onWidgetVisibilityChanged(RenderWidget&);
    void valueChanged(Element&);
    void checkedStateChanged(Element&);
    void autofillTypeChanged(HTMLInputElement&);
    void handleRoleChanged(AccessibilityObject&, AccessibilityRole previousRole);
    void handleReferenceTargetChanged();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void columnIndexChanged(AccessibilityObject&);
    void rowIndexChanged(AccessibilityObject&);
#endif

    void onRendererCreated(Node&);
#if PLATFORM(MAC)
    void onDocumentRenderTreeCreation(const Document&);
#endif
#if ENABLE(AX_THREAD_TEXT_APIS)
    void onTextRunsChanged(const RenderObject&);
#endif

    void onLaidOutInlineContent(const RenderBlockFlow& renderBlock) { setDirtyStitchGroups(renderBlock); }
    const Vector<Vector<AXID>>* stitchGroupsOwnedBy(AccessibilityObject&);

    void updateLoadingProgress(double);
    void loadingFinished() { updateLoadingProgress(1); }
    double loadingProgress() const { return m_loadingProgress; }

    void onTopDocumentLoaded(RenderObject&);
    void onNonTopDocumentLoaded(RenderObject&);
    void handlePageEditibilityChanged(Document&);
    void onAutocorrectionOccured(Element&);
    void onEditableTextValueChanged(Node&);
    void onDocumentInitialFocus(Node&);
    void onLayoutComplete(RenderObject&);

    struct AttributeChange {
        WeakPtr<Element, WeakPtrImplWithEventTargetData> element { nullptr };
        QualifiedName attrName;
        AtomString oldValue;
        AtomString newValue;
    };
    using DeferredCollection = Variant<HashMap<Element*, String>
        , HashSet<AXID>
        , ListHashSet<Node*>
        , ListHashSet<Ref<AccessibilityObject>>
        , Vector<AttributeChange>
        , Vector<std::pair<Node*, Node*>>
        , WeakHashSet<Element, WeakPtrImplWithEventTargetData>
        , WeakHashSet<HTMLTableElement, WeakPtrImplWithEventTargetData>
        , WeakHashSet<AccessibilityObject>
        , WeakHashSet<AccessibilityNodeObject>
        , WeakListHashSet<Node, WeakPtrImplWithEventTargetData>
        , WeakListHashSet<Element, WeakPtrImplWithEventTargetData>
        , WeakHashMap<Element, String, WeakPtrImplWithEventTargetData>>;
    void deferFocusedUIElementChangeIfNeeded(Node* oldFocusedNode, Node* newFocusedNode);
    void deferModalChange(Element&);
    void deferMenuListValueChange(Element*);
    void deferElementAddedOrRemoved(Element*);
    void handleScrolledToAnchor(const Node& anchorNode);
    void onScrollbarUpdate(ScrollView&);
    void onRemoteFrameInitialized(AXRemoteFrame&);

    bool isRetrievingCurrentModalNode() { return m_isRetrievingCurrentModalNode; }
    Node* modalNode();

    void deferAttributeChangeIfNeeded(Element&, const QualifiedName&, const AtomString&, const AtomString&);
    void recomputeIsIgnored(RenderObject&);
    void recomputeIsIgnored(Node*);

    static void enableAccessibility();
    static void disableAccessibility();
#if PLATFORM(MAC)
    WEBCORE_EXPORT static bool isAppleInternalInstall();
#endif
    static bool forceDeferredSpellChecking();
    static void setForceDeferredSpellChecking(bool);
#if PLATFORM(MAC)
    static bool shouldSpellCheck();
#else
    static bool shouldSpellCheck() { return true; }
#endif

    WEBCORE_EXPORT AccessibilityObject* focusedObjectForPage(const Page*);
    WEBCORE_EXPORT AccessibilityObject* focusedObjectForLocalFrame();

    // Enhanced user interface accessibility can be toggled by the assistive technology.
    WEBCORE_EXPORT static void setEnhancedUserInterfaceAccessibility(bool flag);

    static bool accessibilityEnabled();
    WEBCORE_EXPORT static bool accessibilityEnhancedUserInterfaceEnabled();
#if ENABLE(AX_THREAD_TEXT_APIS)
    static bool useAXThreadTextApis() { return gAccessibilityThreadTextApisEnabled && !isMainThread(); }
    static bool shouldCreateAXThreadCompatibleMarkers() { return gAccessibilityThreadTextApisEnabled && isIsolatedTreeEnabled(); }
#endif
    static bool isAXTextStitchingEnabled() { return gAccessibilityTextStitchingEnabled; }

#if PLATFORM(COCOA)
    static bool shouldRepostNotificationsForTests() { return gShouldRepostNotificationsForTests; }

    static void initializeUserDefaultValues();
    static bool accessibilityDOMIdentifiersEnabled() { return gAccessibilityDOMIdentifiersEnabled; }
#endif

    static bool forceInitialFrameCaching() { return gForceInitialFrameCaching; }
    WEBCORE_EXPORT static void setForceInitialFrameCaching(bool);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    static bool shouldServeInitialCachedFrame();
#endif

    const Element* rootAXEditableElement(const Node*);
    bool elementIsTextControl(const Element&);

    AccessibilityObject* objectForID(const AXID id) const { return m_objects.get(id); }
    template<typename U> Vector<Ref<AXCoreObject>> objectsForIDs(const U&) const;
    Node* nodeForID(std::optional<AXID>) const;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void onPaint(const RenderObject&, IntRect&&) const;
    void onPaint(const Widget&, IntRect&&) const;
    void onPaint(const RenderText&, size_t lineIndex);
#else
    NO_RETURN_DUE_TO_ASSERT void onPaint(const RenderObject&, IntRect&&) const { ASSERT_NOT_REACHED(); }
    NO_RETURN_DUE_TO_ASSERT void onPaint(const Widget&, IntRect&&) const { ASSERT_NOT_REACHED(); }
    NO_RETURN_DUE_TO_ASSERT void onPaint(const RenderText&, size_t) { ASSERT_NOT_REACHED(); }
#endif

    // Text marker utilities.
    std::optional<TextMarkerData> textMarkerDataForVisiblePosition(const VisiblePosition&, TextMarkerOrigin = static_cast<TextMarkerOrigin>(0));
    TextMarkerData textMarkerDataForCharacterOffset(const CharacterOffset&, TextMarkerOrigin = static_cast<TextMarkerOrigin>(0));
    TextMarkerData textMarkerDataForNextCharacterOffset(const CharacterOffset&);
    AXTextMarker nextTextMarker(const AXTextMarker&);
    TextMarkerData textMarkerDataForPreviousCharacterOffset(const CharacterOffset&);
    AXTextMarker previousTextMarker(const AXTextMarker&);
    VisiblePosition visiblePositionForTextMarkerData(const TextMarkerData&);
    CharacterOffset characterOffsetForTextMarkerData(const TextMarkerData&);
    // Use ignoreNextNodeStart/ignorePreviousNodeEnd to determine the behavior when we are at node boundary.
    CharacterOffset nextCharacterOffset(const CharacterOffset&, bool ignoreNextNodeStart = true);
    CharacterOffset previousCharacterOffset(const CharacterOffset&, bool ignorePreviousNodeEnd = true);
    TextMarkerData startOrEndTextMarkerDataForRange(const SimpleRange&, bool);
    CharacterOffset startOrEndCharacterOffsetForRange(const SimpleRange&, bool, bool enterTextControls = false);
    AccessibilityObject* objectForTextMarkerData(const TextMarkerData&);
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

    void postNotification(RenderObject*, AXNotification, PostTarget = PostTarget::Element);
    void postNotification(Node*, AXNotification, PostTarget = PostTarget::Element);
    void postNotification(AccessibilityObject*, Document*, AXNotification, PostTarget = PostTarget::Element);
    void postNotification(AccessibilityObject* object, AXNotification notification)
    {
        if (object)
            postNotification(*object, notification);
    }
    void postNotification(AccessibilityObject&, AXNotification);
    void postARIANotifyNotification(Node&, const String&, const AriaNotifyOptions&);
    void postLiveRegionNotification(AccessibilityObject&, LiveRegionStatus, const AttributedString&);
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
    void postLiveRegionChangeNotification(AccessibilityObject&);

    void frameLoadingEventNotification(LocalFrame*, AXLoadingEvent);

    void prepareForDocumentDestruction(const Document&);

    void startCachingComputedObjectAttributesUntilTreeMutates();
    void stopCachingComputedObjectAttributes();

    AXComputedObjectAttributeCache* computedObjectAttributeCache() { return m_computedObjectAttributeCache.get(); }

    Document* document() const { return m_document.get(); }
    RefPtr<Document> protectedDocument() const;
    constexpr const std::optional<FrameIdentifier>& frameID() const { return m_frameID; }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    inline void objectBecameIgnored(const AccessibilityObject&);
    inline void objectBecameUnignored(const AccessibilityObject&);
#endif

#if PLATFORM(COCOA)
    static void setShouldRepostNotificationsForTests(bool);
#endif

    void deferRecomputeIsIgnoredIfNeeded(Element*);
    void deferRecomputeIsIgnored(Element*);
    void deferRecomputeTableIsExposed(Element*);
    void deferRecomputeTableCellSlots(AccessibilityNodeObject&);
    void deferTextChangedIfNeeded(Node*);
    void deferSelectedChildrenChangedIfNeeded(Element&);
    WEBCORE_EXPORT void performDeferredCacheUpdate(ForceLayout);
    void deferTextReplacementNotificationForTextControl(HTMLTextFormControlElement&, const String& previousValue);

    std::optional<SimpleRange> rangeMatchesTextNearRange(const SimpleRange&, const String&);

    static ASCIILiteral notificationPlatformName(AXNotification);

    WEBCORE_EXPORT AXTreeData treeData(std::optional<OptionSet<AXStreamOptions>> = std::nullopt);

    enum class UpdateRelations : bool { No, Yes };
    // Returns the IDs of the objects that relate to the given object with the specified relationship.
    std::optional<ListHashSet<AXID>> relatedObjectIDsFor(const AXCoreObject&, AXRelation, UpdateRelations = UpdateRelations::Yes);
    void updateRelations(Element&, const QualifiedName&);

#if PLATFORM(IOS_FAMILY)
    void relayNotification(String&&, RetainPtr<NSData>&&);

    void setWillPresentDatePopover(bool willPresent) { m_willPresentDatePopover = willPresent; }
    bool willPresentDatePopover() const { return m_willPresentDatePopover; }
#endif

#if PLATFORM(MAC)
    AXCoreObject::AccessibilityChildrenVector sortedLiveRegions();
    AXCoreObject::AccessibilityChildrenVector sortedNonRootWebAreas();
    void deferSortForNewLiveRegion(Ref<AccessibilityObject>&&);
    void queueUnsortedObject(Ref<AccessibilityObject>&&, PreSortedObjectType);
    void addSortedObjects(Vector<Ref<AccessibilityObject>>&&, PreSortedObjectType);
    void removeLiveRegion(AccessibilityObject&);
    void initializeSortedIDLists();

    static bool clientIsInTestMode();
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    inline void scheduleObjectRegionsUpdate(bool scheduleImmediately = false);
    inline void willUpdateObjectRegions();
    WEBCORE_EXPORT static bool isIsolatedTreeEnabled();
    WEBCORE_EXPORT static void initializeAXThreadIfNeeded();
    WEBCORE_EXPORT static bool isAXThreadInitialized();
    WEBCORE_EXPORT RefPtr<AXIsolatedTree> getOrCreateIsolatedTree();

    static bool isAccessibilityList(Element&);
private:
    static bool clientSupportsIsolatedTree();
    // Propagates the root of the isolated tree back into the Core and WebKit.
    void setIsolatedTree(Ref<AXIsolatedTree>);
    void setIsolatedTreeFocusedObject(AccessibilityObject*);
    void buildIsolatedTree();
    void updateIsolatedTree(AccessibilityObject&, AXNotification);
    void updateIsolatedTree(AccessibilityObject*, AXNotification);
    void updateIsolatedTree(const Vector<std::pair<Ref<AccessibilityObject>, AXNotification>>&);
    void updateIsolatedTree(AccessibilityObject*, AXProperty) const;
    void updateIsolatedTree(AccessibilityObject&, AXProperty) const;
    void startUpdateTreeSnapshotTimer();
#endif

protected:
    void postPlatformNotification(AccessibilityObject&, AXNotification);
    void platformHandleFocusedUIElementChanged(Element* oldFocus, Element* newFocus);

    void platformPerformDeferredCacheUpdate();

#if PLATFORM(COCOA) || USE(ATSPI)
    void postTextSelectionChangePlatformNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&);
    void postTextStateChangePlatformNotification(AccessibilityObject*, AXTextEditType, const String&, const VisiblePosition&);
    void postTextReplacementPlatformNotification(AccessibilityObject*, AXTextEditType, const String&, AXTextEditType, const String&, const VisiblePosition&);
    void postTextReplacementPlatformNotificationForTextControl(AccessibilityObject*, const String& deletedText, const String& insertedText);
#else // PLATFORM(COCOA) || USE(ATSPI)
    static AXTextChange textChangeForEditType(AXTextEditType);
    void nodeTextChangePlatformNotification(AccessibilityObject*, AXTextChange, unsigned, const String&);
#endif

#if PLATFORM(MAC)
    void postUserInfoForChanges(AccessibilityObject&, AccessibilityObject&, RetainPtr<NSMutableArray>);
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT void postPlatformAnnouncementNotification(const String&);
    WEBCORE_EXPORT void postPlatformARIANotifyNotification(const String&, NotifyPriority, InterruptBehavior, const String&);
    WEBCORE_EXPORT void postPlatformLiveRegionNotification(AccessibilityObject&, LiveRegionStatus, const AttributedString&);
#else
    void postPlatformAnnouncementNotification(const String&) { }
    void postPlatformARIANotifyNotification(const String&, NotifyPriority, InterruptBehavior, const String&) { }
    void postPlatformLiveRegionNotification(AccessibilityObject&, LiveRegionStatus, const AttributedString&) { }
#endif

    void frameLoadingEventPlatformNotification(RenderView*, AXLoadingEvent);
    void handleLabelChanged(AccessibilityObject*);

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

    // Returns the object or nearest render-tree ancestor object that is already created (i.e.
    // retrievable by |get|, not |getOrCreate|).
    AccessibilityObject* getIncludingAncestors(RenderObject&) const;

    // The AX focus is more finegrained than the notion of focused Node. This method handles those cases where the focused AX object is a descendant or a sub-part of the focused Node.
    AccessibilityObject* focusedObjectForNode(Node*);
    static AccessibilityObject* focusedImageMapUIElement(HTMLAreaElement&);

    void notificationPostTimerFired();

    void liveRegionChangedNotificationPostTimerFired();

    void performCacheUpdateTimerFired() { performDeferredCacheUpdate(ForceLayout::No); }

    void postTextStateChangeNotification(AccessibilityObject*, const AXTextStateChangeIntent&, const VisibleSelection&);

#if PLATFORM(COCOA)
    bool enqueuePasswordNotification(AccessibilityObject&, AXTextChangeContext&&);
    void passwordNotificationTimerFired();
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void selectedTextRangeTimerFired();
    Seconds platformSelectedTextRangeDebounceInterval() const;
    void updateTreeSnapshotTimerFired();
    void processQueuedIsolatedNodeUpdates();

    void deferAddUnconnectedNode(AccessibilityObject&);
#if PLATFORM(MAC)
    void createIsolatedObjectIfNeeded(AccessibilityObject&);
#endif
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    void deferRowspanChange(AccessibilityObject*);
    void handleChildrenChanged(AccessibilityObject&);
    void handleAllDeferredChildrenChanged();
    void handleInputTypeChanged(Element&);
    void handleRoleChanged(Element&, const AtomString&, const AtomString&);
    void handleARIARoleDescriptionChanged(Element&);
    void handleMenuOpened(Element&);
    void handleLiveRegionCreated(Element&);
#if PLATFORM(COCOA)
    void initializeLiveRegionManager();
#endif
    void handleMenuItemSelected(Element*);
    void handleTabPanelSelected(Element*, Element*);
    void handleRowCountChanged(AccessibilityObject*, Document*);
    void handleAttributeChange(Element*, const QualifiedName&, const AtomString&, const AtomString&);
    bool shouldProcessAttributeChange(Element*, const QualifiedName&);
    void selectedChildrenChanged(Node*);
    void selectedChildrenChanged(RenderObject*);
    void handleScrollbarUpdate(ScrollView&);
    void handleActiveDescendantChange(Element&, const AtomString&, const AtomString&);
    void handleAriaExpandedChange(Element&);
    enum class UpdateModal : bool { No, Yes };
    void handleFocusedUIElementChanged(Element* oldFocus, Element* newFocus, UpdateModal = UpdateModal::Yes);
    void handleMenuListValueChanged(Element&);
    void handleTextChanged(AccessibilityObject*);
    void handleRecomputeCellSlots(AccessibilityNodeObject&);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void handleRowspanChanged(AccessibilityNodeObject&);
#endif

    // aria-modal or modal <dialog> related
    bool isModalElement(Element&) const;
    void findModalNodes();
    void updateCurrentModalNode();
    bool isNodeVisible(const Node*) const;
    bool modalElementHasAccessibleContent(Element&);

    void setDirtyStitchGroups(const RenderBlock&);

    // Relationships between objects.
    static Vector<QualifiedName>& relationAttributes();
    static AXRelation attributeToRelationType(const QualifiedName&);
    enum class AddSymmetricRelation : bool { No, Yes };
    static AXRelation symmetricRelation(AXRelation);
    bool addRelation(Element&, Element&, AXRelation);
    bool addRelation(AccessibilityObject*, AccessibilityObject*, AXRelation, AddSymmetricRelation = AddSymmetricRelation::Yes);
    bool addRelation(Element&, const QualifiedName&);
    void addLabelForRelation(Element&);
    bool removeRelation(Element&, AXRelation);
    void removeAllRelations(AXID);
    void removeRelationByID(AXID originID, AXID targetID, AXRelation);
    bool updateLabelFor(HTMLLabelElement&);
    void updateLabeledBy(Element*);
    void updateRelationsIfNeeded();
    void updateRelationsForTree(ContainerNode&);
    void relationsNeedUpdate(bool);
    void dirtyIsolatedTreeRelations();
    HashMap<AXID, AXRelations> relations();
    HashMap<AXID, LineRange> mostRecentlyPaintedText();
    const HashSet<AXID>& relationTargetIDs();
    bool isDescendantOfRelatedNode(Node&);

#if PLATFORM(MAC)
    AXTextStateChangeIntent inferDirectionFromIntent(AccessibilityObject&, const AXTextStateChangeIntent&, const VisibleSelection&);
#endif

    // Object creation.
    Ref<AccessibilityRenderObject> createObjectFromRenderer(RenderObject&);
    Ref<AccessibilityNodeObject> createFromNode(Node&);

    const WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    const std::optional<FrameIdentifier> m_frameID; // constant for object's lifetime.
    OptionSet<ActivityState> m_pageActivityState;
    HashMap<AXID, Ref<AccessibilityObject>> m_objects;

    // Should be used only for renderer-only (i.e. no DOM node) accessibility objects.
    WeakHashMap<RenderObject, AXID, SingleThreadWeakPtrImpl> m_renderObjectIdMapping;
    WeakHashMap<Widget, AXID, SingleThreadWeakPtrImpl> m_widgetIdMapping;
    // FIXME: The type for m_nodeIdMapping really should be:
    // HashMap<WeakRef<Node, WeakPtrImplWithEventTargetData>, AXID>
    // As this guarantees that we've called AXObjectCache::remove(Node&) for every node we store.
    // However, in rare circumstances, we can add a node to this map, then later the document associated
    // with the node loses its m_frame via detachFromFrame(). Then the node gets destroyed, but we can't
    // clean it up from this map, since existingAXObjectCache fails due to the nullptr m_frame.
    // This scenario seems extremely rare, and may only happen when the webpage is about to be destroyed anyways,
    // so, go with WeakHashMap now until we find a completely safe solution based on document / frame lifecycles.
    WeakHashMap<Node, AXID, WeakPtrImplWithEventTargetData> m_nodeIdMapping;
    // This map exists as an optimization, reducing the number of HashMap lookups that AXObjectCache::get
    // has to do to 1 (vs. a m_nodeIdMapping lookup, plus a m_objects lookup). Since this is one of
    // our hottest functions, the extra memory cost is worth it.
    WeakHashMap<Node, Ref<AccessibilityObject>, WeakPtrImplWithEventTargetData> m_nodeObjectMapping;

    WeakHashMap<RenderText, LineRange, SingleThreadWeakPtrImpl> m_mostRecentlyPaintedText;

    std::unique_ptr<AXComputedObjectAttributeCache> m_computedObjectAttributeCache;
#if PLATFORM(COCOA)
    std::unique_ptr<AXLiveRegionManager> m_liveRegionManager;
#endif

    WEBCORE_EXPORT static std::atomic<bool> gAccessibilityEnabled;
    static bool gAccessibilityEnhancedUserInterfaceEnabled;
    WEBCORE_EXPORT static std::atomic<bool> gForceDeferredSpellChecking;

    // FIXME: since the following only affects the behavior of isolated objects, we should move it into AXIsolatedTree in order to keep this class main thread only.
    static std::atomic<bool> gForceInitialFrameCaching;

#if ENABLE(AX_THREAD_TEXT_APIS)
    // Accessed on and off the main thread.
    static std::atomic<bool> gAccessibilityThreadTextApisEnabled;
#endif
    // Accessed on and off the main thread.
    static std::atomic<bool> gAccessibilityTextStitchingEnabled;

#if PLATFORM(COCOA)
    static std::atomic<bool> gAccessibilityDOMIdentifiersEnabled;
#endif

    Timer m_notificationPostTimer;
    Vector<std::pair<Ref<AccessibilityObject>, AXNotification>> m_notificationsToPost;

#if PLATFORM(COCOA)
    Timer m_passwordNotificationTimer;
    Deque<std::pair<Ref<AccessibilityObject>, AXTextChangeContext>> m_passwordNotifications;
#endif

    Timer m_liveRegionChangedPostTimer;
    ListHashSet<Ref<AccessibilityObject>> m_changedLiveRegions;

#if PLATFORM(MAC)
    // This block is PLATFORM(MAC) because the remote search API is currently only used on macOS.

    // AX tree-order-sorted list of a few types of objects. This is useful because some assistive
    // technologies send us frequent remote search requests for all the live regions or non-root webareas
    // on the page.
    bool m_sortedIDListsInitialized { false };
    Vector<AXID> m_sortedLiveRegionIDs;
    Vector<AXID> m_sortedNonRootWebAreaIDs;
#endif // PLATFORM(MAC)

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_currentModalElement;
    // Multiple aria-modals behavior is undefined by spec. We keep them sorted based on DOM order here.
    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> m_modalElements;
    bool m_modalNodesInitialized { false };
    bool m_isRetrievingCurrentModalNode { false };

#if PLATFORM(COCOA)
    bool m_liveRegionManagerInitialized { false };

    static std::atomic<bool> gShouldRepostNotificationsForTests;
#endif

    Timer m_performCacheUpdateTimer;

    AXTextStateChangeIntent m_textSelectionIntent;
    WeakHashSet<AccessibilityObject> m_deferredRendererChangedList;
    WeakHashSet<AccessibilityObject> m_deferredRecomputeActiveSummaryList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredRecomputeIsIgnoredList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredRecomputeTableIsExposedList;
    WeakHashSet<AccessibilityNodeObject> m_deferredRecomputeTableCellSlotsList;
    WeakHashSet<AccessibilityNodeObject> m_deferredRowspanChanges;
    WeakListHashSet<Node, WeakPtrImplWithEventTargetData> m_deferredTextChangedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredSelectedChildredChangedList;
    ListHashSet<Ref<AccessibilityObject>> m_deferredChildrenChangedList;
    WeakListHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredElementAddedOrRemovedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredModalChangedList;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_deferredMenuListChange;
    SingleThreadWeakHashSet<ScrollView> m_deferredScrollbarUpdateChangeList;
    WeakHashMap<Element, String, WeakPtrImplWithEventTargetData> m_deferredTextFormControlValue;
    Vector<AttributeChange> m_deferredAttributeChange;
    std::optional<std::pair<WeakPtr<Element, WeakPtrImplWithEventTargetData>, WeakPtr<Element, WeakPtrImplWithEventTargetData>>> m_deferredFocusedNodeChange;
    WeakHashSet<AccessibilityObject> m_deferredUnconnectedObjects;
#if PLATFORM(MAC)
    HashMap<PreSortedObjectType, Vector<Ref<AccessibilityObject>>, IntHash<PreSortedObjectType>, WTF::StrongEnumHashTraits<PreSortedObjectType>> m_deferredUnsortedObjects;
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    Timer m_buildIsolatedTreeTimer;
    bool m_deferredRegenerateIsolatedTree { false };
    const Ref<AXGeometryManager> m_geometryManager;
    DeferrableOneShotTimer m_selectedTextRangeTimer;
    Markable<AXID> m_lastDebouncedTextRangeObject;

    Timer m_updateTreeSnapshotTimer;
#endif
    bool m_isSynchronizingSelection { false };
    bool m_performingDeferredCacheUpdate { false };
    double m_loadingProgress { 0 };

    unsigned m_cacheUpdateDeferredCount { 0 };

#if PLATFORM(IOS_FAMILY)
    bool m_willPresentDatePopover;
#endif

    // Relationships between objects.
    HashMap<AXID, AXRelations> m_relations;
    bool m_relationsNeedUpdate { true };
    HashSet<AXID> m_relationTargets;
    HashMap<AXID, AXRelations> m_recentlyRemovedRelations;

#if USE(ATSPI)
    ListHashSet<RefPtr<AccessibilityObject>> m_deferredParentChangedList;
#endif

#if PLATFORM(MAC)
    Markable<AXID> m_lastTextFieldAXID;
    VisibleSelection m_lastSelection;
#endif

    WeakHashMap<RenderObject, Vector<Vector<AXID>>, SingleThreadWeakPtrImpl> m_stitchGroups;
};

inline bool AXObjectCache::accessibilityEnabled()
{
    return gAccessibilityEnabled;
}

inline void AXObjectCache::enableAccessibility()
{
    gAccessibilityEnabled = true;
}

inline void AXObjectCache::disableAccessibility()
{
    gAccessibilityEnabled = false;
}

inline bool AXObjectCache::forceDeferredSpellChecking()
{
    return gForceDeferredSpellChecking;
}

inline void AXObjectCache::setForceDeferredSpellChecking(bool shouldForce)
{
    gForceDeferredSpellChecking = shouldForce;
}

#if PLATFORM(COCOA)
WEBCORE_EXPORT RetainPtr<NSString> notifyPriorityToAXValueString(NotifyPriority);
WEBCORE_EXPORT RetainPtr<NSString> interruptBehaviorToAXValueString(InterruptBehavior);
#endif

} // namespace WebCore
