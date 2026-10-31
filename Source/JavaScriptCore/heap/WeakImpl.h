/*
 * Copyright (C) 2012, 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/Weak.h>
#include <wtf/Atomics.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class WeakHandleOwner;

class WeakImpl {
public:
    enum State {
        Live        = 0x0,
        Dead        = 0x1,
        Finalized   = 0x2,
        Deallocated = 0x3
    };

    enum {
        StateMask   = 0x3
    };

    WeakImpl();
    void initialize(JSValue, WeakHandleOwner*, void* context);

    State state();
    void LEGACY_NODELETE clear();

    JSValue jsValue() { return JSValue::decode(m_jsValue.load<MemoryOrder::Relaxed>()); }
    WeakHandleOwner* weakHandleOwner() { return weakHandleOwnerOf(stateWord()); }
    void* context() { return m_context.load<MemoryOrder::Relaxed>(); }

    static constexpr ptrdiff_t offsetOfJSValue() { return OBJECT_OFFSETOF(WeakImpl, m_jsValue); }
    static constexpr ptrdiff_t offsetOfWeakHandleOwner() { return OBJECT_OFFSETOF(WeakImpl, m_weakHandleOwner); }

private:
    friend class WeakBlock;

    static State stateOf(uintptr_t word) { return static_cast<State>(word & StateMask); }
    static WeakHandleOwner* weakHandleOwnerOf(uintptr_t word) { return std::bit_cast<WeakHandleOwner*>(word & ~static_cast<uintptr_t>(StateMask)); }
    static uintptr_t withState(uintptr_t word, State state) { return (word & ~static_cast<uintptr_t>(StateMask)) | state; }

    struct WeakImplSnapshot {
        explicit operator bool() { return !!jsValue; }

        JSValue jsValue { };
        WeakHandleOwner* weakHandleOwner { nullptr };
        void* context { nullptr };
    };
    void setDead();
    WeakImplSnapshot trySetFinalized(uintptr_t deadStateWord);
    uintptr_t stateWord();

    Atomic<EncodedJSValue> m_jsValue;
    Atomic<uintptr_t> m_weakHandleOwner;
    Atomic<void*> m_context { nullptr };
};

inline WeakImpl::WeakImpl()
    : m_jsValue(JSValue::encode(JSValue()))
    , m_weakHandleOwner(Deallocated)
{
}

inline void WeakImpl::initialize(JSValue jsValue, WeakHandleOwner* weakHandleOwner, void* context)
{
    ASSERT(state() == Deallocated);
    ASSERT(jsValue && jsValue.isCell());
    ASSERT(!(std::bit_cast<uintptr_t>(weakHandleOwner) & StateMask));
    m_jsValue.store<MemoryOrder::Relaxed>(JSValue::encode(jsValue));
    m_context.store<MemoryOrder::Relaxed>(context);
    m_weakHandleOwner.store<MemoryOrder::Relaxed>(std::bit_cast<uintptr_t>(weakHandleOwner) | Live);
}

inline uintptr_t WeakImpl::stateWord()
{
    return m_weakHandleOwner.load<MemoryOrder::Relaxed>();
}

inline WeakImpl::State WeakImpl::state()
{
    return stateOf(stateWord());
}

inline void WeakImpl::setDead()
{
    ASSERT(state() == Live);
    m_weakHandleOwner.store<MemoryOrder::Relaxed>(withState(stateWord(), Dead));
}

inline WeakImpl::WeakImplSnapshot WeakImpl::trySetFinalized(uintptr_t deadStateWord)
{
    ASSERT(stateOf(deadStateWord) == Dead);
    JSValue jsValue = JSValue::decode(m_jsValue.load<MemoryOrder::Relaxed>());
    void* context = m_context.load<MemoryOrder::Relaxed>();
    // A concurrent clear() may have already deallocated.
    if (m_weakHandleOwner.compareExchangeStrong<MemoryOrder::AcquireRelease>(deadStateWord, withState(deadStateWord, Finalized)) != deadStateWord)
        return { };
    return { jsValue, weakHandleOwnerOf(deadStateWord), context };
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
