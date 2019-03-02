/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <cmath>
#include <wtf/dtoa.h>

namespace WTF {

class DecimalNumber {
public:
    explicit DecimalNumber(double d)
    {
        ASSERT(std::isfinite(d));
        dtoa(m_significand, d, m_sign, m_exponent, m_precision);

        ASSERT(m_precision);
        // Zero should always have exponent 0.
        ASSERT(m_significand[0] != '0' || !m_exponent);
        // No values other than zero should have a leading zero.
        ASSERT(m_significand[0] != '0' || m_precision == 1);
        // No values other than zero should have trailing zeros.
        ASSERT(m_significand[0] == '0' || m_significand[m_precision - 1] != '0');
    }

    WTF_EXPORT_PRIVATE unsigned bufferLengthForStringDecimal() const;
    WTF_EXPORT_PRIVATE unsigned bufferLengthForStringExponential() const;

    WTF_EXPORT_PRIVATE unsigned toStringDecimal(LChar* buffer, unsigned bufferLength) const;
    WTF_EXPORT_PRIVATE unsigned toStringExponential(LChar* buffer, unsigned bufferLength) const;

private:
    bool m_sign;
    int m_exponent;
    DtoaBuffer m_significand;
    unsigned m_precision;
};

} // namespace WTF

using WTF::DecimalNumber;
