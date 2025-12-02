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

#include <wtf/Platform.h>
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include <WebCore/AXCoreObject.h>
#include <WebCore/AXLoggerBase.h>
#include <WebCore/AXTextMarker.h>
#include <WebCore/AXTextRun.h>
#include <WebCore/AXTreeStore.h>
#include <WebCore/ColorHash.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleSpeakAs.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class AXIsolatedObject;
class AXGeometryManager;
class AXObjectCache;
class AccessibilityObject;
enum class AXStreamOptions : uint16_t;

static constexpr uint16_t lastPropertyFlagIndex = 29;
// The most common boolean properties are stored in a bitfield rather than in a HashMap.
// If you edit these, make sure the corresponding AXProperty is ordered correctly in that
// enum, and update lastPropertyFlagIndex above.
enum class AXPropertyFlag : uint32_t {
    CanSetFocusAttribute                          = 1 << 0,
    CanSetSelectedAttribute                       = 1 << 1,
    CanSetValueAttribute                          = 1 << 2,
    HasBoldFont                                   = 1 << 3,
    HasClickHandler                               = 1 << 4,
    HasCursorPointer                              = 1 << 5,
    HasItalicFont                                 = 1 << 6,
    HasPlainText                                  = 1 << 7,
    HasPointerEventsNone                          = 1 << 8,
    IsBlockFlow                                   = 1 << 9,
    IsEnabled                                     = 1 << 10,
    IsExposedTableCell                            = 1 << 11,
    IsExposedTableRow                             = 1 << 12,
    IsGrabbed                                     = 1 << 13,
    IsHiddenUntilFoundContainer                   = 1 << 14,
    IsIgnored                                     = 1 << 15,
    IsInlineText                                  = 1 << 16,
    IsKeyboardFocusable                           = 1 << 17,
    IsNonLayerSVGObject                           = 1 << 18,
    // These IsTextEmissionBehavior flags are the variants of enum TextEmissionBehavior.
    IsTextEmissionBehaviorTab                     = 1 << 19,
    IsTextEmissionBehaviorNewline                 = 1 << 20,
    IsTextEmissionBehaviorDoubleNewline           = 1 << 21,
    IsVisited                                     = 1 << 22,
    ShowsCursorOnHover                            = 1 << 23,
    SupportsCheckedState                          = 1 << 24,
    SupportsDragging                              = 1 << 25,
    SupportsExpanded                              = 1 << 26,
    SupportsPath                                  = 1 << 27,
    SupportsPosInSet                              = 1 << 28,
    SupportsSetSize                               = 1 << lastPropertyFlagIndex
};

enum class AXProperty : uint16_t {
    CanSetFocusAttribute = 0,
    CanSetSelectedAttribute = 1,
    CanSetValueAttribute = 2,
    HasBoldFont = 3,
    HasClickHandler = 4,
    HasCursorPointer = 5,
    HasItalicFont = 6,
    HasPlainText = 7,
    HasPointerEventsNone = 8,
    IsBlockFlow = 9,
    IsEnabled = 10,
    IsExposedTableCell = 11,
    IsExposedTableRow = 12,
    IsGrabbed = 13,
    IsHiddenUntilFoundContainer = 14,
    IsIgnored = 15,
    IsInlineText = 16,
    IsKeyboardFocusable = 17,
    IsNonLayerSVGObject = 18,
    IsTextEmissionBehaviorTab = 19,
    IsTextEmissionBehaviorNewline = 20,
    IsTextEmissionBehaviorDoubleNewline = 21,
    IsVisited = 22,
    ShowsCursorOnHover = 23,
    SupportsCheckedState = 24,
    SupportsDragging = 25,
    SupportsExpanded = 26,
    SupportsPath = 27,
    SupportsPosInSet = 28,
    SupportsSetSize = lastPropertyFlagIndex,
    // End bool attributes that are matched in order by AXPropertyFlag.

    Abbreviation,
    ARIALevel,
    ARIARoleDescription,
#if !ENABLE(AX_THREAD_TEXT_APIS)
    // Rather than caching text content as property when ENABLE(AX_THREAD_TEXT_APIS), we should
    // synthesize it on-the-fly using AXProperty::TextRuns.
    AttributedText,
#endif // !ENABLE(AX_THREAD_TEXT_APIS)
    AXColumnCount,
    AXColumnIndex,
    AXColumnIndexText,
    AXRowCount,
    AXRowIndex,
    AXRowIndexText,
    AccessKey,
    AccessibilityText,
    ActionVerb,
    BackgroundColor,
    BrailleLabel,
    BrailleRoleDescription,
    ButtonState,
    CanBeMultilineTextField,
#if PLATFORM(MAC)
    CaretBrowsingEnabled,
#endif
    Cells,
    CellScope,
    CellSlots,
    ColorValue,
    Columns,
    ColumnIndex,
    ColumnIndexRange,
    CrossFrameChildFrameID,
    CrossFrameParentFrameID,
    CrossFrameParentAXID,
    CurrentState,
    DateTimeComponentsType,
    DateTimeValue,
    DatetimeAttributeValue,
    DecrementButton,
    Description,
    DisclosedByRow,
    DisclosedRows,
    DocumentEncoding,
    DocumentLinks,
    DocumentURI,
    ElementName,
    EmbeddedImageDescription,
    ExplicitAutoCompleteValue,
    ExplicitInvalidStatus,
    ExplicitLiveRegionRelevant,
    ExplicitLiveRegionStatus,
    ExplicitOrientation,
    ExplicitPopupValue,
    ExtendedDescription,
#if PLATFORM(COCOA)
    Font,
    FontOrientation,
#endif
    TextColor,
    HasApplePDFAnnotationAttribute,
    HasLinethrough,
    HasRemoteFrameChild,
    InputType,
    IsEditableWebArea,
    IsSubscript,
    IsSuperscript,
    HasTextShadow,
    HorizontalScrollBar,
    IdentifierAttribute,
    IncrementButton,
    InitialLocalRect,
    InnerHTML,
    InternalLinkElement,
    IsARIAGridRow,
    IsARIATreeGridRow,
    IsAnonymousMathOperator,
    IsAttachment,
    IsBusy,
    IsChecked,
    IsColumnHeader,
    IsExpanded,
    IsExposableTable,
    IsFieldset,
    IsIndeterminate,
    IsMathElement,
    IsMathFraction,
    IsMathFenced,
    IsMathSubscriptSuperscript,
    IsMathRow,
    IsMathUnderOver,
    IsMathRoot,
    IsMathSquareRoot,
    IsMathTable,
    IsMathTableRow,
    IsMathTableCell,
    IsMathMultiscript,
    IsMathToken,
    IsMultiSelectable,
    IsPlugin,
    IsPressed,
    IsRequired,
    IsRowHeader,
    IsSecureField,
    IsSelected,
    IsSelectedOptionActive,
    IsTable,
    IsTree,
    IsTreeItem,
    IsValueAutofillAvailable,
    IsVisible,
    IsWidget,
    KeyShortcuts,
    Language,
    LinethroughColor,
#if ENABLE(AX_THREAD_TEXT_APIS)
    ListMarkerLineID,
    ListMarkerText,
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    LiveRegionAtomic,
    LocalizedActionVerb,
    MathFencedOpenString,
    MathFencedCloseString,
    MathLineThickness,
    MathPrescripts,
    MathPostscripts,
    MathRadicand,
    MathRootIndexObject,
    MathUnderObject,
    MathOverObject,
    MathNumeratorObject,
    MathDenominatorObject,
    MathBaseObject,
    MathSubscriptObject,
    MathSuperscriptObject,
    MaxValueForRange,
    MinValueForRange,
    NameAttribute,
    OuterHTML,
    Path,
    PlaceholderValue,
#if PLATFORM(COCOA)
    PlatformWidget,
#endif
    PosInSet,
    PreventKeyboardDOMEventDispatch,
    RadioButtonGroupMembers,
    RelativeFrame,
    RemoteFrameOffset,
    RemoteFramePlatformElement,
#if PLATFORM(COCOA)
    RemoteParent,
#endif
    RevealableText,
    RolePlatformString,
    Rows,
    RowHeaders,
    RowIndex,
    RowIndexRange,
    ScreenRelativePosition,
    SelectedTextRange,
    SetSize,
    SortDirection,
    SpeakAs,
    StitchGroups,
    StringValue,
    SubrolePlatformString,
    SupportsDropping,
    SupportsARIAOwns,
    SupportsCurrent,
    SupportsKeyShortcuts,
    TextContentPrefixFromListMarker,
#if !ENABLE(AX_THREAD_TEXT_APIS)
    // Rather than caching text content as property when ENABLE(AX_THREAD_TEXT_APIS), we should
    // synthesize it on-the-fly using AXProperty::TextRuns.
    TextContent,
#endif // !ENABLE(AX_THREAD_TEXT_APIS)
    TextInputMarkedTextMarkerRange,
#if ENABLE(AX_THREAD_TEXT_APIS)
    TextRuns,
#endif
    TitleAttribute,
    URL,
    UnderlineColor,
    ValueAutofillButtonType,
    ValueDescription,
    ValueForRange,
    VerticalScrollBar,
    VisibleChildren,
    VisibleRows,
    WebAreaTitle
};
WTF::TextStream& operator<<(WTF::TextStream&, AXProperty);

using AXPropertySet = HashSet<AXProperty, IntHash<AXProperty>, WTF::StrongEnumHashTraits<AXProperty>>;

struct AXIDAndCharacterRange {
    WTF_MAKE_TZONE_ALLOCATED(AXIDAndCharacterRange);
public:
    Markable<AXID> first;
    CharacterRange second;

    AXIDAndCharacterRange() = default;
    AXIDAndCharacterRange(Markable<AXID> axID, CharacterRange range)
        : first(axID), second(range) { }
};

// If this type is modified, the switchOn statment in AXIsolatedObject::setProperty must be updated as well.
using AXPropertyValueVariant = Variant<std::nullptr_t, Markable<AXID>, String, bool, int, unsigned, double, float, uint64_t, WallTime, DateComponentsType, AccessibilityButtonState, Color, std::unique_ptr<URL>, LayoutRect, FloatPoint, FloatRect, InputType::Type, IntPoint, IntRect, std::pair<unsigned, unsigned>, Vector<AccessibilityText>, Vector<AXID>, Vector<std::pair<Markable<AXID>, Markable<AXID>>>, Vector<String>, std::unique_ptr<Path>, Vector<Vector<AXID>>, OptionSet<AXAncestorFlag>, Vector<Vector<Markable<AXID>>>, CharacterRange, std::unique_ptr<AXIDAndCharacterRange>, ElementName, AccessibilityOrientation
#if PLATFORM(COCOA)
    , RetainPtr<NSAttributedString>
    , RetainPtr<NSView>
    , RetainPtr<id>
    , Style::SpeakAs
#endif // PLATFORM(COCOA)
#if ENABLE(AX_THREAD_TEXT_APIS)
    , RetainPtr<CTFontRef>
    , FontOrientation
    , std::unique_ptr<AXTextRuns>
    , AXTextRunLineID
    , FrameIdentifier
#endif // ENABLE(AX_THREAD_TEXT_APIS)
>;
using AXPropertyVector = Vector<std::pair<AXProperty, AXPropertyValueVariant>>;
WTF::TextStream& operator<<(WTF::TextStream&, const AXPropertyVector&);

static_assert(sizeof(AXPropertyValueVariant) == 24, "The AX property value variant should not be larger than 24.");

struct AXPropertyChange {
    AXID axID; // ID of the object whose properties changed.
    AXPropertyVector properties; // Changed properties.
};

struct NodeUpdateOptions {
    AXPropertySet properties;
    bool shouldUpdateNode { false };
    bool shouldUpdateChildren { false };

    NodeUpdateOptions(const AXPropertySet& propertyNames, bool shouldUpdateNode, bool shouldUpdateChildren)
        : properties(propertyNames)
        , shouldUpdateNode(shouldUpdateNode)
        , shouldUpdateChildren(shouldUpdateChildren)
    { }

    NodeUpdateOptions(const AXPropertySet& propertyNames)
        : properties(propertyNames)
    { }

    NodeUpdateOptions(AXProperty propertyName)
        : properties({ propertyName })
    { }

    static NodeUpdateOptions nodeUpdate()
    {
        return { { }, true, false };
    }

    static NodeUpdateOptions childrenUpdate()
    {
        return { { }, false, true };
    }
};

void setPropertyIn(AXProperty, AXPropertyValueVariant&&, AXPropertyVector&, OptionSet<AXPropertyFlag>&);

struct IsolatedObjectData {
    Vector<AXID> childrenIDs;
    AXPropertyVector properties;
    Ref<AXIsolatedTree> tree;
    Markable<AXID> parentID;
    AXID axID;
    AccessibilityRole role;
    OptionSet<AXPropertyFlag> propertyFlags;
    bool getsGeometryFromChildren;

    IsolatedObjectData(Vector<AXID> childrenIDs, AXPropertyVector properties, Ref<AXIsolatedTree> tree, Markable<AXID> parentID, AXID axID, AccessibilityRole role, OptionSet<AXPropertyFlag> propertyFlags, bool getsGeometryFromChildren)
        : childrenIDs(WTFMove(childrenIDs))
        , properties(WTFMove(properties))
        , tree(WTFMove(tree))
        , parentID(parentID)
        , axID(axID)
        , role(role)
        , propertyFlags(propertyFlags)
        , getsGeometryFromChildren(getsGeometryFromChildren)
    { }

    IsolatedObjectData(const IsolatedObjectData&) = delete;
    IsolatedObjectData(IsolatedObjectData&&) = default;

    void setProperty(AXProperty property, AXPropertyValueVariant&& value)
    {
        properties.removeFirstMatching([&property] (const auto& propertyAndValue) {
            return propertyAndValue.first == property;
        });
        setPropertyIn(property, WTFMove(value), properties, propertyFlags);
    }
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXIsolatedTree);
class AXIsolatedTree : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AXIsolatedTree>
    , public AXTreeStore<AXIsolatedTree> {
    WTF_MAKE_NONCOPYABLE(AXIsolatedTree);
    WTF_MAKE_TZONE_ALLOCATED(AXIsolatedTree);
    friend WTF::TextStream& operator<<(WTF::TextStream&, AXIsolatedTree&);
    friend void streamIsolatedSubtreeOnMainThread(TextStream&, const AXIsolatedTree&, AXID, const OptionSet<AXStreamOptions>&);
public:
    static RefPtr<AXIsolatedTree> create(AXObjectCache&);
    // Creates a tree consisting of only the Scrollview and the WebArea objects. This tree is used as a temporary placeholder while the whole tree is being built.
    static Ref<AXIsolatedTree> createEmpty(AXObjectCache&);
    constexpr bool isEmptyContentTree() const { return m_isEmptyContentTree; }
    virtual ~AXIsolatedTree();

    static void removeTreeForFrameID(FrameIdentifier);

    // Retrieve the tree for the frame ID of any LocalFrame
    static RefPtr<AXIsolatedTree> treeForFrameID(std::optional<FrameIdentifier>);
    static RefPtr<AXIsolatedTree> treeForFrameID(FrameIdentifier);
    static RefPtr<AXIsolatedTree> treeForFrameIDAlreadyLocked(FrameIdentifier);
    AXObjectCache* axObjectCache() const;
    constexpr AXGeometryManager* geometryManager() const { return m_geometryManager.get(); }

    AXIsolatedObject* rootNode() { ASSERT(!isMainThread()); return m_rootNode.get(); }
    RefPtr<AXIsolatedObject> rootWebArea();
    std::optional<AXID> focusedNodeID();
    WEBCORE_EXPORT RefPtr<AXIsolatedObject> focusedNode();

    inline AXIsolatedObject* objectForID(AXID axID) const
    {
        AX_DEBUG_ASSERT(!isMainThread());

        auto iterator = m_readerThreadNodeMap.find(axID);
        if (iterator != m_readerThreadNodeMap.end())
            return iterator->value.ptr();
        return nullptr;
    }
    inline AXIsolatedObject* objectForID(std::optional<AXID> axID) const
    {
        return axID ? objectForID(*axID) : nullptr;
    }
    template<typename U> Vector<Ref<AXCoreObject>> objectsForIDs(const U&);

    void generateSubtree(AccessibilityObject&);
    bool shouldCreateNodeChange(AccessibilityObject&);
    enum class ResolveNodeChanges : bool { No, Yes };
    void updateChildrenForObjects(const ListHashSet<Ref<AccessibilityObject>>&);
    void updateDependentProperties(AccessibilityObject&);
    void updatePropertiesForSelfAndDescendants(AccessibilityObject&, const AXPropertySet&);
    void updateFrame(AXID, IntRect&&);
    void updateRootScreenRelativePosition();
    void overrideNodeProperties(AXID, AXPropertyVector&&);

    double loadingProgress();
    void updateLoadingProgress(double);

    void addUnconnectedNode(Ref<AccessibilityObject>);
    bool isUnconnectedNode(std::optional<AXID> axID) const { return axID && m_unconnectedNodes.contains(*axID); }
    // Removes the corresponding isolated object and all descendants from the m_nodeMap and queues their removal from the tree.
    void removeNode(AXID, std::optional<AXID> /* parentID */);
    // Removes the given node and all its descendants from m_nodeMap.
    void removeSubtreeFromNodeMap(std::optional<AXID>, std::optional<AXID> /* parentID */);

    void objectBecameIgnored(const AccessibilityObject& object)
    {
#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
        // When an object becomes ignored, we should immediately remove it from the nodemap.
        // This is critical because objects can become ignored at any point, including in the
        // middle of AXObjectCache::handleChildrenChanged(). Consider this tree structure:
        //   <main> (not ignored)
        //   ++<div> (not ignored)
        // Imagine <div> gains new children, and we run handleChildrenChanged() for it. However,
        // it becomes ignored in the middle of handleChildrenChanged(). We will still call
        // AXIsolatedTree::updateChildren for this <div>, and because it isn't yet removed from
        // the nodemap, we will run the children update on the <div> rather than the <main>.
        // Eagerly removing <div> from the nodemap when it becomes ignored prevents this by
        // allowing us to ascend up the nodemap to the <main>, which can properly scoop up <div>s children.

        // Note that this problem is only relevant in a world where !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE),
        // because when that flag is on, is-ignored doesn't matter when building the core-tree.

        // Normally, when removing things from the nodemap, we want to use removeSubtreeFromNodeMap because
        // it removes both the given object, and all its descendants, as the descendants should not be in the
        // tree without some parent. However, when something becomes ignored, those descendants still exist,
        // just with a different parent (the next unignored ancestor). So we can safely only remove the given
        // object from the nodemap, and rely on the normal updateChildren flow to repair parent relationships
        // as needed.
        m_nodeMap.remove(object.objectID());
        // Any queued parent updates no longer need to happen (and if we do try to process them, we'll crash,
        // since this object is no longer in the node map).
        m_needsParentUpdate.remove(object.objectID());
#endif // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
        objectChangedIgnoredState(object);
        queueNodeUpdate(object.objectID(), { { AXProperty::IsIgnored, AXProperty::RevealableText } });
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    }
    void objectBecameUnignored(const AccessibilityObject& object)
    {
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
        // We only cache minimal properties for ignored objects, so do a full node update to ensure all properties are cached.
        queueNodeUpdate(object.objectID(), NodeUpdateOptions::nodeUpdate());
        objectChangedIgnoredState(object);
#else
        UNUSED_PARAM(object);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    }

    // Both setPendingRootNodeLocked and setFocusedNodeID are called during the generation
    // of the IsolatedTree.
    // Focused node updates in AXObjectCache use setFocusNodeID.
    void setPendingRootNodeID(AXID);
    void setPendingRootNodeIDLocked(AXID) WTF_REQUIRES_LOCK(m_changeLogLock);
    void setFocusedNodeID(std::optional<AXID>);
    void applyPendingRootNodeLocked() WTF_REQUIRES_LOCK(m_changeLogLock);

    // Relationships between objects.
    std::optional<ListHashSet<AXID>> relatedObjectIDsFor(const AXIsolatedObject&, AXRelation);
    void markRelationsDirty() { m_relationsNeedUpdate = true; }
    void updateRelations(HashMap<AXID, AXRelations>&&);

    AXCoreObject::AccessibilityChildrenVector sortedLiveRegions();
    AXCoreObject::AccessibilityChildrenVector sortedNonRootWebAreas();

    void markMostRecentlyPaintedTextDirty() { m_mostRecentlyPaintedTextIsDirty = true; }
    const HashMap<AXID, LineRange>& mostRecentlyPaintedText() const { return m_mostRecentlyPaintedText; }

    // Called on AX thread from WebAccessibilityObjectWrapper methods.
    WEBCORE_EXPORT void applyPendingChanges();
    void applyPendingChangesUnlessQueuedForDestruction();

    constexpr AXID treeID() const { return m_id; }
    constexpr ProcessID processID() const { return m_processID; }
    void setPageActivityState(OptionSet<ActivityState>);
    OptionSet<ActivityState> pageActivityState() const;
    // Use only if the s_storeLock is already held like in findAXTree.
    WEBCORE_EXPORT OptionSet<ActivityState> lockedPageActivityState() const;

    AXTextMarkerRange selectedTextMarkerRange() { return m_selectedTextMarkerRange; }
    void setSelectedTextMarkerRange(AXTextMarkerRange&&);

    void sortedLiveRegionsDidChange(Vector<AXID>);
    void sortedNonRootWebAreasDidChange(Vector<AXID>);

    void setInitialSortedLiveRegions(Vector<AXID>);
    void setInitialSortedNonRootWebAreas(Vector<AXID>);

    void queueNodeUpdate(AXID, const NodeUpdateOptions&);
    void queueNodeRemoval(const AccessibilityObject&);
    void processQueuedNodeUpdates();

#if ENABLE(AX_THREAD_TEXT_APIS)
    AXTextMarker firstMarker();
    AXTextMarker lastMarker();
#endif

private:
    AXIsolatedTree(AXObjectCache&);
    static void storeTree(AXObjectCache&, const Ref<AXIsolatedTree>&);
    void reportLoadingProgress(double);

    // Queue this isolated tree up to destroy itself on the secondary thread.
    // We can't destroy the tree on the main-thread (by removing all `Ref`s to it)
    // because it could be being used by the secondary thread to service an AX request.
    void queueForDestruction();

    void applyPendingChangesLocked() WTF_REQUIRES_LOCK(m_changeLogLock);

    // rdar://161259641 (Figure out a way to enforce WTF_REQUIRES_LOCK when we might need to access it while already holding the lock)
    static HashMap<FrameIdentifier, Ref<AXIsolatedTree>>& treeFrameCache(); // WTF_REQUIRES_LOCK(s_storeLock);

    void createEmptyContent(AccessibilityObject&);
    constexpr bool isUpdatingSubtree() const { return m_rootOfSubtreeBeingUpdated; }
    constexpr void updatingSubtree(AccessibilityObject* axObject) { m_rootOfSubtreeBeingUpdated = axObject; }

    struct NodeChange {
        IsolatedObjectData data;
#if PLATFORM(COCOA)
        RetainPtr<AccessibilityObjectWrapper> wrapper;
#elif USE(ATSPI)
        RefPtr<AccessibilityObjectWrapper> wrapper;
#endif
        explicit NodeChange(IsolatedObjectData&& isolatedData, RetainPtr<AccessibilityObjectWrapper> wrapper)
            : data(WTFMove(isolatedData))
            , wrapper(WTFMove(wrapper))
        { }

        NodeChange(const NodeChange&) = delete;
        NodeChange(NodeChange&&) = default;
    };

    void updateChildren(AccessibilityObject&, ResolveNodeChanges = ResolveNodeChanges::Yes);
    void updateNode(AccessibilityObject&);
    void updateNodeProperties(AccessibilityObject&, const AXPropertySet&);

    std::optional<NodeChange> nodeChangeForObject(Ref<AccessibilityObject>);
    void collectNodeChangesForSubtree(AccessibilityObject&);
    bool isCollectingNodeChanges() const { return m_isCollectingNodeChanges; }
    void queueChange(NodeChange&&) WTF_REQUIRES_LOCK(m_changeLogLock);
    void queueRemovals(Vector<AXID>&&);
    void queueRemovalsLocked(Vector<AXID>&&) WTF_REQUIRES_LOCK(m_changeLogLock);
    void queueRemovalsAndUnresolvedChanges();
    Vector<NodeChange> resolveAppends();
    void queueAppendsAndRemovals(Vector<NodeChange>&&, Vector<AXID>&&);

    void objectChangedIgnoredState(const AccessibilityObject&);

    const WeakPtr<AXObjectCache> m_axObjectCache;
    RefPtr<AXGeometryManager> m_geometryManager;
    // Reference to a temporary, empty content tree that this tree will replace. Used for updating the empty content tree while this is built.
    RefPtr<AXIsolatedTree> m_replacingTree;
    RefPtr<AccessibilityObject> m_rootOfSubtreeBeingUpdated;

    // Stores the parent ID and children IDs for a given IsolatedObject.
    struct ParentChildrenIDs {
        Markable<AXID> parentID;
        Vector<AXID> childrenIDs;
    };
    // Only accessed on the main thread.
    // A representation of the tree's parent-child relationships. Each
    // IsolatedObject must have one and only one entry in this map, that maps
    // its ObjectID to its ParentChildrenIDs struct.
    HashMap<AXID, ParentChildrenIDs> m_nodeMap;

    // Only accessed on the main thread.
    // Stores all nodes that are added via addUnconnectedNode, which do not get stored in m_nodeMap.
    HashSet<AXID> m_unconnectedNodes;

    // Only accessed on the main thread.
    // The key is the ID of the object that will be resolved into an m_pendingAppends NodeChange.
    HashSet<AXID> m_unresolvedPendingAppends;
    // Only accessed on the main thread.
    // While performing tree updates, we append nodes to this list that are no longer connected
    // in the tree and should be removed. This list turns into m_pendingSubtreeRemovals when
    // handed off to the secondary thread.
    Vector<AXID> m_subtreesToRemove;
    // Only accessed on the main thread.
    // This is used when updating the isolated tree in response to dynamic children changes.
    // It is required to protect objects from being incorrectly deleted when they are re-parented,
    // as the original parent will want to queue it for removal, but we need to keep the object around
    // for the new parent.
    HashSet<AXID> m_protectedFromDeletionIDs;
    // Only accessed on the main thread.
    // Objects whose parent has changed, and said change needs to be synced to the secondary thread.
    HashSet<AXID> m_needsParentUpdate;

    // Only accessed on AX thread.
    HashMap<AXID, Ref<AXIsolatedObject>> m_readerThreadNodeMap;
    RefPtr<AXIsolatedObject> m_rootNode;

    // Written to by main thread under lock, accessed and applied by AX thread.
    Markable<AXID> m_pendingRootNodeID WTF_GUARDED_BY_LOCK(m_changeLogLock);
    Vector<NodeChange> m_pendingAppends WTF_GUARDED_BY_LOCK(m_changeLogLock); // Nodes to be added to the tree and platform-wrapped.
    Vector<AXPropertyChange> m_pendingPropertyChanges WTF_GUARDED_BY_LOCK(m_changeLogLock);
    HashSet<AXID> m_pendingSubtreeRemovals WTF_GUARDED_BY_LOCK(m_changeLogLock); // Nodes whose subtrees are to be removed from the tree.
    Vector<std::pair<AXID, Vector<AXID>>> m_pendingChildrenUpdates WTF_GUARDED_BY_LOCK(m_changeLogLock);
    HashSet<AXID> m_pendingProtectedFromDeletionIDs WTF_GUARDED_BY_LOCK(m_changeLogLock);
    HashMap<AXID, AXID> m_pendingParentUpdates WTF_GUARDED_BY_LOCK(m_changeLogLock);
    Markable<AXID> m_pendingFocusedNodeID WTF_GUARDED_BY_LOCK(m_changeLogLock);
    std::optional<Vector<AXID>> m_pendingSortedLiveRegionIDs WTF_GUARDED_BY_LOCK(m_changeLogLock);

    // These three are placed here to fit in padding that would otherwise be between m_pendingSortedLiveRegionIDs and m_pendingSortedNonRootWebAreaIDs.
    OptionSet<ActivityState> m_pageActivityState;
    bool m_isEmptyContentTree { false };
    bool m_queuedForDestruction WTF_GUARDED_BY_LOCK(m_changeLogLock) { false };

    std::optional<Vector<AXID>> m_pendingSortedNonRootWebAreaIDs WTF_GUARDED_BY_LOCK(m_changeLogLock);
    std::optional<HashMap<AXID, LineRange>> m_pendingMostRecentlyPaintedText WTF_GUARDED_BY_LOCK(m_changeLogLock);
    std::optional<HashMap<AXID, AXRelations>> m_pendingRelations WTF_GUARDED_BY_LOCK(m_changeLogLock);
    std::optional<AXTextMarkerRange> m_pendingSelectedTextMarkerRange WTF_GUARDED_BY_LOCK(m_changeLogLock);
    Markable<AXID> m_focusedNodeID;
    std::atomic<double> m_loadingProgress { 0 };
    std::atomic<double> m_processingProgress { 1 };

    // Only accessed on the accessibility thread.
    Vector<AXID> m_sortedLiveRegionIDs;
    Vector<AXID> m_sortedNonRootWebAreaIDs;
    HashMap<AXID, LineRange> m_mostRecentlyPaintedText;
    HashMap<AXID, AXRelations> m_relations;

    // Set to true by the AXObjectCache and false by AXIsolatedTree.
    // Both are only to be used on the main-thread.
    bool m_relationsNeedUpdate { true };
    bool m_mostRecentlyPaintedTextIsDirty { true };

    Lock m_changeLogLock;

    // Only accessed on the main thread.
    bool m_isCollectingNodeChanges;

    AXTextMarkerRange m_selectedTextMarkerRange;
    const ProcessID m_processID { legacyPresentingApplicationPID() };

    // Queued node updates used for building a new tree snapshot.
    ListHashSet<AXID> m_needsUpdateChildren;
    ListHashSet<AXID> m_needsUpdateNode;
    HashMap<AXID, AXPropertySet> m_needsPropertyUpdates;
    // The key is the ID of the node being removed. The value is the ID of the parent in the core tree (if it exists).
    HashMap<AXID, std::optional<AXID>> m_needsNodeRemoval;
};

IsolatedObjectData createIsolatedObjectData(const Ref<AccessibilityObject>&, Ref<AXIsolatedTree>);
std::optional<AXPropertyFlag> convertToPropertyFlag(AXProperty);

inline AXObjectCache* AXIsolatedTree::axObjectCache() const
{
    AX_DEBUG_ASSERT(isMainThread());
    return m_axObjectCache.get();
}

inline RefPtr<AXIsolatedTree> AXIsolatedTree::treeForFrameID(std::optional<FrameIdentifier> frameID)
{
    return frameID ? treeForFrameID(*frameID) : nullptr;
}

template<typename U>
inline Vector<Ref<AXCoreObject>> AXIsolatedTree::objectsForIDs(const U& axIDs)
{
    ASSERT(!isMainThread());

    Vector<Ref<AXCoreObject>> result;
    result.reserveInitialCapacity(axIDs.size());
    for (const auto& axID : axIDs) {
        if (RefPtr object = objectForID(axID))
            result.append(object.releaseNonNull());
    }
    result.shrinkToFit();
    return result;
}

} // namespace WebCore

#endif
