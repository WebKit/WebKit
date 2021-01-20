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

#if ENABLE(DFG_JIT)

#include "DFGEpoch.h"
#include "DFGStructureClobberState.h"
#include <wtf/PrintStream.h>

namespace JSC { namespace DFG {

class AbstractValueClobberEpoch {
public:
    AbstractValueClobberEpoch()
    {
    }
    
    static AbstractValueClobberEpoch first(StructureClobberState clobberState)
    {
        AbstractValueClobberEpoch result;
        result.m_value = Epoch::first().toUnsigned() << epochShift;
        switch (clobberState) {
        case StructuresAreWatched:
            result.m_value |= watchedFlag;
            break;
        case StructuresAreClobbered:
            break;
        }
        return result;
    }
    
    void clobber()
    {
        m_value += step;
        m_value &= ~watchedFlag;
    }
    
    void observeInvalidationPoint()
    {
        m_value |= watchedFlag;
    }
    
    bool operator==(const AbstractValueClobberEpoch& other) const
    {
        return m_value == other.m_value;
    }
    
    bool operator!=(const AbstractValueClobberEpoch& other) const
    {
        return !(*this == other);
    }
    
    StructureClobberState structureClobberState() const
    {
        return m_value & watchedFlag ? StructuresAreWatched : StructuresAreClobbered;
    }
    
    Epoch clobberEpoch() const
    {
        return Epoch::fromUnsigned(m_value >> epochShift);
    }
    
    void dump(PrintStream&) const;
    
private:
    static constexpr unsigned step = 2;
    static constexpr unsigned watchedFlag = 1;
    static constexpr unsigned epochShift = 1;
    
    unsigned m_value { 0 };
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
