/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef FTLWeight_h
#define FTLWeight_h

#if ENABLE(FTL_JIT)

#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace FTL {

class Weight {
public:
    Weight()
        : m_value(std::numeric_limits<float>::quiet_NaN())
    {
    }
    
    explicit Weight(float value)
        : m_value(value)
    {
    }
    
    bool isSet() const { return m_value == m_value; }
    bool operator!() const { return !isSet(); }
    
    float value() const { return m_value; }
    
    unsigned scaleToTotal(double total) const
    {
        // LLVM accepts branch 32-bit unsigned branch weights but in dumps it might
        // display them as signed values. We don't need all 32 bits, so we just use the
        // 31 bits.
        double result = static_cast<double>(m_value) * INT_MAX / total;
        if (result < 0)
            return 0;
        if (result > INT_MAX)
            return INT_MAX;
        return static_cast<unsigned>(result);
    }
    
private:
    float m_value;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLWeight_h

