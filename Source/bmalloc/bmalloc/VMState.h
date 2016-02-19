/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#ifndef VMState_h
#define VMState_h

namespace bmalloc {

class VMState {
public:
    enum class HasPhysical : bool {
        False = false,
        True = true
    };

    enum State : unsigned {
        Invalid = 0x0,
        Physical = 0x1,
        Virtual = 0x2,
        Mixed = 0x3
    };

    VMState(State vmState)
        : m_state(vmState)
    {
    }

    explicit VMState(unsigned vmState)
        : m_state(static_cast<State>(vmState))
    {
    }

    inline bool hasPhysical()
    {
        return !!(m_state & VMState::Physical);
    }

    inline bool hasVirtual()
    {
        return !!(m_state & VMState::Virtual);
    }

    inline void merge(VMState otherVMState)
    {
        m_state = static_cast<State>(m_state | otherVMState.m_state);
    }

    bool operator==(VMState other) const { return m_state == other.m_state; }
    explicit operator unsigned() const { return m_state; }

private:
    State m_state;
};

} // namespace bmalloc

#endif // VMState_h
