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
class Page;

using AXIsolatedTreeID = unsigned;

enum class AXPropertyName : uint16_t {
    ARIAControlsElements,
    ARIADetailsElements,
    DropEffects,
    ARIAErrorMessageElements,
    ARIAIsMultiline,
    ARIAFlowToElements,
    ARIALandmarkRoleDescription,
    ARIATreeItemContent,
    ARIATreeRows,
    ARIARoleAttribute,
    ARIAOwnsElements,
    AXColumnCount,
    AXColumnIndex,
    AXRowCount,
    AXRowIndex,
    AccessKey,
    AccessibilityButtonState,
    AccessibilityDescription,
    AccessibilityText,
    ActionVerb,
    AncestorFlags,
    AutoCompleteValue,
    BlockquoteLevel,
    BrailleLabel,
    BrailleRoleDescription,
    CanHaveSelectedChildren,
    CanSetExpandedAttribute,
    CanSetFocusAttribute,
    CanSetNumericValue,
    CanSetSelectedAttribute,
    CanSetSelectedChildren,
    CanSetTextRangeAttributes,
    CanSetValueAttribute,
    CanvasHasFallbackContent,
#if PLATFORM(MAC)
    CaretBrowsingEnabled,
#endif
    Cells,
    ClassList,
    ClickPoint,
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
    CurrentValue,
    DatetimeAttributeValue,
    DecrementButton,
    Description,
    DisclosedByRow,
    DisclosedRows,
    DocumentEncoding,
    DocumentLinks,
    DocumentURI,
    EditableAncestor,
    EmbeddedImageDescription,
    ExpandedTextValue,
    FileUploadButtonReturnsValueInTitle,
    FocusableAncestor,
    HasARIAValueNow,
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
    HighestEditableAncestor,
    HorizontalScrollBar,
    IdentifierAttribute,
    IncrementButton,
    InnerHTML,
    InvalidStatus,
    IsAccessibilityIgnored,
    IsActiveDescendantOfFocusedContainer,
    IsAnonymousMathOperator,
    IsGrabbed,
    IsARIATreeGridRow,
    IsAttachment,
    IsButton,
    IsBusy,
    IsChecked,
    IsCollapsed,
    IsColumnHeaderCell,
    IsControl,
    IsDataTable,
    IsDescriptionList,
    IsEnabled,
    IsExpanded,
    IsExposable,
    IsFieldset,
    IsFileUploadButton,
    IsFocused,
    IsGroup,
    IsImageMapLink,
    IsIncrementor,
    IsIndeterminate,
    IsInlineText,
    IsInputImage,
    IsInsideLiveRegion,
    IsHeading,
    IsHovered,
    IsKeyboardFocusable,
    IsLandmark,
    IsLink,
    IsLinked,
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
    IsMathText,
    IsMathNumber,
    IsMathOperator,
    IsMathFenceOperator,
    IsMathSeparatorOperator,
    IsMathIdentifier,
    IsMathTable,
    IsMathTableRow,
    IsMathTableCell,
    IsMathMultiscript,
    IsMathToken,
    IsMathScriptObject,
    IsMediaTimeline,
    IsMenu,
    IsMenuBar,
    IsMenuButton,
    IsMenuItem,
    IsMenuList,
    IsMenuListOption,
    IsMenuListPopup,
    IsMenuRelated,
    IsMeter,
    IsMultiSelectable,
    IsOrderedList,
    IsOutput,
    IsPasswordField,
    IsPressed,
    IsProgressIndicator,
    IsRangeControl,
    IsRequired,
    IsRowHeaderCell,
    IsScrollbar,
    IsSearchField,
    IsSelected,
    IsSelectedOptionActive,
    IsShowingValidationMessage,
    IsSlider,
    IsStyleFormatGroup,
    IsTable,
    IsTableCell,
    IsTableColumn,
    IsTableRow,
    IsTextControl,
    IsTree,
    IsTreeItem,
    IsUnorderedList,
    IsUnvisited,
    IsValueAutofilled,
    IsValueAutofillAvailable,
    IsVisible,
    IsVisited,
    IsWidget,
    KeyShortcutsValue,
    Language,
    LayoutCount,
    LinkRelValue,
    LinkedUIElements,
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
    NextSibling,
    Orientation,
    OuterHTML,
    Path,
    PlaceholderValue,
    PressedIsPresent,
    PreviousSibling,
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
    SpeakAs,
    SpeechHint,
    StringValue,
    SubrolePlatformString,
    SupportsRowCountChange,
    SupportsDragging,
    SupportsDropping,
    SupportsARIAOwns,
    SupportsCheckedState,
    SupportsCurrent,
    SupportsDatetimeAttribute,
    SupportsExpanded,
    SupportsExpandedTextValue,
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
    TagName,
    TextLength,
    Title,
    TitleAttributeValue,
    TitleUIElement,
    URL,
    ValueAutofillButtonType,
    ValueDescription,
    ValueForRange,
    ValidationMessage,
    VerticalScrollBar,
    VisibleChildren,
    VisibleRows,
    WebArea,
};

using AXPropertyValueVariant = std::variant<std::nullptr_t, AXID, String, bool, int, unsigned, double, float, uint64_t, Color, URL, LayoutRect, FloatRect, PAL::SessionID, IntPoint, OptionSet<SpeakAs>, std::pair<unsigned, unsigned>, Vector<AccessibilityText>, Vector<AXID>, Vector<std::pair<AXID, AXID>>, Vector<String>, Path, OptionSet<AXAncestorFlag>>;
using AXPropertyMap = HashMap<AXPropertyName, AXPropertyValueVariant, IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>>;

struct AXPropertyChange {
    AXID axID; // ID of the object whose properties changed.
    AXPropertyMap properties; // Changed properties.
};

class AXIsolatedTree : public ThreadSafeRefCounted<AXIsolatedTree> {
    WTF_MAKE_NONCOPYABLE(AXIsolatedTree); WTF_MAKE_FAST_ALLOCATED;
    friend WTF::TextStream& operator<<(WTF::TextStream&, AXIsolatedTree&);
public:
    static Ref<AXIsolatedTree> create(AXObjectCache*);
    virtual ~AXIsolatedTree();

    static void removeTreeForPageID(PageIdentifier);

    static RefPtr<AXIsolatedTree> treeForPageID(PageIdentifier);
    static RefPtr<AXIsolatedTree> treeForID(AXIsolatedTreeID);
    AXObjectCache* axObjectCache() const;

    RefPtr<AXIsolatedObject> rootNode();
    RefPtr<AXIsolatedObject> focusedNode();
    RefPtr<AXIsolatedObject> nodeForID(AXID) const;
    Vector<RefPtr<AXCoreObject>> objectsForIDs(const Vector<AXID>&) const;
    Vector<AXID> idsForObjects(const Vector<RefPtr<AXCoreObject>>&) const;

    struct NodeChange {
        Ref<AXIsolatedObject> isolatedObject;
#if PLATFORM(COCOA)
        RetainPtr<AccessibilityObjectWrapper> wrapper;
#elif USE(ATSPI)
        RefPtr<AccessibilityObjectWrapper> wrapper;
#endif
    };

    void generateSubtree(AXCoreObject&, AXCoreObject*, bool attachWrapper);
    void updateNode(AXCoreObject&);
    void updateNodeProperty(const AXCoreObject&, AXPropertyName);
    void updateChildren(AXCoreObject&);

    double loadingProgress() { return m_loadingProgress; }
    void updateLoadingProgress(double);

    // Removes the corresponding isolated object and all descendants from the m_nodeMap and queues their removal from the tree.
    void removeNode(const AXCoreObject&);
    // Removes the given node and all its descendants from m_nodeMap.
    void removeSubtreeFromNodeMap(AXID axID, AXCoreObject*, const HashSet<AXID>& = { });

    // Both setRootNodeID and setFocusedNodeID are called during the generation
    // of the IsolatedTree.
    // Focused node updates in AXObjectCache use setFocusNodeID.
    void setRootNode(AXIsolatedObject*) WTF_REQUIRES_LOCK(m_changeLogLock);
    void setFocusedNodeID(AXID);

    // Called on AX thread from WebAccessibilityObjectWrapper methods.
    // During layout tests, it is called on the main thread.
    void applyPendingChanges();

    AXIsolatedTreeID treeID() const { return m_treeID; }

private:
    AXIsolatedTree(AXObjectCache*);
    void clear();

    static Lock s_cacheLock;
    static HashMap<AXIsolatedTreeID, Ref<AXIsolatedTree>>& treeIDCache() WTF_REQUIRES_LOCK(s_cacheLock);
    static HashMap<PageIdentifier, Ref<AXIsolatedTree>>& treePageCache() WTF_REQUIRES_LOCK(s_cacheLock);

    // Methods in this block are called on the main thread.
    // Computes the parent ID of the given object, which is generally the "assumed" parent ID (but not always, like in the case of tables).
    AXID parentIDForObject(AXCoreObject&, AXID assumedParentID);
    NodeChange nodeChangeForObject(AXCoreObject&, AXID parentID, bool attachWrapper = true);
    void collectNodeChangesForSubtree(AXCoreObject&, AXID parentID, bool attachWrapper, Vector<NodeChange>&, HashSet<AXID>* = nullptr);
    void queueChange(const NodeChange&) WTF_REQUIRES_LOCK(m_changeLogLock);
    void queueChangesAndRemovals(const Vector<NodeChange>&, const Vector<AXID>& = { });

    AXIsolatedTreeID m_treeID;
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

    // Only accessed on AX thread requesting data.
    HashMap<AXID, Ref<AXIsolatedObject>> m_readerThreadNodeMap;

    // Written to by main thread under lock, accessed and applied by AX thread.
    RefPtr<AXIsolatedObject> m_rootNode WTF_GUARDED_BY_LOCK(m_changeLogLock);
    Vector<NodeChange> m_pendingAppends WTF_GUARDED_BY_LOCK(m_changeLogLock); // Nodes to be added to the tree and platform-wrapped.
    Vector<AXPropertyChange> m_pendingPropertyChanges WTF_GUARDED_BY_LOCK(m_changeLogLock);
    Vector<AXID> m_pendingNodeRemovals WTF_GUARDED_BY_LOCK(m_changeLogLock); // Nodes to be removed from the tree.
    Vector<AXID> m_pendingSubtreeRemovals WTF_GUARDED_BY_LOCK(m_changeLogLock); // Nodes whose subtrees are to be removed from the tree.
    Vector<std::pair<AXID, Vector<AXID>>> m_pendingChildrenUpdates WTF_GUARDED_BY_LOCK(m_changeLogLock);
    AXID m_pendingFocusedNodeID WTF_GUARDED_BY_LOCK(m_changeLogLock);
    AXID m_focusedNodeID;
    double m_pendingLoadingProgress WTF_GUARDED_BY_LOCK(m_changeLogLock) { 0 };
    double m_loadingProgress { 0 };
    Lock m_changeLogLock;
};

inline AXObjectCache* AXIsolatedTree::axObjectCache() const
{
    ASSERT(isMainThread());
    return m_axObjectCache;
}

} // namespace WebCore

#endif
