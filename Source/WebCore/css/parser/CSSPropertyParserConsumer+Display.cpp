/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "CSSPropertyParserConsumer+Display.h"

#include "CSSParserContext.h"
#include "CSSParserMode.h"
#include "CSSParserTokenRange.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserState.h"
#include "CSSValueKeywords.h"
#include <wtf/EnumeratedArray.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

using DisplayOutsideInsideMap = EnumeratedArray<DisplayOutside, EnumeratedArray<DisplayInside, std::pair<CSSValueID, CSSValueID>>>;

consteval DisplayOutsideInsideMap NODELETE makeDisplayOutsideInsideMap()
{
    using enum DisplayOutside;
    using enum DisplayInside;

    DisplayOutsideInsideMap result;

    // One of either <display-inside> or <display-outside> is needed, so this is case is invalid.
    result[NoOutside][NoInside]  = { CSSValueInvalid, CSSValueInvalid };

    // Aliasing `block <display-inside>`.
    //
    // Everything shortens to be just `<display-inside>` except:
    //   - `block` on its own is aliased to `block flow`, thus stays `block`.
    //   - `block flow` is aliased to `block`, not `flow`.
    //   - `block ruby` is not aliased to anything.

    result[Block][NoInside]      = { CSSValueBlock,     CSSValueInvalid };
    result[Block][Flow]          = { CSSValueBlock,     CSSValueInvalid };
    result[Block][FlowRoot]      = { CSSValueFlowRoot,  CSSValueInvalid };
    result[Block][Table]         = { CSSValueTable,     CSSValueInvalid };
    result[Block][Flex]          = { CSSValueFlex,      CSSValueInvalid };
    result[Block][Grid]          = { CSSValueGrid,      CSSValueInvalid };
    result[Block][GridLanes]     = { CSSValueGridLanes, CSSValueInvalid };
    result[Block][Ruby]          = { CSSValueBlock,     CSSValueRuby };

    // Aliasing `inline <display-inside>`.
    //
    // Everything shortens to `inline-<display-inside>` except:
    //   - `inline` on its own is the same as `inline flow`, thus stays `inline`.
    //   - `inline flow` is aliased to `inline`. not `inline-flow`.
    //   - `inline flow-root` is aliased to `inline-block`. not `inline-flow-root`.
    //   - `inline ruby` is aliased to `ruby`, not `inline-ruby`.

    result[Inline][NoInside]     = { CSSValueInline,          CSSValueInvalid };
    result[Inline][Flow]         = { CSSValueInline,          CSSValueInvalid };
    result[Inline][FlowRoot]     = { CSSValueInlineBlock,     CSSValueInvalid };
    result[Inline][Table]        = { CSSValueInlineTable,     CSSValueInvalid };
    result[Inline][Flex]         = { CSSValueInlineFlex,      CSSValueInvalid };
    result[Inline][Grid]         = { CSSValueInlineGrid,      CSSValueInvalid };
    result[Inline][GridLanes]    = { CSSValueInlineGridLanes, CSSValueInvalid };
    result[Inline][Ruby]         = { CSSValueRuby,            CSSValueInvalid };

    // Aliasing `<display-inside>` on its own.
    //
    // Everything aliases to `block <display-inside>` (and then recursively to what that aliases to) except:
    //   - `ruby` on its own is aliased to `inline ruby`, not `block ruby`, which ultimately aliases back to `ruby`.

    result[NoOutside][Flow]      = result[Block][Flow];
    result[NoOutside][FlowRoot]  = result[Block][FlowRoot];
    result[NoOutside][Table]     = result[Block][Table];
    result[NoOutside][Flex]      = result[Block][Flex];
    result[NoOutside][Grid]      = result[Block][Grid];
    result[NoOutside][GridLanes] = result[Block][GridLanes];
    result[NoOutside][Ruby]      = result[Inline][Ruby];

    return result;
}

constexpr auto displayOutsideInsideMap = makeDisplayOutsideInsideMap();

template<DisplayOutside outside, DisplayInside inside>
RefPtr<CSSValue> mappedDisplayValue()
{
    static constexpr auto result = displayOutsideInsideMap[outside][inside];

    if constexpr (result.first == CSSValueInvalid && result.second == CSSValueInvalid)
        return nullptr;
    else if constexpr (result.second == CSSValueInvalid)
        return CSSPrimitiveValue::create(result.first);
    else
        return CSSValuePair::createNoncoalescing(CSSPrimitiveValue::create(result.first), CSSPrimitiveValue::create(result.second));
}

template<DisplayOutside outside>
static RefPtr<CSSValue> consumeAfterInitialDisplayOutside(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    using enum DisplayInside;

    CSSParserTokenRangeGuard guard { range };
    range.consumeIncludingWhitespace();

    switch (range.peek().id()) {
    case CSSValueFlow:
        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<outside, Flow>();

    case CSSValueFlowRoot:
        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<outside, FlowRoot>();

    case CSSValueTable:
        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<outside, Table>();

    case CSSValueFlex:
        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<outside, Flex>();

    case CSSValueGrid:
        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<outside, Grid>();

    case CSSValueGridLanes:
        if (!state.context.gridLanesEnabled)
            return nullptr;

        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<outside, GridLanes>();

    case CSSValueRuby:
        if (!state.context.cssRubyDisplayTypesEnabled)
            return nullptr;

        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<outside, Ruby>();

    case CSSValueInvalid:
        guard.commit();
        return mappedDisplayValue<outside, NoInside>();

    default:
        return nullptr;
    }
}

template<DisplayInside inside>
static RefPtr<CSSValue> consumeAfterInitialDisplayInside(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    using enum DisplayOutside;

    CSSParserTokenRangeGuard guard { range };
    range.consumeIncludingWhitespace();

    switch (range.peek().id()) {
    case CSSValueBlock:
        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<Block, inside>();

    case CSSValueInline:
        range.consumeIncludingWhitespace();
        guard.commit();
        return mappedDisplayValue<Inline, inside>();

    case CSSValueInvalid:
        guard.commit();
        return mappedDisplayValue<NoOutside, inside>();

    default:
        return nullptr;
    }
}

// Keep in sync with the single keyword value fast path of CSSParserFastPaths's parseDisplay.
RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'display'>        = [ <display-outside> || <display-inside> ] | <display-listitem> | <display-internal> | <display-box> | <display-legacy> | <display-non-standard>
    // <display-outside>  = block | inline | run-in
    // <display-inside>   = flow | flow-root | table | flex | grid | grid-lanes | ruby
    // <display-listitem> = <display-outside>? && [ flow | flow-root ]? && list-item
    // <display-internal> = table-row-group | table-header-group |
    //                      table-footer-group | table-row | table-cell |
    //                      table-column-group | table-column | table-caption |
    //                      ruby-base | ruby-text | ruby-base-container |
    //                      ruby-text-container
    // <display-box>      = contents | none
    // <display-legacy>   = inline-block | inline-table | inline-flex | inline-grid | inline-grid-lanes
    // <display-non-standard> = -webkit-box | -webkit-inline-box | -webkit-flex | -webkit-inline-flex
    // https://drafts.csswg.org/css-display/#propdef-display
    //  and
    // https://drafts.csswg.org/css-grid-3/#grid-lanes-containers (for additions of grid-lanes and inline-grid-lanes)

    switch (range.peek().id()) {
    // <display-outside>
    // FIXME: Add support for `run-in`.
    case CSSValueBlock:
        return consumeAfterInitialDisplayOutside<DisplayOutside::Block>(range, state);
    case CSSValueInline:
        return consumeAfterInitialDisplayOutside<DisplayOutside::Inline>(range, state);

    // <display-inside>
    case CSSValueFlow:
        return consumeAfterInitialDisplayInside<DisplayInside::Flow>(range, state);
    case CSSValueFlowRoot:
        return consumeAfterInitialDisplayInside<DisplayInside::FlowRoot>(range, state);
    case CSSValueTable:
        return consumeAfterInitialDisplayInside<DisplayInside::Table>(range, state);
    case CSSValueFlex:
        return consumeAfterInitialDisplayInside<DisplayInside::Flex>(range, state);
    case CSSValueGrid:
        return consumeAfterInitialDisplayInside<DisplayInside::Grid>(range, state);
    case CSSValueGridLanes:
        if (!state.context.gridLanesEnabled)
            return nullptr;
        return consumeAfterInitialDisplayInside<DisplayInside::GridLanes>(range, state);
    case CSSValueRuby:
        if (!state.context.cssRubyDisplayTypesEnabled)
            return nullptr;
        return consumeAfterInitialDisplayInside<DisplayInside::Ruby>(range, state);

    // <display-listitem>
    // FIXME: Add support for the full <display-listitem> syntax, not just the single value version.
    case CSSValueListItem:
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());

    // <display-internal>
    // FIXME: Add support for `ruby-base-container` and `ruby-text-container`.
    case CSSValueTableCaption:
    case CSSValueTableCell:
    case CSSValueTableColumnGroup:
    case CSSValueTableColumn:
    case CSSValueTableHeaderGroup:
    case CSSValueTableFooterGroup:
    case CSSValueTableRow:
    case CSSValueTableRowGroup:
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());
    case CSSValueRubyBase:
    case CSSValueRubyText:
        if (!state.context.cssRubyDisplayTypesEnabled)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());

    // <display-box>
    case CSSValueContents:
    case CSSValueNone:
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());

    // <display-legacy>
    case CSSValueInlineBlock:
    case CSSValueInlineTable:
    case CSSValueInlineFlex:
    case CSSValueInlineGrid:
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());
    case CSSValueInlineGridLanes:
        if (!state.context.gridLanesEnabled)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());

    // <display-non-standard>
    case CSSValueWebkitBox:
    case CSSValueWebkitInlineBox:
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());
    case CSSValueWebkitFlex:
        // `-webkit-flex` is aliased to `flex`.
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(CSSValueFlex);
    case CSSValueWebkitInlineFlex:
        // `-webkit-inline-flex` is aliased to `inline-flex`.
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(CSSValueInlineFlex);

    default:
        return nullptr;
    }
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
