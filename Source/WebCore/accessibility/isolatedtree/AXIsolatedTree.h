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

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include "AXCoreObject.h"
#include "AXTextMarker.h"
#include "AXTextRun.h"
#include "AXTreeStore.h"
#include "PageIdentifier.h"
#include "RenderStyleConstants.h"
#include "RuntimeApplicationChecks.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
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
class Page;
enum class AXStreamOptions : uint8_t;

enum class AXPropertyName : uint16_t {
    ARIATreeRows,
    AttributedText,
    AXColumnCount,
    AXColumnIndex,
    AXRowCount,
    AXRowIndex,
    AccessKey,
    AccessibilityText,
    ActionVerb,
    AncestorFlags,
    AutoCompleteValue,
    BlockquoteLevel,
    BrailleLabel,
    BrailleRoleDescription,
    ButtonState,
    CanBeMultilineTextField,
    CanSetFocusAttribute,
    CanSetSelectedAttribute,
    CanSetSelectedChildren,
    CanSetValueAttribute,
#if PLATFORM(MAC)
    CaretBrowsingEnabled,
#endif
    Cells,
    CellScope,
    CellSlots,
    ColorValue,
    Columns,
    ColumnHeader,
    ColumnHeaders,
    ColumnIndex,
    ColumnIndexRange,
    CurrentState,
    DatetimeAttributeValue,
    DecrementButton,
    Description,
    DisclosedByRow,
    DisclosedRows,
    DocumentEncoding,
    DocumentLinks,
    DocumentURI,
    EmbeddedImageDescription,
    ExpandedTextValue,
    ExtendedDescription,
    HasApplePDFAnnotationAttribute,
    HasBoldFont,
    HasHighlighting,
    HasItalicFont,
    HasPlainText,
    HasUnderline,
    HeaderContainer,
    HeadingLevel,
    HierarchicalLevel,
    HorizontalScrollBar,
    IdentifierAttribute,
    IncrementButton,
    InnerHTML,
    InternalLinkElement,
    InsideLink,
    InvalidStatus,
    IsGrabbed,
    IsARIATreeGridRow,
    IsAttachment,
    IsBusy,
    IsChecked,
    IsColumnHeader,
    IsControl,
    IsEnabled,
    IsExpanded,
    IsExposable,
    IsExposedTableCell,
    IsFieldset,
    IsFileUploadButton,
    IsIndeterminate,
    IsInlineText,
    IsRadioInput,
    IsInputImage,
    IsKeyboardFocusable,
    IsLink,
    IsList,
    IsListBox,
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
    IsMeter,
    IsMultiSelectable,
    IsNonNativeTextControl,
    IsPlugin,
    IsPressed,
    IsRequired,
    IsRowHeader,
    IsSecureField,
    IsSelected,
    IsSelectedOptionActive,
    IsTable,
    IsTableColumn,
    IsTableRow,
    IsTree,
    IsTreeItem,
    IsValueAutofillAvailable,
    IsVisible,
    IsWidget,
    KeyShortcuts,
    Language,
    LiveRegionAtomic,
    LiveRegionRelevant,
    LiveRegionStatus,
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
    Orientation,
    OuterHTML,
    Path,
    PlaceholderValue,
    PopupValue,
    PosInSet,
    PreventKeyboardDOMEventDispatch,
    RadioButtonGroup,
    RelativeFrame,
    RoleValue,
    RolePlatformString,
    RoleDescription,
    Rows,
    RowGroupAncestorID,
    RowHeader,
    RowHeaders,
    RowIndex,
    RowIndexRange,
    ScreenRelativePosition,
    SelectedChildren,
    SelectedTextRange,
    SetSize,
    SortDirection,
    SpeechHint,
    StringValue,
    SubrolePlatformString,
    SupportsDragging,
    SupportsDropping,
    SupportsARIAOwns,
    SupportsCheckedState,
    SupportsCurrent,
    SupportsDatetimeAttribute,
    SupportsExpanded,
    SupportsExpandedTextValue,
    SupportsKeyShortcuts,
    SupportsPath,
    SupportsPosInSet,
    SupportsPressAction,
    SupportsRangeValue,
    SupportsRequiredAttribute,
    SupportsSelectedRows,
    SupportsSetSize,
    TextContent,
    TextInputMarkedTextMarkerRange,
#if ENABLE(AX_THREAD_TEXT_APIS)
    TextRuns,
#endif
    Title,
    TitleAttributeValue,
    URL,
    ValueAutofillButtonType,
    ValueDescription,
    ValueForRange,
    VerticalScrollBar,
    VisibleChildren,
    VisibleRows,
};

using AXPropertyNameSet = HashSet<AXPropertyName, IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>>;

// If this type is modified, the switchOn statment in AXIsolatedObject::setProperty must be updated as well.
using AXPropertyValueVariant = std::variant<std::nullptr_t, AXID, String, bool, int, unsigned, double, float, uint64_t, AccessibilityButtonState, Color, URL, LayoutRect, FloatPoint, FloatRect, IntPoint, IntRect, std::pair<unsigned, unsigned>, Vector<AccessibilityText>, Vector<AXID>, Vector<std::pair<AXID, AXID>>, Vector<String>, Path, OptionSet<AXAncestorFlag>, InsideLink, Vector<Vector<AXID>>, CharacterRange, std::pair<AXID, CharacterRange>
#if PLATFORM(COCOA)
    , RetainPtr<NSAttributedString>
#endif
#if ENABLE(AX_THREAD_TEXT_APIS)
    , AXTextRuns
#endif
>;
using AXPropertyMap = HashMap<AXPropertyName, AXPropertyValueVariant, IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>>;

struct AXPropertyChange {
    AXID axID; // ID of the object whose properties changed.
    AXPropertyMap properties; // Changed properties.
};

struct NodeUpdateOptions {
    AXPropertyNameSet properties;
    bool shouldUpdateNode { false };
    bool shouldUpdateChildren { false };

    NodeUpdateOptions(const AXPropertyNameSet& propertyNames, bool shouldUpdateNode, bool shouldUpdateChildren)
        : properties(propertyNames)
        , shouldUpdateNode(shouldUpdateNode)
        , shouldUpdateChildren(shouldUpdateChildren)
    { }

    NodeUpdateOptions(const AXPropertyNameSet& propertyNames)
        : properties(propertyNames)
    { }

    NodeUpdateOptions(AXPropertyName propertyName)
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

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXIsolatedTree);
class AXIsolatedTree : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AXIsolatedTree>
    , public AXTreeStore<AXIsolatedTree> {
    WTF_MAKE_NONCOPYABLE(AXIsolatedTree);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXIsolatedTree);
    friend WTF::TextStream& operator<<(WTF::TextStream&, AXIsolatedTree&);
    friend void streamIsolatedSubtreeOnMainThread(TextStream&, const AXIsolatedTree&, AXID, const OptionSet<AXStreamOptions>&);
public:
    static Ref<AXIsolatedTree> create(AXObjectCache&);
    // Creates a tree consisting of only the Scrollview and the WebArea objects. This tree is used as a temporary placeholder while the whole tree is being built.
    static Ref<AXIsolatedTree> createEmpty(AXObjectCache&);
    constexpr bool isEmptyContentTree() const { return m_isEmptyContentTree; }
    virtual ~AXIsolatedTree();

    static void removeTreeForPageID(PageIdentifier);

    static RefPtr<AXIsolatedTree> treeForPageID(std::optional<PageIdentifier>);
    static RefPtr<AXIsolatedTree> treeForPageID(PageIdentifier);
    constexpr ProcessID processID() const { return m_processID; }
    AXObjectCache* axObjectCache() const;
    constexpr AXGeometryManager* geometryManager() const { return m_geometryManager.get(); }

    RefPtr<AXIsolatedObject> rootNode();
    AXID focusedNodeID();
    WEBCORE_EXPORT RefPtr<AXIsolatedObject> focusedNode();

    RefPtr<AXIsolatedObject> objectForID(const AXID) const;
    template<typename U> Vector<RefPtr<AXCoreObject>> objectsForIDs(const U&);

    void generateSubtree(AccessibilityObject&);
    void updateNode(AccessibilityObject&);
    enum class ResolveNodeChanges : bool { No, Yes };
    void updateChildren(AccessibilityObject&, ResolveNodeChanges = ResolveNodeChanges::Yes);
    void updateChildrenForObjects(const ListHashSet<RefPtr<AccessibilityObject>>&);
    void updateNodeProperty(AXCoreObject& object, AXPropertyName property) { updateNodeProperties(object, { property }); };
    void updateNodeProperties(AXCoreObject&, const AXPropertyNameSet&);
    void updateDependentProperties(AccessibilityObject&);
    void updatePropertiesForSelfAndDescendants(AccessibilityObject&, const AXPropertyNameSet&);
    void updateFrame(AXID, IntRect&&);
    void updateRootScreenRelativePosition();
    void overrideNodeProperties(AXID, AXPropertyMap&&);

    double loadingProgress() { return m_loadingProgress; }
    void updateLoadingProgress(double);

    void addUnconnectedNode(Ref<AccessibilityObject>);
    // Removes the corresponding isolated object and all descendants from the m_nodeMap and queues their removal from the tree.
    void removeNode(const AccessibilityObject&);
    // Removes the given node and all its descendants from m_nodeMap.
    void removeSubtreeFromNodeMap(AXID axID, AccessibilityObject*);

    // Both setRootNodeID and setFocusedNodeID are called during the generation
    // of the IsolatedTree.
    // Focused node updates in AXObjectCache use setFocusNodeID.
    void setRootNode(AXIsolatedObject*) WTF_REQUIRES_LOCK(m_changeLogLock);
    void setFocusedNodeID(AXID);

    // Relationships between objects.
    std::optional<ListHashSet<AXID>> relatedObjectIDsFor(const AXIsolatedObject&, AXRelationType);
    void relationsNeedUpdate(bool needUpdate) { m_relationsNeedUpdate = needUpdate; }
    void updateRelations(const HashMap<AXID, AXRelations>&);

    // Called on AX thread from WebAccessibilityObjectWrapper methods.
    // During layout tests, it is called on the main thread.
    void applyPendingChanges();

    AXID treeID() const { return m_id; }
    void setPageActivityState(OptionSet<ActivityState>);
    OptionSet<ActivityState> pageActivityState() const;
    // Use only if the s_storeLock is already held like in findAXTree.
    WEBCORE_EXPORT OptionSet<ActivityState> lockedPageActivityState() const;

    AXTextMarkerRange selectedTextMarkerRange();
    void setSelectedTextMarkerRange(AXTextMarkerRange&&);

    void queueNodeUpdate(AXID, const NodeUpdateOptions&);
    void processQueuedNodeUpdates();

#if ENABLE(AX_THREAD_TEXT_APIS)
    AXTextMarker firstMarker();
    AXTextMarker lastMarker();
#endif

private:
    AXIsolatedTree(AXObjectCache&);
    static void storeTree(AXObjectCache&, const Ref<AXIsolatedTree>&);
    void reportCreationProgress(AXObjectCache&, unsigned percentComplete);

    // Queue this isolated tree up to destroy itself on the secondary thread.
    // We can't destroy the tree on the main-thread (by removing all `Ref`s to it)
    // because it could be being used by the secondary thread to service an AX request.
    void queueForDestruction();

    static HashMap<PageIdentifier, Ref<AXIsolatedTree>>& treePageCache() WTF_REQUIRES_LOCK(s_storeLock);

    void createEmptyContent(AccessibilityObject&);
    constexpr bool isUpdatingSubtree() const { return m_rootOfSubtreeBeingUpdated; }
    constexpr void updatingSubtree(AccessibilityObject* axObject) { m_rootOfSubtreeBeingUpdated = axObject; }

    enum class AttachWrapper : bool { OnMainThread, OnAXThread };
    struct NodeChange {
        Ref<AXIsolatedObject> isolatedObject;
#if PLATFORM(COCOA)
        RetainPtr<AccessibilityObjectWrapper> wrapper;
#elif USE(ATSPI)
        RefPtr<AccessibilityObjectWrapper> wrapper;
#endif
        AttachWrapper attachWrapper { AttachWrapper::OnMainThread };
    };

    std::optional<NodeChange> nodeChangeForObject(Ref<AccessibilityObject>, AttachWrapper = AttachWrapper::OnMainThread);
    void collectNodeChangesForSubtree(AXCoreObject&);
    bool isCollectingNodeChanges() const { return m_collectingNodeChangesAtTreeLevel > 0; }
    void queueChange(const NodeChange&) WTF_REQUIRES_LOCK(m_changeLogLock);
    void queueRemovals(Vector<AXID>&&);
    void queueRemovalsLocked(Vector<AXID>&&) WTF_REQUIRES_LOCK(m_changeLogLock);
    void queueRemovalsAndUnresolvedChanges(Vector<AXID>&&);
    Vector<NodeChange> resolveAppends();
    void queueAppendsAndRemovals(Vector<NodeChange>&&, Vector<AXID>&&);

    const ProcessID m_processID { presentingApplicationPID() };
    unsigned m_maxTreeDepth { 0 };
    WeakPtr<AXObjectCache> m_axObjectCache;
    OptionSet<ActivityState> m_pageActivityState;
    RefPtr<AXGeometryManager> m_geometryManager;
    bool m_isEmptyContentTree { false };
    // Reference to a temporary, empty content tree that this tree will replace. Used for updating the empty content tree while this is built.
    RefPtr<AXIsolatedTree> m_replacingTree;
    RefPtr<AccessibilityObject> m_rootOfSubtreeBeingUpdated;

    // Stores the parent ID and children IDS for a given IsolatedObject.
    struct ParentChildrenIDs {
        AXID parentID;
        Vector<AXID> childrenIDs;
    };
    // Only accessed on the main thread.
    // A representation of the tree's parent-child relationships. Each
    // IsolatedObject must have one and only one entry in this map, that maps
    // its ObjectID to its ParentChildrenIDs struct.
    HashMap<AXID, ParentChildrenIDs> m_nodeMap;

    // Only accessed on the main thread.
    // The key is the ID of the object that will be resolved into an m_pendingAppends NodeChange.
    // The value is whether the wrapper should be attached on the main thread or the AX thread.
    HashMap<AXID, AttachWrapper> m_unresolvedPendingAppends;
    // Only accessed on the main thread.
    // This is used when updating the isolated tree in response to dynamic children changes.
    // It is required to protect objects from being incorrectly deleted when they are re-parented,
    // as the original parent will want to queue it for removal, but we need to keep the object around
    // for the new parent.
    HashSet<AXID> m_protectedFromDeletionIDs;
    // Only accessed on the main thread.
    // Objects whose parent has changed, and said change needs to be synced to the secondary thread.
    HashSet<AXID> m_needsParentUpdate;
    // 1-based tree level, 0 = not collecting. Only accessed on the main thread.
    unsigned m_collectingNodeChangesAtTreeLevel { 0 };

    // Only accessed on AX thread requesting data.
    HashMap<AXID, Ref<AXIsolatedObject>> m_readerThreadNodeMap;

    // Written to by main thread under lock, accessed and applied by AX thread.
    RefPtr<AXIsolatedObject> m_rootNode WTF_GUARDED_BY_LOCK(m_changeLogLock);
    Vector<NodeChange> m_pendingAppends WTF_GUARDED_BY_LOCK(m_changeLogLock); // Nodes to be added to the tree and platform-wrapped.
    Vector<AXPropertyChange> m_pendingPropertyChanges WTF_GUARDED_BY_LOCK(m_changeLogLock);
    Vector<AXID> m_pendingSubtreeRemovals WTF_GUARDED_BY_LOCK(m_changeLogLock); // Nodes whose subtrees are to be removed from the tree.
    Vector<std::pair<AXID, Vector<AXID>>> m_pendingChildrenUpdates WTF_GUARDED_BY_LOCK(m_changeLogLock);
    HashSet<AXID> m_pendingProtectedFromDeletionIDs WTF_GUARDED_BY_LOCK(m_changeLogLock);
    HashMap<AXID, AXID> m_pendingParentUpdates WTF_GUARDED_BY_LOCK(m_changeLogLock);
    AXID m_pendingFocusedNodeID WTF_GUARDED_BY_LOCK(m_changeLogLock);
    bool m_queuedForDestruction WTF_GUARDED_BY_LOCK(m_changeLogLock) { false };
    AXID m_focusedNodeID;
    std::atomic<double> m_loadingProgress { 0 };

    // Relationships between objects.
    HashMap<AXID, AXRelations> m_relations WTF_GUARDED_BY_LOCK(m_changeLogLock);
    // Set to true by the AXObjectCache and false by AXIsolatedTree.
    bool m_relationsNeedUpdate { true };

    Lock m_changeLogLock;
    AXTextMarkerRange m_selectedTextMarkerRange WTF_GUARDED_BY_LOCK(m_changeLogLock);

    // Queued node updates used for building a new tree snapshot.
    ListHashSet<AXID> m_needsUpdateChildren;
    ListHashSet<AXID> m_needsUpdateNode;
    HashMap<AXID, AXPropertyNameSet> m_needsPropertyUpdates;
};

inline AXObjectCache* AXIsolatedTree::axObjectCache() const
{
    ASSERT(isMainThread());
    return m_axObjectCache.get();
}

inline RefPtr<AXIsolatedTree> AXIsolatedTree::treeForPageID(std::optional<PageIdentifier> pageID)
{
    return pageID ? treeForPageID(*pageID) : nullptr;
}

} // namespace WebCore

#endif
