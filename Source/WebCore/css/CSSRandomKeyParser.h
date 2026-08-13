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

#pragma once

#include "CSSCalcTree.h"
#include "CSSPropertyNames.h"
#include <optional>
#include <wtf/Compiler.h>
#include <wtf/Function.h>

namespace WebCore {

class CSSParserTokenRange;

namespace CSS {
struct PropertyParserState;
}

namespace CSSPropertyParserHelpers {

// <random-key> = auto | <random-cache-key> | fixed <number [0,1]>
// <random-cache-key> = <dashed-ident> || element-scoped || [ property-scoped | property-index-scoped | <random-ua-ident> ]
// https://drafts.csswg.org/css-values-5/#random-caching
//
// <random-ua-ident> is intentionally not yet supported by this consumer (follow-up).
//
// The implementation-derived parts of a <random-key> come from the caller: `property` identifies the
// property the value is being parsed for, `autoElementScoped` is the scoping `auto` resolves to, and
// `consumeIndex` yields the value index under the caller's own scheme (parse-time for random(),
// substitution-time for random-item()). `consumeIndex` is only called for the productions that actually
// carry an index (`auto` and property-index-scoped), so property-scoped cannot disturb a caller's index
// counter.
struct RandomKeySource {
    CSSCalc::RandomScopedProperty property;
    std::optional<CSS::Keyword::ElementScoped> autoElementScoped;
};

std::optional<CSSCalc::Random::Sharing> consumeUnresolvedRandomKey(CSSParserTokenRange&, CSS::PropertyParserState&, const RandomKeySource&, NOESCAPE const Function<unsigned()>& consumeIndex);

// Builds the sharing `auto` resolves to. Shared with callers that need it for an omitted <random-key>,
// which means the same thing as `auto`.
CSSCalc::RandomSharingAuto randomSharingAuto(const RandomKeySource&, unsigned index);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
