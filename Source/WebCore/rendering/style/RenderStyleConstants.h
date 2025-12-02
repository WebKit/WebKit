/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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
#include <limits>
#include <optional>
#include <type_traits>
#include <wtf/EnumSet.h>
#include <wtf/EnumTraits.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class DumpStyleValues {
    All,
    NonInitial,
};

enum class PrintColorAdjust : bool {
    Economy,
    Exact
};

// The difference between two styles.  The following values are used:
// - StyleDifference::Equal - The two styles are identical
// - StyleDifference::RecompositeLayer - The layer needs its position and transform updated, but no repaint
// - StyleDifference::Repaint - The object just needs to be repainted.
// - StyleDifference::RepaintIfText - The object needs to be repainted if it contains text.
// - StyleDifference::RepaintLayer - The layer and its descendant layers needs to be repainted.
// - StyleDifference::LayoutOutOfFlowMovementOnly - Only the position of this out-of-flow box has been updated
// - StyleDifference::Overflow - Only overflow needs to be recomputed
// - StyleDifference::OverflowAndOutOfFlowMovement - Both out-of-flow movement and overflow updates are required.
// - StyleDifference::Layout - A full layout is required.
enum class StyleDifference : uint8_t {
    Equal,
    RecompositeLayer,
    Repaint,
    RepaintIfText,
    RepaintLayer,
    LayoutOutOfFlowMovementOnly,
    Overflow,
    OverflowAndOutOfFlowMovement,
    Layout,
    NewStyle
};

// When some style properties change, different amounts of work have to be done depending on
// context (e.g. whether the property is changing on an element which has a compositing layer).
// A simple StyleDifference does not provide enough information so we return a bit mask of
// StyleDifferenceContextSensitiveProperties from RenderStyle::diff() too.
enum class StyleDifferenceContextSensitiveProperty : uint8_t {
    Transform   = 1 << 0,
    Opacity     = 1 << 1,
    Filter      = 1 << 2,
    ClipRect    = 1 << 3,
    ClipPath    = 1 << 4,
    WillChange  = 1 << 5,
};

enum class PseudoElementType : uint8_t {
    // Public:
    FirstLine,
    FirstLetter,
    GrammarError,
    Highlight,
    Marker,
    Before,
    After,
    Selection,
    Backdrop,
    WebKitScrollbar,
    SpellingError,
    TargetText,
    ViewTransition,
    ViewTransitionGroup,
    ViewTransitionImagePair,
    ViewTransitionOld,
    ViewTransitionNew,

    // Internal:
    WebKitScrollbarThumb,
    WebKitScrollbarButton,
    WebKitScrollbarTrack,
    WebKitScrollbarTrackPiece,
    WebKitScrollbarCorner,
    WebKitResizer,
    InternalWritingSuggestions,

    HighestEnumValue = InternalWritingSuggestions
};

constexpr auto allPublicPseudoElementTypes = EnumSet {
    PseudoElementType::FirstLine,
    PseudoElementType::FirstLetter,
    PseudoElementType::GrammarError,
    PseudoElementType::Highlight,
    PseudoElementType::Marker,
    PseudoElementType::Before,
    PseudoElementType::After,
    PseudoElementType::Selection,
    PseudoElementType::Backdrop,
    PseudoElementType::WebKitScrollbar,
    PseudoElementType::SpellingError,
    PseudoElementType::TargetText,
    PseudoElementType::ViewTransition,
    PseudoElementType::ViewTransitionGroup,
    PseudoElementType::ViewTransitionImagePair,
    PseudoElementType::ViewTransitionOld,
    PseudoElementType::ViewTransitionNew
};

constexpr auto allInternalPseudoElementTypes = EnumSet {
    PseudoElementType::WebKitScrollbarThumb,
    PseudoElementType::WebKitScrollbarButton,
    PseudoElementType::WebKitScrollbarTrack,
    PseudoElementType::WebKitScrollbarTrackPiece,
    PseudoElementType::WebKitScrollbarCorner,
    PseudoElementType::WebKitResizer,
    PseudoElementType::InternalWritingSuggestions,
};

constexpr auto allPseudoElementTypes = allPublicPseudoElementTypes | allInternalPseudoElementTypes;

inline std::optional<PseudoElementType> parentPseudoElement(PseudoElementType pseudoElementType)
{
    switch (pseudoElementType) {
    case PseudoElementType::FirstLetter: return PseudoElementType::FirstLine;
    case PseudoElementType::ViewTransitionGroup: return PseudoElementType::ViewTransition;
    case PseudoElementType::ViewTransitionImagePair: return PseudoElementType::ViewTransitionGroup;
    case PseudoElementType::ViewTransitionNew: return PseudoElementType::ViewTransitionImagePair;
    case PseudoElementType::ViewTransitionOld: return PseudoElementType::ViewTransitionImagePair;
    default: return std::nullopt;
    }
}

enum class ColumnFill : bool {
    Balance,
    Auto
};

enum class ColumnSpan : bool {
    None,
    All
};

enum class BorderCollapse : bool {
    Separate,
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

enum class OutlineStyle : uint8_t {
    Auto,
    None,
    Inset,
    Groove,
    Outset,
    Ridge,
    Dotted,
    Dashed,
    Solid,
    Double
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
    None,
    Left,
    Right,
    InlineStart,
    InlineEnd,
};

enum class UsedFloat : uint8_t {
    None  = 1 << 0,
    Left  = 1 << 1,
    Right = 1 << 2
};

// Box decoration attributes. Not inherited.

enum class BoxDecorationBreak : bool {
    Slice,
    Clone
};

// Box attributes. Not inherited.

enum class BoxSizing : bool {
    ContentBox,
    BorderBox
};

// Random visual rendering model attributes. Not inherited.

enum class Overflow : uint8_t {
    Visible,
    Hidden,
    Clip,
    Scroll,
    Auto,
    PagedX,
    PagedY
};

enum class Clear : uint8_t {
    None,
    Left,
    Right,
    InlineStart,
    InlineEnd,
    Both
};

enum class UsedClear : uint8_t {
    None,
    Left,
    Right,
    Both
};

enum class TableLayoutType : bool {
    Auto,
    Fixed
};

enum class TextCombine : bool {
    None,
    All
};

enum class FillAttachment : uint8_t {
    ScrollBackground,
    LocalBackground,
    FixedBackground
};

enum class FillBox : uint8_t {
    BorderBox,
    PaddingBox,
    ContentBox,
    BorderArea,
    Text,
    NoClip
};

constexpr unsigned FillBoxBitWidth = 3;

constexpr inline FillBox clipMax(FillBox clipA, FillBox clipB)
{
    if (clipA == FillBox::BorderBox || clipB == FillBox::BorderBox)
        return FillBox::BorderBox;
    if (clipA == FillBox::PaddingBox || clipB == FillBox::PaddingBox)
        return FillBox::PaddingBox;
    if (clipA == FillBox::ContentBox || clipB == FillBox::ContentBox)
        return FillBox::ContentBox;
    return FillBox::NoClip;
}

enum class FillRepeat : uint8_t {
    Repeat,
    NoRepeat,
    Round,
    Space
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

enum class BoxOrient : bool {
    Horizontal,
    Vertical
};

enum class BoxLines : bool {
    Single,
    Multiple
};

enum class BoxDirection : bool {
    Normal,
    Reverse
};

// CSS3 Flexbox Properties

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
    Right,
    AnchorCenter,
};

enum class OverflowAlignment : uint8_t {
    Default,
    Unsafe,
    Safe
};

enum class ItemPositionType : bool {
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

enum class InputSecurity : bool {
    Auto,
    None
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
    Ratio,
    AutoAndRatio,
    AutoZero
};

enum class WordBreak : uint8_t {
    Normal,
    BreakAll,
    KeepAll,
    BreakWord,
    AutoPhrase
};

enum class OverflowWrap : uint8_t {
    Normal,
    BreakWord,
    Anywhere
};

enum class NBSPMode : bool {
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

enum class QuoteType : uint8_t {
    OpenQuote,
    CloseQuote,
    NoOpenQuote,
    NoCloseQuote
};

enum class AnimationFillMode : uint8_t {
    None,
    Forwards,
    Backwards,
    Both
};

enum class AnimationPlayState : bool {
    Running,
    Paused
};

enum class WhiteSpace : uint8_t {
    Normal,
    Pre,
    PreWrap,
    PreLine,
    NoWrap,
    BreakSpaces
};

enum class WhiteSpaceCollapse : uint8_t {
    Collapse,
    Preserve,
    PreserveBreaks,
    BreakSpaces
};

enum class ReflectionDirection : uint8_t {
    Below,
    Above,
    Left,
    Right
};

enum class TextDecorationStyle : uint8_t {
    Solid,
    Double,
    Dotted,
    Dashed,
    Wavy
};

enum class TextJustify : uint8_t {
    Auto,
    None,
    InterWord,
    InterCharacter
};

enum class TextDecorationSkipInk : uint8_t {
    None,
    Auto,
    All
};

enum class TextGroupAlign : uint8_t {
    None,
    Start,
    End,
    Left,
    Right,
    Center
};

enum class TextBoxTrim : uint8_t {
    None,
    TrimStart,
    TrimEnd,
    TrimBoth
};

enum class TextEdgeOver : uint8_t {
    Text,
    Ideographic,
    IdeographicInk,
    Cap,
    Ex
};

enum class TextEdgeUnder : uint8_t {
    Text,
    Ideographic,
    IdeographicInk,
    Alphabetic,
};

enum class TextZoom : bool {
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

enum class EmptyCell : bool {
    Show,
    Hide
};

enum class CaptionSide : uint8_t {
    Top,
    Bottom
};

enum class ListStylePosition : bool {
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
enum class CursorVisibility : bool {
    Auto,
    AutoHide,
};
#endif

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
    InlineFlex,
    Contents,
    Grid,
    InlineGrid,
    GridLanes,
    InlineGridLanes,
    FlowRoot,
    Ruby,
    RubyBlock,
    RubyBase,
    RubyAnnotation,
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
    Preserve3D,
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    Separated
#endif
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

enum class OverflowContinue : bool {
    Auto,
    Discard
};

enum class Hyphens : uint8_t {
    None,
    Manual,
    Auto
};

enum class TextEmphasisFill : bool {
    Filled,
    Open
};

enum class TextEmphasisMark : uint8_t {
    Dot,
    Circle,
    DoubleCircle,
    Triangle,
    Sesame
};

enum class TextOverflow : bool {
    Clip,
    Ellipsis
};

enum class TextWrapMode : bool {
    Wrap,
    NoWrap
};

enum class TextWrapStyle : uint8_t {
    Auto,
    Balance,
    Pretty,
    Stable
};

enum class ImageRendering : uint8_t {
    Auto = 0,
    OptimizeSpeed,
    OptimizeQuality,
    CrispEdges,
    Pixelated
};

enum class Order : bool {
    Logical,
    Visual
};

enum class ColumnAxis : uint8_t {
    Horizontal,
    Vertical,
    Auto
};

enum class ColumnProgression : bool {
    Normal,
    Reverse
};

enum class LineSnap : uint8_t {
    None,
    Baseline,
    Contain
};

enum class LineAlign : bool {
    None,
    Edges
};

enum class RubyPosition : uint8_t {
    Over,
    Under,
    InterCharacter,
    LegacyInterCharacter
};

enum class RubyAlign : uint8_t {
    Start,
    Center,
    SpaceBetween,
    SpaceAround
};

enum class RubyOverhang : bool {
    Auto,
    None
};

enum class ColorScheme : uint8_t {
    Light = 1 << 0,
    Dark = 1 << 1
};

constexpr size_t ColorSchemeBits = 2;

enum class AutoRepeatType : uint8_t {
    None,
    Fill,
    Fit
};

#if USE(FREETYPE)
// The maximum allowed font size is 32767 because `hb_position_t` is `int32_t`,
// where the first 16 bits are used to represent the integer part which effectively makes it `signed short`
constexpr float maximumAllowedFontSize = std::numeric_limits<short>::max();
#else
// Reasonable maximum to prevent insane font sizes from causing crashes on some platforms (such as Windows).
constexpr float maximumAllowedFontSize = 1000000.0f;
#endif

enum class Isolation : bool {
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

enum class ScrollSnapStrictness : bool {
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

enum class ScrollSnapStop : bool {
    Normal,
    Always,
};

enum class FontLoadingBehavior : uint8_t {
    Auto,
    Block,
    Swap,
    Fallback,
    Optional
};

enum class EventListenerRegionType : uint64_t {
    Wheel                  = 1LLU << 0,
    NonPassiveWheel        = 1LLU << 1,
    MouseClick             = 1LLU << 2,
    TouchStart             = 1LLU << 3,
    NonPassiveTouchStart   = 1LLU << 4,
    TouchEnd               = 1LLU << 5,
    NonPassiveTouchEnd     = 1LLU << 6,
    TouchCancel            = 1LLU << 7,
    NonPassiveTouchCancel  = 1LLU << 8,
    TouchMove              = 1LLU << 9,
    NonPassiveTouchMove    = 1LLU << 10,
    PointerDown            = 1LLU << 11,
    NonPassivePointerDown  = 1LLU << 12,
    PointerEnter           = 1LLU << 13,
    NonPassivePointerEnter = 1LLU << 14,
    PointerLeave           = 1LLU << 15,
    NonPassivePointerLeave = 1LLU << 16,
    PointerMove            = 1LLU << 17,
    NonPassivePointerMove  = 1LLU << 18,
    PointerOut             = 1LLU << 19,
    NonPassivePointerOut   = 1LLU << 20,
    PointerOver            = 1LLU << 21,
    NonPassivePointerOver  = 1LLU << 22,
    PointerUp              = 1LLU << 23,
    NonPassivePointerUp    = 1LLU << 24,
    MouseDown              = 1LLU << 25,
    NonPassiveMouseDown    = 1LLU << 26,
    MouseUp                = 1LLU << 27,
    NonPassiveMouseUp      = 1LLU << 28,
    MouseMove              = 1LLU << 29,
    NonPassiveMouseMove    = 1LLU << 30,
    GestureChange          = 1LLU << 31,
    NonPassiveGestureChange= 1LLU << 32,
    GestureEnd             = 1LLU << 33,
    NonPassiveGestureEnd   = 1LLU << 34,
    GestureStart           = 1LLU << 35,
    NonPassiveGestureStart = 1LLU << 36,
};

enum class MathShift : bool {
    Normal,
    Compact,
};

enum class MathStyle : bool {
    Normal,
    Compact,
};

enum class ContainerType : uint8_t {
    Normal,
    Size,
    InlineSize,
};

enum class ContainIntrinsicSizeType : uint8_t {
    None,
    Length,
    AutoAndLength,
    AutoAndNone,
};

enum class ContentVisibility : uint8_t {
    Visible,
    Auto,
    Hidden,
};

enum class BlockStepAlign : uint8_t {
    Auto,
    Center,
    Start,
    End
};

enum class BlockStepInsert : uint8_t {
    MarginBox,
    PaddingBox,
    ContentBox
};

enum class BlockStepRound : uint8_t {
    Up,
    Down,
    Nearest
};

enum class FieldSizing : bool {
    Fixed,
    Content
};

enum class NinePieceImageRule : uint8_t {
    Stretch,
    Round,
    Space,
    Repeat,
};

enum class AnimationDirection : uint8_t {
    Normal,
    Alternate,
    Reverse,
    AlternateReverse
};

enum class TransitionBehavior : bool {
    Normal,
    AllowDiscrete,
};

enum class Scroller : uint8_t {
    Nearest,
    Root,
    Self
};

enum class TextAnchor : uint8_t {
    Start,
    Middle,
    End
};

enum class ColorInterpolation : uint8_t {
    Auto,
    SRGB,
    LinearRGB
};

enum class ShapeRendering : uint8_t {
    Auto,
    OptimizeSpeed,
    CrispEdges,
    GeometricPrecision
};

enum class GlyphOrientation : uint8_t {
    Degrees0,
    Degrees90,
    Degrees180,
    Degrees270,
    Auto
};

enum class AlignmentBaseline : uint8_t {
    Baseline,
    BeforeEdge,
    TextBeforeEdge,
    Middle,
    Central,
    AfterEdge,
    TextAfterEdge,
    Ideographic,
    Alphabetic,
    Hanging,
    Mathematical
};

enum class DominantBaseline : uint8_t {
    Auto,
    UseScript,
    NoChange,
    ResetSize,
    Ideographic,
    Alphabetic,
    Hanging,
    Mathematical,
    Central,
    Middle,
    TextAfterEdge,
    TextBeforeEdge
};

enum class VectorEffect : uint8_t {
    None,
    NonScalingStroke
};

enum class BufferedRendering : uint8_t {
    Auto,
    Dynamic,
    Static
};

enum class MaskType : uint8_t {
    Luminance,
    Alpha
};

CSSBoxType transformBoxToCSSBoxType(TransformBox);

constexpr float defaultMiterLimit = 4;

enum class UsesSVGZoomRulesForLength : bool { No, Yes };

WTF::TextStream& operator<<(WTF::TextStream&, AnimationDirection);
WTF::TextStream& operator<<(WTF::TextStream&, AnimationFillMode);
WTF::TextStream& operator<<(WTF::TextStream&, AnimationPlayState);
WTF::TextStream& operator<<(WTF::TextStream&, AspectRatioType);
WTF::TextStream& operator<<(WTF::TextStream&, AutoRepeatType);
WTF::TextStream& operator<<(WTF::TextStream&, BackfaceVisibility);
WTF::TextStream& operator<<(WTF::TextStream&, BlockStepAlign);
WTF::TextStream& operator<<(WTF::TextStream&, BlockStepInsert);
WTF::TextStream& operator<<(WTF::TextStream&, BlockStepRound);
WTF::TextStream& operator<<(WTF::TextStream&, BorderCollapse);
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
WTF::TextStream& operator<<(WTF::TextStream&, UsedClear);
#if ENABLE(DARK_MODE_CSS)
WTF::TextStream& operator<<(WTF::TextStream&, ColorScheme);
#endif
WTF::TextStream& operator<<(WTF::TextStream&, ColumnAxis);
WTF::TextStream& operator<<(WTF::TextStream&, ColumnFill);
WTF::TextStream& operator<<(WTF::TextStream&, ColumnProgression);
WTF::TextStream& operator<<(WTF::TextStream&, ColumnSpan);
WTF::TextStream& operator<<(WTF::TextStream&, ContentDistribution);
WTF::TextStream& operator<<(WTF::TextStream&, ContentPosition);
WTF::TextStream& operator<<(WTF::TextStream&, ContentVisibility);
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
WTF::TextStream& operator<<(WTF::TextStream&, UsedFloat);
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
WTF::TextStream& operator<<(WTF::TextStream&, MarqueeBehavior);
WTF::TextStream& operator<<(WTF::TextStream&, MarqueeDirection);
WTF::TextStream& operator<<(WTF::TextStream&, NBSPMode);
WTF::TextStream& operator<<(WTF::TextStream&, NinePieceImageRule);
WTF::TextStream& operator<<(WTF::TextStream&, ObjectFit);
WTF::TextStream& operator<<(WTF::TextStream&, Order);
WTF::TextStream& operator<<(WTF::TextStream&, OutlineStyle);
WTF::TextStream& operator<<(WTF::TextStream&, WebCore::Overflow);
WTF::TextStream& operator<<(WTF::TextStream&, OverflowAlignment);
WTF::TextStream& operator<<(WTF::TextStream&, OverflowWrap);
WTF::TextStream& operator<<(WTF::TextStream&, PointerEvents);
WTF::TextStream& operator<<(WTF::TextStream&, PositionType);
WTF::TextStream& operator<<(WTF::TextStream&, PrintColorAdjust);
WTF::TextStream& operator<<(WTF::TextStream&, PseudoElementType);
WTF::TextStream& operator<<(WTF::TextStream&, QuoteType);
WTF::TextStream& operator<<(WTF::TextStream&, ReflectionDirection);
WTF::TextStream& operator<<(WTF::TextStream&, RubyPosition);
WTF::TextStream& operator<<(WTF::TextStream&, RubyAlign);
WTF::TextStream& operator<<(WTF::TextStream&, RubyOverhang);
WTF::TextStream& operator<<(WTF::TextStream&, ScrollSnapAxis);
WTF::TextStream& operator<<(WTF::TextStream&, ScrollSnapAxisAlignType);
WTF::TextStream& operator<<(WTF::TextStream&, ScrollSnapStop);
WTF::TextStream& operator<<(WTF::TextStream&, ScrollSnapStrictness);
WTF::TextStream& operator<<(WTF::TextStream&, Scroller);
WTF::TextStream& operator<<(WTF::TextStream&, StyleDifference);
WTF::TextStream& operator<<(WTF::TextStream&, StyleDifferenceContextSensitiveProperty);
WTF::TextStream& operator<<(WTF::TextStream&, TableLayoutType);
WTF::TextStream& operator<<(WTF::TextStream&, TextCombine);
WTF::TextStream& operator<<(WTF::TextStream&, TextDecorationSkipInk);
WTF::TextStream& operator<<(WTF::TextStream&, TextDecorationStyle);
WTF::TextStream& operator<<(WTF::TextStream&, TextEmphasisFill);
WTF::TextStream& operator<<(WTF::TextStream&, TextEmphasisMark);
WTF::TextStream& operator<<(WTF::TextStream&, TextGroupAlign);
WTF::TextStream& operator<<(WTF::TextStream&, TextJustify);
WTF::TextStream& operator<<(WTF::TextStream&, TextOverflow);
WTF::TextStream& operator<<(WTF::TextStream&, TextSecurity);
WTF::TextStream& operator<<(WTF::TextStream&, TextWrapMode);
WTF::TextStream& operator<<(WTF::TextStream&, TextWrapStyle);
WTF::TextStream& operator<<(WTF::TextStream&, TextBoxTrim);
WTF::TextStream& operator<<(WTF::TextStream&, TextEdgeOver);
WTF::TextStream& operator<<(WTF::TextStream&, TextEdgeUnder);
WTF::TextStream& operator<<(WTF::TextStream&, TextZoom);
WTF::TextStream& operator<<(WTF::TextStream&, TransformBox);
WTF::TextStream& operator<<(WTF::TextStream&, TransformStyle3D);
WTF::TextStream& operator<<(WTF::TextStream&, TransitionBehavior);
WTF::TextStream& operator<<(WTF::TextStream&, UserDrag);
WTF::TextStream& operator<<(WTF::TextStream&, UserModify);
WTF::TextStream& operator<<(WTF::TextStream&, UserSelect);
WTF::TextStream& operator<<(WTF::TextStream&, Visibility);
WTF::TextStream& operator<<(WTF::TextStream&, WhiteSpace);
WTF::TextStream& operator<<(WTF::TextStream&, WhiteSpaceCollapse);
WTF::TextStream& operator<<(WTF::TextStream&, WordBreak);
WTF::TextStream& operator<<(WTF::TextStream&, MathShift);
WTF::TextStream& operator<<(WTF::TextStream&, MathStyle);
WTF::TextStream& operator<<(WTF::TextStream&, ContainIntrinsicSizeType);
WTF::TextStream& operator<<(WTF::TextStream&, FieldSizing);
WTF::TextStream& operator<<(WTF::TextStream&, OverflowContinue);

WTF::TextStream& operator<<(WTF::TextStream&, AlignmentBaseline);
WTF::TextStream& operator<<(WTF::TextStream&, BufferedRendering);
WTF::TextStream& operator<<(WTF::TextStream&, ColorInterpolation);
WTF::TextStream& operator<<(WTF::TextStream&, DominantBaseline);
WTF::TextStream& operator<<(WTF::TextStream&, GlyphOrientation);
WTF::TextStream& operator<<(WTF::TextStream&, MaskType);
WTF::TextStream& operator<<(WTF::TextStream&, ShapeRendering);
WTF::TextStream& operator<<(WTF::TextStream&, TextAnchor);
WTF::TextStream& operator<<(WTF::TextStream&, VectorEffect);

} // namespace WebCore
