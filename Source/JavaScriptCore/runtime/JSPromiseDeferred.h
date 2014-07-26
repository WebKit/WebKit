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

#ifndef JSPromiseDeferred_h
#define JSPromiseDeferred_h

#if ENABLE(PROMISES)

#include "JSCell.h"
#include "Structure.h"

namespace JSC {

class JSPromiseDeferred : public JSCell {
public:
    typedef JSCell Base;

    JS_EXPORT_PRIVATE static JSPromiseDeferred* create(ExecState*, JSGlobalObject*);
    JS_EXPORT_PRIVATE static JSPromiseDeferred* create(VM&, JSObject* promise, JSValue resolve, JSValue reject);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CompoundType, StructureFlags), info());
    }

    static const bool hasImmortalStructure = true;

    DECLARE_EXPORT_INFO;

    JSObject* promise() const { return m_promise.get(); }
    JSValue resolve() const { return m_resolve.get(); }
    JSValue reject() const { return m_reject.get(); }

private:
    JSPromiseDeferred(VM&);
    void finishCreation(VM&, JSObject*, JSValue, JSValue);
    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;
    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSObject> m_promise;
    WriteBarrier<Unknown> m_resolve;
    WriteBarrier<Unknown> m_reject;
};

enum ThenableStatus {
    WasAThenable,
    NotAThenable
};

JSValue createJSPromiseDeferredFromConstructor(ExecState*, JSValue constructor);
ThenableStatus updateDeferredFromPotentialThenable(ExecState*, JSValue, JSPromiseDeferred*);

void performDeferredResolve(ExecState*, JSPromiseDeferred*, JSValue argument);
void performDeferredReject(ExecState*, JSPromiseDeferred*, JSValue argument);

JSValue abruptRejection(ExecState*, JSPromiseDeferred*);

} // namespace JSC

#endif // ENABLE(PROMISES)

#endif // JSPromiseDeferred_h
