/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <cstdint>

namespace WebCore {

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
    FrameHost,
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
    LocalFrame,
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

} // namespace WebCore
