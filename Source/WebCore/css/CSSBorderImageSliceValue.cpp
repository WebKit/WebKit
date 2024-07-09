/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "CSSBorderImageSliceValue.h"

#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSBorderImageSliceValue::CSSBorderImageSliceValue(Quad slices, bool fill)
    : CSSValue(BorderImageSliceClass)
    , m_slices(WTFMove(slices))
    , m_fill(fill)
{
}

CSSBorderImageSliceValue::~CSSBorderImageSliceValue() = default;

Ref<CSSBorderImageSliceValue> CSSBorderImageSliceValue::create(Quad slices, bool fill)
{
    return adoptRef(*new CSSBorderImageSliceValue(WTFMove(slices), fill));
}

String CSSBorderImageSliceValue::customCSSText() const
{
    if (m_fill)
        return makeString(m_slices.cssText(), " fill"_s);
    return m_slices.cssText();
}

bool CSSBorderImageSliceValue::equals(const CSSBorderImageSliceValue& other) const
{
    return m_fill == other.m_fill && m_slices.equals(other.m_slices);
}

} // namespace WebCore
