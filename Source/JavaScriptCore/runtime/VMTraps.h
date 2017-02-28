/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace JSC {

class VM;

class VMTraps {
public:
    enum EventType {
        // Sorted in servicing priority order from highest to lowest.
        NeedTermination,
        NeedWatchdogCheck,
        NumberOfEventTypes, // This entry must be last in this list.
        Invalid
    };

    bool needTrapHandling() { return m_needTrapHandling; }
    void* needTrapHandlingAddress() { return &m_needTrapHandling; }

    JS_EXPORT_PRIVATE void fireTrap(EventType);

    bool takeTrap(EventType);
    EventType takeTopPriorityTrap();

private:
    VM& vm() const;

    bool hasTrapForEvent(Locker<Lock>&, EventType eventType)
    {
        ASSERT(eventType < NumberOfEventTypes);
        return (m_trapsBitField & (1 << eventType));
    }
    void setTrapForEvent(Locker<Lock>&, EventType eventType)
    {
        ASSERT(eventType < NumberOfEventTypes);
        m_trapsBitField |= (1 << eventType);
    }
    void clearTrapForEvent(Locker<Lock>&, EventType eventType)
    {
        ASSERT(eventType < NumberOfEventTypes);
        m_trapsBitField &= ~(1 << eventType);
    }

    Lock m_lock;
    union {
        uint8_t m_needTrapHandling { false };
        uint8_t m_trapsBitField;
    };

    friend class LLIntOffsetsExtractor;
};

} // namespace JSC
