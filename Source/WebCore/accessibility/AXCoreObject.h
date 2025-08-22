/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#include <WebCore/AXTextRun.h>
#include <WebCore/CharacterRange.h>
#include <WebCore/Color.h>
#include <WebCore/ColorConversion.h>
#include <WebCore/HTMLTextFormControlElement.h>
#include <WebCore/InputType.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/NodeName.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextChecking.h>
#include <WebCore/TextIteratorBehavior.h>
#include <WebCore/VisibleSelection.h>
#include <WebCore/Widget.h>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Platform.h>
#include <wtf/ProcessID.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WallTime.h>

#if PLATFORM(WIN)
#include "AccessibilityObjectWrapperWin.h"
#include "COMPtr.h"
#endif

#if USE(ATSPI)
#include "AccessibilityObjectAtspi.h"
#endif

#if PLATFORM(COCOA)
OBJC_CLASS WebAccessibilityObjectWrapper;
typedef WebAccessibilityObjectWrapper AccessibilityObjectWrapper;
typedef struct _NSRange NSRange;
typedef const struct __AXTextMarker* AXTextMarkerRef;
typedef const struct __AXTextMarkerRange* AXTextMarkerRangeRef;
typedef const struct __CTFont* CTFontRef;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSMutableAttributedString;
#elif USE(ATSPI)

namespace WebCore {
class AccessibilityObjectAtspi;
}
typedef WebCore::AccessibilityObjectAtspi AccessibilityObjectWrapper;

#elif PLATFORM(PLAYSTATION)
class AccessibilityObjectWrapper : public RefCounted<AccessibilityObjectWrapper> { };
#else
class AccessibilityObjectWrapper;
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class AXCoreObject;
class AXObjectCache;
class AXTextMarkerRange;
class AccessibilityScrollView;
class Document;
class Element;
class LocalFrame;
class LocalFrameView;
class Node;
class Page;
class Path;
class QualifiedName;
class RenderObject;
class ScrollView;

struct AccessibilitySearchCriteria;
struct AccessibilityText;
struct CharacterRange;
struct ScrollRectToVisibleOptions;

enum class ClickHandlerFilter : bool {
    ExcludeBody,
    IncludeBody,
};

enum class PreSortedObjectType : uint8_t { LiveRegion, WebArea };

enum class DateComponentsType : uint8_t;

enum class AXIDType { };
using AXID = ObjectIdentifier<AXIDType>;

enum class AXAncestorFlag : uint8_t {
    // When the flags aren't initialized, it means the object hasn't been inserted into the tree,
    // and thus we haven't set any of these ancestry flags.
    FlagsInitialized = 1 << 0,
    IsInDescriptionListDetail = 1 << 1,
    IsInDescriptionListTerm = 1 << 2,
    IsInCell = 1 << 3,
    IsInRow = 1 << 4,

    // Bit 5 is free.
};

enum class AccessibilityRole : uint8_t {
    Application,
    ApplicationAlert,
    ApplicationAlertDialog,
    ApplicationDialog,
    ApplicationLog,
    ApplicationMarquee,
    ApplicationStatus,
    ApplicationTimer,
    Audio,
    Blockquote,
    Button,
    Canvas,
    Caption,
    Cell,
    Checkbox,
    Code,
    ColorWell,
    Column,
    ColumnHeader,
    ComboBox,
    DateTime,
    Definition,
    Deletion,
    DescriptionList,
    DescriptionListDetail,
    DescriptionListTerm,
    Details,
    Directory,
    Document,
    DocumentArticle,
    DocumentMath,
    DocumentNote,
    Emphasis,
    Feed,
    Figure,
    Footnote,
    Form,
    Generic,
    GraphicsDocument,
    GraphicsObject,
    GraphicsSymbol,
    Grid,
    GridCell,
    Group,
    Heading,
    HorizontalRule,
    Ignored,
    Inline,
    Image,
    ImageMap,
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
    LineBreak,
    Link,
    List,
    ListBox,
    ListBoxOption,
    ListItem,
    ListMarker,
    Mark,
    MathElement,
    Menu,
    MenuBar,
    MenuItem,
    MenuItemCheckbox,
    MenuItemRadio,
    MenuListPopup,
    MenuListOption,
    Meter,
    Model,
    Paragraph,
    PopUpButton,
    Pre,
    Presentational,
    ProgressIndicator,
    RadioButton,
    RadioGroup,
    RemoteFrame,
    RowHeader,
    Row,
    RowGroup,
    RubyInline,
    RubyText,
    ScrollArea,
    ScrollBar,
    SearchField,
    SectionFooter,
    SectionHeader,
    Slider,
    SliderThumb,
    SpinButton,
    SpinButtonPart,
    Splitter,
    StaticText,
    Strong,
    Subscript,
    Suggestion,
    Summary,
    Superscript,
    Switch,
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
    Video,
    WebApplication,
    WebArea,
};

using AccessibilityRoleSet = HashSet<AccessibilityRole, IntHash<AccessibilityRole>, WTF::StrongEnumHashTraits<AccessibilityRole>>;

ALWAYS_INLINE String accessibilityRoleToString(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Application:
        return "Application"_s;
    case AccessibilityRole::ApplicationAlert:
        return "ApplicationAlert"_s;
    case AccessibilityRole::ApplicationAlertDialog:
        return "ApplicationAlertDialog"_s;
    case AccessibilityRole::ApplicationDialog:
        return "ApplicationDialog"_s;
    case AccessibilityRole::ApplicationLog:
        return "ApplicationLog"_s;
    case AccessibilityRole::ApplicationMarquee:
        return "ApplicationMarquee"_s;
    case AccessibilityRole::ApplicationStatus:
        return "ApplicationStatus"_s;
    case AccessibilityRole::ApplicationTimer:
        return "ApplicationTimer"_s;
    case AccessibilityRole::Audio:
        return "Audio"_s;
    case AccessibilityRole::Blockquote:
        return "Blockquote"_s;
    case AccessibilityRole::Button:
        return "Button"_s;
    case AccessibilityRole::Canvas:
        return "Canvas"_s;
    case AccessibilityRole::Caption:
        return "Caption"_s;
    case AccessibilityRole::Cell:
        return "Cell"_s;
    case AccessibilityRole::Checkbox:
        return "Checkbox"_s;
    case AccessibilityRole::Code:
        return "Code"_s;
    case AccessibilityRole::ColorWell:
        return "ColorWell"_s;
    case AccessibilityRole::Column:
        return "Column"_s;
    case AccessibilityRole::ColumnHeader:
        return "ColumnHeader"_s;
    case AccessibilityRole::ComboBox:
        return "ComboBox"_s;
    case AccessibilityRole::DateTime:
        return "DateTime"_s;
    case AccessibilityRole::Definition:
        return "Definition"_s;
    case AccessibilityRole::Deletion:
        return "Deletion"_s;
    case AccessibilityRole::DescriptionList:
        return "DescriptionList"_s;
    case AccessibilityRole::DescriptionListTerm:
        return "DescriptionListTerm"_s;
    case AccessibilityRole::DescriptionListDetail:
        return "DescriptionListDetail"_s;
    case AccessibilityRole::Details:
        return "Details"_s;
    case AccessibilityRole::Directory:
        return "Directory"_s;
    case AccessibilityRole::Document:
        return "Document"_s;
    case AccessibilityRole::DocumentArticle:
        return "DocumentArticle"_s;
    case AccessibilityRole::DocumentMath:
        return "DocumentMath"_s;
    case AccessibilityRole::DocumentNote:
        return "DocumentNote"_s;
    case AccessibilityRole::Emphasis:
        return "Emphasis"_s;
    case AccessibilityRole::Feed:
        return "Feed"_s;
    case AccessibilityRole::Figure:
        return "Figure"_s;
    case AccessibilityRole::Footnote:
        return "Footnote"_s;
    case AccessibilityRole::Form:
        return "Form"_s;
    case AccessibilityRole::Generic:
        return "Generic"_s;
    case AccessibilityRole::GraphicsDocument:
        return "GraphicsDocument"_s;
    case AccessibilityRole::GraphicsObject:
        return "GraphicsObject"_s;
    case AccessibilityRole::GraphicsSymbol:
        return "GraphicsSymbol"_s;
    case AccessibilityRole::Grid:
        return "Grid"_s;
    case AccessibilityRole::GridCell:
        return "GridCell"_s;
    case AccessibilityRole::Group:
        return "Group"_s;
    case AccessibilityRole::Heading:
        return "Heading"_s;
    case AccessibilityRole::HorizontalRule:
        return "HorizontalRule"_s;
    case AccessibilityRole::Ignored:
        return "Ignored"_s;
    case AccessibilityRole::Inline:
        return "Inline"_s;
    case AccessibilityRole::Image:
        return "Image"_s;
    case AccessibilityRole::ImageMap:
        return "ImageMap"_s;
    case AccessibilityRole::Insertion:
        return "Insertion"_s;
    case AccessibilityRole::Label:
        return "Label"_s;
    case AccessibilityRole::LandmarkBanner:
        return "LandmarkBanner"_s;
    case AccessibilityRole::LandmarkComplementary:
        return "LandmarkComplementary"_s;
    case AccessibilityRole::LandmarkContentInfo:
        return "LandmarkContentInfo"_s;
    case AccessibilityRole::LandmarkDocRegion:
        return "LandmarkDocRegion"_s;
    case AccessibilityRole::LandmarkMain:
        return "LandmarkMain"_s;
    case AccessibilityRole::LandmarkNavigation:
        return "LandmarkNavigation"_s;
    case AccessibilityRole::LandmarkRegion:
        return "LandmarkRegion"_s;
    case AccessibilityRole::LandmarkSearch:
        return "LandmarkSearch"_s;
    case AccessibilityRole::Legend:
        return "Legend"_s;
    case AccessibilityRole::Link:
        return "Link"_s;
    case AccessibilityRole::LineBreak:
        return "LineBreak"_s;
    case AccessibilityRole::List:
        return "List"_s;
    case AccessibilityRole::ListBox:
        return "ListBox"_s;
    case AccessibilityRole::ListBoxOption:
        return "ListBoxOption"_s;
    case AccessibilityRole::ListItem:
        return "ListItem"_s;
    case AccessibilityRole::ListMarker:
        return "ListMarker"_s;
    case AccessibilityRole::Mark:
        return "Mark"_s;
    case AccessibilityRole::MathElement:
        return "MathElement"_s;
    case AccessibilityRole::Menu:
        return "Menu"_s;
    case AccessibilityRole::MenuBar:
        return "MenuBar"_s;
    case AccessibilityRole::MenuItem:
        return "MenuItem"_s;
    case AccessibilityRole::MenuItemCheckbox:
        return "MenuItemCheckbox"_s;
    case AccessibilityRole::MenuItemRadio:
        return "MenuItemRadio"_s;
    case AccessibilityRole::MenuListPopup:
        return "MenuListPopup"_s;
    case AccessibilityRole::MenuListOption:
        return "MenuListOption"_s;
    case AccessibilityRole::Meter:
        return "Meter"_s;
    case AccessibilityRole::Model:
        return "Model"_s;
    case AccessibilityRole::Paragraph:
        return "Paragraph"_s;
    case AccessibilityRole::PopUpButton:
        return "PopUpButton"_s;
    case AccessibilityRole::Pre:
        return "Pre"_s;
    case AccessibilityRole::Presentational:
        return "Presentational"_s;
    case AccessibilityRole::ProgressIndicator:
        return "ProgressIndicator"_s;
    case AccessibilityRole::RadioButton:
        return "RadioButton"_s;
    case AccessibilityRole::RadioGroup:
        return "RadioGroup"_s;
    case AccessibilityRole::RemoteFrame:
        return "RemoteFrame"_s;
    case AccessibilityRole::RowHeader:
        return "RowHeader"_s;
    case AccessibilityRole::Row:
        return "Row"_s;
    case AccessibilityRole::RowGroup:
        return "RowGroup"_s;
    case AccessibilityRole::RubyInline:
        return "RubyInline"_s;
    case AccessibilityRole::RubyText:
        return "RubyText"_s;
    case AccessibilityRole::ScrollArea:
        return "ScrollArea"_s;
    case AccessibilityRole::ScrollBar:
        return "ScrollBar"_s;
    case AccessibilityRole::SearchField:
        return "SearchField"_s;
    case AccessibilityRole::SectionFooter:
        return "SectionFooter"_s;
    case AccessibilityRole::SectionHeader:
        return "SectionHeader"_s;
    case AccessibilityRole::Slider:
        return "Slider"_s;
    case AccessibilityRole::SliderThumb:
        return "SliderThumb"_s;
    case AccessibilityRole::SpinButton:
        return "SpinButton"_s;
    case AccessibilityRole::SpinButtonPart:
        return "SpinButtonPart"_s;
    case AccessibilityRole::Splitter:
        return "Splitter"_s;
    case AccessibilityRole::StaticText:
        return "StaticText"_s;
    case AccessibilityRole::Strong:
        return "Strong"_s;
    case AccessibilityRole::Subscript:
        return "Subscript"_s;
    case AccessibilityRole::Suggestion:
        return "Suggestion"_s;
    case AccessibilityRole::Summary:
        return "Summary"_s;
    case AccessibilityRole::Superscript:
        return "Superscript"_s;
    case AccessibilityRole::Switch:
        return "Switch"_s;
    case AccessibilityRole::SVGRoot:
        return "SVGRoot"_s;
    case AccessibilityRole::SVGText:
        return "SVGText"_s;
    case AccessibilityRole::SVGTSpan:
        return "SVGTSpan"_s;
    case AccessibilityRole::SVGTextPath:
        return "SVGTextPath"_s;
    case AccessibilityRole::TabGroup:
        return "TabGroup"_s;
    case AccessibilityRole::TabList:
        return "TabList"_s;
    case AccessibilityRole::TabPanel:
        return "TabPanel"_s;
    case AccessibilityRole::Tab:
        return "Tab"_s;
    case AccessibilityRole::Table:
        return "Table"_s;
    case AccessibilityRole::TableHeaderContainer:
        return "TableHeaderContainer"_s;
    case AccessibilityRole::Term:
        return "Term"_s;
    case AccessibilityRole::TextArea:
        return "TextArea"_s;
    case AccessibilityRole::TextField:
        return "TextField"_s;
    case AccessibilityRole::TextGroup:
        return "TextGroup"_s;
    case AccessibilityRole::Time:
        return "Time"_s;
    case AccessibilityRole::Tree:
        return "Tree"_s;
    case AccessibilityRole::TreeGrid:
        return "TreeGrid"_s;
    case AccessibilityRole::TreeItem:
        return "TreeItem"_s;
    case AccessibilityRole::ToggleButton:
        return "ToggleButton"_s;
    case AccessibilityRole::Toolbar:
        return "Toolbar"_s;
    case AccessibilityRole::Unknown:
        return "Unknown"_s;
    case AccessibilityRole::UserInterfaceTooltip:
        return "UserInterfaceTooltip"_s;
    case AccessibilityRole::Video:
        return "Video"_s;
    case AccessibilityRole::WebApplication:
        return "WebApplication"_s;
    case AccessibilityRole::WebArea:
        return "WebArea"_s;
    }
    UNREACHABLE();
    return ""_s;
}

enum class AccessibilityDetachmentType { CacheDestroyed, ElementDestroyed, ElementChanged };

enum class AccessibilityConversionSpace { Screen, Page };

// FIXME: This should be replaced by AXDirection (or vice versa).
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
    Heading,
};

using AXEditingStyleValueVariant = Variant<String, bool, int>;

enum class AccessibilityObjectInclusion : uint8_t {
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

enum class AXDirection : bool { Next, Previous };

enum class AccessibilitySortDirection {
    // It's important that Invalid is the first entry, as that means it is the "default value"
    // according to AXIsolatedObject::setProperty, and thus won't be cached unless it's something
    // other than Invalid.
    Invalid,
    None,
    Ascending,
    Descending,
    Other,
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

struct AccessibilityText {
    String text;
    AccessibilityTextSource textSource;

    AccessibilityText(String&& text, AccessibilityTextSource source)
        : text(WTFMove(text))
        , textSource(source)
    { }
    AccessibilityText(const String& text, AccessibilityTextSource source)
        : text(text)
        , textSource(source)
    { }
};

enum class AccessibilityTextOperationType {
    Select,
    Replace,
    Capitalize,
    Lowercase,
    Uppercase,
    ReplacePreserveCase
};

enum class AccessibilityTextOperationSmartReplace : bool { No, Yes };

struct AccessibilityTextOperation {
    Vector<SimpleRange> textRanges; // text on which perform the operation.
    AccessibilityTextOperationType type { AccessibilityTextOperationType::Select };
    Vector<String> replacementStrings; // For type = Replace, ReplacePreserveCase.
    AccessibilityTextOperationSmartReplace smartReplace { AccessibilityTextOperationSmartReplace::Yes };
};

enum class AccessibilityOrientation : uint8_t {
    Undefined,
    Horizontal,
    Vertical
};

enum class IncludeListMarkerText : bool { No, Yes };
enum class TrimWhitespace : bool { No, Yes };

struct TextUnderElementMode {
    enum class Children : uint8_t {
        SkipIgnoredChildren,
        IncludeAllChildren,
        IncludeNameFromContentsChildren, // This corresponds to ARIA concept: nameFrom
    };

    Children childrenInclusion { Children::SkipIgnoredChildren };
    bool includeFocusableContent { false };
    bool considerHiddenState { true };
    bool inHiddenSubtree { false };
    TrimWhitespace trimWhitespace { TrimWhitespace::Yes };
    CheckedPtr<Node> ignoredChildNode { nullptr };

    bool isHidden() { return considerHiddenState && inHiddenSubtree; }
};

struct LineRange {
    unsigned startLineIndex;
    // This index is inclusive.
    unsigned endLineIndex;

    LineRange()
        : startLineIndex(0)
        , endLineIndex(0)
    { }
    explicit LineRange(unsigned startIndex, unsigned endIndex)
        : startLineIndex(startIndex)
        , endLineIndex(endIndex)
    { }

    String debugDescription() const
    {
        return makeString("{start: "_s, startLineIndex, ", end: "_s, endLineIndex, '}');
    }
};

enum class AccessibilityVisiblePositionForBounds {
    First,
    Last,
};

enum class AccessibilityMathScriptObjectType { Subscript, Superscript };
enum class AccessibilityMathMultiscriptObjectType { PreSubscript, PreSuperscript, PostSubscript, PostSuperscript };

enum class CompositionState : uint8_t { Started, InProgress, Ended };

// Relationships between AX objects.
enum class AXRelation : uint8_t {
    None,
    ActiveDescendant,
    ActiveDescendantOf,
    ControlledBy,
    ControllerFor,
    DescribedBy,
    DescriptionFor,
    Details,
    DetailsFor,
    ErrorMessage,
    ErrorMessageFor,
    FlowsFrom,
    FlowsTo,
    Headers,
    HeaderFor,
    LabeledBy,
    LabelFor,
    OwnedBy,
    OwnerFor,
};
using AXRelations = HashMap<AXRelation, ListHashSet<AXID>, DefaultHash<uint8_t>, WTF::UnsignedWithZeroKeyHashTraits<uint8_t>>;

enum class SpinButtonType : bool {
    // The spin button is standalone. It has no separate controls, and should receive and perform actions itself.
    Standalone,
    // The spin button has separate increment and decrement controls.
    Composite
};

enum class ForceLayout : bool { No, Yes };

// Use this struct to store the isIgnored data that depends on the parents, so that in addChildren()
// we avoid going up the parent chain for each element while traversing the tree with useful information already.
struct AccessibilityIsIgnoredFromParentData {
    bool isValid : 1;
    bool isAXHidden : 1;
    bool isPresentationalChildOfAriaRole : 1;
    bool isDescendantOfBarrenParent : 1;

    AccessibilityIsIgnoredFromParentData(AXCoreObject* parent = nullptr)
        : isValid(!!parent)
        , isAXHidden(false)
        , isPresentationalChildOfAriaRole(false)
        , isDescendantOfBarrenParent(false)
    { }

    bool isNull() const { return !isValid; }
};

struct LineDecorationStyle {
    bool hasUnderline { false };
    Color underlineColor;
    bool hasLinethrough { false };
    Color linethroughColor;

    LineDecorationStyle() = default;
    explicit LineDecorationStyle(RenderObject& renderer);
    explicit LineDecorationStyle(bool hasUnderline, Color underlineColor, bool hasLinethrough, Color linethroughColor)
        : hasUnderline(hasUnderline)
        , underlineColor(underlineColor)
        , hasLinethrough(hasLinethrough)
        , linethroughColor(linethroughColor)
    { }
    bool operator==(const LineDecorationStyle&) const = default;

    String debugDescription() const;
};

struct AttributedStringStyle {
#if PLATFORM(COCOA)
    RetainPtr<CTFontRef> font { nil };
#endif
    Color textColor;
    Color backgroundColor;
    bool isSubscript { false };
    bool isSuperscript { false };
    bool hasTextShadow { false };
    LineDecorationStyle lineStyle;

    bool operator==(const AttributedStringStyle&) const = default;

    bool hasUnderline() const { return lineStyle.hasUnderline; }
    Color underlineColor() const { return lineStyle.underlineColor; }
    bool hasLinethrough() const { return lineStyle.hasLinethrough; }
    Color linethroughColor() const { return lineStyle.linethroughColor; }
};

enum class AXDebugStringOption {
    Ignored,
    IsRemoteFrame,
    RelativeFrame,
    RemoteFrameOffset
};

enum class TextEmissionBehavior : uint8_t {
    None,
    Tab,
    Newline,
    DoubleNewline
};

enum class ListBoxInterpretation : uint8_t {
    ActuallyListBox,
    ActuallyStaticList,
    InvalidListBox,
    NotListBox
};

class AXCoreObject : public RefCountedAndCanMakeWeakPtr<AXCoreObject> {
public:
    virtual ~AXCoreObject() = default;
    String debugDescription(bool verbose = false) const { return debugDescriptionInternal(verbose); }
    String debugDescription(OptionSet<AXDebugStringOption> options) const { return debugDescriptionInternal(false, { options }); }

    inline AXID objectID() const { return m_id; }
    virtual std::optional<AXID> treeID() const = 0;
    virtual ProcessID processID() const = 0;

    // When the corresponding WebCore object that this accessible object
    // represents is deleted, it must be detached.
    void detach(AccessibilityDetachmentType);
    virtual bool isDetached() const = 0;

    std::partial_ordering partialOrder(const AXCoreObject&);

    typedef Vector<Ref<AXCoreObject>> AccessibilityChildrenVector;

    virtual bool isAccessibilityObject() const = 0;
    virtual bool isAccessibilityRenderObject() const = 0;
    virtual bool isAXIsolatedObjectInstance() const = 0;
    virtual bool isAXRemoteFrame() const = 0;

    bool isHeading() const { return role() == AccessibilityRole::Heading; }
    bool isLink() const { return role() == AccessibilityRole::Link; };
    bool isCode() const { return role() == AccessibilityRole::Code; }
    bool isImage() const { return role() == AccessibilityRole::Image; }
    bool isImageMap() const { return role() == AccessibilityRole::ImageMap; }
    bool isVideo() const { return role() == AccessibilityRole::Video; }
    virtual bool isSecureField() const = 0;
    virtual bool isNativeTextControl() const = 0;
    bool isWebArea() const { return role() == AccessibilityRole::WebArea; }
    bool isRootWebArea() const;
    bool isCheckbox() const { return role() == AccessibilityRole::Checkbox; }
    bool isRadioButton() const { return role() == AccessibilityRole::RadioButton; }
    bool isListBox() const { return role() == AccessibilityRole::ListBox; }
    // For elements with role=listbox, checks its children to determine if it's actually a valid listbox, a static list, or neither.
    ListBoxInterpretation listBoxInterpretation() const;
    bool isListBoxOption() const { return role() == AccessibilityRole::ListBoxOption; }
    virtual bool isAttachment() const = 0;
    bool isMenuRelated() const;
    bool isMenu() const { return role() == AccessibilityRole::Menu; }
    bool isMenuBar() const { return role() == AccessibilityRole::MenuBar; }
    bool isMenuItem() const;
    bool isInputImage() const;
    bool isProgressIndicator() const { return role() == AccessibilityRole::ProgressIndicator || role() == AccessibilityRole::Meter; }
    bool isSlider() const { return role() == AccessibilityRole::Slider; }
    bool isControl() const;
    bool isRadioInput() const;
    // lists support (l, ul, ol, dl)
    bool isList() const;
    virtual bool isDescriptionList() const = 0;
    bool isFileUploadButton() const;
    // Returns true for objects whose role implies interactivity. For example, when a screen
    // reader announces "link", it doesn't need to announce "clickable" or "pressable" — that
    // is implicit in the concept of a link.
    bool isImplicitlyInteractive() const;
    bool isReplacedElement() const;

    virtual std::optional<InputType::Type> inputType() const = 0;

    // Table support.
    virtual bool isTable() const = 0;
    virtual bool isExposableTable() const = 0;
    unsigned tableLevel() const;
    bool hasGridRole() const;
    bool hasCellRole() const;
    bool supportsSelectedRows() const { return hasGridRole(); }
    virtual AccessibilityChildrenVector columns() = 0;
    virtual AccessibilityChildrenVector rows() = 0;
    virtual unsigned columnCount() = 0;
    virtual unsigned rowCount() = 0;
    // All the cells in the table.
    virtual AccessibilityChildrenVector cells() = 0;
    virtual AXCoreObject* cellForColumnAndRow(unsigned column, unsigned row) = 0;
    AccessibilityChildrenVector columnHeaders();
    virtual AccessibilityChildrenVector rowHeaders() = 0;
    virtual AccessibilityChildrenVector visibleRows() = 0;
    AccessibilityChildrenVector selectedCells();
    // Returns an object that contains, as children, all the objects that act as headers.
    virtual AXCoreObject* tableHeaderContainer() = 0;
    virtual int axColumnCount() const = 0;
    virtual int axRowCount() const = 0;

    // Table cell support.
    virtual bool isTableCell() const = 0;
    virtual bool isExposedTableCell() const = 0;
    virtual bool isColumnHeader() const { return false; }
    virtual bool isRowHeader() const { return false; }
    bool isTableCellInSameRowGroup(AXCoreObject&);
    bool isTableCellInSameColGroup(AXCoreObject*);
    std::optional<AXID> rowGroupAncestorID() const;
    virtual String cellScope() const { return { }; }
    // Returns the start location and row span of the cell.
    virtual std::pair<unsigned, unsigned> rowIndexRange() const = 0;
    // Returns the start location and column span of the cell.
    virtual std::pair<unsigned, unsigned> columnIndexRange() const = 0;
    virtual std::optional<unsigned> axColumnIndex() const = 0;
    virtual std::optional<unsigned> axRowIndex() const = 0;
    virtual String axColumnIndexText() const = 0;
    virtual String axRowIndexText() const = 0;

    // Table column support.
    bool isTableColumn() const { return role() == AccessibilityRole::Column; }
    virtual unsigned columnIndex() const = 0;
    AXCoreObject* columnHeader();

    // Table row support.
    virtual bool isTableRow() const = 0;
    virtual unsigned rowIndex() const = 0;
    AXCoreObject* rowHeader();

    // ARIA tree/grid row support.
    virtual bool isARIAGridRow() const = 0;
    virtual bool isARIATreeGridRow() const = 0;
    virtual AccessibilityChildrenVector disclosedRows() = 0; // Also implemented by ARIATreeItems.
    virtual AXCoreObject* disclosedByRow() const = 0;

    virtual bool isFieldset() const = 0;
    bool isGroup() const;
#if PLATFORM(MAC)
    bool isEmptyGroup();
#endif

    // Native spin buttons.
    bool isSpinButton() const { return role() == AccessibilityRole::SpinButton; }
    SpinButtonType spinButtonType();
    virtual AXCoreObject* incrementButton() = 0;
    virtual AXCoreObject* decrementButton() = 0;

    virtual bool isMockObject() const = 0;
    bool isSwitch() const { return role() == AccessibilityRole::Switch; }
    bool isToggleButton() const { return role() == AccessibilityRole::ToggleButton; }
    bool isTextControl() const;
    virtual bool isEditableWebArea() const = 0;
    virtual bool isNonNativeTextControl() const = 0;
    bool isTabList() const { return role() == AccessibilityRole::TabList; }
    bool isTabItem() const { return role() == AccessibilityRole::Tab; }
    bool isRadioGroup() const { return role() == AccessibilityRole::RadioGroup; }
    bool isComboBox() const { return role() == AccessibilityRole::ComboBox; }
    bool isDateTime() const { return role() == AccessibilityRole::DateTime; }
    bool isGrid() const { return role() == AccessibilityRole::Grid; }
    bool isTree() const { return role() == AccessibilityRole::Tree; }
    bool isTreeGrid() const { return role() == AccessibilityRole::TreeGrid; }
    bool isTreeItem() const { return role() == AccessibilityRole::TreeItem; }
    bool isScrollbar() const { return role() == AccessibilityRole::ScrollBar; }
    bool isRemoteFrame() const { return role() == AccessibilityRole::RemoteFrame; }
#if PLATFORM(COCOA)
    virtual RetainPtr<id> remoteFramePlatformElement() const = 0;
#endif
    virtual bool hasRemoteFrameChild() const = 0;

    bool isButton() const;
    bool isMeter() const { return role() == AccessibilityRole::Meter; }

    bool isListItem() const { return role() == AccessibilityRole::ListItem; }
    bool isCheckboxOrRadio() const { return isCheckbox() || isRadioButton(); }
    bool isScrollView() const { return role() == AccessibilityRole::ScrollArea; }
    bool isCanvas() const { return role() == AccessibilityRole::Canvas; }
    bool isPopUpButton() const { return role() == AccessibilityRole::PopUpButton; }
    bool isColorWell() const { return role() == AccessibilityRole::ColorWell; }
    bool isSplitter() const { return role() == AccessibilityRole::Splitter; }
    bool isToolbar() const { return role() == AccessibilityRole::Toolbar; }
    bool isSummary() const { return role() == AccessibilityRole::Summary; }
    bool isBlockquote() const { return role() == AccessibilityRole::Blockquote; }
#if ENABLE(MODEL_ELEMENT)
    bool isModel() const { return role() == AccessibilityRole::Model; }
#endif
    bool isLineBreak() const { return role() == AccessibilityRole::LineBreak; }

    bool isLandmark() const;
    virtual bool isKeyboardFocusable() const = 0;
    virtual bool isOutput() const = 0;

    virtual bool isChecked() const = 0;
    virtual bool isEnabled() const = 0;
    virtual bool isSelected() const = 0;
    virtual bool isFocused() const = 0;
    virtual bool isIndeterminate() const = 0;
    virtual bool isLoaded() const = 0;
    virtual bool isMultiSelectable() const = 0;
    // FIXME: should need just one since onscreen should be !offscreen.
    virtual bool isOnScreen() const = 0;
    virtual bool isOffScreen() const = 0;
    virtual bool isPressed() const = 0;
    bool isUnvisitedLink() const { return isLink() && !isVisited(); }
    bool isVisitedLink() const { return isLink() && isVisited(); }
    virtual bool isVisited() const = 0;
    virtual bool isRequired() const = 0;
    bool supportsRequiredAttribute() const;
    virtual bool isExpanded() const = 0;
    virtual bool isVisible() const = 0;
    virtual void setIsExpanded(bool) = 0;
    virtual bool supportsCheckedState() const = 0;

    // In a multi-select list, many items can be selected but only one is active at a time.
    virtual bool isSelectedOptionActive() const = 0;

    virtual bool hasBoldFont() const = 0;
    virtual bool hasItalicFont() const = 0;

    // Returns all ranges of misspellings contained within the object.
    virtual Vector<AXTextMarkerRange> misspellingRanges() const = 0;
    virtual std::optional<SimpleRange> misspellingRange(const SimpleRange& start, AccessibilitySearchDirection) const = 0;
#if PLATFORM(COCOA)
    virtual std::optional<NSRange> visibleCharacterRange() const = 0;
#endif
    virtual bool hasPlainText() const = 0;
    virtual bool hasSameFont(AXCoreObject&) = 0;
    virtual bool hasSameFontColor(AXCoreObject&) = 0;
    virtual bool hasSameStyle(AXCoreObject&) = 0;
    bool isStaticText() const { return role() == AccessibilityRole::StaticText; }
    virtual bool hasUnderline() const = 0;
    bool hasHighlighting() const;
    virtual AXTextMarkerRange textInputMarkedTextMarkerRange() const = 0;

    virtual WallTime dateTimeValue() const = 0;
    virtual DateComponentsType dateTimeComponentsType() const = 0;
    bool supportsDatetimeAttribute() const;
    virtual String datetimeAttributeValue() const = 0;

    virtual bool canSetFocusAttribute() const = 0;
    bool canSetTextRangeAttributes() const;
    virtual bool canSetValueAttribute() const = 0;
    bool canSetNumericValue() const { return role() == AccessibilityRole::ScrollBar; }
    virtual bool canSetSelectedAttribute() const = 0;
    bool canSetSelectedChildren() const;
    bool canSetExpandedAttribute() const;

    virtual Element* element() const = 0;
    virtual Node* node() const = 0;
    virtual RenderObject* renderer() const = 0;

    virtual bool isIgnored() const = 0;

    unsigned blockquoteLevel() const;
    unsigned headingLevel() const;
    virtual AccessibilityButtonState checkboxOrRadioValue() const = 0;
    virtual String valueDescription() const = 0;
    virtual float valueForRange() const = 0;
    virtual float maxValueForRange() const = 0;
    virtual float minValueForRange() const = 0;
#if ENABLE(ATTACHMENT_ELEMENT)
    virtual bool hasProgress() const { return false; }
#endif
    AXCoreObject* selectedRadioButton();
    AXCoreObject* selectedTabItem();
    virtual int layoutCount() const = 0;
    virtual double loadingProgress() const = 0;
    virtual String brailleLabel() const = 0;
    virtual String brailleRoleDescription() const = 0;
    virtual String embeddedImageDescription() const = 0;
    virtual std::optional<AccessibilityChildrenVector> imageOverlayElements() = 0;
    virtual String extendedDescription() const = 0;

    bool supportsActiveDescendant() const;
    bool isActiveDescendantOfFocusedContainer() const;
    virtual bool supportsARIAOwns() const = 0;
    bool supportsARIARoleDescription() const;

    // Retrieval of related objects.
    AXCoreObject* activeDescendant() const;
    AccessibilityChildrenVector activeDescendantOfObjects() const { return relatedObjects(AXRelation::ActiveDescendantOf); }
    AccessibilityChildrenVector controlledObjects() const { return relatedObjects(AXRelation::ControllerFor); }
    AccessibilityChildrenVector controllers() const { return relatedObjects(AXRelation::ControlledBy); }
    AccessibilityChildrenVector describedByObjects() const { return relatedObjects(AXRelation::DescribedBy); }
    AccessibilityChildrenVector descriptionForObjects() const { return relatedObjects(AXRelation::DescriptionFor); }
    AccessibilityChildrenVector detailedByObjects() const { return relatedObjects(AXRelation::Details); }
    AccessibilityChildrenVector detailsForObjects() const { return relatedObjects(AXRelation::DetailsFor); }
    AccessibilityChildrenVector errorMessageObjects() const { return relatedObjects(AXRelation::ErrorMessage); }
    AccessibilityChildrenVector errorMessageForObjects() const { return relatedObjects(AXRelation::ErrorMessageFor); }
    AccessibilityChildrenVector flowToObjects() const { return relatedObjects(AXRelation::FlowsTo); }
    AccessibilityChildrenVector flowFromObjects() const { return relatedObjects(AXRelation::FlowsFrom); }
    AccessibilityChildrenVector labeledByObjects() const { return relatedObjects(AXRelation::LabeledBy); }
    AccessibilityChildrenVector labelForObjects() const { return relatedObjects(AXRelation::LabelFor); }
    AccessibilityChildrenVector ownedObjects() const { return relatedObjects(AXRelation::OwnerFor); }
    AccessibilityChildrenVector owners() const { return relatedObjects(AXRelation::OwnedBy); }
    virtual AccessibilityChildrenVector relatedObjects(AXRelation) const = 0;

    virtual AXCoreObject* internalLinkElement() const = 0;
    void appendRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const;
    void appendRadioButtonDescendants(AXCoreObject&, AccessibilityChildrenVector&) const;
    virtual AccessibilityChildrenVector radioButtonGroup() const = 0;

    virtual bool containsOnlyStaticText() const;

    bool hasPopup() const;
    bool selfOrAncestorLinkHasPopup() const;
    virtual String explicitPopupValue() const = 0;
    String popupValue() const;
    virtual bool supportsHasPopup() const = 0;
    virtual bool pressedIsPresent() const = 0;
    virtual String explicitInvalidStatus() const = 0;
    String invalidStatus() const;
    virtual bool supportsExpanded() const = 0;
    virtual bool supportsChecked() const = 0;
    virtual AccessibilitySortDirection sortDirection() const = 0;
    AccessibilitySortDirection sortDirectionIncludingAncestors() const;
    bool supportsRangeValue() const;
    virtual String identifierAttribute() const = 0;
    virtual String linkRelValue() const = 0;
    virtual Vector<String> classList() const = 0;
    virtual AccessibilityCurrentState currentState() const = 0;
    virtual bool supportsCurrent() const = 0;
    String currentValue() const;
    virtual bool supportsKeyShortcuts() const = 0;
    virtual String keyShortcuts() const = 0;

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

    virtual AXCoreObject* focusedUIElement() const = 0;

#if PLATFORM(COCOA)
    virtual RetainPtr<RemoteAXObjectRef> remoteParent() const = 0;
#endif
    virtual AXCoreObject* parentObject() const = 0;
    virtual AXCoreObject* parentObjectUnignored() const;
    AXCoreObject* parentInCoreTree() const
    {
        // Returns the parent in the "core", platform-agnostic accessibility tree, which is not necessarily
        // the same parent that is actually exposed to assistive technologies (i.e. one that is unignored).
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
        return parentObject();
#else
        return parentObjectUnignored();
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    }

    virtual AccessibilityChildrenVector findMatchingObjects(AccessibilitySearchCriteria&&) = 0;
    virtual bool isDescendantOfRole(AccessibilityRole) const = 0;
    AXCoreObject* selfOrFirstTextDescendant();

    virtual bool isInDescriptionListTerm() const = 0;

    // Text selection
    virtual Vector<SimpleRange> findTextRanges(const AccessibilitySearchTextCriteria&) const = 0;
    virtual Vector<String> performTextOperation(const AccessibilityTextOperation&) = 0;

    AccessibilityChildrenVector linkedObjects() const;
    virtual AXCoreObject* titleUIElement() const;
    virtual AXCoreObject* scrollBar(AccessibilityOrientation) = 0;

    virtual bool inheritsPresentationalRole() const = 0;

    using AXValue = Variant<bool, unsigned, float, String, WallTime, AccessibilityButtonState, AXCoreObject*>;
    AXValue value();

    // Accessibility Text
    virtual void accessibilityText(Vector<AccessibilityText>&) const = 0;
    // A programmatic way to set a name on an AccessibleObject.
    virtual void setAccessibleName(const AtomString&) = 0;

    bool fileUploadButtonReturnsValueInTitle() const
    {
#if PLATFORM(MAC)
        return true;
#else
        return false;
#endif
    }
    // This should be the visible text that's actually on the screen if possible.
    // If there's alternative text (e.g. provided by description()), that can override the title.
    virtual String title() const;
    // This is the value of the title HTML / SVG attribute, differing from the above function which refers to
    // the notion of "title" accessibility text, a composite of many different attributes and page text.
    virtual String titleAttribute() const = 0;
    virtual String webAreaTitle() const { return emptyString(); }

    virtual String description() const = 0;

    virtual std::optional<String> textContent() const = 0;
    virtual String textContentPrefixFromListMarker() const = 0;
#if ENABLE(AX_THREAD_TEXT_APIS)
    virtual bool hasTextRuns() = 0;
    virtual TextEmissionBehavior textEmissionBehavior() const = 0;
    bool emitsNewline() const;
    virtual AXTextRunLineID listMarkerLineID() const = 0;
    virtual String listMarkerText() const = 0;
    virtual FontOrientation fontOrientation() const = 0;
#endif

    // Methods for determining accessibility text.
    virtual String stringValue() const = 0;
    virtual String textUnderElement(TextUnderElementMode = { }) const = 0;
    virtual String text() const = 0;
    virtual unsigned textLength() const = 0;
#if PLATFORM(COCOA)
    enum class SpellCheck : bool { No, Yes };
    virtual RetainPtr<NSAttributedString> attributedStringForTextMarkerRange(AXTextMarkerRange&&, SpellCheck) const = 0;
    virtual AttributedStringStyle stylesForAttributedString() const = 0;
    virtual Color textColor() const = 0;
    virtual RetainPtr<CTFontRef> font() const = 0;
#endif

#if PLATFORM(MAC)
    RetainPtr<NSMutableAttributedString> createAttributedString(StringView text, SpellCheck) const;
#endif

    virtual const String placeholderValue() const = 0;

    // Abbreviations
    virtual String abbreviation() const = 0;
    String expandedTextValue() const;
    bool supportsExpandedTextValue() const;

    // Only if isColorWell()
    virtual SRGBA<uint8_t> colorValue() const = 0;

    AccessibilityRole role() const { return m_role; }
#if PLATFORM(MAC)
    // Non-localized string associated with the object role.
    String rolePlatformString();
#else
    String rolePlatformString() { return { }; }
#endif // PLATFORM(MAC)
    // Localized string that describes the object's role.
    String roleDescription();
#if PLATFORM(MAC)
    String rolePlatformDescription();
#else
    String rolePlatformDescription() { return String(); }
#endif
    virtual String ariaRoleDescription() const = 0;
    // Localized string that describes ARIA landmark roles.
    String ariaLandmarkRoleDescription() const;
    // Non-localized string associated with the object's subrole.
    virtual String subrolePlatformString() const = 0;

    bool supportsPressAction() const;
    virtual Element* actionElement() const = 0;

    // Rect relative to root document origin (i.e. absolute coordinates), disregarding viewport state.
    // This does not change when the viewport does (i.e via scrolling).
    virtual LayoutRect elementRect() const = 0;

    // Position relative to the viewport and normalized to screen coordinates.
    // Viewport-relative means that when the page scrolls, the portion of the page in the viewport changes, and thus
    // any viewport-relative rects do too (since they are either closer to or farther from the viewport origin after the scroll).
    virtual FloatPoint screenRelativePosition() const = 0;
    // This is the amount that the RemoteFrame is offset from its containing parent.
    virtual IntPoint remoteFrameOffset() const = 0;

    virtual FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const = 0;
#if PLATFORM(COCOA)
    virtual FloatRect convertRectToPlatformSpace(const FloatRect&, AccessibilityConversionSpace) const = 0;
#endif

    // Rect relative to the viewport.
    virtual FloatRect relativeFrame() const = 0;
#if PLATFORM(MAC)
    virtual FloatRect primaryScreenRect() const = 0;
#endif
    virtual IntSize size() const = 0;
    virtual IntPoint clickPoint() = 0;
    virtual Path elementPath() const = 0;
    virtual bool supportsPath() const = 0;

    virtual CharacterRange selectedTextRange() const = 0;
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
    virtual bool isPlugin() const = 0;

    // FIXME: Remove the following methods from the AXCoreObject interface and instead use methods such as axScrollView() if needed.
    virtual Page* page() const = 0;
    virtual Document* document() const = 0;
    virtual LocalFrameView* documentFrameView() const = 0;
    // Should eliminate the need for exposing scrollView().
    AXCoreObject* axScrollView() const;

    virtual String language() const = 0;
    String languageIncludingAncestors() const;
    virtual unsigned ariaLevel() const = 0;
    // 1-based, to match the aria-level spec.
    unsigned hierarchicalLevel() const;
    virtual bool isInlineText() const = 0;

    virtual void setFocused(bool) = 0;
    virtual void setSelectedText(const String&) = 0;
    virtual void setSelectedTextRange(CharacterRange&&) = 0;
    virtual bool setValue(const String&) = 0;
    virtual void setValueIgnoringResult(const String&) = 0;
    virtual bool replaceTextInRange(const String&, const CharacterRange&) = 0;
    virtual bool insertText(const String&) = 0;

    virtual bool setValue(float) = 0;
    virtual void setValueIgnoringResult(float) = 0;
    virtual void setSelected(bool) = 0;
    virtual void setSelectedRows(AccessibilityChildrenVector&&) = 0;

    virtual bool press() = 0;
    bool performDefaultAction() { return press(); }
    virtual bool performDismissAction() { return false; }
    virtual void performDismissActionIgnoringResult() = 0;

    // An object has an "explicit orientation" when its backing entity explicitly provides one,
    // vs. an "implicit" orientation which is determined inherently by its size or role.
    //
    // An example of an explicit orientation is one provided by aria-orientation. Another is scrollbars,
    // which inherently are horizontal or vertical.
    virtual std::optional<AccessibilityOrientation> explicitOrientation() const = 0;
    AccessibilityOrientation orientation() const;

    virtual void increment() = 0;
    virtual void decrement() = 0;

    // When ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE) is true, this returns ignored children.
    // When it is not, it returns unignored children. After ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    // is the default, we should rename this function to childrenIncludingIgnored, and all callers
    // should be audited to either use that, or unignoredChildren.
    virtual const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) = 0;

    const AccessibilityChildrenVector& childrenIncludingIgnored(bool updateChildrenIfNeeded = true)
    {
        return children(updateChildrenIfNeeded);
    };
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    bool onlyAddsUnignoredChildren() const { return isTableColumn() || role() == AccessibilityRole::TableHeaderContainer; }
    AccessibilityChildrenVector unignoredChildren(bool updateChildrenIfNeeded = true);
    AXCoreObject* firstUnignoredChild();
#else
    const AccessibilityChildrenVector& unignoredChildren(bool updateChildrenIfNeeded = true) { return children(updateChildrenIfNeeded); }
    AXCoreObject* firstUnignoredChild()
    {
        const auto& children = this->children();
        return children.size() ? children[0].ptr() : nullptr;
    }
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    // When ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE) is true, this returns IDs of ignored children.
    // When it is not, it returns IDs of unignored children. After ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    // is the default, we should rename this function to childrenIDsIncludingIgnored, as that is what all
    // callers will expect at the time this comment was written.
    Vector<AXID> childrenIDs(bool updateChildrenIfNeeded = true);
    AXCoreObject* nextInPreOrder(bool updateChildrenIfNeeded = true, AXCoreObject* stayWithin = nullptr);
    AXCoreObject* nextSiblingIncludingIgnored(bool updateChildrenIfNeeded) const;
    AXCoreObject* nextUnignoredSibling(bool updateChildrenIfNeeded, AXCoreObject* unignoredParent = nullptr) const;
    AXCoreObject* nextSiblingIncludingIgnoredOrParent() const;
    std::optional<AXID> idOfNextSiblingIncludingIgnoredOrParent() const
    {
        RefPtr object = nextSiblingIncludingIgnoredOrParent();
        return object ? std::optional(object->objectID()) : std::nullopt;
    }

    AXCoreObject* previousInPreOrder(bool updateChildrenIfNeeded = true, AXCoreObject* stayWithin = nullptr);
    AXCoreObject* previousSiblingIncludingIgnored(bool updateChildrenIfNeeded);
    AXCoreObject* deepestLastChildIncludingIgnored(bool updateChildrenIfNeeded);

    void setIndexInParent(unsigned index)
    {
        m_indexInParent = index;
    }
    bool shouldSetChildIndexInParent() const
    {
        auto role = this->role();
        // Columns and table header containers add cells as children, but are not their "true" parent
        // (the rows are), so these two roles should not update their children's index-in-parent.
        return role != AccessibilityRole::Column && role != AccessibilityRole::TableHeaderContainer;
    }
    // Returns true if setting the index-in-parent was successful.
    bool setChildIndexInParent(AXCoreObject& child, unsigned index) const
    {
        bool shouldSetChildIndex = shouldSetChildIndexInParent();
        if (shouldSetChildIndex)
            child.setIndexInParent(index);
        return shouldSetChildIndex;
    }
    unsigned indexInParent() const { return m_indexInParent; }
#ifndef NDEBUG
    virtual void verifyChildrenIndexInParent() const = 0;
    void verifyChildrenIndexInParent(const AccessibilityChildrenVector&) const;
#endif

    virtual void detachFromParent() = 0;

    AccessibilityChildrenVector listboxSelectedChildren();
    AccessibilityChildrenVector selectedRows();
    AccessibilityChildrenVector selectedListItems();
    bool canHaveSelectedChildren() const;
    AccessibilityChildrenVector selectedChildren();
    virtual void setSelectedChildren(const AccessibilityChildrenVector&) = 0;
    virtual AccessibilityChildrenVector visibleChildren() = 0;
    AccessibilityChildrenVector tabChildren();
    bool isDescendantOfObject(const AXCoreObject&) const;
    bool isAncestorOfObject(const AXCoreObject&) const;

    virtual String nameAttribute() const = 0;

    virtual std::optional<SimpleRange> simpleRange() const = 0;
    virtual VisiblePositionRange visiblePositionRange() const = 0;
    virtual AXTextMarkerRange textMarkerRange() const = 0;

    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const = 0;
    virtual VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const = 0;
    virtual VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const = 0;
    virtual VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const = 0;

    virtual std::optional<SimpleRange> rangeForCharacterRange(const CharacterRange&) const = 0;
#if PLATFORM(COCOA)
    virtual AXTextMarkerRange textMarkerRangeForNSRange(const NSRange&) const = 0;
#endif
#if PLATFORM(MAC)
    virtual AXTextMarkerRange selectedTextMarkerRange() const = 0;
#endif

    virtual IntRect boundsForRange(const SimpleRange&) const = 0;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const = 0;

    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const = 0;
    virtual VisiblePosition visiblePositionForIndex(unsigned, bool /* lastIndexOK */) const = 0;
    virtual VisiblePosition nextLineEndPosition(const VisiblePosition&) const = 0;
    virtual VisiblePosition previousLineStartPosition(const VisiblePosition&) const = 0;

    virtual VisiblePosition visiblePositionForIndex(int) const = 0;
    virtual int indexForVisiblePosition(const VisiblePosition&) const = 0;

    virtual int lineForPosition(const VisiblePosition&) const = 0;

    virtual CharacterRange doAXRangeForLine(unsigned) const = 0;
    virtual CharacterRange characterRangeForPoint(const IntPoint&) const = 0;
    virtual CharacterRange doAXRangeForIndex(unsigned) const = 0;
    virtual CharacterRange doAXStyleRangeForIndex(unsigned) const = 0;

    virtual String doAXStringForRange(const CharacterRange&) const = 0;
    virtual IntRect doAXBoundsForRange(const CharacterRange&) const = 0;
    virtual IntRect doAXBoundsForRangeUsingCharacterOffset(const CharacterRange&) const = 0;

    virtual unsigned doAXLineForIndex(unsigned) = 0;

    virtual String computedRoleString() const = 0;

    virtual bool isValueAutofillAvailable() const = 0;
    virtual AutoFillButtonType valueAutofillButtonType() const = 0;

    // Used by an ARIA tree to get all its rows.
    // FIXME: this should be folded into rows().
    AccessibilityChildrenVector ariaTreeRows();
    // Used by an ARIA tree item to get only its content, and not its child tree items and groups.
    AccessibilityChildrenVector ariaTreeItemContent();

    // ARIA live-region features.
    static bool liveRegionStatusIsEnabled(const AtomString&);
    static const String defaultLiveRegionStatusForRole(AccessibilityRole);
    bool supportsLiveRegion(bool excludeIfOff = true) const;
#if PLATFORM(MAC)
    virtual AccessibilityChildrenVector allSortedLiveRegions() const = 0;
    virtual AccessibilityChildrenVector allSortedNonRootWebAreas() const = 0;
    AccessibilityChildrenVector sortedDescendants(size_t limit, PreSortedObjectType) const;
#endif // PLATFORM(MAC)
    virtual AXCoreObject* liveRegionAncestor(bool excludeIfOff = true) const = 0;
    virtual const String explicitLiveRegionStatus() const = 0;
    const String liveRegionStatus() const;
    virtual const String explicitLiveRegionRelevant() const = 0;
    const String liveRegionRelevant() const;
    virtual bool liveRegionAtomic() const = 0;
    virtual bool isBusy() const = 0;
    virtual String explicitAutoCompleteValue() const = 0;
    String autoCompleteValue() const;

    // Make this object visible by scrolling as many nested scrollable views as needed.
    virtual void scrollToMakeVisible() const = 0;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    virtual void scrollToMakeVisibleWithSubFocus(IntRect&&) const = 0;
    // Scroll this object to a given point in global coordinates of the top-level window.
    virtual void scrollToGlobalPoint(IntPoint&&) const = 0;

    AccessibilityChildrenVector contents();

    // All math elements return true for isMathElement().
    virtual bool isMathElement() const = 0;
    virtual bool isMathFraction() const = 0;
    virtual bool isMathFenced() const = 0;
    virtual bool isMathSubscriptSuperscript() const = 0;
    virtual bool isMathRow() const = 0;
    virtual bool isMathUnderOver() const = 0;
    virtual bool isMathRoot() const = 0;
    virtual bool isMathSquareRoot() const = 0;
    virtual bool isMathTable() const = 0;
    virtual bool isMathTableRow() const = 0;
    virtual bool isMathTableCell() const = 0;
    virtual bool isMathMultiscript() const = 0;
    virtual bool isMathToken() const = 0;
    virtual bool isAnonymousMathOperator() const = 0;

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

    // Multiscripts components.
    typedef std::pair<AXCoreObject*, AXCoreObject*> AccessibilityMathMultiscriptPair;
    typedef Vector<AccessibilityMathMultiscriptPair> AccessibilityMathMultiscriptPairs;
    virtual void mathPrescripts(AccessibilityMathMultiscriptPairs&) = 0;
    virtual void mathPostscripts(AccessibilityMathMultiscriptPairs&) = 0;

    AccessibilityObjectWrapper* wrapper() const { return m_wrapper.get(); }
    void setWrapper(AccessibilityObjectWrapper* wrapper) { m_wrapper = wrapper; }
    void detachWrapper(AccessibilityDetachmentType);

#if PLATFORM(IOS_FAMILY)
    virtual unsigned accessibilitySecureFieldLength() = 0;
    virtual bool hasTouchEventListener() const = 0;
#endif

    // allows for an AccessibilityObject to update its render tree or perform
    // other operations update type operations
    virtual void updateBackingStore() = 0;

#if PLATFORM(COCOA)
    virtual bool preventKeyboardDOMEventDispatch() const = 0;
    virtual void setPreventKeyboardDOMEventDispatch(bool) = 0;
    virtual OptionSet<SpeakAs> speakAs() const = 0;
    String speechHint() const;
    String descriptionAttributeValue() const;
    String helpTextAttributeValue() const;

    virtual bool hasApplePDFAnnotationAttribute() const = 0;
#endif

#if PLATFORM(MAC)
    virtual bool caretBrowsingEnabled() const = 0;
    virtual void setCaretBrowsingEnabled(bool) = 0;
#endif

    virtual bool hasClickHandler() const = 0;
    virtual AXCoreObject* clickableSelfOrAncestor(ClickHandlerFilter = ClickHandlerFilter::ExcludeBody) const = 0;
    virtual AXCoreObject* focusableAncestor() = 0;
    virtual AXCoreObject* editableAncestor() const = 0;
    virtual AXCoreObject* highestEditableAncestor() = 0;
    virtual AXCoreObject* exposedTableAncestor(bool includeSelf = false) const = 0;

    virtual AccessibilityChildrenVector documentLinks() = 0;

    virtual bool hasElementName(ElementName tag) const = 0;
    virtual ElementName elementName() const = 0;

    virtual bool hasAttachmentTag() const = 0;
    virtual bool hasBodyTag() const = 0;
    virtual bool hasMarkTag() const = 0;
    virtual bool hasRowGroupTag() const = 0;

    virtual String innerHTML() const = 0;
    virtual String outerHTML() const = 0;

#if PLATFORM(COCOA) && ENABLE(MODEL_ELEMENT)
    virtual Vector<RetainPtr<id>> modelElementChildren() = 0;
#endif

    String infoStringForTesting();

protected:
    AXCoreObject() = delete;
    explicit AXCoreObject(AXID axID)
        : m_id(axID)
    { }

    explicit AXCoreObject(AXID axID, AccessibilityRole role)
        : m_role(role)
        , m_id(axID)
    { }

    explicit AXCoreObject(AXID axID, AccessibilityRole role, bool getsGeometryFromChildren)
        : m_role(role)
        , m_getsGeometryFromChildren(getsGeometryFromChildren)
        , m_id(axID)
    { }

private:
    virtual String debugDescriptionInternal(bool, std::optional<OptionSet<AXDebugStringOption>> = std::nullopt) const = 0;

    // Detaches this object from the objects it references and it is referenced by.
    virtual void detachRemoteParts(AccessibilityDetachmentType) = 0;
    virtual void detachPlatformWrapper(AccessibilityDetachmentType) = 0;

    void ariaTreeRows(AXCoreObject::AccessibilityChildrenVector& rows, AXCoreObject::AccessibilityChildrenVector& ancestors);

    size_t indexInSiblings(const AccessibilityChildrenVector&) const;

// MARK: Member variables
protected:
    AccessibilityRole m_role { AccessibilityRole::Unknown };
    // Only used by AccessibilityObject, but placed here to use space that would otherwise be taken by padding.
    OptionSet<AXAncestorFlag> m_ancestorFlags;
    // Only used by AccessibilityObject, but placed here to use space that would otherwise be taken by padding.
    AccessibilityObjectInclusion m_lastKnownIsIgnoredValue { AccessibilityObjectInclusion::DefaultBehavior };
    // Only used by AccessibilityObject, but placed here to use space that would otherwise be taken by padding.
    // FIXME: This can be replaced by AXAncestorFlags.
    AccessibilityIsIgnoredFromParentData m_isIgnoredFromParentData;

    // This index always refers to the parent's m_children. Keep in mind that when
    // ENABLE(INCLUDE_IGNORE_IN_CORE_AX_TREE), m_children includes ignored objects, so cannot be
    // used to determine the place of |this| relative to its unignored siblings (only its ignored ones).
    unsigned m_indexInParent;

    bool m_childrenDirty { false };
    // Only used by AccessibilityObject, but placed here to use space that would otherwise be taken by padding.
    bool m_subtreeDirty { false };
    // Only used by AccessibilityObject, but placed here to use space that would otherwise be taken by padding.
    mutable bool m_childrenInitialized { false };
    // Only used by AXIsolatedObject, but placed here to use space that would otherwise be taken by padding.
    // Some objects (e.g. display:contents) form their geometry through their children.
    bool m_getsGeometryFromChildren { false };

private:
    AXID m_id;
#if PLATFORM(COCOA)
    RetainPtr<WebAccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(WIN)
    COMPtr<AccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(PLAYSTATION)
    RefPtr<AccessibilityObjectWrapper> m_wrapper;
#elif USE(ATSPI)
    RefPtr<AccessibilityObjectAtspi> m_wrapper;
#endif
};

inline Vector<AXID> axIDs(const AXCoreObject::AccessibilityChildrenVector& objects)
{
    return WTF::map(objects, [](auto& object) {
        return object->objectID();
    });
}

#if PLATFORM(MAC)
void attributedStringSetExpandedText(NSMutableAttributedString *, const AXCoreObject&, const NSRange&);
void attributedStringSetNeedsSpellCheck(NSMutableAttributedString *, const AXCoreObject&);
void attributedStringSetElement(NSMutableAttributedString *, NSString *attribute, const AXCoreObject&, const NSRange&);
#endif // PLATFORM(MAC)

inline const String AXCoreObject::defaultLiveRegionStatusForRole(AccessibilityRole role)
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

inline bool AXCoreObject::liveRegionStatusIsEnabled(const AtomString& liveRegionStatus)
{
    return equalLettersIgnoringASCIICase(liveRegionStatus, "polite"_s) || equalLettersIgnoringASCIICase(liveRegionStatus, "assertive"_s);
}

inline const String AXCoreObject::liveRegionRelevant() const
{
    auto explicitValue = explicitLiveRegionRelevant();
    // Default aria-relevant = "additions text".
    return explicitValue.isEmpty() ? "additions text"_s : explicitValue;
}

inline const String AXCoreObject::liveRegionStatus() const
{
    auto explicitStatus = explicitLiveRegionStatus();
    return explicitStatus.isEmpty() ? defaultLiveRegionStatusForRole(role()) : explicitStatus;
}

inline bool AXCoreObject::supportsLiveRegion(bool excludeIfOff) const
{
    auto liveRegionStatusValue = liveRegionStatus();
    return excludeIfOff ? liveRegionStatusIsEnabled(AtomString { liveRegionStatusValue }) : !liveRegionStatusValue.isEmpty();
}

inline SpinButtonType AXCoreObject::spinButtonType()
{
    ASSERT_WITH_MESSAGE(isSpinButton(), "spinButtonType() should only be called on spinbuttons.");
    return incrementButton() || decrementButton() ? SpinButtonType::Composite : SpinButtonType::Standalone;
}

inline bool AXCoreObject::canSetTextRangeAttributes() const
{
    return isTextControl();
}

inline bool AXCoreObject::canSetExpandedAttribute() const
{
    return supportsExpanded();
}

inline bool AXCoreObject::canSetSelectedChildren() const
{
    return isListBox() && isEnabled();
}

inline void AXCoreObject::detach(AccessibilityDetachmentType detachmentType)
{
    detachWrapper(detachmentType);
    if (detachmentType != AccessibilityDetachmentType::ElementChanged)
        detachRemoteParts(detachmentType);
}

inline void AXCoreObject::detachWrapper(AccessibilityDetachmentType detachmentType)
{
    detachPlatformWrapper(detachmentType);
    m_wrapper = nullptr;
}

inline Vector<AXID> AXCoreObject::childrenIDs(bool updateChildrenIfNeeded)
{
    return axIDs(children(updateChildrenIfNeeded));
}

#if ENABLE(AX_THREAD_TEXT_APIS)
inline bool AXCoreObject::emitsNewline() const
{
    auto behavior = textEmissionBehavior();
    return behavior == TextEmissionBehavior::Newline || behavior == TextEmissionBehavior::DoubleNewline;
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

namespace Accessibility {

template<typename T, typename MatchFunctionT, typename StopFunctionT>
T* findAncestor(const T& object, bool includeSelf, const MatchFunctionT& matches, const StopFunctionT& shouldStop)
{
    RefPtr<T> current;
    if (includeSelf)
        current = const_cast<T*>(&object);
    else
        current = object.parentObject();

    for (; current; current = current->parentObject()) {
        if (shouldStop(*current))
            return nullptr;

        if (matches(*current))
            return current.get();
    }
    return nullptr;
}

template<typename T, typename MatchFunctionT>
T* findAncestor(const T& object, bool includeSelf, const MatchFunctionT& matches)
{
    return findAncestor(object, includeSelf, matches, [] (const auto&) {
        return false;
    });
}

template<typename T>
T* focusableAncestor(T& startObject)
{
    return findAncestor<T>(startObject, false, [] (const auto& ancestor) {
        return ancestor.canSetFocusAttribute();
    });
}

template<typename T>
T* clickableSelfOrAncestor(const T& startObject, ClickHandlerFilter filter)
{
    if (filter == ClickHandlerFilter::IncludeBody) {
        return clickableSelfOrAncestor<T>(startObject, [] (const T&) {
            return false;
        });
    }

    return clickableSelfOrAncestor<T>(startObject, [] (const T& ancestor) {
        // Stop iterating if we get to the <body>.
        return ancestor.hasBodyTag();
    });
}

template<typename T, typename F>
T* clickableSelfOrAncestor(const T& startObject, const F& shouldStop)
{
    RefPtr<T> ancestor = findAncestor<T>(startObject, true, [](const auto& ancestor) {
        return ancestor.hasClickHandler();
    }, shouldStop);

    // Presentational objects should not be allowed to be clicked.
    if (ancestor && ancestor->role() == AccessibilityRole::Presentational)
        return nullptr;
    return ancestor.get();
}

template<typename T>
T* editableAncestor(const T& startObject)
{
    return findAncestor<T>(startObject, false, [] (const auto& ancestor) {
        return ancestor.isTextControl() || ancestor.isEditableWebArea();
    });
}

template<typename T>
T* highestEditableAncestor(T& startObject)
{
    RefPtr<T> editableAncestor = startObject.editableAncestor();
    RefPtr<T> previousEditableAncestor;
    while (editableAncestor) {
        if (editableAncestor == previousEditableAncestor) {
            if (RefPtr<T> parent = editableAncestor->parentObject()) {
                editableAncestor = parent->editableAncestor();
                continue;
            }
            break;
        }
        previousEditableAncestor = editableAncestor;
        editableAncestor = editableAncestor->editableAncestor();
    }
    return previousEditableAncestor.get();
}

template<typename T>
T* findRelatedObjectInAncestry(const T& object, AXRelation relation, const T& descendant)
{
    auto relatedObjects = object.relatedObjects(relation);
    for (const auto& relatedObject : relatedObjects) {
        RefPtr ancestor = findAncestor(descendant, false, [&relatedObject](const auto& ancestor) {
            return relatedObject.get() == &ancestor;
        });
        if (ancestor)
            return ancestor.get();
    }
    return nullptr;
}

template<typename T>
T* liveRegionAncestor(const T& object, bool excludeIfOff)
{
    return findAncestor<T>(object, true, [excludeIfOff] (const T& object) {
        return object.supportsLiveRegion(excludeIfOff);
    });
}

template<typename T>
T* exposedTableAncestor(const T& object, bool includeSelf = false)
{
    return findAncestor<T>(object, includeSelf, [] (const T& object) {
        return object.isExposableTable();
    });
}

template<typename T, typename F>
AXCoreObject* findUnignoredDescendant(T& object, bool includeSelf, const F& matches)
{
    if (includeSelf && matches(object) && !object.isIgnored())
        return &object;

    for (Ref child : object.childrenIncludingIgnored()) {
        if (RefPtr descendant = findUnignoredDescendant(child.get(), /* includeSelf */ true, matches))
            return descendant.get();
    }
    return nullptr;
}

template<typename T, typename F>
T* findUnignoredChild(T& object, F&& matches)
{
    for (auto child : object.unignoredChildren()) {
        if (matches(child))
            return downcast<T>(child.ptr());
    }
    return nullptr;
}

template<typename T, typename F>
void enumerateAncestors(const T& object, bool includeSelf, const F& lambda)
{
    if (includeSelf)
        lambda(object);

    if (auto* parent = object.parentObject())
        enumerateAncestors(*parent, true, lambda);
}

template<typename T, typename F>
void enumerateDescendantsIncludingIgnored(T& object, bool includeSelf, const F& lambda)
{
    if (includeSelf)
        lambda(object);

    for (const auto& child : object.childrenIncludingIgnored())
        enumerateDescendantsIncludingIgnored(child.get(), true, lambda);
}

template<typename T, typename F>
void enumerateUnignoredDescendants(T& object, bool includeSelf, const F& lambda)
{
    if (includeSelf)
        lambda(object);

    // We have a reference to unignored children here, so it's possible that it will change when enumerating the unignored
    // descendants, so copying here ensures they don't change.
    auto children = object.unignoredChildren();
    for (const auto& child : children)
        enumerateUnignoredDescendants(child.get(), true, lambda);
}

template<typename U> inline void performFunctionOnMainThreadAndWait(U&& lambda)
{
    callOnMainThreadAndWait([lambda = std::forward<U>(lambda)] () {
        lambda();
    });
}

template<typename U> inline void performFunctionOnMainThread(U&& lambda)
{
    ensureOnMainThread([lambda = std::forward<U>(lambda)] () mutable {
        lambda();
    });
}

template<typename T, typename U> inline T retrieveValueFromMainThread(U&& lambda)
{
    std::optional<T> value;
    callOnMainThreadAndWait([&value, &lambda] {
        value = lambda();
    });
    return *value;
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

bool inRenderTreeOrStyleUpdate(const Document&);

using PlatformRoleMap = HashMap<AccessibilityRole, String, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

void initializeRoleMap();
PlatformRoleMap createPlatformRoleMap();
String roleToPlatformString(AccessibilityRole);
#if ENABLE(AX_THREAD_TEXT_APIS)
std::optional<AXTextMarkerRange> markerRangeFrom(NSRange, const AXCoreObject&);
#endif
Color defaultColor();

// Intended to work with size-types (like IntSize) or rect-types (like LayoutRect).
template <typename SizeOrRectType>
void adjustControlSize(SizeOrRectType& sizeOrRect)
{
    // It's very common for web developers to have a "screenreader only" CSS class that makes important
    // elements like controls unrendered (e.g. via opacity:0 or CSS clipping) with a width and height
    // of 1px. VoiceOver on macOS won't draw a cursor for 1px-large elements, so enforce a minimum size of 2px.
    if (sizeOrRect.width() < 2)
        sizeOrRect.setWidth(2);
    if (sizeOrRect.height() < 2)
        sizeOrRect.setHeight(2);
}

} // namespace Accessibility

inline bool AXCoreObject::isDescendantOfObject(const AXCoreObject& axObject) const
{
    return Accessibility::findAncestor<AXCoreObject>(*this, false, [&axObject] (const AXCoreObject& object) {
        return &object == &axObject;
    }) != nullptr;
}

inline bool AXCoreObject::isAncestorOfObject(const AXCoreObject& axObject) const
{
    return this == &axObject || axObject.isDescendantOfObject(*this);
}

inline AXCoreObject* AXCoreObject::axScrollView() const
{
    return Accessibility::findAncestor(*this, true, [] (const auto& ancestor) {
        return ancestor.isScrollView();
    });
}

// Logging helpers.
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilityRole);
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilitySearchDirection);
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilityObjectInclusion);
WTF::TextStream& operator<<(WTF::TextStream&, const AXCoreObject&);
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilityText);
WTF::TextStream& operator<<(WTF::TextStream&, AccessibilityTextSource);
WTF::TextStream& operator<<(WTF::TextStream&, AXRelation);
WTF::TextStream& operator<<(WTF::TextStream&, const TextUnderElementMode&);

} // namespace WebCore
