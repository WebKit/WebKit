/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'display'> = [ <display-outside> || <display-inside> ] | <display-listitem> | <display-internal> | <display-box> | <display-legacy> | <-webkit-display>
// https://drafts.csswg.org/css-display/#propdef-display
enum class Display : uint8_t {
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
    FlowRoot,
    Ruby,
    RubyBlock,
    RubyBase,
    RubyAnnotation,
    None
};

constexpr bool isDisplayInlineType(Display display)
{
    return display == Style::Display::Inline
        || display == Style::Display::InlineBlock
        || display == Style::Display::InlineBox
        || display == Style::Display::InlineFlex
        || display == Style::Display::InlineGrid
        || display == Style::Display::InlineTable
        || display == Style::Display::Ruby
        || display == Style::Display::RubyBase
        || display == Style::Display::RubyAnnotation;
}

constexpr bool isDisplayBlockType(Display display)
{
    return display == Style::Display::Block
        || display == Style::Display::Box
        || display == Style::Display::Flex
        || display == Style::Display::FlowRoot
        || display == Style::Display::Grid
        || display == Style::Display::ListItem
        || display == Style::Display::Table
        || display == Style::Display::RubyBlock;
}

constexpr bool isDisplayFlexibleBox(Display display)
{
    return display == Style::Display::Flex
        || display == Style::Display::InlineFlex;
}

constexpr bool isDisplayGridBox(Display display)
{
    return display == Style::Display::Grid
        || display == Style::Display::InlineGrid;
}

constexpr bool isDisplayFlexibleOrGridBox(Display display)
{
    return isDisplayFlexibleBox(display)
        || isDisplayGridBox(display);
}

constexpr bool isDisplayDeprecatedFlexibleBox(Style::Display display)
{
    return display == Style::Display::Box
        || display == Style::Display::InlineBox;
}

constexpr bool isDisplayFlexibleBoxIncludingDeprecatedOrGridBox(Display display)
{
    return isDisplayFlexibleOrGridBox(display)
        || isDisplayDeprecatedFlexibleBox(display);
}

constexpr bool isDisplayListItemType(Display display)
{
    return display == Style::Display::ListItem;
}

constexpr bool isDisplayTableOrTablePart(Display display)
{
    return display == Style::Display::Table
        || display == Style::Display::InlineTable
        || display == Style::Display::TableCell
        || display == Style::Display::TableCaption
        || display == Style::Display::TableRowGroup
        || display == Style::Display::TableHeaderGroup
        || display == Style::Display::TableFooterGroup
        || display == Style::Display::TableRow
        || display == Style::Display::TableColumnGroup
        || display == Style::Display::TableColumn;
}

constexpr bool isInternalTableBox(Display display)
{
    // https://drafts.csswg.org/css-display/#layout-specific-display
    return display == Style::Display::TableCell
        || display == Style::Display::TableRowGroup
        || display == Style::Display::TableHeaderGroup
        || display == Style::Display::TableFooterGroup
        || display == Style::Display::TableRow
        || display == Style::Display::TableColumnGroup
        || display == Style::Display::TableColumn;
}

constexpr bool isRubyContainerOrInternalRubyBox(Display display)
{
    return display == Style::Display::Ruby
        || display == Style::Display::RubyAnnotation
        || display == Style::Display::RubyBase;
}

constexpr bool isDisplayRegionType(Display display)
{
    return display == Style::Display::Block
        || display == Style::Display::InlineBlock
        || display == Style::Display::TableCell
        || display == Style::Display::TableCaption
        || display == Style::Display::ListItem;
}

constexpr bool doesDisplayGenerateBlockContainer(Display display)
{
    return display == Style::Display::Block
        || display == Style::Display::InlineBlock
        || display == Style::Display::FlowRoot
        || display == Style::Display::ListItem
        || display == Style::Display::TableCell
        || display == Style::Display::TableCaption;
}

// MARK: - Blending

template<> struct Blending<Display> {
    constexpr auto canBlend(const Display&, const Display&) -> bool { return false; }
    constexpr auto requiresNormalizedProgress(const Display&, const Display&) -> bool { return false; }
    auto blend(const Display&, const Display&, const BlendingContext&) -> Display;
};

} // namespace Style
} // namespace WebCore
