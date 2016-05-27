/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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

#ifndef ModuleLoaderObject_h
#define ModuleLoaderObject_h

#include "JSObject.h"

namespace JSC {

class JSInternalPromise;

class ModuleLoaderObject : public JSNonFinalObject {
private:
    ModuleLoaderObject(VM&, Structure*);
public:
    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    enum Status {
        Fetch = 1,
        Translate = 2,
        Instantiate = 3,
        ResolveDependencies = 4,
        Link = 5,
        Ready = 6,
    };

    static ModuleLoaderObject* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        ModuleLoaderObject* object = new (NotNull, allocateCell<ModuleLoaderObject>(vm.heap)) ModuleLoaderObject(vm, structure);
        object->finishCreation(vm, globalObject);
        return object;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    // APIs to control the module loader.
    JSValue provide(ExecState*, JSValue key, Status, const String&);
    JSInternalPromise* loadAndEvaluateModule(ExecState*, JSValue moduleName, JSValue referrer);
    JSInternalPromise* loadModule(ExecState*, JSValue moduleName, JSValue referrer);
    JSInternalPromise* linkAndEvaluateModule(ExecState*, JSValue moduleKey);

    // Platform dependent hooked APIs.
    JSInternalPromise* resolve(ExecState*, JSValue name, JSValue referrer);
    JSInternalPromise* fetch(ExecState*, JSValue key);
    JSInternalPromise* translate(ExecState*, JSValue key, JSValue payload);
    JSInternalPromise* instantiate(ExecState*, JSValue key, JSValue source);

    // Additional platform dependent hooked APIs.
    JSValue evaluate(ExecState*, JSValue key, JSValue moduleRecord);

protected:
    void finishCreation(VM&, JSGlobalObject*);
};

} // namespace JSC

#endif // ModuleLoaderObject_h
