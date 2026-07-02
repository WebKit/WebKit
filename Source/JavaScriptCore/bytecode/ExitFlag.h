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

#include "ExitingInlineKind.h"

namespace WTF {
class PrintStream;
} // namespace WTF

namespace JSC {

class ExitFlag {
public:
    ExitFlag() { }
    
    ExitFlag(bool value, ExitingInlineKind inlineKind)
    {
        if (!value)
            return;
        
        switch (inlineKind) {
        case ExitFromAnyInlineKind:
            m_bits = trueNotInlined | trueInlined;
            break;
        case ExitFromNotInlined:
            m_bits = trueNotInlined;
            break;
        case ExitFromInlined:
            m_bits = trueInlined;
            break;
        }
    }
    
    ExitFlag operator|(const ExitFlag& other) const
    {
        ExitFlag result;
        result.m_bits = m_bits | other.m_bits;
        return result;
    }
    
    ExitFlag& operator|=(const ExitFlag& other)
    {
        *this = *this | other;
        return *this;
    }
    
    ExitFlag operator&(const ExitFlag& other) const
    {
        ExitFlag result;
        result.m_bits = m_bits & other.m_bits;
        return result;
    }
    
    ExitFlag& operator&=(const ExitFlag& other)
    {
        *this = *this & other;
        return *this;
    }
    
    explicit operator bool() const
    {
        return !!m_bits;
    }
    
    bool isSet(ExitingInlineKind inlineKind) const
    {
        return !!(*this & ExitFlag(true, inlineKind));
    }
    
    void dump(WTF::PrintStream&) const;
    
private:
    static constexpr uint8_t trueNotInlined = 1;
    static constexpr uint8_t trueInlined = 2;
    
    uint8_t m_bits { 0 };
};

} // namespace JSC

