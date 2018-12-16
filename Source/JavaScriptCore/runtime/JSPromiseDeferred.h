/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "JSCast.h"
#include "JSPromise.h"
#include "Structure.h"

namespace JSC {

class Exception;
class JSPromiseConstructor;
class JSFunction;

class JSPromiseDeferred : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    struct DeferredData {
        WTF_FORBID_HEAP_ALLOCATION;
    public:
        JSPromise* promise { nullptr };
        JSFunction* resolve { nullptr };
        JSFunction* reject { nullptr };
    };
    static DeferredData createDeferredData(ExecState*, JSGlobalObject*, JSPromiseConstructor*);

    JS_EXPORT_PRIVATE static JSPromiseDeferred* tryCreate(ExecState*, JSGlobalObject*);
    JS_EXPORT_PRIVATE static JSPromiseDeferred* create(VM&, JSPromise*, JSFunction* resolve, JSFunction* reject);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    DECLARE_EXPORT_INFO;

    JSPromise* promise() const { return m_promise.get(); }
    JSFunction* resolve() const { return m_resolve.get(); }
    JSFunction* reject() const { return m_reject.get(); }

    JS_EXPORT_PRIVATE void resolve(ExecState*, JSValue);
    JS_EXPORT_PRIVATE void reject(ExecState*, JSValue);
    JS_EXPORT_PRIVATE void reject(ExecState*, Exception*);

#ifndef NDEBUG
    void promiseAsyncPending() { m_promiseIsAsyncPending = true; }
#endif

protected:
    JSPromiseDeferred(VM&, Structure*);
    void finishCreation(VM&, JSPromise*, JSFunction* resolve, JSFunction* reject);
    static void visitChildren(JSCell*, SlotVisitor&);

private:
    JSPromiseDeferred(VM&);

#ifndef NDEBUG
    bool m_promiseIsAsyncPending { false };
#endif

    WriteBarrier<JSPromise> m_promise;
    WriteBarrier<JSFunction> m_resolve;
    WriteBarrier<JSFunction> m_reject;
};

} // namespace JSC
