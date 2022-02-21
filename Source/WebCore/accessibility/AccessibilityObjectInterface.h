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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

// FIXME: Should rename this file AXCoreObject.h.

#include "FrameLoaderClient.h"
#include "HTMLTextFormControlElement.h"
#include "LayoutRect.h"
#include "SimpleRange.h"
#include "TextIteratorBehavior.h"
#include "VisibleSelection.h"
#include "Widget.h"
#include <variant>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefCounted.h>

#if PLATFORM(WIN)
#include "AccessibilityObjectWrapperWin.h"
#include "COMPtr.h"
#endif

#if USE(ATSPI)
#include "AccessibilityObjectAtspi.h"
#endif

#if USE(ATK)
#include <wtf/glib/GRefPtr.h>
#endif

#if PLATFORM(COCOA)
OBJC_CLASS WebAccessibilityObjectWrapper;
typedef WebAccessibilityObjectWrapper AccessibilityObjectWrapper;
typedef struct _NSRange NSRange;
typedef const struct __AXTextMarker* AXTextMarkerRef;
typedef const struct __AXTextMarkerRange* AXTextMarkerRangeRef;
#elif USE(ATSPI)
typedef WebCore::AccessibilityObjectAtspi AccessibilityObjectWrapper;
#elif USE(ATK)
typedef struct _WebKitAccessible WebKitAccessible;
typedef struct _WebKitAccessible AccessibilityObjectWrapper;
#else
class AccessibilityObjectWrapper;
#endif

namespace PAL {
class SessionID;
}

namespace WTF {
class TextStream;
}

namespace WebCore {

class AXCoreObject;
class AXObjectCache;
class AccessibilityScrollView;
class Document;
class Element;
class Frame;
class FrameView;
class Node;
class Page;
class Path;
class QualifiedName;
class RenderObject;
class ScrollView;

struct AccessibilityText;
struct ScrollRectToVisibleOptions;

enum AXIDType { };
using AXID = ObjectIdentifier<AXIDType>;

enum class AXAncestorFlag : uint8_t {
    // When the flags aren't initialized, it means the object hasn't been inserted into the tree,
    // and thus we haven't set any of these ancestry flags.
    FlagsInitialized = 1 << 0,
    HasDocumentRoleAncestor = 1 << 1,
    HasWebApplicationAncestor = 1 << 2,
    IsInDescriptionListDetail = 1 << 3,
    IsInDescriptionListTerm = 1 << 4,
    IsInCell = 1 << 5,

    // Bits 6 and 7 are free.
};

enum class AccessibilityRole {
    Annotation = 1,
    Application,
    ApplicationAlert,
    ApplicationAlertDialog,
    ApplicationDialog,
    ApplicationGroup,
    ApplicationLog,
    ApplicationMarquee,
    ApplicationStatus,
    ApplicationTextGroup,
    ApplicationTimer,
    Audio,
    Blockquote,
    Browser,
    BusyIndicator,
    Button,
    Canvas,
    Caption,
    Cell,
    CheckBox,
    ColorWell,
    Column,
    ColumnHeader,
    ComboBox,
    Definition,
    Deletion,
    DescriptionList,
    DescriptionListDetail,
    DescriptionListTerm,
    Details,
    Directory,
    DisclosureTriangle,
    Div,
    Document,
    DocumentArticle,
    DocumentMath,
    DocumentNote,
    Drawer,
    EditableText,
    Feed,
    Figure,
    Footer,
    Footnote,
    Form,
    GraphicsDocument,
    GraphicsObject,
    GraphicsSymbol,
    Grid,
    GridCell,
    Group,
    GrowArea,
    Heading,
    HelpTag,
    HorizontalRule,
    Ignored,
    Inline,
    Image,
    ImageMap,
    ImageMapLink,
    Incrementor,
    Insertion,
    Label,
    LandmarkBanner,
    LandmarkComplementary,
    LandmarkContentInfo,
    LandmarkDocRegion,
    LandmarkMain,
    LandmarkNavigation,
    LandmarkRegion,
    LandmarkSearch,
    Legend,
    Link,
    List,
    ListBox,
    ListBoxOption,
    ListItem,
    ListMarker,
    Mark,
    MathElement,
    Matte,
    Menu,
    MenuBar,
    MenuButton,
    MenuItem,
    MenuItemCheckbox,
    MenuItemRadio,
    MenuListPopup,
    MenuListOption,
    Meter,
    Model,
    Outline,
    Paragraph,
    PopUpButton,
    Pre,
    Presentational,
    ProgressIndicator,
    RadioButton,
    RadioGroup,
    RowHeader,
    Row,
    RowGroup,
    RubyBase,
    RubyBlock,
    RubyInline,
    RubyRun,
    RubyText,
    Ruler,
    RulerMarker,
    ScrollArea,
    ScrollBar,
    SearchField,
    Sheet,
    Slider,
    SliderThumb,
    SpinButton,
    SpinButtonPart,
    SplitGroup,
    Splitter,
    StaticText,
    Subscript,
    Summary,
    Superscript,
    Switch,
    SystemWide,
    SVGRoot,
    SVGText,
    SVGTSpan,
    SVGTextPath,
    TabGroup,
    TabList,
    TabPanel,
    Tab,
    Table,
    TableHeaderContainer,
    Term,
    TextArea,
    TextField,
    TextGroup,
    Time,
    Tree,
    TreeGrid,
    TreeItem,
    ToggleButton,
    Toolbar,
    Unknown,
    UserInterfaceTooltip,
    ValueIndicator,
    Video,
    WebApplication,
    WebArea,
    WebCoreLink,
    Window,
};

using AccessibilityRoleSet = HashSet<AccessibilityRole, IntHash<AccessibilityRole>, WTF::StrongEnumHashTraits<AccessibilityRole>>;

ALWAYS_INLINE String accessibilityRoleToString(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Annotation:
        return "Annotation";
    case AccessibilityRole::Application:
        return "Application";
    case AccessibilityRole::ApplicationAlert:
        return "ApplicationAlert";
    case AccessibilityRole::ApplicationAlertDialog:
        return "ApplicationAlertDialog";
    case AccessibilityRole::ApplicationDialog:
        return "ApplicationDialog";
    case AccessibilityRole::ApplicationGroup:
        return "ApplicationGroup";
    case AccessibilityRole::ApplicationLog:
        return "ApplicationLog";
    case AccessibilityRole::ApplicationMarquee:
        return "ApplicationMarquee";
    case AccessibilityRole::ApplicationStatus:
        return "ApplicationStatus";
    case AccessibilityRole::ApplicationTextGroup:
        return "ApplicationTextGroup";
    case AccessibilityRole::ApplicationTimer:
        return "ApplicationTimer";
    case AccessibilityRole::Audio:
        return "Audio";
    case AccessibilityRole::Blockquote:
        return "Blockquote";
    case AccessibilityRole::Browser:
        return "Browser";
    case AccessibilityRole::BusyIndicator:
        return "BusyIndicator";
    case AccessibilityRole::Button:
        return "Button";
    case AccessibilityRole::Canvas:
        return "Canvas";
    case AccessibilityRole::Caption:
        return "Caption";
    case AccessibilityRole::Cell:
        return "Cell";
    case AccessibilityRole::CheckBox:
        return "CheckBox";
    case AccessibilityRole::ColorWell:
        return "ColorWell";
    case AccessibilityRole::Column:
        return "Column";
    case AccessibilityRole::ColumnHeader:
        return "ColumnHeader";
    case AccessibilityRole::ComboBox:
        return "ComboBox";
    case AccessibilityRole::Definition:
        return "Definition";
    case AccessibilityRole::Deletion:
        return "Deletion";
    case AccessibilityRole::DescriptionList:
        return "DescriptionList";
    case AccessibilityRole::DescriptionListTerm:
        return "DescriptionListTerm";
    case AccessibilityRole::DescriptionListDetail:
        return "DescriptionListDetail";
    case AccessibilityRole::Details:
        return "Details";
    case AccessibilityRole::Directory:
        return "Directory";
    case AccessibilityRole::DisclosureTriangle:
        return "DisclosureTriangle";
    case AccessibilityRole::Div:
        return "Div";
    case AccessibilityRole::Document:
        return "Document";
    case AccessibilityRole::DocumentArticle:
        return "DocumentArticle";
    case AccessibilityRole::DocumentMath:
        return "DocumentMath";
    case AccessibilityRole::DocumentNote:
        return "DocumentNote";
    case AccessibilityRole::Drawer:
        return "Drawer";
    case AccessibilityRole::EditableText:
        return "EditableText";
    case AccessibilityRole::Feed:
        return "Feed";
    case AccessibilityRole::Figure:
        return "Figure";
    case AccessibilityRole::Footer:
        return "Footer";
    case AccessibilityRole::Footnote:
        return "Footnote";
    case AccessibilityRole::Form:
        return "Form";
    case AccessibilityRole::GraphicsDocument:
        return "GraphicsDocument";
    case AccessibilityRole::GraphicsObject:
        return "GraphicsObject";
    case AccessibilityRole::GraphicsSymbol:
        return "GraphicsSymbol";
    case AccessibilityRole::Grid:
        return "Grid";
    case AccessibilityRole::GridCell:
        return "GridCell";
    case AccessibilityRole::Group:
        return "Group";
    case AccessibilityRole::GrowArea:
        return "GrowArea";
    case AccessibilityRole::Heading:
        return "Heading";
    case AccessibilityRole::HelpTag:
        return "HelpTag";
    case AccessibilityRole::HorizontalRule:
        return "HorizontalRule";
    case AccessibilityRole::Ignored:
        return "Ignored";
    case AccessibilityRole::Inline:
        return "Inline";
    case AccessibilityRole::Image:
        return "Image";
    case AccessibilityRole::ImageMap:
        return "ImageMap";
    case AccessibilityRole::ImageMapLink:
        return "ImageMapLink";
    case AccessibilityRole::Incrementor:
        return "Incrementor";
    case AccessibilityRole::Insertion:
        return "Insertion";
    case AccessibilityRole::Label:
        return "Label";
    case AccessibilityRole::LandmarkBanner:
        return "LandmarkBanner";
    case AccessibilityRole::LandmarkComplementary:
        return "LandmarkComplementary";
    case AccessibilityRole::LandmarkContentInfo:
        return "LandmarkContentInfo";
    case AccessibilityRole::LandmarkDocRegion:
        return "LandmarkDocRegion";
    case AccessibilityRole::LandmarkMain:
        return "LandmarkMain";
    case AccessibilityRole::LandmarkNavigation:
        return "LandmarkNavigation";
    case AccessibilityRole::LandmarkRegion:
        return "LandmarkRegion";
    case AccessibilityRole::LandmarkSearch:
        return "LandmarkSearch";
    case AccessibilityRole::Legend:
        return "Legend";
    case AccessibilityRole::Link:
        return "Link";
    case AccessibilityRole::List:
        return "List";
    case AccessibilityRole::ListBox:
        return "ListBox";
    case AccessibilityRole::ListBoxOption:
        return "ListBoxOption";
    case AccessibilityRole::ListItem:
        return "ListItem";
    case AccessibilityRole::ListMarker:
        return "ListMarker";
    case AccessibilityRole::Mark:
        return "Mark";
    case AccessibilityRole::MathElement:
        return "MathElement";
    case AccessibilityRole::Matte:
        return "Matte";
    case AccessibilityRole::Menu:
        return "Menu";
    case AccessibilityRole::MenuBar:
        return "MenuBar";
    case AccessibilityRole::MenuButton:
        return "MenuButton";
    case AccessibilityRole::MenuItem:
        return "MenuItem";
    case AccessibilityRole::MenuItemCheckbox:
        return "MenuItemCheckbox";
    case AccessibilityRole::MenuItemRadio:
        return "MenuItemRadio";
    case AccessibilityRole::MenuListPopup:
        return "MenuListPopup";
    case AccessibilityRole::MenuListOption:
        return "MenuListOption";
    case AccessibilityRole::Meter:
        return "Meter";
    case AccessibilityRole::Model:
        return "Model";
    case AccessibilityRole::Outline:
        return "Outline";
    case AccessibilityRole::Paragraph:
        return "Paragraph";
    case AccessibilityRole::PopUpButton:
        return "PopUpButton";
    case AccessibilityRole::Pre:
        return "Pre";
    case AccessibilityRole::Presentational:
        return "Presentational";
    case AccessibilityRole::ProgressIndicator:
        return "ProgressIndicator";
    case AccessibilityRole::RadioButton:
        return "RadioButton";
    case AccessibilityRole::RadioGroup:
        return "RadioGroup";
    case AccessibilityRole::RowHeader:
        return "RowHeader";
    case AccessibilityRole::Row:
        return "Row";
    case AccessibilityRole::RowGroup:
        return "RowGroup";
    case AccessibilityRole::RubyBase:
        return "RubyBase";
    case AccessibilityRole::RubyBlock:
        return "RubyBlock";
    case AccessibilityRole::RubyInline:
        return "RubyInline";
    case AccessibilityRole::RubyRun:
        return "RubyRun";
    case AccessibilityRole::RubyText:
        return "RubyText";
    case AccessibilityRole::Ruler:
        return "Ruler";
    case AccessibilityRole::RulerMarker:
        return "RulerMarker";
    case AccessibilityRole::ScrollArea:
        return "ScrollArea";
    case AccessibilityRole::ScrollBar:
        return "ScrollBar";
    case AccessibilityRole::SearchField:
        return "SearchField";
    case AccessibilityRole::Sheet:
        return "Sheet";
    case AccessibilityRole::Slider:
        return "Slider";
    case AccessibilityRole::SliderThumb:
        return "SliderThumb";
    case AccessibilityRole::SpinButton:
        return "SpinButton";
    case AccessibilityRole::SpinButtonPart:
        return "SpinButtonPart";
    case AccessibilityRole::SplitGroup:
        return "SplitGroup";
    case AccessibilityRole::Splitter:
        return "Splitter";
    case AccessibilityRole::StaticText:
        return "StaticText";
    case AccessibilityRole::Subscript:
        return "Subscript";
    case AccessibilityRole::Summary:
        return "Summary";
    case AccessibilityRole::Superscript:
        return "Superscript";
    case AccessibilityRole::Switch:
        return "Switch";
    case AccessibilityRole::SystemWide:
        return "SystemWide";
    case AccessibilityRole::SVGRoot:
        return "SVGRoot";
    case AccessibilityRole::SVGText:
        return "SVGText";
    case AccessibilityRole::SVGTSpan:
        return "SVGTSpan";
    case AccessibilityRole::SVGTextPath:
        return "SVGTextPath";
    case AccessibilityRole::TabGroup:
        return "TabGroup";
    case AccessibilityRole::TabList:
        return "TabList";
    case AccessibilityRole::TabPanel:
        return "TabPanel";
    case AccessibilityRole::Tab:
        return "Tab";
    case AccessibilityRole::Table:
        return "Table";
    case AccessibilityRole::TableHeaderContainer:
        return "TableHeaderContainer";
    case AccessibilityRole::Term:
        return "Term";
    case AccessibilityRole::TextArea:
        return "TextArea";
    case AccessibilityRole::TextField:
        return "TextField";
    case AccessibilityRole::TextGroup:
        return "TextGroup";
    case AccessibilityRole::Time:
        return "Time";
    case AccessibilityRole::Tree:
        return "Tree";
    case AccessibilityRole::TreeGrid:
        return "TreeGrid";
    case AccessibilityRole::TreeItem:
        return "TreeItem";
    case AccessibilityRole::ToggleButton:
        return "ToggleButton";
    case AccessibilityRole::Toolbar:
        return "Toolbar";
    case AccessibilityRole::Unknown:
        return "Unknown";
    case AccessibilityRole::UserInterfaceTooltip:
        return "UserInterfaceTooltip";
    case AccessibilityRole::ValueIndicator:
        return "ValueIndicator";
    case AccessibilityRole::Video:
        return "Video";
    case AccessibilityRole::WebApplication:
        return "WebApplication";
    case AccessibilityRole::WebArea:
        return "WebArea";
    case AccessibilityRole::WebCoreLink:
        return "WebCoreLink";
    case AccessibilityRole::Window:
        return "Window";
    }
    UNREACHABLE();
    return "";
}

enum class AccessibilityDetachmentType { CacheDestroyed, ElementDestroyed, ElementChanged };

enum class AccessibilityConversionSpace { Screen, Page };

enum class AccessibilitySearchDirection {
    Next = 1,
    Previous,
};

enum class AccessibilityTextSource {
    Alternative,
    Children,
    Summary,
    Help,
    Visible,
    TitleTag,
    Placeholder,
    LabelByElement,
    Title,
    Subtitle,
    Action,
};

enum class AccessibilitySearchKey {
    AnyType = 1,
    Article,
    BlockquoteSameLevel,
    Blockquote,
    BoldFont,
    Button,
    CheckBox,
    Control,
    DifferentType,
    FontChange,
    FontColorChange,
    Frame,
    Graphic,
    HeadingLevel1,
    HeadingLevel2,
    HeadingLevel3,
    HeadingLevel4,
    HeadingLevel5,
    HeadingLevel6,
    HeadingSameLevel,
    Heading,
    Highlighted,
    ItalicFont,
    KeyboardFocusable,
    Landmark,
    Link,
    List,
    LiveRegion,
    MisspelledWord,
    Outline,
    PlainText,
    RadioGroup,
    SameType,
    StaticText,
    StyleChange,
    TableSameLevel,
    Table,
    TextField,
    Underline,
    UnvisitedLink,
    VisitedLink,
};

using AXEditingStyleValueVariant = std::variant<String, bool, int>;

struct AccessibilitySearchCriteria {
    AXCoreObject* anchorObject { nullptr };
    AXCoreObject* startObject;
    AccessibilitySearchDirection searchDirection;
    Vector<AccessibilitySearchKey> searchKeys;
    String searchText;
    unsigned resultsLimit;
    bool visibleOnly;
    bool immediateDescendantsOnly;

    AccessibilitySearchCriteria(AXCoreObject* startObject, AccessibilitySearchDirection searchDirection, String searchText, unsigned resultsLimit, bool visibleOnly, bool immediateDescendantsOnly)
        : startObject(startObject)
        , searchDirection(searchDirection)
        , searchText(searchText)
        , resultsLimit(resultsLimit)
        , visibleOnly(visibleOnly)
        , immediateDescendantsOnly(immediateDescendantsOnly)
    { }
};

enum class AccessibilityObjectInclusion {
    IncludeObject,
    IgnoreObject,
    DefaultBehavior,
};

enum class AccessibilityCurrentState { False, True, Page, Step, Location, Date, Time };

enum class AccessibilityButtonState {
    Off = 0,
    On,
    Mixed,
};

enum class AccessibilitySortDirection {
    None,
    Ascending,
    Descending,
    Other,
    Invalid,
};

enum class AccessibilitySearchTextStartFrom {
    Begin, // Search from the beginning of the element.
    Selection, // Search from the position of the current selection.
    End // Search from the end of the element.
};

enum class AccessibilitySearchTextDirection {
    Forward, // Occurrence after the starting range.
    Backward, // Occurrence before the starting range.
    Closest, // Closest occurrence to the starting range, whether after or before.
    All // All occurrences
};

struct AccessibilitySearchTextCriteria {
    Vector<String> searchStrings; // Text strings to search for.
    AccessibilitySearchTextStartFrom start;
    AccessibilitySearchTextDirection direction;

    AccessibilitySearchTextCriteria()
        : start(AccessibilitySearchTextStartFrom::Selection)
        , direction(AccessibilitySearchTextDirection::Forward)
    { }
};

enum class AccessibilityTextOperationType {
    Select,
    Replace,
    Capitalize,
    Lowercase,
    Uppercase
};

struct AccessibilityTextOperation {
    Vector<SimpleRange> textRanges; // text on which perform the operation.
    AccessibilityTextOperationType type;
    String replacementText; // For type = replace.

    AccessibilityTextOperation()
        : type(AccessibilityTextOperationType::Select)
    { }
};

enum class AccessibilityOrientation {
    Vertical,
    Horizontal,
    Undefined,
};

struct AccessibilityTextUnderElementMode {
    enum ChildrenInclusion {
        TextUnderElementModeSkipIgnoredChildren,
        TextUnderElementModeIncludeAllChildren,
        TextUnderElementModeIncludeNameFromContentsChildren, // This corresponds to ARIA concept: nameFrom
    };

    ChildrenInclusion childrenInclusion;
    bool includeFocusableContent;
    Node* ignoredChildNode;

    AccessibilityTextUnderElementMode(ChildrenInclusion c = TextUnderElementModeSkipIgnoredChildren, bool i = false, Node* ignored = nullptr)
        : childrenInclusion(c)
        , includeFocusableContent(i)
        , ignoredChildNode(ignored)
    { }
};

// FIXME: Merge this with CharacterRange (by deleting this and using CharacterRange instead).
struct PlainTextRange {
    unsigned start { 0 };
    unsigned length { 0 };

    PlainTextRange() = default;

    PlainTextRange(unsigned s, unsigned l)
        : start(s)
        , length(l)
    { }

#if PLATFORM(COCOA)
    PlainTextRange(NSRange);
#endif

    bool isNull() const { return !start && !length; }
};

enum class AccessibilityVisiblePositionForBounds {
    First,
    Last,
};

enum class AccessibilityMathScriptObjectType { Subscript, Superscript };
enum class AccessibilityMathMultiscriptObjectType { PreSubscript, PreSuperscript, PostSubscript, PostSuperscript };

// Use this struct to store the isIgnored data that depends on the parents, so that in addChildren()
// we avoid going up the parent chain for each element while traversing the tree with useful information already.
struct AccessibilityIsIgnoredFromParentData {
    AXCoreObject* parent { nullptr };
    bool isAXHidden { false };
    bool isPresentationalChildOfAriaRole { false };
    bool isDescendantOfBarrenParent { false };

    AccessibilityIsIgnoredFromParentData(AXCoreObject* parent = nullptr)
        : parent(parent)
    { }

    bool isNull() const { return !parent; }
};

class AXCoreObject : public ThreadSafeRefCounted<AXCoreObject> {
public:
    virtual ~AXCoreObject() = default;

    // After constructing an accessible object, it must be given a
    // unique ID, then added to AXObjectCache, and finally init() must
    // be called last.
    virtual void setObjectID(AXID) = 0;
    virtual AXID objectID() const = 0;
    virtual void init() = 0;

    // When the corresponding WebCore object that this accessible object
    // represents is deleted, it must be detached.
    void detach(AccessibilityDetachmentType);
    virtual bool isDetached() const = 0;

    typedef Vector<RefPtr<AXCoreObject>> AccessibilityChildrenVector;

    virtual bool isAccessibilityObject() const = 0;
    virtual bool isAccessibilityNodeObject() const = 0;
    virtual bool isAccessibilityRenderObject() const = 0;
    virtual bool isAccessibilityScrollbar() const = 0;
    virtual bool isAccessibilityScrollViewInstance() const = 0;
    virtual bool isAXImageInstance() const = 0;
    virtual bool isAccessibilitySVGRoot() const = 0;
    virtual bool isAccessibilitySVGElement() const = 0;
    virtual bool isAccessibilityTableInstance() const = 0;
    virtual bool isAccessibilityTableColumnInstance() const = 0;
    virtual bool isAccessibilityProgressIndicatorInstance() const = 0;
    virtual bool isAccessibilityListBoxInstance() const = 0;
    virtual bool isAXIsolatedObjectInstance() const = 0;

    virtual bool isAttachmentElement() const = 0;
    virtual bool isHeading() const = 0;
    virtual bool isLink() const = 0;
    bool isImage() const { return roleValue() == AccessibilityRole::Image; }
    bool isImageMap() const { return roleValue() == AccessibilityRole::ImageMap; }
    bool isVideo() const { return roleValue() == AccessibilityRole::Video; }
    virtual bool isNativeImage() const = 0;
    virtual bool isImageButton() const = 0;
    virtual bool isPasswordField() const = 0;
    virtual bool isContainedByPasswordField() const = 0;
    virtual AXCoreObject* passwordFieldOrContainingPasswordField() = 0;
    virtual bool isNativeTextControl() const = 0;
    virtual bool isSearchField() const = 0;
    bool isWebArea() const { return roleValue() == AccessibilityRole::WebArea; }
    bool isCheckbox() const { return roleValue() == AccessibilityRole::CheckBox; }
    bool isRadioButton() const { return roleValue() == AccessibilityRole::RadioButton; }
    bool isListBox() const { return roleValue() == AccessibilityRole::ListBox; }
    virtual bool isListBoxOption() const = 0;
    virtual bool isAttachment() const = 0;
    virtual bool isMediaTimeline() const = 0;
    virtual bool isMenuRelated() const = 0;
    virtual bool isMenu() const = 0;
    virtual bool isMenuBar() const = 0;
    virtual bool isMenuButton() const = 0;
    virtual bool isMenuItem() const = 0;
    virtual bool isFileUploadButton() const = 0;
    virtual bool isInputImage() const = 0;
    virtual bool isProgressIndicator() const = 0;
    virtual bool isSlider() const = 0;
    virtual bool isSliderThumb() const = 0;
    virtual bool isInputSlider() const = 0;
    virtual bool isControl() const = 0;
    virtual bool isLabel() const = 0;
    // lists support (l, ul, ol, dl)
    virtual bool isList() const = 0;
    virtual bool isUnorderedList() const = 0;
    virtual bool isOrderedList() const = 0;
    virtual bool isDescriptionList() const = 0;

    // Table support.
    virtual bool isTable() const = 0;
    virtual bool isExposable() const = 0;
    virtual bool isDataTable() const = 0;
    virtual int tableLevel() const = 0;
    virtual bool supportsSelectedRows() const = 0;
    virtual AccessibilityChildrenVector columns() = 0;
    virtual AccessibilityChildrenVector rows() = 0;
    virtual unsigned columnCount() = 0;
    virtual unsigned rowCount() = 0;
    // All the cells in the table.
    virtual AccessibilityChildrenVector cells() = 0;
    virtual AXCoreObject* cellForColumnAndRow(unsigned column, unsigned row) = 0;
    virtual AccessibilityChildrenVector columnHeaders() = 0;
    virtual AccessibilityChildrenVector rowHeaders() = 0;
    virtual AccessibilityChildrenVector visibleRows() = 0;
    // Returns an object that contains, as children, all the objects that act as headers.
    virtual AXCoreObject* headerContainer() = 0;
    virtual int axColumnCount() const = 0;
    virtual int axRowCount() const = 0;

    // Table cell support.
    virtual bool isTableCell() const = 0;
    // Returns the start location and row span of the cell.
    virtual std::pair<unsigned, unsigned> rowIndexRange() const = 0;
    // Returns the start location and column span of the cell.
    virtual std::pair<unsigned, unsigned> columnIndexRange() const = 0;
    virtual bool isColumnHeaderCell() const = 0;
    virtual bool isRowHeaderCell() const = 0;
    virtual int axColumnIndex() const = 0;
    virtual int axRowIndex() const = 0;

    // Table column support.
    virtual bool isTableColumn() const = 0;
    virtual unsigned columnIndex() const = 0;
    virtual AXCoreObject* columnHeader() = 0;

    // Table row support.
    virtual bool isTableRow() const = 0;
    virtual unsigned rowIndex() const = 0;

    // ARIA tree/grid row support.
    virtual bool isARIATreeGridRow() const = 0;
    virtual AccessibilityChildrenVector disclosedRows() = 0; // Also implemented by ARIATreeItems.
    virtual AXCoreObject* disclosedByRow() const = 0;

    virtual bool isFieldset() const = 0;
    virtual bool isGroup() const = 0;
    virtual bool isImageMapLink() const = 0;
    virtual bool isMenuList() const = 0;
    virtual bool isMenuListPopup() const = 0;
    virtual bool isMenuListOption() const = 0;

    // Native spin buttons.
    bool isSpinButton() const { return roleValue() == AccessibilityRole::SpinButton; }
    virtual bool isNativeSpinButton() const = 0;
    virtual AXCoreObject* incrementButton() = 0;
    virtual AXCoreObject* decrementButton() = 0;
    virtual bool isSpinButtonPart() const = 0;
    virtual bool isIncrementor() const = 0;

    virtual bool isMockObject() const = 0;
    virtual bool isMediaObject() const = 0;
    bool isSwitch() const { return roleValue() == AccessibilityRole::Switch; }
    bool isToggleButton() const { return roleValue() == AccessibilityRole::ToggleButton; }
    virtual bool isTextControl() const = 0;
    virtual bool isARIATextControl() const = 0;
    virtual bool isNonNativeTextControl() const = 0;
    bool isTabList() const { return roleValue() == AccessibilityRole::TabList; }
    bool isTabItem() const { return roleValue() == AccessibilityRole::Tab; }
    bool isRadioGroup() const { return roleValue() == AccessibilityRole::RadioGroup; }
    bool isComboBox() const { return roleValue() == AccessibilityRole::ComboBox; }
    bool isTree() const { return roleValue() == AccessibilityRole::Tree; }
    bool isTreeGrid() const { return roleValue() == AccessibilityRole::TreeGrid; }
    bool isTreeItem() const { return roleValue() == AccessibilityRole::TreeItem; }
    bool isScrollbar() const { return roleValue() == AccessibilityRole::ScrollBar; }
    virtual bool isButton() const = 0;
    
    virtual HashMap<String, AXEditingStyleValueVariant> resolvedEditingStyles() const = 0;

    bool isListItem() const { return roleValue() == AccessibilityRole::ListItem; }
    bool isCheckboxOrRadio() const { return isCheckbox() || isRadioButton(); }
    bool isScrollView() const { return roleValue() == AccessibilityRole::ScrollArea; }
    bool isCanvas() const { return roleValue() == AccessibilityRole::Canvas; }
    bool isPopUpButton() const { return roleValue() == AccessibilityRole::PopUpButton; }
    bool isColorWell() const { return roleValue() == AccessibilityRole::ColorWell; }
    bool isSplitter() const { return roleValue() == AccessibilityRole::Splitter; }
    bool isToolbar() const { return roleValue() == AccessibilityRole::Toolbar; }
    bool isSummary() const { return roleValue() == AccessibilityRole::Summary; }
    bool isBlockquote() const { return roleValue() == AccessibilityRole::Blockquote; }
#if ENABLE(MODEL_ELEMENT)
    bool isModel() const { return roleValue() == AccessibilityRole::Model; }
#endif

    virtual bool isLandmark() const = 0;
    virtual bool isRangeControl() const = 0;
    virtual bool isMeter() const = 0;
    virtual bool isStyleFormatGroup() const = 0;
    virtual bool isFigureElement() const = 0;
    virtual bool isKeyboardFocusable() const = 0;
    virtual bool isOutput() const = 0;

    virtual bool isChecked() const = 0;
    virtual bool isEnabled() const = 0;
    virtual bool isSelected() const = 0;
    virtual bool isFocused() const = 0;
    virtual bool isHovered() const = 0;
    virtual bool isIndeterminate() const = 0;
    virtual bool isLoaded() const = 0;
    virtual bool isMultiSelectable() const = 0;
    // FIXME: should need just one since onscreen should be !offscreen.
    virtual bool isOnScreen() const = 0;
    virtual bool isOffScreen() const = 0;
    virtual bool isPressed() const = 0;
    virtual bool isUnvisited() const = 0;
    virtual bool isVisited() const = 0;
    virtual bool isRequired() const = 0;
    virtual bool supportsRequiredAttribute() const = 0;
    virtual bool isLinked() const = 0;
    virtual bool isExpanded() const = 0;
    virtual bool isVisible() const = 0;
    virtual bool isCollapsed() const = 0;
    virtual void setIsExpanded(bool) = 0;
    virtual FloatRect relativeFrame() const = 0;
    virtual FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const = 0;
    virtual FloatRect unobscuredContentRect() const = 0;
    virtual bool supportsCheckedState() const = 0;
    
    // In a multi-select list, many items can be selected but only one is active at a time.
    virtual bool isSelectedOptionActive() const = 0;

    virtual bool hasBoldFont() const = 0;
    virtual bool hasItalicFont() const = 0;
    virtual bool hasMisspelling() const = 0;
    virtual std::optional<SimpleRange> misspellingRange(const SimpleRange& start, AccessibilitySearchDirection) const = 0;
    virtual std::optional<SimpleRange> visibleCharacterRange() const = 0;
    virtual bool hasPlainText() const = 0;
    virtual bool hasSameFont(const AXCoreObject&) const = 0;
    virtual bool hasSameFontColor(const AXCoreObject&) const = 0;
    virtual bool hasSameStyle(const AXCoreObject&) const = 0;
    bool isStaticText() const { return roleValue() == AccessibilityRole::StaticText; }
    virtual bool hasUnderline() const = 0;
    virtual bool hasHighlighting() const = 0;

    virtual bool supportsDatetimeAttribute() const = 0;
    virtual String datetimeAttributeValue() const = 0;

    virtual bool canSetFocusAttribute() const = 0;
    virtual bool canSetTextRangeAttributes() const = 0;
    virtual bool canSetValueAttribute() const = 0;
    virtual bool canSetNumericValue() const = 0;
    virtual bool canSetSelectedAttribute() const = 0;
    virtual bool canSetSelectedChildren() const = 0;
    virtual bool canSetExpandedAttribute() const = 0;

    virtual Element* element() const = 0;
    virtual Node* node() const = 0;
    virtual RenderObject* renderer() const = 0;

    virtual bool accessibilityIsIgnored() const = 0;
    virtual AccessibilityObjectInclusion defaultObjectInclusion() const = 0;
    virtual bool accessibilityIsIgnoredByDefault() const = 0;

    virtual bool isShowingValidationMessage() const = 0;
    virtual String validationMessage() const = 0;

    virtual unsigned blockquoteLevel() const = 0;
    virtual unsigned headingLevel() const = 0;
    virtual AccessibilityButtonState checkboxOrRadioValue() const = 0;
    virtual String valueDescription() const = 0;
    virtual float valueForRange() const = 0;
    virtual float maxValueForRange() const = 0;
    virtual float minValueForRange() const = 0;
    virtual float stepValueForRange() const = 0;
    virtual AXCoreObject* selectedRadioButton() = 0;
    virtual AXCoreObject* selectedTabItem() = 0;
    virtual AXCoreObject* selectedListItem() = 0;
    virtual int layoutCount() const = 0;
    virtual double loadingProgress() const = 0;
    virtual String brailleLabel() const = 0;
    virtual String brailleRoleDescription() const = 0;
    virtual String embeddedImageDescription() const = 0;
    virtual std::optional<AccessibilityChildrenVector> imageOverlayElements() = 0;

    virtual bool supportsARIAOwns() const = 0;
    virtual bool isActiveDescendantOfFocusedContainer() const = 0;
    virtual void ariaActiveDescendantReferencingElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaControlsElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaControlsReferencingElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaDescribedByElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaDescribedByReferencingElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaDetailsElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaDetailsReferencingElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaErrorMessageElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaErrorMessageReferencingElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaFlowToReferencingElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaLabelledByElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaLabelledByReferencingElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaOwnsElements(AccessibilityChildrenVector&) const = 0;
    virtual void ariaOwnsReferencingElements(AccessibilityChildrenVector&) const = 0;

    virtual bool hasPopup() const = 0;
    virtual String popupValue() const = 0;
    virtual bool hasDatalist() const = 0;
    virtual bool supportsHasPopup() const = 0;
    virtual bool pressedIsPresent() const = 0;
    virtual bool ariaIsMultiline() const = 0;
    virtual String invalidStatus() const = 0;
    virtual bool supportsPressed() const = 0;
    virtual bool supportsExpanded() const = 0;
    virtual bool supportsChecked() const = 0;
    virtual bool supportsRowCountChange() const = 0;
    virtual AccessibilitySortDirection sortDirection() const = 0;
    virtual bool canvasHasFallbackContent() const = 0;
    virtual bool supportsRangeValue() const = 0;
    virtual String identifierAttribute() const = 0;
    virtual String linkRelValue() const = 0;
    virtual void classList(Vector<String>&) const = 0;
    virtual AccessibilityCurrentState currentState() const = 0;
    virtual String currentValue() const = 0;
    virtual bool supportsCurrent() const = 0;
    virtual const String keyShortcutsValue() const = 0;

    // This function checks if the object should be ignored when there's a modal dialog displayed.
    virtual bool isModalDescendant(Node*) const = 0;
    virtual bool isModalNode() const = 0;

    virtual bool supportsSetSize() const = 0;
    virtual bool supportsPosInSet() const = 0;
    virtual int setSize() const = 0;
    virtual int posInSet() const = 0;

    // ARIA drag and drop
    virtual bool supportsDropping() const = 0;
    virtual bool supportsDragging() const = 0;
    virtual bool isGrabbed() = 0;
    virtual void setARIAGrabbed(bool) = 0;
    virtual Vector<String> determineDropEffects() const = 0;

    // Called on the root AX object to return the deepest available element.
    virtual AXCoreObject* accessibilityHitTest(const IntPoint&) const = 0;
    // Called on the AX object after the render tree determines which is the right AccessibilityRenderObject.
    virtual AXCoreObject* elementAccessibilityHitTest(const IntPoint&) const = 0;

    virtual AXCoreObject* focusedUIElement() const = 0;

    virtual AXCoreObject* firstChild() const = 0;
    virtual AXCoreObject* lastChild() const = 0;
    virtual AXCoreObject* previousSibling() const = 0;
    virtual AXCoreObject* nextSibling() const = 0;
    virtual AXCoreObject* nextSiblingUnignored(int limit) const = 0;
    virtual AXCoreObject* previousSiblingUnignored(int limit) const = 0;
    virtual AXCoreObject* parentObject() const = 0;
    virtual AXCoreObject* parentObjectUnignored() const = 0;
    virtual AXCoreObject* parentObjectIfExists() const = 0;
    virtual void findMatchingObjects(AccessibilitySearchCriteria*, AccessibilityChildrenVector&) = 0;
    virtual bool isDescendantOfBarrenParent() const = 0;
    virtual bool isDescendantOfRole(AccessibilityRole) const = 0;

    virtual bool hasDocumentRoleAncestor() const = 0;
    virtual bool hasWebApplicationAncestor() const = 0;
    virtual bool isInDescriptionListDetail() const = 0;
    virtual bool isInDescriptionListTerm() const = 0;
    virtual bool isInCell() const = 0;

    // Text selection
    virtual Vector<SimpleRange> findTextRanges(const AccessibilitySearchTextCriteria&) const = 0;
    virtual Vector<String> performTextOperation(AccessibilityTextOperation const&) = 0;

    virtual AXCoreObject* observableObject() const = 0;
    virtual void linkedUIElements(AccessibilityChildrenVector&) const = 0;
    virtual AXCoreObject* titleUIElement() const = 0;
    virtual AXCoreObject* correspondingLabelForControlElement() const = 0;
    virtual AXCoreObject* correspondingControlForLabelElement() const = 0;
    virtual AXCoreObject* scrollBar(AccessibilityOrientation) = 0;

    virtual AccessibilityRole ariaRoleAttribute() const = 0;
    virtual bool isPresentationalChildOfAriaRole() const = 0;
    virtual bool ariaRoleHasPresentationalChildren() const = 0;
    virtual bool inheritsPresentationalRole() const = 0;

    using AXValue = std::variant<bool, unsigned, float, String, AccessibilityButtonState, AXCoreObject*>;
    AXValue value();

    // Accessibility Text
    virtual void accessibilityText(Vector<AccessibilityText>&) const = 0;
    // A single method for getting a computed label for an AXObject. It condenses the nuances of accessibilityText. Used by Inspector.
    virtual String computedLabel() = 0;

    // A programmatic way to set a name on an AccessibleObject.
    virtual void setAccessibleName(const AtomString&) = 0;
    virtual bool hasAttributesRequiredForInclusion() const = 0;

    // Accessibility Text - (To be deprecated).
    virtual String accessibilityDescription() const = 0;
    virtual String title() const = 0;
    virtual String helpText() const = 0;

    // Methods for determining accessibility text.
    virtual bool isARIAStaticText() const = 0;
    virtual String stringValue() const = 0;
    virtual String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const = 0;
    virtual String text() const = 0;
    virtual int textLength() const = 0;
    virtual String ariaLabeledByAttribute() const = 0;
    virtual String ariaDescribedByAttribute() const = 0;
    virtual const String placeholderValue() const = 0;
    virtual bool accessibleNameDerivesFromContent() const = 0;

    // Abbreviations
    virtual String expandedTextValue() const = 0;
    virtual bool supportsExpandedTextValue() const = 0;

    virtual void elementsFromAttribute(Vector<Element*>&, const QualifiedName&) const = 0;

    // Only if isColorWell()
    virtual SRGBA<uint8_t> colorValue() const = 0;

    virtual AccessibilityRole roleValue() const = 0;
    // Non-localized string associated with the object role.
    virtual String rolePlatformString() const = 0;
    // Localized string that describes the object's role.
    virtual String roleDescription() const = 0;
    // Localized string that describes ARIA landmark roles.
    virtual String ariaLandmarkRoleDescription() const = 0;
    // Non-localized string associated with the object's subrole.
    virtual String subrolePlatformString() const = 0;

    virtual AXObjectCache* axObjectCache() const = 0;

    virtual Element* anchorElement() const = 0;
    virtual bool supportsPressAction() const = 0;
    virtual Element* actionElement() const = 0;
    virtual LayoutRect boundingBoxRect() const = 0;
    IntRect pixelSnappedBoundingBoxRect() const { return snappedIntRect(boundingBoxRect()); }
    virtual LayoutRect elementRect() const = 0;
    LayoutSize size() const { return elementRect().size(); }
    virtual IntPoint clickPoint() = 0;
    virtual Path elementPath() const = 0;
    virtual bool supportsPath() const = 0;

    virtual TextIteratorBehaviors textIteratorBehaviorForTextRange() const = 0;
    virtual PlainTextRange selectedTextRange() const = 0;
    virtual int insertionPointLineNumber() const = 0;

    virtual URL url() const = 0;
    virtual VisibleSelection selection() const = 0;
    virtual String selectedText() const = 0;
    virtual String accessKey() const = 0;
    virtual String localizedActionVerb() const = 0;
    virtual String actionVerb() const = 0;

    // Widget support.
    virtual bool isWidget() const = 0;
    virtual Widget* widget() const = 0;
    virtual PlatformWidget platformWidget() const = 0;
    virtual Widget* widgetForAttachmentView() const = 0;

#if PLATFORM(COCOA)
    virtual RemoteAXObjectRef remoteParentObject() const = 0;
    virtual FloatRect convertRectToPlatformSpace(const FloatRect&, AccessibilityConversionSpace) const = 0;
#endif
    virtual Page* page() const = 0;
    virtual Document* document() const = 0;
    virtual FrameView* documentFrameView() const = 0;
    virtual Frame* frame() const = 0;
    virtual Frame* mainFrame() const = 0;
    virtual Document* topDocument() const = 0;
    virtual ScrollView* scrollView() const = 0;
    virtual ScrollView* scrollViewAncestor() const = 0;
    virtual String language() const = 0;
    // 1-based, to match the aria-level spec.
    virtual unsigned hierarchicalLevel() const = 0;
    virtual bool isInlineText() const = 0;
    
    virtual void setFocused(bool) = 0;
    virtual void setSelectedText(const String&) = 0;
    virtual void setSelectedTextRange(const PlainTextRange&) = 0;
    virtual bool setValue(const String&) = 0;
    virtual bool replaceTextInRange(const String&, const PlainTextRange&) = 0;
    virtual bool insertText(const String&) = 0;

    virtual bool setValue(float) = 0;
    virtual void setSelected(bool) = 0;
    virtual void setSelectedRows(AccessibilityChildrenVector&) = 0;

    virtual void makeRangeVisible(const PlainTextRange&) = 0;
    virtual bool press() = 0;
    virtual bool performDefaultAction() = 0;
    virtual bool performDismissAction() { return false; }
    
    virtual AccessibilityOrientation orientation() const = 0;
    virtual void increment() = 0;
    virtual void decrement() = 0;

    virtual const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) = 0;

    enum class DescendIfIgnored : uint8_t {
        No,
        Yes
    };
    virtual void addChildren() = 0;
    virtual void addChild(AXCoreObject*, DescendIfIgnored = DescendIfIgnored::Yes) = 0;
    virtual void insertChild(AXCoreObject*, unsigned, DescendIfIgnored = DescendIfIgnored::Yes) = 0;
    Vector<AXID> childrenIDs(bool updateChildrenIfNecessary = true);

    virtual bool canHaveChildren() const = 0;
    virtual void updateChildrenIfNecessary() = 0;
    virtual void setNeedsToUpdateChildren() = 0;
    virtual void setNeedsToUpdateSubtree() = 0;
    virtual void clearChildren() = 0;
    virtual bool needsToUpdateChildren() const = 0;
    virtual void detachFromParent() = 0;
    virtual bool isDetachedFromParent() = 0;

    virtual bool canHaveSelectedChildren() const = 0;
    virtual void selectedChildren(AccessibilityChildrenVector&) = 0;
    virtual void setSelectedChildren(const AccessibilityChildrenVector&) = 0;
    virtual void visibleChildren(AccessibilityChildrenVector&) = 0;
    virtual void tabChildren(AccessibilityChildrenVector&) = 0;
    virtual bool shouldFocusActiveDescendant() const = 0;
    virtual AXCoreObject* activeDescendant() const = 0;
    virtual void handleActiveDescendantChanged() = 0;
    bool isDescendantOfObject(const AXCoreObject*) const;
    bool isAncestorOfObject(const AXCoreObject*) const;
    virtual AXCoreObject* firstAnonymousBlockChild() const = 0;

    virtual std::optional<String> attributeValue(const String&) const = 0;
    virtual bool hasTagName(const QualifiedName&) const = 0;
    virtual String tagName() const = 0;

    virtual VisiblePositionRange visiblePositionRange() const = 0;
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const = 0;
    virtual std::optional<SimpleRange> elementRange() const = 0;
    virtual VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const = 0;
    virtual VisiblePositionRange positionOfLeftWord(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange positionOfRightWord(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange sentenceForPosition(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange paragraphForPosition(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange visiblePositionRangeForRange(const PlainTextRange&) const = 0;
    virtual VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange selectedVisiblePositionRange() const = 0;

    virtual std::optional<SimpleRange> rangeForPlainTextRange(const PlainTextRange&) const = 0;
#if PLATFORM(MAC)
    // FIXME: make this a COCOA method.
    virtual AXTextMarkerRangeRef textMarkerRangeForNSRange(const NSRange&) const = 0;
#endif

    virtual String stringForRange(const SimpleRange&) const = 0;
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const = 0;
    virtual IntRect boundsForRange(const SimpleRange&) const = 0;
    virtual int lengthForVisiblePositionRange(const VisiblePositionRange&) const = 0;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const = 0;

    virtual VisiblePosition visiblePositionForBounds(const IntRect&, AccessibilityVisiblePositionForBounds) const = 0;
    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const = 0;
    virtual VisiblePosition nextVisiblePosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition previousVisiblePosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition nextWordEnd(const VisiblePosition&) const = 0;
    virtual VisiblePosition previousWordStart(const VisiblePosition&) const = 0;
    virtual VisiblePosition nextLineEndPosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition previousLineStartPosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition visiblePositionForIndex(unsigned, bool /*lastIndexOK */) const = 0;

    virtual VisiblePosition visiblePositionForIndex(int) const = 0;
    virtual int indexForVisiblePosition(const VisiblePosition&) const = 0;

    virtual AXCoreObject* accessibilityObjectForPosition(const VisiblePosition&) const = 0;
    virtual int lineForPosition(const VisiblePosition&) const = 0;
    virtual PlainTextRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const = 0;
    virtual int index(const VisiblePosition&) const = 0;

    virtual void lineBreaks(Vector<int>&) const = 0;
    virtual PlainTextRange doAXRangeForLine(unsigned) const = 0;
    virtual PlainTextRange doAXRangeForPosition(const IntPoint&) const = 0;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const = 0;
    virtual PlainTextRange doAXStyleRangeForIndex(unsigned) const = 0;

    virtual String doAXStringForRange(const PlainTextRange&) const = 0;
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const = 0;
    virtual IntRect doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange&) const = 0;

    virtual unsigned doAXLineForIndex(unsigned) = 0;

    virtual String computedRoleString() const = 0;

    // MSAA
    virtual String stringValueForMSAA() const = 0;
    virtual String stringRoleForMSAA() const = 0;
    virtual String nameForMSAA() const = 0;
    virtual String descriptionForMSAA() const = 0;
    virtual AccessibilityRole roleValueForMSAA() const = 0;

    virtual String passwordFieldValue() const = 0;
    virtual bool isValueAutofilled() const = 0;
    virtual bool isValueAutofillAvailable() const = 0;
    virtual AutoFillButtonType valueAutofillButtonType() const = 0;
    virtual bool hasARIAValueNow() const = 0;

    // Used by an ARIA tree to get all its rows.
    virtual void ariaTreeRows(AccessibilityChildrenVector&) = 0;
    // Used by an ARIA tree item to get only its content, and not its child tree items and groups.
    virtual void ariaTreeItemContent(AccessibilityChildrenVector&) = 0;

    // ARIA live-region features.
    virtual bool supportsLiveRegion(bool excludeIfOff = true) const = 0;
    virtual bool isInsideLiveRegion(bool excludeIfOff = true) const = 0;
    virtual AXCoreObject* liveRegionAncestor(bool excludeIfOff = true) const = 0;
    virtual const String liveRegionStatus() const = 0;
    virtual const String liveRegionRelevant() const = 0;
    virtual bool liveRegionAtomic() const = 0;
    virtual bool isBusy() const = 0;

    virtual bool hasContentEditableAttributeSet() const = 0;

    virtual bool supportsReadOnly() const = 0;
    virtual String readOnlyValue() const = 0;

    virtual bool supportsAutoComplete() const = 0;
    virtual String autoCompleteValue() const = 0;

    virtual bool supportsARIAAttributes() const = 0;

    // CSS3 Speech properties.
    virtual OptionSet<SpeakAs> speakAsProperty() const = 0;

    // Make this object visible by scrolling as many nested scrollable views as needed.
    virtual void scrollToMakeVisible() const = 0;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    virtual void scrollToMakeVisibleWithSubFocus(const IntRect&) const = 0;
    // Scroll this object to a given point in global coordinates of the top-level window.
    virtual void scrollToGlobalPoint(const IntPoint&) const = 0;

    enum class ScrollByPageDirection { Up, Down, Left, Right };
    virtual bool scrollByPage(ScrollByPageDirection) const = 0;
    virtual IntPoint scrollPosition() const = 0;
    virtual AccessibilityChildrenVector contents() = 0;
    virtual IntSize scrollContentsSize() const = 0;
    virtual IntRect scrollVisibleContentRect() const = 0;
    virtual void scrollToMakeVisible(const ScrollRectToVisibleOptions&) const = 0;

    // All math elements return true for isMathElement().
    virtual bool isMathElement() const = 0;
    virtual bool isMathFraction() const = 0;
    virtual bool isMathFenced() const = 0;
    virtual bool isMathSubscriptSuperscript() const = 0;
    virtual bool isMathRow() const = 0;
    virtual bool isMathUnderOver() const = 0;
    virtual bool isMathRoot() const = 0;
    virtual bool isMathSquareRoot() const = 0;
    virtual bool isMathText() const = 0;
    virtual bool isMathNumber() const = 0;
    virtual bool isMathOperator() const = 0;
    virtual bool isMathFenceOperator() const = 0;
    virtual bool isMathSeparatorOperator() const = 0;
    virtual bool isMathIdentifier() const = 0;
    virtual bool isMathTable() const = 0;
    virtual bool isMathTableRow() const = 0;
    virtual bool isMathTableCell() const = 0;
    virtual bool isMathMultiscript() const = 0;
    virtual bool isMathToken() const = 0;
    virtual bool isMathScriptObject(AccessibilityMathScriptObjectType) const = 0;
    virtual bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const = 0;

    // Root components.
    virtual std::optional<AccessibilityChildrenVector> mathRadicand() = 0;
    virtual AXCoreObject* mathRootIndexObject() = 0;

    // Under over components.
    virtual AXCoreObject* mathUnderObject() = 0;
    virtual AXCoreObject* mathOverObject() = 0;

    // Fraction components.
    virtual AXCoreObject* mathNumeratorObject() = 0;
    virtual AXCoreObject* mathDenominatorObject() = 0;

    // Subscript/superscript components.
    virtual AXCoreObject* mathBaseObject() = 0;
    virtual AXCoreObject* mathSubscriptObject() = 0;
    virtual AXCoreObject* mathSuperscriptObject() = 0;

    // Fenced components.
    virtual String mathFencedOpenString() const = 0;
    virtual String mathFencedCloseString() const = 0;
    virtual int mathLineThickness() const = 0;
    virtual bool isAnonymousMathOperator() const = 0;

    // Multiscripts components.
    typedef std::pair<AXCoreObject*, AXCoreObject*> AccessibilityMathMultiscriptPair;
    typedef Vector<AccessibilityMathMultiscriptPair> AccessibilityMathMultiscriptPairs;
    virtual void mathPrescripts(AccessibilityMathMultiscriptPairs&) = 0;
    virtual void mathPostscripts(AccessibilityMathMultiscriptPairs&) = 0;

    // Visibility.
    virtual bool isAXHidden() const = 0;
    virtual bool isDOMHidden() const = 0;
    virtual bool isHidden() const = 0;

#if ENABLE(ACCESSIBILITY)
    AccessibilityObjectWrapper* wrapper() const { return m_wrapper.get(); }
    void setWrapper(AccessibilityObjectWrapper* wrapper) { m_wrapper = wrapper; }
    void detachWrapper(AccessibilityDetachmentType);
#else
    AccessibilityObjectWrapper* wrapper() const { return nullptr; }
    void setWrapper(AccessibilityObjectWrapper*) { }
    void detachWrapper(AccessibilityDetachmentType) { }
#endif

    virtual void overrideAttachmentParent(AXCoreObject* parent) = 0;

    // a platform-specific method for determining if an attachment is ignored
    virtual bool accessibilityIgnoreAttachment() const = 0;
    // gives platforms the opportunity to indicate if and how an object should be included
    virtual AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const = 0;

#if PLATFORM(IOS_FAMILY)
    virtual int accessibilityPasswordFieldLength() = 0;
    virtual bool hasTouchEventListener() const = 0;
    virtual bool isInputTypePopupButton() const = 0;
#endif

    // allows for an AccessibilityObject to update its render tree or perform
    // other operations update type operations
    virtual void updateBackingStore() = 0;

#if PLATFORM(COCOA)
    virtual bool preventKeyboardDOMEventDispatch() const = 0;
    virtual void setPreventKeyboardDOMEventDispatch(bool) = 0;
    virtual bool fileUploadButtonReturnsValueInTitle() const = 0;
    virtual String speechHintAttributeValue() const = 0;
    virtual String descriptionAttributeValue() const = 0;
    virtual String helpTextAttributeValue() const = 0;
    // This should be the visible text that's actually on the screen if possible.
    // If there's alternative text, that can override the title.
    virtual String titleAttributeValue() const = 0;

    virtual bool hasApplePDFAnnotationAttribute() const = 0;
#endif

#if PLATFORM(MAC)
    virtual bool caretBrowsingEnabled() const = 0;
    virtual void setCaretBrowsingEnabled(bool) = 0;
#endif

    virtual AXCoreObject* focusableAncestor() = 0;
    virtual AXCoreObject* editableAncestor() = 0;
    virtual AXCoreObject* highestEditableAncestor() = 0;

    virtual const AccessibilityScrollView* ancestorAccessibilityScrollView(bool includeSelf) const = 0;
    virtual AXCoreObject* webAreaObject() const = 0;

    virtual void setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData&) = 0;
    virtual void clearIsIgnoredFromParentData() = 0;
    virtual void setIsIgnoredFromParentDataForChild(AXCoreObject*) = 0;
    
    virtual PAL::SessionID sessionID() const = 0;
    virtual String documentURI() const = 0;
    virtual String documentEncoding() const = 0;
    virtual AccessibilityChildrenVector documentLinks() = 0;

    virtual String innerHTML() const = 0;
    virtual String outerHTML() const = 0;

    
#if PLATFORM(COCOA) && ENABLE(MODEL_ELEMENT)
    virtual Vector<RetainPtr<id>> modelElementChildren() = 0;
#endif
    
private:
    // Detaches this object from the objects it references and it is referenced by.
    virtual void detachRemoteParts(AccessibilityDetachmentType) = 0;

#if PLATFORM(COCOA)
    RetainPtr<WebAccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(WIN)
    COMPtr<AccessibilityObjectWrapper> m_wrapper;
#elif USE(ATSPI)
    RefPtr<AccessibilityObjectAtspi> m_wrapper;
#elif USE(ATK)
    GRefPtr<WebKitAccessible> m_wrapper;
#endif
    virtual void detachPlatformWrapper(AccessibilityDetachmentType) = 0;
};

inline AXCoreObject::AXValue AXCoreObject::value()
{
    if (supportsRangeValue())
        return valueForRange();

    if (roleValue() == AccessibilityRole::SliderThumb)
        return parentObject()->valueForRange();

    if (isHeading())
        return headingLevel();

    if (supportsCheckedState())
        return checkboxOrRadioValue();

    // Radio groups return the selected radio button as the AXValue.
    if (isRadioGroup())
        return selectedRadioButton();

    if (isTabList())
        return selectedTabItem();

    if (isTabItem())
        return isSelected();

    if (isColorWell()) {
        auto color = convertColor<SRGBA<float>>(colorValue()).resolved();
        return makeString("rgb ", String::numberToStringFixedPrecision(color.red, 6, KeepTrailingZeros), " ", String::numberToStringFixedPrecision(color.green, 6, KeepTrailingZeros), " ", String::numberToStringFixedPrecision(color.blue, 6, KeepTrailingZeros), " 1");
    }

    return stringValue();
}

inline void AXCoreObject::detach(AccessibilityDetachmentType detachmentType)
{
    detachWrapper(detachmentType);
    detachRemoteParts(detachmentType);
    setObjectID({ });
}

#if ENABLE(ACCESSIBILITY)
inline void AXCoreObject::detachWrapper(AccessibilityDetachmentType detachmentType)
{
    detachPlatformWrapper(detachmentType);
    m_wrapper = nullptr;
}
#endif

inline Vector<AXID> AXCoreObject::childrenIDs(bool updateChildrenIfNecessary)
{
    return children(updateChildrenIfNecessary).map([] (auto& axObject) -> AXID {
        return axObject->objectID();
    });
}

namespace Accessibility {

template<typename T, typename F>
T* findAncestor(const T& object, bool includeSelf, const F& matches)
{
    T* parent;
    if (includeSelf)
        parent = const_cast<T*>(&object);
    else {
        auto* parentPtr = object.parentObject();
        if (!is<T>(parentPtr))
            return nullptr;
        parent = parentPtr;
    }

    for (; parent; parent = parent->parentObject()) {
        if (matches(*parent))
            return parent;
    }

    return nullptr;
}

void findMatchingObjects(AccessibilitySearchCriteria const&, AXCoreObject::AccessibilityChildrenVector&);

template<typename T, typename F>
void enumerateAncestors(const T& object, bool includeSelf, const F& lambda)
{
    if (includeSelf)
        lambda(object);

    if (auto* parent = object.parentObject())
        enumerateAncestors(*parent, true, lambda);
}

template<typename T, typename F>
void enumerateDescendants(T& object, bool includeSelf, const F& lambda)
{
    if (includeSelf)
        lambda(object);

    for (const auto& child : object.children())
        enumerateDescendants(*child, true, lambda);
}

template<typename U> inline void performFunctionOnMainThread(U&& lambda)
{
    callOnMainThreadAndWait([&lambda] {
        lambda();
    });
}

template<typename T, typename U> inline T retrieveValueFromMainThread(U&& lambda)
{
    T value;
    callOnMainThreadAndWait([&value, &lambda] {
        value = lambda();
    });
    return value;
}

#if PLATFORM(COCOA)
template<typename T, typename U> inline T retrieveAutoreleasedValueFromMainThread(U&& lambda)
{
    RetainPtr<T> value;
    callOnMainThreadAndWait([&value, &lambda] {
        value = lambda();
    });
    return value.autorelease();
}
#endif

} // namespace Accessibility

inline bool AXCoreObject::isDescendantOfObject(const AXCoreObject* axObject) const
{
    return axObject && Accessibility::findAncestor<AXCoreObject>(*this, false, [axObject] (const AXCoreObject& object) {
        return &object == axObject;
    }) != nullptr;
}

inline bool AXCoreObject::isAncestorOfObject(const AXCoreObject* axObject) const
{
    return axObject && (this == axObject || axObject->isDescendantOfObject(this));
}

// Logging helpers.
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilityRole);
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilitySearchDirection);
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilitySearchKey);
WTF::TextStream& operator<<(WTF::TextStream&, const AccessibilitySearchCriteria&);
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilityObjectInclusion);
WTF::TextStream& operator<<(WTF::TextStream&, const AXCoreObject&);

} // namespace WebCore
