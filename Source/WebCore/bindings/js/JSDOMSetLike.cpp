/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "JSDOMSetLike.h"

#include "WebCoreJSClientData.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/JSSet.h>
#include <JavaScriptCore/JSSetInlines.h>
#include <JavaScriptCore/VMTrapsInlines.h>

namespace WebCore {

void DOMSetAdapter::clear()
{
    clearBackingSet(m_lexicalGlobalObject, m_backingSet);
}

std::pair<bool, std::reference_wrapper<JSC::JSObject>> getBackingSet(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& setLike)
{
    auto& vm = lexicalGlobalObject.vm();
    auto backingSet = setLike.get(&lexicalGlobalObject, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingSetPrivateName());
    if (backingSet.isUndefined()) {
        auto& vm = lexicalGlobalObject.vm();
        JSC::DeferTermination deferScope(vm);
        auto scope = DECLARE_CATCH_SCOPE(vm);

        backingSet = JSC::JSSet::create(&lexicalGlobalObject, vm, lexicalGlobalObject.setStructure());
        scope.releaseAssertNoException();

        setLike.putDirect(vm, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().backingSetPrivateName(), backingSet, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum));
        return { true, *JSC::asObject(backingSet) };
    }
    return { false, *JSC::asObject(backingSet) };
}

void clearBackingSet(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingSet)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    JSC::JSValue function = backingSet.get(&lexicalGlobalObject, vm.propertyNames->clear);
    ASSERT(!function.isUndefined());

    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != JSC::CallData::Type::None);
    JSC::MarkedArgumentBuffer arguments;
    JSC::call(&lexicalGlobalObject, function, callData, &backingSet, arguments);
}

void addToBackingSet(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingSet, JSC::JSValue item)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    JSC::JSValue function = backingSet.get(&lexicalGlobalObject, vm.propertyNames->add);
    ASSERT(!function.isUndefined());

    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != JSC::CallData::Type::None);
    JSC::MarkedArgumentBuffer arguments;
    arguments.append(item);
    JSC::call(&lexicalGlobalObject, function, callData, &backingSet, arguments);
}

JSC::JSValue forwardAttributeGetterToBackingSet(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingSet, const JSC::Identifier& attributeName)
{
    return backingSet.get(&lexicalGlobalObject, attributeName);
}

JSC::JSValue forwardFunctionCallToBackingSet(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, JSC::JSObject& backingSet, const JSC::Identifier& functionName)
{
    JSC::JSValue function = backingSet.get(&lexicalGlobalObject, functionName);
    ASSERT(!function.isUndefined());

    auto callData = JSC::getCallData(lexicalGlobalObject.vm(), function);
    ASSERT(callData.type != JSC::CallData::Type::None);
    JSC::MarkedArgumentBuffer arguments;
    for (size_t cptr = 0; cptr < callFrame.argumentCount(); ++cptr)
        arguments.append(callFrame.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
    return JSC::call(&lexicalGlobalObject, function, callData, &backingSet, arguments);
}

JSC::JSValue forwardForEachCallToBackingSet(JSDOMGlobalObject& globalObject, JSC::CallFrame& callFrame, JSC::JSObject& setLike)
{
    auto result = getBackingSet(globalObject, setLike);
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
    return JSC::call(&globalObject, function, callData, &setLike, arguments);
}

}
