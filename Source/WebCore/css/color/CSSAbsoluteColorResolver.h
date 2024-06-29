/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSColorConversion+ToColor.h"
#include "CSSColorConversion+ToTypedColor.h"
#include "CSSColorDescriptors.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include "Color.h"
#include <optional>

namespace WebCore {

template<typename Descriptor, unsigned Index>
using CSSUnresolvedAbsoluteColorComponent = GetComponentResultWithCalcResult<Descriptor, Index>;

template<typename D>
struct CSSAbsoluteColorResolver {
    using Descriptor = D;

    CSSColorParseTypeWithCalc<Descriptor> components;
    unsigned nestingLevel;
};

template<typename Descriptor>
Color resolve(const CSSAbsoluteColorResolver<Descriptor>& absolute)
{
    // Evaluated any calc values to their corresponding channel value.
    auto components = CSSColorParseType<Descriptor> {
        evaluateCalc(std::get<0>(absolute.components), { }),
        evaluateCalc(std::get<1>(absolute.components), { }),
        evaluateCalc(std::get<2>(absolute.components), { }),
        evaluateCalc(std::get<3>(absolute.components), { })
    };

    // Normalize values into their numeric form, forming a validated typed color.
    auto typedColor = convertToTypedColor<Descriptor>(components, 1.0);

    // Convert the validated typed color into a `Color`,
    return convertToColor<Descriptor, CSSColorFunctionForm::Absolute>(typedColor, absolute.nestingLevel);
}

} // namespace WebCore
