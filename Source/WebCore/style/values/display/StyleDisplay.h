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
// NOTE: All <display-legacy> values are aliases of other values, so do not appear in the enum.
// https://drafts.csswg.org/css-display/#propdef-display
enum class DisplayType : uint8_t {
    // [ <display-outside> || <display-inside> ] and <-webkit-display>
    BlockFlow,            // Shortens to `block`
    BlockFlowRoot,        // Shortens to `flow-root`
    BlockTable,           // Shortens to `table`
    BlockFlex,            // Shortens to `flex`
    BlockGrid,            // Shortens to `grid`
    BlockGridLanes,       // Shortens to `grid-lanes`
    BlockRuby,
    BlockDeprecatedFlex,  // Shortens to `-webkit-box`

    InlineFlow,           // Shortens to `inline`
    InlineFlowRoot,       // Shortens to `inline-block`
    InlineTable,
    InlineFlex,
    InlineGrid,
    InlineGridLanes,
    InlineRuby,           // Shortens to `ruby`
    InlineDeprecatedFlex, // Shortens to `-webkit-inline-box`

    // <display-listitem>
    BlockFlowListItem,    // Shortens to `list-item`

    // <display-internal>
    TableCaption,
    TableCell,
    TableColumnGroup,
    TableColumn,
    TableHeaderGroup,
    TableFooterGroup,
    TableRow,
    TableRowGroup,
    RubyBase,
    RubyText,

    // <display-box>
    Contents,
    None
};

struct Display {
    DisplayType value;

    constexpr Display(DisplayType value) : value { value } { }

    // Special constructor for initial value.
    constexpr Display(CSS::Keyword::Inline) : value { DisplayType::InlineFlow } { }

    static constexpr Display fromRaw(unsigned rawValue) { return static_cast<DisplayType>(rawValue); }
    constexpr unsigned toRaw() const { return static_cast<unsigned>(value); }

    constexpr Display blockified() const;
    constexpr Display inlinified() const;

    constexpr bool isBlockType() const;
    constexpr bool isInlineType() const;
    constexpr bool isTableBox() const;
    constexpr bool isTableOrTablePart() const;
    constexpr bool isInternalTableBox() const;
    constexpr bool isRubyContainerOrInternalRubyBox() const;
    constexpr bool isGridBox() const;
    constexpr bool isGridLanesBox() const;
    constexpr bool isListItemType() const;
    constexpr bool isDeprecatedFlexibleBox() const;
    constexpr bool isFlexibleBox() const;
    constexpr bool isGridFormattingContextBox() const;
    constexpr bool isFlexibleOrGridFormattingContextBox() const;
    constexpr bool isFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox() const;
    constexpr bool doesGenerateBlockContainer() const;
    constexpr bool doesGenerateBox() const;

    constexpr bool operator==(const Display&) const = default;
    constexpr bool operator==(DisplayType other) const { return value == other; }
};
DEFINE_TYPE_WRAPPER_GET(Display, value);

// https://drafts.csswg.org/css-display/#blockify
constexpr Display Display::blockified() const
{
    switch (value) {
    case DisplayType::BlockFlow:
    case DisplayType::BlockFlowRoot:
    case DisplayType::BlockTable:
    case DisplayType::BlockFlex:
    case DisplayType::BlockGrid:
    case DisplayType::BlockGridLanes:
    case DisplayType::BlockRuby:
    case DisplayType::BlockDeprecatedFlex:
    case DisplayType::BlockFlowListItem:
        return value;

    case DisplayType::InlineTable:
        return DisplayType::BlockTable;
    case DisplayType::InlineFlex:
        return DisplayType::BlockFlex;
    case DisplayType::InlineGrid:
        return DisplayType::BlockGrid;
    case DisplayType::InlineGridLanes:
        return DisplayType::BlockGridLanes;
    case DisplayType::InlineRuby:
        return DisplayType::BlockRuby;
    case DisplayType::InlineDeprecatedFlex:
        return DisplayType::BlockDeprecatedFlex;

    case DisplayType::InlineFlow:
    case DisplayType::InlineFlowRoot:
    case DisplayType::TableRowGroup:
    case DisplayType::TableHeaderGroup:
    case DisplayType::TableFooterGroup:
    case DisplayType::TableRow:
    case DisplayType::TableColumnGroup:
    case DisplayType::TableColumn:
    case DisplayType::TableCell:
    case DisplayType::TableCaption:
    case DisplayType::RubyBase:
    case DisplayType::RubyText:
        return DisplayType::BlockFlow;

    case DisplayType::Contents:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::Contents;
    case DisplayType::None:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::None;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
}

// https://drafts.csswg.org/css-display/#inlinify
constexpr Display Display::inlinified() const
{
    switch (value) {
    case DisplayType::BlockFlow:
        return DisplayType::InlineFlowRoot;
    case DisplayType::BlockTable:
        return DisplayType::InlineTable;
    case DisplayType::BlockFlex:
        return DisplayType::InlineFlex;
    case DisplayType::BlockGrid:
        return DisplayType::InlineGrid;
    case DisplayType::BlockGridLanes:
        return DisplayType::InlineGridLanes;
    case DisplayType::BlockRuby:
        return DisplayType::InlineRuby;
    case DisplayType::BlockDeprecatedFlex:
        return DisplayType::InlineDeprecatedFlex;

    case DisplayType::InlineFlow:
    case DisplayType::InlineFlowRoot:
    case DisplayType::InlineTable:
    case DisplayType::InlineFlex:
    case DisplayType::InlineGrid:
    case DisplayType::InlineGridLanes:
    case DisplayType::InlineRuby:
    case DisplayType::InlineDeprecatedFlex:
    case DisplayType::RubyBase:
    case DisplayType::RubyText:
        return value;

    case DisplayType::BlockFlowRoot:
    case DisplayType::BlockFlowListItem:
    case DisplayType::TableRowGroup:
    case DisplayType::TableHeaderGroup:
    case DisplayType::TableFooterGroup:
    case DisplayType::TableRow:
    case DisplayType::TableColumnGroup:
    case DisplayType::TableColumn:
    case DisplayType::TableCell:
    case DisplayType::TableCaption:
        return DisplayType::InlineFlow;

    case DisplayType::Contents:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::Contents;
    case DisplayType::None:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::None;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
}

constexpr bool Display::isBlockType() const
{
    return value == DisplayType::BlockFlow
        || value == DisplayType::BlockFlowRoot
        || value == DisplayType::BlockTable
        || value == DisplayType::BlockFlex
        || value == DisplayType::BlockGrid
        || value == DisplayType::BlockGridLanes
        || value == DisplayType::BlockRuby
        || value == DisplayType::BlockDeprecatedFlex
        || value == DisplayType::BlockFlowListItem;
}

constexpr bool Display::isInlineType() const
{
    return value == DisplayType::InlineFlow
        || value == DisplayType::InlineFlowRoot
        || value == DisplayType::InlineTable
        || value == DisplayType::InlineFlex
        || value == DisplayType::InlineGrid
        || value == DisplayType::InlineGridLanes
        || value == DisplayType::InlineRuby
        || value == DisplayType::InlineDeprecatedFlex
        || value == DisplayType::RubyBase
        || value == DisplayType::RubyText;
}

constexpr bool Display::isTableBox() const
{
    return value == DisplayType::BlockTable
        || value == DisplayType::InlineTable;
}

constexpr bool Display::isTableOrTablePart() const
{
    return value == DisplayType::BlockTable
        || value == DisplayType::InlineTable
        || value == DisplayType::TableCell
        || value == DisplayType::TableCaption
        || value == DisplayType::TableRowGroup
        || value == DisplayType::TableHeaderGroup
        || value == DisplayType::TableFooterGroup
        || value == DisplayType::TableRow
        || value == DisplayType::TableColumnGroup
        || value == DisplayType::TableColumn;
}

// https://drafts.csswg.org/css-display/#internal-table-box
constexpr bool Display::isInternalTableBox() const
{
    return value == DisplayType::TableCell
        || value == DisplayType::TableRowGroup
        || value == DisplayType::TableHeaderGroup
        || value == DisplayType::TableFooterGroup
        || value == DisplayType::TableRow
        || value == DisplayType::TableColumnGroup
        || value == DisplayType::TableColumn;
}

// https://drafts.csswg.org/css-display/#internal-ruby-box
constexpr bool Display::isRubyContainerOrInternalRubyBox() const
{
    return value == DisplayType::InlineRuby
        || value == DisplayType::RubyText
        || value == DisplayType::RubyBase;
}

constexpr bool Display::isGridBox() const
{
    return value == DisplayType::BlockGrid
        || value == DisplayType::InlineGrid;
}

constexpr bool Display::isGridLanesBox() const
{
    return value == DisplayType::BlockGridLanes
        || value == DisplayType::InlineGridLanes;
}

constexpr bool Display::isListItemType() const
{
    return value == DisplayType::BlockFlowListItem;
}

constexpr bool Display::isDeprecatedFlexibleBox() const
{
    return value == DisplayType::BlockDeprecatedFlex
        || value == DisplayType::InlineDeprecatedFlex;
}

constexpr bool Display::isFlexibleBox() const
{
    return value == DisplayType::BlockFlex
        || value == DisplayType::InlineFlex;
}

constexpr bool Display::isGridFormattingContextBox() const
{
    return isGridBox()
        || isGridLanesBox();
}

constexpr bool Display::isFlexibleOrGridFormattingContextBox() const
{
    return isFlexibleBox()
        || isGridFormattingContextBox();
}

constexpr bool Display::isFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox() const
{
    return isFlexibleOrGridFormattingContextBox()
        || isDeprecatedFlexibleBox();
}

constexpr bool Display::doesGenerateBlockContainer() const
{
    return value == DisplayType::BlockFlow
        || value == DisplayType::BlockFlowRoot
        || value == DisplayType::BlockFlowListItem
        || value == DisplayType::InlineFlowRoot
        || value == DisplayType::TableCell
        || value == DisplayType::TableCaption;
}

constexpr bool Display::doesGenerateBox() const
{
    return value != DisplayType::Contents
        && value != DisplayType::None;
}

// MARK: - Conversion

template<> struct CSSValueConversion<Display> { auto operator()(BuilderState&, const CSSValue&) -> Display; };

template<> struct ValueRepresentation<DisplayType> {
    template<typename... F> constexpr decltype(auto) operator()(DisplayType value, F&&... f)
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        switch (value) {
        // [ <display-outside> || <display-inside> ] and <-webkit-display>
        case DisplayType::BlockFlow:
            return visitor(CSS::Keyword::Block { });
        case DisplayType::BlockFlowRoot:
            return visitor(CSS::Keyword::FlowRoot { });
        case DisplayType::BlockTable:
            return visitor(CSS::Keyword::Table { });
        case DisplayType::BlockFlex:
            return visitor(CSS::Keyword::Flex { });
        case DisplayType::BlockGrid:
            return visitor(CSS::Keyword::Grid { });
        case DisplayType::BlockGridLanes:
            return visitor(CSS::Keyword::GridLanes { });
        case DisplayType::BlockRuby:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Block { }, CSS::Keyword::Ruby { } });
        case DisplayType::BlockDeprecatedFlex:
            return visitor(CSS::Keyword::WebkitBox { });

        case DisplayType::InlineFlow:
            return visitor(CSS::Keyword::Inline { });
        case DisplayType::InlineFlowRoot:
            return visitor(CSS::Keyword::InlineBlock { });
        case DisplayType::InlineTable:
            return visitor(CSS::Keyword::InlineTable { });
        case DisplayType::InlineFlex:
            return visitor(CSS::Keyword::InlineFlex { });
        case DisplayType::InlineGrid:
            return visitor(CSS::Keyword::InlineGrid { });
        case DisplayType::InlineGridLanes:
            return visitor(CSS::Keyword::InlineGridLanes { });
        case DisplayType::InlineRuby:
            return visitor(CSS::Keyword::Ruby { });
        case DisplayType::InlineDeprecatedFlex:
            return visitor(CSS::Keyword::WebkitInlineBox { });

        // <display-listitem>
        case DisplayType::BlockFlowListItem:
            return visitor(CSS::Keyword::ListItem { });

        // <display-internal>
        case DisplayType::TableRowGroup:
            return visitor(CSS::Keyword::TableRowGroup { });
        case DisplayType::TableHeaderGroup:
            return visitor(CSS::Keyword::TableHeaderGroup { });
        case DisplayType::TableFooterGroup:
            return visitor(CSS::Keyword::TableFooterGroup { });
        case DisplayType::TableRow:
            return visitor(CSS::Keyword::TableRow { });
        case DisplayType::TableColumnGroup:
            return visitor(CSS::Keyword::TableColumnGroup { });
        case DisplayType::TableColumn:
            return visitor(CSS::Keyword::TableColumn { });
        case DisplayType::TableCell:
            return visitor(CSS::Keyword::TableCell { });
        case DisplayType::TableCaption:
            return visitor(CSS::Keyword::TableCaption { });
        case DisplayType::RubyBase:
            return visitor(CSS::Keyword::RubyBase { });
        case DisplayType::RubyText:
            return visitor(CSS::Keyword::RubyText { });

        // <display-box>
        case DisplayType::None:
            return visitor(CSS::Keyword::None { });
        case DisplayType::Contents:
            return visitor(CSS::Keyword::Contents { });
        }

        RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    }
};

// MARK: - Blending

template<> struct Blending<Display> {
    constexpr auto canBlend(Display, Display) -> bool { return false; }
    auto blend(Display, Display, const BlendingContext&) -> Display;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Display, 1)
