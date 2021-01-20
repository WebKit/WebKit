/*
 * Copyright (C) 2015-2019 Apple Inc. All Rights Reserved.
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
class JSModuleNamespaceObject;
class JSModuleRecord;
class SourceCode;

class JSModuleLoader final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSModuleLoader, Base);
        return &vm.plainObjectSpace;
    }

    enum Status {
        Fetch = 1,
        Instantiate,
        Satisfy,
        Link,
        Ready,
    };

    static JSModuleLoader* create(JSGlobalObject* globalObject, VM& vm, Structure* structure)
    {
        JSModuleLoader* object = new (NotNull, allocateCell<JSModuleLoader>(vm.heap)) JSModuleLoader(vm, structure);
        object->finishCreation(globalObject, vm);
        return object;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    // APIs to control the module loader.
    JSValue provideFetch(JSGlobalObject*, JSValue key, const SourceCode&);
    JSInternalPromise* loadAndEvaluateModule(JSGlobalObject*, JSValue moduleName, JSValue parameters, JSValue scriptFetcher);
    JSInternalPromise* loadModule(JSGlobalObject*, JSValue moduleName, JSValue parameters, JSValue scriptFetcher);
    JSValue linkAndEvaluateModule(JSGlobalObject*, JSValue moduleKey, JSValue scriptFetcher);
    JSInternalPromise* requestImportModule(JSGlobalObject*, const Identifier&, JSValue parameters, JSValue scriptFetcher);

    // Platform dependent hooked APIs.
    JSInternalPromise* importModule(JSGlobalObject*, JSString* moduleName, JSValue parameters, const SourceOrigin& referrer);
    JSInternalPromise* resolve(JSGlobalObject*, JSValue name, JSValue referrer, JSValue scriptFetcher);
    Identifier resolveSync(JSGlobalObject*, JSValue name, JSValue referrer, JSValue scriptFetcher);
    JSInternalPromise* fetch(JSGlobalObject*, JSValue key, JSValue parameters, JSValue scriptFetcher);
    JSObject* createImportMetaProperties(JSGlobalObject*, JSValue key, JSModuleRecord*, JSValue scriptFetcher);

    // Additional platform dependent hooked APIs.
    JSValue evaluate(JSGlobalObject*, JSValue key, JSValue moduleRecord, JSValue scriptFetcher);
    JSValue evaluateNonVirtual(JSGlobalObject*, JSValue key, JSValue moduleRecord, JSValue scriptFetcher);

    // Utility functions.
    JSModuleNamespaceObject* getModuleNamespaceObject(JSGlobalObject*, JSValue moduleRecord);
    JSArray* dependencyKeysIfEvaluated(JSGlobalObject*, JSValue key);

private:
    JSModuleLoader(VM&, Structure*);
    void finishCreation(JSGlobalObject*, VM&);
};

} // namespace JSC
