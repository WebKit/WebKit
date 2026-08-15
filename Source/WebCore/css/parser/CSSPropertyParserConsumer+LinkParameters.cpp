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
#include "CSSPropertyParserConsumer+LinkParameters.h"

#include "CSSLinkParameter.h"
#include "CSSParamValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSSubstitutionParser.h"
#include "CSSValueKeywords.h"
#include "CSSVariableData.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// <param()> = param( <dashed-ident> , <declaration-value>? )
// https://drafts.csswg.org/css-link-params/#funcdef-param
RefPtr<CSSValue> consumeParamFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().functionId() != CSSValueParam)
        return nullptr;

    auto arguments = consumeFunction(range);

    auto name = consumeUnresolvedDashedIdent(arguments, state);
    if (!name)
        return nullptr;

    // The value may be empty but the comma is required.
    // https://github.com/w3c/csswg-drafts/issues/13767
    if (!consumeCommaIncludingWhitespace(arguments))
        return nullptr;

    // A value containing substitutions cannot be resolved until computed-value time, so
    // fail here and let the declaration be stored unresolved instead. The longhand parser
    // runs before the substitution path, so succeeding would swallow the substitution.
    if (CSSSubstitutionParser::containsSubstitutionFunctions(arguments, state.context))
        return nullptr;

    return CSSParamValue::create(CSS::ParamFunction { CSS::LinkParameter { WTF::move(*name), CSS::DeclarationValue { CSSVariableData::create(arguments) } } });
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
