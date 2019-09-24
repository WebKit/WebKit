/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "GCLogging.h"
#include <wtf/PrintStream.h>

namespace JSC {

class OptionRange {
private:
    enum RangeState { Uninitialized, InitError, Normal, Inverted };
public:
    OptionRange& operator= (const int& rhs)
    { // Only needed for initialization
        if (!rhs) {
            m_state = Uninitialized;
            m_rangeString = 0;
            m_lowLimit = 0;
            m_highLimit = 0;
        }
        return *this;
    }

    bool init(const char*);
    bool isInRange(unsigned);
    const char* rangeString() const { return (m_state > InitError) ? m_rangeString : s_nullRangeStr; }
    
    void dump(PrintStream& out) const;

private:
    static const char* const s_nullRangeStr;

    RangeState m_state;
    const char* m_rangeString;
    unsigned m_lowLimit;
    unsigned m_highLimit;
};

// For storing for an option value:
union OptionEntry {
    using Bool = bool;
    using Unsigned = unsigned;
    using Double = double;
    using Int32 = int32_t;
    using Size = size_t;
    using OptionRange = JSC::OptionRange;
    using OptionString = const char*;
    using GCLogLevel = GCLogging::Level;

    bool valBool;
    unsigned valUnsigned;
    double valDouble;
    int32_t valInt32;
    size_t valSize;
    OptionRange valOptionRange;
    const char* valOptionString;
    GCLogging::Level valGCLogLevel;
};

} // namespace JSC
