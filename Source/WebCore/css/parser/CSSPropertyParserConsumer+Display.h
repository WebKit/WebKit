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

#pragma once

#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSValue;

namespace CSS {
struct PropertyParserState;
}

namespace CSSPropertyParserHelpers {

enum class DisplayOutside : uint8_t { NoOutside, Block, Inline };
enum class DisplayInside  : uint8_t { NoInside, Flow, FlowRoot, Table, Flex, Grid, GridLanes, Ruby };

// MARK: <'display'> consuming
// https://drafts.csswg.org/css-display/#propdef-display
RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange&, CSS::PropertyParserState&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::CSSPropertyParserHelpers::DisplayOutside> {
    using values = EnumValues<
        WebCore::CSSPropertyParserHelpers::DisplayOutside,
        WebCore::CSSPropertyParserHelpers::DisplayOutside::NoOutside,
        WebCore::CSSPropertyParserHelpers::DisplayOutside::Block,
        WebCore::CSSPropertyParserHelpers::DisplayOutside::Inline
    >;
};

template<> struct EnumTraits<WebCore::CSSPropertyParserHelpers::DisplayInside> {
    using values = EnumValues<
        WebCore::CSSPropertyParserHelpers::DisplayInside,
        WebCore::CSSPropertyParserHelpers::DisplayInside::NoInside,
        WebCore::CSSPropertyParserHelpers::DisplayInside::Flow,
        WebCore::CSSPropertyParserHelpers::DisplayInside::FlowRoot,
        WebCore::CSSPropertyParserHelpers::DisplayInside::Table,
        WebCore::CSSPropertyParserHelpers::DisplayInside::Flex,
        WebCore::CSSPropertyParserHelpers::DisplayInside::Grid,
        WebCore::CSSPropertyParserHelpers::DisplayInside::GridLanes,
        WebCore::CSSPropertyParserHelpers::DisplayInside::Ruby
    >;
};

} // namespace WTF
