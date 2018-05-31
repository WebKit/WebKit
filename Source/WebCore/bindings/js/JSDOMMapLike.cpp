/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDOMMapLike.h"

#include "WebCoreJSClientData.h"
#include <JavaScriptCore/CatchScope.h>

namespace WebCore {

static inline JSC::JSObject& getBackingMap(JSC::ExecState& state, JSC::JSObject& mapLike)
{
    auto& vm = state.vm();
    auto backingMap = mapLike.get(&state, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingMapPrivateName());
    return *JSC::asObject(backingMap);
}

void initializeBackingMap(JSC::VM& vm, JSC::JSObject& mapLike, JSC::JSMap& backingMap)
{
    mapLike.putDirect(vm, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingMapPrivateName(), &backingMap, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum));
}

JSC::JSMap& createBackingMap(JSC::ExecState& state, JSC::JSGlobalObject& globalObject, JSC::JSObject& mapLike)
{
    auto& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    ASSERT(mapLike.get(&state, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingMapPrivateName()).isUndefined());
    auto backingMap = JSC::JSMap::create(&state, vm, globalObject.mapStructure());
    scope.releaseAssertNoException();
    mapLike.putDirect(vm, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingMapPrivateName(), backingMap, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum));
    return *backingMap;
}

JSC::JSValue forwardAttributeGetterToBackingMap(JSC::ExecState& state, JSC::JSObject& mapLike, const JSC::Identifier& attributeName)
{
    return getBackingMap(state, mapLike).get(&state, attributeName);
}

JSC::JSValue forwardFunctionCallToBackingMap(JSC::ExecState& state, JSC::JSObject& mapLike, const JSC::Identifier& functionName)
{
    auto& backingMap = getBackingMap(state, mapLike);

    JSC::JSValue function = backingMap.get(&state, functionName);
    ASSERT(function);

    JSC::CallData callData;
    JSC::CallType callType = JSC::getCallData(state.vm(), function, callData);
    ASSERT(callType != JSC::CallType::None);
    JSC::MarkedArgumentBuffer arguments;
    for (size_t cptr = 0; cptr < state.argumentCount(); ++cptr)
        arguments.append(state.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
    return JSC::call(&state, function, callType, callData, &backingMap, arguments);
}

JSC::JSValue forwardForEachCallToBackingMap(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::JSObject& mapLike)
{
    auto* function = globalObject.builtinInternalFunctions().jsDOMBindingInternals().m_mapLikeForEachFunction.get();
    ASSERT(function);

    getBackingMap(state, mapLike);

    JSC::CallData callData;
    JSC::CallType callType = JSC::getCallData(state.vm(), function, callData);
    ASSERT(callType != JSC::CallType::None);
    JSC::MarkedArgumentBuffer arguments;
    for (size_t cptr = 0; cptr < state.argumentCount(); ++cptr)
        arguments.append(state.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
    return JSC::call(&state, function, callType, callData, &mapLike, arguments);
}

}
