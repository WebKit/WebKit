/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "CSSClip.h"
#include "CSSColor.h"
#include "CSSContent.h"
#include "CSSCustomIdent.h"
#include "CSSFontFamilyName.h"
#include "CSSKeyword.h"
#include "CSSPrimitiveNumericRaw.h"
#include "CSSString.h"
#include "CSSURL.h"
#include "CSSUnevaluatedCalc.h"
#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>

namespace WebCore {

struct DeprecatedCSSOMPrimitiveValueData {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(DeprecatedCSSOMPrimitiveValueData);

    using TypeErasedValue = Function<String(const CSS::SerializationContext&)>;
    using NumericRaw = CSS::UnconstrainedPrimitiveNumericRaw;
    using NumericCalc = CSS::UnevaluatedCalcBase;

    Variant<
        TypeErasedValue,
        NumericRaw,
        NumericCalc,
        CSS::CustomIdent,
        CSS::Keyword,
        CSS::String,
        CSS::FontFamilyName,
        CSS::URL,
        CSS::Color,
        CSS::ContentCounterFunction,
        CSS::ContentCountersFunction,
        CSS::ContentLegacyAttrFunction,
        CSS::ClipRect
    > value;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }
};

} // namespace WebCore
