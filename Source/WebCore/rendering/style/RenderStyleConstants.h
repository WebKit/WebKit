/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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

enum class DumpStyleValues {
    All,
    NonInitial,
};

static const size_t PrintColorAdjustBits = 1;
enum class PrintColorAdjust : uint8_t {
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
enum class PseudoId : uint16_t {
    // The order must be None, public IDs, and then internal IDs.
    None,

    // Public:
    FirstLine,
    FirstLetter,
    Highlight,
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

    void remove(PseudoId pseudoId)
    {
        ASSERT((sizeof(m_data) * 8) > static_cast<unsigned>(pseudoId));
        m_data &= ~(1U << static_cast<unsigned>(pseudoId));
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

enum class ColumnFill : uint8_t {
    Balance,
    Auto
};

enum class ColumnSpan : uint8_t {
    None = 0,
    All
};

enum class BorderCollapse : uint8_t {
    Separate = 0,
    Collapse
};

// These have been defined in the order of their precedence for border-collapsing. Do
// not change this order! This order also must match the order in CSSValueKeywords.in.
enum class BorderStyle : uint8_t {
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

enum class BorderPrecedence : uint8_t {
    Off,
    Table,
    ColumnGroup,
    Column,
    RowGroup,
    Row,
    Cell
};

enum class OutlineIsAuto : uint8_t {
    Off = 0,
    On
};

enum class PositionType : uint8_t {
    Static = 0,
    Relative = 1,
    Absolute = 2,
    Sticky = 3,
    // This value is required to pack our bits efficiently in RenderObject.
    Fixed = 6
};

enum class Float : uint8_t {
    No,
    Left,
    Right
};

enum class MarginCollapse : uint8_t {
    Collapse,
    Separate,
    Discard
};

// Box decoration attributes. Not inherited.

enum class BoxDecorationBreak : uint8_t {
    Slice,
    Clone
};

// Box attributes. Not inherited.

enum class BoxSizing : uint8_t {
    ContentBox,
    BorderBox
};

// Random visual rendering model attributes. Not inherited.

enum class Overflow : uint8_t {
    Visible,
    Hidden,
    Scroll,
    Auto,
    PagedX,
    PagedY
};

enum class VerticalAlign : uint8_t {
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

enum class Clear : uint8_t {
    None = 0,
    Left = 1,
    Right = 2,
    Both = 3
};

enum class TableLayoutType : uint8_t {
    Auto,
    Fixed
};

enum class TextCombine : uint8_t {
    None,
    Horizontal
};

enum class FillAttachment : uint8_t {
    ScrollBackground,
    LocalBackground,
    FixedBackground
};

enum class FillBox : uint8_t {
    Border,
    Padding,
    Content,
    Text
};

enum class FillRepeat : uint8_t {
    Repeat,
    NoRepeat,
    Round,
    Space
};

enum class FillLayerType : uint8_t {
    Background,
    Mask
};

// CSS3 Background Values
enum class FillSizeType : uint8_t {
    Contain,
    Cover,
    Size,
    None
};

// CSS3 <position>
enum class Edge : uint8_t {
    Top,
    Right,
    Bottom,
    Left
};

// CSS3 Mask Source Types

enum class MaskSourceType : uint8_t {
    Alpha,
    Luminance
};

// CSS3 Marquee Properties

enum class MarqueeBehavior : uint8_t {
    None,
    Scroll,
    Slide,
    Alternate
};

enum class MarqueeDirection : uint8_t {
    Auto,
    Left,
    Right,
    Up,
    Down,
    Forward,
    Backward
};

// Deprecated Flexible Box Properties

enum class BoxPack : uint8_t {
    Start,
    Center,
    End,
    Justify
};

enum class BoxAlignment : uint8_t {
    Stretch,
    Start,
    Center,
    End,
    Baseline
};

enum class BoxOrient : uint8_t {
    Horizontal,
    Vertical
};

enum class BoxLines : uint8_t {
    Single,
    Multiple
};

enum class BoxDirection : uint8_t {
    Normal,
    Reverse
};

// CSS3 Flexbox Properties

enum class AlignContent : uint8_t {
    FlexStart,
    FlexEnd,
    Center,
    SpaceBetween,
    SpaceAround,
    Stretch
};

enum class FlexDirection : uint8_t {
    Row,
    RowReverse,
    Column,
    ColumnReverse
};

enum class FlexWrap : uint8_t {
    NoWrap,
    Wrap,
    Reverse
};

enum class ItemPosition : uint8_t {
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

enum class OverflowAlignment : uint8_t {
    Default,
    Unsafe,
    Safe
};

enum class ItemPositionType : uint8_t {
    NonLegacy,
    Legacy
};

enum class ContentPosition : uint8_t {
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

enum class ContentDistribution : uint8_t {
    Default,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
    Stretch
};


enum class TextSecurity : uint8_t {
    None,
    Disc,
    Circle,
    Square
};

// CSS3 User Modify Properties

enum class UserModify : uint8_t {
    ReadOnly,
    ReadWrite,
    ReadWritePlaintextOnly
};

// CSS3 User Drag Values

enum class UserDrag : uint8_t {
    Auto,
    None,
    Element
};

// CSS3 User Select Values

enum class UserSelect : uint8_t {
    None,
    Text,
    All
};

// CSS3 Image Values
enum class ObjectFit : uint8_t {
    Fill,
    Contain,
    Cover,
    None,
    ScaleDown
};

enum class AspectRatioType : uint8_t {
    Auto,
    FromIntrinsic,
    FromDimensions,
    Specified
};

enum class WordBreak : uint8_t {
    Normal,
    BreakAll,
    KeepAll,
    BreakWord
};

enum class OverflowWrap : uint8_t {
    Normal,
    Break
};

enum class NBSPMode : uint8_t {
    Normal,
    Space
};

enum class LineBreak : uint8_t {
    Auto,
    Loose,
    Normal,
    Strict,
    AfterWhiteSpace,
    Anywhere
};

enum class Resize : uint8_t {
    None,
    Both,
    Horizontal,
    Vertical
};

// The order of this enum must match the order of the list style types in CSSValueKeywords.in.
enum class ListStyleType : uint8_t {
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

enum class QuoteType : uint8_t {
    OpenQuote,
    CloseQuote,
    NoOpenQuote,
    NoCloseQuote
};

enum class BorderFit : uint8_t {
    Border,
    Lines
};

enum class AnimationFillMode : uint8_t {
    None,
    Forwards,
    Backwards,
    Both
};

enum class AnimationPlayState : uint8_t {
    Playing = 0x0,
    Paused = 0x1
};

enum class WhiteSpace : uint8_t {
    Normal,
    Pre,
    PreWrap,
    PreLine,
    NoWrap,
    KHTMLNoWrap,
    BreakSpaces
};

enum class ReflectionDirection : uint8_t {
    Below,
    Above,
    Left,
    Right
};

// The order of this enum must match the order of the text align values in CSSValueKeywords.in.
enum class TextAlignMode : uint8_t {
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

enum class TextTransform : uint8_t {
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
enum class TextDecoration : uint8_t {
    None          = 0,
    Underline     = 1 << 0,
    Overline      = 1 << 1,
    LineThrough   = 1 << 2,
    Blink         = 1 << 3,
#if ENABLE(LETTERPRESS)
    Letterpress   = 1 << 4,
#endif
};

enum class TextDecorationStyle : uint8_t {
    Solid,
    Double,
    Dotted,
    Dashed,
    Wavy
};

#if ENABLE(CSS3_TEXT)
enum class TextAlignLast : uint8_t {
    Auto,
    Start,
    End,
    Left,
    Right,
    Center,
    Justify
};

enum class TextJustify : uint8_t {
    Auto,
    None,
    InterWord,
    Distribute
};
#endif // CSS3_TEXT

enum class TextDecorationSkip : uint8_t {
    None      = 0,
    Ink       = 1 << 0,
    Objects   = 1 << 1,
    Auto      = 1 << 2
};

enum class TextUnderlinePosition : uint8_t {
    // FIXME: Implement support for 'under left' and 'under right' values.
    Auto,
    Under,
    FromFont
};

enum class TextZoom : uint8_t {
    Normal,
    Reset
};

enum class BreakBetween : uint8_t {
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
    
enum class BreakInside : uint8_t {
    Auto,
    Avoid,
    AvoidColumn,
    AvoidPage
};

enum class HangingPunctuation : uint8_t {
    None      = 0,
    First     = 1 << 0,
    Last      = 1 << 1,
    AllowEnd  = 1 << 2,
    ForceEnd  = 1 << 3
};

enum class EmptyCell : uint8_t {
    Show,
    Hide
};

enum class CaptionSide : uint8_t {
    Top,
    Bottom,
    Left,
    Right
};

enum class ListStylePosition : uint8_t {
    Outside,
    Inside
};

enum class Visibility : uint8_t {
    Visible,
    Hidden,
    Collapse
};

enum class CursorType : uint8_t {
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
enum class CursorVisibility : uint8_t {
    Auto,
    AutoHide,
};
#endif

// The order of this enum must match the order of the display values in CSSValueKeywords.in.
enum class DisplayType : uint8_t {
    Inline,
    Block,
    ListItem,
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
    FlowRoot,
    None
};

enum class InsideLink : uint8_t {
    NotInside,
    InsideUnvisited,
    InsideVisited
};
    
enum class PointerEvents : uint8_t {
    None,
    Auto,
    Stroke,
    Fill,
    Painted,
    Visible,
    VisibleStroke,
    VisibleFill,
    VisiblePainted,
    BoundingBox,
    All
};

enum class TransformStyle3D : uint8_t {
    Flat,
    Preserve3D
};

enum class BackfaceVisibility : uint8_t {
    Visible,
    Hidden
};

enum class TransformBox : uint8_t {
    StrokeBox,
    ContentBox,
    BorderBox,
    FillBox,
    ViewBox
};

enum class LineClamp : uint8_t {
    LineCount,
    Percentage
};

enum class Hyphens : uint8_t {
    None,
    Manual,
    Auto
};

enum class SpeakAs : uint8_t {
    Normal             = 0,
    SpellOut           = 1 << 0,
    Digits             = 1 << 1,
    LiteralPunctuation = 1 << 2,
    NoPunctuation      = 1 << 3
};

enum class TextEmphasisFill : uint8_t {
    Filled,
    Open
};

enum class TextEmphasisMark : uint8_t {
    None,
    Auto,
    Dot,
    Circle,
    DoubleCircle,
    Triangle,
    Sesame,
    Custom
};

enum class TextEmphasisPosition : uint8_t {
    Over  = 1 << 0,
    Under = 1 << 1,
    Left  = 1 << 2,
    Right = 1 << 3
};

enum class TextOrientation : uint8_t {
    Mixed,
    Upright,
    Sideways
};

enum class TextOverflow : uint8_t {
    Clip = 0,
    Ellipsis
};

enum class ImageRendering : uint8_t {
    Auto = 0,
    OptimizeSpeed,
    OptimizeQuality,
    CrispEdges,
    Pixelated
};

enum class ImageResolutionSource : uint8_t {
    Specified = 0,
    FromImage
};

enum class ImageResolutionSnap : uint8_t {
    None = 0,
    Pixels
};

enum class Order : uint8_t {
    Logical = 0,
    Visual
};

enum class ColumnAxis : uint8_t {
    Horizontal,
    Vertical,
    Auto
};

enum class ColumnProgression : uint8_t {
    Normal,
    Reverse
};

enum class LineSnap : uint8_t {
    None,
    Baseline,
    Contain
};

enum class LineAlign : uint8_t {
    None,
    Edges
};

enum class RubyPosition : uint8_t {
    Before,
    After,
    InterCharacter
};

#if ENABLE(DARK_MODE_CSS)
enum class ColorScheme : uint8_t {
    Light = 1 << 0,
    Dark = 1 << 1
};

static const size_t ColorSchemeBits = 2;
#endif

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

enum class AutoRepeatType : uint8_t {
    None,
    Fill,
    Fit
};

// Reasonable maximum to prevent insane font sizes from causing crashes on some platforms (such as Windows).
static const float maximumAllowedFontSize = 1000000.0f;

#if ENABLE(CSS3_TEXT)

enum class TextIndentLine : uint8_t {
    FirstLine,
    EachLine
};

enum class TextIndentType : uint8_t {
    Normal,
    Hanging
};

#endif

enum class Isolation : uint8_t {
    Auto,
    Isolate
};

// Fill, Stroke, ViewBox are just used for SVG.
enum class CSSBoxType : uint8_t {
    BoxMissing = 0,
    MarginBox,
    BorderBox,
    PaddingBox,
    ContentBox,
    FillBox,
    StrokeBox,
    ViewBox
};

#if ENABLE(CSS_SCROLL_SNAP)
enum class ScrollSnapStrictness : uint8_t {
    None,
    Proximity,
    Mandatory
};

enum class ScrollSnapAxis : uint8_t {
    XAxis,
    YAxis,
    Block,
    Inline,
    Both
};

enum class ScrollSnapAxisAlignType : uint8_t {
    None,
    Start,
    Center,
    End
};
#endif

#if ENABLE(CSS_TRAILING_WORD)
enum class TrailingWord : uint8_t {
    Auto,
    PartiallyBalanced
};
#endif

#if ENABLE(APPLE_PAY)
enum class ApplePayButtonStyle : uint8_t {
    White,
    WhiteOutline,
    Black,
};

enum class ApplePayButtonType : uint8_t {
    Plain,
    Buy,
    SetUp,
    Donate,
    CheckOut,
    Book,
    Subscribe,
#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
    Reload,
    AddMoney,
    TopUp,
    Order,
    Rent,
    Support,
    Contribute,
    Tip,
#endif
};
#endif

// These are all minimized combinations of paint-order.
enum class PaintOrder : uint8_t {
    Normal,
    Fill,
    FillMarkers,
    Stroke,
    StrokeMarkers,
    Markers,
    MarkersStroke
};

enum class PaintType : uint8_t {
    Fill,
    Stroke,
    Markers
};

enum class FontLoadingBehavior : uint8_t {
    Auto,
    Block,
    Swap,
    Fallback,
    Optional
};

enum class EventListenerRegionType : uint8_t {
    Wheel           = 1 << 0,
    NonPassiveWheel = 1 << 1,
};

enum class MathStyle : uint8_t {
    Normal,
    Compact,
};

extern const float defaultMiterLimit;

WTF::TextStream& operator<<(WTF::TextStream&, AnimationFillMode);
WTF::TextStream& operator<<(WTF::TextStream&, AnimationPlayState);
#if ENABLE(APPLE_PAY)
WTF::TextStream& operator<<(WTF::TextStream&, ApplePayButtonStyle);
WTF::TextStream& operator<<(WTF::TextStream&, ApplePayButtonType);
#endif
WTF::TextStream& operator<<(WTF::TextStream&, AspectRatioType);
WTF::TextStream& operator<<(WTF::TextStream&, AutoRepeatType);
WTF::TextStream& operator<<(WTF::TextStream&, BackfaceVisibility);
WTF::TextStream& operator<<(WTF::TextStream&, BorderCollapse);
WTF::TextStream& operator<<(WTF::TextStream&, BorderFit);
WTF::TextStream& operator<<(WTF::TextStream&, BorderStyle);
WTF::TextStream& operator<<(WTF::TextStream&, BoxAlignment);
WTF::TextStream& operator<<(WTF::TextStream&, BoxDecorationBreak);
WTF::TextStream& operator<<(WTF::TextStream&, BoxDirection);
WTF::TextStream& operator<<(WTF::TextStream&, BoxLines);
WTF::TextStream& operator<<(WTF::TextStream&, BoxOrient);
WTF::TextStream& operator<<(WTF::TextStream&, BoxPack);
WTF::TextStream& operator<<(WTF::TextStream&, BoxSizing);
WTF::TextStream& operator<<(WTF::TextStream&, BreakBetween);
WTF::TextStream& operator<<(WTF::TextStream&, BreakInside);
WTF::TextStream& operator<<(WTF::TextStream&, CSSBoxType);
WTF::TextStream& operator<<(WTF::TextStream&, CaptionSide);
WTF::TextStream& operator<<(WTF::TextStream&, Clear);
#if ENABLE(DARK_MODE_CSS)
WTF::TextStream& operator<<(WTF::TextStream&, ColorScheme);
#endif
WTF::TextStream& operator<<(WTF::TextStream&, ColumnAxis);
WTF::TextStream& operator<<(WTF::TextStream&, ColumnFill);
WTF::TextStream& operator<<(WTF::TextStream&, ColumnProgression);
WTF::TextStream& operator<<(WTF::TextStream&, ColumnSpan);
WTF::TextStream& operator<<(WTF::TextStream&, ContentDistribution);
WTF::TextStream& operator<<(WTF::TextStream&, ContentPosition);
WTF::TextStream& operator<<(WTF::TextStream&, CursorType);
#if ENABLE(CURSOR_VISIBILITY)
WTF::TextStream& operator<<(WTF::TextStream&, CursorVisibility);
#endif
WTF::TextStream& operator<<(WTF::TextStream&, DisplayType);
WTF::TextStream& operator<<(WTF::TextStream&, Edge);
WTF::TextStream& operator<<(WTF::TextStream&, EmptyCell);
WTF::TextStream& operator<<(WTF::TextStream&, EventListenerRegionType);
WTF::TextStream& operator<<(WTF::TextStream&, FillAttachment);
WTF::TextStream& operator<<(WTF::TextStream&, FillBox);
WTF::TextStream& operator<<(WTF::TextStream&, FillRepeat);
WTF::TextStream& operator<<(WTF::TextStream&, FillSizeType);
WTF::TextStream& operator<<(WTF::TextStream&, FlexDirection);
WTF::TextStream& operator<<(WTF::TextStream&, FlexWrap);
WTF::TextStream& operator<<(WTF::TextStream&, Float);
WTF::TextStream& operator<<(WTF::TextStream&, GridAutoFlow);
WTF::TextStream& operator<<(WTF::TextStream&, HangingPunctuation);
WTF::TextStream& operator<<(WTF::TextStream&, Hyphens);
WTF::TextStream& operator<<(WTF::TextStream&, ImageRendering);
WTF::TextStream& operator<<(WTF::TextStream&, InsideLink);
WTF::TextStream& operator<<(WTF::TextStream&, Isolation);
WTF::TextStream& operator<<(WTF::TextStream&, ItemPosition);
WTF::TextStream& operator<<(WTF::TextStream&, ItemPositionType);
WTF::TextStream& operator<<(WTF::TextStream&, LineAlign);
WTF::TextStream& operator<<(WTF::TextStream&, LineBreak);
WTF::TextStream& operator<<(WTF::TextStream&, LineSnap);
WTF::TextStream& operator<<(WTF::TextStream&, ListStylePosition);
WTF::TextStream& operator<<(WTF::TextStream&, ListStyleType);
WTF::TextStream& operator<<(WTF::TextStream&, MarginCollapse);
WTF::TextStream& operator<<(WTF::TextStream&, MarqueeBehavior);
WTF::TextStream& operator<<(WTF::TextStream&, MarqueeDirection);
WTF::TextStream& operator<<(WTF::TextStream&, MaskSourceType);
WTF::TextStream& operator<<(WTF::TextStream&, NBSPMode);
WTF::TextStream& operator<<(WTF::TextStream&, ObjectFit);
WTF::TextStream& operator<<(WTF::TextStream&, Order);
WTF::TextStream& operator<<(WTF::TextStream&, WebCore::Overflow);
WTF::TextStream& operator<<(WTF::TextStream&, OverflowAlignment);
WTF::TextStream& operator<<(WTF::TextStream&, OverflowWrap);
WTF::TextStream& operator<<(WTF::TextStream&, PaintOrder);
WTF::TextStream& operator<<(WTF::TextStream&, PointerEvents);
WTF::TextStream& operator<<(WTF::TextStream&, PositionType);
WTF::TextStream& operator<<(WTF::TextStream&, PrintColorAdjust);
WTF::TextStream& operator<<(WTF::TextStream&, PseudoId);
WTF::TextStream& operator<<(WTF::TextStream&, QuoteType);
WTF::TextStream& operator<<(WTF::TextStream&, ReflectionDirection);
WTF::TextStream& operator<<(WTF::TextStream&, Resize);
WTF::TextStream& operator<<(WTF::TextStream&, RubyPosition);
#if ENABLE(CSS_SCROLL_SNAP)
WTF::TextStream& operator<<(WTF::TextStream&, ScrollSnapAxis);
WTF::TextStream& operator<<(WTF::TextStream&, ScrollSnapAxisAlignType);
WTF::TextStream& operator<<(WTF::TextStream&, ScrollSnapStrictness);
#endif
WTF::TextStream& operator<<(WTF::TextStream&, SpeakAs);
WTF::TextStream& operator<<(WTF::TextStream&, StyleDifference);
WTF::TextStream& operator<<(WTF::TextStream&, TableLayoutType);
WTF::TextStream& operator<<(WTF::TextStream&, TextAlignMode);
WTF::TextStream& operator<<(WTF::TextStream&, TextCombine);
WTF::TextStream& operator<<(WTF::TextStream&, TextDecoration);
WTF::TextStream& operator<<(WTF::TextStream&, TextDecorationSkip);
WTF::TextStream& operator<<(WTF::TextStream&, TextDecorationStyle);
WTF::TextStream& operator<<(WTF::TextStream&, TextEmphasisFill);
WTF::TextStream& operator<<(WTF::TextStream&, TextEmphasisMark);
WTF::TextStream& operator<<(WTF::TextStream&, TextEmphasisPosition);
WTF::TextStream& operator<<(WTF::TextStream&, TextOrientation);
WTF::TextStream& operator<<(WTF::TextStream&, TextOverflow);
WTF::TextStream& operator<<(WTF::TextStream&, TextSecurity);
WTF::TextStream& operator<<(WTF::TextStream&, TextTransform);
WTF::TextStream& operator<<(WTF::TextStream&, TextUnderlinePosition);
WTF::TextStream& operator<<(WTF::TextStream&, TextZoom);
WTF::TextStream& operator<<(WTF::TextStream&, TransformBox);
WTF::TextStream& operator<<(WTF::TextStream&, TransformStyle3D);
WTF::TextStream& operator<<(WTF::TextStream&, UserDrag);
WTF::TextStream& operator<<(WTF::TextStream&, UserModify);
WTF::TextStream& operator<<(WTF::TextStream&, UserSelect);
WTF::TextStream& operator<<(WTF::TextStream&, VerticalAlign);
WTF::TextStream& operator<<(WTF::TextStream&, Visibility);
WTF::TextStream& operator<<(WTF::TextStream&, WhiteSpace);
WTF::TextStream& operator<<(WTF::TextStream&, WordBreak);
WTF::TextStream& operator<<(WTF::TextStream&, MathStyle);

} // namespace WebCore
