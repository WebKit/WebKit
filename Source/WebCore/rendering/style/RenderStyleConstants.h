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
enum PseudoId : unsigned char {
    // The order must be NOP ID, public IDs, and then internal IDs.
    NOPSEUDO,
    FIRST_LINE,
    FIRST_LETTER,
    MARKER,
    BEFORE,
    AFTER,
    SELECTION,
    SCROLLBAR,

    // Internal IDs follow:
    SCROLLBAR_THUMB,
    SCROLLBAR_BUTTON,
    SCROLLBAR_TRACK,
    SCROLLBAR_TRACK_PIECE,
    SCROLLBAR_CORNER,
    RESIZER,

    AFTER_LAST_INTERNAL_PSEUDOID,

    FIRST_PUBLIC_PSEUDOID = FIRST_LINE,
    FIRST_INTERNAL_PSEUDOID = SCROLLBAR_THUMB,
    PUBLIC_PSEUDOID_MASK = ((1 << FIRST_INTERNAL_PSEUDOID) - 1) & ~((1 << FIRST_PUBLIC_PSEUDOID) - 1)
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
        ASSERT((sizeof(m_data) * 8) > pseudoId);
        return m_data & (1U << pseudoId);
    }

    void add(PseudoId pseudoId)
    {
        ASSERT((sizeof(m_data) * 8) > pseudoId);
        m_data |= (1U << pseudoId);
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
enum EListStyleType {
    Disc,
    Circle,
    Square,
    DecimalListStyle,
    DecimalLeadingZero,
    ArabicIndic,
    BinaryListStyle,
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
    NoneListStyle
};

enum class QuoteType {
    OpenQuote,
    CloseQuote,
    NoOpenQuote,
    NoCloseQuote
};

enum EBorderFit {
    BorderFitBorder,
    BorderFitLines
};

enum EAnimationFillMode {
    AnimationFillModeNone,
    AnimationFillModeForwards,
    AnimationFillModeBackwards,
    AnimationFillModeBoth
};

enum EAnimPlayState {
    AnimPlayStatePlaying = 0x0,
    AnimPlayStatePaused = 0x1
};

enum EWhiteSpace {
    NORMAL,
    PRE,
    PRE_WRAP,
    PRE_LINE,
    NOWRAP,
    KHTML_NOWRAP
};

// The order of this enum must match the order of the text align values in CSSValueKeywords.in.
enum ETextAlign {
    LEFT,
    RIGHT,
    CENTER,
    JUSTIFY,
    WEBKIT_LEFT,
    WEBKIT_RIGHT,
    WEBKIT_CENTER,
    TASTART,
    TAEND,
};

enum ETextTransform {
    CAPITALIZE,
    UPPERCASE,
    LOWERCASE,
    TTNONE
};

#if ENABLE(LETTERPRESS)
static const size_t TextDecorationBits = 5;
#else
static const size_t TextDecorationBits = 4;
#endif
enum TextDecoration {
    TextDecorationNone = 0x0,
    TextDecorationUnderline = 0x1,
    TextDecorationOverline = 0x2,
    TextDecorationLineThrough = 0x4,
    TextDecorationBlink = 0x8,
#if ENABLE(LETTERPRESS)
    TextDecorationLetterpress = 0x10,
#endif
};
inline TextDecoration operator|(TextDecoration a, TextDecoration b) { return TextDecoration(int(a) | int(b)); }
inline TextDecoration& operator|=(TextDecoration& a, TextDecoration b) { return a = a | b; }

enum TextDecorationStyle {
    TextDecorationStyleSolid,
    TextDecorationStyleDouble,
    TextDecorationStyleDotted,
    TextDecorationStyleDashed,
    TextDecorationStyleWavy
};

#if ENABLE(CSS3_TEXT)
enum TextAlignLast {
    TextAlignLastAuto,
    TextAlignLastStart,
    TextAlignLastEnd,
    TextAlignLastLeft,
    TextAlignLastRight,
    TextAlignLastCenter,
    TextAlignLastJustify
};

enum TextJustify {
    TextJustifyAuto,
    TextJustifyNone,
    TextJustifyInterWord,
    TextJustifyDistribute
};
#endif // CSS3_TEXT

enum TextDecorationSkipItems {
    TextDecorationSkipNone = 0,
    TextDecorationSkipInk = 1 << 0,
    TextDecorationSkipObjects = 1 << 1,
    TextDecorationSkipAuto = 1 << 2
};
typedef unsigned TextDecorationSkip;

enum TextUnderlinePosition {
    // FIXME: Implement support for 'under left' and 'under right' values.
    TextUnderlinePositionAuto = 0x1,
    TextUnderlinePositionAlphabetic = 0x2,
    TextUnderlinePositionUnder = 0x4
};

enum TextZoom {
    TextZoomNormal,
    TextZoomReset
};

enum BreakBetween {
    AutoBreakBetween,
    AvoidBreakBetween,
    AvoidColumnBreakBetween,
    AvoidPageBreakBetween,
    ColumnBreakBetween,
    PageBreakBetween,
    LeftPageBreakBetween,
    RightPageBreakBetween,
    RectoPageBreakBetween,
    VersoPageBreakBetween
};
bool alwaysPageBreak(BreakBetween);
    
enum BreakInside {
    AutoBreakInside,
    AvoidBreakInside,
    AvoidColumnBreakInside,
    AvoidPageBreakInside
};

enum HangingPunctuation {
    NoHangingPunctuation = 0,
    FirstHangingPunctuation = 1 << 0,
    LastHangingPunctuation = 1 << 1,
    AllowEndHangingPunctuation = 1 << 2,
    ForceEndHangingPunctuation = 1 << 3
};
inline HangingPunctuation operator|(HangingPunctuation a, HangingPunctuation b) { return HangingPunctuation(int(a) | int(b)); }
inline HangingPunctuation& operator|=(HangingPunctuation& a, HangingPunctuation b) { return a = a | b; }

enum EEmptyCell {
    SHOW,
    HIDE
};

enum ECaptionSide {
    CAPTOP,
    CAPBOTTOM,
    CAPLEFT,
    CAPRIGHT
};

enum EListStylePosition {
    OUTSIDE,
    INSIDE
};

enum EVisibility {
    VISIBLE,
    HIDDEN,
    COLLAPSE
};

enum ECursor {
    // The following must match the order in CSSValueKeywords.in.
    CursorAuto,
    CursorDefault,
    // CursorNone
    CursorContextMenu,
    CursorHelp,
    CursorPointer,
    CursorProgress,
    CursorWait,
    CursorCell,
    CursorCrosshair,
    CursorText,
    CursorVerticalText,
    CursorAlias,
    // CursorCopy
    CursorMove,
    CursorNoDrop,
    CursorNotAllowed,
    CursorGrab,
    CursorGrabbing,
    CursorEResize,
    CursorNResize,
    CursorNeResize,
    CursorNwResize,
    CursorSResize,
    CursorSeResize,
    CursorSwResize,
    CursorWResize,
    CursorEwResize,
    CursorNsResize,
    CursorNeswResize,
    CursorNwseResize,
    CursorColResize,
    CursorRowResize,
    CursorAllScroll,
    CursorZoomIn,
    CursorZoomOut,

    // The following are handled as exceptions so don't need to match.
    CursorCopy,
    CursorNone
};

#if ENABLE(CURSOR_VISIBILITY)
enum CursorVisibility {
    CursorVisibilityAuto,
    CursorVisibilityAutoHide,
};
#endif

// The order of this enum must match the order of the display values in CSSValueKeywords.in.
enum EDisplay {
    INLINE,
    BLOCK,
    LIST_ITEM,
    COMPACT,
    INLINE_BLOCK,
    TABLE,
    INLINE_TABLE,
    TABLE_ROW_GROUP,
    TABLE_HEADER_GROUP,
    TABLE_FOOTER_GROUP,
    TABLE_ROW,
    TABLE_COLUMN_GROUP,
    TABLE_COLUMN,
    TABLE_CELL,
    TABLE_CAPTION,
    BOX,
    INLINE_BOX,
    FLEX,
    WEBKIT_FLEX,
    INLINE_FLEX,
    WEBKIT_INLINE_FLEX,
    CONTENTS,
    GRID,
    INLINE_GRID,
    NONE
};

enum EInsideLink {
    NotInsideLink,
    InsideUnvisitedLink,
    InsideVisitedLink
};
    
enum EPointerEvents {
    PE_NONE,
    PE_AUTO,
    PE_STROKE,
    PE_FILL,
    PE_PAINTED,
    PE_VISIBLE,
    PE_VISIBLE_STROKE,
    PE_VISIBLE_FILL,
    PE_VISIBLE_PAINTED,
    PE_ALL
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

enum Hyphens {
    HyphensNone,
    HyphensManual,
    HyphensAuto
};

enum ESpeakAs {
    SpeakNormal = 0,
    SpeakSpellOut = 1 << 0,
    SpeakDigits = 1 << 1,
    SpeakLiteralPunctuation = 1 << 2,
    SpeakNoPunctuation = 1 << 3
};
inline ESpeakAs operator|(ESpeakAs a, ESpeakAs b) { return ESpeakAs(int(a) | int(b)); }
inline ESpeakAs& operator|=(ESpeakAs& a, ESpeakAs b) { return a = a | b; }

enum TextEmphasisFill {
    TextEmphasisFillFilled,
    TextEmphasisFillOpen
};

enum TextEmphasisMark {
    TextEmphasisMarkNone,
    TextEmphasisMarkAuto,
    TextEmphasisMarkDot,
    TextEmphasisMarkCircle,
    TextEmphasisMarkDoubleCircle,
    TextEmphasisMarkTriangle,
    TextEmphasisMarkSesame,
    TextEmphasisMarkCustom
};

enum TextEmphasisPositions {
    TextEmphasisPositionOver = 1 << 0,
    TextEmphasisPositionUnder = 1 << 1,
    TextEmphasisPositionLeft = 1 << 2,
    TextEmphasisPositionRight = 1 << 3
};
typedef unsigned TextEmphasisPosition;

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

enum ImageResolutionSource {
    ImageResolutionSpecified = 0,
    ImageResolutionFromImage
};

enum ImageResolutionSnap {
    ImageResolutionNoSnap = 0,
    ImageResolutionSnapPixels
};

enum Order {
    LogicalOrder = 0,
    VisualOrder
};

enum ColumnAxis {
    HorizontalColumnAxis,
    VerticalColumnAxis,
    AutoColumnAxis
};

enum ColumnProgression {
    NormalColumnProgression,
    ReverseColumnProgression
};

enum LineSnap {
    LineSnapNone,
    LineSnapBaseline,
    LineSnapContain
};

enum LineAlign {
    LineAlignNone,
    LineAlignEdges
};

enum RubyPosition {
    RubyPositionBefore,
    RubyPositionAfter,
    RubyPositionInterCharacter
};

static const size_t GridAutoFlowBits = 4;
enum InternalGridAutoFlowAlgorithm {
    InternalAutoFlowAlgorithmSparse = 0x1,
    InternalAutoFlowAlgorithmDense = 0x2,
};

enum InternalGridAutoFlowDirection {
    InternalAutoFlowDirectionRow = 0x4,
    InternalAutoFlowDirectionColumn = 0x8
};

enum GridAutoFlow {
    AutoFlowRow = InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionRow,
    AutoFlowColumn = InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionColumn,
    AutoFlowRowDense = InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionRow,
    AutoFlowColumnDense = InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionColumn
};

enum AutoRepeatType {
    NoAutoRepeat,
    AutoFill,
    AutoFit
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
enum CSSBoxType {
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
