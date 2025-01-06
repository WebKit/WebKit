/*
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

#include "CSSFilterFunction.h"
#include "DeprecatedCSSOMValue.h"

namespace WebCore {

// This class is needed to maintain compatibility with the historical CSSOM representation of the `filter` related properties.
// It should be used only as an element in a DeprecatedCSSOMValueList created by CSSAppleColorFilterPropertyValue or CSSFilterPropertyValue.
class DeprecatedCSSOMFilterFunctionValue final : public DeprecatedCSSOMValue {
public:
    static Ref<DeprecatedCSSOMFilterFunctionValue> create(CSS::FilterFunction, CSSStyleDeclaration&);

    String cssText() const;
    unsigned short cssValueType() const { return CSS_CUSTOM; }

private:
    DeprecatedCSSOMFilterFunctionValue(CSS::FilterFunction&&, CSSStyleDeclaration&);

    CSS::FilterFunction m_filter;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSSOM_VALUE(DeprecatedCSSOMFilterFunctionValue, isFilterFunctionValue())
