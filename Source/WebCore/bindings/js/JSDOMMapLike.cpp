/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/JSMap.h>
#include <JavaScriptCore/VMTrapsInlines.h>

namespace WebCore {

std::pair<bool, std::reference_wrapper<JSC::JSObject>> getBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& mapLike)
{
    auto& vm = lexicalGlobalObject.vm();
    auto backingMap = mapLike.get(&lexicalGlobalObject, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingMapPrivateName());
    if (!backingMap.isUndefined())
        return { false, *JSC::asObject(backingMap) };

    JSC::DeferTerminationForAWhile deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    backingMap = JSC::JSMap::create(&lexicalGlobalObject, vm, lexicalGlobalObject.mapStructure());
    scope.releaseAssertNoException();

    mapLike.putDirect(vm, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingMapPrivateName(), backingMap, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum));
    return { true, *JSC::asObject(backingMap) };
}

void clearBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingMap)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto function = backingMap.get(&lexicalGlobalObject, vm.propertyNames->clear);

    auto callData = JSC::getCallData(vm, function);
    if (callData.type == JSC::CallData::Type::None)
        return;

    JSC::MarkedArgumentBuffer arguments;
    JSC::call(&lexicalGlobalObject, function, callData, &backingMap, arguments);
}

void setToBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingMap, JSC::JSValue key, JSC::JSValue value)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto function = backingMap.get(&lexicalGlobalObject, vm.propertyNames->set);

    auto callData = JSC::getCallData(vm, function);
    if (callData.type == JSC::CallData::Type::None)
        return;

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(key);
    arguments.append(value);
    JSC::call(&lexicalGlobalObject, function, callData, &backingMap, arguments);
}

JSC::JSValue forwardAttributeGetterToBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingMap, const JSC::Identifier& attributeName)
{
    return backingMap.get(&lexicalGlobalObject, attributeName);
}

JSC::JSValue forwardFunctionCallToBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, JSC::JSObject& backingMap, const JSC::Identifier& functionName)
{
    auto function = backingMap.get(&lexicalGlobalObject, functionName);

    auto callData = JSC::getCallData(lexicalGlobalObject.vm(), function);
    if (callData.type == JSC::CallData::Type::None)
        return JSC::jsUndefined();

    JSC::MarkedArgumentBuffer arguments;
    for (size_t cptr = 0; cptr < callFrame.argumentCount(); ++cptr)
        arguments.append(callFrame.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
    return JSC::call(&lexicalGlobalObject, function, callData, &backingMap, arguments);
}

JSC::JSValue forwardForEachCallToBackingMap(JSDOMGlobalObject& globalObject, JSC::CallFrame& callFrame, JSC::JSObject& mapLike)
{
    auto result = getBackingMap(globalObject, mapLike);
    ASSERT(!result.first);

    auto* function = globalObject.builtinInternalFunctions().jsDOMBindingInternals().m_forEachWrapperFunction.get();
    ASSERT(function);

    auto callData = JSC::getCallData(globalObject.vm(), function);
    ASSERT(callData.type != JSC::CallData::Type::None);

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(&result.second.get());
    for (size_t cptr = 0; cptr < callFrame.argumentCount(); ++cptr)
        arguments.append(callFrame.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
    return JSC::call(&globalObject, function, callData, &mapLike, arguments);
}

void DOMMapAdapter::clear()
{
    clearBackingMap(m_lexicalGlobalObject, m_backingMap);
}

}
