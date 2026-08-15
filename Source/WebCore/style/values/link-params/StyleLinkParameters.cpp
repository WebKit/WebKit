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
#include "StyleLinkParameters.h"

#include "CSSDeclarationValue.h"
#include "CSSLinkParameter.h"
#include "CSSParamValue.h"
#include "CSSParserTokenRange.h"
#include "CSSVariableData.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<LinkParameter>::operator()(BuilderState& state, const CSSValue& value) -> LinkParameter
{
    RefPtr parameter = requiredDowncast<CSSParamValue>(state, value);
    if (!parameter)
        return { CustomIdent { nullAtom() }, DeclarationValue { CSSVariableData::create(CSSParserTokenRange { }) } };

    return { CustomIdent { parameter->parameter()->name.value }, toStyle(parameter->parameter()->value, state) };
}

auto CSSValueCreation<LinkParameter>::operator()(CSSValuePool&, const ComputedStyle& style, const LinkParameter& parameter) -> Ref<CSSValue>
{
    return CSSParamValue::create(CSS::ParamFunction { CSS::LinkParameter { CSS::CustomIdent { parameter.name.value }, toCSS(parameter.value, style) } });
}

} // namespace Style
} // namespace WebCore
