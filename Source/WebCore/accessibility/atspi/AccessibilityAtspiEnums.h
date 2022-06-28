/*
 * Copyright (C) 2021 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(ATSPI)
namespace WebCore {
namespace Atspi {

enum Role {
    InvalidRole,
    AcceleratorLabel,
    Alert,
    Animation,
    Arrow,
    Calendar,
    Canvas,
    CheckBox,
    CheckMenuItem,
    ColorChooser,
    ColumnHeader,
    ComboBox,
    DateEditor,
    DesktopIcon,
    DesktopFrame,
    Dial,
    Dialog,
    DirectoryPane,
    DrawingArea,
    FileChooser,
    Filler,
    FocusTraversable,
    FontChooser,
    Frame,
    GlassPane,
    HtmlContainer,
    Icon,
    Image,
    InternalFrame,
    Label,
    LayeredPane,
    List,
    ListItem,
    Menu,
    MenuBar,
    MenuItem,
    OptionPane,
    PageTab,
    PageTabList,
    Panel,
    PasswordText,
    PopupMenu,
    ProgressBar,
    PushButton,
    RadioButton,
    RadioMenuItem,
    RootPane,
    RowHeader,
    ScrollBar,
    ScrollPane,
    Separator,
    Slider,
    SpinButton,
    SplitPane,
    StatusBar,
    Table,
    TableCell,
    TableColumnHeader,
    TableRowHeader,
    TearoffMenuItem,
    Terminal,
    Text,
    ToggleButton,
    ToolBar,
    ToolTip,
    Tree,
    TreeTable,
    Unknown,
    Viewport,
    Window,
    Extended,
    Header,
    Footer,
    Paragraph,
    Ruler,
    Application,
    Autocomplete,
    Editbar,
    Embedded,
    Entry,
    Chart,
    Caption,
    DocumentFrame,
    Heading,
    Page,
    Section,
    RedundantObject,
    Form,
    Link,
    InputMethodWindow,
    TableRow,
    TreeItem,
    DocumentSpreadsheet,
    DocumentPresentation,
    DocumentText,
    DocumentWeb,
    DocumentEmail,
    Comment,
    ListBox,
    Grouping,
    ImageMap,
    Notification,
    InfoBar,
    LevelBar,
    TitleBar,
    BlockQuote,
    Audio,
    Video,
    Definition,
    Article,
    Landmark,
    Log,
    Marquee,
    Math,
    Rating,
    Timer,
    Static,
    MathFraction,
    MathRoot,
    Subscript,
    Superscript,
    DescriptionList,
    DescriptionTerm,
    DescriptionValue,
    Footnote,
    ContentDeletion,
    ContentInsertion,
    Mark,
    Suggestion,
    LastDefinedRole,
};

enum State {
    InvalidState,
    Active,
    Armed,
    Busy,
    Checked,
    Collapsed,
    Defunct,
    Editable,
    Enabled,
    Expandable,
    Expanded,
    Focusable,
    Focused,
    HasTooltip,
    Horizontal,
    Iconified,
    Modal,
    MultiLine,
    Multiselectable,
    Opaque,
    Pressed,
    Resizable,
    Selectable,
    Selected,
    Sensitive,
    Showing,
    SingleLine,
    Stale,
    Transient,
    Vertical,
    Visible,
    ManagesDescendants,
    Indeterminate,
    Required,
    Truncated,
    Animated,
    InvalidEntry,
    SupportsAutocompletion,
    SelectableText,
    IsDefault,
    Visited,
    Checkable,
    HasPopup,
    ReadOnly,
    LastDefinedState,
};

enum Relation {
    Null,
    LabelFor,
    LabelledBy,
    ControllerFor,
    ControlledBy,
    MemberOf,
    TooltipFor,
    NodeChildOf,
    NodeParentOf,
    ExtendedRelation,
    FlowsTo,
    FlowsFrom,
    SubwindowOf,
    Embeds,
    EmbeddedBy,
    PoupFor,
    ParentWindowOf,
    DescriptionFor,
    DescribedBy,
    Details,
    DetailsFor,
    ErrorMessage,
    ErrorFor,
    LastDefinedRelation,
};

enum CoordinateType {
    ScreenCoordinates,
    WindowCoordinates,
    ParentCoordinates,
};

enum ComponentLayer {
    InvalidLayer,
    BackgroundLayer,
    CanvasLayer,
    WidgetLayer,
    MdiLayer,
    PopupLayer,
    OverlayLayer,
    WindowLayer,
};

enum ScrollType {
    TopLeft,
    BottomRight,
    TopEdge,
    BottomEdge,
    LeftEdge,
    RightEdge,
    Anywhere
};

enum TextBoundaryType {
    CharBoundary,
    WordStartBoundary,
    WordEndBoundary,
    SentenceStartBoundary,
    SentenceEndBoundary,
    LineStartBoundary,
    LineEndBoundary
};

enum TextGranularityType {
    CharGranularity,
    WordGranularity,
    SentenceGranularity,
    LineGranularity,
    ParagraphGranularity
};

enum CollectionMatchType {
    MatchInvalid,
    MatchAll,
    MatchAny,
    MatchNone,
    MatchEmpty
};

enum CollectionSortOrder {
    SortOrderInvalid,
    SortOrderCanonical,
    SortOrderFlow,
    SortOrderTab,
    SortOrderReverseCanonical,
    SortOrderReverseFlow,
    SortOrderReverseTab
};

} // namespace Atspi
} // namespace WebCore

#endif // USE(ATSPI)
