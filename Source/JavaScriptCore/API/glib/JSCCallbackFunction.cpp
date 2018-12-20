/*
 * Copyright (C) 2018 Igalia S.L.
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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
#include "JSCCallbackFunction.h"

#include "APICallbackFunction.h"
#include "APICast.h"
#include "IsoSubspacePerVM.h"
#include "JSCClassPrivate.h"
#include "JSCContextPrivate.h"
#include "JSDestructibleObjectHeapCellType.h"
#include "JSCExceptionPrivate.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSLock.h"

namespace JSC {

static JSValueRef callAsFunction(JSContextRef callerContext, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return static_cast<JSCCallbackFunction*>(toJS(function))->call(callerContext, thisObject, argumentCount, arguments, exception);
}

static JSObjectRef callAsConstructor(JSContextRef callerContext, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return static_cast<JSCCallbackFunction*>(toJS(constructor))->construct(callerContext, argumentCount, arguments, exception);
}

const ClassInfo JSCCallbackFunction::s_info = { "CallbackFunction", &InternalFunction::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCCallbackFunction) };

JSCCallbackFunction* JSCCallbackFunction::create(VM& vm, JSGlobalObject* globalObject, const String& name, Type type, JSCClass* jscClass, GRefPtr<GClosure>&& closure, GType returnType, Optional<Vector<GType>>&& parameters)
{
    Structure* structure = globalObject->glibCallbackFunctionStructure();
    JSCCallbackFunction* function = new (NotNull, allocateCell<JSCCallbackFunction>(vm.heap)) JSCCallbackFunction(vm, structure, type, jscClass, WTFMove(closure), returnType, WTFMove(parameters));
    function->finishCreation(vm, name);
    return function;
}

JSCCallbackFunction::JSCCallbackFunction(VM& vm, Structure* structure, Type type, JSCClass* jscClass, GRefPtr<GClosure>&& closure, GType returnType, Optional<Vector<GType>>&& parameters)
    : InternalFunction(vm, structure, APICallbackFunction::call<JSCCallbackFunction>, type == Type::Constructor ? APICallbackFunction::construct<JSCCallbackFunction> : nullptr)
    , m_functionCallback(callAsFunction)
    , m_constructCallback(callAsConstructor)
    , m_type(type)
    , m_class(jscClass)
    , m_closure(WTFMove(closure))
    , m_returnType(returnType)
    , m_parameters(WTFMove(parameters))
{
    ASSERT(type != Type::Constructor || jscClass);
    if (G_CLOSURE_NEEDS_MARSHAL(m_closure.get()))
        g_closure_set_marshal(m_closure.get(), g_cclosure_marshal_generic);
}

JSValueRef JSCCallbackFunction::call(JSContextRef callerContext, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSLockHolder locker(toJS(callerContext));
    auto context = jscContextGetOrCreate(toGlobalRef(globalObject()->globalExec()));
    auto* jsContext = jscContextGetJSContext(context.get());

    if (m_type == Type::Constructor) {
        *exception = toRef(JSC::createTypeError(toJS(jsContext), "cannot call a class constructor without |new|"_s));
        return JSValueMakeUndefined(jsContext);
    }

    gpointer instance = nullptr;
    if (m_type == Type::Method) {
        instance = jscContextWrappedObject(context.get(), thisObject);
        if (!instance) {
            *exception = toRef(JSC::createTypeError(toJS(jsContext), "invalid instance type in method"_s));
            return JSValueMakeUndefined(jsContext);
        }
    }

    auto callbackData = jscContextPushCallback(context.get(), toRef(this), thisObject, argumentCount, arguments);

    // GClosure always expect to have at least the instance parameter.
    bool addInstance = instance || (m_parameters && m_parameters->isEmpty());

    auto parameterCount = m_parameters ? std::min(m_parameters->size(), argumentCount) : 1;
    if (addInstance)
        parameterCount++;
    auto* values = static_cast<GValue*>(g_alloca(sizeof(GValue) * parameterCount));
    memset(values, 0, sizeof(GValue) * parameterCount);

    size_t firstParameter = 0;
    if (addInstance) {
        g_value_init(&values[0], G_TYPE_POINTER);
        g_value_set_pointer(&values[0], instance);
        firstParameter = 1;
    }
    if (m_parameters) {
        for (size_t i = firstParameter; i < parameterCount && !*exception; ++i)
            jscContextJSValueToGValue(context.get(), arguments[i - firstParameter], m_parameters.value()[i - firstParameter], &values[i], exception);
    } else {
        auto* parameters = g_ptr_array_new_full(argumentCount, g_object_unref);
        for (size_t i = 0; i < argumentCount; ++i)
            g_ptr_array_add(parameters, jscContextGetOrCreateValue(context.get(), arguments[i]).leakRef());
        g_value_init(&values[firstParameter], G_TYPE_PTR_ARRAY);
        g_value_take_boxed(&values[firstParameter], parameters);
    }

    GValue returnValue = G_VALUE_INIT;
    if (m_returnType != G_TYPE_NONE)
        g_value_init(&returnValue, m_returnType);

    if (!*exception)
        g_closure_invoke(m_closure.get(), m_returnType != G_TYPE_NONE ? &returnValue : nullptr, parameterCount, values, nullptr);

    for (size_t i = 0; i < parameterCount; ++i)
        g_value_unset(&values[i]);

    if (auto* jscException = jsc_context_get_exception(context.get()))
        *exception = jscExceptionGetJSValue(jscException);

    jscContextPopCallback(context.get(), WTFMove(callbackData));

    if (m_returnType == G_TYPE_NONE)
        return JSValueMakeUndefined(jsContext);

    auto* retval = *exception ? JSValueMakeUndefined(jsContext) : jscContextGValueToJSValue(context.get(), &returnValue, exception);
    g_value_unset(&returnValue);
    return retval;
}

JSObjectRef JSCCallbackFunction::construct(JSContextRef callerContext, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSLockHolder locker(toJS(callerContext));
    auto context = jscContextGetOrCreate(toGlobalRef(globalObject()->globalExec()));
    auto* jsContext = jscContextGetJSContext(context.get());

    if (m_returnType == G_TYPE_NONE) {
        *exception = toRef(JSC::createTypeError(toJS(jsContext), "constructors cannot be void"_s));
        return nullptr;
    }

    auto callbackData = jscContextPushCallback(context.get(), toRef(this), nullptr, argumentCount, arguments);

    GValue returnValue = G_VALUE_INIT;
    g_value_init(&returnValue, m_returnType);

    if (m_parameters && m_parameters->isEmpty()) {
        // GClosure always expect to have at least the instance parameter.
        GValue dummyValue = G_VALUE_INIT;
        g_value_init(&dummyValue, G_TYPE_POINTER);
        g_closure_invoke(m_closure.get(), &returnValue, 1, &dummyValue, nullptr);
        g_value_unset(&dummyValue);
    } else {
        auto parameterCount = m_parameters ? std::min(m_parameters->size(), argumentCount) : 1;
        auto* values = static_cast<GValue*>(g_alloca(sizeof(GValue) * parameterCount));
        memset(values, 0, sizeof(GValue) * parameterCount);

        if (m_parameters) {
            for (size_t i = 0; i < parameterCount && !*exception; ++i)
                jscContextJSValueToGValue(context.get(), arguments[i], m_parameters.value()[i], &values[i], exception);
        } else {
            auto* parameters = g_ptr_array_new_full(argumentCount, g_object_unref);
            for (size_t i = 0; i < argumentCount; ++i)
                g_ptr_array_add(parameters, jscContextGetOrCreateValue(context.get(), arguments[i]).leakRef());
            g_value_init(&values[0], G_TYPE_PTR_ARRAY);
            g_value_take_boxed(&values[0], parameters);
        }

        if (!*exception)
            g_closure_invoke(m_closure.get(), &returnValue, parameterCount, values, nullptr);

        for (size_t i = 0; i < parameterCount; ++i)
            g_value_unset(&values[i]);
    }

    if (auto* jscException = jsc_context_get_exception(context.get()))
        *exception = jscExceptionGetJSValue(jscException);

    jscContextPopCallback(context.get(), WTFMove(callbackData));

    if (*exception) {
        g_value_unset(&returnValue);
        return nullptr;
    }

    switch (g_type_fundamental(G_VALUE_TYPE(&returnValue))) {
    case G_TYPE_POINTER:
    case G_TYPE_OBJECT:
        if (auto* ptr = returnValue.data[0].v_pointer) {
            auto* retval = jscClassGetOrCreateJSWrapper(m_class.get(), ptr);
            g_value_unset(&returnValue);
            return toRef(retval);
        }
        *exception = toRef(JSC::createTypeError(toJS(jsContext), "constructor returned null"_s));
        break;
    default:
        *exception = toRef(JSC::createTypeError(toJS(jsContext), makeString("invalid type ", g_type_name(G_VALUE_TYPE(&returnValue)), " returned by constructor")));
        break;
    }
    return nullptr;
}

void JSCCallbackFunction::destroy(JSCell* cell)
{
    static_cast<JSCCallbackFunction*>(cell)->JSCCallbackFunction::~JSCCallbackFunction();
}

IsoSubspace* JSCCallbackFunction::subspaceForImpl(VM& vm)
{
    NeverDestroyed<IsoSubspacePerVM> perVM([] (VM& vm) -> IsoSubspacePerVM::SubspaceParameters { return ISO_SUBSPACE_PARAMETERS(vm.destructibleObjectHeapCellType.get(), JSCCallbackFunction); });
    return &perVM.get().forVM(vm);
}

} // namespace JSC
