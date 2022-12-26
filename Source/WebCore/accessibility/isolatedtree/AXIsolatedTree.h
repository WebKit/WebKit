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

#include "AccessibilityObjectInterface.h"
#include "PageIdentifier.h"
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class AXIsolatedObject;
class AXObjectCache;
class AccessibilityObject;
class Page;
enum class AXStreamOptions : uint8_t;

using AXIsolatedTreeID = unsigned;

enum class AXPropertyName : uint16_t {
    ARIAIsMultiline,
    ARIALandmarkRoleDescription,
    ARIATreeItemContent,
    ARIATreeRows,
    ARIARoleAttribute,
    AXColumnCount,
    AXColumnIndex,
    AXRowCount,
    AXRowIndex,
    AccessKey,
    AccessibilityDescription,
    AccessibilityText,
    ActionVerb,
    AncestorFlags,
    AutoCompleteValue,
    BlockquoteLevel,
    BrailleLabel,
    BrailleRoleDescription,
    ButtonState,
    CanHaveSelectedChildren,
    CanSetExpandedAttribute,
    CanSetFocusAttribute,
    CanSetNumericValue,
    CanSetSelectedAttribute,
    CanSetSelectedChildren,
    CanSetTextRangeAttributes,
    CanSetValueAttribute,
#if PLATFORM(MAC)
    CaretBrowsingEnabled,
#endif
    Cells,
    ColorValue,
    Columns,
    ColumnCount,
    ColumnHeader,
    ColumnHeaders,
    ColumnIndex,
    ColumnIndexRange,
    ComputedRoleString,
    Contents,
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
    HasPopup,
    HasUnderline,
    HeaderContainer,
    HeadingLevel,
    HelpText,
    HierarchicalLevel,
    HorizontalScrollBar,
    IdentifierAttribute,
    IncrementButton,
    InnerHTML,
    InvalidStatus,
    IsGrabbed,
    IsARIATreeGridRow,
    IsAttachment,
    IsButton,
    IsBusy,
    IsChecked,
    IsControl,
    IsEnabled,
    IsExpanded,
    IsExposable,
    IsFieldset,
    IsFocused,
    IsGroup,
    IsIndeterminate,
    IsInlineText,
    IsInputImage,
    IsInsideLiveRegion,
    IsHeading,
    IsKeyboardFocusable,
    IsLandmark,
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
    IsMenu,
    IsMenuBar,
    IsMenuButton,
    IsMenuItem,
    IsMenuList,
    IsMenuListOption,
    IsMenuListPopup,
    IsMenuRelated,
    IsMultiSelectable,
    IsPasswordField,
    IsPressed,
    IsProgressIndicator,
    IsRequired,
    IsScrollbar,
    IsSelected,
    IsSelectedOptionActive,
    IsSlider,
    IsStyleFormatGroup,
    IsTable,
    IsTableCell,
    IsTableColumn,
    IsTableRow,
    IsTextControl,
    IsTree,
    IsTreeItem,
    IsUnvisited,
    IsValueAutofillAvailable,
    IsVisible,
    IsVisited,
    IsWidget,
    KeyShortcuts,
    Language,
    LinkRelValue,
    LinkedObjects,
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
    ReadOnlyValue,
    RoleValue,
    RolePlatformString,
    RoleDescription,
    Rows,
    RowCount,
    RowHeaders,
    RowIndex,
    RowIndexRange,
    SelectedChildren,
    SelectedRadioButton,
    SelectedTabItem,
    SessionID,
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
    SupportsLiveRegion,
    SupportsPath,
    SupportsPosInSet,
    SupportsPressAction,
    SupportsRangeValue,
    SupportsRequiredAttribute,
    SupportsSelectedRows,
    SupportsSetSize,
    TabChildren,
    TableLevel,
    TextLength,
    Title,
    TitleAttributeValue,
    TitleUIElement,
    URL,
    ValueAutofillButtonType,
    ValueDescription,
    ValueForRange,
    VerticalScrollBar,
    VisibleChildren,
    VisibleRows,
};

using AXPropertyValueVariant = std::variant<std::nullptr_t, AXID, String, bool, int, unsigned, double, float, uint64_t, AccessibilityButtonState, Color, URL, LayoutRect, FloatRect, PAL::SessionID, IntPoint, std::pair<unsigned, unsigned>, Vector<AccessibilityText>, Vector<AXID>, Vector<std::pair<AXID, AXID>>, Vector<String>, Path, OptionSet<AXAncestorFlag>>;
using AXPropertyMap = HashMap<AXPropertyName, AXPropertyValueVariant, IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>>;

struct AXPropertyChange {
    AXID axID; // ID of the object whose properties changed.
    AXPropertyMap properties; // Changed properties.
};

class AXIsolatedTree : public ThreadSafeRefCounted<AXIsolatedTree> {
    WTF_MAKE_NONCOPYABLE(AXIsolatedTree); WTF_MAKE_FAST_ALLOCATED;
    friend WTF::TextStream& operator<<(WTF::TextStream&, AXIsolatedTree&);
    friend void streamIsolatedSubtreeOnMainThread(TextStream&, const AXIsolatedTree&, AXID, const OptionSet<AXStreamOptions>&);
public:
    static Ref<AXIsolatedTree> create(AXObjectCache*);
    virtual ~AXIsolatedTree();

    static void removeTreeForPageID(PageIdentifier);

    static RefPtr<AXIsolatedTree> treeForPageID(std::optional<PageIdentifier>);
    static RefPtr<AXIsolatedTree> treeForPageID(PageIdentifier);
    static RefPtr<AXIsolatedTree> treeForID(AXIsolatedTreeID);
    AXObjectCache* axObjectCache() const;

    RefPtr<AXIsolatedObject> rootNode();
    RefPtr<AXIsolatedObject> focusedNode();
    RefPtr<AXIsolatedObject> nodeForID(const AXID&) const;
    Vector<RefPtr<AXCoreObject>> objectsForIDs(const Vector<AXID>&);

    struct NodeChange {
        Ref<AXIsolatedObject> isolatedObject;
#if PLATFORM(COCOA)
        RetainPtr<AccessibilityObjectWrapper> wrapper;
#elif USE(ATSPI)
        RefPtr<AccessibilityObjectWrapper> wrapper;
#endif
    };

    void generateSubtree(AXCoreObject&);
    void updateNode(AXCoreObject&);
    enum class ResolveNodeChanges : bool { No, Yes };
    void updateChildren(AccessibilityObject&, ResolveNodeChanges = ResolveNodeChanges::Yes);
    void updateNodeProperty(AXCoreObject& object, AXPropertyName property) { updateNodeProperties(object, { property }); };
    void updateNodeProperties(AXCoreObject&, const Vector<AXPropertyName>&);
    void updateNodeAndDependentProperties(AXCoreObject&);
    void updatePropertiesForSelfAndDescendants(AXCoreObject&, const Vector<AXPropertyName>&);

    double loadingProgress() { return m_loadingProgress; }
    void updateLoadingProgress(double);

    void addUnconnectedNode(Ref<AccessibilityObject>);
    // Removes the corresponding isolated object and all descendants from the m_nodeMap and queues their removal from the tree.
    void removeNode(const AXCoreObject&);
    // Removes the given node and all its descendants from m_nodeMap.
    void removeSubtreeFromNodeMap(AXID axID, AXCoreObject*);

    // Both setRootNodeID and setFocusedNodeID are called during the generation
    // of the IsolatedTree.
    // Focused node updates in AXObjectCache use setFocusNodeID.
    void setRootNode(AXIsolatedObject*) WTF_REQUIRES_LOCK(m_changeLogLock);
    void setFocusedNodeID(AXID);

    // Relationships between objects.
    std::optional<Vector<AXID>> relatedObjectIDsFor(const AXCoreObject&, AXRelationType);
    void relationsNeedUpdate(bool needUpdate) { m_relationsNeedUpdate = needUpdate; }

    // Called on AX thread from WebAccessibilityObjectWrapper methods.
    // During layout tests, it is called on the main thread.
    void applyPendingChanges();

    AXIsolatedTreeID treeID() const { return m_treeID; }

private:
    AXIsolatedTree(AXObjectCache*);

    // Queue this isolated tree up to destroy itself on the secondary thread.
    // We can't destroy the tree on the main-thread (by removing all `Ref`s to it)
    // because it could be being used by the secondary thread to service an AX request.
    void queueForDestruction();

    static Lock s_cacheLock;
    static HashMap<AXIsolatedTreeID, Ref<AXIsolatedTree>>& treeIDCache() WTF_REQUIRES_LOCK(s_cacheLock);
    static HashMap<PageIdentifier, Ref<AXIsolatedTree>>& treePageCache() WTF_REQUIRES_LOCK(s_cacheLock);

    enum class AttachWrapper : bool { OnMainThread, OnAXThread };
    std::optional<NodeChange> nodeChangeForObject(Ref<AXCoreObject>, AttachWrapper = AttachWrapper::OnMainThread);
    void collectNodeChangesForSubtree(AXCoreObject&);
    bool isCollectingNodeChanges() const { return m_collectingNodeChangesAtTreeLevel > 0; }
    void queueChange(const NodeChange&) WTF_REQUIRES_LOCK(m_changeLogLock);
    void queueRemovals(Vector<AXID>&&);
    void queueRemovalsLocked(Vector<AXID>&&) WTF_REQUIRES_LOCK(m_changeLogLock);
    void queueRemovalsAndUnresolvedChanges(Vector<AXID>&&);

    AXIsolatedTreeID m_treeID;
    unsigned m_maxTreeDepth { 0 };
    AXObjectCache* m_axObjectCache { nullptr };
    bool m_usedOnAXThread { true };

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
    AXID m_pendingFocusedNodeID WTF_GUARDED_BY_LOCK(m_changeLogLock);
    bool m_queuedForDestruction WTF_GUARDED_BY_LOCK(m_changeLogLock) { false };
    AXID m_focusedNodeID;
    std::atomic<double> m_loadingProgress { 0 };

    // Relationships between objects.
    // Accessed only on the AX thread.
    HashMap<AXID, AXRelations> m_relations;
    // Set to true by the AXObjectCache on the main thread.
    // Set to false on the AX thread by relatedObjectIDsFor.
    std::atomic<bool> m_relationsNeedUpdate { true };

    Lock m_changeLogLock;
};

inline AXObjectCache* AXIsolatedTree::axObjectCache() const
{
    ASSERT(isMainThread());
    return m_axObjectCache;
}

inline RefPtr<AXIsolatedTree> AXIsolatedTree::treeForPageID(std::optional<PageIdentifier> pageID)
{
    if (pageID)
        return treeForPageID(*pageID);
    return nullptr;
}

} // namespace WebCore

#endif
