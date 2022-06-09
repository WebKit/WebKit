/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_TYPED_OM)

#include "CSSNumericValue.h"
#include <wtf/RefCounted.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSUnitValue final : public CSSNumericValue {
    WTF_MAKE_ISO_ALLOCATED(CSSUnitValue);
public:
    static Ref<CSSUnitValue> create(double value, const String& unit)
    {
        return adoptRef(*new CSSUnitValue(value, unit));
    }

    // FIXME: not correct.
    String toString() const final { return makeString((int) m_value, m_unit); }

    double value() const { return m_value; }
    void setValue(double value) { m_value = value; }
    const String& unit() const { return m_unit; }
    void setUnit(const String& unit) { m_unit = unit; }

private:
    CSSUnitValue(double value, const String& unit)
        : m_value(value)
        , m_unit(unit)
    {
    }

    CSSStyleValueType getType() const final { return CSSStyleValueType::CSSUnitValue; }

    double m_value;
    String m_unit;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSUnitValue)
    static bool isType(const WebCore::CSSStyleValue& styleValue) { return styleValue.getType() == WebCore::CSSStyleValueType::CSSUnitValue; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
