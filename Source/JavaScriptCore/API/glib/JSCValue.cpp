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
#include "JSCValue.h"

#include "APICast.h"
#include "APIUtils.h"
#include "JSArrayBuffer.h"
#include "JSCCallbackFunction.h"
#include "JSCClassPrivate.h"
#include "JSCContextPrivate.h"
#include "JSCInlines.h"
#include "JSCValuePrivate.h"
#include "JSRetainPtr.h"
#include "JSTypedArray.h"
#include "LiteralParser.h"
#include "OpaqueJSString.h"
#include "TypedArrayType.h"
#include <gobject/gvaluecollector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/MakeString.h>

/**
 * JSCValue:
 * @short_description: JavaScript value
 * @title: JSCValue
 * @see_also: JSCContext
 *
 * JSCValue represents a reference to a value in a #JSCContext. The JSCValue
 * protects the referenced value from being garbage collected.
 */

enum {
    PROP_0,

    PROP_CONTEXT,
};

struct _JSCValuePrivate {
    GRefPtr<JSCContext> context;
    JSValueRef jsValue;
};

WEBKIT_DEFINE_FINAL_TYPE(JSCValue, jsc_value, G_TYPE_OBJECT, GObject)

static void jscValueGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    JSCValuePrivate* priv = JSC_VALUE(object)->priv;

    switch (propID) {
    case PROP_CONTEXT:
        g_value_set_object(value, priv->context.get());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void jscValueSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    JSCValuePrivate* priv = JSC_VALUE(object)->priv;

    switch (propID) {
    case PROP_CONTEXT:
        priv->context = JSC_CONTEXT(g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void jscValueDispose(GObject* object)
{
    JSCValuePrivate* priv = JSC_VALUE(object)->priv;

    if (priv->context) {
        auto* jsContext = jscContextGetJSContext(priv->context.get());

        JSValueUnprotect(jsContext, priv->jsValue);
        jscContextValueDestroyed(priv->context.get(), priv->jsValue);
        priv->jsValue = nullptr;
        priv->context = nullptr;
    }

    G_OBJECT_CLASS(jsc_value_parent_class)->dispose(object);
}

static void jsc_value_class_init(JSCValueClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS(klass);

    objClass->get_property = jscValueGetProperty;
    objClass->set_property = jscValueSetProperty;
    objClass->dispose = jscValueDispose;

    /**
     * JSCValue:context:
     *
     * The #JSCContext in which the value was created.
     */
    g_object_class_install_property(objClass,
        PROP_CONTEXT,
        g_param_spec_object(
            "context",
            nullptr, nullptr,
            JSC_TYPE_CONTEXT,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

JSValueRef jscValueGetJSValue(JSCValue* value)
{
    return value->priv->jsValue;
}

JSCValue* jscValueCreate(JSCContext* context, JSValueRef jsValue)
{
    auto* value = JSC_VALUE(g_object_new(JSC_TYPE_VALUE, "context", context, nullptr));
    JSValueProtect(jscContextGetJSContext(context), jsValue);
    value->priv->jsValue = jsValue;
    return value;
}

/**
 * jsc_value_get_context:
 * @value: a #JSCValue
 *
 * Get the #JSCContext in which @value was created.
 *
 * Returns: (transfer none): the #JSCValue context.
 */
JSCContext* jsc_value_get_context(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    return value->priv->context.get();
}

/**
 * jsc_value_new_undefined:
 * @context: a #JSCContext
 *
 * Create a new #JSCValue referencing <function>undefined</function> in @context.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_undefined(JSCContext* context)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    return jscContextGetOrCreateValue(context, JSValueMakeUndefined(jscContextGetJSContext(context))).leakRef();
}

/**
 * jsc_value_is_undefined:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is <function>undefined</function>.
 *
 * Returns: whether the value is undefined.
 */
gboolean jsc_value_is_undefined(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueIsUndefined(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_new_null:
 * @context: a #JSCContext
 *
 * Create a new #JSCValue referencing <function>null</function> in @context.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_null(JSCContext* context)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    return jscContextGetOrCreateValue(context, JSValueMakeNull(jscContextGetJSContext(context))).leakRef();
}

/**
 * jsc_value_is_null:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is <function>null</function>.
 *
 * Returns: whether the value is null.
 */
gboolean jsc_value_is_null(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueIsNull(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_new_number:
 * @context: a #JSCContext
 * @number: a number
 *
 * Create a new #JSCValue from @number.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_number(JSCContext* context, double number)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    return jscContextGetOrCreateValue(context, JSValueMakeNumber(jscContextGetJSContext(context), number)).leakRef();
}

/**
 * jsc_value_is_number:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is a number.
 *
 * Returns: whether the value is a number.
 */
gboolean jsc_value_is_number(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueIsNumber(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_to_double:
 * @value: a #JSCValue
 *
 * Convert @value to a double.
 *
 * Returns: a #gdouble result of the conversion.
 */
double jsc_value_to_double(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), std::numeric_limits<double>::quiet_NaN());

    JSCValuePrivate* priv = value->priv;
    JSValueRef exception = nullptr;
    auto result = JSValueToNumber(jscContextGetJSContext(priv->context.get()), priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return std::numeric_limits<double>::quiet_NaN();

    return result;
}

/**
 * jsc_value_to_int32:
 * @value: a #JSCValue
 *
 * Convert @value to a #gint32.
 *
 * Returns: a #gint32 result of the conversion.
 */
gint32 jsc_value_to_int32(JSCValue* value)
{
    return JSC::toInt32(jsc_value_to_double(value));
}

/**
 * jsc_value_new_boolean:
 * @context: a #JSCContext
 * @value: a #gboolean
 *
 * Create a new #JSCValue from @value
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_boolean(JSCContext* context, gboolean value)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    return jscContextGetOrCreateValue(context, JSValueMakeBoolean(jscContextGetJSContext(context), value)).leakRef();
}

/**
 * jsc_value_is_boolean:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is a boolean.
 *
 * Returns: whether the value is a boolean.
 */
gboolean jsc_value_is_boolean(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueIsBoolean(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_to_boolean:
 * @value: a #JSCValue
 *
 * Convert @value to a boolean.
 *
 * Returns: a #gboolean result of the conversion.
 */
gboolean jsc_value_to_boolean(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueToBoolean(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_new_string:
 * @context: a #JSCContext
 * @string: (nullable): a null-terminated string
 *
 * Create a new #JSCValue from @string. If you need to create a #JSCValue from a
 * string containing null characters, use jsc_value_new_string_from_bytes() instead.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_string(JSCContext* context, const char* string)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    JSValueRef jsStringValue;
    if (string) {
        JSRetainPtr<JSStringRef> jsString(Adopt, JSStringCreateWithUTF8CString(string));
        jsStringValue = JSValueMakeString(jscContextGetJSContext(context), jsString.get());
    } else
        jsStringValue = JSValueMakeString(jscContextGetJSContext(context), nullptr);
    return jscContextGetOrCreateValue(context, jsStringValue).leakRef();
}

/**
 * jsc_value_new_string_from_bytes:
 * @context: a #JSCContext
 * @bytes: (nullable): a #GBytes
 *
 * Create a new #JSCValue from @bytes.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_string_from_bytes(JSCContext* context, GBytes* bytes)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    if (!bytes)
        return jsc_value_new_string(context, nullptr);

    gsize dataSize;
    const auto* data = static_cast<const char*>(g_bytes_get_data(bytes, &dataSize));
    auto string = String::fromUTF8(std::span(data, dataSize));
    JSRetainPtr<JSStringRef> jsString(Adopt, OpaqueJSString::tryCreate(WTFMove(string)).leakRef());
    return jscContextGetOrCreateValue(context, JSValueMakeString(jscContextGetJSContext(context), jsString.get())).leakRef();
}

/**
 * jsc_value_is_string:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is a string
 *
 * Returns: whether the value is a string
 */
gboolean jsc_value_is_string(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueIsString(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_to_string:
 * @value: a #JSCValue
 *
 * Convert @value to a string. Use jsc_value_to_string_as_bytes() instead, if you need to
 * handle strings containing null characters.
 *
 * Returns: (transfer full): a null-terminated string result of the conversion.
 */
char* jsc_value_to_string(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    JSCValuePrivate* priv = value->priv;
    JSValueRef exception = nullptr;
    JSRetainPtr<JSStringRef> jsString(Adopt, JSValueToStringCopy(jscContextGetJSContext(priv->context.get()), priv->jsValue, &exception));
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return nullptr;

    if (!jsString)
        return nullptr;

    size_t maxSize = JSStringGetMaximumUTF8CStringSize(jsString.get());
    auto* string = static_cast<char*>(g_malloc(maxSize));
    if (!JSStringGetUTF8CString(jsString.get(), string, maxSize)) {
        g_free(string);
        return nullptr;
    }

    return string;
}

/**
 * jsc_value_to_string_as_bytes:
 * @value: a #JSCValue
 *
 * Convert @value to a string and return the results as #GBytes. This is needed
 * to handle strings with null characters.
 *
 * Returns: (transfer full): a #GBytes with the result of the conversion.
 */
GBytes* jsc_value_to_string_as_bytes(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    JSCValuePrivate* priv = value->priv;
    JSValueRef exception = nullptr;
    JSRetainPtr<JSStringRef> jsString(Adopt, JSValueToStringCopy(jscContextGetJSContext(priv->context.get()), priv->jsValue, &exception));
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return nullptr;

    if (!jsString)
        return nullptr;

    size_t maxSize = JSStringGetMaximumUTF8CStringSize(jsString.get());
    if (maxSize == 1)
        return g_bytes_new_static("", 0);

    auto* string = static_cast<char*>(fastMalloc(maxSize));
    auto stringSize = JSStringGetUTF8CString(jsString.get(), string, maxSize);
    if (!stringSize) {
        fastFree(string);
        return nullptr;
    }

    // Ignore the null character added by JSStringGetUTF8CString.
    return g_bytes_new_with_free_func(string, stringSize - 1, fastFree, string);
}

/**
 * jsc_value_new_array: (skip)
 * @context: a #JSCContext
 * @first_item_type: #GType of first item, or %G_TYPE_NONE
 * @...: value of the first item, followed optionally by more type/value pairs, followed by %G_TYPE_NONE.
 *
 * Create a new #JSCValue referencing an array with the given items. If @first_item_type
 * is %G_TYPE_NONE an empty array is created.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_array(JSCContext* context, GType firstItemType, ...)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    auto* jsContext = jscContextGetJSContext(context);
    JSC::JSGlobalObject* globalObject = toJS(jsContext);


    JSValueRef exception = nullptr;
    auto* jsArray = JSObjectMakeArray(jsContext, 0, nullptr, &exception);
    if (jscContextHandleExceptionIfNeeded(context, exception))
        return nullptr;

    auto* jsArrayObject = JSValueToObject(jsContext, jsArray, &exception);
    if (jscContextHandleExceptionIfNeeded(context, exception))
        return nullptr;

    unsigned index = 0;
    va_list args;
    va_start(args, firstItemType);
    GType itemType = firstItemType;
    while (itemType != G_TYPE_NONE) {
        GValue item;
        GUniqueOutPtr<char> error;
        G_VALUE_COLLECT_INIT(&item, itemType, args, G_VALUE_NOCOPY_CONTENTS, &error.outPtr());
        if (error) {
            exception = toRef(JSC::createTypeError(globalObject, makeString("failed to collect array item: "_s, span(error.get()))));
            jscContextHandleExceptionIfNeeded(context, exception);
            jsArray = nullptr;
            break;
        }

        auto* jsValue = jscContextGValueToJSValue(context, &item, &exception);
        g_value_unset(&item);
        if (jscContextHandleExceptionIfNeeded(context, exception)) {
            jsArray = nullptr;
            break;
        }

        JSObjectSetPropertyAtIndex(jsContext, jsArrayObject, index, jsValue, &exception);
        if (jscContextHandleExceptionIfNeeded(context, exception)) {
            jsArray = nullptr;
            break;
        }

        itemType = va_arg(args, GType);
        index++;
    }
    va_end(args);

    return jsArray ? jscContextGetOrCreateValue(context, jsArray).leakRef() : nullptr;
}

/**
 * jsc_value_new_array_from_garray:
 * @context: a #JSCContext
 * @array: (nullable) (element-type JSCValue): a #GPtrArray
 *
 * Create a new #JSCValue referencing an array with the items from @array. If @array
 * is %NULL or empty a new empty array will be created. Elements of @array should be
 * pointers to a #JSCValue.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_array_from_garray(JSCContext* context, GPtrArray* gArray)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    if (!gArray || !gArray->len)
        return jsc_value_new_array(context, G_TYPE_NONE);

    JSValueRef exception = nullptr;
    auto* jsArray = jscContextGArrayToJSArray(context, gArray, &exception);
    if (jscContextHandleExceptionIfNeeded(context, exception))
        return nullptr;

    return jscContextGetOrCreateValue(context, jsArray).leakRef();
}

/**
 * jsc_value_new_array_from_strv:
 * @context: a #JSCContext
 * @strv: (array zero-terminated=1) (element-type utf8): a %NULL-terminated array of strings
 *
 * Create a new #JSCValue referencing an array of strings with the items from @strv. If @array
 * is %NULL or empty a new empty array will be created.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_array_from_strv(JSCContext* context, const char* const* strv)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    auto strvLength = strv ? g_strv_length(const_cast<char**>(strv)) : 0;
    if (!strvLength)
        return jsc_value_new_array(context, G_TYPE_NONE);

    GRefPtr<GPtrArray> gArray = adoptGRef(g_ptr_array_new_full(strvLength, g_object_unref));
    for (unsigned i = 0; i < strvLength; i++)
        g_ptr_array_add(gArray.get(), jsc_value_new_string(context, strv[i]));

    return jsc_value_new_array_from_garray(context, gArray.get());
}

/**
 * jsc_value_is_array:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is an array.
 *
 * Returns: whether the value is an array.
 */
gboolean jsc_value_is_array(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueIsArray(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_new_object:
 * @context: a #JSCContext
 * @instance: (nullable) (transfer full): an object instance or %NULL
 * @jsc_class: (nullable): the #JSCClass of @instance
 *
 * Create a new #JSCValue from @instance. If @instance is %NULL a new empty object is created.
 * When @instance is provided, @jsc_class must be provided too. @jsc_class takes ownership of
 * @instance that will be freed by the #GDestroyNotify passed to jsc_context_register_class().
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_object(JSCContext* context, gpointer instance, JSCClass* jscClass)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(!instance || JSC_IS_CLASS(jscClass), nullptr);

    return jscContextGetOrCreateValue(context, instance ? toRef(jscClassGetOrCreateJSWrapper(jscClass, context, instance)) : JSObjectMake(jscContextGetJSContext(context), nullptr, nullptr)).leakRef();
}

/**
 * jsc_value_is_object:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is an object.
 *
 * Returns: whether the value is an object.
 */
gboolean jsc_value_is_object(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    return JSValueIsObject(jscContextGetJSContext(priv->context.get()), priv->jsValue);
}

/**
 * jsc_value_object_is_instance_of:
 * @value: a #JSCValue
 * @name: a class name
 *
 * Get whether the value referenced by @value is an instance of class @name.
 *
 * Returns: whether the value is an object instance of class @name.
 */
gboolean jsc_value_object_is_instance_of(JSCValue* value, const char* name)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);
    g_return_val_if_fail(name, FALSE);

    JSCValuePrivate* priv = value->priv;
    // We use evaluate here and not get_value because classes are not necessarily a property of the global object.
    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-global-environment-records
    GRefPtr<JSCValue> constructor = adoptGRef(jsc_context_evaluate(priv->context.get(), name, -1));
    auto* jsContext = jscContextGetJSContext(priv->context.get());

    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, constructor->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return FALSE;

    gboolean returnValue = JSValueIsInstanceOfConstructor(jsContext, priv->jsValue, object, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return FALSE;

    return returnValue;
}

/**
 * jsc_value_object_set_property:
 * @value: a #JSCValue
 * @name: the property name
 * @property: the #JSCValue to set
 *
 * Set @property with @name on @value.
 */
void jsc_value_object_set_property(JSCValue* value, const char* name, JSCValue* property)
{
    g_return_if_fail(JSC_IS_VALUE(value));
    g_return_if_fail(name);
    g_return_if_fail(JSC_IS_VALUE(property));

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return;

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString(name));
    JSObjectSetProperty(jsContext, object, propertyName.get(), property->priv->jsValue, kJSPropertyAttributeNone, &exception);
    jscContextHandleExceptionIfNeeded(priv->context.get(), exception);
}

/**
 * jsc_value_object_get_property:
 * @value: a #JSCValue
 * @name: the property name
 *
 * Get property with @name from @value.
 *
 * Returns: (transfer full): the property #JSCValue.
 */
JSCValue* jsc_value_object_get_property(JSCValue* value, const char* name)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);
    g_return_val_if_fail(name, nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString(name));
    JSValueRef result = JSObjectGetProperty(jsContext, object, propertyName.get(), &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    return jscContextGetOrCreateValue(priv->context.get(), result).leakRef();
}

/**
 * jsc_value_object_set_property_at_index:
 * @value: a #JSCValue
 * @index: the property index
 * @property: the #JSCValue to set
 *
 * Set @property at @index on @value.
 */
void jsc_value_object_set_property_at_index(JSCValue* value, unsigned index, JSCValue* property)
{
    g_return_if_fail(JSC_IS_VALUE(value));
    g_return_if_fail(JSC_IS_VALUE(property));

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return;

    JSObjectSetPropertyAtIndex(jsContext, object, index, property->priv->jsValue, &exception);
    jscContextHandleExceptionIfNeeded(priv->context.get(), exception);
}

/**
 * jsc_value_object_get_property_at_index:
 * @value: a #JSCValue
 * @index: the property index
 *
 * Get property at @index from @value.
 *
 * Returns: (transfer full): the property #JSCValue.
 */
JSCValue* jsc_value_object_get_property_at_index(JSCValue* value, unsigned index)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    JSValueRef result = JSObjectGetPropertyAtIndex(jsContext, object, index, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    return jscContextGetOrCreateValue(priv->context.get(), result).leakRef();
}

/**
 * jsc_value_object_has_property:
 * @value: a #JSCValue
 * @name: the property name
 *
 * Get whether @value has property with @name.
 *
 * Returns: %TRUE if @value has a property with @name, or %FALSE otherwise
 */
gboolean jsc_value_object_has_property(JSCValue* value, const char* name)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);
    g_return_val_if_fail(name, FALSE);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return FALSE;

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString(name));
    return JSObjectHasProperty(jsContext, object, propertyName.get());
}

/**
 * jsc_value_object_delete_property:
 * @value: a #JSCValue
 * @name: the property name
 *
 * Try to delete property with @name from @value. This function will return %FALSE if
 * the property was defined without %JSC_VALUE_PROPERTY_CONFIGURABLE flag.
 *
 * Returns: %TRUE if the property was deleted, or %FALSE otherwise.
 */
gboolean jsc_value_object_delete_property(JSCValue* value, const char* name)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);
    g_return_val_if_fail(name, FALSE);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return FALSE;

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString(name));
    gboolean result = JSObjectDeleteProperty(jsContext, object, propertyName.get(), &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return FALSE;

    return result;
}

/**
 * jsc_value_object_enumerate_properties:
 * @value: a #JSCValue
 *
 * Get the list of property names of @value. Only properties defined with %JSC_VALUE_PROPERTY_ENUMERABLE
 * flag will be collected.
 *
 * Returns: (array zero-terminated=1) (transfer full) (nullable): a %NULL-terminated array of strings containing the
 *    property names, or %NULL if @value doesn't have enumerable properties.  Use g_strfreev() to free.
 */
char** jsc_value_object_enumerate_properties(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return nullptr;

    auto* propertiesArray = JSObjectCopyPropertyNames(jsContext, object);
    if (!propertiesArray)
        return nullptr;

    auto propertiesArraySize = JSPropertyNameArrayGetCount(propertiesArray);
    if (!propertiesArraySize) {
        JSPropertyNameArrayRelease(propertiesArray);
        return nullptr;
    }

    auto* result = static_cast<char**>(g_new0(char*, propertiesArraySize + 1));
    for (unsigned i = 0; i < propertiesArraySize; ++i) {
        RefPtr jsString = JSPropertyNameArrayGetNameAtIndex(propertiesArray, i);
        size_t maxSize = JSStringGetMaximumUTF8CStringSize(jsString.get());
        auto* string = static_cast<char*>(g_malloc(maxSize));
        JSStringGetUTF8CString(jsString.get(), string, maxSize);
        result[i] = string;
    }
    JSPropertyNameArrayRelease(propertiesArray);

    return result;
}

static JSValueRef jsObjectCall(JSGlobalContextRef jsContext, JSObjectRef function, JSC::JSCCallbackFunction::Type functionType, JSObjectRef thisObject, const Vector<JSValueRef>& arguments, JSValueRef* exception)
{
    switch (functionType) {
    case JSC::JSCCallbackFunction::Type::Constructor:
        return JSObjectCallAsConstructor(jsContext, function, arguments.size(), arguments.data(), exception);
    case JSC::JSCCallbackFunction::Type::Method:
        ASSERT(thisObject);
        FALLTHROUGH;
    case JSC::JSCCallbackFunction::Type::Function:
        return JSObjectCallAsFunction(jsContext, function, thisObject, arguments.size(), arguments.data(), exception);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static GRefPtr<JSCValue> jscValueCallFunction(JSCValue* value, JSObjectRef function, JSC::JSCCallbackFunction::Type functionType, JSObjectRef thisObject, GType firstParameterType, va_list args)
{
    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSC::JSGlobalObject* globalObject = toJS(jsContext);


    JSValueRef exception = nullptr;
    Vector<JSValueRef> arguments;
    GType parameterType = firstParameterType;
    while (parameterType != G_TYPE_NONE) {
        GValue parameter;
        GUniqueOutPtr<char> error;
        G_VALUE_COLLECT_INIT(&parameter, parameterType, args, G_VALUE_NOCOPY_CONTENTS, &error.outPtr());
        if (error) {
            exception = toRef(JSC::createTypeError(globalObject, makeString("failed to collect function paramater: "_s, span(error.get()))));
            jscContextHandleExceptionIfNeeded(priv->context.get(), exception);
            return adoptGRef(jsc_value_new_undefined(priv->context.get()));
        }

        auto* jsValue = jscContextGValueToJSValue(priv->context.get(), &parameter, &exception);
        g_value_unset(&parameter);
        if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
            return jscContextGetOrCreateValue(priv->context.get(), jsValue);

        arguments.append(jsValue);
        parameterType = va_arg(args, GType);
    }

    auto result = jsObjectCall(jsContext, function, functionType, thisObject, arguments, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return adoptGRef(jsc_value_new_undefined(priv->context.get()));

    return jscContextGetOrCreateValue(priv->context.get(), result);
}

/**
 * jsc_value_object_invoke_method: (skip)
 * @value: a #JSCValue
 * @name: the method name
 * @first_parameter_type: #GType of first parameter, or %G_TYPE_NONE
 * @...: value of the first parameter, followed optionally by more type/value pairs, followed by %G_TYPE_NONE
 *
 * Invoke method with @name on object referenced by @value, passing the given parameters. If
 * @first_parameter_type is %G_TYPE_NONE no parameters will be passed to the method.
 * The object instance will be handled automatically even when the method is a custom one
 * registered with jsc_class_add_method(), so it should never be passed explicitly as parameter
 * of this function.
 *
 * This function always returns a #JSCValue, in case of void methods a #JSCValue referencing
 * <function>undefined</function> is returned.
 *
 * Returns: (transfer full): a #JSCValue with the return value of the method.
 */
JSCValue* jsc_value_object_invoke_method(JSCValue* value, const char* name, GType firstParameterType, ...)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);
    g_return_val_if_fail(name, nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    JSRetainPtr<JSStringRef> methodName(Adopt, JSStringCreateWithUTF8CString(name));
    JSValueRef functionValue = JSObjectGetProperty(jsContext, object, methodName.get(), &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    JSObjectRef function = JSValueToObject(jsContext, functionValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    va_list args;
    va_start(args, firstParameterType);
    auto result = jscValueCallFunction(value, function, JSC::JSCCallbackFunction::Type::Method, object, firstParameterType, args);
    va_end(args);

    return result.leakRef();
}

/**
 * jsc_value_object_invoke_methodv: (rename-to jsc_value_object_invoke_method)
 * @value: a #JSCValue
 * @name: the method name
 * @n_parameters: the number of parameters
 * @parameters: (nullable) (array length=n_parameters) (element-type JSCValue): the #JSCValue<!-- -->s to pass as parameters to the method, or %NULL
 *
 * Invoke method with @name on object referenced by @value, passing the given @parameters. If
 * @n_parameters is 0 no parameters will be passed to the method.
 * The object instance will be handled automatically even when the method is a custom one
 * registered with jsc_class_add_method(), so it should never be passed explicitly as parameter
 * of this function.
 *
 * This function always returns a #JSCValue, in case of void methods a #JSCValue referencing
 * <function>undefined</function> is returned.
 *
 * Returns: (transfer full): a #JSCValue with the return value of the method.
 */
JSCValue* jsc_value_object_invoke_methodv(JSCValue* value, const char* name, unsigned parametersCount, JSCValue** parameters)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);
    g_return_val_if_fail(name, nullptr);
    g_return_val_if_fail(!parametersCount || parameters, nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    JSRetainPtr<JSStringRef> methodName(Adopt, JSStringCreateWithUTF8CString(name));
    JSValueRef functionValue = JSObjectGetProperty(jsContext, object, methodName.get(), &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    JSObjectRef function = JSValueToObject(jsContext, functionValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    Vector<JSValueRef> arguments(parametersCount, [&](size_t i) {
        return jscValueGetJSValue(parameters[i]);
    });

    auto result = jsObjectCall(jsContext, function, JSC::JSCCallbackFunction::Type::Method, object, arguments, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    return jscContextGetOrCreateValue(priv->context.get(), result).leakRef();
}

/**
 * JSCValuePropertyFlags:
 * @JSC_VALUE_PROPERTY_CONFIGURABLE: the type of the property descriptor may be changed and the
 *  property may be deleted from the corresponding object.
 * @JSC_VALUE_PROPERTY_ENUMERABLE: the property shows up during enumeration of the properties on
 *  the corresponding object.
 * @JSC_VALUE_PROPERTY_WRITABLE: the value associated with the property may be changed with an
 *  assignment operator. This doesn't have any effect when passed to jsc_value_object_define_property_accessor().
 *
 * Flags used when defining properties with jsc_value_object_define_property_data() and
 * jsc_value_object_define_property_accessor().
 */

/**
 * jsc_value_object_define_property_data:
 * @value: a #JSCValue
 * @property_name: the name of the property to define
 * @flags: #JSCValuePropertyFlags
 * @property_value: (nullable): the default property value
 *
 * Define or modify a property with @property_name in object referenced by @value. This is equivalent to
 * JavaScript <function>Object.defineProperty()</function> when used with a data descriptor.
 */
void jsc_value_object_define_property_data(JSCValue* value, const char* propertyName, JSCValuePropertyFlags flags, JSCValue* propertyValue)
{
    g_return_if_fail(JSC_IS_VALUE(value));
    g_return_if_fail(propertyName);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSC::JSGlobalObject* globalObject = toJS(jsContext);
    Ref vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSC::JSValue jsValue = toJS(globalObject, priv->jsValue);
    JSC::JSObject* object = jsValue.toObject(globalObject);
    JSValueRef exception = nullptr;
    if (handleExceptionIfNeeded(scope, jsContext, &exception) == ExceptionStatus::DidThrow) {
        jscContextHandleExceptionIfNeeded(priv->context.get(), exception);
        return;
    }

    auto name = OpaqueJSString::tryCreate(String::fromUTF8(propertyName));
    if (!name)
        return;

    JSC::PropertyDescriptor descriptor;
    descriptor.setValue(toJS(globalObject, propertyValue->priv->jsValue));
    descriptor.setEnumerable(flags & JSC_VALUE_PROPERTY_ENUMERABLE);
    descriptor.setConfigurable(flags & JSC_VALUE_PROPERTY_CONFIGURABLE);
    descriptor.setWritable(flags & JSC_VALUE_PROPERTY_WRITABLE);
    object->methodTable()->defineOwnProperty(object, globalObject, name->identifier(vm.ptr()), descriptor, true);
    if (handleExceptionIfNeeded(scope, jsContext, &exception) == ExceptionStatus::DidThrow) {
        jscContextHandleExceptionIfNeeded(priv->context.get(), exception);
        return;
    }
}

static void jscValueObjectDefinePropertyAccessor(JSCValue* value, const char* propertyName, JSCValuePropertyFlags flags, GType propertyType, JSC::JSCCallbackFunction::Type functionType, GCallback getter, GCallback setter, gpointer userData, GDestroyNotify destroyNotify)
{
    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSC::JSGlobalObject* globalObject = toJS(jsContext);
    Ref vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSC::JSValue jsValue = toJS(globalObject, priv->jsValue);
    JSC::JSObject* object = jsValue.toObject(globalObject);
    JSValueRef exception = nullptr;
    if (handleExceptionIfNeeded(scope, jsContext, &exception) == ExceptionStatus::DidThrow) {
        jscContextHandleExceptionIfNeeded(priv->context.get(), exception);
        return;
    }

    auto name = OpaqueJSString::tryCreate(String::fromUTF8(propertyName));
    if (!name)
        return;

    JSC::PropertyDescriptor descriptor;
    descriptor.setEnumerable(flags & JSC_VALUE_PROPERTY_ENUMERABLE);
    descriptor.setConfigurable(flags & JSC_VALUE_PROPERTY_CONFIGURABLE);
    if (getter) {
        GRefPtr<GClosure> closure;
        if (functionType == JSC::JSCCallbackFunction::Type::Function && userData)
            closure = adoptGRef(g_cclosure_new_swap(getter, userData, reinterpret_cast<GClosureNotify>(reinterpret_cast<GCallback>(destroyNotify))));
        else
            closure = adoptGRef(g_cclosure_new(getter, userData, reinterpret_cast<GClosureNotify>(reinterpret_cast<GCallback>(destroyNotify))));
        auto function = JSC::JSCCallbackFunction::create(vm, globalObject, "get"_s, functionType, nullptr, WTFMove(closure), propertyType, Vector<GType> { });
        descriptor.setGetter(function);
    }
    if (setter) {
        GRefPtr<GClosure> closure = adoptGRef(g_cclosure_new(setter, userData, getter ? nullptr : reinterpret_cast<GClosureNotify>(reinterpret_cast<GCallback>(destroyNotify))));
        auto function = JSC::JSCCallbackFunction::create(vm, globalObject, "set"_s, functionType, nullptr, WTFMove(closure), G_TYPE_NONE, Vector<GType> { propertyType });
        descriptor.setSetter(function);
    }
    object->methodTable()->defineOwnProperty(object, globalObject, name->identifier(vm.ptr()), descriptor, true);
    if (handleExceptionIfNeeded(scope, jsContext, &exception) == ExceptionStatus::DidThrow) {
        jscContextHandleExceptionIfNeeded(priv->context.get(), exception);
        return;
    }
}

/**
 * jsc_value_object_define_property_accessor:
 * @value: a #JSCValue
 * @property_name: the name of the property to define
 * @flags: #JSCValuePropertyFlags
 * @property_type: the #GType of the property
 * @getter: (scope async) (nullable) (closure user_data): a #GCallback to be called to get the property value
 * @setter: (scope async) (nullable) (closure user_data): a #GCallback to be called to set the property value
 * @user_data: user data to pass to @getter and @setter
 * @destroy_notify: (nullable): destroy notifier for @user_data
 *
 * Define or modify a property with @property_name in object referenced by @value. When the
 * property value needs to be getted or set, @getter and @setter callbacks will be called.
 * When the property is cleared in the #JSCClass context, @destroy_notify is called with
 * @user_data as parameter. This is equivalent to JavaScript <function>Object.defineProperty()</function>
 * when used with an accessor descriptor.
 *
 * Note that the value returned by @getter must be fully transferred. In case of boxed types, you could use
 * %G_TYPE_POINTER instead of the actual boxed #GType to ensure that the instance owned by #JSCClass is used.
 * If you really want to return a new copy of the boxed type, use #JSC_TYPE_VALUE and return a #JSCValue created
 * with jsc_value_new_object() that receives the copy as instance parameter.
 *
 * Note that @getter and @setter are called as functions and not methods, so they don't receive an instance as
 * first parameter. Use jsc_class_add_property() if you want to add property accessor invoked as a method.
 */
void jsc_value_object_define_property_accessor(JSCValue* value, const char* propertyName, JSCValuePropertyFlags flags, GType propertyType, GCallback getter, GCallback setter, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_if_fail(JSC_IS_VALUE(value));
    g_return_if_fail(propertyName);
    g_return_if_fail(propertyType != G_TYPE_INVALID && propertyType != G_TYPE_NONE);
    g_return_if_fail(getter || setter);

    jscValueObjectDefinePropertyAccessor(value, propertyName, flags, propertyType, JSC::JSCCallbackFunction::Type::Function, getter, setter, userData, destroyNotify);
}

void jscValueAddPropertyAccessor(JSCValue* value, const char* propertyName, GType propertyType, GCallback getter, GCallback setter, gpointer userData, GDestroyNotify destroyNotify)
{
    jscValueObjectDefinePropertyAccessor(value, propertyName, JSC_VALUE_PROPERTY_CONFIGURABLE, propertyType, JSC::JSCCallbackFunction::Type::Method, getter, setter, userData, destroyNotify);
}

static GRefPtr<JSCValue> jscValueFunctionCreate(JSCContext* context, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, std::optional<Vector<GType>>&& parameters)
{
    GRefPtr<GClosure> closure;
    // If the function doesn't have arguments, we need to swap the fake instance and user data to ensure
    // user data is the first parameter and fake instance ignored.
    if (parameters && parameters->isEmpty() && userData)
        closure = adoptGRef(g_cclosure_new_swap(callback, userData, reinterpret_cast<GClosureNotify>(reinterpret_cast<GCallback>(destroyNotify))));
    else
        closure = adoptGRef(g_cclosure_new(callback, userData, reinterpret_cast<GClosureNotify>(reinterpret_cast<GCallback>(destroyNotify))));
    JSC::JSGlobalObject* globalObject = toJS(jscContextGetJSContext(context));
    Ref vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto* functionObject = toRef(JSC::JSCCallbackFunction::create(vm, globalObject, name ? String::fromUTF8(name) : "anonymous"_s,
        JSC::JSCCallbackFunction::Type::Function, nullptr, WTFMove(closure), returnType, WTFMove(parameters)));
    return jscContextGetOrCreateValue(context, functionObject);
}

/**
 * jsc_value_new_function: (skip)
 * @context: a #JSCContext:
 * @name: (nullable): the function name or %NULL
 * @callback: (scope async): a #GCallback.
 * @user_data: user data to pass to @callback.
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the function return value, or %G_TYPE_NONE if the function is void.
 * @n_params: the number of parameter types to follow or 0 if the function doesn't receive parameters.
 * @...: a list of #GType<!-- -->s, one for each parameter.
 *
 * Create a function in @context. If @name is %NULL an anonymous function will be created.
 * When the function is called by JavaScript or jsc_value_function_call(), @callback is called
 * receiving the function parameters and then @user_data as last parameter. When the function is
 * cleared in @context, @destroy_notify is called with @user_data as parameter.
 *
 * Note that the value returned by @callback must be fully transferred. In case of boxed types, you could use
 * %G_TYPE_POINTER instead of the actual boxed #GType to ensure that the instance owned by #JSCClass is used.
 * If you really want to return a new copy of the boxed type, use #JSC_TYPE_VALUE and return a #JSCValue created
 * with jsc_value_new_object() that receives the copy as instance parameter.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_function(JSCContext* context, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, unsigned paramCount, ...)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(callback, nullptr);

    va_list args;
    va_start(args, paramCount);
    Vector<GType> parameters(paramCount, [&](size_t) -> GType {
        return va_arg(args, GType);
    });
    va_end(args);

    return jscValueFunctionCreate(context, name, callback, userData, destroyNotify, returnType, WTFMove(parameters)).leakRef();
}

/**
 * jsc_value_new_functionv: (rename-to jsc_value_new_function)
 * @context: a #JSCContext
 * @name: (nullable): the function name or %NULL
 * @callback: (scope async): a #GCallback.
 * @user_data: user data to pass to @callback.
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the function return value, or %G_TYPE_NONE if the function is void.
 * @n_parameters: the number of parameters
 * @parameter_types: (nullable) (array length=n_parameters) (element-type GType): a list of #GType<!-- -->s, one for each parameter, or %NULL
 *
 * Create a function in @context. If @name is %NULL an anonymous function will be created.
 * When the function is called by JavaScript or jsc_value_function_call(), @callback is called
 * receiving the function parameters and then @user_data as last parameter. When the function is
 * cleared in @context, @destroy_notify is called with @user_data as parameter.
 *
 * Note that the value returned by @callback must be fully transferred. In case of boxed types, you could use
 * %G_TYPE_POINTER instead of the actual boxed #GType to ensure that the instance owned by #JSCClass is used.
 * If you really want to return a new copy of the boxed type, use #JSC_TYPE_VALUE and return a #JSCValue created
 * with jsc_value_new_object() that receives the copy as instance parameter.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_functionv(JSCContext* context, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, unsigned parametersCount, GType *parameterTypes)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(callback, nullptr);
    g_return_val_if_fail(!parametersCount || parameterTypes, nullptr);

    Vector<GType> parameters(parametersCount, [&](size_t i) -> GType {
        return parameterTypes[i];
    });

    return jscValueFunctionCreate(context, name, callback, userData, destroyNotify, returnType, WTFMove(parameters)).leakRef();
}

/**
 * jsc_value_new_function_variadic:
 * @context: a #JSCContext
 * @name: (nullable): the function name or %NULL
 * @callback: (scope async): a #GCallback.
 * @user_data: user data to pass to @callback.
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the function return value, or %G_TYPE_NONE if the function is void.
 *
 * Create a function in @context. If @name is %NULL an anonymous function will be created.
 * When the function is called by JavaScript or jsc_value_function_call(), @callback is called
 * receiving an #GPtrArray of #JSCValue<!-- -->s with the arguments and then @user_data as last parameter.
 * When the function is cleared in @context, @destroy_notify is called with @user_data as parameter.
 *
 * Note that the value returned by @callback must be fully transferred. In case of boxed types, you could use
 * %G_TYPE_POINTER instead of the actual boxed #GType to ensure that the instance owned by #JSCClass is used.
 * If you really want to return a new copy of the boxed type, use #JSC_TYPE_VALUE and return a #JSCValue created
 * with jsc_value_new_object() that receives the copy as instance parameter.
 *
 * Returns: (transfer full): a #JSCValue.
 */
JSCValue* jsc_value_new_function_variadic(JSCContext* context, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(callback, nullptr);

    return jscValueFunctionCreate(context, name, callback, userData, destroyNotify, returnType, std::nullopt).leakRef();
}

/**
 * jsc_value_is_function:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is a function
 *
 * Returns: whether the value is a function.
 */
gboolean jsc_value_is_function(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    return !exception ? JSObjectIsFunction(jsContext, object) : FALSE;
}

/**
 * jsc_value_function_call: (skip)
 * @value: a #JSCValue
 * @first_parameter_type: #GType of first parameter, or %G_TYPE_NONE
 * @...: value of the first parameter, followed optionally by more type/value pairs, followed by %G_TYPE_NONE
 *
 * Call function referenced by @value, passing the given parameters. If @first_parameter_type
 * is %G_TYPE_NONE no parameters will be passed to the function.
 *
 * This function always returns a #JSCValue, in case of void functions a #JSCValue referencing
 * <function>undefined</function> is returned
 *
 * Returns: (transfer full): a #JSCValue with the return value of the function.
 */
JSCValue* jsc_value_function_call(JSCValue* value, GType firstParameterType, ...)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef function = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    va_list args;
    va_start(args, firstParameterType);
    auto result = jscValueCallFunction(value, function, JSC::JSCCallbackFunction::Type::Function, nullptr, firstParameterType, args);
    va_end(args);

    return result.leakRef();
}

/**
 * jsc_value_function_callv: (rename-to jsc_value_function_call)
 * @value: a #JSCValue
 * @n_parameters: the number of parameters
 * @parameters: (nullable) (array length=n_parameters) (element-type JSCValue): the #JSCValue<!-- -->s to pass as parameters to the function, or %NULL
 *
 * Call function referenced by @value, passing the given @parameters. If @n_parameters
 * is 0 no parameters will be passed to the function.
 *
 * This function always returns a #JSCValue, in case of void functions a #JSCValue referencing
 * <function>undefined</function> is returned
 *
 * Returns: (transfer full): a #JSCValue with the return value of the function.
 */
JSCValue* jsc_value_function_callv(JSCValue* value, unsigned parametersCount, JSCValue** parameters)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);
    g_return_val_if_fail(!parametersCount || parameters, nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef function = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    Vector<JSValueRef> arguments(parametersCount, [&](size_t i) {
        return jscValueGetJSValue(parameters[i]);
    });

    auto result = jsObjectCall(jsContext, function, JSC::JSCCallbackFunction::Type::Function, nullptr, arguments, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    return jscContextGetOrCreateValue(priv->context.get(), result).leakRef();
}

/**
 * jsc_value_is_constructor:
 * @value: a #JSCValue
 *
 * Get whether the value referenced by @value is a constructor.
 *
 * Returns: whether the value is a constructor.
 */
gboolean jsc_value_is_constructor(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(jsContext, priv->jsValue, &exception);
    return !exception ? JSObjectIsConstructor(jsContext, object) : FALSE;
}

/**
 * jsc_value_constructor_call: (skip)
 * @value: a #JSCValue
 * @first_parameter_type: #GType of first parameter, or %G_TYPE_NONE
 * @...: value of the first parameter, followed optionally by more type/value pairs, followed by %G_TYPE_NONE
 *
 * Invoke <function>new</function> with constructor referenced by @value. If @first_parameter_type
 * is %G_TYPE_NONE no parameters will be passed to the constructor.
 *
 * Returns: (transfer full): a #JSCValue referencing the newly created object instance.
 */
JSCValue* jsc_value_constructor_call(JSCValue* value, GType firstParameterType, ...)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef function = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    va_list args;
    va_start(args, firstParameterType);
    auto result = jscValueCallFunction(value, function, JSC::JSCCallbackFunction::Type::Constructor, nullptr, firstParameterType, args);
    va_end(args);

    return result.leakRef();
}

/**
 * jsc_value_constructor_callv: (rename-to jsc_value_constructor_call)
 * @value: a #JSCValue
 * @n_parameters: the number of parameters
 * @parameters: (nullable) (array length=n_parameters) (element-type JSCValue): the #JSCValue<!-- -->s to pass as parameters to the constructor, or %NULL
 *
 * Invoke <function>new</function> with constructor referenced by @value. If @n_parameters
 * is 0 no parameters will be passed to the constructor.
 *
 * Returns: (transfer full): a #JSCValue referencing the newly created object instance.
 */
JSCValue* jsc_value_constructor_callv(JSCValue* value, unsigned parametersCount, JSCValue** parameters)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);
    g_return_val_if_fail(!parametersCount || parameters, nullptr);

    JSCValuePrivate* priv = value->priv;
    auto* jsContext = jscContextGetJSContext(priv->context.get());
    JSValueRef exception = nullptr;
    JSObjectRef function = JSValueToObject(jsContext, priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    Vector<JSValueRef> arguments(parametersCount, [&](size_t i) {
        return jscValueGetJSValue(parameters[i]);
    });

    auto result = jsObjectCall(jsContext, function, JSC::JSCCallbackFunction::Type::Constructor, nullptr, arguments, &exception);
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return jsc_value_new_undefined(priv->context.get());

    return jscContextGetOrCreateValue(priv->context.get(), result).leakRef();
}

struct ArrayBufferDeallocatorContext {
    gpointer userData;
    GDestroyNotify destroyNotify;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ArrayBufferDeallocatorContext)

/**
 * jsc_value_new_array_buffer:
 * @context: A #JSCContext
 * @data: Pointer to a region of memory.
 * @size: Size in bytes of the memory region.
 * @destroy_notify: (nullable) (closure user_data): destroy notifier for @user_data.
 * @user_data: user data.
 *
 * Creates a new %ArrayBuffer from existing @data in memory.
 *
 * The @data is not copied: while this allows sharing data with JavaScript
 * efficiently, the caller must ensure that the memory region remains valid
 * until the newly created object is released by JSC.
 *
 * Optionally, a @destroy_notify callback can be provided, which will be
 * invoked with @user_data as parameter when the %ArrayBuffer object is
 * released. This is intended to be used for freeing resources related to
 * the memory region which contains the data:
 *
 * |[!<-- language="C" -->
 * GMappedFile *f = g_mapped_file_new (file_path, TRUE, NULL);
 * JSCValue *value = jsc_value_new_array_buffer (context,
 *     g_mapped_file_get_contents (f), g_mapped_file_get_length (f),
 *     (GDestroyNotify) g_mapped_file_unref, f);
 * ]|
 *
 * Note that the @user_data can be the same value as @data:
 *
 * |[!<-- language="C" -->
 * void *bytes = g_malloc0 (100);
 * JSCValue *value = jsc_value_new_array_buffer (context, bytes, 100, g_free, bytes);
 * ]|
 *
 * Returns: (transfer full) (nullable): A #JSCValue, or %NULL in case of exception.
 *
 * Since: 2.38
 */
JSCValue* jsc_value_new_array_buffer(JSCContext* context, void* data, size_t length, GDestroyNotify destroyNotify, gpointer userData)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    ArrayBufferDeallocatorContext* deallocatorContext = nullptr;
    if (destroyNotify) {
        deallocatorContext = createArrayBufferDeallocatorContext();
        deallocatorContext->destroyNotify = destroyNotify;
        deallocatorContext->userData = userData;
    }

    JSValueRef exception = nullptr;
    auto* jsContext = jscContextGetJSContext(context);
    auto* jsArrayBuffer = JSObjectMakeArrayBufferWithBytesNoCopy(jsContext, data, length, [](void*, void* deallocatorContext) {
        if (deallocatorContext) {
            auto* context = static_cast<ArrayBufferDeallocatorContext*>(deallocatorContext);
            context->destroyNotify(context->userData);
            destroyArrayBufferDeallocatorContext(context);
        }
    }, deallocatorContext, &exception);

    if (jscContextHandleExceptionIfNeeded(context, exception))
        return nullptr;

    return jscContextGetOrCreateValue(context, jsArrayBuffer).leakRef();
}

/**
 * jsc_value_is_array_buffer:
 * @value: A #JSCValue.
 *
 * Check whether the @value is an %ArrayBuffer.
 *
 * Returns: whether the value is an %ArrayBuffer
 *
 * Since: 2.38
 */
gboolean jsc_value_is_array_buffer(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    using namespace JSC;

    JSGlobalObject* globalObject = toJS(jscContextGetJSContext(value->priv->context.get()));
    Ref vm = globalObject->vm();
    JSLockHolder locker(vm);

    JSValue jsValue = toJS(globalObject, value->priv->jsValue);
    if (!jsValue.isObject())
        return FALSE;

    return !!jsDynamicCast<JSArrayBuffer*>(jsValue.getObject());
}

/**
 * jsc_value_array_buffer_get_data:
 * @value: A #JSCValue
 * @size: (nullable): location where to store the size of the memory region.
 *
 * Gets a pointer to memory that contains the array buffer data.
 *
 * Obtains a pointer to the memory region that holds the contents of the
 * %ArrayBuffer; modifications done to the data will be visible to JavaScript
 * code. If @size is not %NULL, the size in bytes of the memory region
 * will also be stored in the pointed location.
 *
 * Note that the pointer returned by this function is not guaranteed to remain
 * the same after calls to other JSC API functions. If you plan to access the
 * data of the %ArrayBuffer later, you can keep a reference to the @value and
 * obtain the data pointer at a later point. Keep in mind that if JavaScript
 * code has a chance to run, for example due to main loop events that result
 * in JSC being called, the contents of the memory region might be modified in
 * the meantime. Consider taking a copy of the data and using the copy instead
 * in asynchronous code.
 *
 * Returns: (transfer none): pointer to memory.
 *
 * Since: 2.38
 */
gpointer jsc_value_array_buffer_get_data(JSCValue* value, gsize* size)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    auto* jsContext = jscContextGetJSContext(value->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, value->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return nullptr;

    void* data = JSObjectGetArrayBufferBytesPtr(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return nullptr;

    if (size) {
        *size = JSObjectGetArrayBufferByteLength(jsContext, jsObject, &exception);
        if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
            return nullptr;
    }

    return data;
}

/**
 * jsc_value_array_buffer_get_size:
 * @value: A #JSCValue
 *
 * Gets the size in bytes of the array buffer.
 *
 * Obtains the size in bytes of the memory region that holds the contents of
 * an %ArrayBuffer.
 *
 * Returns: size, in bytes.
 *
 * Since: 2.38
 */
gsize jsc_value_array_buffer_get_size(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), 0);

    auto* jsContext = jscContextGetJSContext(value->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, value->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    size_t size = JSObjectGetArrayBufferByteLength(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    return size;
}

/**
 * JSCTypedArrayType:
 * @JSC_TYPED_ARRAY_NONE: Not a typed array, or type unsupported.
 * @JSC_TYPED_ARRAY_INT8: Array elements are 8-bit signed integers (int8_t).
 * @JSC_TYPED_ARRAY_INT16: Array elements are 16-bit signed integers (int16_t).
 * @JSC_TYPED_ARRAY_INT32: Array elements are 32-bit signed integers (int32_t).
 * @JSC_TYPED_ARRAY_INT64: Array elements are 64-bit signed integers (int64_t).
 * @JSC_TYPED_ARRAY_UINT8: Array elements are 8-bit unsigned integers (uint8_t).
 * @JSC_TYPED_ARRAY_UINT8_CLAMPED: Array elements are 8-bit unsigned integers (uint8_t).
 * @JSC_TYPED_ARRAY_UINT16: Array elements are 16-bit unsigned integers (uint16_t).
 * @JSC_TYPED_ARRAY_UINT32: Array elements are 32-bit unsigned integers (uint32_t).
 * @JSC_TYPED_ARRAY_UINT64: Array elements are 64-bit unsigned integers (uint64_t).
 * @JSC_TYPED_ARRAY_FLOAT32: Array elements are 32-bit floating point numbers (float).
 * @JSC_TYPED_ARRAY_FLOAT64: Array elements are 64-bit floating point numbers (double).
 *
 * Possible types of the elements contained in a typed array.
 *
 * Since: 2.38
 */

static JSTypedArrayType toTypedArrayType(JSCTypedArrayType type)
{
    switch (type) {
    case JSC_TYPED_ARRAY_NONE:
        return kJSTypedArrayTypeNone;
    case JSC_TYPED_ARRAY_INT8:
        return kJSTypedArrayTypeInt8Array;
    case JSC_TYPED_ARRAY_INT16:
        return kJSTypedArrayTypeInt16Array;
    case JSC_TYPED_ARRAY_INT32:
        return kJSTypedArrayTypeInt32Array;
    case JSC_TYPED_ARRAY_INT64:
        return kJSTypedArrayTypeBigInt64Array;
    case JSC_TYPED_ARRAY_UINT8:
        return kJSTypedArrayTypeUint8Array;
    case JSC_TYPED_ARRAY_UINT8_CLAMPED:
        return kJSTypedArrayTypeUint8ClampedArray;
    case JSC_TYPED_ARRAY_UINT16:
        return kJSTypedArrayTypeUint16Array;
    case JSC_TYPED_ARRAY_UINT32:
        return kJSTypedArrayTypeUint32Array;
    case JSC_TYPED_ARRAY_UINT64:
        return kJSTypedArrayTypeBigUint64Array;
    case JSC_TYPED_ARRAY_FLOAT32:
        return kJSTypedArrayTypeFloat32Array;
    case JSC_TYPED_ARRAY_FLOAT64:
        return kJSTypedArrayTypeFloat64Array;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static JSC::TypedArrayType toJSCTypedArrayType(JSCTypedArrayType type)
{
    switch (type) {
    case JSC_TYPED_ARRAY_NONE:
        return JSC::TypedArrayType::NotTypedArray;
    case JSC_TYPED_ARRAY_INT8:
        return JSC::TypedArrayType::TypeInt8;
    case JSC_TYPED_ARRAY_INT16:
        return JSC::TypedArrayType::TypeInt16;
    case JSC_TYPED_ARRAY_INT32:
        return JSC::TypedArrayType::TypeInt32;
    case JSC_TYPED_ARRAY_INT64:
        return JSC::TypedArrayType::TypeBigInt64;
    case JSC_TYPED_ARRAY_UINT8:
        return JSC::TypedArrayType::TypeUint8;
    case JSC_TYPED_ARRAY_UINT8_CLAMPED:
        return JSC::TypedArrayType::TypeUint8Clamped;
    case JSC_TYPED_ARRAY_UINT16:
        return JSC::TypedArrayType::TypeUint16;
    case JSC_TYPED_ARRAY_UINT32:
        return JSC::TypedArrayType::TypeUint32;
    case JSC_TYPED_ARRAY_UINT64:
        return JSC::TypedArrayType::TypeBigUint64;
    case JSC_TYPED_ARRAY_FLOAT32:
        return JSC::TypedArrayType::TypeFloat32;
    case JSC_TYPED_ARRAY_FLOAT64:
        return JSC::TypedArrayType::TypeFloat64;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

/**
 * jsc_value_new_typed_array:
 * @context: a #JSCContext
 * @type: the type of array elements
 * @length: number of elements in the array
 *
 * Create a new typed array containing a given amount of elements.
 *
 * Create a #JSCValue referencing a new typed array with space for @length
 * elements of a given @type. As all typed arrays must have an associated
 * `ArrayBuffer`, a new one of suitable size will be allocated to store
 * the elements, which will be initialized to zero.
 *
 * The @type must *not* be %JSC_TYPED_ARRAY_NONE.
 *
 * Returns: (transfer full): a #JSCValue
 *
 * Since: 2.38
 */
JSCValue* jsc_value_new_typed_array(JSCContext* context, JSCTypedArrayType type, gsize count)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);
    g_return_val_if_fail(type != JSC_TYPED_ARRAY_NONE, nullptr);

    auto* jsContext = jscContextGetJSContext(context);

    JSValueRef exception = nullptr;
    auto* jsTypedArray = JSObjectMakeTypedArray(jsContext, toTypedArrayType(type), count, &exception);
    if (jscContextHandleExceptionIfNeeded(context, exception))
        return nullptr;

    return jscContextGetOrCreateValue(context, jsTypedArray).leakRef();
}

/**
 * jsc_value_new_typed_array_with_buffer:
 * @array_buffer: a #JSCValue.
 * @type: type of array elements.
 * @offset: offset, in bytes.
 * @length: number of array elements, or `-1`.
 *
 * Create a new typed array value with elements from an array buffer.
 *
 * Create a #JSCValue referencing a new typed array value containing
 * elements of the given @type, where the elements are stored at the memory
 * region represented by the @array_buffer.
 *
 * The @type must *not* be %JSC_TYPED_ARRAY_NONE.
 *
 * The @offset and @length parameters can be used to indicate which part of
 * the array buffer can be accessed through the typed array. If both are
 * omitted (passing zero as @offset, and `-1` as @length), the whole
 * @array_buffer is exposed through the typed array. Omitting the @length
 * with a non-zero @offset will expose the remainder of the @array_buffer
 * starting at the indicated offset.
 *
 * Returns: (transfer full): a #JSCValue
 *
 * Since: 2.38
 */
JSCValue* jsc_value_new_typed_array_with_buffer(JSCValue* arrayBuffer, JSCTypedArrayType type, gsize offset, gssize length)
{
    g_return_val_if_fail(JSC_IS_VALUE(arrayBuffer), nullptr);
    g_return_val_if_fail(jsc_value_is_array_buffer(arrayBuffer), nullptr);
    g_return_val_if_fail(type != JSC_TYPED_ARRAY_NONE, nullptr);
    g_return_val_if_fail(length >= -1, nullptr);

    auto* jsContext = jscContextGetJSContext(arrayBuffer->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, arrayBuffer->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(arrayBuffer->priv->context.get(), exception))
        return nullptr;

    if (length < 0) {
        const auto bufferByteSize = JSObjectGetArrayBufferByteLength(jsContext, jsObject, &exception);
        if (jscContextHandleExceptionIfNeeded(arrayBuffer->priv->context.get(), exception))
            return nullptr;

        const auto remainingBytes = bufferByteSize - offset;
        const auto elementByteSize = JSC::elementSize(toJSCTypedArrayType(type));
        if (remainingBytes >= elementByteSize)
            length = remainingBytes / elementByteSize;
        else
            length = 0;
    }

    auto* jsTypedArray = JSObjectMakeTypedArrayWithArrayBufferAndOffset(jsContext, toTypedArrayType(type), jsObject, offset, length, &exception);
    if (jscContextHandleExceptionIfNeeded(arrayBuffer->priv->context.get(), exception))
        return nullptr;

    return jscContextGetOrCreateValue(arrayBuffer->priv->context.get(), jsTypedArray).leakRef();
}

/**
 * jsc_value_is_typed_array:
 * @value: a #JSCValue
 *
 * Determines whether a value is a typed array.
 *
 * Returns: Whether @value is a typed array.
 *
 * Since: 2.38
 */
gboolean jsc_value_is_typed_array(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), FALSE);

    return jsc_value_typed_array_get_type(value) != JSC_TYPED_ARRAY_NONE;
}

/**
 * jsc_value_typed_array_get_type:
 * @value: a #JSCValue
 *
 * Gets the type of elements contained in a typed array.
 *
 * Returns: type of the elements, or %JSC_TYPED_ARRAY_NONE if @value is not a typed array.
 *
 * Since: 2.38
 */
JSCTypedArrayType jsc_value_typed_array_get_type(JSCValue *value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), JSC_TYPED_ARRAY_NONE);

    JSC::JSGlobalObject* globalObject = toJS(jscContextGetJSContext(value->priv->context.get()));
    Ref vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    JSC::JSValue jsValue = toJS(globalObject, value->priv->jsValue);
    if (!jsValue.isObject())
        return JSC_TYPED_ARRAY_NONE;

    switch (jsValue.getObject()->type()) {
    case JSC::Int8ArrayType:
        return JSC_TYPED_ARRAY_INT8;
    case JSC::Uint8ArrayType:
        return JSC_TYPED_ARRAY_UINT8;
    case JSC::Uint8ClampedArrayType:
        return JSC_TYPED_ARRAY_UINT8_CLAMPED;
    case JSC::Int16ArrayType:
        return JSC_TYPED_ARRAY_INT16;
    case JSC::Uint16ArrayType:
        return JSC_TYPED_ARRAY_UINT16;
    case JSC::Int32ArrayType:
        return JSC_TYPED_ARRAY_INT32;
    case JSC::Uint32ArrayType:
        return JSC_TYPED_ARRAY_UINT32;
    case JSC::Float32ArrayType:
        return JSC_TYPED_ARRAY_FLOAT32;
    case JSC::Float64ArrayType:
        return JSC_TYPED_ARRAY_FLOAT64;
    case JSC::BigInt64ArrayType:
        return JSC_TYPED_ARRAY_INT64;
    case JSC::BigUint64ArrayType:
        return JSC_TYPED_ARRAY_UINT64;
    default:
        return JSC_TYPED_ARRAY_NONE;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

/**
 * jsc_value_typed_array_get_data:
 * @value: a #JSCValue
 * @length: (nullable) (out): location to return the number of elements contained
 *
 * Obtains a pointer to the memory region that holds the elements of the typed
 * array; modifications done to them will be visible to JavaScript code. If
 * @length is not %NULL, the number of elements contained in the typed array
 * are also stored in the pointed location.
 *
 * The returned pointer needs to be casted to the appropriate type (see
 * #JSCTypedArrayType), and has the `offset` over the underlying array
 * buffer data applied—that is, points to the first element of the typed
 * array:
 *
 * |[<!-- language="C" -->
 * if (jsc_value_typed_array_get_type(value) != JSC_TYPED_ARRAY_UINT32)
 *     g_error ("Only arrays of uint32_t are supported");
 *
 * gsize count = 0;
 * uint32_t *elements = jsc_value_typed_array_get_contents (value, &count);
 * for (gsize i = 0; i < count; i++)
 *      g_print ("index %zu, value %" PRIu32 "\n", i, elements[i]);
 * ]|
 *
 * Note that the pointer returned by this function is not guaranteed to remain
 * the same after calls to other JSC API functions. See
 * jsc_value_array_buffer_get_data() for details.
 *
 * Returns: (transfer none): pointer to memory.
 *
 * Since: 2.38
 */
gpointer jsc_value_typed_array_get_data(JSCValue* value, gsize* length)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    auto* jsContext = jscContextGetJSContext(value->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, value->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return nullptr;

    auto* bytes = JSObjectGetTypedArrayBytesPtr(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return nullptr;

    // Pointer may have an offset, which we want to return directly a pointer to the actual data.
    const auto offset = JSObjectGetTypedArrayByteOffset(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return nullptr;

    if (length) {
        const auto numElements = JSObjectGetTypedArrayLength(jsContext, jsObject, &exception);
        if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
            return nullptr;
        *length = numElements;
    }

    return static_cast<uint8_t*>(bytes) + offset;
}

/**
 * jsc_value_typed_array_get_length:
 * @value: a #JSCValue
 *
 * Gets the number of elements in a typed array.
 *
 * Returns: number of elements.
 *
 * Since: 2.38
 */
gsize jsc_value_typed_array_get_length(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), 0);

    auto* jsContext = jscContextGetJSContext(value->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, value->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    const auto length = JSObjectGetTypedArrayLength(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    return length;
}

/**
 * jsc_value_typed_array_get_size:
 * @value: a #JSCValue
 *
 * Gets the size of a typed array.
 *
 * Returns: size, in bytes.
 *
 * Since: 2.38
 */
gsize jsc_value_typed_array_get_size(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), 0);

    auto* jsContext = jscContextGetJSContext(value->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, value->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    const auto size = JSObjectGetTypedArrayByteLength(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    return size;
}

/**
 * jsc_value_typed_array_get_offset:
 * @value: A #JSCValue
 *
 * Gets the offset over the underlying array buffer data.
 *
 * Returns: offset, in bytes.
 *
 * Since: 2.38
 */
gsize jsc_value_typed_array_get_offset(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), 0);

    auto* jsContext = jscContextGetJSContext(value->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, value->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    const auto offset = JSObjectGetTypedArrayByteOffset(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return 0;

    return offset;
}

/**
 * jsc_value_typed_array_get_buffer:
 * @value: a #JSCValue
 *
 * Obtain the %ArrayBuffer for the memory region of the typed array elements.
 *
 * Returns: (transfer full): A #JSCValue
 *
 * Since: 2.38
 */
JSCValue* jsc_value_typed_array_get_buffer(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    auto* jsContext = jscContextGetJSContext(value->priv->context.get());

    JSValueRef exception = nullptr;
    auto* jsObject = JSValueToObject(jsContext, value->priv->jsValue, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return nullptr;

    auto* jsArrayBuffer = JSObjectGetTypedArrayBuffer(jsContext, jsObject, &exception);
    if (jscContextHandleExceptionIfNeeded(value->priv->context.get(), exception))
        return nullptr;

    return jscContextGetOrCreateValue(value->priv->context.get(), jsArrayBuffer).leakRef();
}

/**
 * jsc_value_new_from_json:
 * @context: a #JSCContext
 * @json: the JSON string to be parsed
 *
 * Create a new #JSCValue referencing a new value created by parsing @json.
 *
 * Returns: (transfer full): a #JSCValue.
 *
 * Since: 2.28
 */
JSCValue* jsc_value_new_from_json(JSCContext* context, const char* json)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    if (!json)
        return jsc_value_new_null(context);

    auto* jsContext = jscContextGetJSContext(context);
    JSC::JSGlobalObject* globalObject = toJS(jsContext);


    JSValueRef exception = nullptr;
    JSC::JSValue jsValue;
    String jsonString = String::fromUTF8(json);
    if (jsonString.is8Bit()) {
        JSC::LiteralParser jsonParser(globalObject, jsonString.span8(), JSC::StrictJSON);
        jsValue = jsonParser.tryLiteralParse();
        if (!jsValue)
            exception = toRef(JSC::createSyntaxError(globalObject, jsonParser.getErrorMessage()));
    } else {
        JSC::LiteralParser jsonParser(globalObject, jsonString.span16(), JSC::StrictJSON);
        jsValue = jsonParser.tryLiteralParse();
        if (!jsValue)
            exception = toRef(JSC::createSyntaxError(globalObject, jsonParser.getErrorMessage()));
    }

    if (exception) {
        jscContextHandleExceptionIfNeeded(context, exception);
        return nullptr;
    }

    return jsValue ? jscContextGetOrCreateValue(context, toRef(globalObject, jsValue)).leakRef() : nullptr;
}

/**
 * jsc_value_to_json:
 * @value: a #JSCValue
 * @indent: The number of spaces to indent when nesting.
 *
 * Create a JSON string of @value serialization. If @indent is 0, the resulting JSON will
 * not contain newlines. The size of the indent is clamped to 10 spaces.
 *
 * Returns: (transfer full): a null-terminated JSON string with serialization of @value
 *
 * Since: 2.28
 */
char* jsc_value_to_json(JSCValue* value, unsigned indent)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    JSCValuePrivate* priv = value->priv;
    JSValueRef exception = nullptr;
    JSRetainPtr<JSStringRef> jsJSON(Adopt, JSValueCreateJSONString(jscContextGetJSContext(priv->context.get()), priv->jsValue, indent, &exception));
    if (jscContextHandleExceptionIfNeeded(priv->context.get(), exception))
        return nullptr;

    if (!jsJSON)
        return nullptr;

    size_t maxSize = JSStringGetMaximumUTF8CStringSize(jsJSON.get());
    auto* json = static_cast<char*>(g_malloc(maxSize));
    if (!JSStringGetUTF8CString(jsJSON.get(), json, maxSize)) {
        g_free(json);
        return nullptr;
    }

    return json;
}
