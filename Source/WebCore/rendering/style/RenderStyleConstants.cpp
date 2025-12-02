/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveValueMappings.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

bool alwaysPageBreak(BreakBetween between)
{
    return between >= BreakBetween::Page;
}

CSSBoxType transformBoxToCSSBoxType(TransformBox transformBox)
{
    switch (transformBox) {
    case TransformBox::StrokeBox:
        return CSSBoxType::StrokeBox;
    case TransformBox::ContentBox:
        return CSSBoxType::ContentBox;
    case TransformBox::BorderBox:
        return CSSBoxType::BorderBox;
    case TransformBox::FillBox:
        return CSSBoxType::FillBox;
    case TransformBox::ViewBox:
        return CSSBoxType::ViewBox;
    default:
        ASSERT_NOT_REACHED();
        return CSSBoxType::BorderBox;
    }
}

TextStream& operator<<(TextStream& ts, AnimationDirection direction)
{
    switch (direction) {
    case AnimationDirection::Normal: ts << "normal"_s; break;
    case AnimationDirection::Alternate: ts << "alternate"_s; break;
    case AnimationDirection::Reverse: ts << "reverse"_s; break;
    case AnimationDirection::AlternateReverse: ts << "alternate-reverse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AnimationFillMode fillMode)
{
    switch (fillMode) {
    case AnimationFillMode::None: ts << "none"_s; break;
    case AnimationFillMode::Forwards: ts << "forwards"_s; break;
    case AnimationFillMode::Backwards: ts << "backwards"_s; break;
    case AnimationFillMode::Both: ts << "both"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AnimationPlayState playState)
{
    switch (playState) {
    case AnimationPlayState::Running: ts << "running"_s; break;
    case AnimationPlayState::Paused: ts << "paused"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AspectRatioType aspectRatioType)
{
    switch (aspectRatioType) {
    case AspectRatioType::Auto: ts << "auto"_s; break;
    case AspectRatioType::Ratio: ts << "ratio"_s; break;
    case AspectRatioType::AutoAndRatio: ts << "autoandratio"_s; break;
    case AspectRatioType::AutoZero: ts << "autozero"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AutoRepeatType repeatType)
{
    switch (repeatType) {
    case AutoRepeatType::None: ts << "none"_s; break;
    case AutoRepeatType::Fill: ts << "fill"_s; break;
    case AutoRepeatType::Fit: ts << "fit"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BackfaceVisibility visibility)
{
    switch (visibility) {
    case BackfaceVisibility::Visible: ts << "visible"_s; break;
    case BackfaceVisibility::Hidden: ts << "hidden"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BlockStepAlign blockStepAlign)
{
    switch (blockStepAlign) {
    case BlockStepAlign::Auto: ts << "auto"_s; break;
    case BlockStepAlign::Center: ts << "center"_s; break;
    case BlockStepAlign::Start: ts << "start"_s; break;
    case BlockStepAlign::End: ts << "end"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BlockStepInsert blockStepInsert)
{
    switch (blockStepInsert) {
    case BlockStepInsert::MarginBox: ts << "margin-box"_s; break;
    case BlockStepInsert::PaddingBox: ts << "padding-box"_s; break;
    case BlockStepInsert::ContentBox: ts << "content-box"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BlockStepRound blockStepRound)
{
    switch (blockStepRound) {
    case BlockStepRound::Up: ts << "up"_s; break;
    case BlockStepRound::Down: ts << "down"_s; break;
    case BlockStepRound::Nearest: ts << "nearest"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BorderCollapse collapse)
{
    switch (collapse) {
    case BorderCollapse::Separate: ts << "separate"_s; break;
    case BorderCollapse::Collapse: ts << "collapse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BorderStyle borderStyle)
{
    switch (borderStyle) {
    case BorderStyle::None: ts << "none"_s; break;
    case BorderStyle::Hidden: ts << "hidden"_s; break;
    case BorderStyle::Inset: ts << "inset"_s; break;
    case BorderStyle::Groove: ts << "groove"_s; break;
    case BorderStyle::Outset: ts << "outset"_s; break;
    case BorderStyle::Ridge: ts << "ridge"_s; break;
    case BorderStyle::Dotted: ts << "dotted"_s; break;
    case BorderStyle::Dashed: ts << "dashed"_s; break;
    case BorderStyle::Solid: ts << "solid"_s; break;
    case BorderStyle::Double: ts << "double"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxAlignment boxAlignment)
{
    switch (boxAlignment) {
    case BoxAlignment::Stretch: ts << "stretch"_s; break;
    case BoxAlignment::Start: ts << "start"_s; break;
    case BoxAlignment::Center: ts << "center"_s; break;
    case BoxAlignment::End: ts << "end"_s; break;
    case BoxAlignment::Baseline: ts << "baseline"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxDecorationBreak decorationBreak)
{
    switch (decorationBreak) {
    case BoxDecorationBreak::Slice: ts << "slice"_s; break;
    case BoxDecorationBreak::Clone: ts << "clone"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxDirection boxDirection)
{
    switch (boxDirection) {
    case BoxDirection::Normal: ts << "normal"_s; break;
    case BoxDirection::Reverse: ts << "reverse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxLines boxLines)
{
    switch (boxLines) {
    case BoxLines::Single: ts << "single"_s; break;
    case BoxLines::Multiple: ts << "multiple"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxOrient boxOrient)
{
    switch (boxOrient) {
    case BoxOrient::Horizontal: ts << "horizontal"_s; break;
    case BoxOrient::Vertical: ts << "vertical"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxPack boxPack)
{
    switch (boxPack) {
    case BoxPack::Start: ts << "start"_s; break;
    case BoxPack::Center: ts << "center"_s; break;
    case BoxPack::End: ts << "end"_s; break;
    case BoxPack::Justify: ts << "justify"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BoxSizing boxSizing)
{
    switch (boxSizing) {
    case BoxSizing::ContentBox: ts << "content-box"_s; break;
    case BoxSizing::BorderBox: ts << "border-box"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BreakBetween breakBetween)
{
    switch (breakBetween) {
    case BreakBetween::Auto: ts << "auto"_s; break;
    case BreakBetween::Avoid: ts << "avoid"_s; break;
    case BreakBetween::AvoidColumn: ts << "avoid-column"_s; break;
    case BreakBetween::AvoidPage: ts << "avoid-page"_s; break;
    case BreakBetween::Column: ts << "column"_s; break;
    case BreakBetween::Page: ts << "page"_s; break;
    case BreakBetween::LeftPage: ts << "left-page"_s; break;
    case BreakBetween::RightPage: ts << "right-page"_s; break;
    case BreakBetween::RectoPage: ts << "recto-page"_s; break;
    case BreakBetween::VersoPage: ts << "verso-page"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BreakInside breakInside)
{
    switch (breakInside) {
    case BreakInside::Auto: ts << "auto"_s; break;
    case BreakInside::Avoid: ts << "avoid"_s; break;
    case BreakInside::AvoidColumn: ts << "avoidColumn"_s; break;
    case BreakInside::AvoidPage: ts << "avoidPage"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CSSBoxType boxType)
{
    switch (boxType) {
    case CSSBoxType::BoxMissing: ts << "missing"_s; break;
    case CSSBoxType::MarginBox: ts << "margin-box"_s; break;
    case CSSBoxType::BorderBox: ts << "border-box"_s; break;
    case CSSBoxType::PaddingBox: ts << "padding-box"_s; break;
    case CSSBoxType::ContentBox: ts << "content-box"_s; break;
    case CSSBoxType::FillBox: ts << "fill-box"_s; break;
    case CSSBoxType::StrokeBox: ts << "stroke-box"_s; break;
    case CSSBoxType::ViewBox: ts << "view-box"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CaptionSide side)
{
    switch (side) {
    case CaptionSide::Top: ts << "top"_s; break;
    case CaptionSide::Bottom: ts << "bottom"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Clear clear)
{
    switch (clear) {
    case Clear::None: ts << "none"_s; break;
    case Clear::Left: ts << "left"_s; break;
    case Clear::Right: ts << "right"_s; break;
    case Clear::InlineStart : ts << "inline-start"_s; break;
    case Clear::InlineEnd : ts << "inline-end"_s; break;
    case Clear::Both: ts << "both"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UsedClear clear)
{
    switch (clear) {
    case UsedClear::None: ts << "none"_s; break;
    case UsedClear::Left: ts << "left"_s; break;
    case UsedClear::Right: ts << "right"_s; break;
    case UsedClear::Both: ts << "both"_s; break;
    }
    return ts;
}

#if ENABLE(DARK_MODE_CSS)
TextStream& operator<<(TextStream& ts, ColorScheme colorScheme)
{
    switch (colorScheme) {
    case ColorScheme::Light: ts << "light"_s; break;
    case ColorScheme::Dark: ts << "dark"_s; break;
    }
    return ts;
}
#endif

TextStream& operator<<(TextStream& ts, ColumnAxis axis)
{
    switch (axis) {
    case ColumnAxis::Horizontal: ts << "horizontal"_s; break;
    case ColumnAxis::Vertical: ts << "vertical"_s; break;
    case ColumnAxis::Auto: ts << "auto"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColumnFill fill)
{
    switch (fill) {
    case ColumnFill::Auto: ts << "auto"_s; break;
    case ColumnFill::Balance: ts << "balance"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColumnProgression progression)
{
    switch (progression) {
    case ColumnProgression::Normal: ts << "normal"_s; break;
    case ColumnProgression::Reverse: ts << "reverse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColumnSpan span)
{
    switch (span) {
    case ColumnSpan::None: ts << "none"_s; break;
    case ColumnSpan::All: ts << "all"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ContentDistribution distribution)
{
    switch (distribution) {
    case ContentDistribution::Default: ts << "default"_s; break;
    case ContentDistribution::SpaceBetween: ts << "space-between"_s; break;
    case ContentDistribution::SpaceAround: ts << "space-around"_s; break;
    case ContentDistribution::SpaceEvenly: ts << "space-evenly"_s; break;
    case ContentDistribution::Stretch: ts << "stretch"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ContentPosition position)
{
    switch (position) {
    case ContentPosition::Normal: ts << "normal"_s; break;
    case ContentPosition::Baseline: ts << "baseline"_s; break;
    case ContentPosition::LastBaseline: ts << "last-baseline"_s; break;
    case ContentPosition::Center: ts << "center"_s; break;
    case ContentPosition::Start: ts << "start"_s; break;
    case ContentPosition::End: ts << "end"_s; break;
    case ContentPosition::FlexStart: ts << "flex-start"_s; break;
    case ContentPosition::FlexEnd: ts << "flex-end"_s; break;
    case ContentPosition::Left: ts << "left"_s; break;
    case ContentPosition::Right: ts << "right"_s; break;

    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ContentVisibility contentVisibility)
{
    switch (contentVisibility) {
    case ContentVisibility::Visible: ts << "visible"_s; break;
    case ContentVisibility::Auto: ts << "auto"_s; break;
    case ContentVisibility::Hidden: ts << "hidden"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CursorType cursor)
{
    switch (cursor) {
    case CursorType::Auto: ts << "auto"_s; break;
    case CursorType::Default: ts << "default"_s; break;
    case CursorType::ContextMenu: ts << "contextmenu"_s; break;
    case CursorType::Help: ts << "help"_s; break;
    case CursorType::Pointer: ts << "pointer"_s; break;
    case CursorType::Progress: ts << "progress"_s; break;
    case CursorType::Wait: ts << "wait"_s; break;
    case CursorType::Cell: ts << "cell"_s; break;
    case CursorType::Crosshair: ts << "crosshair"_s; break;
    case CursorType::Text: ts << "text"_s; break;
    case CursorType::VerticalText: ts << "vertical-text"_s; break;
    case CursorType::Alias: ts << "alias"_s; break;
    case CursorType::Move: ts << "move"_s; break;
    case CursorType::NoDrop: ts << "nodrop"_s; break;
    case CursorType::NotAllowed: ts << "not-allowed"_s; break;
    case CursorType::Grab: ts << "grab"_s; break;
    case CursorType::Grabbing: ts << "grabbing"_s; break;
    case CursorType::EResize: ts << "e-resize"_s; break;
    case CursorType::NResize: ts << "n-resize"_s; break;
    case CursorType::NEResize: ts << "ne-resize"_s; break;
    case CursorType::NWResize: ts << "nw-resize"_s; break;
    case CursorType::SResize: ts << "sr-esize"_s; break;
    case CursorType::SEResize: ts << "se-resize"_s; break;
    case CursorType::SWResize: ts << "sw-resize"_s; break;
    case CursorType::WResize: ts << "w-resize"_s; break;
    case CursorType::EWResize: ts << "ew-resize"_s; break;
    case CursorType::NSResize: ts << "ns-resize"_s; break;
    case CursorType::NESWResize: ts << "nesw-resize"_s; break;
    case CursorType::NWSEResize: ts << "nwse-resize"_s; break;
    case CursorType::ColumnResize: ts << "column-resize"_s; break;
    case CursorType::RowResize: ts << "row-resize"_s; break;
    case CursorType::AllScroll: ts << "all-scroll"_s; break;
    case CursorType::ZoomIn: ts << "zoom-in"_s; break;
    case CursorType::ZoomOut: ts << "zoom-out"_s; break;
    case CursorType::Copy: ts << "copy"_s; break;
    case CursorType::None: ts << "none"_s; break;
    }
    return ts;
}

#if ENABLE(CURSOR_VISIBILITY)
TextStream& operator<<(TextStream& ts, CursorVisibility visibility)
{
    switch (visibility) {
    case CursorVisibility::Auto: ts << "auto"_s; break;
    case CursorVisibility::AutoHide: ts << "autohide"_s; break;
    }
    return ts;
}
#endif

TextStream& operator<<(TextStream& ts, DisplayType display)
{
    switch (display) {
    case DisplayType::Inline: ts << "inline"_s; break;
    case DisplayType::Block: ts << "block"_s; break;
    case DisplayType::ListItem: ts << "list-item"_s; break;
    case DisplayType::InlineBlock: ts << "inline-block"_s; break;
    case DisplayType::Table: ts << "table"_s; break;
    case DisplayType::InlineTable: ts << "inline-table"_s; break;
    case DisplayType::TableRowGroup: ts << "table-row-group"_s; break;
    case DisplayType::TableHeaderGroup: ts << "table-header-group"_s; break;
    case DisplayType::TableFooterGroup: ts << "table-footer-group"_s; break;
    case DisplayType::TableRow: ts << "table-row"_s; break;
    case DisplayType::TableColumnGroup: ts << "table-column-group"_s; break;
    case DisplayType::TableColumn: ts << "table-column"_s; break;
    case DisplayType::TableCell: ts << "table-cell"_s; break;
    case DisplayType::TableCaption: ts << "table-caption"_s; break;
    case DisplayType::Box: ts << "box"_s; break;
    case DisplayType::InlineBox: ts << "inline-box"_s; break;
    case DisplayType::Flex: ts << "flex"_s; break;
    case DisplayType::InlineFlex: ts << "inline-flex"_s; break;
    case DisplayType::Contents: ts << "contents"_s; break;
    case DisplayType::Grid: ts << "grid"_s; break;
    case DisplayType::InlineGrid: ts << "inline-grid"_s; break;
    case DisplayType::GridLanes: ts << "grid-lanes"_s; break;
    case DisplayType::InlineGridLanes: ts << "inline-grid-lanes"_s; break;
    case DisplayType::FlowRoot: ts << "flow-root"_s; break;
    case DisplayType::Ruby: ts << "ruby"_s; break;
    case DisplayType::RubyBlock: ts << "block ruby"_s; break;
    case DisplayType::RubyBase: ts << "ruby-base"_s; break;
    case DisplayType::RubyAnnotation: ts << "ruby-text"_s; break;
    case DisplayType::None: ts << "none"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Edge edge)
{
    switch (edge) {
    case Edge::Top: ts << "top"_s; break;
    case Edge::Right: ts << "right"_s; break;
    case Edge::Bottom: ts << "bottom"_s; break;
    case Edge::Left: ts << "left"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, EmptyCell emptyCell)
{
    switch (emptyCell) {
    case EmptyCell::Show: ts << "show"_s; break;
    case EmptyCell::Hide: ts << "hide"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, EventListenerRegionType listenerType)
{
    switch (listenerType) {
    case EventListenerRegionType::Wheel: ts << "wheel"_s; break;
    case EventListenerRegionType::NonPassiveWheel: ts << "active wheel"_s; break;
    case EventListenerRegionType::MouseClick: ts << "mouse click"_s; break;
    case EventListenerRegionType::TouchStart: ts << "touch start"_s; break;
    case EventListenerRegionType::NonPassiveTouchStart: ts << "active touch start"_s; break;
    case EventListenerRegionType::TouchEnd: ts << "touch end"_s; break;
    case EventListenerRegionType::NonPassiveTouchEnd: ts << "active touch end"_s; break;
    case EventListenerRegionType::TouchCancel: ts << "touch cancel"_s; break;
    case EventListenerRegionType::NonPassiveTouchCancel: ts << "active touch cancel"_s; break;
    case EventListenerRegionType::TouchMove: ts << "touch move"_s; break;
    case EventListenerRegionType::NonPassiveTouchMove: ts << "active touch move"_s; break;
    case EventListenerRegionType::PointerDown: ts << "pointer down"_s; break;
    case EventListenerRegionType::NonPassivePointerDown: ts << "active pointer down"_s; break;
    case EventListenerRegionType::PointerEnter: ts << "pointer enter"_s; break;
    case EventListenerRegionType::NonPassivePointerEnter: ts << "active pointer down"_s; break;
    case EventListenerRegionType::PointerLeave: ts << "pointer leave"_s; break;
    case EventListenerRegionType::NonPassivePointerLeave: ts << "active pointer down"_s; break;
    case EventListenerRegionType::PointerMove: ts << "pointer move"_s; break;
    case EventListenerRegionType::NonPassivePointerMove: ts << "active pointer down"_s; break;
    case EventListenerRegionType::PointerOut: ts << "pointer out"_s; break;
    case EventListenerRegionType::NonPassivePointerOut: ts << "active pointer down"_s; break;
    case EventListenerRegionType::PointerOver: ts << "pointer over"_s; break;
    case EventListenerRegionType::NonPassivePointerOver: ts << "active pointer down"_s; break;
    case EventListenerRegionType::PointerUp: ts << "pointer up"_s; break;
    case EventListenerRegionType::NonPassivePointerUp: ts << "active pointer down"_s; break;
    case EventListenerRegionType::MouseDown: ts << "mouse down"_s; break;
    case EventListenerRegionType::NonPassiveMouseDown: ts << "active mouse down"_s; break;
    case EventListenerRegionType::MouseUp: ts << "mouse up"_s; break;
    case EventListenerRegionType::NonPassiveMouseUp: ts << "active mouse up"_s; break;
    case EventListenerRegionType::MouseMove: ts << "mouse down"_s; break;
    case EventListenerRegionType::NonPassiveMouseMove: ts << "active mouse move"_s; break;
    case EventListenerRegionType::GestureChange: ts << "gesture change"_s; break;
    case EventListenerRegionType::NonPassiveGestureChange: ts << "active gesture change"_s; break;
    case EventListenerRegionType::GestureEnd: ts << "gesture end"_s; break;
    case EventListenerRegionType::NonPassiveGestureEnd: ts << "active gesture end"_s; break;
    case EventListenerRegionType::GestureStart: ts << "gesture start"_s; break;
    case EventListenerRegionType::NonPassiveGestureStart: ts << "active gesture start"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FieldSizing sizing)
{
    switch (sizing) {
    case FieldSizing::Fixed: ts << "fixed"_s; break;
    case FieldSizing::Content: ts << "content"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillAttachment attachment)
{
    switch (attachment) {
    case FillAttachment::ScrollBackground: ts << "scroll"_s; break;
    case FillAttachment::LocalBackground: ts << "local"_s; break;
    case FillAttachment::FixedBackground: ts << "fixed"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillBox fill)
{
    switch (fill) {
    case FillBox::BorderBox: ts << "border-box"_s; break;
    case FillBox::PaddingBox: ts << "padding-box"_s; break;
    case FillBox::ContentBox: ts << "content-box"_s; break;
    case FillBox::BorderArea: ts << "border-area"_s; break;
    case FillBox::Text: ts << "text"_s; break;
    case FillBox::NoClip: ts << "no-clip"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillRepeat repeat)
{
    switch (repeat) {
    case FillRepeat::Repeat: ts << "repeat"_s; break;
    case FillRepeat::NoRepeat: ts << "no-repeat"_s; break;
    case FillRepeat::Round: ts << "round"_s; break;
    case FillRepeat::Space: ts << "space"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillSizeType sizeType)
{
    switch (sizeType) {
    case FillSizeType::Contain: ts << "contain"_s; break;
    case FillSizeType::Cover: ts << "cover"_s; break;
    case FillSizeType::Size: ts << "size-length"_s; break;
    case FillSizeType::None: ts << "size-none"_s; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, FlexDirection flexDirection)
{
    switch (flexDirection) {
    case FlexDirection::Row: ts << "row"_s; break;
    case FlexDirection::RowReverse: ts << "row-reverse"_s; break;
    case FlexDirection::Column: ts << "column"_s; break;
    case FlexDirection::ColumnReverse: ts << "column-reverse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FlexWrap flexWrap)
{
    switch (flexWrap) {
    case FlexWrap::NoWrap: ts << "no-wrap"_s; break;
    case FlexWrap::Wrap: ts << "wrap"_s; break;
    case FlexWrap::Reverse: ts << "reverse"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Float floating)
{
    switch (floating) {
    case Float::None: ts << "none"_s; break;
    case Float::Left: ts << "left"_s; break;
    case Float::Right: ts << "right"_s; break;
    case Float::InlineStart: ts << "inline-start"_s; break;
    case Float::InlineEnd: ts << "inline-end"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UsedFloat floating)
{
    switch (floating) {
    case UsedFloat::None: ts << "none"_s; break;
    case UsedFloat::Left: ts << "left"_s; break;
    case UsedFloat::Right: ts << "right"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Hyphens hyphens)
{
    switch (hyphens) {
    case Hyphens::None: ts << "none"_s; break;
    case Hyphens::Manual: ts << "manual"_s; break;
    case Hyphens::Auto: ts << "auto"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ImageRendering imageRendering)
{
    switch (imageRendering) {
    case ImageRendering::Auto: ts << "auto"_s; break;
    case ImageRendering::OptimizeSpeed: ts << "optimizeSpeed"_s; break;
    case ImageRendering::OptimizeQuality: ts << "optimizeQuality"_s; break;
    case ImageRendering::CrispEdges: ts << "crispEdges"_s; break;
    case ImageRendering::Pixelated: ts << "pixelated"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, InsideLink inside)
{
    switch (inside) {
    case InsideLink::NotInside: ts << "not-inside"_s; break;
    case InsideLink::InsideUnvisited: ts << "inside-unvisited"_s; break;
    case InsideLink::InsideVisited: ts << "inside-visited"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Isolation isolation)
{
    switch (isolation) {
    case Isolation::Auto: ts << "auto"_s; break;
    case Isolation::Isolate: ts << "isolate"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ItemPosition position)
{
    switch (position) {
    case ItemPosition::Legacy: ts << "legacy"_s; break;
    case ItemPosition::Auto: ts << "auto"_s; break;
    case ItemPosition::Normal: ts << "normal"_s; break;
    case ItemPosition::Stretch: ts << "stretch"_s; break;
    case ItemPosition::Baseline: ts << "baseline"_s; break;
    case ItemPosition::LastBaseline: ts << "last-baseline"_s; break;
    case ItemPosition::Center: ts << "center"_s; break;
    case ItemPosition::Start: ts << "start"_s; break;
    case ItemPosition::End: ts << "end"_s; break;
    case ItemPosition::SelfStart: ts << "self-start"_s; break;
    case ItemPosition::SelfEnd: ts << "self-end"_s; break;
    case ItemPosition::FlexStart: ts << "flex-start"_s; break;
    case ItemPosition::FlexEnd: ts << "flex-end"_s; break;
    case ItemPosition::Left: ts << "left"_s; break;
    case ItemPosition::Right: ts << "right"_s; break;
    case ItemPosition::AnchorCenter: ts << "anchor-center"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ItemPositionType positionType)
{
    switch (positionType) {
    case ItemPositionType::NonLegacy: ts << "non-legacy"_s; break;
    case ItemPositionType::Legacy: ts << "legacy"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineAlign align)
{
    switch (align) {
    case LineAlign::None: ts << "none"_s; break;
    case LineAlign::Edges: ts << "edges"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineBreak lineBreak)
{
    switch (lineBreak) {
    case LineBreak::Auto: ts << "auto"_s; break;
    case LineBreak::Loose: ts << "loose"_s; break;
    case LineBreak::Normal: ts << "normal"_s; break;
    case LineBreak::Strict: ts << "strict"_s; break;
    case LineBreak::AfterWhiteSpace: ts << "after-whiteSpace"_s; break;
    case LineBreak::Anywhere: ts << "anywhere"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineSnap lineSnap)
{
    switch (lineSnap) {
    case LineSnap::None: ts << "none"_s; break;
    case LineSnap::Baseline: ts << "baseline"_s; break;
    case LineSnap::Contain: ts << "contain"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ListStylePosition position)
{
    switch (position) {
    case ListStylePosition::Outside: ts << "outside"_s; break;
    case ListStylePosition::Inside: ts << "inside"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MarqueeBehavior marqueeBehavior)
{
    switch (marqueeBehavior) {
    case MarqueeBehavior::None: ts << "none"_s; break;
    case MarqueeBehavior::Scroll: ts << "scroll"_s; break;
    case MarqueeBehavior::Slide: ts << "slide"_s; break;
    case MarqueeBehavior::Alternate: ts << "alternate"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MarqueeDirection marqueeDirection)
{
    switch (marqueeDirection) {
    case MarqueeDirection::Auto: ts << "auto"_s; break;
    case MarqueeDirection::Left: ts << "left"_s; break;
    case MarqueeDirection::Right: ts << "right"_s; break;
    case MarqueeDirection::Up: ts << "up"_s; break;
    case MarqueeDirection::Down: ts << "down"_s; break;
    case MarqueeDirection::Forward: ts << "forward"_s; break;
    case MarqueeDirection::Backward: ts << "backward"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, NBSPMode mode)
{
    switch (mode) {
    case NBSPMode::Normal: ts << "normal"_s; break;
    case NBSPMode::Space: ts << "space"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, NinePieceImageRule rule)
{
    switch (rule) {
    case NinePieceImageRule::Stretch: ts << "stretch"_s; break;
    case NinePieceImageRule::Round: ts << "round"_s; break;
    case NinePieceImageRule::Space: ts << "space"_s; break;
    case NinePieceImageRule::Repeat: ts << "repeat"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ObjectFit objectFit)
{
    switch (objectFit) {
    case ObjectFit::Fill: ts << "fill"_s; break;
    case ObjectFit::Contain: ts << "contain"_s; break;
    case ObjectFit::Cover: ts << "cover"_s; break;
    case ObjectFit::None: ts << "none"_s; break;
    case ObjectFit::ScaleDown: ts << "scale-down"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Order order)
{
    switch (order) {
    case Order::Logical: ts << "logical"_s; break;
    case Order::Visual: ts << "visual"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, OutlineStyle outlineStyle)
{
    switch (outlineStyle) {
    case OutlineStyle::Auto: ts << "auto"_s; break;
    case OutlineStyle::None: ts << "none"_s; break;
    case OutlineStyle::Inset: ts << "inset"_s; break;
    case OutlineStyle::Groove: ts << "groove"_s; break;
    case OutlineStyle::Outset: ts << "outset"_s; break;
    case OutlineStyle::Ridge: ts << "ridge"_s; break;
    case OutlineStyle::Dotted: ts << "dotted"_s; break;
    case OutlineStyle::Dashed: ts << "dashed"_s; break;
    case OutlineStyle::Solid: ts << "solid"_s; break;
    case OutlineStyle::Double: ts << "double"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Overflow overflow)
{
    switch (overflow) {
    case Overflow::Visible: ts << "visible"_s; break;
    case Overflow::Hidden: ts << "hidden"_s; break;
    case Overflow::Scroll: ts << "scroll"_s; break;
    case Overflow::Auto: ts << "auto"_s; break;
    case Overflow::PagedX: ts << "paged-x"_s; break;
    case Overflow::PagedY: ts << "paged-y"_s; break;
    case Overflow::Clip: ts << "clip"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, OverflowAlignment alignment)
{
    switch (alignment) {
    case OverflowAlignment::Default: ts << "default"_s; break;
    case OverflowAlignment::Unsafe: ts << "unsafe"_s; break;
    case OverflowAlignment::Safe: ts << "safe"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, OverflowWrap overflowWrap)
{
    switch (overflowWrap) {
    case OverflowWrap::Normal: ts << "normal"_s; break;
    case OverflowWrap::BreakWord: ts << "break-word"_s; break;
    case OverflowWrap::Anywhere: ts << "anywhere"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PointerEvents pointerEvents)
{
    switch (pointerEvents) {
    case PointerEvents::None: ts << "none"_s; break;
    case PointerEvents::Auto: ts << "auto"_s; break;
    case PointerEvents::Stroke: ts << "stroke"_s; break;
    case PointerEvents::Fill: ts << "fill"_s; break;
    case PointerEvents::Painted: ts << "painted"_s; break;
    case PointerEvents::Visible: ts << "visible"_s; break;
    case PointerEvents::BoundingBox: ts << "bounding-box"_s; break;
    case PointerEvents::VisibleStroke: ts << "visible-stroke"_s; break;
    case PointerEvents::VisibleFill: ts << "visible-fill"_s; break;
    case PointerEvents::VisiblePainted: ts << "visible-painted"_s; break;
    case PointerEvents::All: ts << "all"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PositionType position)
{
    switch (position) {
    case PositionType::Static: ts << "static"_s; break;
    case PositionType::Relative: ts << "relative"_s; break;
    case PositionType::Absolute: ts << "absolute"_s; break;
    case PositionType::Sticky: ts << "sticky"_s; break;
    case PositionType::Fixed: ts << "fixed"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PrintColorAdjust colorAdjust)
{
    switch (colorAdjust) {
    case PrintColorAdjust::Economy: ts << "economy"_s; break;
    case PrintColorAdjust::Exact: ts << "exact"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PseudoElementType pseudoElementType)
{
    switch (pseudoElementType) {
    case PseudoElementType::FirstLine: ts << "first-line"_s; break;
    case PseudoElementType::FirstLetter: ts << "first-letter"_s; break;
    case PseudoElementType::GrammarError: ts << "grammar-error"_s; break;
    case PseudoElementType::Highlight: ts << "highlight"_s; break;
    case PseudoElementType::InternalWritingSuggestions: ts << "-internal-writing-suggestions"_s; break;
    case PseudoElementType::Marker: ts << "marker"_s; break;
    case PseudoElementType::Backdrop: ts << "backdrop"_s; break;
    case PseudoElementType::Before: ts << "before"_s; break;
    case PseudoElementType::After: ts << "after"_s; break;
    case PseudoElementType::Selection: ts << "selection"_s; break;
    case PseudoElementType::SpellingError: ts << "spelling-error"_s; break;
    case PseudoElementType::TargetText: ts << "target-text"_s; break;
    case PseudoElementType::ViewTransition: ts << "view-transition"_s; break;
    case PseudoElementType::ViewTransitionGroup: ts << "view-transition-group"_s; break;
    case PseudoElementType::ViewTransitionImagePair: ts << "view-transition-image-pair"_s; break;
    case PseudoElementType::ViewTransitionOld: ts << "view-transition-old"_s; break;
    case PseudoElementType::ViewTransitionNew: ts << "view-transition-new"_s; break;
    case PseudoElementType::WebKitResizer: ts << "-webkit-resizer"_s; break;
    case PseudoElementType::WebKitScrollbar: ts << "-webkit-scrollbar"_s; break;
    case PseudoElementType::WebKitScrollbarThumb: ts << "-webkit-scrollbar-thumb"_s; break;
    case PseudoElementType::WebKitScrollbarButton: ts << "-webkit-scrollbar-button"_s; break;
    case PseudoElementType::WebKitScrollbarTrack: ts << "-webkit-scrollbar-track"_s; break;
    case PseudoElementType::WebKitScrollbarTrackPiece: ts << "-webkit-scrollbar-trackpiece"_s; break;
    case PseudoElementType::WebKitScrollbarCorner: ts << "-webkit-scrollbar-corner"_s; break;
    default:
        ts << "other"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, QuoteType quoteType)
{
    switch (quoteType) {
    case QuoteType::OpenQuote: ts << "open"_s; break;
    case QuoteType::CloseQuote: ts << "close"_s; break;
    case QuoteType::NoOpenQuote: ts << "no-open"_s; break;
    case QuoteType::NoCloseQuote: ts << "no-close"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ReflectionDirection direction)
{
    switch (direction) {
    case ReflectionDirection::Below: ts << "below"_s; break;
    case ReflectionDirection::Above: ts << "above"_s; break;
    case ReflectionDirection::Left: ts << "left"_s; break;
    case ReflectionDirection::Right: ts << "right"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, RubyPosition position)
{
    switch (position) {
    case RubyPosition::Over: ts << "over"_s; break;
    case RubyPosition::Under: ts << "under"_s; break;
    case RubyPosition::InterCharacter: ts << "inter-character"_s; break;
    case RubyPosition::LegacyInterCharacter: ts << "legacy inter-character"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, RubyAlign alignment)
{
    switch (alignment) {
    case RubyAlign::Start: ts << "start"_s; break;
    case RubyAlign::Center: ts << "center"_s; break;
    case RubyAlign::SpaceBetween: ts << "space-between"_s; break;
    case RubyAlign::SpaceAround: ts << "space-around"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, RubyOverhang overhang)
{
    switch (overhang) {
    case RubyOverhang::Auto: ts << "auto"_s; break;
    case RubyOverhang::None: ts << "none"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollSnapAxis axis)
{
    switch (axis) {
    case ScrollSnapAxis::XAxis: ts << "x-axis"_s; break;
    case ScrollSnapAxis::YAxis: ts << "y-Axis"_s; break;
    case ScrollSnapAxis::Block: ts << "block"_s; break;
    case ScrollSnapAxis::Inline: ts << "inline"_s; break;
    case ScrollSnapAxis::Both: ts << "both"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollSnapAxisAlignType alignType)
{
    switch (alignType) {
    case ScrollSnapAxisAlignType::None: ts << "none"_s; break;
    case ScrollSnapAxisAlignType::Start: ts << "start"_s; break;
    case ScrollSnapAxisAlignType::Center: ts << "center"_s; break;
    case ScrollSnapAxisAlignType::End: ts << "end"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollSnapStrictness strictness)
{
    switch (strictness) {
    case ScrollSnapStrictness::Proximity: ts << "proximity"_s; break;
    case ScrollSnapStrictness::Mandatory: ts << "mandatory"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ScrollSnapStop stop)
{
    switch (stop) {
    case ScrollSnapStop::Normal: ts << "normal"_s; break;
    case ScrollSnapStop::Always: ts << "always"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Scroller scroller)
{
    switch (scroller) {
    case Scroller::Nearest: ts << "nearest"_s; break;
    case Scroller::Root: ts << "root"_s; break;
    case Scroller::Self: ts << "self"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, StyleDifference diff)
{
    switch (diff) {
    case StyleDifference::Equal: ts << "equal"_s; break;
    case StyleDifference::RecompositeLayer: ts << "recomposite layer"_s; break;
    case StyleDifference::Repaint: ts << "repaint"_s; break;
    case StyleDifference::RepaintIfText: ts << "repaint if text"_s; break;
    case StyleDifference::RepaintLayer: ts << "repaint layer"_s; break;
    case StyleDifference::LayoutOutOfFlowMovementOnly: ts << "layout positioned movement only"_s; break;
    case StyleDifference::Overflow: ts << "overflow"_s; break;
    case StyleDifference::OverflowAndOutOfFlowMovement: ts << "overflow and positioned movement"_s; break;
    case StyleDifference::Layout: ts << "layout"_s; break;
    case StyleDifference::NewStyle: ts << "new style"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TableLayoutType layoutType)
{
    switch (layoutType) {
    case TableLayoutType::Auto: ts << "Auto"_s; break;
    case TableLayoutType::Fixed: ts << "Fixed"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextCombine textCombine)
{
    switch (textCombine) {
    case TextCombine::None: ts << "none"_s; break;
    case TextCombine::All: ts << "all"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextDecorationSkipInk skip)
{
    switch (skip) {
    case TextDecorationSkipInk::None: ts << "none"_s; break;
    case TextDecorationSkipInk::Auto: ts << "auto"_s; break;
    case TextDecorationSkipInk::All: ts << "all"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextDecorationStyle decorationStyle)
{
    switch (decorationStyle) {
    case TextDecorationStyle::Solid: ts << "solid"_s; break;
    case TextDecorationStyle::Double: ts << "double"_s; break;
    case TextDecorationStyle::Dotted: ts << "dotted"_s; break;
    case TextDecorationStyle::Dashed: ts << "dashed"_s; break;
    case TextDecorationStyle::Wavy: ts << "wavy"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextEmphasisFill fill)
{
    switch (fill) {
    case TextEmphasisFill::Filled: ts << "filled"_s; break;
    case TextEmphasisFill::Open: ts << "open"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextEmphasisMark mark)
{
    switch (mark) {
    case TextEmphasisMark::Dot: ts << "dot"_s; break;
    case TextEmphasisMark::Circle: ts << "circle"_s; break;
    case TextEmphasisMark::DoubleCircle: ts << "double-circle"_s; break;
    case TextEmphasisMark::Triangle: ts << "triangle"_s; break;
    case TextEmphasisMark::Sesame: ts << "sesame"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextGroupAlign textGroupAlign)
{
    switch (textGroupAlign) {
    case TextGroupAlign::None: ts << "none"_s; break;
    case TextGroupAlign::Start: ts << "start"_s; break;
    case TextGroupAlign::End: ts << "end"_s; break;
    case TextGroupAlign::Left: ts << "left"_s; break;
    case TextGroupAlign::Right: ts << "right"_s; break;
    case TextGroupAlign::Center: ts << "center"_s; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, TextJustify justify)
{
    switch (justify) {
    case TextJustify::Auto: ts << "auto"_s; break;
    case TextJustify::InterCharacter: ts << "inter-character"_s; break;
    case TextJustify::InterWord: ts << "inter-word"_s; break;
    case TextJustify::None: ts << "none"_s; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, TextOverflow overflow)
{
    switch (overflow) {
    case TextOverflow::Clip: ts << "clip"_s; break;
    case TextOverflow::Ellipsis: ts << "ellipsis"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextSecurity textSecurity)
{
    switch (textSecurity) {
    case TextSecurity::None: ts << "none"_s; break;
    case TextSecurity::Disc: ts << "disc"_s; break;
    case TextSecurity::Circle: ts << "circle"_s; break;
    case TextSecurity::Square: ts << "square"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextWrapMode wrap)
{
    switch (wrap) {
    case TextWrapMode::Wrap: ts << "wrap"_s; break;
    case TextWrapMode::NoWrap: ts << "nowrap"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextWrapStyle style)
{
    switch (style) {
    case TextWrapStyle::Auto: ts << "auto"_s; break;
    case TextWrapStyle::Balance: ts << "balance"_s; break;
    case TextWrapStyle::Pretty: ts << "pretty"_s; break;
    case TextWrapStyle::Stable: ts << "stable"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextBoxTrim textBoxTrim)
{
    switch (textBoxTrim) {
    case TextBoxTrim::None: ts << "None"_s; break;
    case TextBoxTrim::TrimStart: ts << "trim-start"_s; break;
    case TextBoxTrim::TrimEnd: ts << "trim-end"_s; break;
    case TextBoxTrim::TrimBoth: ts << "trim-both"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextEdgeOver textEdgeOver)
{
    switch (textEdgeOver) {
    case TextEdgeOver::Text: ts << "text"_s; break;
    case TextEdgeOver::Ideographic: ts << "ideographic"_s; break;
    case TextEdgeOver::IdeographicInk: ts << "ideographic-ink"_s; break;
    case TextEdgeOver::Cap: ts << "cap"_s; break;
    case TextEdgeOver::Ex: ts << "ex"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextEdgeUnder textEdgeUnder)
{
    switch (textEdgeUnder) {
    case TextEdgeUnder::Text: ts << "text"_s; break;
    case TextEdgeUnder::Ideographic: ts << "ideographic"_s; break;
    case TextEdgeUnder::IdeographicInk: ts << "ideographic-ink"_s; break;
    case TextEdgeUnder::Alphabetic: ts << "alphabetic"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextZoom zoom)
{
    switch (zoom) {
    case TextZoom::Normal: ts << "normal"_s; break;
    case TextZoom::Reset: ts << "reset"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TransformBox box)
{
    switch (box) {
    case TransformBox::BorderBox: ts << "border-box"_s; break;
    case TransformBox::FillBox: ts << "fill-box"_s; break;
    case TransformBox::ViewBox: ts << "view-box"_s; break;
    case TransformBox::StrokeBox: ts << "stroke-box"_s; break;
    case TransformBox::ContentBox: ts << "content-box"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TransformStyle3D transformStyle)
{
    switch (transformStyle) {
    case TransformStyle3D::Flat: ts << "flat"_s; break;
    case TransformStyle3D::Preserve3D: ts << "preserve-3d"_s; break;
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case TransformStyle3D::Separated: ts << "separated"_s; break;
#endif
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TransitionBehavior transitionBehavior)
{
    switch (transitionBehavior) {
    case TransitionBehavior::Normal: ts << "normal"_s; break;
    case TransitionBehavior::AllowDiscrete: ts << "allow-discrete"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UserDrag userDrag)
{
    switch (userDrag) {
    case UserDrag::Auto: ts << "auto"_s; break;
    case UserDrag::None: ts << "none"_s; break;
    case UserDrag::Element: ts << "element"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UserModify userModify)
{
    switch (userModify) {
    case UserModify::ReadOnly: ts << "read-only"_s; break;
    case UserModify::ReadWrite: ts << "read-write"_s; break;
    case UserModify::ReadWritePlaintextOnly: ts << "read-write plaintext only"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, UserSelect userSelect)
{
    switch (userSelect) {
    case UserSelect::None: ts << "none"_s; break;
    case UserSelect::Text: ts << "text"_s; break;
    case UserSelect::All: ts << "all"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Visibility visibility)
{
    switch (visibility) {
    case Visibility::Visible: ts << "visible"_s; break;
    case Visibility::Hidden: ts << "hidden"_s; break;
    case Visibility::Collapse: ts << "collapse"_s; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, WhiteSpace whiteSpace)
{
    switch (whiteSpace) {
    case WhiteSpace::Normal: ts << "normal"_s; break;
    case WhiteSpace::Pre: ts << "pre"_s; break;
    case WhiteSpace::PreWrap: ts << "pre-wrap"_s; break;
    case WhiteSpace::PreLine: ts << "pre-line"_s; break;
    case WhiteSpace::NoWrap: ts << "nowrap"_s; break;
    case WhiteSpace::BreakSpaces: ts << "break-spaces"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WhiteSpaceCollapse whiteSpaceCollapse)
{
    switch (whiteSpaceCollapse) {
    case WhiteSpaceCollapse::Collapse: ts << "collapse"_s; break;
    case WhiteSpaceCollapse::Preserve: ts << "preserve"_s; break;
    case WhiteSpaceCollapse::PreserveBreaks: ts << "preserve-breaks"_s; break;
    case WhiteSpaceCollapse::BreakSpaces: ts << "break-spaces"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WordBreak wordBreak)
{
    switch (wordBreak) {
    case WordBreak::Normal: ts << "normal"_s; break;
    case WordBreak::BreakAll: ts << "break-all"_s; break;
    case WordBreak::KeepAll: ts << "keep-all"_s; break;
    case WordBreak::BreakWord: ts << "break-word"_s; break;
    case WordBreak::AutoPhrase: ts << "auto-phrase"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MathShift mathShift)
{
    switch (mathShift) {
    case MathShift::Normal: ts << "normal"_s; break;
    case MathShift::Compact: ts << "compact"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MathStyle mathStyle)
{
    switch (mathStyle) {
    case MathStyle::Normal: ts << "normal"_s; break;
    case MathStyle::Compact: ts << "compact"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ContainIntrinsicSizeType containIntrinsicSizeType)
{
    switch (containIntrinsicSizeType) {
    case ContainIntrinsicSizeType::None: ts << "none"_s; break;
    case ContainIntrinsicSizeType::Length: ts << "length"_s; break;
    case ContainIntrinsicSizeType::AutoAndLength: ts << "autoandlength"_s; break;
    case ContainIntrinsicSizeType::AutoAndNone: ts << "autoandnone"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, OverflowContinue overflowContinue)
{
    switch (overflowContinue) {
    case OverflowContinue::Auto:
        ts << "auto"_s;
        break;
    case OverflowContinue::Discard:
        ts << "discard"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, StyleDifferenceContextSensitiveProperty property)
{
    switch (property) {
    case StyleDifferenceContextSensitiveProperty::Transform: ts << "transform"_s; break;
    case StyleDifferenceContextSensitiveProperty::Opacity: ts << "opacity"_s; break;
    case StyleDifferenceContextSensitiveProperty::Filter: ts << "filter"_s; break;
    case StyleDifferenceContextSensitiveProperty::ClipRect: ts << "clipRect"_s; break;
    case StyleDifferenceContextSensitiveProperty::ClipPath: ts << "clipPath"_s; break;
    case StyleDifferenceContextSensitiveProperty::WillChange: ts << "willChange"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AlignmentBaseline value)
{
    switch (value) {
    case AlignmentBaseline::Baseline: ts << "baseline"_s; break;
    case AlignmentBaseline::BeforeEdge: ts << "before-edge"_s; break;
    case AlignmentBaseline::TextBeforeEdge: ts << "text-before-edge"_s; break;
    case AlignmentBaseline::Middle: ts << "middle"_s; break;
    case AlignmentBaseline::Central: ts << "central"_s; break;
    case AlignmentBaseline::AfterEdge: ts << "after-edge"_s; break;
    case AlignmentBaseline::TextAfterEdge: ts << "text-after-edge"_s; break;
    case AlignmentBaseline::Ideographic: ts << "ideographic"_s; break;
    case AlignmentBaseline::Alphabetic: ts << "alphabetic"_s; break;
    case AlignmentBaseline::Hanging: ts << "hanging"_s; break;
    case AlignmentBaseline::Mathematical: ts << "mathematical"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BufferedRendering value)
{
    switch (value) {
    case BufferedRendering::Auto: ts << "auto"_s; break;
    case BufferedRendering::Dynamic: ts << "dynamic"_s; break;
    case BufferedRendering::Static: ts << "static"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColorInterpolation value)
{
    switch (value) {
    case ColorInterpolation::Auto: ts << "auto"_s; break;
    case ColorInterpolation::SRGB: ts << "sRGB"_s; break;
    case ColorInterpolation::LinearRGB: ts << "linearRGB"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, DominantBaseline value)
{
    switch (value) {
    case DominantBaseline::Auto: ts << "auto"_s; break;
    case DominantBaseline::UseScript: ts << "use-script"_s; break;
    case DominantBaseline::NoChange: ts << "no-change"_s; break;
    case DominantBaseline::ResetSize: ts << "reset-size"_s; break;
    case DominantBaseline::Ideographic: ts << "ideographic"_s; break;
    case DominantBaseline::Alphabetic: ts << "alphabetic"_s; break;
    case DominantBaseline::Hanging: ts << "hanging"_s; break;
    case DominantBaseline::Mathematical: ts << "mathematical"_s; break;
    case DominantBaseline::Central: ts << "central"_s; break;
    case DominantBaseline::Middle: ts << "middle"_s; break;
    case DominantBaseline::TextAfterEdge: ts << "text-after-edge"_s; break;
    case DominantBaseline::TextBeforeEdge: ts << "text-before-edge"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, GlyphOrientation value)
{
    switch (value) {
    case GlyphOrientation::Degrees0: ts << '0'; break;
    case GlyphOrientation::Degrees90: ts << "90"_s; break;
    case GlyphOrientation::Degrees180: ts << "180"_s; break;
    case GlyphOrientation::Degrees270: ts << "270"_s; break;
    case GlyphOrientation::Auto: ts << "Auto"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MaskType value)
{
    switch (value) {
    case MaskType::Luminance: ts << "luminance"_s; break;
    case MaskType::Alpha: ts << "alpha"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ShapeRendering value)
{
    switch (value) {
    case ShapeRendering::Auto: ts << "auto"_s; break;
    case ShapeRendering::OptimizeSpeed: ts << "optimizeSpeed"_s; break;
    case ShapeRendering::CrispEdges: ts << "crispEdges"_s; break;
    case ShapeRendering::GeometricPrecision: ts << "geometricPrecision"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextAnchor value)
{
    switch (value) {
    case TextAnchor::Start: ts << "start"_s; break;
    case TextAnchor::Middle: ts << "middle"_s; break;
    case TextAnchor::End: ts << "end"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, VectorEffect value)
{
    switch (value) {
    case VectorEffect::None: ts << "none"_s; break;
    case VectorEffect::NonScalingStroke: ts << "non-scaling-stroke"_s; break;
    }
    return ts;
}

} // namespace WebCore
