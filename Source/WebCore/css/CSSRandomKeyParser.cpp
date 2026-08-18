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
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserState.h"
#include "CSSValueKeywords.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// <random-ua-ident> = ua- PROPERTY [ - INDEX ]?, where INDEX is 1-based. Returns nullopt when the
// ident does not name a known property, which makes the whole <random-key> invalid.
// FIXME: § 9.4.1 also spells an in-custom-function form, ua-FUNCTIONNAME-PROPERTY[-INDEX]. Both names
// can contain hyphens, so splitting it is ambiguous; see https://github.com/w3c/csswg-drafts/issues/14330
static std::optional<Variant<CSSCalc::Random::Key::PropertyScoped, CSSCalc::Random::Key::PropertyIndexScoped>> parseRandomUAIdent(StringView ident)
{
    auto body = ident.substring(3);

    if (auto lastHyphen = body.reverseFind('-'); lastHyphen != notFound) {
        if (auto index = parseInteger<unsigned>(body.substring(lastHyphen + 1)); index >= 1) {
            auto property = cssPropertyID(body.left(lastHyphen));
            if (property == CSSPropertyInvalid)
                return { };
            return { CSSCalc::Random::Key::PropertyIndexScoped { CSSCalc::RandomScopedProperty { property, nullAtom() }, *index - 1 } };
        }
    }

    auto property = cssPropertyID(body);
    if (property == CSSPropertyInvalid)
        return { };
    return { CSSCalc::Random::Key::PropertyScoped { CSSCalc::RandomScopedProperty { property, nullAtom() } } };
}

std::optional<CSSCalc::Random::Sharing> consumeUnresolvedRandomKey(CSSParserTokenRange& tokens, CSS::PropertyParserState& state, const RandomKeySource& source, NOESCAPE const Function<unsigned()>& consumeIndex)
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

    // `auto` is a standalone <random-key> alternative (its scoping is chosen by the caller); it does not
    // combine with the <random-cache-key> keywords, so it is handled before the || loop below.
    if (tokens.peek().id() == CSSValueAuto) {
        tokens.consumeIncludingWhitespace();
        return CSSCalc::Random::Sharing { randomSharingAuto(source, consumeIndex()) };
    }

    CSSCalc::Random::Key key;

    CSSParserTokenRangeGuard guard { tokens };

    auto consumeName = [&] -> bool {
        if (key.name)
            return false;
        if (auto dashedIdent = consumeUnresolvedDashedIdent(tokens, state)) {
            key.name = WTF::move(*dashedIdent);
            return true;
        }
        return false;
    };
    auto consumeElementScoped = [&] -> bool {
        if (key.elementScoped)
            return false;
        if (tokens.peek().id() == CSSValueElementScoped) {
            tokens.consumeIncludingWhitespace();
            key.elementScoped = CSS::Keyword::ElementScoped { };
            return true;
        }
        return false;
    };
    // property-scoped and property-index-scoped are mutually exclusive, so a single lambda handles both.
    auto consumeProperty = [&] -> bool {
        if (key.propertyScoped)
            return false;
        if (tokens.peek().id() == CSSValuePropertyScoped) {
            tokens.consumeIncludingWhitespace();
            // No index is consumed: property-scoped keys on the property alone, and taking an index here
            // would shift the index of any later `auto` or property-index-scoped in the same value.
            key.propertyScoped = CSSCalc::Random::Key::PropertyScoped { source.property };
            return true;
        }
        if (tokens.peek().id() == CSSValuePropertyIndexScoped) {
            tokens.consumeIncludingWhitespace();
            key.propertyScoped = CSSCalc::Random::Key::PropertyIndexScoped { source.property, consumeIndex() };
            return true;
        }
        // <random-ua-ident> = <custom-ident> starting with `ua-`. It spells out the property, and
        // optionally the index, that the keywords derive implicitly, so `ua-height-1` names the same
        // key as property-index-scoped does while parsing `height`. The index is 1-based in the ident.
        // No index is consumed from the caller: the ident says which one it means.
        if (tokens.peek().type() == IdentToken) {
            if (auto identValue = tokens.peek().value(); startsWithLettersIgnoringASCIICase(identValue, "ua-"_s)) {
                if (auto scoped = parseRandomUAIdent(identValue)) {
                    tokens.consumeIncludingWhitespace();
                    key.propertyScoped = WTF::move(*scoped);
                    return true;
                }
                return false;
            }
        }
        return false;
    };

    for (unsigned i = 0; i < 3; ++i) {
        if (consumeName() || consumeElementScoped() || consumeProperty())
            continue;
        break;
    }

    if (!key.name && !key.elementScoped && !key.propertyScoped)
        return { };

    guard.commit();

    return CSSCalc::Random::Sharing { WTF::move(key) };
}

CSSCalc::RandomSharingAuto randomSharingAuto(const RandomKeySource& source, unsigned index)
{
    return {
        .property = source.property,
        .index = index,
        .elementScoped = source.autoElementScoped
    };
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
