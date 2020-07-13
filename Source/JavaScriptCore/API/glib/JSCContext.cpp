/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSCContext.h"

#include "JSCClassPrivate.h"
#include "JSCContextPrivate.h"
#include "JSCExceptionPrivate.h"
#include "JSCInlines.h"
#include "JSCValuePrivate.h"
#include "JSCVirtualMachinePrivate.h"
#include "JSCWrapperMap.h"
#include "JSRetainPtr.h"
#include "JSWithScope.h"
#include "OpaqueJSString.h"
#include "Parser.h"
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * SECTION: JSCContext
 * @short_description: JavaScript execution context
 * @title: JSCContext
 *
 * JSCContext represents a JavaScript execution context, where all operations
 * take place and where the values will be associated.
 *
 * When a new context is created, a global object is allocated and the built-in JavaScript
 * objects (Object, Function, String, Array) are populated. You can execute JavaScript in
 * the context by using jsc_context_evaluate() or jsc_context_evaluate_with_source_uri().
 * It's also possible to register custom objects in the context with jsc_context_register_class().
 */

enum {
    PROP_0,

    PROP_VIRTUAL_MACHINE,
};

struct JSCContextExceptionHandler {
    JSCContextExceptionHandler(JSCExceptionHandler handler, void* userData = nullptr, GDestroyNotify destroyNotifyFunction = nullptr)
        : handler(handler)
        , userData(userData)
        , destroyNotifyFunction(destroyNotifyFunction)
    {
    }

    ~JSCContextExceptionHandler()
    {
        if (destroyNotifyFunction)
            destroyNotifyFunction(userData);
    }

    JSCContextExceptionHandler(JSCContextExceptionHandler&& other)
    {
        std::swap(handler, other.handler);
        std::swap(userData, other.userData);
        std::swap(destroyNotifyFunction, other.destroyNotifyFunction);
    }

    JSCContextExceptionHandler(const JSCContextExceptionHandler&) = delete;
    JSCContextExceptionHandler& operator=(const JSCContextExceptionHandler&) = delete;

    JSCExceptionHandler handler { nullptr };
    void* userData { nullptr };
    GDestroyNotify destroyNotifyFunction { nullptr };
};

struct _JSCContextPrivate {
    GRefPtr<JSCVirtualMachine> vm;
    JSRetainPtr<JSGlobalContextRef> jsContext;
    GRefPtr<JSCException> exception;
    Vector<JSCContextExceptionHandler> exceptionHandlers;
};

WEBKIT_DEFINE_TYPE(JSCContext, jsc_context, G_TYPE_OBJECT)

static void jscContextSetVirtualMachine(JSCContext* context, GRefPtr<JSCVirtualMachine>&& vm)
{
    JSCContextPrivate* priv = context->priv;
    if (vm) {
        ASSERT(!priv->vm);
        priv->vm = WTFMove(vm);
        ASSERT(!priv->jsContext);
        GUniquePtr<char> name(g_strdup_printf("%p-jsContext", &Thread::current()));
        if (auto* data = g_object_get_data(G_OBJECT(priv->vm.get()), name.get())) {
            priv->jsContext = static_cast<JSGlobalContextRef>(data);
            g_object_set_data(G_OBJECT(priv->vm.get()), name.get(), nullptr);
        } else
            priv->jsContext = JSRetainPtr<JSGlobalContextRef>(Adopt, JSGlobalContextCreateInGroup(jscVirtualMachineGetContextGroup(priv->vm.get()), nullptr));
        auto* globalObject = toJSGlobalObject(priv->jsContext.get());
        if (!globalObject->wrapperMap())
            globalObject->setWrapperMap(makeUnique<JSC::WrapperMap>(priv->jsContext.get()));
        jscVirtualMachineAddContext(priv->vm.get(), context);
    } else if (priv->vm) {
        ASSERT(priv->jsContext);
        jscVirtualMachineRemoveContext(priv->vm.get(), context);
        priv->jsContext = nullptr;
        priv->vm = nullptr;
    }
}

static void jscContextGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    JSCContextPrivate* priv = JSC_CONTEXT(object)->priv;

    switch (propID) {
    case PROP_VIRTUAL_MACHINE:
        g_value_set_object(value, priv->vm.get());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void jscContextSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    JSCContext* context = JSC_CONTEXT(object);

    switch (propID) {
    case PROP_VIRTUAL_MACHINE:
        if (gpointer vm = g_value_get_object(value))
            jscContextSetVirtualMachine(context, GRefPtr<JSCVirtualMachine>(JSC_VIRTUAL_MACHINE(vm)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void jscContextConstructed(GObject* object)
{
    G_OBJECT_CLASS(jsc_context_parent_class)->constructed(object);

    JSCContext* context = JSC_CONTEXT(object);
    if (!context->priv->vm)
        jscContextSetVirtualMachine(context, adoptGRef(jsc_virtual_machine_new()));

    context->priv->exceptionHandlers.append(JSCContextExceptionHandler([](JSCContext* context, JSCException* exception, gpointer) {
        jsc_context_throw_exception(context, exception);
    }));
}

static void jscContextDispose(GObject* object)
{
    JSCContext* context = JSC_CONTEXT(object);
    jscContextSetVirtualMachine(context, nullptr);

    G_OBJECT_CLASS(jsc_context_parent_class)->dispose(object);
}

static void jsc_context_class_init(JSCContextClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS(klass);
    objClass->get_property = jscContextGetProperty;
    objClass->set_property = jscContextSetProperty;
    objClass->constructed = jscContextConstructed;
    objClass->dispose = jscContextDispose;

    /**
     * JSCContext:virtual-machine:
     *
     * The #JSCVirtualMachine in which the context was created.
     */
    g_object_class_install_property(objClass,
        PROP_VIRTUAL_MACHINE,
        g_param_spec_object(
            "virtual-machine",
            "JSCVirtualMachine",
            "JSC Virtual Machine",
            JSC_TYPE_VIRTUAL_MACHINE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

GRefPtr<JSCContext> jscContextGetOrCreate(JSGlobalContextRef jsContext)
{
    auto vm = jscVirtualMachineGetOrCreate(toRef(&toJS(jsContext)->vm()));
    if (GRefPtr<JSCContext> context = jscVirtualMachineGetContext(vm.get(), jsContext))
        return context;

    GUniquePtr<char> name(g_strdup_printf("%p-jsContext", &Thread::current()));
    g_object_set_data(G_OBJECT(vm.get()), name.get(), jsContext);
    return adoptGRef(jsc_context_new_with_virtual_machine(vm.get()));
}

JSGlobalContextRef jscContextGetJSContext(JSCContext* context)
{
    ASSERT(JSC_IS_CONTEXT(context));

    JSCContextPrivate* priv = context->priv;
    return priv->jsContext.get();
}

static JSC::WrapperMap& wrapperMap(JSCContext* context)
{
    auto* map = toJSGlobalObject(context->priv->jsContext.get())->wrapperMap();
    ASSERT(map);
    return *map;
}

GRefPtr<JSCValue> jscContextGetOrCreateValue(JSCContext* context, JSValueRef jsValue)
{
    return wrapperMap(context).gobjectWrapper(context, jsValue);
}

void jscContextValueDestroyed(JSCContext* context, JSValueRef jsValue)
{
    wrapperMap(context).unwrap(jsValue);
}

JSC::JSObject* jscContextGetJSWrapper(JSCContext* context, gpointer wrappedObject)
{
    return wrapperMap(context).jsWrapper(wrappedObject);
}

JSC::JSObject* jscContextGetOrCreateJSWrapper(JSCContext* context, JSClassRef jsClass, JSValueRef prototype, gpointer wrappedObject, GDestroyNotify destroyFunction)
{
    if (auto* jsWrapper = jscContextGetJSWrapper(context, wrappedObject))
        return jsWrapper;

    return wrapperMap(context).createJSWrappper(context->priv->jsContext.get(), jsClass, prototype, wrappedObject, destroyFunction);
}

JSGlobalContextRef jscContextCreateContextWithJSWrapper(JSCContext* context, JSClassRef jsClass, JSValueRef prototype, gpointer wrappedObject, GDestroyNotify destroyFunction)
{
    return wrapperMap(context).createContextWithJSWrappper(jscVirtualMachineGetContextGroup(context->priv->vm.get()), jsClass, prototype, wrappedObject, destroyFunction);
}

gpointer jscContextWrappedObject(JSCContext* context, JSObjectRef jsObject)
{
    return wrapperMap(context).wrappedObject(context->priv->jsContext.get(), jsObject);
}

JSCClass* jscContextGetRegisteredClass(JSCContext* context, JSClassRef jsClass)
{
    return wrapperMap(context).registeredClass(jsClass);
}

CallbackData jscContextPushCallback(JSCContext* context, JSValueRef calleeValue, JSValueRef thisValue, size_t argumentCount, const JSValueRef* arguments)
{
    Thread& thread = Thread::current();
    auto* previousStack = static_cast<CallbackData*>(thread.m_apiData);
    CallbackData data = { context, WTFMove(context->priv->exception), calleeValue, thisValue, argumentCount, arguments, previousStack };
    thread.m_apiData = &data;
    return data;
}

void jscContextPopCallback(JSCContext* context, CallbackData&& data)
{
    Thread& thread = Thread::current();
    context->priv->exception = WTFMove(data.preservedException);
    thread.m_apiData = data.next;
}

JSValueRef jscContextGArrayToJSArray(JSCContext* context, GPtrArray* gArray, JSValueRef* exception)
{
    JSCContextPrivate* priv = context->priv;
    JSC::JSGlobalObject* globalObject = toJS(priv->jsContext.get());
    JSC::JSLockHolder locker(globalObject);

    auto* jsArray = JSObjectMakeArray(priv->jsContext.get(), 0, nullptr, exception);
    if (*exception)
        return JSValueMakeUndefined(priv->jsContext.get());

    if (!gArray)
        return jsArray;

    auto* jsArrayObject = JSValueToObject(priv->jsContext.get(), jsArray, exception);
    if (*exception)
        return JSValueMakeUndefined(priv->jsContext.get());

    for (unsigned i = 0; i < gArray->len; ++i) {
        gpointer item = g_ptr_array_index(gArray, i);
        if (!item)
            JSObjectSetPropertyAtIndex(priv->jsContext.get(), jsArrayObject, i, JSValueMakeNull(priv->jsContext.get()), exception);
        else if (JSC_IS_VALUE(item))
            JSObjectSetPropertyAtIndex(priv->jsContext.get(), jsArrayObject, i, jscValueGetJSValue(JSC_VALUE(item)), exception);
        else
            *exception = toRef(JSC::createTypeError(globalObject, makeString("invalid item type in GPtrArray")));

        if (*exception)
            return JSValueMakeUndefined(priv->jsContext.get());
    }

    return jsArray;
}

static GRefPtr<GPtrArray> jscContextJSArrayToGArray(JSCContext* context, JSValueRef jsArray, JSValueRef* exception)
{
    JSCContextPrivate* priv = context->priv;
    JSC::JSGlobalObject* globalObject = toJS(priv->jsContext.get());
    JSC::JSLockHolder locker(globalObject);

    if (JSValueIsNull(priv->jsContext.get(), jsArray))
        return nullptr;

    if (!JSValueIsArray(priv->jsContext.get(), jsArray)) {
        *exception = toRef(JSC::createTypeError(globalObject, makeString("invalid js type for GPtrArray")));
        return nullptr;
    }

    auto* jsArrayObject = JSValueToObject(priv->jsContext.get(), jsArray, exception);
    if (*exception)
        return nullptr;

    JSRetainPtr<JSStringRef> lengthString(Adopt, JSStringCreateWithUTF8CString("length"));
    auto* jsLength = JSObjectGetProperty(priv->jsContext.get(), jsArrayObject, lengthString.get(), exception);
    if (*exception)
        return nullptr;

    auto length = JSC::toUInt32(JSValueToNumber(priv->jsContext.get(), jsLength, exception));
    if (*exception)
        return nullptr;

    GRefPtr<GPtrArray> gArray = adoptGRef(g_ptr_array_new_with_free_func(g_object_unref));
    for (unsigned i = 0; i < length; ++i) {
        auto* jsItem = JSObjectGetPropertyAtIndex(priv->jsContext.get(), jsArrayObject, i, exception);
        if (*exception)
            return nullptr;

        g_ptr_array_add(gArray.get(), jsItem ? jscContextGetOrCreateValue(context, jsItem).leakRef() : nullptr);
    }

    return gArray;
}

GUniquePtr<char*> jscContextJSArrayToGStrv(JSCContext* context, JSValueRef jsArray, JSValueRef* exception)
{
    JSCContextPrivate* priv = context->priv;
    JSC::JSGlobalObject* globalObject = toJS(priv->jsContext.get());
    JSC::JSLockHolder locker(globalObject);

    if (JSValueIsNull(priv->jsContext.get(), jsArray))
        return nullptr;

    if (!JSValueIsArray(priv->jsContext.get(), jsArray)) {
        *exception = toRef(JSC::createTypeError(globalObject, makeString("invalid js type for GStrv")));
        return nullptr;
    }

    auto* jsArrayObject = JSValueToObject(priv->jsContext.get(), jsArray, exception);
    if (*exception)
        return nullptr;

    JSRetainPtr<JSStringRef> lengthString(Adopt, JSStringCreateWithUTF8CString("length"));
    auto* jsLength = JSObjectGetProperty(priv->jsContext.get(), jsArrayObject, lengthString.get(), exception);
    if (*exception)
        return nullptr;

    auto length = JSC::toUInt32(JSValueToNumber(priv->jsContext.get(), jsLength, exception));
    if (*exception)
        return nullptr;

    GUniquePtr<char*> strv(static_cast<char**>(g_new0(char*, length + 1)));
    for (unsigned i = 0; i < length; ++i) {
        auto* jsItem = JSObjectGetPropertyAtIndex(priv->jsContext.get(), jsArrayObject, i, exception);
        if (*exception)
            return nullptr;

        auto jsValueItem = jscContextGetOrCreateValue(context, jsItem);
        if (!jsc_value_is_string(jsValueItem.get())) {
            *exception = toRef(JSC::createTypeError(globalObject, makeString("invalid js type for GStrv: item ", String::number(i), " is not a string")));
            return nullptr;
        }

        strv.get()[i] = jsc_value_to_string(jsValueItem.get());
    }

    return strv;
}

JSValueRef jscContextGValueToJSValue(JSCContext* context, const GValue* value, JSValueRef* exception)
{
    JSCContextPrivate* priv = context->priv;
    JSC::JSGlobalObject* globalObject = toJS(priv->jsContext.get());
    JSC::JSLockHolder locker(globalObject);

    switch (g_type_fundamental(G_VALUE_TYPE(value))) {
    case G_TYPE_BOOLEAN:
        return JSValueMakeBoolean(priv->jsContext.get(), g_value_get_boolean(value));
    case G_TYPE_CHAR:
    case G_TYPE_INT:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_int);
    case G_TYPE_ENUM:
        return JSValueMakeNumber(priv->jsContext.get(), g_value_get_enum(value));
    case G_TYPE_FLAGS:
        return JSValueMakeNumber(priv->jsContext.get(), g_value_get_flags(value));
    case G_TYPE_UCHAR:
    case G_TYPE_UINT:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_uint);
    case G_TYPE_FLOAT:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_float);
    case G_TYPE_DOUBLE:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_double);
    case G_TYPE_LONG:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_long);
    case G_TYPE_ULONG:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_ulong);
    case G_TYPE_INT64:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_int64);
    case G_TYPE_UINT64:
        return JSValueMakeNumber(priv->jsContext.get(), value->data[0].v_uint64);
    case G_TYPE_STRING:
        if (const char* stringValue = g_value_get_string(value)) {
            JSRetainPtr<JSStringRef> jsString(Adopt, JSStringCreateWithUTF8CString(stringValue));
            return JSValueMakeString(priv->jsContext.get(), jsString.get());
        }
        return JSValueMakeNull(priv->jsContext.get());
    case G_TYPE_POINTER:
    case G_TYPE_OBJECT:
    case G_TYPE_BOXED:
        if (auto* ptr = value->data[0].v_pointer) {
            if (auto* jsWrapper = jscContextGetJSWrapper(context, ptr))
                return toRef(jsWrapper);

            if (g_type_is_a(G_VALUE_TYPE(value), JSC_TYPE_VALUE))
                return jscValueGetJSValue(JSC_VALUE(ptr));

            if (g_type_is_a(G_VALUE_TYPE(value), JSC_TYPE_EXCEPTION))
                return jscExceptionGetJSValue(JSC_EXCEPTION(ptr));

            if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_PTR_ARRAY))
                return jscContextGArrayToJSArray(context, static_cast<GPtrArray*>(ptr), exception);

            if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_STRV)) {
                auto** strv = static_cast<char**>(ptr);
                auto strvLength = g_strv_length(strv);
                GRefPtr<GPtrArray> gArray = adoptGRef(g_ptr_array_new_full(strvLength, g_object_unref));
                for (unsigned i = 0; i < strvLength; i++)
                    g_ptr_array_add(gArray.get(), jsc_value_new_string(context, strv[i]));
                return jscContextGArrayToJSArray(context, gArray.get(), exception);
            }
        } else
            return JSValueMakeNull(priv->jsContext.get());

        break;
    case G_TYPE_PARAM:
    case G_TYPE_INTERFACE:
    case G_TYPE_VARIANT:
    default:
        break;
    }

    *exception = toRef(JSC::createTypeError(globalObject, makeString("unsupported type ", g_type_name(G_VALUE_TYPE(value)))));
    return JSValueMakeUndefined(priv->jsContext.get());
}

void jscContextJSValueToGValue(JSCContext* context, JSValueRef jsValue, GType type, GValue* value, JSValueRef* exception)
{
    JSCContextPrivate* priv = context->priv;
    JSC::JSGlobalObject* globalObject = toJS(priv->jsContext.get());
    JSC::JSLockHolder locker(globalObject);

    g_value_init(value, type);
    auto fundamentalType = g_type_fundamental(G_VALUE_TYPE(value));
    switch (fundamentalType) {
    case G_TYPE_INT:
        g_value_set_int(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_FLOAT:
        g_value_set_float(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_DOUBLE:
        g_value_set_double(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_BOOLEAN:
        g_value_set_boolean(value, JSValueToBoolean(priv->jsContext.get(), jsValue));
        break;
    case G_TYPE_STRING:
        if (!JSValueIsNull(priv->jsContext.get(), jsValue)) {
            JSRetainPtr<JSStringRef> jsString(Adopt, JSValueToStringCopy(priv->jsContext.get(), jsValue, exception));
            if (*exception)
                return;
            size_t maxSize = JSStringGetMaximumUTF8CStringSize(jsString.get());
            auto* string = static_cast<char*>(g_malloc(maxSize));
            JSStringGetUTF8CString(jsString.get(), string, maxSize);
            g_value_take_string(value, string);
        } else
            g_value_set_string(value, nullptr);
        break;
    case G_TYPE_CHAR:
        g_value_set_schar(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_UCHAR:
        g_value_set_uchar(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_UINT:
        g_value_set_uint(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_POINTER:
    case G_TYPE_OBJECT:
    case G_TYPE_BOXED: {
        gpointer wrappedObject = nullptr;

        if (!JSValueIsNull(priv->jsContext.get(), jsValue)) {
            auto jsObject = JSValueToObject(priv->jsContext.get(), jsValue, exception);
            if (*exception)
                return;

            wrappedObject = jscContextWrappedObject(context, jsObject);
            if (!wrappedObject) {
                if (g_type_is_a(G_VALUE_TYPE(value), JSC_TYPE_VALUE)) {
                    auto jscValue = jscContextGetOrCreateValue(context, jsValue);
                    g_value_set_object(value, jscValue.get());
                    return;
                }

                if (g_type_is_a(G_VALUE_TYPE(value), JSC_TYPE_EXCEPTION)) {
                    auto exception = jscExceptionCreate(context, jsValue);
                    g_value_set_object(value, exception.get());
                    return;
                }

                if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_PTR_ARRAY)) {
                    auto gArray = jscContextJSArrayToGArray(context, jsValue, exception);
                    if (!*exception)
                        g_value_take_boxed(value, gArray.leakRef());
                    return;
                }

                if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_STRV)) {
                    auto strv = jscContextJSArrayToGStrv(context, jsValue, exception);
                    if (!*exception)
                        g_value_take_boxed(value, strv.release());
                    return;
                }

                *exception = toRef(JSC::createTypeError(globalObject, "invalid pointer type"_s));
                return;
            }
        }
        if (fundamentalType == G_TYPE_POINTER)
            g_value_set_pointer(value, wrappedObject);
        else if (fundamentalType == G_TYPE_BOXED)
            g_value_set_boxed(value, wrappedObject);
        else if (G_IS_OBJECT(wrappedObject))
            g_value_set_object(value, wrappedObject);
        else
            *exception = toRef(JSC::createTypeError(globalObject, "wrapped object is not a GObject"_s));
        break;
    }
    case G_TYPE_LONG:
        g_value_set_long(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_ULONG:
        g_value_set_ulong(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_INT64:
        g_value_set_int64(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_UINT64:
        g_value_set_uint64(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_ENUM:
        g_value_set_enum(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_FLAGS:
        g_value_set_flags(value, JSValueToNumber(priv->jsContext.get(), jsValue, exception));
        break;
    case G_TYPE_PARAM:
    case G_TYPE_INTERFACE:
    case G_TYPE_VARIANT:
    default:
        *exception = toRef(JSC::createTypeError(globalObject, makeString("unsupported type ", g_type_name(G_VALUE_TYPE(value)))));
        break;
    }
}

/**
 * jsc_context_new:
 *
 * Create a new #JSCContext. The context is created in a new #JSCVirtualMachine.
 * Use jsc_context_new_with_virtual_machine() to create a new #JSCContext in an
 * existing #JSCVirtualMachine.
 *
 * Returns: (transfer full): the newly created #JSCContext.
 */
JSCContext* jsc_context_new()
{
    return JSC_CONTEXT(g_object_new(JSC_TYPE_CONTEXT, nullptr));
}

/**
 * jsc_context_new_with_virtual_machine:
 * @vm: a #JSCVirtualMachine
 *
 * Create a new #JSCContext in @virtual_machine.
 *
 * Returns: (transfer full): the newly created #JSCContext.
 */
JSCContext* jsc_context_new_with_virtual_machine(JSCVirtualMachine* vm)
{
    g_return_val_if_fail(JSC_IS_VIRTUAL_MACHINE(vm), nullptr);
    return JSC_CONTEXT(g_object_new(JSC_TYPE_CONTEXT, "virtual-machine", vm, nullptr));
}

/**
 * jsc_context_get_virtual_machine:
 * @context: a #JSCContext
 *
 * Get the #JSCVirtualMachine where @context was created.
 *
 * Returns: (transfer none): the #JSCVirtualMachine where the #JSCContext was created.
 */
JSCVirtualMachine* jsc_context_get_virtual_machine(JSCContext* context)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    return context->priv->vm.get();
}

/**
 * jsc_context_get_exception:
 * @context: a #JSCContext
 *
 * Get the last unhandled exception thrown in @context by API functions calls.
 *
 * Returns: (transfer none) (nullable): a #JSCException or %NULL if there isn't any
 *    unhandled exception in the #JSCContext.
 */
JSCException* jsc_context_get_exception(JSCContext *context)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    return context->priv->exception.get();
}

/**
 * jsc_context_throw:
 * @context: a #JSCContext
 * @error_message: an error message
 *
 * Throw an exception to @context using the given error message. The created #JSCException
 * can be retrieved with jsc_context_get_exception().
 */
void jsc_context_throw(JSCContext* context, const char* errorMessage)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));

    context->priv->exception = adoptGRef(jsc_exception_new(context, errorMessage));
}

/**
 * jsc_context_throw_printf:
 * @context: a #JSCContext
 * @format: the string format
 * @...: the parameters to insert into the format string
 *
 * Throw an exception to @context using the given formatted string as error message.
 * The created #JSCException can be retrieved with jsc_context_get_exception().
 */
void jsc_context_throw_printf(JSCContext* context, const char* format, ...)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));

    va_list args;
    va_start(args, format);
    context->priv->exception = adoptGRef(jsc_exception_new_vprintf(context, format, args));
    va_end(args);
}

/**
 * jsc_context_throw_with_name:
 * @context: a #JSCContext
 * @error_name: the error name
 * @error_message: an error message
 *
 * Throw an exception to @context using the given error name and message. The created #JSCException
 * can be retrieved with jsc_context_get_exception().
 */
void jsc_context_throw_with_name(JSCContext* context, const char* errorName, const char* errorMessage)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));
    g_return_if_fail(errorName);

    context->priv->exception = adoptGRef(jsc_exception_new_with_name(context, errorName, errorMessage));
}

/**
 * jsc_context_throw_with_name_printf:
 * @context: a #JSCContext
 * @error_name: the error name
 * @format: the string format
 * @...: the parameters to insert into the format string
 *
 * Throw an exception to @context using the given error name and the formatted string as error message.
 * The created #JSCException can be retrieved with jsc_context_get_exception().
 */
void jsc_context_throw_with_name_printf(JSCContext* context, const char* errorName, const char* format, ...)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));

    va_list args;
    va_start(args, format);
    context->priv->exception = adoptGRef(jsc_exception_new_with_name_vprintf(context, errorName, format, args));
    va_end(args);
}

/**
 * jsc_context_throw_exception:
 * @context: a #JSCContext
 * @exception: a #JSCException
 *
 * Throw @exception to @context.
 */
void jsc_context_throw_exception(JSCContext* context, JSCException* exception)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));
    g_return_if_fail(JSC_IS_EXCEPTION(exception));

    context->priv->exception = exception;
}

/**
 * jsc_context_clear_exception:
 * @context: a #JSCContext
 *
 * Clear the uncaught exception in @context if any.
 */
void jsc_context_clear_exception(JSCContext* context)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));

    context->priv->exception = nullptr;
}

/**
 * JSCExceptionHandler:
 * @context: a #JSCContext
 * @exception: a #JSCException
 * @user_data: user data
 *
 * Function used to handle JavaScript exceptions in a #JSCContext.
 */

/**
 * jsc_context_push_exception_handler:
 * @context: a #JSCContext
 * @handler: a #JSCExceptionHandler
 * @user_data: (closure): user data to pass to @handler
 * @destroy_notify: (nullable): destroy notifier for @user_data
 *
 * Push an exception handler in @context. Whenever a JavaScript exception happens in
 * the #JSCContext, the given @handler will be called. The default #JSCExceptionHandler
 * simply calls jsc_context_throw_exception() to throw the exception to the #JSCContext.
 * If you don't want to catch the exception, but only get notified about it, call
 * jsc_context_throw_exception() in @handler like the default one does.
 * The last exception handler pushed is the only one used by the #JSCContext, use
 * jsc_context_pop_exception_handler() to remove it and set the previous one. When @handler
 * is removed from the context, @destroy_notify i called with @user_data as parameter.
 */
void jsc_context_push_exception_handler(JSCContext* context, JSCExceptionHandler handler, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));
    g_return_if_fail(handler);

    context->priv->exceptionHandlers.append({ handler, userData, destroyNotify });
}

/**
 * jsc_context_pop_exception_handler:
 * @context: a #JSCContext
 *
 * Remove the last #JSCExceptionHandler previously pushed to @context with
 * jsc_context_push_exception_handler().
 */
void jsc_context_pop_exception_handler(JSCContext* context)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));
    g_return_if_fail(context->priv->exceptionHandlers.size() > 1);

    context->priv->exceptionHandlers.removeLast();
}

bool jscContextHandleExceptionIfNeeded(JSCContext* context, JSValueRef jsException)
{
    if (!jsException)
        return false;

    auto exception = jscExceptionCreate(context, jsException);
    ASSERT(!context->priv->exceptionHandlers.isEmpty());
    const auto& exceptionHandler = context->priv->exceptionHandlers.last();
    exceptionHandler.handler(context, exception.get(), exceptionHandler.userData);

    return true;
}

/**
 * jsc_context_get_current:
 *
 * Get the #JSCContext that is currently executing a function. This should only be
 * called within a function or method callback, otherwise %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): the #JSCContext that is currently executing.
 */
JSCContext* jsc_context_get_current()
{
    auto* data = static_cast<CallbackData*>(Thread::current().m_apiData);
    return data ? data->context.get() : nullptr;
}

/**
 * jsc_context_evaluate:
 * @context: a #JSCContext
 * @code: a JavaScript script to evaluate
 * @length: length of @code, or -1 if @code is a nul-terminated string
 *
 * Evaluate @code in @context.
 *
 * Returns: (transfer full): a #JSCValue representing the last value generated by the script.
 */
JSCValue* jsc_context_evaluate(JSCContext* context, const char* code, gssize length)
{
    return jsc_context_evaluate_with_source_uri(context, code, length, nullptr, 0);
}

static JSValueRef evaluateScriptInContext(JSGlobalContextRef jsContext, String&& script, const char* uri, unsigned lineNumber, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> scriptJS(Adopt, OpaqueJSString::tryCreate(WTFMove(script)).leakRef());
    JSRetainPtr<JSStringRef> sourceURI = uri ? adopt(JSStringCreateWithUTF8CString(uri)) : nullptr;
    return JSEvaluateScript(jsContext, scriptJS.get(), nullptr, sourceURI.get(), lineNumber, exception);
}

/**
 * jsc_context_evaluate_with_source_uri:
 * @context: a #JSCContext
 * @code: a JavaScript script to evaluate
 * @length: length of @code, or -1 if @code is a nul-terminated string
 * @uri: the source URI
 * @line_number: the starting line number
 *
 * Evaluate @code in @context using @uri as the source URI. The @line_number is the starting line number
 * in @uri; the value is one-based so the first line is 1. @uri and @line_number will be shown in exceptions and
 * they don't affect the behavior of the script.
 *
 * Returns: (transfer full): a #JSCValue representing the last value generated by the script.
 */
JSCValue* jsc_context_evaluate_with_source_uri(JSCContext* context, const char* code, gssize length, const char* uri, unsigned lineNumber)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(code, nullptr);

    JSValueRef exception = nullptr;
    JSValueRef result = evaluateScriptInContext(context->priv->jsContext.get(), String::fromUTF8(code, length < 0 ? strlen(code) : length), uri, lineNumber, &exception);
    if (jscContextHandleExceptionIfNeeded(context, exception))
        return jsc_value_new_undefined(context);

    return jscContextGetOrCreateValue(context, result).leakRef();
}

/**
 * jsc_context_evaluate_in_object:
 * @context: a #JSCContext
 * @code: a JavaScript script to evaluate
 * @length: length of @code, or -1 if @code is a nul-terminated string
 * @object_instance: (nullable): an object instance
 * @object_class: (nullable): a #JSCClass or %NULL to use the default
 * @uri: the source URI
 * @line_number: the starting line number
 * @object: (out) (transfer full): return location for a #JSCValue.
 *
 * Evaluate @code and create an new object where symbols defined in @code will be added as properties,
 * instead of being added to @context global object. The new object is returned as @object parameter.
 * Similar to how jsc_value_new_object() works, if @object_instance is not %NULL @object_class must be provided too.
 * The @line_number is the starting line number in @uri; the value is one-based so the first line is 1.
 * @uri and @line_number will be shown in exceptions and they don't affect the behavior of the script.
 *
 * Returns: (transfer full): a #JSCValue representing the last value generated by the script.
 */
JSCValue* jsc_context_evaluate_in_object(JSCContext* context, const char* code, gssize length, gpointer instance, JSCClass* objectClass, const char* uri, unsigned lineNumber, JSCValue** object)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(code, nullptr);
    g_return_val_if_fail(!instance || JSC_IS_CLASS(objectClass), nullptr);
    g_return_val_if_fail(object && !*object, nullptr);

    JSRetainPtr<JSGlobalContextRef> objectContext(Adopt,
        instance ? jscClassCreateContextWithJSWrapper(objectClass, context, instance) : JSGlobalContextCreateInGroup(jscVirtualMachineGetContextGroup(context->priv->vm.get()), nullptr));
    JSC::JSGlobalObject* globalObject = toJS(objectContext.get());
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(globalObject);
    globalObject->setGlobalScopeExtension(JSC::JSWithScope::create(vm, globalObject, globalObject->globalScope(), toJS(JSContextGetGlobalObject(context->priv->jsContext.get()))));
    JSValueRef exception = nullptr;
    JSValueRef result = evaluateScriptInContext(objectContext.get(), String::fromUTF8(code, length < 0 ? strlen(code) : length), uri, lineNumber, &exception);
    if (jscContextHandleExceptionIfNeeded(context, exception))
        return jsc_value_new_undefined(context);

    *object = jscContextGetOrCreateValue(context, JSContextGetGlobalObject(objectContext.get())).leakRef();

    return jscContextGetOrCreateValue(context, result).leakRef();
}

/**
 * JSCCheckSyntaxMode:
 * @JSC_CHECK_SYNTAX_MODE_SCRIPT: mode to check syntax of a script
 * @JSC_CHECK_SYNTAX_MODE_MODULE: mode to check syntax of a module
 *
 * Enum values to specify a mode to check for syntax errors in jsc_context_check_syntax().
 */

/**
 * JSCCheckSyntaxResult:
 * @JSC_CHECK_SYNTAX_RESULT_SUCCESS: no errors
 * @JSC_CHECK_SYNTAX_RESULT_RECOVERABLE_ERROR: recoverable syntax error
 * @JSC_CHECK_SYNTAX_RESULT_IRRECOVERABLE_ERROR: irrecoverable syntax error
 * @JSC_CHECK_SYNTAX_RESULT_UNTERMINATED_LITERAL_ERROR: unterminated literal error
 * @JSC_CHECK_SYNTAX_RESULT_OUT_OF_MEMORY_ERROR: out of memory error
 * @JSC_CHECK_SYNTAX_RESULT_STACK_OVERFLOW_ERROR: stack overflow error
 *
 * Enum values to specify the result of jsc_context_check_syntax().
 */

/**
 * jsc_context_check_syntax:
 * @context: a #JSCContext
 * @code: a JavaScript script to check
 * @length: length of @code, or -1 if @code is a nul-terminated string
 * @mode: a #JSCCheckSyntaxMode
 * @uri: the source URI
 * @line_number: the starting line number
 * @exception: (out) (optional) (transfer full): return location for a #JSCException, or %NULL to ignore
 *
 * Check the given @code in @context for syntax errors. The @line_number is the starting line number in @uri;
 * the value is one-based so the first line is 1. @uri and @line_number are only used to fill the @exception.
 * In case of errors @exception will be set to a new #JSCException with the details. You can pass %NULL to
 * @exception to ignore the error details.
 *
 * Returns: a #JSCCheckSyntaxResult
 */
JSCCheckSyntaxResult jsc_context_check_syntax(JSCContext* context, const char* code, gssize length, JSCCheckSyntaxMode mode, const char* uri, unsigned lineNumber, JSCException **exception)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), JSC_CHECK_SYNTAX_RESULT_IRRECOVERABLE_ERROR);
    g_return_val_if_fail(code, JSC_CHECK_SYNTAX_RESULT_IRRECOVERABLE_ERROR);
    g_return_val_if_fail(!exception || !*exception, JSC_CHECK_SYNTAX_RESULT_IRRECOVERABLE_ERROR);

    lineNumber = std::max<unsigned>(1, lineNumber);

    auto* jsContext = context->priv->jsContext.get();
    JSC::JSGlobalObject* globalObject = toJS(jsContext);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    URL sourceURL = uri ? URL({ }, uri) : URL();
    JSC::SourceCode source = JSC::makeSource(String::fromUTF8(code, length < 0 ? strlen(code) : length), JSC::SourceOrigin { sourceURL },
        sourceURL.string() , TextPosition(OrdinalNumber::fromOneBasedInt(lineNumber), OrdinalNumber()));
    bool success = false;
    JSC::ParserError error;
    switch (mode) {
    case JSC_CHECK_SYNTAX_MODE_SCRIPT:
        success = !!JSC::parse<JSC::ProgramNode>(vm, source, JSC::Identifier(), JSC::JSParserBuiltinMode::NotBuiltin,
            JSC::JSParserStrictMode::NotStrict, JSC::JSParserScriptMode::Classic, JSC::SourceParseMode::ProgramMode, JSC::SuperBinding::NotNeeded, error);
        break;
    case JSC_CHECK_SYNTAX_MODE_MODULE:
        success = !!JSC::parse<JSC::ModuleProgramNode>(vm, source, JSC::Identifier(), JSC::JSParserBuiltinMode::NotBuiltin,
            JSC::JSParserStrictMode::Strict, JSC::JSParserScriptMode::Module, JSC::SourceParseMode::ModuleAnalyzeMode, JSC::SuperBinding::NotNeeded, error);
        break;
    }

    JSCCheckSyntaxResult result = JSC_CHECK_SYNTAX_RESULT_SUCCESS;
    if (success)
        return result;

    switch (error.type()) {
    case JSC::ParserError::ErrorType::SyntaxError: {
        switch (error.syntaxErrorType()) {
        case JSC::ParserError::SyntaxErrorType::SyntaxErrorIrrecoverable:
            result = JSC_CHECK_SYNTAX_RESULT_IRRECOVERABLE_ERROR;
            break;
        case JSC::ParserError::SyntaxErrorType::SyntaxErrorUnterminatedLiteral:
            result = JSC_CHECK_SYNTAX_RESULT_UNTERMINATED_LITERAL_ERROR;
            break;
        case JSC::ParserError::SyntaxErrorType::SyntaxErrorRecoverable:
            result = JSC_CHECK_SYNTAX_RESULT_RECOVERABLE_ERROR;
            break;
        case JSC::ParserError::SyntaxErrorType::SyntaxErrorNone:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
    case JSC::ParserError::ErrorType::StackOverflow:
        result = JSC_CHECK_SYNTAX_RESULT_STACK_OVERFLOW_ERROR;
        break;
    case JSC::ParserError::ErrorType::OutOfMemory:
        result = JSC_CHECK_SYNTAX_RESULT_OUT_OF_MEMORY_ERROR;
        break;
    case JSC::ParserError::ErrorType::EvalError:
    case JSC::ParserError::ErrorType::ErrorNone:
        ASSERT_NOT_REACHED();
        break;
    }

    if (exception) {
        auto* jsError = error.toErrorObject(globalObject, source);
        *exception = jscExceptionCreate(context, toRef(globalObject, jsError)).leakRef();
    }

    return result;
}

/**
 * jsc_context_get_global_object:
 * @context: a #JSCContext
 *
 * Get a #JSCValue referencing the @context global object
 *
 * Returns: (transfer full): a #JSCValue
 */
JSCValue* jsc_context_get_global_object(JSCContext* context)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    return jscContextGetOrCreateValue(context, JSContextGetGlobalObject(context->priv->jsContext.get())).leakRef();
}

/**
 * jsc_context_set_value:
 * @context: a #JSCContext
 * @name: the value name
 * @value: a #JSCValue
 *
 * Set a property of @context global object with @name and @value.
 */
void jsc_context_set_value(JSCContext* context, const char* name, JSCValue* value)
{
    g_return_if_fail(JSC_IS_CONTEXT(context));
    g_return_if_fail(name);
    g_return_if_fail(JSC_IS_VALUE(value));

    auto contextObject = jscContextGetOrCreateValue(context, JSContextGetGlobalObject(context->priv->jsContext.get()));
    jsc_value_object_set_property(contextObject.get(), name, value);
}

/**
 * jsc_context_get_value:
 * @context: a #JSCContext
 * @name: the value name
 *
 * Get a property of @context global object with @name.
 *
 * Returns: (transfer full): a #JSCValue
 */
JSCValue* jsc_context_get_value(JSCContext* context, const char* name)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(name, nullptr);

    auto contextObject = jscContextGetOrCreateValue(context, JSContextGetGlobalObject(context->priv->jsContext.get()));
    return jsc_value_object_get_property(contextObject.get(), name);
}

/**
 * jsc_context_register_class:
 * @context: a #JSCContext
 * @name: the class name
 * @parent_class: (nullable): a #JSCClass or %NULL
 * @vtable: (nullable): an optional #JSCClassVTable or %NULL
 * @destroy_notify: (nullable): a destroy notifier for class instances
 *
 * Register a custom class in @context using the given @name. If the new class inherits from
 * another #JSCClass, the parent should be passed as @parent_class, otherwise %NULL should be
 * used. The optional @vtable parameter allows to provide a custom implementation for handling
 * the class, for example, to handle external properties not added to the prototype.
 * When an instance of the #JSCClass is cleared in the context, @destroy_notify is called with
 * the instance as parameter.
 *
 * Returns: (transfer none): a #JSCClass
 */
JSCClass* jsc_context_register_class(JSCContext* context, const char* name, JSCClass* parentClass, JSCClassVTable* vtable, GDestroyNotify destroyFunction)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(name, nullptr);
    g_return_val_if_fail(!parentClass || JSC_IS_CLASS(parentClass), nullptr);

    auto jscClass = jscClassCreate(context, name, parentClass, vtable, destroyFunction);
    wrapperMap(context).registerClass(jscClass.get());
    return jscClass.get();
}
