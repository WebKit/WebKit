/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderStyleConstants.h"

#include "TabSize.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, AnimationFillMode fillMode)
{
    switch (fillMode) {
    case AnimationFillMode::None: ts << "none"; break;
    case AnimationFillMode::Forwards: ts << "forwards"; break;
    case AnimationFillMode::Backwards: ts << "backwards"; break;
    case AnimationFillMode::Both: ts << "both"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AnimationPlayState playState)
{
    switch (playState) {
    case AnimationPlayState::Playing: ts << "playing"; break;
    case AnimationPlayState::Paused: ts << "paused"; break;
    }
    return ts;
}

#if ENABLE(APPLE_PAY)
TextStream& operator<<(TextStream& ts, ApplePayButtonStyle buttonStyle)
{
    switch (buttonStyle) {
    case ApplePayButtonStyle::White: ts << "white"; break;
    case ApplePayButtonStyle::WhiteOutline: ts << "white-outline"; break;
    case ApplePayButtonStyle::Black: ts << "black"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ApplePayButtonType playState)
{
    switch (playState) {
    case ApplePayButtonType::Plain: ts << "plain"; break;
    case ApplePayButtonType::Buy: ts << "buy"; break;
    case ApplePayButtonType::SetUp: ts << "setup"; break;
    case ApplePayButtonType::Donate: ts << "donate"; break;
    case ApplePayButtonType::CheckOut: ts << "checkout"; break;
    case ApplePayButtonType::Book: ts << "book"; break;
    case ApplePayButtonType::Subscribe: ts << "subscribe"; break;
#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
    case ApplePayButtonType::Reload: ts << "reload"; break;
    case ApplePayButtonType::AddMoney: ts << "add-money"; break;
    case ApplePayButtonType::TopUp: ts << "top-up"; break;
    case ApplePayButtonType::Order: ts << "order"; break;
    case ApplePayButtonType::Rent: ts << "rent"; break;
    case ApplePayButtonType::Support: ts << "support"; break;
    case ApplePayButtonType::Contribute: ts << "contribute"; break;
    case ApplePayButtonType::Tip: ts << "tip"; break;
#endif
    }
    return ts;
}
#endif

TextStream& operator<<(TextStream& ts, AspectRatioType aspectRatioType)
{
    switch (aspectRatioType) {
    case AspectRatioType::Auto: ts << "auto"; break;
    case AspectRatioType::FromIntrinsic: ts << "from-intrinsic"; break;
    case AspectRatioType::FromDimensions: ts << "from-dimensions"; break;
    case AspectRatioType::Specified: ts << "specified"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AutoRepeatType repeatType)
{
    switch (repeatType) {
    case AutoRepeatType::None: ts << "none"; break;
    case AutoRepeatType::Fill: ts << "fill"; break;
    case AutoRepeatType::Fit: ts << "fit"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BackfaceVisibility visibility)
{
    switch (visibility) {
    case BackfaceVisibility::Visible: ts << "visible"; break;
    case BackfaceVisibility::Hidden: ts << "hidden"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BorderCollapse collapse)
{
    switch (collapse) {
    case BorderCollapse::Separate: ts << "separate"; break;
    case BorderCollapse::Collapse: ts << "collapse"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BorderFit borderFit)
{
    switch (borderFit) {
    case BorderFit::Border: ts << "border"; break;
    case BorderFit::Lines: ts << "lines"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BorderStyle borderStyle)
{
    switch (borderStyle) {
    case BorderStyle::None: ts << "none"; break;
    case BorderStyle::Hidden: ts << "hidden"; break;
    case BorderStyle::Inset: ts << "inset"; break;
    case BorderStyle::Groove: ts << "groove"; break;
    case BorderStyle::Outset: ts << "outset"; break;
    case BorderStyle::Ridge: ts << "ridge"; break;
    case BorderStyle::Dotted: ts << "dotted"; break;
    case BorderStyle::Dashed: ts << "dashed"; break;
    case BorderStyle::Solid: ts << "solid"; break;
    case BorderStyle::Double: ts << "double"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxAlignment boxAlignment)
{
    switch (boxAlignment) {
    case BoxAlignment::Stretch: ts << "stretch"; break;
    case BoxAlignment::Start: ts << "start"; break;
    case BoxAlignment::Center: ts << "center"; break;
    case BoxAlignment::End: ts << "end"; break;
    case BoxAlignment::Baseline: ts << "baseline"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxDecorationBreak decorationBreak)
{
    switch (decorationBreak) {
    case BoxDecorationBreak::Slice: ts << "slice"; break;
    case BoxDecorationBreak::Clone: ts << "clone"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxDirection boxDirection)
{
    switch (boxDirection) {
    case BoxDirection::Normal: ts << "normal"; break;
    case BoxDirection::Reverse: ts << "reverse"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxLines boxLines)
{
    switch (boxLines) {
    case BoxLines::Single: ts << "single"; break;
    case BoxLines::Multiple: ts << "multiple"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxOrient boxOrient)
{
    switch (boxOrient) {
    case BoxOrient::Horizontal: ts << "horizontal"; break;
    case BoxOrient::Vertical: ts << "vertical"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxPack boxPack)
{
    switch (boxPack) {
    case BoxPack::Start: ts << "start"; break;
    case BoxPack::Center: ts << "center"; break;
    case BoxPack::End: ts << "end"; break;
    case BoxPack::Justify: ts << "justify"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxSizing boxSizing)
{
    switch (boxSizing) {
    case BoxSizing::ContentBox: ts << "content-box"; break;
    case BoxSizing::BorderBox: ts << "border-box"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BreakBetween breakBetween)
{
    switch (breakBetween) {
    case BreakBetween::Auto: ts << "auto"; break;
    case BreakBetween::Avoid: ts << "avoid"; break;
    case BreakBetween::AvoidColumn: ts << "avoid-column"; break;
    case BreakBetween::AvoidPage: ts << "avoid-page"; break;
    case BreakBetween::Column: ts << "column"; break;
    case BreakBetween::Page: ts << "page"; break;
    case BreakBetween::LeftPage: ts << "left-page"; break;
    case BreakBetween::RightPage: ts << "right-page"; break;
    case BreakBetween::RectoPage: ts << "recto-page"; break;
    case BreakBetween::VersoPage: ts << "verso-page"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BreakInside breakInside)
{
    switch (breakInside) {
    case BreakInside::Auto: ts << "auto"; break;
    case BreakInside::Avoid: ts << "avoid"; break;
    case BreakInside::AvoidColumn: ts << "avoidColumn"; break;
    case BreakInside::AvoidPage: ts << "avoidPage"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CSSBoxType boxType)
{
    switch (boxType) {
    case CSSBoxType::BoxMissing: ts << "missing"; break;
    case CSSBoxType::MarginBox: ts << "margin-box"; break;
    case CSSBoxType::BorderBox: ts << "border-box"; break;
    case CSSBoxType::PaddingBox: ts << "padding-box"; break;
    case CSSBoxType::ContentBox: ts << "content-box"; break;
    case CSSBoxType::FillBox: ts << "fill-box"; break;
    case CSSBoxType::StrokeBox: ts << "stroke-box"; break;
    case CSSBoxType::ViewBox: ts << "view-box"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CaptionSide side)
{
    switch (side) {
    case CaptionSide::Top: ts << "top"; break;
    case CaptionSide::Bottom: ts << "bottom"; break;
    case CaptionSide::Left: ts << "left"; break;
    case CaptionSide::Right: ts << "right"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Clear clear)
{
    switch (clear) {
    case Clear::None: ts << "none"; break;
    case Clear::Left: ts << "left"; break;
    case Clear::Right: ts << "right"; break;
    case Clear::Both: ts << "both"; break;
    }
    return ts;
}

#if ENABLE(DARK_MODE_CSS)
TextStream& operator<<(TextStream& ts, ColorScheme colorScheme)
{
    switch (colorScheme) {
    case ColorScheme::Light: ts << "light"; break;
    case ColorScheme::Dark: ts << "dark"; break;
    }
    return ts;
}
#endif

TextStream& operator<<(TextStream& ts, ColumnAxis axis)
{
    switch (axis) {
    case ColumnAxis::Horizontal: ts << "horizontal"; break;
    case ColumnAxis::Vertical: ts << "vertical"; break;
    case ColumnAxis::Auto: ts << "auto"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColumnFill fill)
{
    switch (fill) {
    case ColumnFill::Auto: ts << "auto"; break;
    case ColumnFill::Balance: ts << "balance"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColumnProgression progression)
{
    switch (progression) {
    case ColumnProgression::Normal: ts << "normal"; break;
    case ColumnProgression::Reverse: ts << "reverse"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColumnSpan span)
{
    switch (span) {
    case ColumnSpan::None: ts << "none"; break;
    case ColumnSpan::All: ts << "all"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ContentDistribution distribution)
{
    switch (distribution) {
    case ContentDistribution::Default: ts << "default"; break;
    case ContentDistribution::SpaceBetween: ts << "space-between"; break;
    case ContentDistribution::SpaceAround: ts << "space-around"; break;
    case ContentDistribution::SpaceEvenly: ts << "space-evenly"; break;
    case ContentDistribution::Stretch: ts << "stretch"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ContentPosition position)
{
    switch (position) {
    case ContentPosition::Normal: ts << "normal"; break;
    case ContentPosition::Baseline: ts << "baseline"; break;
    case ContentPosition::LastBaseline: ts << "last-baseline"; break;
    case ContentPosition::Center: ts << "center"; break;
    case ContentPosition::Start: ts << "start"; break;
    case ContentPosition::End: ts << "end"; break;
    case ContentPosition::FlexStart: ts << "flex-start"; break;
    case ContentPosition::FlexEnd: ts << "flex-end"; break;
    case ContentPosition::Left: ts << "left"; break;
    case ContentPosition::Right: ts << "right"; break;

    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CursorType cursor)
{
    switch (cursor) {
    case CursorType::Auto: ts << "auto"; break;
    case CursorType::Default: ts << "default"; break;
    case CursorType::ContextMenu: ts << "contextmenu"; break;
    case CursorType::Help: ts << "help"; break;
    case CursorType::Pointer: ts << "pointer"; break;
    case CursorType::Progress: ts << "progress"; break;
    case CursorType::Wait: ts << "wait"; break;
    case CursorType::Cell: ts << "cell"; break;
    case CursorType::Crosshair: ts << "crosshair"; break;
    case CursorType::Text: ts << "text"; break;
    case CursorType::VerticalText: ts << "vertical-text"; break;
    case CursorType::Alias: ts << "alias"; break;
    case CursorType::Move: ts << "move"; break;
    case CursorType::NoDrop: ts << "nodrop"; break;
    case CursorType::NotAllowed: ts << "not-allowed"; break;
    case CursorType::Grab: ts << "grab"; break;
    case CursorType::Grabbing: ts << "grabbing"; break;
    case CursorType::EResize: ts << "e-resize"; break;
    case CursorType::NResize: ts << "n-resize"; break;
    case CursorType::NEResize: ts << "ne-resize"; break;
    case CursorType::NWResize: ts << "nw-resize"; break;
    case CursorType::SResize: ts << "sr-esize"; break;
    case CursorType::SEResize: ts << "se-resize"; break;
    case CursorType::SWResize: ts << "sw-resize"; break;
    case CursorType::WResize: ts << "w-resize"; break;
    case CursorType::EWResize: ts << "ew-resize"; break;
    case CursorType::NSResize: ts << "ns-resize"; break;
    case CursorType::NESWResize: ts << "nesw-resize"; break;
    case CursorType::NWSEResize: ts << "nwse-resize"; break;
    case CursorType::ColumnResize: ts << "column-resize"; break;
    case CursorType::RowResize: ts << "row-resize"; break;
    case CursorType::AllScroll: ts << "all-scroll"; break;
    case CursorType::ZoomIn: ts << "zoom-in"; break;
    case CursorType::ZoomOut: ts << "zoom-out"; break;
    case CursorType::Copy: ts << "copy"; break;
    case CursorType::None: ts << "none"; break;
    }
    return ts;
}

#if ENABLE(CURSOR_VISIBILITY)
TextStream& operator<<(TextStream& ts, CursorVisibility visibility)
{
    switch (visibility) {
    case CursorVisibility::Auto: ts << "auto"; break;
    case CursorVisibility::AutoHide: ts << "autohide"; break;
    }
    return ts;
}
#endif

TextStream& operator<<(TextStream& ts, DisplayType display)
{
    switch (display) {
    case DisplayType::Inline: ts << "inline"; break;
    case DisplayType::Block: ts << "block"; break;
    case DisplayType::ListItem: ts << "list-item"; break;
    case DisplayType::InlineBlock: ts << "inline-block"; break;
    case DisplayType::Table: ts << "table"; break;
    case DisplayType::InlineTable: ts << "inline-table"; break;
    case DisplayType::TableRowGroup: ts << "table-row-group"; break;
    case DisplayType::TableHeaderGroup: ts << "table-header-group"; break;
    case DisplayType::TableFooterGroup: ts << "table-footer-group"; break;
    case DisplayType::TableRow: ts << "table-row"; break;
    case DisplayType::TableColumnGroup: ts << "table-column-group"; break;
    case DisplayType::TableColumn: ts << "table-column"; break;
    case DisplayType::TableCell: ts << "table-cell"; break;
    case DisplayType::TableCaption: ts << "table-caption"; break;
    case DisplayType::Box: ts << "box"; break;
    case DisplayType::InlineBox: ts << "inline-box"; break;
    case DisplayType::Flex: ts << "flex"; break;
    case DisplayType::WebKitFlex: ts << "web-kit-flex"; break;
    case DisplayType::InlineFlex: ts << "inline-flex"; break;
    case DisplayType::WebKitInlineFlex: ts << "web-kit-inline-flex"; break;
    case DisplayType::Contents: ts << "contents"; break;
    case DisplayType::Grid: ts << "grid"; break;
    case DisplayType::InlineGrid: ts << "inline-grid"; break;
    case DisplayType::FlowRoot: ts << "flow-root"; break;
    case DisplayType::None: ts << "none"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Edge edge)
{
    switch (edge) {
    case Edge::Top: ts << "top"; break;
    case Edge::Right: ts << "right"; break;
    case Edge::Bottom: ts << "bottom"; break;
    case Edge::Left: ts << "left"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, EmptyCell emptyCell)
{
    switch (emptyCell) {
    case EmptyCell::Show: ts << "show"; break;
    case EmptyCell::Hide: ts << "hide"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, EventListenerRegionType listenerType)
{
    switch (listenerType) {
    case EventListenerRegionType::Wheel: ts << "wheel"; break;
    case EventListenerRegionType::NonPassiveWheel: ts << "active wheel"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillAttachment attachment)
{
    switch (attachment) {
    case FillAttachment::ScrollBackground: ts << "scroll"; break;
    case FillAttachment::LocalBackground: ts << "local"; break;
    case FillAttachment::FixedBackground: ts << "fixed"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillBox fill)
{
    switch (fill) {
    case FillBox::Border: ts << "border"; break;
    case FillBox::Padding: ts << "padding"; break;
    case FillBox::Content: ts << "content"; break;
    case FillBox::Text: ts << "text"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillRepeat repeat)
{
    switch (repeat) {
    case FillRepeat::Repeat: ts << "repeat"; break;
    case FillRepeat::NoRepeat: ts << "no-repeat"; break;
    case FillRepeat::Round: ts << "round"; break;
    case FillRepeat::Space: ts << "space"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillSizeType sizeType)
{
    switch (sizeType) {
    case FillSizeType::Contain: ts << "contain"; break;
    case FillSizeType::Cover: ts << "cover"; break;
    case FillSizeType::Size: ts << "size-length"; break;
    case FillSizeType::None: ts << "size-none"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, FlexDirection flexDirection)
{
    switch (flexDirection) {
    case FlexDirection::Row: ts << "row"; break;
    case FlexDirection::RowReverse: ts << "row-reverse"; break;
    case FlexDirection::Column: ts << "column"; break;
    case FlexDirection::ColumnReverse: ts << "column-reverse"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FlexWrap flexWrap)
{
    switch (flexWrap) {
    case FlexWrap::NoWrap: ts << "no-wrap"; break;
    case FlexWrap::Wrap: ts << "wrap"; break;
    case FlexWrap::Reverse: ts << "reverse"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Float floating)
{
    switch (floating) {
    case Float::No: ts << "none"; break;
    case Float::Left: ts << "left"; break;
    case Float::Right: ts << "right"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, GridAutoFlow gridAutoFlow)
{
    switch (gridAutoFlow) {
    case AutoFlowRow: ts << "row"; break;
    case AutoFlowColumn: ts << "column"; break;
    case AutoFlowRowDense: ts << "row-dense"; break;
    case AutoFlowColumnDense: ts << "column-dense"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, HangingPunctuation punctuation)
{
    switch (punctuation) {
    case HangingPunctuation::None: ts << "none"; break;
    case HangingPunctuation::First: ts << "first"; break;
    case HangingPunctuation::Last: ts << "last"; break;
    case HangingPunctuation::AllowEnd: ts << "allow-end"; break;
    case HangingPunctuation::ForceEnd: ts << "force-end"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Hyphens hyphens)
{
    switch (hyphens) {
    case Hyphens::None: ts << "none"; break;
    case Hyphens::Manual: ts << "manual"; break;
    case Hyphens::Auto: ts << "auto"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ImageRendering imageRendering)
{
    switch (imageRendering) {
    case ImageRendering::Auto: ts << "auto"; break;
    case ImageRendering::OptimizeSpeed: ts << "optimizeSpeed"; break;
    case ImageRendering::OptimizeQuality: ts << "optimizeQuality"; break;
    case ImageRendering::CrispEdges: ts << "crispEdges"; break;
    case ImageRendering::Pixelated: ts << "pixelated"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, InsideLink inside)
{
    switch (inside) {
    case InsideLink::NotInside: ts << "not-inside"; break;
    case InsideLink::InsideUnvisited: ts << "inside-unvisited"; break;
    case InsideLink::InsideVisited: ts << "inside-visited"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Isolation isolation)
{
    switch (isolation) {
    case Isolation::Auto: ts << "auto"; break;
    case Isolation::Isolate: ts << "isolate"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ItemPosition position)
{
    switch (position) {
    case ItemPosition::Legacy: ts << "legacy"; break;
    case ItemPosition::Auto: ts << "auto"; break;
    case ItemPosition::Normal: ts << "normal"; break;
    case ItemPosition::Stretch: ts << "stretch"; break;
    case ItemPosition::Baseline: ts << "baseline"; break;
    case ItemPosition::LastBaseline: ts << "last-baseline"; break;
    case ItemPosition::Center: ts << "center"; break;
    case ItemPosition::Start: ts << "start"; break;
    case ItemPosition::End: ts << "end"; break;
    case ItemPosition::SelfStart: ts << "self-start"; break;
    case ItemPosition::SelfEnd: ts << "self-end"; break;
    case ItemPosition::FlexStart: ts << "flex-start"; break;
    case ItemPosition::FlexEnd: ts << "flex-end"; break;
    case ItemPosition::Left: ts << "left"; break;
    case ItemPosition::Right: ts << "right"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ItemPositionType positionType)
{
    switch (positionType) {
    case ItemPositionType::NonLegacy: ts << "non-legacy"; break;
    case ItemPositionType::Legacy: ts << "legacy"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineAlign align)
{
    switch (align) {
    case LineAlign::None: ts << "none"; break;
    case LineAlign::Edges: ts << "edges"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineBreak lineBreak)
{
    switch (lineBreak) {
    case LineBreak::Auto: ts << "auto"; break;
    case LineBreak::Loose: ts << "loose"; break;
    case LineBreak::Normal: ts << "normal"; break;
    case LineBreak::Strict: ts << "strict"; break;
    case LineBreak::AfterWhiteSpace: ts << "after-whiteSpace"; break;
    case LineBreak::Anywhere: ts << "anywhere"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineSnap lineSnap)
{
    switch (lineSnap) {
    case LineSnap::None: ts << "none"; break;
    case LineSnap::Baseline: ts << "baseline"; break;
    case LineSnap::Contain: ts << "contain"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ListStylePosition position)
{
    switch (position) {
    case ListStylePosition::Outside: ts << "outside"; break;
    case ListStylePosition::Inside: ts << "inside"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ListStyleType styleType)
{
    switch (styleType) {
    case ListStyleType::Disc: ts << "disc"; break;
    case ListStyleType::Circle: ts << "circle"; break;
    case ListStyleType::Square: ts << "square"; break;
    case ListStyleType::Decimal: ts << "decimal"; break;
    case ListStyleType::DecimalLeadingZero: ts << "decimal-leading-zero"; break;
    case ListStyleType::ArabicIndic: ts << "arabic-indic"; break;
    case ListStyleType::Binary: ts << "binary"; break;
    case ListStyleType::Bengali: ts << "bengali"; break;
    case ListStyleType::Cambodian: ts << "cambodian"; break;
    case ListStyleType::Khmer: ts << "khmer"; break;
    case ListStyleType::Devanagari: ts << "devanagari"; break;
    case ListStyleType::Gujarati: ts << "gujarati"; break;
    case ListStyleType::Gurmukhi: ts << "gurmukhi"; break;
    case ListStyleType::Kannada: ts << "kannada"; break;
    case ListStyleType::LowerHexadecimal: ts << "lower-hexadecimal"; break;
    case ListStyleType::Lao: ts << "lao"; break;
    case ListStyleType::Malayalam: ts << "malayalam"; break;
    case ListStyleType::Mongolian: ts << "mongolian"; break;
    case ListStyleType::Myanmar: ts << "myanmar"; break;
    case ListStyleType::Octal: ts << "octal"; break;
    case ListStyleType::Oriya: ts << "oriya"; break;
    case ListStyleType::Persian: ts << "persian"; break;
    case ListStyleType::Urdu: ts << "urdu"; break;
    case ListStyleType::Telugu: ts << "telugu"; break;
    case ListStyleType::Tibetan: ts << "tibetan"; break;
    case ListStyleType::Thai: ts << "thai"; break;
    case ListStyleType::UpperHexadecimal: ts << "upper-hexadecimal"; break;
    case ListStyleType::LowerRoman: ts << "lower-roman"; break;
    case ListStyleType::UpperRoman: ts << "upper-roman"; break;
    case ListStyleType::LowerGreek: ts << "lower-greek"; break;
    case ListStyleType::LowerAlpha: ts << "lower-alpha"; break;
    case ListStyleType::LowerLatin: ts << "lower-latin"; break;
    case ListStyleType::UpperAlpha: ts << "upper-alpha"; break;
    case ListStyleType::UpperLatin: ts << "upper-latin"; break;
    case ListStyleType::Afar: ts << "afar"; break;
    case ListStyleType::EthiopicHalehameAaEt: ts << "ethiopic-halehame-aa-et"; break;
    case ListStyleType::EthiopicHalehameAaEr: ts << "ethiopic-halehame-aa-er"; break;
    case ListStyleType::Amharic: ts << "amharic"; break;
    case ListStyleType::EthiopicHalehameAmEt: ts << "ethiopic-halehame-am-et"; break;
    case ListStyleType::AmharicAbegede: ts << "amharic-abegede"; break;
    case ListStyleType::EthiopicAbegedeAmEt: ts << "ethiopic-abegede-am-et"; break;
    case ListStyleType::CjkEarthlyBranch: ts << "cjk-earthly-branch"; break;
    case ListStyleType::CjkHeavenlyStem: ts << "cjk-heavenly-stem"; break;
    case ListStyleType::Ethiopic: ts << "ethiopic"; break;
    case ListStyleType::EthiopicHalehameGez: ts << "ethiopic-halehame-gez"; break;
    case ListStyleType::EthiopicAbegede: ts << "ethiopic-abegede"; break;
    case ListStyleType::EthiopicAbegedeGez: ts << "ethiopic-abegede-gez"; break;
    case ListStyleType::HangulConsonant: ts << "hangul-consonant"; break;
    case ListStyleType::Hangul: ts << "hangul"; break;
    case ListStyleType::LowerNorwegian: ts << "lower-norwegian"; break;
    case ListStyleType::Oromo: ts << "oromo"; break;
    case ListStyleType::EthiopicHalehameOmEt: ts << "ethiopic-halehame-om-et"; break;
    case ListStyleType::Sidama: ts << "sidama"; break;
    case ListStyleType::EthiopicHalehameSidEt: ts << "ethiopic-halehame-sid-et"; break;
    case ListStyleType::Somali: ts << "somali"; break;
    case ListStyleType::EthiopicHalehameSoEt: ts << "ethiopic-halehame-so-et"; break;
    case ListStyleType::Tigre: ts << "tigre"; break;
    case ListStyleType::EthiopicHalehameTig: ts << "ethiopic-halehame-tig"; break;
    case ListStyleType::TigrinyaEr: ts << "tigrinya-er"; break;
    case ListStyleType::EthiopicHalehameTiEr: ts << "ethiopic-halehame-ti-er"; break;
    case ListStyleType::TigrinyaErAbegede: ts << "tigrinya-er-abegede"; break;
    case ListStyleType::EthiopicAbegedeTiEr: ts << "ethiopic-abegede-ti-er"; break;
    case ListStyleType::TigrinyaEt: ts << "tigrinya-et"; break;
    case ListStyleType::EthiopicHalehameTiEt: ts << "ethiopic-halehame-ti-et"; break;
    case ListStyleType::TigrinyaEtAbegede: ts << "tigrinya-et-abegede"; break;
    case ListStyleType::EthiopicAbegedeTiEt: ts << "ethiopic-abegede-ti-et"; break;
    case ListStyleType::UpperGreek: ts << "upper-greek"; break;
    case ListStyleType::UpperNorwegian: ts << "upper-norwegian"; break;
    case ListStyleType::Asterisks: ts << "asterisks"; break;
    case ListStyleType::Footnotes: ts << "footnotes"; break;
    case ListStyleType::Hebrew: ts << "hebrew"; break;
    case ListStyleType::Armenian: ts << "armenian"; break;
    case ListStyleType::LowerArmenian: ts << "lower-armenian"; break;
    case ListStyleType::UpperArmenian: ts << "upper-armenian"; break;
    case ListStyleType::Georgian: ts << "georgian"; break;
    case ListStyleType::CJKIdeographic: ts << "cjk-ideographic"; break;
    case ListStyleType::Hiragana: ts << "hiragana"; break;
    case ListStyleType::Katakana: ts << "katakana"; break;
    case ListStyleType::HiraganaIroha: ts << "hiragana-iroha"; break;
    case ListStyleType::KatakanaIroha: ts << "katakana-iroha"; break;
    case ListStyleType::None: ts << "none"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MarginCollapse collapse)
{
    switch (collapse) {
    case MarginCollapse::Collapse: ts << "collapse"; break;
    case MarginCollapse::Separate: ts << "separate"; break;
    case MarginCollapse::Discard: ts << "discard"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MarqueeBehavior marqueeBehavior)
{
    switch (marqueeBehavior) {
    case MarqueeBehavior::None: ts << "none"; break;
    case MarqueeBehavior::Scroll: ts << "scroll"; break;
    case MarqueeBehavior::Slide: ts << "slide"; break;
    case MarqueeBehavior::Alternate: ts << "alternate"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MarqueeDirection marqueeDirection)
{
    switch (marqueeDirection) {
    case MarqueeDirection::Auto: ts << "auto"; break;
    case MarqueeDirection::Left: ts << "left"; break;
    case MarqueeDirection::Right: ts << "right"; break;
    case MarqueeDirection::Up: ts << "up"; break;
    case MarqueeDirection::Down: ts << "down"; break;
    case MarqueeDirection::Forward: ts << "forward"; break;
    case MarqueeDirection::Backward: ts << "backward"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MaskSourceType maskSource)
{
    switch (maskSource) {
    case MaskSourceType::Alpha: ts << "alpha"; break;
    case MaskSourceType::Luminance: ts << "luminance"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, NBSPMode mode)
{
    switch (mode) {
    case NBSPMode::Normal: ts << "normal"; break;
    case NBSPMode::Space: ts << "space"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ObjectFit objectFit)
{
    switch (objectFit) {
    case ObjectFit::Fill: ts << "fill"; break;
    case ObjectFit::Contain: ts << "contain"; break;
    case ObjectFit::Cover: ts << "cover"; break;
    case ObjectFit::None: ts << "none"; break;
    case ObjectFit::ScaleDown: ts << "scale-down"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Order order)
{
    switch (order) {
    case Order::Logical: ts << "logical"; break;
    case Order::Visual: ts << "visual"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Overflow overflow)
{
    switch (overflow) {
    case Overflow::Visible: ts << "visible"; break;
    case Overflow::Hidden: ts << "hidden"; break;
    case Overflow::Scroll: ts << "scroll"; break;
    case Overflow::Auto: ts << "auto"; break;
    case Overflow::PagedX: ts << "paged-x"; break;
    case Overflow::PagedY: ts << "paged-y"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, OverflowAlignment alignment)
{
    switch (alignment) {
    case OverflowAlignment::Default: ts << "default"; break;
    case OverflowAlignment::Unsafe: ts << "unsafe"; break;
    case OverflowAlignment::Safe: ts << "safe"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, OverflowWrap overflowWrap)
{
    switch (overflowWrap) {
    case OverflowWrap::Normal: ts << "normal"; break;
    case OverflowWrap::Break: ts << "break"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PaintOrder paintOrder)
{
    switch (paintOrder) {
    case PaintOrder::Normal: ts << "normal"; break;
    case PaintOrder::Fill: ts << "fill"; break;
    case PaintOrder::FillMarkers: ts << "fill markers"; break;
    case PaintOrder::Stroke: ts << "stroke"; break;
    case PaintOrder::StrokeMarkers: ts << "stroke markers"; break;
    case PaintOrder::Markers: ts << "markers"; break;
    case PaintOrder::MarkersStroke: ts << "markers stroke"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PointerEvents pointerEvents)
{
    switch (pointerEvents) {
    case PointerEvents::None: ts << "none"; break;
    case PointerEvents::Auto: ts << "auto"; break;
    case PointerEvents::Stroke: ts << "stroke"; break;
    case PointerEvents::Fill: ts << "fill"; break;
    case PointerEvents::Painted: ts << "painted"; break;
    case PointerEvents::Visible: ts << "visible"; break;
    case PointerEvents::BoundingBox: ts << "bounding-box"; break;
    case PointerEvents::VisibleStroke: ts << "visible-stroke"; break;
    case PointerEvents::VisibleFill: ts << "visible-fill"; break;
    case PointerEvents::VisiblePainted: ts << "visible-painted"; break;
    case PointerEvents::All: ts << "all"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PositionType position)
{
    switch (position) {
    case PositionType::Static: ts << "static"; break;
    case PositionType::Relative: ts << "relative"; break;
    case PositionType::Absolute: ts << "absolute"; break;
    case PositionType::Sticky: ts << "sticky"; break;
    case PositionType::Fixed: ts << "fixed"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PrintColorAdjust colorAdjust)
{
    switch (colorAdjust) {
    case PrintColorAdjust::Economy: ts << "economy"; break;
    case PrintColorAdjust::Exact: ts << "exact"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PseudoId pseudoId)
{
    switch (pseudoId) {
    case PseudoId::None: ts << "none"; break;
    case PseudoId::FirstLine: ts << "first-line"; break;
    case PseudoId::FirstLetter: ts << "first-letter"; break;
    case PseudoId::Highlight: ts << "highlight"; break;
    case PseudoId::Marker: ts << "marker"; break;
    case PseudoId::Before: ts << "before"; break;
    case PseudoId::After: ts << "after"; break;
    case PseudoId::Selection: ts << "selection"; break;
    case PseudoId::Scrollbar: ts << "scrollbar"; break;
    case PseudoId::ScrollbarThumb: ts << "scrollbar-thumb"; break;
    case PseudoId::ScrollbarButton: ts << "scrollbar-button"; break;
    case PseudoId::ScrollbarTrack: ts << "scrollbar-track"; break;
    case PseudoId::ScrollbarTrackPiece: ts << "scrollbar-trackpiece"; break;
    case PseudoId::ScrollbarCorner: ts << "scrollbar-corner"; break;
    case PseudoId::Resizer: ts << "resizer"; break;
    default:
        ts << "other";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, QuoteType quoteType)
{
    switch (quoteType) {
    case QuoteType::OpenQuote: ts << "open"; break;
    case QuoteType::CloseQuote: ts << "close"; break;
    case QuoteType::NoOpenQuote: ts << "no-open"; break;
    case QuoteType::NoCloseQuote: ts << "no-close"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ReflectionDirection direction)
{
    switch (direction) {
    case ReflectionDirection::Below: ts << "below"; break;
    case ReflectionDirection::Above: ts << "above"; break;
    case ReflectionDirection::Left: ts << "left"; break;
    case ReflectionDirection::Right: ts << "right"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Resize resize)
{
    switch (resize) {
    case Resize::None: ts << "none"; break;
    case Resize::Both: ts << "both"; break;
    case Resize::Horizontal: ts << "horizontal"; break;
    case Resize::Vertical: ts << "vertical"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, RubyPosition position)
{
    switch (position) {
    case RubyPosition::Before: ts << "before"; break;
    case RubyPosition::After: ts << "after"; break;
    case RubyPosition::InterCharacter: ts << "inter-character"; break;
    }
    return ts;
}

#if ENABLE(CSS_SCROLL_SNAP)
TextStream& operator<<(TextStream& ts, ScrollSnapAxis axis)
{
    switch (axis) {
    case ScrollSnapAxis::XAxis: ts << "x-axis"; break;
    case ScrollSnapAxis::YAxis: ts << "y-Axis"; break;
    case ScrollSnapAxis::Block: ts << "block"; break;
    case ScrollSnapAxis::Inline: ts << "inline"; break;
    case ScrollSnapAxis::Both: ts << "both"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollSnapAxisAlignType alignType)
{
    switch (alignType) {
    case ScrollSnapAxisAlignType::None: ts << "none"; break;
    case ScrollSnapAxisAlignType::Start: ts << "start"; break;
    case ScrollSnapAxisAlignType::Center: ts << "center"; break;
    case ScrollSnapAxisAlignType::End: ts << "end"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollSnapStrictness strictness)
{
    switch (strictness) {
    case ScrollSnapStrictness::None: ts << "none"; break;
    case ScrollSnapStrictness::Proximity: ts << "proximity"; break;
    case ScrollSnapStrictness::Mandatory: ts << "mandatory"; break;
    }
    return ts;
}
#endif

TextStream& operator<<(TextStream& ts, SpeakAs speakAs)
{
    switch (speakAs) {
    case SpeakAs::Normal: ts << "normal"; break;
    case SpeakAs::SpellOut: ts << "spell-out"; break;
    case SpeakAs::Digits: ts << "digits"; break;
    case SpeakAs::LiteralPunctuation: ts << "literal-punctuation"; break;
    case SpeakAs::NoPunctuation: ts << "no-punctuation"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, StyleDifference diff)
{
    switch (diff) {
    case StyleDifference::Equal: ts << "equal"; break;
    case StyleDifference::RecompositeLayer: ts << "recomposite layer"; break;
    case StyleDifference::Repaint: ts << "repaint"; break;
    case StyleDifference::RepaintIfTextOrBorderOrOutline: ts << "repaint if text or border or outline"; break;
    case StyleDifference::RepaintLayer: ts << "repaint layer"; break;
    case StyleDifference::LayoutPositionedMovementOnly: ts << "layout positioned movement only"; break;
    case StyleDifference::SimplifiedLayout: ts << "simplified layout"; break;
    case StyleDifference::SimplifiedLayoutAndPositionedMovement: ts << "simplified layout and positioned movement"; break;
    case StyleDifference::Layout: ts << "layout"; break;
    case StyleDifference::NewStyle: ts << "new style"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TableLayoutType layoutType)
{
    switch (layoutType) {
    case TableLayoutType::Auto: ts << "Auto"; break;
    case TableLayoutType::Fixed: ts << "Fixed"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextAlignMode alignMode)
{
    switch (alignMode) {
    case TextAlignMode::Left: ts << "left"; break;
    case TextAlignMode::Right: ts << "right"; break;
    case TextAlignMode::Center: ts << "center"; break;
    case TextAlignMode::Justify: ts << "justify"; break;
    case TextAlignMode::WebKitLeft: ts << "webkit-left"; break;
    case TextAlignMode::WebKitRight: ts << "webkit-right"; break;
    case TextAlignMode::WebKitCenter: ts << "webkit-center"; break;
    case TextAlignMode::Start: ts << "start"; break;
    case TextAlignMode::End: ts << "end"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextCombine textCombine)
{
    switch (textCombine) {
    case TextCombine::None: ts << "none"; break;
    case TextCombine::Horizontal: ts << "horizontal"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextDecoration textDecoration)
{
    switch (textDecoration) {
    case TextDecoration::None: ts << "none"; break;
    case TextDecoration::Underline: ts << "underline"; break;
    case TextDecoration::Overline: ts << "overline"; break;
    case TextDecoration::LineThrough: ts << "line-through"; break;
    case TextDecoration::Blink: ts << "blink"; break;
#if ENABLE(LETTERPRESS)
    case TextDecoration::Letterpress: ts << "letterpress"; break;
#endif
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextDecorationSkip skip)
{
    switch (skip) {
    case TextDecorationSkip::None: ts << "none"; break;
    case TextDecorationSkip::Ink: ts << "ink"; break;
    case TextDecorationSkip::Objects: ts << "objects"; break;
    case TextDecorationSkip::Auto: ts << "auto"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextDecorationStyle decorationStyle)
{
    switch (decorationStyle) {
    case TextDecorationStyle::Solid: ts << "solid"; break;
    case TextDecorationStyle::Double: ts << "double"; break;
    case TextDecorationStyle::Dotted: ts << "dotted"; break;
    case TextDecorationStyle::Dashed: ts << "dashed"; break;
    case TextDecorationStyle::Wavy: ts << "wavy"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextEmphasisFill fill)
{
    switch (fill) {
    case TextEmphasisFill::Filled: ts << "filled"; break;
    case TextEmphasisFill::Open: ts << "open"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextEmphasisMark mark)
{
    switch (mark) {
    case TextEmphasisMark::None: ts << "none"; break;
    case TextEmphasisMark::Auto: ts << "auto"; break;
    case TextEmphasisMark::Dot: ts << "dot"; break;
    case TextEmphasisMark::Circle: ts << "circle"; break;
    case TextEmphasisMark::DoubleCircle: ts << "double-circle"; break;
    case TextEmphasisMark::Triangle: ts << "triangle"; break;
    case TextEmphasisMark::Sesame: ts << "sesame"; break;
    case TextEmphasisMark::Custom: ts << "custom"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextEmphasisPosition position)
{
    switch (position) {
    case TextEmphasisPosition::Over: ts << "Over"; break;
    case TextEmphasisPosition::Under: ts << "Under"; break;
    case TextEmphasisPosition::Left: ts << "Left"; break;
    case TextEmphasisPosition::Right: ts << "Right"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextOrientation orientation)
{
    switch (orientation) {
    case TextOrientation::Mixed: ts << "mixed"; break;
    case TextOrientation::Upright: ts << "upright"; break;
    case TextOrientation::Sideways: ts << "sideways"; break;
    }
    return ts;
}
TextStream& operator<<(TextStream& ts, TextOverflow overflow)
{
    switch (overflow) {
    case TextOverflow::Clip: ts << "clip"; break;
    case TextOverflow::Ellipsis: ts << "ellipsis"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextSecurity textSecurity)
{
    switch (textSecurity) {
    case TextSecurity::None: ts << "none"; break;
    case TextSecurity::Disc: ts << "disc"; break;
    case TextSecurity::Circle: ts << "circle"; break;
    case TextSecurity::Square: ts << "square"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextTransform textTransform)
{
    switch (textTransform) {
    case TextTransform::Capitalize: ts << "capitalize"; break;
    case TextTransform::Uppercase: ts << "uppercase"; break;
    case TextTransform::Lowercase: ts << "lowercase"; break;
    case TextTransform::None: ts << "none"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextUnderlinePosition underlinePosition)
{
    switch (underlinePosition) {
    case TextUnderlinePosition::Auto: ts << "Auto"; break;
    case TextUnderlinePosition::Under: ts << "Under"; break;
    case TextUnderlinePosition::FromFont: ts << "FromFont"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextZoom zoom)
{
    switch (zoom) {
    case TextZoom::Normal: ts << "normal"; break;
    case TextZoom::Reset: ts << "reset"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TransformBox box)
{
    switch (box) {
    case TransformBox::BorderBox: ts << "border-box"; break;
    case TransformBox::FillBox: ts << "fill-box"; break;
    case TransformBox::ViewBox: ts << "view-box"; break;
    case TransformBox::StrokeBox: ts << "stroke-box"; break;
    case TransformBox::ContentBox: ts << "content-box"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TransformStyle3D transformStyle)
{
    switch (transformStyle) {
    case TransformStyle3D::Flat: ts << "flat"; break;
    case TransformStyle3D::Preserve3D: ts << "preserve-3d"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UserDrag userDrag)
{
    switch (userDrag) {
    case UserDrag::Auto: ts << "auto"; break;
    case UserDrag::None: ts << "none"; break;
    case UserDrag::Element: ts << "element"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UserModify userModify)
{
    switch (userModify) {
    case UserModify::ReadOnly: ts << "read-only"; break;
    case UserModify::ReadWrite: ts << "read-write"; break;
    case UserModify::ReadWritePlaintextOnly: ts << "read-write plaintext only"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UserSelect userSelect)
{
    switch (userSelect) {
    case UserSelect::None: ts << "none"; break;
    case UserSelect::Text: ts << "text"; break;
    case UserSelect::All: ts << "all"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, VerticalAlign verticalAlign)
{
    switch (verticalAlign) {
    case VerticalAlign::Baseline: ts << "baseline"; break;
    case VerticalAlign::Middle: ts << "middle"; break;
    case VerticalAlign::Sub: ts << "sub"; break;
    case VerticalAlign::Super: ts << "super"; break;
    case VerticalAlign::TextTop: ts << "text-top"; break;
    case VerticalAlign::TextBottom: ts << "text-bottom"; break;
    case VerticalAlign::Top: ts << "top"; break;
    case VerticalAlign::Bottom: ts << "bottom"; break;
    case VerticalAlign::BaselineMiddle: ts << "baseline-middle"; break;
    case VerticalAlign::Length: ts << "length"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Visibility visibility)
{
    switch (visibility) {
    case Visibility::Visible: ts << "visible"; break;
    case Visibility::Hidden: ts << "hidden"; break;
    case Visibility::Collapse: ts << "collapse"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, WhiteSpace whiteSpace)
{
    switch (whiteSpace) {
    case WhiteSpace::Normal: ts << "normal"; break;
    case WhiteSpace::Pre: ts << "pre"; break;
    case WhiteSpace::PreWrap: ts << "pre-wrap"; break;
    case WhiteSpace::PreLine: ts << "pre-line"; break;
    case WhiteSpace::NoWrap: ts << "nowrap"; break;
    case WhiteSpace::KHTMLNoWrap: ts << "khtml-nowrap"; break;
    case WhiteSpace::BreakSpaces: ts << "break-spaces"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WordBreak wordBreak)
{
    switch (wordBreak) {
    case WordBreak::Normal: ts << "normal"; break;
    case WordBreak::BreakAll: ts << "break-all"; break;
    case WordBreak::KeepAll: ts << "keep-all"; break;
    case WordBreak::BreakWord: ts << "break-word"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MathStyle mathStyle)
{
    switch (mathStyle) {
    case MathStyle::Normal: ts << "normal"; break;
    case MathStyle::Compact: ts << "compact"; break;
    }
    return ts;
}

bool alwaysPageBreak(BreakBetween between)
{
    return between >= BreakBetween::Page;
}

const float defaultMiterLimit = 4;

} // namespace WebCore
