/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// Keep in sync with the single keyword value fast path of CSSParserFastPaths's parseDisplay.
RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'display'>        = [ <display-outside> || <display-inside> ] | <display-listitem> | <display-internal> | <display-box> | <display-legacy>
    // <display-outside>  = block | inline | run-in
    // <display-inside>   = flow | flow-root | table | flex | grid | ruby
    // <display-listitem> = <display-outside>? && [ flow | flow-root ]? && list-item
    // <display-internal> = table-row-group | table-header-group |
    //                      table-footer-group | table-row | table-cell |
    //                      table-column-group | table-column | table-caption |
    //                      ruby-base | ruby-text | ruby-base-container |
    //                      ruby-text-container
    // <display-box>      = contents | none
    // <display-legacy>   = inline-block | inline-table | inline-flex | inline-grid
    // https://drafts.csswg.org/css-display/#propdef-display

    // Parse single keyword values
    auto singleKeyword = consumeIdent<
        // <display-box>
        CSSValueContents,
        CSSValueNone,
        // <display-internal>
        CSSValueTableCaption,
        CSSValueTableCell,
        CSSValueTableColumnGroup,
        CSSValueTableColumn,
        CSSValueTableHeaderGroup,
        CSSValueTableFooterGroup,
        CSSValueTableRow,
        CSSValueTableRowGroup,
        CSSValueRubyBase,
        CSSValueRubyText,
        // <display-legacy>
        CSSValueInlineBlock,
        CSSValueInlineFlex,
        CSSValueInlineGrid,
        CSSValueInlineTable,
        // Prefixed values
        CSSValueWebkitInlineBox,
        CSSValueWebkitBox,
        // No layout support for the full <display-listitem> syntax, so treat it as <display-legacy>
        CSSValueListItem
    >(range);

    auto allowsValue = [&](CSSValueID value) {
        bool isRuby = value == CSSValueRubyBase || value == CSSValueRubyText || value == CSSValueBlockRuby || value == CSSValueRuby;
        return !isRuby || isUASheetBehavior(context.mode);
    };

    if (singleKeyword) {
        if (!allowsValue(singleKeyword->valueID()))
            return nullptr;
        return singleKeyword;
    }

    // Empty value, stop parsing
    if (range.atEnd())
        return nullptr;

    // Convert -webkit-flex/-webkit-inline-flex to flex/inline-flex
    CSSValueID nextValueID = range.peek().id();
    if (nextValueID == CSSValueWebkitInlineFlex || nextValueID == CSSValueWebkitFlex) {
        consumeIdent(range);
        return CSSPrimitiveValue::create(
            nextValueID == CSSValueWebkitInlineFlex ? CSSValueInlineFlex : CSSValueFlex);
    }

    // Parse [ <display-outside> || <display-inside> ]
    std::optional<CSSValueID> parsedDisplayOutside;
    std::optional<CSSValueID> parsedDisplayInside;
    while (!range.atEnd()) {
        auto nextValueID = range.peek().id();
        switch (nextValueID) {
        // <display-outside>
        case CSSValueBlock:
        case CSSValueInline:
            if (parsedDisplayOutside)
                return nullptr;
            parsedDisplayOutside = nextValueID;
            break;
        // <display-inside>
        case CSSValueFlex:
        case CSSValueFlow:
        case CSSValueFlowRoot:
        case CSSValueGrid:
        case CSSValueTable:
        case CSSValueRuby:
            if (parsedDisplayInside)
                return nullptr;
            parsedDisplayInside = nextValueID;
            break;
        default:
            return nullptr;
        }
        consumeIdent(range);
    }

    // Set defaults when one of the two values are unspecified
    CSSValueID displayInside = parsedDisplayInside.value_or(CSSValueFlow);

    auto selectShortValue = [&]() -> CSSValueID {
        if (!parsedDisplayOutside || *parsedDisplayOutside == CSSValueInline) {
            if (displayInside == CSSValueRuby)
                return CSSValueRuby;
        }

        if (!parsedDisplayOutside || *parsedDisplayOutside == CSSValueBlock) {
            // Alias display: flow to display: block
            if (displayInside == CSSValueFlow)
                return CSSValueBlock;
            if (displayInside == CSSValueRuby)
                return CSSValueBlockRuby;
            return displayInside;
        }

        // Convert `display: inline <display-inside>` to the equivalent short value
        switch (displayInside) {
        case CSSValueFlex:
            return CSSValueInlineFlex;
        case CSSValueFlow:
            return CSSValueInline;
        case CSSValueFlowRoot:
            return CSSValueInlineBlock;
        case CSSValueGrid:
            return CSSValueInlineGrid;
        case CSSValueTable:
            return CSSValueInlineTable;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueInline;
        }
    };

    auto shortValue = selectShortValue();
    if (!allowsValue(shortValue))
        return nullptr;

    return CSSPrimitiveValue::create(shortValue);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
