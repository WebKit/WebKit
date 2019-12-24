/*
 * Copyright (C) 2008-2019 Apple Inc. All Rights Reserved.
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
#include "ConstructData.h"

#include "Interpreter.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "ScriptProfilingScope.h"

namespace JSC {

JSObject* construct(JSGlobalObject* globalObject, JSValue constructorObject, const ArgList& args, const char* errorMessage)
{
    return construct(globalObject, constructorObject, constructorObject, args, errorMessage);
}

JSObject* construct(JSGlobalObject* globalObject, JSValue constructorObject, JSValue newTarget, const ArgList& args, const char* errorMessage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ConstructData constructData;
    ConstructType constructType = getConstructData(vm, constructorObject, constructData);
    if (UNLIKELY(constructType == ConstructType::None)) {
        throwTypeError(globalObject, scope, errorMessage);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, construct(globalObject, constructorObject, constructType, constructData, args, newTarget));  
}

JSObject* construct(JSGlobalObject* globalObject, JSValue constructorObject, ConstructType constructType, const ConstructData& constructData, const ArgList& args, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    ASSERT(constructType == ConstructType::JS || constructType == ConstructType::Host);
    return vm.interpreter->executeConstruct(globalObject, asObject(constructorObject), constructType, constructData, args, newTarget);
}

JSObject* profiledConstruct(JSGlobalObject* globalObject, ProfilingReason reason, JSValue constructorObject, ConstructType constructType, const ConstructData& constructData, const ArgList& args, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    ScriptProfilingScope profilingScope(vm.deprecatedVMEntryGlobalObject(globalObject), reason);
    return construct(globalObject, constructorObject, constructType, constructData, args, newTarget);
}

} // namespace JSC
