/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include <wtf/AutomaticThread.h>
#include <wtf/Box.h>
#include <wtf/Expected.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/RefPtr.h>
#include <wtf/StackBounds.h>

namespace JSC {

class CallFrame;
class JSGlobalObject;
class VM;

class VMTraps {
    typedef uint8_t BitField;
public:
    enum class Error {
        None,
        LockUnavailable,
        NotJITCode
    };

    enum EventType {
        // Sorted in servicing priority order from highest to lowest.
        NeedDebuggerBreak,
        NeedShellTimeoutCheck,
        NeedTermination,
        NeedWatchdogCheck,
        NumberOfEventTypes, // This entry must be last in this list.
        Invalid
    };

    class Mask {
    public:
        enum AllEventTypes { AllEventTypesTag };
        constexpr Mask(AllEventTypes)
            : m_mask(std::numeric_limits<BitField>::max())
        { }
        static constexpr Mask allEventTypes() { return Mask(AllEventTypesTag); }

        constexpr Mask(const Mask&) = default;
        constexpr Mask(Mask&&) = default;

        template<typename... Arguments>
        constexpr Mask(Arguments... args)
            : m_mask(0)
        {
            init(args...);
        }

        BitField bits() const { return m_mask; }

    private:
        template<typename... Arguments>
        constexpr void init(EventType eventType, Arguments... args)
        {
            ASSERT(eventType < NumberOfEventTypes);
            m_mask |= (1 << eventType);
            init(args...);
        }

        constexpr void init() { }

        BitField m_mask;
    };

    static constexpr Mask interruptingTraps() { return Mask(NeedShellTimeoutCheck, NeedTermination, NeedWatchdogCheck); }

    ~VMTraps();
    VMTraps();

    void willDestroyVM();

    bool needTrapHandling() { return m_needTrapHandling; }
    bool needTrapHandling(Mask mask) { return m_needTrapHandling & mask.bits(); }
    void* needTrapHandlingAddress() { return &m_needTrapHandling; }

    void notifyGrabAllLocks()
    {
        if (needTrapHandling())
            invalidateCodeBlocksOnStack();
    }

    JS_EXPORT_PRIVATE void fireTrap(EventType);

    void handleTraps(JSGlobalObject*, CallFrame*, VMTraps::Mask);

    void tryInstallTrapBreakpoints(struct SignalContext&, StackBounds);

private:
    VM& vm() const;

    bool hasTrapForEvent(Locker<Lock>&, EventType eventType, Mask mask)
    {
        ASSERT(eventType < NumberOfEventTypes);
        return (m_trapsBitField & mask.bits() & (1 << eventType));
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

    EventType takeTopPriorityTrap(Mask);

#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    class SignalSender;
    friend class SignalSender;

    void invalidateCodeBlocksOnStack();
    void invalidateCodeBlocksOnStack(CallFrame* topCallFrame);
    void invalidateCodeBlocksOnStack(Locker<Lock>& codeBlockSetLocker, CallFrame* topCallFrame);

    void addSignalSender(SignalSender*);
    void removeSignalSender(SignalSender*);
#else
    void invalidateCodeBlocksOnStack() { }
    void invalidateCodeBlocksOnStack(CallFrame*) { }
#endif

    Box<Lock> m_lock;
    Ref<AutomaticThreadCondition> m_condition;
    union {
        BitField m_needTrapHandling { 0 };
        BitField m_trapsBitField;
    };
    bool m_needToInvalidatedCodeBlocks { false };
    bool m_isShuttingDown { false };

#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    RefPtr<SignalSender> m_signalSender;
#endif

    friend class LLIntOffsetsExtractor;
    friend class SignalSender;
};

} // namespace JSC
