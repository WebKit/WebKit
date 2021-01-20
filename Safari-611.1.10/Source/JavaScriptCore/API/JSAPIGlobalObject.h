/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include "JSGlobalObject.h"

OBJC_CLASS JSScript;

namespace JSC {

class JSAPIGlobalObject final : public JSGlobalObject {
public:
    using Base = JSGlobalObject;

    DECLARE_EXPORT_INFO;
    static const GlobalObjectMethodTable s_globalObjectMethodTable;

    static constexpr bool needsDestruction = true;
    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.apiGlobalObjectSpace<mode>();
    }

    static JSAPIGlobalObject* create(VM& vm, Structure* structure)
    {
        auto* object = new (NotNull, allocateCell<JSAPIGlobalObject>(vm.heap)) JSAPIGlobalObject(vm, structure);
        object->finishCreation(vm);
        return object;
    }

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        auto* result = Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
        result->setTransitionWatchpointIsLikelyToBeFired(true);
        return result;
    }

    static void reportUncaughtExceptionAtEventLoop(JSGlobalObject*, Exception*);

    static JSInternalPromise* moduleLoaderImportModule(JSGlobalObject*, JSModuleLoader*, JSString* moduleNameValue, JSValue parameters, const SourceOrigin&);
    static Identifier moduleLoaderResolve(JSGlobalObject*, JSModuleLoader*, JSValue keyValue, JSValue referrerValue, JSValue);
    static JSInternalPromise* moduleLoaderFetch(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, JSValue);
    static JSObject* moduleLoaderCreateImportMetaProperties(JSGlobalObject*, JSModuleLoader*, JSValue, JSModuleRecord*, JSValue);
    static JSValue moduleLoaderEvaluate(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, JSValue);

    JSValue loadAndEvaluateJSScriptModule(const JSLockHolder&, JSScript *);

private:
    JSAPIGlobalObject(VM& vm, Structure* structure)
        : Base(vm, structure, &s_globalObjectMethodTable)
    { }
};

}
