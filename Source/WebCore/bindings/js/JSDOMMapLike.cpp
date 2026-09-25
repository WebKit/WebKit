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

#include "WebCoreJSBuiltinInternals.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/JSMap.h>
#include <JavaScriptCore/VMTrapsInlines.h>

namespace WebCore {

std::pair<bool, std::reference_wrapper<JSC::JSObject>> getBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& mapLike)
{
    auto& vm = lexicalGlobalObject.vm();
#ifndef RC_REMOVE_SEC_ACCEL_3
    auto scope = DECLARE_THROW_SCOPE(vm);
#endif
    auto backingMap = mapLike.getDirect(vm, builtinNames(vm).backingMapPrivateName());
#ifndef RC_REMOVE_SEC_ACCEL_3
    scope.assertNoExceptionExceptTermination();
#endif
    if (backingMap)
        return { false, *JSC::asObject(backingMap) };

    backingMap = JSC::JSMap::create(vm, lexicalGlobalObject.mapStructure());
    mapLike.putDirect(vm, builtinNames(vm).backingMapPrivateName(), backingMap, enumToUnderlyingType(JSC::PropertyAttribute::DontEnum));
#ifndef RC_REMOVE_SEC_ACCEL_3
    scope.assertNoExceptionExceptTermination();
#endif
    return { true, *JSC::asObject(backingMap) };
}

void clearBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingMap)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
#ifndef RC_REMOVE_SEC_ACCEL_3
    auto scope = DECLARE_THROW_SCOPE(vm);
#endif
    auto function = lexicalGlobalObject.mapPrototype()->getDirect(vm, vm.propertyNames->builtinNames().clearPrivateName());
    ASSERT(function);
#ifndef RC_REMOVE_SEC_ACCEL_3
    RETURN_IF_EXCEPTION(scope, void());
#endif

    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != JSC::CallData::Type::None);

    JSC::MarkedArgumentBuffer arguments;
    JSC::call(&lexicalGlobalObject, function, callData, &backingMap, arguments);
}

void setToBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingMap, JSC::JSValue key, JSC::JSValue value)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
#ifndef RC_REMOVE_SEC_ACCEL_3
    auto scope = DECLARE_THROW_SCOPE(vm);
#endif
    auto function = lexicalGlobalObject.mapPrototype()->getDirect(vm, vm.propertyNames->builtinNames().setPrivateName());
    ASSERT(function);
#ifndef RC_REMOVE_SEC_ACCEL_3
    RETURN_IF_EXCEPTION(scope, void());
#endif

    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != JSC::CallData::Type::None);

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(key);
    arguments.append(value);
    ASSERT(!arguments.hasOverflowed());
    JSC::call(&lexicalGlobalObject, function, callData, &backingMap, arguments);
}

JSC::JSValue forwardAttributeGetterToBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& backingMap, const JSC::Identifier& attributeName)
{
    return backingMap.get(&lexicalGlobalObject, attributeName);
}

JSC::JSValue forwardFunctionCallToBackingMap(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, JSC::JSObject& backingMap, const JSC::Identifier& functionName)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
#ifndef RC_REMOVE_SEC_ACCEL_3
    auto scope = DECLARE_THROW_SCOPE(vm);
#endif
    auto function = lexicalGlobalObject.mapPrototype()->getDirect(vm, functionName);
    ASSERT(function);
#ifndef RC_REMOVE_SEC_ACCEL_3
    RETURN_IF_EXCEPTION(scope, { });
#endif

    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != JSC::CallData::Type::None);

    JSC::MarkedArgumentBuffer arguments;
    arguments.ensureCapacity(callFrame.argumentCount());
    for (size_t cptr = 0; cptr < callFrame.argumentCount(); ++cptr)
        arguments.append(callFrame.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
#ifndef RC_REMOVE_SEC_ACCEL_3
    RELEASE_AND_RETURN(scope, JSC::call(&lexicalGlobalObject, function, callData, &backingMap, arguments));
#else
    return JSC::call(&lexicalGlobalObject, function, callData, &backingMap, arguments);
#endif
}

JSC::JSValue forwardForEachCallToBackingMap(JSDOMGlobalObject& globalObject, JSC::CallFrame& callFrame, JSC::JSObject& mapLike)
{
#ifndef RC_REMOVE_SEC_ACCEL_3
    auto& vm = JSC::getVM(&globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
#endif
    auto result = getBackingMap(globalObject, mapLike);
    ASSERT(!result.first);
#ifndef RC_REMOVE_SEC_ACCEL_3
    RETURN_IF_EXCEPTION(scope, { });
#endif

    auto* function = globalObject.builtinInternalFunctions().jsDOMBindingInternals().m_forEachWrapperFunction.get();
    ASSERT(function);

    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != JSC::CallData::Type::None);

    JSC::MarkedArgumentBuffer arguments;
    arguments.ensureCapacity(callFrame.argumentCount() + 1);
    arguments.append(&result.second.get());
    for (size_t cptr = 0; cptr < callFrame.argumentCount(); ++cptr)
        arguments.append(callFrame.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
#ifndef RC_REMOVE_SEC_ACCEL_3
    RELEASE_AND_RETURN(scope, JSC::call(&globalObject, function, callData, &mapLike, arguments));
#else
    return JSC::call(&globalObject, function, callData, &mapLike, arguments);
#endif
}

void DOMMapAdapter::clear()
{
    clearBackingMap(m_lexicalGlobalObject, m_backingMap);
}

}
