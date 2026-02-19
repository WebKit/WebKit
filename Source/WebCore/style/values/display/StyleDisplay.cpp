/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleDisplay.h"

#include "AnimationUtilities.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<Display>::operator()(BuilderState& state, const CSSValue& value) -> Display
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return DisplayType::InlineFlow;

    switch (primitiveValue->valueID()) {
    // [ <display-outside> || <display-inside> ] and <-webkit-display>
    case CSSValueBlock:
        return Style::DisplayType::BlockFlow;
    case CSSValueFlowRoot:
        return Style::DisplayType::BlockFlowRoot;
    case CSSValueTable:
        return Style::DisplayType::BlockTable;
    case CSSValueFlex:
        return Style::DisplayType::BlockFlex;
    case CSSValueGrid:
        return Style::DisplayType::BlockGrid;
    case CSSValueGridLanes:
        return Style::DisplayType::BlockGridLanes;
    case CSSValueBlockRuby:
        return Style::DisplayType::BlockRuby;
    case CSSValueWebkitBox:
        return Style::DisplayType::BlockDeprecatedFlex;

    case CSSValueInline:
        return Style::DisplayType::InlineFlow;
    case CSSValueInlineBlock:
        return Style::DisplayType::InlineFlowRoot;
    case CSSValueInlineTable:
        return Style::DisplayType::InlineTable;
    case CSSValueInlineFlex:
        return Style::DisplayType::InlineFlex;
    case CSSValueInlineGrid:
        return Style::DisplayType::InlineGrid;
    case CSSValueInlineGridLanes:
        return Style::DisplayType::InlineGridLanes;
    case CSSValueRuby:
        return Style::DisplayType::InlineRuby;
    case CSSValueWebkitInlineBox:
        return Style::DisplayType::InlineDeprecatedFlex;

    // <display-listitem>
    case CSSValueListItem:
        return Style::DisplayType::BlockFlowListItem;

    // <display-internal>
    case CSSValueTableRowGroup:
        return Style::DisplayType::TableRowGroup;
    case CSSValueTableHeaderGroup:
        return Style::DisplayType::TableHeaderGroup;
    case CSSValueTableFooterGroup:
        return Style::DisplayType::TableFooterGroup;
    case CSSValueTableRow:
        return Style::DisplayType::TableRow;
    case CSSValueTableColumnGroup:
        return Style::DisplayType::TableColumnGroup;
    case CSSValueTableColumn:
        return Style::DisplayType::TableColumn;
    case CSSValueTableCell:
        return Style::DisplayType::TableCell;
    case CSSValueTableCaption:
        return Style::DisplayType::TableCaption;
    case CSSValueRubyBase:
        return Style::DisplayType::RubyBase;
    case CSSValueRubyText:
        return Style::DisplayType::RubyText;

    // <display-box>
    case CSSValueContents:
        return Style::DisplayType::Contents;
    case CSSValueNone:
        return Style::DisplayType::None;

    default:
        break;
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return Style::DisplayType::InlineFlow;
}

// MARK: - Serialization

void Serialize<DisplayType>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, DisplayType value)
{
    if (value == DisplayType::InlineGridLanes) {
        // `inline-grid-lanes` is not a <display-legacy> keyword; serialize as
        // the two-keyword form `inline grid-lanes`.
        serializationForCSS(builder, context, style, CSS::Keyword::Inline { });
        builder.append(' ');
        serializationForCSS(builder, context, style, CSS::Keyword::GridLanes { });
        return;
    }

    valueRepresentation(value, [&](const auto& alternative) {
        serializationForCSS(builder, context, style, alternative);
    });
}

// MARK: - Blending

auto Blending<Display>::blend(Display a, Display b, const BlendingContext& context) -> Display
{
    // "In general, the display property's animation type is discrete. However, similar to interpolation of
    //  visibility, during interpolation between none and any other display value, p values between 0 and 1
    //  map to the non-none value. Additionally, the element is inert as long as its display value would
    //  compute to none when ignoring the Transitions and Animations cascade origins."
    // (https://drafts.csswg.org/css-display-4/#display-animation)

    if (a != DisplayType::None && b != DisplayType::None)
        return context.progress < 0.5 ? a : b;
    if (context.progress <= 0)
        return a;
    if (context.progress >= 1)
        return b;
    return a == DisplayType::None ? b : a;
}

} // namespace Style
} // namespace WebCore
