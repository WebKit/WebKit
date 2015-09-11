/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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

#ifndef WeakImpl_h
#define WeakImpl_h

#include "JSCJSValue.h"

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
    WeakImpl(JSCell&, WeakHandleOwner*, void* context);

    State state() const;
    void setState(State);

    JSCell* cell();
    WeakHandleOwner* weakHandleOwner();
    void* context();

    static WeakImpl* asWeakImpl(JSCell**);

private:
    friend class WeakBlock;

    JSCell* m_cell { nullptr };
    WeakHandleOwner* m_weakHandleOwner { nullptr };
    void* m_context { nullptr };
};

inline WeakImpl::WeakImpl()
{
    setState(Deallocated);
}

inline WeakImpl::WeakImpl(JSCell& cell, WeakHandleOwner* weakHandleOwner, void* context)
    : m_cell(&cell)
    , m_weakHandleOwner(weakHandleOwner)
    , m_context(context)
{
    ASSERT(state() == Live);
}

inline WeakImpl::State WeakImpl::state() const
{
    return static_cast<State>(reinterpret_cast<uintptr_t>(m_weakHandleOwner) & StateMask);
}

inline void WeakImpl::setState(WeakImpl::State state)
{
    ASSERT(state >= this->state());
    m_weakHandleOwner = reinterpret_cast<WeakHandleOwner*>((reinterpret_cast<uintptr_t>(m_weakHandleOwner) & ~StateMask) | state);
}

inline JSCell* WeakImpl::cell()
{
    return m_cell;
}

inline WeakHandleOwner* WeakImpl::weakHandleOwner()
{
    return reinterpret_cast<WeakHandleOwner*>((reinterpret_cast<uintptr_t>(m_weakHandleOwner) & ~StateMask));
}

inline void* WeakImpl::context()
{
    return m_context;
}

inline WeakImpl* WeakImpl::asWeakImpl(JSCell** slot)
{
    return reinterpret_cast_ptr<WeakImpl*>(reinterpret_cast_ptr<char*>(slot));
}

} // namespace JSC

#endif // WeakImpl_h
