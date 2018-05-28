/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 *
 */

#pragma once

#include <initializer_list>

namespace WTF {
class TextStream;
}

namespace WebCore {

static const size_t PrintColorAdjustBits = 1;
enum class PrintColorAdjust {
    Economy,
    Exact
};

// The difference between two styles.  The following values are used:
// - StyleDifference::Equal - The two styles are identical
// - StyleDifference::RecompositeLayer - The layer needs its position and transform updated, but no repaint
// - StyleDifference::Repaint - The object just needs to be repainted.
// - StyleDifference::RepaintIfTextOrBorderOrOutline - The object needs to be repainted if it contains text or a border or outline.
// - StyleDifference::RepaintLayer - The layer and its descendant layers needs to be repainted.
// - StyleDifference::LayoutPositionedMovementOnly - Only the position of this positioned object has been updated
// - StyleDifference::SimplifiedLayout - Only overflow needs to be recomputed
// - StyleDifference::SimplifiedLayoutAndPositionedMovement - Both positioned movement and simplified layout updates are required.
// - StyleDifference::Layout - A full layout is required.
enum class StyleDifference {
    Equal,
    RecompositeLayer,
    Repaint,
    RepaintIfTextOrBorderOrOutline,
    RepaintLayer,
    LayoutPositionedMovementOnly,
    SimplifiedLayout,
    SimplifiedLayoutAndPositionedMovement,
    Layout,
    NewStyle
};

// When some style properties change, different amounts of work have to be done depending on
// context (e.g. whether the property is changing on an element which has a compositing layer).
// A simple StyleDifference does not provide enough information so we return a bit mask of
// StyleDifferenceContextSensitiveProperties from RenderStyle::diff() too.
enum class StyleDifferenceContextSensitiveProperty {
    None        = 0,
    Transform   = 1 << 0,
    Opacity     = 1 << 1,
    Filter      = 1 << 2,
    ClipRect    = 1 << 3,
    ClipPath    = 1 << 4,
    WillChange  = 1 << 5,
};

// Static pseudo styles. Dynamic ones are produced on the fly.
enum class PseudoId : unsigned char {
    // The order must be None, public IDs, and then internal IDs.
    None,

    // Public:
    FirstLine,
    FirstLetter,
    Marker,
    Before,
    After,
    Selection,
    Scrollbar,

    // Internal:
    ScrollbarThumb,
    ScrollbarButton,
    ScrollbarTrack,
    ScrollbarTrackPiece,
    ScrollbarCorner,
    Resizer,

    AfterLastInternalPseudoId,

    FirstPublicPseudoId = FirstLine,
    FirstInternalPseudoId = ScrollbarThumb,
    PublicPseudoIdMask = ((1 << FirstInternalPseudoId) - 1) & ~((1 << FirstPublicPseudoId) - 1)
};

class PseudoIdSet {
public:
    PseudoIdSet()
        : m_data(0)
    {
    }

    PseudoIdSet(std::initializer_list<PseudoId> initializerList)
        : m_data(0)
    {
        for (PseudoId pseudoId : initializerList)
            add(pseudoId);
    }

    static PseudoIdSet fromMask(unsigned rawPseudoIdSet)
    {
        return PseudoIdSet(rawPseudoIdSet);
    }

    bool has(PseudoId pseudoId) const
    {
        ASSERT((sizeof(m_data) * 8) > static_cast<unsigned>(pseudoId));
        return m_data & (1U << static_cast<unsigned>(pseudoId));
    }

    void add(PseudoId pseudoId)
    {
        ASSERT((sizeof(m_data) * 8) > static_cast<unsigned>(pseudoId));
        m_data |= (1U << static_cast<unsigned>(pseudoId));
    }

    void merge(PseudoIdSet source)
    {
        m_data |= source.m_data;
    }

    PseudoIdSet operator &(const PseudoIdSet& pseudoIdSet) const
    {
        return PseudoIdSet(m_data & pseudoIdSet.m_data);
    }

    PseudoIdSet operator |(const PseudoIdSet& pseudoIdSet) const
    {
        return PseudoIdSet(m_data | pseudoIdSet.m_data);
    }

    explicit operator bool() const
    {
        return m_data;
    }

    unsigned data() const { return m_data; }

    static ptrdiff_t dataMemoryOffset() { return OBJECT_OFFSETOF(PseudoIdSet, m_data); }

private:
    explicit PseudoIdSet(unsigned rawPseudoIdSet)
        : m_data(rawPseudoIdSet)
    {
    }

    unsigned m_data;
};

enum class ColumnFill {
    Balance,
    Auto
};

enum class ColumnSpan {
    None = 0,
    All
};

enum class BorderCollapse {
    Separate = 0,
    Collapse
};

// These have been defined in the order of their precedence for border-collapsing. Do
// not change this order! This order also must match the order in CSSValueKeywords.in.
enum class BorderStyle {
    None,
    Hidden,
    Inset,
    Groove,
    Outset,
    Ridge,
    Dotted,
    Dashed,
    Solid,
    Double
};

enum class BorderPrecedence {
    Off,
    Table,
    ColumnGroup,
    Column,
    RowGroup,
    Row,
    Cell
};

enum class OutlineIsAuto {
    Off = 0,
    On
};

enum class PositionType {
    Static = 0,
    Relative = 1,
    Absolute = 2,
    Sticky = 3,
    // This value is required to pack our bits efficiently in RenderObject.
    Fixed = 6
};

enum class Float {
    No,
    Left,
    Right
};

enum class MarginCollapse {
    Collapse,
    Separate,
    Discard
};

// Box decoration attributes. Not inherited.

enum class BoxDecorationBreak {
    Slice,
    Clone
};

// Box attributes. Not inherited.

enum class BoxSizing {
    ContentBox,
    BorderBox
};

// Random visual rendering model attributes. Not inherited.

enum class Overflow {
    Visible,
    Hidden,
    Scroll,
    Auto,
    Overlay,
    PagedX,
    PagedY
};

enum class VerticalAlign {
    Baseline,
    Middle,
    Sub,
    Super,
    TextTop,
    TextBottom,
    Top,
    Bottom,
    BaselineMiddle,
    Length
};

enum class Clear {
    None = 0,
    Left = 1,
    Right = 2,
    Both = 3
};

enum class TableLayoutType {
    Auto,
    Fixed
};

enum class TextCombine {
    None,
    Horizontal
};

enum class FillAttachment {
    ScrollBackground,
    LocalBackground,
    FixedBackground
};

enum class FillBox {
    Border,
    Padding,
    Content,
    Text
};

enum class FillRepeat {
    Repeat,
    NoRepeat,
    Round,
    Space
};

enum class FillLayerType {
    Background,
    Mask
};

// CSS3 Background Values
enum class FillSizeType {
    Contain,
    Cover,
    Size,
    None
};

// CSS3 <position>
enum class Edge {
    Top,
    Right,
    Bottom,
    Left
};

// CSS3 Mask Source Types

enum class MaskSourceType {
    Alpha,
    Luminance
};

// CSS3 Marquee Properties

enum class MarqueeBehavior {
    None,
    Scroll,
    Slide,
    Alternate
};

enum class MarqueeDirection {
    Auto,
    Left,
    Right,
    Up,
    Down,
    Forward,
    Backward
};

// Deprecated Flexible Box Properties

enum class BoxPack {
    Start,
    Center,
    End,
    Justify
};

enum class BoxAlignment {
    Stretch,
    Start,
    Center,
    End,
    Baseline
};

enum class BoxOrient {
    Horizontal,
    Vertical
};

enum class BoxLines {
    Single,
    Multiple
};

enum class BoxDirection {
    Normal,
    Reverse
};

// CSS3 Flexbox Properties

enum class AlignContent {
    FlexStart,
    FlexEnd,
    Center,
    SpaceBetween,
    SpaceAround,
    Stretch
};

enum class FlexDirection {
    Row,
    RowReverse,
    Column,
    ColumnReverse
};

enum class FlexWrap {
    NoWrap,
    Wrap,
    Reverse
};

enum class ItemPosition {
    Legacy,
    Auto,
    Normal,
    Stretch,
    Baseline,
    LastBaseline,
    Center,
    Start,
    End,
    SelfStart,
    SelfEnd,
    FlexStart,
    FlexEnd,
    Left,
    Right
};

enum class OverflowAlignment {
    Default,
    Unsafe,
    Safe
};

enum class ItemPositionType {
    NonLegacy,
    Legacy
};

enum class ContentPosition {
    Normal,
    Baseline,
    LastBaseline,
    Center,
    Start,
    End,
    FlexStart,
    FlexEnd,
    Left,
    Right
};

enum class ContentDistribution {
    Default,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
    Stretch
};


enum class TextSecurity {
    None,
    Disc,
    Circle,
    Square
};

// CSS3 User Modify Properties

enum class UserModify {
    ReadOnly,
    ReadWrite,
    ReadWritePlaintextOnly
};

// CSS3 User Drag Values

enum class UserDrag {
    Auto,
    None,
    Element
};

// CSS3 User Select Values

enum class UserSelect {
    None,
    Text,
    All
};

// CSS3 Image Values
enum class ObjectFit {
    Fill,
    Contain,
    Cover,
    None,
    ScaleDown
};

enum class AspectRatioType {
    Auto,
    FromIntrinsic,
    FromDimensions,
    Specified
};

enum class WordBreak {
    Normal,
    BreakAll,
    KeepAll,
    Break
};

enum class OverflowWrap {
    Normal,
    Break
};

enum class NBSPMode {
    Normal,
    Space
};

enum class LineBreak {
    Auto,
    Loose,
    Normal,
    Strict,
    AfterWhiteSpace
};

enum class Resize {
    None,
    Both,
    Horizontal,
    Vertical
};

// The order of this enum must match the order of the list style types in CSSValueKeywords.in.
enum class ListStyleType {
    Disc,
    Circle,
    Square,
    Decimal,
    DecimalLeadingZero,
    ArabicIndic,
    Binary,
    Bengali,
    Cambodian,
    Khmer,
    Devanagari,
    Gujarati,
    Gurmukhi,
    Kannada,
    LowerHexadecimal,
    Lao,
    Malayalam,
    Mongolian,
    Myanmar,
    Octal,
    Oriya,
    Persian,
    Urdu,
    Telugu,
    Tibetan,
    Thai,
    UpperHexadecimal,
    LowerRoman,
    UpperRoman,
    LowerGreek,
    LowerAlpha,
    LowerLatin,
    UpperAlpha,
    UpperLatin,
    Afar,
    EthiopicHalehameAaEt,
    EthiopicHalehameAaEr,
    Amharic,
    EthiopicHalehameAmEt,
    AmharicAbegede,
    EthiopicAbegedeAmEt,
    CjkEarthlyBranch,
    CjkHeavenlyStem,
    Ethiopic,
    EthiopicHalehameGez,
    EthiopicAbegede,
    EthiopicAbegedeGez,
    HangulConsonant,
    Hangul,
    LowerNorwegian,
    Oromo,
    EthiopicHalehameOmEt,
    Sidama,
    EthiopicHalehameSidEt,
    Somali,
    EthiopicHalehameSoEt,
    Tigre,
    EthiopicHalehameTig,
    TigrinyaEr,
    EthiopicHalehameTiEr,
    TigrinyaErAbegede,
    EthiopicAbegedeTiEr,
    TigrinyaEt,
    EthiopicHalehameTiEt,
    TigrinyaEtAbegede,
    EthiopicAbegedeTiEt,
    UpperGreek,
    UpperNorwegian,
    Asterisks,
    Footnotes,
    Hebrew,
    Armenian,
    LowerArmenian,
    UpperArmenian,
    Georgian,
    CJKIdeographic,
    Hiragana,
    Katakana,
    HiraganaIroha,
    KatakanaIroha,
    None
};

enum class QuoteType {
    OpenQuote,
    CloseQuote,
    NoOpenQuote,
    NoCloseQuote
};

enum class BorderFit {
    Border,
    Lines
};

enum class AnimationFillMode {
    None,
    Forwards,
    Backwards,
    Both
};

enum class AnimationPlayState {
    Playing = 0x0,
    Paused = 0x1
};

enum class WhiteSpace {
    Normal,
    Pre,
    PreWrap,
    PreLine,
    NoWrap,
    KHTMLNoWrap
};

// The order of this enum must match the order of the text align values in CSSValueKeywords.in.
enum class TextAlignMode {
    Left,
    Right,
    Center,
    Justify,
    WebKitLeft,
    WebKitRight,
    WebKitCenter,
    Start,
    End,
};

enum class TextTransform {
    Capitalize,
    Uppercase,
    Lowercase,
    None
};

#if ENABLE(LETTERPRESS)
static const size_t TextDecorationBits = 5;
#else
static const size_t TextDecorationBits = 4;
#endif
enum class TextDecoration {
    None          = 0,
    Underline     = 1 << 0,
    Overline      = 1 << 1,
    LineThrough   = 1 << 2,
    Blink         = 1 << 3,
#if ENABLE(LETTERPRESS)
    Letterpress   = 1 << 4,
#endif
};

enum class TextDecorationStyle {
    Solid,
    Double,
    Dotted,
    Dashed,
    Wavy
};

#if ENABLE(CSS3_TEXT)
enum class TextAlignLast {
    Auto,
    Start,
    End,
    Left,
    Right,
    Center,
    Justify
};

enum class TextJustify {
    Auto,
    None,
    InterWord,
    Distribute
};
#endif // CSS3_TEXT

enum class TextDecorationSkip {
    None      = 0,
    Ink       = 1 << 0,
    Objects   = 1 << 1,
    Auto      = 1 << 2
};

// FIXME: There is no reason for the values in the enum to be powers of two.
enum class TextUnderlinePosition {
    // FIXME: Implement support for 'under left' and 'under right' values.
    Auto       = 1 << 0,
    Alphabetic = 1 << 1,
    Under      = 1 << 2
};

enum class TextZoom {
    Normal,
    Reset
};

enum class BreakBetween {
    Auto,
    Avoid,
    AvoidColumn,
    AvoidPage,
    Column,
    Page,
    LeftPage,
    RightPage,
    RectoPage,
    VersoPage
};
bool alwaysPageBreak(BreakBetween);
    
enum class BreakInside {
    Auto,
    Avoid,
    AvoidColumn,
    AvoidPage
};

enum class HangingPunctuation {
    None      = 0,
    First     = 1 << 0,
    Last      = 1 << 1,
    AllowEnd  = 1 << 2,
    ForceEnd  = 1 << 3
};

enum class EmptyCell {
    Show,
    Hide
};

enum class CaptionSide {
    Top,
    Bottom,
    Left,
    Right
};

enum class ListStylePosition {
    Outside,
    Inside
};

enum class Visibility {
    Visible,
    Hidden,
    Collapse
};

WTF::TextStream& operator<<(WTF::TextStream&, Visibility);

enum class CursorType {
    // The following must match the order in CSSValueKeywords.in.
    Auto,
    Default,
    // None
    ContextMenu,
    Help,
    Pointer,
    Progress,
    Wait,
    Cell,
    Crosshair,
    Text,
    VerticalText,
    Alias,
    // Copy
    Move,
    NoDrop,
    NotAllowed,
    Grab,
    Grabbing,
    EResize,
    NResize,
    NEResize,
    NWResize,
    SResize,
    SEResize,
    SWResize,
    WResize,
    EWResize,
    NSResize,
    NESWResize,
    NWSEResize,
    ColumnResize,
    RowResize,
    AllScroll,
    ZoomIn,
    ZoomOut,

    // The following are handled as exceptions so don't need to match.
    Copy,
    None
};

#if ENABLE(CURSOR_VISIBILITY)
enum class CursorVisibility {
    Auto,
    AutoHide,
};
#endif

// The order of this enum must match the order of the display values in CSSValueKeywords.in.
enum class DisplayType {
    Inline,
    Block,
    ListItem,
    Compact,
    InlineBlock,
    Table,
    InlineTable,
    TableRowGroup,
    TableHeaderGroup,
    TableFooterGroup,
    TableRow,
    TableColumnGroup,
    TableColumn,
    TableCell,
    TableCaption,
    Box,
    InlineBox,
    Flex,
    WebKitFlex,
    InlineFlex,
    WebKitInlineFlex,
    Contents,
    Grid,
    InlineGrid,
    None
};

enum class InsideLink {
    NotInside,
    InsideUnvisited,
    InsideVisited
};
    
enum class PointerEvents {
    None,
    Auto,
    Stroke,
    Fill,
    Painted,
    Visible,
    VisibleStroke,
    VisibleFill,
    VisiblePainted,
    All
};

enum class TransformStyle3D {
    Flat,
    Preserve3D
};

enum class BackfaceVisibility {
    Visible,
    Hidden
};

enum class TransformBox {
    BorderBox,
    FillBox,
    ViewBox
};

enum class LineClamp {
    LineCount,
    Percentage
};

enum class Hyphens {
    None,
    Manual,
    Auto
};

enum class SpeakAs {
    Normal             = 0,
    SpellOut           = 1 << 0,
    Digits             = 1 << 1,
    LiteralPunctuation = 1 << 2,
    NoPunctuation      = 1 << 3
};

enum class TextEmphasisFill {
    Filled,
    Open
};

enum class TextEmphasisMark {
    None,
    Auto,
    Dot,
    Circle,
    DoubleCircle,
    Triangle,
    Sesame,
    Custom
};

enum class TextEmphasisPosition {
    Over  = 1 << 0,
    Under = 1 << 1,
    Left  = 1 << 2,
    Right = 1 << 3
};

enum class TextOrientation {
    Mixed,
    Upright,
    Sideways
};

enum class TextOverflow {
    Clip = 0,
    Ellipsis
};

enum class ImageRendering {
    Auto = 0,
    OptimizeSpeed,
    OptimizeQuality,
    CrispEdges,
    Pixelated
};

WTF::TextStream& operator<<(WTF::TextStream&, ImageRendering);

enum class ImageResolutionSource {
    Specified = 0,
    FromImage
};

enum class ImageResolutionSnap {
    None = 0,
    Pixels
};

enum class Order {
    Logical = 0,
    Visual
};

enum class ColumnAxis {
    Horizontal,
    Vertical,
    Auto
};

enum class ColumnProgression {
    Normal,
    Reverse
};

enum class LineSnap {
    None,
    Baseline,
    Contain
};

enum class LineAlign {
    None,
    Edges
};

enum class RubyPosition {
    Before,
    After,
    InterCharacter
};

static const size_t GridAutoFlowBits = 4;
enum InternalGridAutoFlowAlgorithm {
    InternalAutoFlowAlgorithmSparse = 1 << 0,
    InternalAutoFlowAlgorithmDense  = 1 << 1,
};

enum InternalGridAutoFlowDirection {
    InternalAutoFlowDirectionRow    = 1 << 2,
    InternalAutoFlowDirectionColumn = 1 << 3
};

enum GridAutoFlow {
    AutoFlowRow = InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionRow,
    AutoFlowColumn = InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionColumn,
    AutoFlowRowDense = InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionRow,
    AutoFlowColumnDense = InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionColumn
};

enum class AutoRepeatType {
    None,
    Fill,
    Fit
};

// Reasonable maximum to prevent insane font sizes from causing crashes on some platforms (such as Windows).
static const float maximumAllowedFontSize = 1000000.0f;

#if ENABLE(CSS3_TEXT)

enum class TextIndentLine {
    FirstLine,
    EachLine
};

enum class TextIndentType {
    Normal,
    Hanging
};

#endif

enum class Isolation {
    Auto,
    Isolate
};

// Fill, Stroke, ViewBox are just used for SVG.
enum class CSSBoxType {
    BoxMissing = 0,
    MarginBox,
    BorderBox,
    PaddingBox,
    ContentBox,
    Fill,
    Stroke,
    ViewBox
};

#if ENABLE(TOUCH_EVENTS)
enum class TouchAction {
    Auto,
    Manipulation
};
#endif

#if ENABLE(CSS_SCROLL_SNAP)
enum class ScrollSnapStrictness {
    None,
    Proximity,
    Mandatory
};

enum class ScrollSnapAxis {
    XAxis,
    YAxis,
    Block,
    Inline,
    Both
};

enum class ScrollSnapAxisAlignType {
    None,
    Start,
    Center,
    End
};
#endif

#if ENABLE(CSS_TRAILING_WORD)
enum class TrailingWord {
    Auto,
    PartiallyBalanced
};
#endif

#if ENABLE(APPLE_PAY)
enum class ApplePayButtonStyle {
    White,
    WhiteOutline,
    Black,
};

enum class ApplePayButtonType {
    Plain,
    Buy,
    SetUp,
    Donate,
};
#endif

WTF::TextStream& operator<<(WTF::TextStream&, FillSizeType);
WTF::TextStream& operator<<(WTF::TextStream&, FillAttachment);
WTF::TextStream& operator<<(WTF::TextStream&, FillBox);
WTF::TextStream& operator<<(WTF::TextStream&, FillRepeat);
WTF::TextStream& operator<<(WTF::TextStream&, MaskSourceType);
WTF::TextStream& operator<<(WTF::TextStream&, Edge);

// These are all minimized combinations of paint-order.
enum class PaintOrder {
    Normal,
    Fill,
    FillMarkers,
    Stroke,
    StrokeMarkers,
    Markers,
    MarkersStroke
};

enum class PaintType {
    Fill,
    Stroke,
    Markers
};

enum class FontLoadingBehavior {
    Auto,
    Block,
    Swap,
    Fallback,
    Optional
};

extern const float defaultMiterLimit;

} // namespace WebCore
