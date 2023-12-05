/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSViewValue.h"

#include "CSSPrimitiveValueMappings.h"

namespace WebCore {

String CSSViewValue::customCSSText() const
{
    auto hasAxis = m_axis && m_axis->valueID() != CSSValueBlock;
    auto hasEndInset = m_endInset && m_endInset != m_startInset;
    auto hasStartInset = (m_startInset && m_startInset->valueID() != CSSValueAuto) || (m_startInset && m_startInset->valueID() == CSSValueAuto && hasEndInset);

    return makeString(
        "view("_s,
        hasAxis ? m_axis->cssText() : ""_s,
        hasAxis && hasStartInset ? " "_s : ""_s,
        hasStartInset ? m_startInset->cssText() : ""_s,
        hasStartInset && hasEndInset ? " "_s : ""_s,
        hasEndInset ? m_endInset->cssText() : ""_s,
        ")"_s
    );
}

bool CSSViewValue::equals(const CSSViewValue& other) const
{
    return compareCSSValuePtr(m_axis, other.m_axis)
        && compareCSSValuePtr(m_startInset, other.m_startInset)
        && compareCSSValuePtr(m_endInset, other.m_endInset);
}

}
