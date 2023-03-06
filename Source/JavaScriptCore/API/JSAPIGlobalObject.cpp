/**
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSAPIGlobalObject.h"

#include "GlobalObjectMethodTable.h"
#include "JSCellInlines.h"

namespace JSC {

const ClassInfo JSAPIGlobalObject::s_info = { "GlobalObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAPIGlobalObject) };

#if !JSC_OBJC_API_ENABLED

const GlobalObjectMethodTable* JSAPIGlobalObject::globalObjectMethodTable()
{
    static constexpr GlobalObjectMethodTable table {
        &supportsRichSourceInfo,
        &shouldInterruptScript,
        &javaScriptRuntimeFlags,
        nullptr, // queueMicrotaskToEventLoop
        &shouldInterruptScriptBeforeTimeout,
        nullptr, // moduleLoaderImportModule
        nullptr, // moduleLoaderResolve
        nullptr, // moduleLoaderFetch
        nullptr, // moduleLoaderCreateImportMetaProperties
        nullptr, // moduleLoaderEvaluate
        nullptr, // promiseRejectionTracker
        &reportUncaughtExceptionAtEventLoop,
        &currentScriptExecutionOwner,
        &scriptExecutionStatus,
        &reportViolationForUnsafeEval,
        nullptr, // defaultLanguage
        nullptr, // compileStreaming
        nullptr, // instantiateStreaming
        nullptr, // deriveShadowRealmGlobalObject
    };
    return &table;
}

void JSAPIGlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject* globalObject, Exception* exception)
{
    Base::reportUncaughtExceptionAtEventLoop(globalObject, exception);
}

#endif

JSAPIGlobalObject::JSAPIGlobalObject(VM& vm, Structure* structure)
    : Base(vm, structure, globalObjectMethodTable())
{
}

JSAPIGlobalObject* JSAPIGlobalObject::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<JSAPIGlobalObject>(vm)) JSAPIGlobalObject(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* JSAPIGlobalObject::createStructure(VM& vm, JSValue prototype)
{
    auto* result = Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    result->setTransitionWatchpointIsLikelyToBeFired(true);
    return result;
}

}
