/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitJavascriptResult.h"

#include "APISerializedScriptValue.h"
#include "WebKitJavascriptResultPrivate.h"
#include <jsc/JSCContextPrivate.h>
#include <jsc/JSCValuePrivate.h>

/**
 * WebKitJavascriptResult: (ref-func webkit_javascript_result_ref) (unref-func webkit_javascript_result_unref)
 *
 * Result of JavaScript evaluation in a web view.
 */

struct _WebKitJavascriptResult {
    explicit _WebKitJavascriptResult(WebCore::SerializedScriptValue& serializedScriptValue)
    {
        jsValue = API::SerializedScriptValue::deserialize(serializedScriptValue);
    }

    GRefPtr<JSCValue> jsValue;

    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WebKitJavascriptResult, webkit_javascript_result, webkit_javascript_result_ref, webkit_javascript_result_unref)

WebKitJavascriptResult* webkitJavascriptResultCreate(WebCore::SerializedScriptValue& serializedScriptValue)
{
    WebKitJavascriptResult* result = static_cast<WebKitJavascriptResult*>(fastMalloc(sizeof(WebKitJavascriptResult)));
    new (result) WebKitJavascriptResult(serializedScriptValue);
    return result;
}

/**
 * webkit_javascript_result_ref:
 * @js_result: a #WebKitJavascriptResult
 *
 * Atomically increments the reference count of @js_result by one.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed in #WebKitJavascriptResult
 */
WebKitJavascriptResult* webkit_javascript_result_ref(WebKitJavascriptResult* javascriptResult)
{
    g_atomic_int_inc(&javascriptResult->referenceCount);
    return javascriptResult;
}

/**
 * webkit_javascript_result_unref:
 * @js_result: a #WebKitJavascriptResult
 *
 * Atomically decrements the reference count of @js_result by one.
 *
 * If the reference count drops to 0,
 * all memory allocated by the #WebKitJavascriptResult is
 * released. This function is MT-safe and may be called from any
 * thread.
 */
void webkit_javascript_result_unref(WebKitJavascriptResult* javascriptResult)
{
    if (g_atomic_int_dec_and_test(&javascriptResult->referenceCount)) {
        javascriptResult->~WebKitJavascriptResult();
        fastFree(javascriptResult);
    }
}

#if PLATFORM(GTK)
/**
 * webkit_javascript_result_get_global_context: (skip)
 * @js_result: a #WebKitJavascriptResult
 *
 * Get the global Javascript context.
 *
 * Get the global Javascript context that should be used with the
 * <function>JSValueRef</function> returned by webkit_javascript_result_get_value().
 *
 * Returns: the <function>JSGlobalContextRef</function> for the #WebKitJavascriptResult
 *
 * Deprecated: 2.22: Use jsc_value_get_context() instead.
 */
JSGlobalContextRef webkit_javascript_result_get_global_context(WebKitJavascriptResult* javascriptResult)
{
    g_return_val_if_fail(javascriptResult, nullptr);
    return jscContextGetJSContext(jsc_value_get_context(javascriptResult->jsValue.get()));
}

/**
 * webkit_javascript_result_get_value: (skip)
 * @js_result: a #WebKitJavascriptResult
 *
 * Get the value of @js_result.
 *
 * You should use the <function>JSGlobalContextRef</function>
 * returned by webkit_javascript_result_get_global_context() to use the <function>JSValueRef</function>.
 *
 * Returns: the <function>JSValueRef</function> of the #WebKitJavascriptResult
 *
 * Deprecated: 2.22: Use webkit_javascript_result_get_js_value() instead.
 */
JSValueRef webkit_javascript_result_get_value(WebKitJavascriptResult* javascriptResult)
{
    g_return_val_if_fail(javascriptResult, nullptr);
    return jscValueGetJSValue(javascriptResult->jsValue.get());
}
#endif

/**
 * webkit_javascript_result_get_js_value:
 * @js_result: a #WebKitJavascriptResult
 *
 * Get the #JSCValue of @js_result.
 *
 * Returns: (transfer none): the #JSCValue of the #WebKitJavascriptResult
 *
 * Since: 2.22
 */
JSCValue* webkit_javascript_result_get_js_value(WebKitJavascriptResult* javascriptResult)
{
    g_return_val_if_fail(javascriptResult, nullptr);
    return javascriptResult->jsValue.get();
}
