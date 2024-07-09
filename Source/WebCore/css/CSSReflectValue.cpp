/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
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
#include "CSSReflectValue.h"

#include <wtf/text/MakeString.h>

namespace WebCore {

CSSReflectValue::CSSReflectValue(CSSValueID direction, Ref<CSSPrimitiveValue> offset, RefPtr<CSSValue> mask)
    : CSSValue(ReflectClass)
    , m_direction(direction)
    , m_offset(WTFMove(offset))
    , m_mask(WTFMove(mask))
{
}

Ref<CSSReflectValue> CSSReflectValue::create(CSSValueID direction, Ref<CSSPrimitiveValue> offset, RefPtr<CSSValue> mask)
{
    return adoptRef(*new CSSReflectValue(direction, WTFMove(offset), WTFMove(mask)));
}

String CSSReflectValue::customCSSText() const
{
    if (m_mask)
        return makeString(nameLiteral(m_direction), ' ', m_offset->cssText(), ' ', m_mask->cssText());
    return makeString(nameLiteral(m_direction), ' ', m_offset->cssText());
}

bool CSSReflectValue::equals(const CSSReflectValue& other) const
{
    return m_direction == other.m_direction
        && compareCSSValue(m_offset, other.m_offset)
        && compareCSSValuePtr(m_mask, other.m_mask);
}

} // namespace WebCore
