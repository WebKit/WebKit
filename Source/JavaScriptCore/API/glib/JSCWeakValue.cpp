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
#include "JSCWeakValue.h"

#include "APICast.h"
#include "JSCContextPrivate.h"
#include "JSCInlines.h"
#include "JSCValuePrivate.h"
#include "JSWeakValue.h"
#include "WeakHandleOwner.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * SECTION: JSCWeakValue
 * @short_description: JavaScript weak value
 * @title: JSCWeakValue
 * @see_also: JSCValue
 *
 * JSCWeakValue represents a weak reference to a value in a #JSCContext. It can be used
 * to keep a reference to a JavaScript value without protecting it from being garbage
 * collected and without referencing the #JSCContext either.
 */

enum {
    PROP_0,

    PROP_VALUE,
};

enum {
    CLEARED,

    LAST_SIGNAL
};

struct _JSCWeakValuePrivate {
    JSC::Weak<JSC::JSGlobalObject> globalObject;
    RefPtr<JSC::JSLock> lock;
    JSC::JSWeakValue weakValueRef;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(JSCWeakValue, jsc_weak_value, G_TYPE_OBJECT)

static void jscWeakValueClear(JSCWeakValue* weakValue)
{
    JSCWeakValuePrivate* priv = weakValue->priv;
    priv->globalObject.clear();
    priv->weakValueRef.clear();
}

class JSCWeakValueHandleOwner : public JSC::WeakHandleOwner {
public:
    void finalize(JSC::Handle<JSC::Unknown>, void* context) override
    {
        auto* weakValue = JSC_WEAK_VALUE(context);
        jscWeakValueClear(weakValue);
        g_signal_emit(weakValue, signals[CLEARED], 0, nullptr);
    }
};

static JSCWeakValueHandleOwner& weakValueHandleOwner()
{
    static NeverDestroyed<JSCWeakValueHandleOwner> jscWeakValueHandleOwner;
    return jscWeakValueHandleOwner;
}

static void jscWeakValueInitialize(JSCWeakValue* weakValue, JSCValue* value)
{
    JSCWeakValuePrivate* priv = weakValue->priv;
    auto* jsContext = jscContextGetJSContext(jsc_value_get_context(value));
    JSC::JSGlobalObject* globalObject = toJS(jsContext);
    auto& owner = weakValueHandleOwner();
    JSC::Weak<JSC::JSGlobalObject> weak(globalObject, &owner, weakValue);
    priv->globalObject.swap(weak);
    priv->lock = &globalObject->vm().apiLock();

    JSC::JSValue jsValue = toJS(globalObject, jscValueGetJSValue(value));
    if (jsValue.isObject())
        priv->weakValueRef.setObject(JSC::jsCast<JSC::JSObject*>(jsValue.asCell()), owner, weakValue);
    else if (jsValue.isString())
        priv->weakValueRef.setString(JSC::jsCast<JSC::JSString*>(jsValue.asCell()), owner, weakValue);
    else
        priv->weakValueRef.setPrimitive(jsValue);
}

static void jscWeakValueSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    switch (propID) {
    case PROP_VALUE:
        jscWeakValueInitialize(JSC_WEAK_VALUE(object), JSC_VALUE(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void jscWeakValueDispose(GObject* object)
{
    JSCWeakValue* weakValue = JSC_WEAK_VALUE(object);
    jscWeakValueClear(weakValue);

    G_OBJECT_CLASS(jsc_weak_value_parent_class)->dispose(object);
}

static void jsc_weak_value_class_init(JSCWeakValueClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS(klass);
    objClass->set_property = jscWeakValueSetProperty;
    objClass->dispose = jscWeakValueDispose;

    /**
     * JSCWeakValue:value:
     *
     * The #JSCValue referencing the JavaScript value.
     */
    g_object_class_install_property(objClass,
        PROP_VALUE,
        g_param_spec_object(
            "value",
            "JSCValue",
            "JSC Value",
            JSC_TYPE_VALUE,
            static_cast<GParamFlags>(WEBKIT_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * JSCWeakValue::cleared:
     * @weak_value: the #JSCWeakValue
     *
     * This signal is emitted when the JavaScript value is destroyed.
     */
    signals[CLEARED] = g_signal_new(
        "cleared",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0,
        G_TYPE_NONE);
}

/**
 * jsc_weak_value_new:
 * @value: a #JSCValue
 *
 * Create a new #JSCWeakValue for the JavaScript value referenced by @value.
 *
 * Returns: (transfer full): a new #JSCWeakValue
 */
JSCWeakValue* jsc_weak_value_new(JSCValue* value)
{
    g_return_val_if_fail(JSC_IS_VALUE(value), nullptr);

    return JSC_WEAK_VALUE(g_object_new(JSC_TYPE_WEAK_VALUE, "value", value, nullptr));
}

/**
 * jsc_weak_value_get_value:
 * @weak_value: a #JSCWeakValue
 *
 * Get a #JSCValue referencing the JavaScript value of @weak_value.
 *
 * Returns: (transfer full): a new #JSCValue or %NULL if @weak_value was cleared.
 */
JSCValue* jsc_weak_value_get_value(JSCWeakValue* weakValue)
{
    g_return_val_if_fail(JSC_IS_WEAK_VALUE(weakValue), nullptr);

    JSCWeakValuePrivate* priv = weakValue->priv;
    WTF::Locker<JSC::JSLock> locker(priv->lock.get());
    JSC::VM* vm = priv->lock->vm();
    if (!vm)
        return nullptr;

    JSC::JSLockHolder apiLocker(vm);
    if (!priv->globalObject || priv->weakValueRef.isClear())
        return nullptr;

    JSC::JSValue value;
    if (priv->weakValueRef.isPrimitive())
        value = priv->weakValueRef.primitive();
    else if (priv->weakValueRef.isString())
        value = priv->weakValueRef.string();
    else
        value = priv->weakValueRef.object();

    JSC::JSGlobalObject* globalObject = priv->globalObject.get();
    GRefPtr<JSCContext> context = jscContextGetOrCreate(toGlobalRef(globalObject));
    return jscContextGetOrCreateValue(context.get(), toRef(globalObject, value)).leakRef();
}
