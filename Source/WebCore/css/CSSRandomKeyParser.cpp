/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSRandomKeyParser.h"

#include "CSSParserTokenRange.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserState.h"
#include "CSSValueKeywords.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSSCalc::Random::Sharing> consumeUnresolvedRandomKey(CSSParserTokenRange& tokens, CSS::PropertyParserState& state, NOESCAPE const Function<CSSCalc::RandomSharingOptions::Auto()>& makeAuto)
{
    // <random-key> = auto | <random-cache-key> | fixed <number [0,1]>
    // <random-cache-key> = <dashed-ident> || element-scoped || [ property-scoped | property-index-scoped | <random-ua-ident> ]

    if (tokens.peek().id() == CSSValueFixed) {
        CSSParserTokenRangeGuard guard { tokens };

        tokens.consumeIncludingWhitespace();

        // Use a non-property parsing state for the fixed number value to disconnect it from the current parse.
        // FIXME: Add a mechanism to pass along the depth count when doing this so that we can limit stack usage.
        // FIXME: This should probably maintain the `cssRandomFunctionCount` state from the current state to allow for random() functions nested in the <number> or should document why this is not necessary.
        auto numberParsingState = CSS::PropertyParserState {
            .context = state.context,
            .pool = state.pool,
            .absoluteLengthUnitsOnly = state.absoluteLengthUnitsOnly
        };

        auto number = MetaConsumer<CSS::Number<CSS::ClosedUnitRange>>::consume(tokens, numberParsingState);
        if (!number)
            return { };

        guard.commit();

        return CSSCalc::Random::Sharing { CSSCalc::Random::SharingFixed { .value = WTF::move(*number) } };
    }

    std::optional<Variant<CSSCalc::RandomSharingOptions::Auto, CSS::CustomIdent>> identifier;
    std::optional<CSS::Keyword::ElementScoped> elementScoped;

    CSSParserTokenRangeGuard guard { tokens };

    auto consumeIdentifier = [&] -> bool {
        if (identifier)
            return false;
        if (tokens.peek().id() == CSSValueAuto) {
            tokens.consumeIncludingWhitespace();
            identifier = makeAuto();
            return true;
        }
        if (auto dashedIdent = consumeUnresolvedDashedIdent(tokens, state)) {
            identifier = WTF::move(*dashedIdent);
            return true;
        }
        return false;
    };
    auto consumeElementScoped = [&] -> bool {
        if (elementScoped)
            return false;
        if (tokens.peek().id() == CSSValueElementScoped) {
            tokens.consumeIncludingWhitespace();
            elementScoped = CSS::Keyword::ElementScoped { };
            return true;
        }
        return false;
    };

    for (unsigned i = 0; i < 2; ++i) {
        if (consumeIdentifier() || consumeElementScoped())
            continue;
        break;
    }

    if (!identifier && !elementScoped)
        return { };

    guard.commit();

    return CSSCalc::Random::Sharing { CSSCalc::Random::SharingOptions {
        .identifier = identifier.value_or(CSS::CustomIdent { nullAtom() }),
        .elementScoped = elementScoped
    } };
}

CSSCalc::RandomSharingOptions::Auto autoRandomSharingKey(const CSS::PropertyParserState& state)
{
    return {
        .property = state.currentProperty,
        .index = state.cssRandomFunctionCount
    };
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
