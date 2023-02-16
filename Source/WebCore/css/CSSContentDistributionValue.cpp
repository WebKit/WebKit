/*
 * Copyright (C) 2015 Igalia S.L. All rights reserved.
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
#include "CSSContentDistributionValue.h"

#include "CSSValueKeywords.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSContentDistributionValue::CSSContentDistributionValue(CSSValueID distribution, CSSValueID position, CSSValueID overflow)
    : CSSValue(ContentDistributionClass)
    , m_distribution(distribution)
    , m_position(position)
    , m_overflow(overflow)
{
}

Ref<CSSContentDistributionValue> CSSContentDistributionValue::create(CSSValueID distribution, CSSValueID position, CSSValueID overflow)
{
    return adoptRef(*new CSSContentDistributionValue(distribution, position, overflow));
}

String CSSContentDistributionValue::customCSSText() const
{
    auto word1 = m_distribution;
    CSSValueID word2;
    CSSValueID word3;
    switch (m_position) {
    case CSSValueFirstBaseline:
        word2 = CSSValueFirst;
        word3 = CSSValueBaseline;
        break;
    case CSSValueLastBaseline:
        word2 = CSSValueLast;
        word3 = CSSValueBaseline;
        break;
    default:
        word2 = m_overflow;
        word3 = m_position;
        break;
    }
    return makeString(
        word1 == CSSValueInvalid ? ""_s : nameLiteral(word1),
        word1 != CSSValueInvalid && word2 != CSSValueInvalid ? " "_s : ""_s,
        word2 == CSSValueInvalid ? ""_s : nameLiteral(word2),
        word2 != CSSValueInvalid && word3 != CSSValueInvalid ? " "_s : ""_s,
        word3 == CSSValueInvalid ? ""_s : nameLiteral(word3)
    );
}

bool CSSContentDistributionValue::equals(const CSSContentDistributionValue& other) const
{
    return m_distribution == other.m_distribution && m_position == other.m_position && m_overflow == other.m_overflow;
}

}
