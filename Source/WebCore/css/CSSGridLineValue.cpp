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
#include "CSSGridLineValue.h"

#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

String CSSGridLineValue::customCSSText(const CSS::SerializationContext& context) const
{
    Vector<String> parts;
    if (m_spanValue)
        parts.append(m_spanValue->cssText(context));
    // Only return the numeric value if not 1, or if it provided without a span value.
    // https://drafts.csswg.org/css-grid-2/#grid-placement-span-int
    if (m_numericValue) {
        if (m_numericValue->isOne() != true || !m_spanValue || !m_gridLineName)
            parts.append(m_numericValue->cssText(context));
    }
    if (m_gridLineName)
        parts.append(m_gridLineName->cssText(context));
    return makeStringByJoining(parts, " "_s);
}

CSSGridLineValue::CSSGridLineValue(RefPtr<CSSPrimitiveValue>&& spanValue, RefPtr<CSSPrimitiveValue>&& numericValue, RefPtr<CSSPrimitiveValue>&& gridLineName)
    : CSSValue(ClassType::GridLineValue)
    , m_spanValue(WTFMove(spanValue))
    , m_numericValue(WTFMove(numericValue))
    , m_gridLineName(WTFMove(gridLineName))
{
}

Ref<CSSGridLineValue> CSSGridLineValue::create(RefPtr<CSSPrimitiveValue>&& spanValue, RefPtr<CSSPrimitiveValue>&& numericValue, RefPtr<CSSPrimitiveValue>&& gridLineName)
{
    return adoptRef(*new CSSGridLineValue(WTFMove(spanValue), WTFMove(numericValue), WTFMove(gridLineName)));
}

bool CSSGridLineValue::equals(const CSSGridLineValue& other) const
{
    if (m_spanValue) {
        if (!other.m_spanValue || !m_spanValue->equals(*other.m_spanValue))
            return false;
    } else if (other.m_spanValue)
        return false;

    if (m_numericValue) {
        if (!other.m_numericValue || !m_numericValue->equals(*other.m_numericValue))
            return false;
    } else if (other.m_numericValue)
        return false;

    if (m_gridLineName) {
        if (!other.m_gridLineName || !m_gridLineName->equals(*other.m_gridLineName))
            return false;
    } else if (other.m_gridLineName)
        return false;

    return true;
}

} // namespace WebCore
