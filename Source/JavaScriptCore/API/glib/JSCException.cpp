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
#include "JSCException.h"

#include "APICast.h"
#include "JSCContextPrivate.h"
#include "JSCExceptionPrivate.h"
#include "JSCInlines.h"
#include "JSRetainPtr.h"
#include "StrongInlines.h"
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * SECTION: JSCException
 * @short_description: JavaScript exception
 * @title: JSCException
 * @see_also: JSCContext
 *
 * JSCException represents a JavaScript exception.
 */

struct _JSCExceptionPrivate {
    JSCContext* context;
    JSC::Strong<JSC::JSObject> jsException;
    bool cached;
    GUniquePtr<char> message;
    unsigned lineNumber;
    GUniquePtr<char> sourceURI;
};

WEBKIT_DEFINE_TYPE(JSCException, jsc_exception, G_TYPE_OBJECT)

static void jscExceptionDispose(GObject* object)
{
    JSCExceptionPrivate* priv = JSC_EXCEPTION(object)->priv;
    if (priv->context) {
        g_object_remove_weak_pointer(G_OBJECT(priv->context), reinterpret_cast<void**>(&priv->context));
        priv->context = nullptr;
    }

    G_OBJECT_CLASS(jsc_exception_parent_class)->dispose(object);
}

static void jsc_exception_class_init(JSCExceptionClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS(klass);
    objClass->dispose = jscExceptionDispose;
}

GRefPtr<JSCException> jscExceptionCreate(JSCContext* context, JSValueRef jsException)
{
    GRefPtr<JSCException> exception = adoptGRef(JSC_EXCEPTION(g_object_new(JSC_TYPE_EXCEPTION, nullptr)));
    auto* jsContext = jscContextGetJSContext(context);
    JSC::ExecState* exec = toJS(jsContext);
    JSC::VM& vm = exec->vm();
    JSC::JSLockHolder locker(vm);
    exception->priv->jsException.set(vm, toJS(JSValueToObject(jsContext, jsException, nullptr)));
    // The context has a strong reference to the exception, so we can't ref the context. We use a weak
    // pointer instead to invalidate the exception if the context is destroyed before.
    exception->priv->context = context;
    g_object_add_weak_pointer(G_OBJECT(context), reinterpret_cast<void**>(&exception->priv->context));
    return exception;
}

JSValueRef jscExceptionGetJSValue(JSCException* exception)
{
    return toRef(exception->priv->jsException.get());
}

void jscExceptionEnsureProperties(JSCException* exception)
{
    JSCExceptionPrivate* priv = exception->priv;
    if (priv->cached)
        return;

    priv->cached = true;

    auto value = jscContextGetOrCreateValue(priv->context, toRef(priv->jsException.get()));
    auto propertyValue = adoptGRef(jsc_value_object_get_property(value.get(), "message"));
    if (!jsc_value_is_undefined(propertyValue.get()))
        priv->message.reset(jsc_value_to_string(propertyValue.get()));
    propertyValue = adoptGRef(jsc_value_object_get_property(value.get(), "line"));
    if (!jsc_value_is_undefined(propertyValue.get()))
        priv->lineNumber = jsc_value_to_int32(propertyValue.get());
    propertyValue = adoptGRef(jsc_value_object_get_property(value.get(), "sourceURL"));
    if (!jsc_value_is_undefined(propertyValue.get()))
        priv->sourceURI.reset(jsc_value_to_string(propertyValue.get()));
}

/**
 * jsc_exception_new:
 * @context: a #JSCContext
 * @message: the error message
 *
 * Create a new #JSCException in @context with @message.
 *
 * Returns: (transfer full): a new JSCException.
 */
JSCException* jsc_exception_new(JSCContext* context, const char* message)
{
    g_return_val_if_fail(JSC_IS_CONTEXT(context), nullptr);

    auto* jsContext = jscContextGetJSContext(context);
    JSValueRef jsMessage = nullptr;
    if (message) {
        JSRetainPtr<JSStringRef> jsMessageString(Adopt, JSStringCreateWithUTF8CString(message));
        jsMessage = JSValueMakeString(jsContext, jsMessageString.get());
    }
    return jscExceptionCreate(context, JSObjectMakeError(jsContext, jsMessage ? 1 : 0, &jsMessage, nullptr)).leakRef();
}

/**
 * jsc_exception_get_message:
 * @exception: a #JSCException
 *
 * Get the error message of @exception.
 *
 * Returns: the @exception error message.
 */
const char* jsc_exception_get_message(JSCException* exception)
{
    g_return_val_if_fail(JSC_IS_EXCEPTION(exception), nullptr);

    JSCExceptionPrivate* priv = exception->priv;
    g_return_val_if_fail(priv->context, nullptr);

    jscExceptionEnsureProperties(exception);
    return priv->message.get();
}

/**
 * jsc_exception_get_line_number:
 * @exception: a #JSCException
 *
 * Get the line number at which @exception happened.
 *
 * Returns: the line number of @exception.
 */
guint jsc_exception_get_line_number(JSCException* exception)
{
    g_return_val_if_fail(JSC_IS_EXCEPTION(exception), 0);

    JSCExceptionPrivate* priv = exception->priv;
    g_return_val_if_fail(priv->context, 0);

    jscExceptionEnsureProperties(exception);
    return priv->lineNumber;
}

/**
 * jsc_exception_get_source_uri:
 * @exception: a #JSCException
 *
 * Get the source URI of @exception.
 *
 * Returns: (nullable): the the source URI of @exception, or %NULL.
 */
const char* jsc_exception_get_source_uri(JSCException* exception)
{
    g_return_val_if_fail(JSC_IS_EXCEPTION(exception), nullptr);

    JSCExceptionPrivate* priv = exception->priv;
    g_return_val_if_fail(priv->context, nullptr);

    jscExceptionEnsureProperties(exception);
    return priv->sourceURI.get();
}
