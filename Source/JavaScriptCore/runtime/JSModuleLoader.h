/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "JSObject.h"

namespace JSC {

class JSInternalPromise;

class JSModuleLoader : public JSNonFinalObject {
private:
    JSModuleLoader(VM&, Structure*);
public:
    typedef JSNonFinalObject Base;

    enum Status {
        Fetch = 1,
        Translate = 2,
        Instantiate = 3,
        Satisfy = 4,
        Link = 5,
        Ready = 6,
    };

    static JSModuleLoader* create(ExecState* exec, VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        JSModuleLoader* object = new (NotNull, allocateCell<JSModuleLoader>(vm.heap)) JSModuleLoader(vm, structure);
        object->finishCreation(exec, vm, globalObject);
        return object;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    // APIs to control the module loader.
    JSValue provide(ExecState*, JSValue key, Status, const String&);
    JSInternalPromise* loadAndEvaluateModule(ExecState*, JSValue moduleName, JSValue referrer, JSValue initiator);
    JSInternalPromise* loadModule(ExecState*, JSValue moduleName, JSValue referrer, JSValue initiator);
    JSValue linkAndEvaluateModule(ExecState*, JSValue moduleKey, JSValue initiator);

    // Platform dependent hooked APIs.
    JSInternalPromise* resolve(ExecState*, JSValue name, JSValue referrer, JSValue initiator);
    JSInternalPromise* fetch(ExecState*, JSValue key, JSValue initiator);
    JSInternalPromise* translate(ExecState*, JSValue key, JSValue payload, JSValue initiator);
    JSInternalPromise* instantiate(ExecState*, JSValue key, JSValue source, JSValue initiator);

    // Additional platform dependent hooked APIs.
    JSValue evaluate(ExecState*, JSValue key, JSValue moduleRecord, JSValue initiator);

protected:
    void finishCreation(ExecState*, VM&, JSGlobalObject*);
};

} // namespace JSC
