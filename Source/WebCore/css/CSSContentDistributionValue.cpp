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

namespace WebCore {

CSSContentDistributionValue::CSSContentDistributionValue(CSSValueID distribution, CSSValueID position, CSSValueID overflow)
    : CSSValue(CSSContentDistributionClass)
    , m_distribution(distribution)
    , m_position(position)
    , m_overflow(overflow)
{
}

CSSContentDistributionValue::~CSSContentDistributionValue() = default;

String CSSContentDistributionValue::customCSSText() const
{
    StringBuilder builder;
    if (m_distribution != CSSValueInvalid)
        builder.append(nameLiteral(m_distribution));
    if (m_position != CSSValueInvalid) {
        if (m_position == CSSValueFirstBaseline)
            builder.append(builder.isEmpty() ? "" : " ", "first baseline");
        else if (m_position == CSSValueLastBaseline)
            builder.append(builder.isEmpty() ? "" : " ", "last baseline");
        else {
            if (m_overflow != CSSValueInvalid)
                builder.append(builder.isEmpty() ? "" : " ", nameLiteral(m_overflow));
            builder.append(builder.isEmpty() ? "" : " ", nameLiteral(m_position));
        }
    }
    return builder.toString();
}

bool CSSContentDistributionValue::equals(const CSSContentDistributionValue& other) const
{
    return m_distribution == other.m_distribution && m_position == other.m_position && m_overflow == other.m_overflow;
}

}
