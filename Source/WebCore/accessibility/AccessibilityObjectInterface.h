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

#include "LayoutRect.h"
#include <wtf/RefCounted.h>

#if PLATFORM(WIN)
#include "AccessibilityObjectWrapperWin.h"
#include "COMPtr.h"
#endif

#if PLATFORM(COCOA)
OBJC_CLASS WebAccessibilityObjectWrapper;
typedef WebAccessibilityObjectWrapper AccessibilityObjectWrapper;
#elif USE(ATK)
typedef struct _WebKitAccessible WebKitAccessible;
typedef struct _WebKitAccessible AccessibilityObjectWrapper;
#else
class AccessibilityObjectWrapper;
#endif

namespace WebCore {

typedef unsigned AXID;
extern const AXID InvalidAXID;
typedef unsigned AXIsolatedTreeID;    

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

class AccessibilityObjectInterface {
public:
    virtual ~AccessibilityObjectInterface() = default;

    virtual bool isMediaControlLabel() const = 0;
    virtual AccessibilityRole roleValue() const = 0;
    virtual bool isAttachment() const = 0;
    virtual bool isLink() const = 0;
    virtual bool isImageMapLink() const = 0;
    virtual bool isImage() const = 0;
    virtual bool isFileUploadButton() const = 0;
    virtual bool isTree() const = 0;
    virtual bool isTreeItem() const = 0;
    virtual bool isScrollbar() const = 0;
    virtual bool accessibilityIsIgnored() const = 0;
    virtual FloatRect relativeFrame() const = 0;
    virtual AccessibilityObjectInterface* parentObjectInterfaceUnignored() const = 0;
    virtual AccessibilityObjectInterface* focusedUIElement() const = 0;
    virtual bool isAccessibilityObject() const { return false; }
    
#if PLATFORM(COCOA)
    virtual String speechHintAttributeValue() const = 0;
    virtual String descriptionAttributeValue() const = 0;
    virtual String helpTextAttributeValue() const = 0;
    virtual String titleAttributeValue() const = 0;
#endif

    virtual AccessibilityObjectWrapper* wrapper() const = 0;
    virtual AccessibilityObjectInterface* accessibilityHitTest(const IntPoint&) const = 0;
    virtual void updateChildrenIfNecessary() = 0;
};

} // namespace WebCore
