/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatQuad.h"
#include "HTMLTextFormControlElement.h"
#include "LayoutRect.h"
#include "Path.h"
#include "Range.h"
#include "TextIteratorBehavior.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#elif PLATFORM(WIN)
#include "AccessibilityObjectWrapperWin.h"
#include "COMPtr.h"
#endif

#if PLATFORM(COCOA)

typedef struct _NSRange NSRange;

OBJC_CLASS NSArray;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSData;
OBJC_CLASS NSMutableAttributedString;
OBJC_CLASS NSString;
OBJC_CLASS NSValue;
OBJC_CLASS NSView;
OBJC_CLASS WebAccessibilityObjectWrapper;

typedef WebAccessibilityObjectWrapper AccessibilityObjectWrapper;

#elif PLATFORM(GTK)
typedef struct _AtkObject AtkObject;
typedef struct _AtkObject AccessibilityObjectWrapper;
#elif PLATFORM(WPE)
class AccessibilityObjectWrapper : public RefCounted<AccessibilityObjectWrapper> { };
#else
class AccessibilityObjectWrapper;
#endif

namespace WebCore {

class AccessibilityObject;
class AXObjectCache;
class Element;
class Frame;
class FrameView;
class IntPoint;
class IntSize;
class Node;
class Page;
class RenderObject;
class ScrollableArea;
class ScrollView;
class Widget;

typedef unsigned AXID;

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
    DescriptionList,
    DescriptionListTerm,
    DescriptionListDetail,
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
    Summary,
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
    TextArea,
    TextGroup,
    Term,
    Time,
    Tree,
    TreeGrid,
    TreeItem,
    TextField,
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

enum class AccessibilityEventType {
    ContextMenu,
    Click,
    Decrement,
    Dismiss,
    Focus,
    Increment,
    ScrollIntoView,
    Select,
};
    
struct AccessibilityText {
    String text;
    AccessibilityTextSource textSource;
    Vector<RefPtr<AccessibilityObject>> textElements;
    
    AccessibilityText(const String& t, const AccessibilityTextSource& s)
        : text(t)
        , textSource(s)
    { }

    AccessibilityText(const String& t, const AccessibilityTextSource& s, Vector<RefPtr<AccessibilityObject>> elements)
        : text(t)
        , textSource(s)
        , textElements(WTFMove(elements))
    { }

    AccessibilityText(const String& t, const AccessibilityTextSource& s, RefPtr<AccessibilityObject>&& element)
        : text(t)
        , textSource(s)
    {
        textElements.append(WTFMove(element));
    }
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

// Use this struct to store the isIgnored data that depends on the parents, so that in addChildren()
// we avoid going up the parent chain for each element while traversing the tree with useful information already.
struct AccessibilityIsIgnoredFromParentData {
    AccessibilityObject* parent { nullptr };
    bool isAXHidden { false };
    bool isPresentationalChildOfAriaRole { false };
    bool isDescendantOfBarrenParent { false };
    
    AccessibilityIsIgnoredFromParentData(AccessibilityObject* parent = nullptr)
        : parent(parent)
    { }

    bool isNull() const { return !parent; }
};
    
enum class AccessibilityOrientation {
    Vertical,
    Horizontal,
    Undefined,
};
    
enum class AccessibilityObjectInclusion {
    IncludeObject,
    IgnoreObject,
    DefaultBehavior,
};
    
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

enum class AccessibilitySearchDirection {
    Next = 1,
    Previous,
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

enum class AccessibilityVisiblePositionForBounds {
    First,
    Last,
};

struct AccessibilitySearchCriteria {
    AccessibilityObject* startObject;
    AccessibilitySearchDirection searchDirection;
    Vector<AccessibilitySearchKey> searchKeys;
    String searchText;
    unsigned resultsLimit;
    bool visibleOnly;
    bool immediateDescendantsOnly;
    
    AccessibilitySearchCriteria(AccessibilityObject* startObject, AccessibilitySearchDirection searchDirection, String searchText, unsigned resultsLimit, bool visibleOnly, bool immediateDescendantsOnly)
        : startObject(startObject)
        , searchDirection(searchDirection)
        , searchText(searchText)
        , resultsLimit(resultsLimit)
        , visibleOnly(visibleOnly)
        , immediateDescendantsOnly(immediateDescendantsOnly)
    { }
};
    
enum class AccessibilityDetachmentType { CacheDestroyed, ElementDestroyed };

struct VisiblePositionRange {

    VisiblePosition start;
    VisiblePosition end;

    VisiblePositionRange() {}

    VisiblePositionRange(const VisiblePosition& s, const VisiblePosition& e)
        : start(s)
        , end(e)
    { }

    VisiblePositionRange(const VisibleSelection& selection)
        : start(selection.start())
        , end(selection.end())
    { }

    bool isNull() const { return start.isNull() || end.isNull(); }
};

struct PlainTextRange {
        
    unsigned start { 0 };
    unsigned length { 0 };
    
    PlainTextRange() = default;
    
    PlainTextRange(unsigned s, unsigned l)
        : start(s)
        , length(l)
    { }
    
    bool isNull() const { return !start && !length; }
};

enum class AccessibilitySelectTextActivity {
    FindAndReplace,
    FindAndSelect,
    FindAndCapitalize,
    FindAndLowercase,
    FindAndUppercase
};

enum class AccessibilitySelectTextAmbiguityResolution {
    ClosestAfter,
    ClosestBefore,
    ClosestTo
};

struct AccessibilitySelectTextCriteria {
    AccessibilitySelectTextActivity activity;
    AccessibilitySelectTextAmbiguityResolution ambiguityResolution;
    String replacementString;
    Vector<String> searchStrings;
    
    AccessibilitySelectTextCriteria(AccessibilitySelectTextActivity activity, AccessibilitySelectTextAmbiguityResolution ambiguityResolution, const String& replacementString)
        : activity(activity)
        , ambiguityResolution(ambiguityResolution)
        , replacementString(replacementString)
    { }
};

enum class AccessibilityMathScriptObjectType { Subscript, Superscript };
enum class AccessibilityMathMultiscriptObjectType { PreSubscript, PreSuperscript, PostSubscript, PostSuperscript };

enum class AccessibilityCurrentState { False, True, Page, Step, Location, Date, Time };
    
bool nodeHasPresentationRole(Node*);
    
class AccessibilityObject : public RefCounted<AccessibilityObject> {
protected:
    AccessibilityObject() = default;
    
public:
    virtual ~AccessibilityObject();

    // After constructing an AccessibilityObject, it must be given a
    // unique ID, then added to AXObjectCache, and finally init() must
    // be called last.
    void setAXObjectID(AXID axObjectID) { m_id = axObjectID; }
    virtual void init() { }

    // When the corresponding WebCore object that this AccessibilityObject
    // wraps is deleted, it must be detached.
    virtual void detach(AccessibilityDetachmentType, AXObjectCache* cache = nullptr);
    virtual bool isDetached() const;

    typedef Vector<RefPtr<AccessibilityObject>> AccessibilityChildrenVector;
    
    virtual bool isAccessibilityNodeObject() const { return false; }    
    virtual bool isAccessibilityRenderObject() const { return false; }
    virtual bool isAccessibilityScrollbar() const { return false; }
    virtual bool isAccessibilityScrollView() const { return false; }
    virtual bool isAccessibilitySVGRoot() const { return false; }
    virtual bool isAccessibilitySVGElement() const { return false; }

    bool accessibilityObjectContainsText(String *) const;

    virtual bool isAttachmentElement() const { return false; }
    virtual bool isHeading() const { return false; }
    virtual bool isLink() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isImageMap() const { return roleValue() == AccessibilityRole::ImageMap; }
    virtual bool isNativeImage() const { return false; }
    virtual bool isImageButton() const { return false; }
    virtual bool isPasswordField() const { return false; }
    bool isContainedByPasswordField() const;
    virtual AccessibilityObject* passwordFieldOrContainingPasswordField() { return nullptr; }
    virtual bool isNativeTextControl() const { return false; }
    virtual bool isSearchField() const { return false; }
    bool isWebArea() const { return roleValue() == AccessibilityRole::WebArea; }
    virtual bool isCheckbox() const { return roleValue() == AccessibilityRole::CheckBox; }
    virtual bool isRadioButton() const { return roleValue() == AccessibilityRole::RadioButton; }
    virtual bool isNativeListBox() const { return false; }
    bool isListBox() const { return roleValue() == AccessibilityRole::ListBox; }
    virtual bool isListBoxOption() const { return false; }
    virtual bool isAttachment() const { return false; }
    virtual bool isMediaTimeline() const { return false; }
    virtual bool isMenuRelated() const { return false; }
    virtual bool isMenu() const { return false; }
    virtual bool isMenuBar() const { return false; }
    virtual bool isMenuButton() const { return false; }
    virtual bool isMenuItem() const { return false; }
    virtual bool isFileUploadButton() const { return false; }
    virtual bool isInputImage() const { return false; }
    virtual bool isProgressIndicator() const { return false; }
    virtual bool isSlider() const { return false; }
    virtual bool isSliderThumb() const { return false; }
    virtual bool isInputSlider() const { return false; }
    virtual bool isControl() const { return false; }
    virtual bool isLabel() const { return false; }
    virtual bool isList() const { return false; }
    virtual bool isTable() const { return false; }
    virtual bool isDataTable() const { return false; }
    virtual bool isTableRow() const { return false; }
    virtual bool isTableColumn() const { return false; }
    virtual bool isTableCell() const { return false; }
    virtual bool isFieldset() const { return false; }
    virtual bool isGroup() const { return false; }
    virtual bool isARIATreeGridRow() const { return false; }
    virtual bool isImageMapLink() const { return false; }
    virtual bool isMenuList() const { return false; }
    virtual bool isMenuListPopup() const { return false; }
    virtual bool isMenuListOption() const { return false; }
    virtual bool isSpinButton() const { return roleValue() == AccessibilityRole::SpinButton; }
    virtual bool isNativeSpinButton() const { return false; }
    virtual bool isSpinButtonPart() const { return false; }
    virtual bool isMockObject() const { return false; }
    virtual bool isMediaControlLabel() const { return false; }
    virtual bool isMediaObject() const { return false; }
    bool isSwitch() const { return roleValue() == AccessibilityRole::Switch; }
    bool isToggleButton() const { return roleValue() == AccessibilityRole::ToggleButton; }
    bool isTextControl() const;
    bool isARIATextControl() const;
    bool isNonNativeTextControl() const;
    bool isTabList() const { return roleValue() == AccessibilityRole::TabList; }
    bool isTabItem() const { return roleValue() == AccessibilityRole::Tab; }
    bool isRadioGroup() const { return roleValue() == AccessibilityRole::RadioGroup; }
    bool isComboBox() const { return roleValue() == AccessibilityRole::ComboBox; }
    bool isTree() const { return roleValue() == AccessibilityRole::Tree; }
    bool isTreeGrid() const { return roleValue() == AccessibilityRole::TreeGrid; }
    bool isTreeItem() const { return roleValue() == AccessibilityRole::TreeItem; }
    bool isScrollbar() const { return roleValue() == AccessibilityRole::ScrollBar; }
    bool isButton() const;
    bool isListItem() const { return roleValue() == AccessibilityRole::ListItem; }
    bool isCheckboxOrRadio() const { return isCheckbox() || isRadioButton(); }
    bool isScrollView() const { return roleValue() == AccessibilityRole::ScrollArea; }
    bool isCanvas() const { return roleValue() == AccessibilityRole::Canvas; }
    bool isPopUpButton() const { return roleValue() == AccessibilityRole::PopUpButton; }
    bool isBlockquote() const;
    bool isLandmark() const;
    bool isColorWell() const { return roleValue() == AccessibilityRole::ColorWell; }
    bool isRangeControl() const;
    bool isMeter() const;
    bool isSplitter() const { return roleValue() == AccessibilityRole::Splitter; }
    bool isToolbar() const { return roleValue() == AccessibilityRole::Toolbar; }
    bool isStyleFormatGroup() const;
    bool isSubscriptStyleGroup() const;
    bool isSuperscriptStyleGroup() const;
    bool isFigureElement() const;
    bool isSummary() const { return roleValue() == AccessibilityRole::Summary; }
    bool isOutput() const;
    
    virtual bool isChecked() const { return false; }
    virtual bool isEnabled() const { return false; }
    virtual bool isSelected() const { return false; }
    virtual bool isFocused() const { return false; }
    virtual bool isHovered() const { return false; }
    virtual bool isIndeterminate() const { return false; }
    virtual bool isLoaded() const { return false; }
    virtual bool isMultiSelectable() const { return false; }
    virtual bool isOffScreen() const { return false; }
    virtual bool isPressed() const { return false; }
    virtual bool isUnvisited() const { return false; }
    virtual bool isVisited() const { return false; }
    virtual bool isRequired() const { return false; }
    virtual bool supportsRequiredAttribute() const { return false; }
    virtual bool isLinked() const { return false; }
    virtual bool isExpanded() const;
    virtual bool isVisible() const { return true; }
    virtual bool isCollapsed() const { return false; }
    virtual void setIsExpanded(bool) { }

    // In a multi-select list, many items can be selected but only one is active at a time.
    virtual bool isSelectedOptionActive() const { return false; }

    virtual bool hasBoldFont() const { return false; }
    virtual bool hasItalicFont() const { return false; }
    bool hasMisspelling() const;
    virtual bool hasPlainText() const { return false; }
    virtual bool hasSameFont(RenderObject*) const { return false; }
    virtual bool hasSameFontColor(RenderObject*) const { return false; }
    virtual bool hasSameStyle(RenderObject*) const { return false; }
    bool isStaticText() const { return roleValue() == AccessibilityRole::StaticText; }
    virtual bool hasUnderline() const { return false; }
    bool hasHighlighting() const;

    bool supportsDatetimeAttribute() const;
    const AtomicString& datetimeAttributeValue() const;
    
    virtual bool canSetFocusAttribute() const { return false; }
    virtual bool canSetTextRangeAttributes() const { return false; }
    virtual bool canSetValueAttribute() const { return false; }
    virtual bool canSetNumericValue() const { return false; }
    virtual bool canSetSelectedAttribute() const { return false; }
    virtual bool canSetSelectedChildrenAttribute() const { return false; }
    virtual bool canSetExpandedAttribute() const { return false; }
    
    virtual Element* element() const;
    virtual Node* node() const { return nullptr; }
    virtual RenderObject* renderer() const { return nullptr; }
    virtual bool accessibilityIsIgnored() const;
    virtual AccessibilityObjectInclusion defaultObjectInclusion() const;
    bool accessibilityIsIgnoredByDefault() const;
    
    bool isShowingValidationMessage() const;
    String validationMessage() const;
    
    unsigned blockquoteLevel() const;
    virtual int headingLevel() const { return 0; }
    virtual int tableLevel() const { return 0; }
    virtual AccessibilityButtonState checkboxOrRadioValue() const;
    virtual String valueDescription() const { return String(); }
    virtual float valueForRange() const { return 0.0f; }
    virtual float maxValueForRange() const { return 0.0f; }
    virtual float minValueForRange() const { return 0.0f; }
    virtual float stepValueForRange() const { return 0.0f; }
    virtual AccessibilityObject* selectedRadioButton() { return nullptr; }
    virtual AccessibilityObject* selectedTabItem() { return nullptr; }
    AccessibilityObject* selectedListItem();
    virtual int layoutCount() const { return 0; }
    virtual double estimatedLoadingProgress() const { return 0; }
    static bool isARIAControl(AccessibilityRole);
    static bool isARIAInput(AccessibilityRole);

    virtual bool supportsARIAOwns() const { return false; }
    bool isActiveDescendantOfFocusedContainer() const;
    void ariaActiveDescendantReferencingElements(AccessibilityChildrenVector&) const;
    void ariaControlsElements(AccessibilityChildrenVector&) const;
    void ariaControlsReferencingElements(AccessibilityChildrenVector&) const;
    void ariaDescribedByElements(AccessibilityChildrenVector&) const;
    void ariaDescribedByReferencingElements(AccessibilityChildrenVector&) const;
    void ariaDetailsElements(AccessibilityChildrenVector&) const;
    void ariaDetailsReferencingElements(AccessibilityChildrenVector&) const;
    void ariaErrorMessageElements(AccessibilityChildrenVector&) const;
    void ariaErrorMessageReferencingElements(AccessibilityChildrenVector&) const;
    void ariaFlowToElements(AccessibilityChildrenVector&) const;
    void ariaFlowToReferencingElements(AccessibilityChildrenVector&) const;
    void ariaLabelledByElements(AccessibilityChildrenVector&) const;
    void ariaLabelledByReferencingElements(AccessibilityChildrenVector&) const;
    void ariaOwnsElements(AccessibilityChildrenVector&) const;
    void ariaOwnsReferencingElements(AccessibilityChildrenVector&) const;

    virtual bool hasPopup() const { return false; }
    String hasPopupValue() const;
    bool supportsHasPopup() const;
    bool pressedIsPresent() const;
    bool ariaIsMultiline() const;
    String invalidStatus() const;
    bool supportsPressed() const;
    bool supportsExpanded() const;
    bool supportsChecked() const;
    AccessibilitySortDirection sortDirection() const;
    virtual bool canvasHasFallbackContent() const { return false; }
    bool supportsRangeValue() const;
    const AtomicString& identifierAttribute() const;
    const AtomicString& linkRelValue() const;
    void classList(Vector<String>&) const;
    virtual String roleDescription() const;
    AccessibilityCurrentState currentState() const;
    String currentValue() const;
    bool supportsCurrent() const;
    const String keyShortcutsValue() const;
    
    // This function checks if the object should be ignored when there's a modal dialog displayed.
    bool ignoredFromModalPresence() const;
    bool isModalDescendant(Node*) const;
    bool isModalNode() const;
    
    bool supportsSetSize() const;
    bool supportsPosInSet() const;
    int setSize() const;
    int posInSet() const;
    
    // ARIA drag and drop
    virtual bool supportsARIADropping() const { return false; }
    virtual bool supportsARIADragging() const { return false; }
    virtual bool isARIAGrabbed() { return false; }
    virtual void setARIAGrabbed(bool) { }
    virtual Vector<String> determineARIADropEffects() { return { }; }
    
    // Called on the root AX object to return the deepest available element.
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const { return nullptr; }
    // Called on the AX object after the render tree determines which is the right AccessibilityRenderObject.
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const;

    virtual AccessibilityObject* focusedUIElement() const;

    virtual AccessibilityObject* firstChild() const { return nullptr; }
    virtual AccessibilityObject* lastChild() const { return nullptr; }
    virtual AccessibilityObject* previousSibling() const { return nullptr; }
    virtual AccessibilityObject* nextSibling() const { return nullptr; }
    virtual AccessibilityObject* nextSiblingUnignored(int limit) const;
    virtual AccessibilityObject* previousSiblingUnignored(int limit) const;
    virtual AccessibilityObject* parentObject() const = 0;
    virtual AccessibilityObject* parentObjectUnignored() const;
    virtual AccessibilityObject* parentObjectIfExists() const { return nullptr; }
    static AccessibilityObject* firstAccessibleObjectFromNode(const Node*);
    void findMatchingObjects(AccessibilitySearchCriteria*, AccessibilityChildrenVector&);
    virtual bool isDescendantOfBarrenParent() const { return false; }

    bool isDescendantOfRole(AccessibilityRole) const;
    
    // Text selection
    RefPtr<Range> rangeOfStringClosestToRangeInDirection(Range*, AccessibilitySearchDirection, Vector<String>&) const;
    RefPtr<Range> selectionRange() const;
    String selectText(AccessibilitySelectTextCriteria*);
    
    virtual AccessibilityObject* observableObject() const { return nullptr; }
    virtual void linkedUIElements(AccessibilityChildrenVector&) const { }
    virtual AccessibilityObject* titleUIElement() const { return nullptr; }
    virtual bool exposesTitleUIElement() const { return true; }
    virtual AccessibilityObject* correspondingLabelForControlElement() const { return nullptr; }
    virtual AccessibilityObject* correspondingControlForLabelElement() const { return nullptr; }
    virtual AccessibilityObject* scrollBar(AccessibilityOrientation) { return nullptr; }
    
    virtual AccessibilityRole ariaRoleAttribute() const { return AccessibilityRole::Unknown; }
    virtual bool isPresentationalChildOfAriaRole() const { return false; }
    virtual bool ariaRoleHasPresentationalChildren() const { return false; }
    virtual bool inheritsPresentationalRole() const { return false; }

    // Accessibility Text
    virtual void accessibilityText(Vector<AccessibilityText>&) { };
    // A single method for getting a computed label for an AXObject. It condenses the nuances of accessibilityText. Used by Inspector.
    String computedLabel();
    
    // A programmatic way to set a name on an AccessibleObject.
    virtual void setAccessibleName(const AtomicString&) { }
    virtual bool hasAttributesRequiredForInclusion() const;

    // Accessibility Text - (To be deprecated).
    virtual String accessibilityDescription() const { return String(); }
    virtual String title() const { return String(); }
    virtual String helpText() const { return String(); }

    // Methods for determining accessibility text.
    bool isARIAStaticText() const { return ariaRoleAttribute() == AccessibilityRole::StaticText; }
    virtual String stringValue() const { return String(); }
    virtual String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const { return String(); }
    virtual String text() const { return String(); }
    virtual int textLength() const { return 0; }
    virtual String ariaLabeledByAttribute() const { return String(); }
    virtual String ariaDescribedByAttribute() const { return String(); }
    const String placeholderValue() const;
    bool accessibleNameDerivesFromContent() const;
    
    // Abbreviations
    virtual String expandedTextValue() const { return String(); }
    virtual bool supportsExpandedTextValue() const { return false; }
    
    void elementsFromAttribute(Vector<Element*>&, const QualifiedName&) const;

    // Only if isColorWell()
    virtual void colorValue(int& r, int& g, int& b) const { r = 0; g = 0; b = 0; }

    virtual AccessibilityRole roleValue() const { return m_role; }

    virtual AXObjectCache* axObjectCache() const;
    AXID axObjectID() const { return m_id; }
    
    static AccessibilityObject* anchorElementForNode(Node*);
    static AccessibilityObject* headingElementForNode(Node*);
    virtual Element* anchorElement() const { return nullptr; }
    bool supportsPressAction() const;
    virtual Element* actionElement() const { return nullptr; }
    virtual LayoutRect boundingBoxRect() const { return LayoutRect(); }
    IntRect pixelSnappedBoundingBoxRect() const { return snappedIntRect(boundingBoxRect()); }
    virtual LayoutRect elementRect() const = 0;
    LayoutSize size() const { return elementRect().size(); }
    virtual IntPoint clickPoint();
    static IntRect boundingBoxForQuads(RenderObject*, const Vector<FloatQuad>&);
    virtual Path elementPath() const { return Path(); }
    virtual bool supportsPath() const { return false; }
    
    TextIteratorBehavior textIteratorBehaviorForTextRange() const;
    virtual PlainTextRange selectedTextRange() const { return PlainTextRange(); }
    unsigned selectionStart() const { return selectedTextRange().start; }
    unsigned selectionEnd() const { return selectedTextRange().length; }
    
    virtual URL url() const { return URL(); }
    virtual VisibleSelection selection() const { return VisibleSelection(); }
    virtual String selectedText() const { return String(); }
    virtual const AtomicString& accessKey() const { return nullAtom(); }
    const String& actionVerb() const;
    virtual Widget* widget() const { return nullptr; }
    virtual Widget* widgetForAttachmentView() const { return nullptr; }
    Page* page() const;
    virtual Document* document() const;
    virtual FrameView* documentFrameView() const;
    Frame* frame() const;
    Frame* mainFrame() const;
    Document* topDocument() const;
    ScrollView* scrollViewAncestor() const;
    String language() const;
    // 1-based, to match the aria-level spec.
    virtual unsigned hierarchicalLevel() const { return 0; }
    
    virtual void setFocused(bool) { }
    virtual void setSelectedText(const String&) { }
    virtual void setSelectedTextRange(const PlainTextRange&) { }
    virtual void setValue(const String&) { }
    virtual void setValue(float) { }
    virtual void setSelected(bool) { }
    virtual void setSelectedRows(AccessibilityChildrenVector&) { }
    
    virtual void makeRangeVisible(const PlainTextRange&) { }
    virtual bool press();
    bool performDefaultAction() { return press(); }
    
    virtual AccessibilityOrientation orientation() const;
    virtual void increment() { }
    virtual void decrement() { }

    virtual void childrenChanged() { }
    virtual void textChanged() { }
    virtual void updateAccessibilityRole() { }
    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true);
    virtual void addChildren() { }
    virtual void addChild(AccessibilityObject*);
    virtual void insertChild(AccessibilityObject*, unsigned);

    virtual bool shouldIgnoreAttributeRole() const { return false; }
    
    virtual bool canHaveChildren() const { return true; }
    virtual bool hasChildren() const { return m_haveChildren; }
    virtual void updateChildrenIfNecessary();
    virtual void setNeedsToUpdateChildren() { }
    virtual void setNeedsToUpdateSubtree() { }
    virtual void clearChildren();
    virtual bool needsToUpdateChildren() const { return false; }
#if PLATFORM(COCOA)
    virtual void detachFromParent();
#else
    virtual void detachFromParent() { }
#endif
    virtual bool isDetachedFromParent() { return false; }

    virtual bool canHaveSelectedChildren() const { return false; }
    virtual void selectedChildren(AccessibilityChildrenVector&) { }
    virtual void visibleChildren(AccessibilityChildrenVector&) { }
    virtual void tabChildren(AccessibilityChildrenVector&) { }
    virtual bool shouldFocusActiveDescendant() const { return false; }
    virtual AccessibilityObject* activeDescendant() const { return nullptr; }
    virtual void handleActiveDescendantChanged() { }
    virtual void handleAriaExpandedChanged() { }
    bool isDescendantOfObject(const AccessibilityObject*) const;
    bool isAncestorOfObject(const AccessibilityObject*) const;
    AccessibilityObject* firstAnonymousBlockChild() const;

    static AccessibilityRole ariaRoleToWebCoreRole(const String&);
    bool hasAttribute(const QualifiedName&) const;
    const AtomicString& getAttribute(const QualifiedName&) const;
    bool hasTagName(const QualifiedName&) const;
    
    bool shouldDispatchAccessibilityEvent() const;
    bool dispatchAccessibilityEvent(Event&) const;
    bool dispatchAccessibilityEventWithType(AccessibilityEventType) const;
    bool dispatchAccessibleSetValueEvent(const String&) const;

    virtual VisiblePositionRange visiblePositionRange() const { return VisiblePositionRange(); }
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const { return VisiblePositionRange(); }
    
    RefPtr<Range> elementRange() const;
    static bool replacedNodeNeedsCharacter(Node* replacedNode);
    
    VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const;
    VisiblePositionRange positionOfLeftWord(const VisiblePosition&) const;
    VisiblePositionRange positionOfRightWord(const VisiblePosition&) const;
    VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const;
    VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const;
    VisiblePositionRange sentenceForPosition(const VisiblePosition&) const;
    VisiblePositionRange paragraphForPosition(const VisiblePosition&) const;
    VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const;
    VisiblePositionRange visiblePositionRangeForRange(const PlainTextRange&) const;
    VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const;
    
    RefPtr<Range> rangeForPlainTextRange(const PlainTextRange&) const;

    static String stringForVisiblePositionRange(const VisiblePositionRange&);
    String stringForRange(RefPtr<Range>) const;
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const { return IntRect(); }
    virtual IntRect boundsForRange(const RefPtr<Range>) const { return IntRect(); }
    int lengthForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const { }

    VisiblePosition visiblePositionForBounds(const IntRect&, AccessibilityVisiblePositionForBounds) const;
    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const { return VisiblePosition(); }
    VisiblePosition nextVisiblePosition(const VisiblePosition& visiblePos) const { return visiblePos.next(); }
    VisiblePosition previousVisiblePosition(const VisiblePosition& visiblePos) const { return visiblePos.previous(); }
    VisiblePosition nextWordEnd(const VisiblePosition&) const;
    VisiblePosition previousWordStart(const VisiblePosition&) const;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const;
    VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const;
    VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const;
    VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const;
    VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const;
    virtual VisiblePosition visiblePositionForIndex(unsigned, bool /*lastIndexOK */) const { return VisiblePosition(); }
    
    virtual VisiblePosition visiblePositionForIndex(int) const { return VisiblePosition(); }
    virtual int indexForVisiblePosition(const VisiblePosition&) const { return 0; }

    AccessibilityObject* accessibilityObjectForPosition(const VisiblePosition&) const;
    int lineForPosition(const VisiblePosition&) const;
    PlainTextRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual int index(const VisiblePosition&) const { return -1; }

    virtual void lineBreaks(Vector<int>&) const { }
    virtual PlainTextRange doAXRangeForLine(unsigned) const { return PlainTextRange(); }
    PlainTextRange doAXRangeForPosition(const IntPoint&) const;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const { return PlainTextRange(); }
    PlainTextRange doAXStyleRangeForIndex(unsigned) const;

    virtual String doAXStringForRange(const PlainTextRange&) const { return String(); }
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const { return IntRect(); }
    virtual IntRect doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange&) const { return IntRect(); }
    static String listMarkerTextForNodeAndPosition(Node*, const VisiblePosition&);

    unsigned doAXLineForIndex(unsigned);

    String computedRoleString() const;

    virtual String stringValueForMSAA() const { return String(); }
    virtual String stringRoleForMSAA() const { return String(); }
    virtual String nameForMSAA() const { return String(); }
    virtual String descriptionForMSAA() const { return String(); }
    virtual AccessibilityRole roleValueForMSAA() const { return roleValue(); }

    virtual String passwordFieldValue() const { return String(); }
    bool isValueAutofilled() const;
    bool isValueAutofillAvailable() const;
    AutoFillButtonType valueAutofillButtonType() const;
    
    // Used by an ARIA tree to get all its rows.
    void ariaTreeRows(AccessibilityChildrenVector&);
    // Used by an ARIA tree item to get all of its direct rows that it can disclose.
    void ariaTreeItemDisclosedRows(AccessibilityChildrenVector&);
    // Used by an ARIA tree item to get only its content, and not its child tree items and groups. 
    void ariaTreeItemContent(AccessibilityChildrenVector&);
    
    // ARIA live-region features.
    bool supportsLiveRegion(bool excludeIfOff = true) const;
    bool isInsideLiveRegion(bool excludeIfOff = true) const;
    AccessibilityObject* liveRegionAncestor(bool excludeIfOff = true) const;
    virtual const String liveRegionStatus() const { return String(); }
    virtual const String liveRegionRelevant() const { return nullAtom(); }
    virtual bool liveRegionAtomic() const { return false; }
    virtual bool isBusy() const { return false; }
    static const String defaultLiveRegionStatusForRole(AccessibilityRole);
    static bool liveRegionStatusIsEnabled(const AtomicString&);
    static bool contentEditableAttributeIsEnabled(Element*);
    bool hasContentEditableAttributeSet() const;

    bool supportsReadOnly() const;
    virtual String readOnlyValue() const;

    bool supportsAutoComplete() const;
    String autoCompleteValue() const;
    
    bool supportsARIAAttributes() const;
    
    // CSS3 Speech properties.
    virtual OptionSet<SpeakAs> speakAsProperty() const { return OptionSet<SpeakAs> { }; }

    // Make this object visible by scrolling as many nested scrollable views as needed.
    virtual void scrollToMakeVisible() const;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    virtual void scrollToMakeVisibleWithSubFocus(const IntRect&) const;
    // Scroll this object to a given point in global coordinates of the top-level window.
    virtual void scrollToGlobalPoint(const IntPoint&) const;
    
    enum class ScrollByPageDirection { Up, Down, Left, Right };
    bool scrollByPage(ScrollByPageDirection) const;
    IntPoint scrollPosition() const;
    IntSize scrollContentsSize() const;    
    IntRect scrollVisibleContentRect() const;
    
    bool lastKnownIsIgnoredValue();
    void setLastKnownIsIgnoredValue(bool);

    // Fires a children changed notification on the parent if the isIgnored value changed.
    void notifyIfIgnoredValueChanged();

    // All math elements return true for isMathElement().
    virtual bool isMathElement() const { return false; }
    virtual bool isMathFraction() const { return false; }
    virtual bool isMathFenced() const { return false; }
    virtual bool isMathSubscriptSuperscript() const { return false; }
    virtual bool isMathRow() const { return false; }
    virtual bool isMathUnderOver() const { return false; }
    virtual bool isMathRoot() const { return false; }
    virtual bool isMathSquareRoot() const { return false; }
    virtual bool isMathText() const { return false; }
    virtual bool isMathNumber() const { return false; }
    virtual bool isMathOperator() const { return false; }
    virtual bool isMathFenceOperator() const { return false; }
    virtual bool isMathSeparatorOperator() const { return false; }
    virtual bool isMathIdentifier() const { return false; }
    virtual bool isMathTable() const { return false; }
    virtual bool isMathTableRow() const { return false; }
    virtual bool isMathTableCell() const { return false; }
    virtual bool isMathMultiscript() const { return false; }
    virtual bool isMathToken() const { return false; }
    virtual bool isMathScriptObject(AccessibilityMathScriptObjectType) const { return false; }
    virtual bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const { return false; }

    // Root components.
    virtual AccessibilityObject* mathRadicandObject() { return nullptr; }
    virtual AccessibilityObject* mathRootIndexObject() { return nullptr; }
    
    // Under over components.
    virtual AccessibilityObject* mathUnderObject() { return nullptr; }
    virtual AccessibilityObject* mathOverObject() { return nullptr; }

    // Fraction components.
    virtual AccessibilityObject* mathNumeratorObject() { return nullptr; }
    virtual AccessibilityObject* mathDenominatorObject() { return nullptr; }

    // Subscript/superscript components.
    virtual AccessibilityObject* mathBaseObject() { return nullptr; }
    virtual AccessibilityObject* mathSubscriptObject() { return nullptr; }
    virtual AccessibilityObject* mathSuperscriptObject() { return nullptr; }
    
    // Fenced components.
    virtual String mathFencedOpenString() const { return String(); }
    virtual String mathFencedCloseString() const { return String(); }
    virtual int mathLineThickness() const { return 0; }
    virtual bool isAnonymousMathOperator() const { return false; }
    
    // Multiscripts components.
    typedef Vector<std::pair<AccessibilityObject*, AccessibilityObject*>> AccessibilityMathMultiscriptPairs;
    virtual void mathPrescripts(AccessibilityMathMultiscriptPairs&) { }
    virtual void mathPostscripts(AccessibilityMathMultiscriptPairs&) { }
    
    // Visibility.
    bool isAXHidden() const;
    bool isDOMHidden() const;
    bool isHidden() const { return isAXHidden() || isDOMHidden(); }
    
#if HAVE(ACCESSIBILITY)
#if PLATFORM(GTK)
    AccessibilityObjectWrapper* wrapper() const;
    void setWrapper(AccessibilityObjectWrapper*);
#else
    AccessibilityObjectWrapper* wrapper() const { return m_wrapper.get(); }
    void setWrapper(AccessibilityObjectWrapper* wrapper) 
    {
        m_wrapper = wrapper;
    }
#endif
#endif
    
#if PLATFORM(COCOA)
    void overrideAttachmentParent(AccessibilityObject* parent);
#else
    void overrideAttachmentParent(AccessibilityObject*) { }
#endif
    
#if HAVE(ACCESSIBILITY)
    // a platform-specific method for determining if an attachment is ignored
    bool accessibilityIgnoreAttachment() const;
    // gives platforms the opportunity to indicate if and how an object should be included
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const;
#else
    bool accessibilityIgnoreAttachment() const { return true; }
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const { return AccessibilityObjectInclusion::DefaultBehavior; }
#endif

#if PLATFORM(IOS_FAMILY)
    int accessibilityPasswordFieldLength();
    bool hasTouchEventListener() const;
    bool isInputTypePopupButton() const;
    bool hasAccessibleDismissEventListener() const;
#endif
    
    // allows for an AccessibilityObject to update its render tree or perform
    // other operations update type operations
    void updateBackingStore();
    
#if PLATFORM(COCOA)
    bool preventKeyboardDOMEventDispatch() const;
    void setPreventKeyboardDOMEventDispatch(bool);
#endif
    
#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
    bool caretBrowsingEnabled() const;
    void setCaretBrowsingEnabled(bool);
#endif

    AccessibilityObject* focusableAncestor();
    AccessibilityObject* editableAncestor();
    AccessibilityObject* highestEditableAncestor();
    
    static const AccessibilityObject* matchedParent(const AccessibilityObject&, bool includeSelf, const WTF::Function<bool(const AccessibilityObject&)>&);
    
    void clearIsIgnoredFromParentData() { m_isIgnoredFromParentData = AccessibilityIsIgnoredFromParentData(); }
    void setIsIgnoredFromParentDataForChild(AccessibilityObject*);
    
protected:
    AXID m_id { 0 };
    AccessibilityChildrenVector m_children;
    mutable bool m_haveChildren { false };
    AccessibilityRole m_role { AccessibilityRole::Unknown };
    AccessibilityObjectInclusion m_lastKnownIsIgnoredValue { AccessibilityObjectInclusion::DefaultBehavior };
    AccessibilityIsIgnoredFromParentData m_isIgnoredFromParentData { };
    bool m_childrenDirty { false };
    bool m_subtreeDirty { false };

    void setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData& data) { m_isIgnoredFromParentData = data; }

    virtual bool computeAccessibilityIsIgnored() const { return true; }

    // If this object itself scrolls, return its ScrollableArea.
    virtual ScrollableArea* getScrollableAreaIfScrollable() const { return nullptr; }
    virtual void scrollTo(const IntPoint&) const { }
    ScrollableArea* scrollableAreaAncestor() const;
    void scrollAreaAndAncestor(std::pair<ScrollableArea*, AccessibilityObject*>&) const;
    
    static bool isAccessibilityObjectSearchMatchAtIndex(AccessibilityObject*, AccessibilitySearchCriteria*, size_t);
    static bool isAccessibilityObjectSearchMatch(AccessibilityObject*, AccessibilitySearchCriteria*);
    static bool isAccessibilityTextSearchMatch(AccessibilityObject*, AccessibilitySearchCriteria*);
    static bool objectMatchesSearchCriteriaWithResultLimit(AccessibilityObject*, AccessibilitySearchCriteria*, AccessibilityChildrenVector&);
    virtual AccessibilityRole buttonRoleType() const;
    bool isOnscreen() const;
    bool dispatchTouchEvent();

    void ariaElementsFromAttribute(AccessibilityChildrenVector&, const QualifiedName&) const;
    void ariaElementsReferencedByAttribute(AccessibilityChildrenVector&, const QualifiedName&) const;

    AccessibilityObject* radioGroupAncestor() const;

#if PLATFORM(GTK) && HAVE(ACCESSIBILITY)
    bool allowsTextRanges() const;
    unsigned getLengthForTextRange() const;
#else
    bool allowsTextRanges() const { return isTextControl(); }
    unsigned getLengthForTextRange() const { return text().length(); }
#endif

#if PLATFORM(COCOA)
    RetainPtr<WebAccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(WIN)
    COMPtr<AccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(GTK)
    AtkObject* m_wrapper { nullptr };
#elif PLATFORM(WPE)
    RefPtr<AccessibilityObjectWrapper> m_wrapper;
#endif
};

#if !HAVE(ACCESSIBILITY)
inline const AccessibilityObject::AccessibilityChildrenVector& AccessibilityObject::children(bool) { return m_children; }
inline const String& AccessibilityObject::actionVerb() const { return emptyString(); }
inline int AccessibilityObject::lineForPosition(const VisiblePosition&) const { return -1; }
inline void AccessibilityObject::updateBackingStore() { }
#endif

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::AccessibilityObject& object) { return object.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
