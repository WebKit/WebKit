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

#include "config.h"
#include "CSSAnchorValue.h"

#include "CSSPrimitiveValue.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

void CSSAnchorValue::collectAnchorNames(HashSet<String>& accumulator) const
{
    if (m_anchorElement)
        accumulator.add(m_anchorElement->stringValue());
    // FIXME: Support collecting anchor names from m_fallback
}


Ref<CSSAnchorValue> CSSAnchorValue::create(RefPtr<CSSPrimitiveValue>&& anchorElement, Ref<CSSValue>&& anchorSide, RefPtr<CSSPrimitiveValue>&& fallback)
{
    return adoptRef(*new CSSAnchorValue(WTFMove(anchorElement), WTFMove(anchorSide), WTFMove(fallback)));
}

void CSSAnchorValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    if (m_fallback)
        m_fallback->collectComputedStyleDependencies(dependencies);
}

String CSSAnchorValue::customCSSText() const
{
    auto element = m_anchorElement ? m_anchorElement->cssText() : String { };
    auto side = m_anchorSide->cssText();
    auto fallback = m_fallback ? m_fallback->cssText() : String { };
    auto optionalSpace = element.isEmpty() ? ""_s : " "_s;
    auto optionalComma = fallback.isEmpty() ? ""_s : ", "_s;
    return makeString("anchor("_s, element, optionalSpace, side, optionalComma, fallback, ')');
}

bool CSSAnchorValue::equals(const CSSAnchorValue& other) const
{
    return compareCSSValuePtr(m_anchorElement, other.m_anchorElement)
        && compareCSSValue(m_anchorSide, other.m_anchorSide)
        && compareCSSValuePtr(m_fallback, other.m_fallback);
}

String CSSAnchorValue::anchorElementString() const
{
    return m_anchorElement ? m_anchorElement->stringValue() : nullString();
}

Ref<CSSValue> CSSAnchorValue::anchorSide() const
{
    return m_anchorSide;
}

} // namespace WebCore
