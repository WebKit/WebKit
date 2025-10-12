/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "JSInternalFieldObjectImpl.h"

namespace JSC {

class JSPromiseReaction final : public JSCell {
public:
    using Base = JSCell;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.promiseReactionSpace<mode>();
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSPromiseReaction* create(VM&, JSValue promise, JSValue onFulfilled, JSValue onRejected, JSValue context, JSPromiseReaction* next);

    JSValue promise() const { return m_promise.get(); }
    JSValue onFulfilled() const { return m_onFulfilled.get(); }
    JSValue onRejected() const { return m_onRejected.get(); }
    JSValue context() const { return m_context.get(); }
    JSPromiseReaction* next() const { return m_next.get(); }

    void setPromise(VM& vm, JSValue value) { m_promise.set(vm, this, value); }
    void setOnFulfilled(VM& vm, JSValue value) { m_onFulfilled.set(vm, this, value); }
    void setOnRejected(VM& vm, JSValue value) { m_onRejected.set(vm, this, value); }
    void setContext(VM& vm, JSValue value) { m_context.set(vm, this, value); }
    void setNext(VM& vm, JSPromiseReaction* value) { m_next.setMayBeNull(vm, this, value); }

private:
    JSPromiseReaction(VM& vm, Structure* structure, JSValue promise, JSValue onFulfilled, JSValue onRejected, JSValue context, JSPromiseReaction* next)
        : Base(vm, structure)
        , m_promise(promise, WriteBarrierEarlyInit)
        , m_onFulfilled(onFulfilled, WriteBarrierEarlyInit)
        , m_onRejected(onRejected, WriteBarrierEarlyInit)
        , m_context(context, WriteBarrierEarlyInit)
        , m_next(next, WriteBarrierEarlyInit)
    {
    }

    WriteBarrier<Unknown> m_promise;
    WriteBarrier<Unknown> m_onFulfilled;
    WriteBarrier<Unknown> m_onRejected;
    WriteBarrier<Unknown> m_context;
    WriteBarrier<JSPromiseReaction> m_next;
};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseReaction);

} // namespace JSC
