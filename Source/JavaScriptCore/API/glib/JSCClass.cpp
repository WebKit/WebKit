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
#include "JSCClass.h"

#include "APICast.h"
#include "JSAPIWrapperGlobalObject.h"
#include "JSAPIWrapperObject.h"
#include "JSCCallbackFunction.h"
#include "JSCClassPrivate.h"
#include "JSCContextPrivate.h"
#include "JSCExceptionPrivate.h"
#include "JSCInlines.h"
#include "JSCValuePrivate.h"
#include "JSCallbackObject.h"
#include "JSRetainPtr.h"
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * SECTION: JSCClass
 * @short_description: JavaScript custom class
 * @title: JSCClass
 * @see_also: JSCContext
 *
 * A JSSClass represents a custom JavaScript class registered by the user in a #JSCContext.
 * It allows to create new JavaScripts objects whose instances are created by the user using
 * this API.
 * It's possible to add constructors, properties and methods for a JSSClass by providing
 * #GCallback<!-- -->s to implement them.
 */

enum {
    PROP_0,

    PROP_CONTEXT,
    PROP_NAME,
    PROP_PARENT
};

typedef struct _JSCClassPrivate {
    JSCContext* context;
    CString name;
    JSClassRef jsClass;
    JSCClassVTable* vtable;
    GDestroyNotify destroyFunction;
    JSCClass* parentClass;
    JSC::Weak<JSC::JSObject> prototype;
    HashMap<CString, JSC::Weak<JSC::JSObject>> constructors;
} JSCClassPrivate;

struct _JSCClass {
    GObject parent;

    JSCClassPrivate* priv;
};

struct _JSCClassClass {
    GObjectClass parent_class;
};

WEBKIT_DEFINE_TYPE(JSCClass, jsc_class, G_TYPE_OBJECT)

class VTableExceptionHandler {
public:
    VTableExceptionHandler(JSCContext* context, JSValueRef* exception)
        : m_context(context)
        , m_exception(exception)
        , m_savedException(exception ? jsc_context_get_exception(m_context) : nullptr)
    {
    }

    ~VTableExceptionHandler()
    {
        if (!m_exception)
            return;

        auto* exception = jsc_context_get_exception(m_context);
        if (m_savedException.get() == exception)
            return;

        *m_exception = jscExceptionGetJSValue(exception);
        if (m_savedException)
            jsc_context_throw_exception(m_context, m_savedException.get());
        else
            jsc_context_clear_exception(m_context);
    }

private:
    JSCContext* m_context { nullptr };
    JSValueRef* m_exception { nullptr };
    GRefPtr<JSCException> m_savedException;
};

static bool isWrappedObject(JSC::JSObject* jsObject)
{
    JSC::ExecState* exec = jsObject->globalObject()->globalExec();
    if (jsObject->isGlobalObject())
        return jsObject->inherits<JSC::JSCallbackObject<JSC::JSAPIWrapperGlobalObject>>(exec->vm());
    return jsObject->inherits<JSC::JSCallbackObject<JSC::JSAPIWrapperObject>>(exec->vm());
}

static JSClassRef wrappedObjectClass(JSC::JSObject* jsObject)
{
    ASSERT(isWrappedObject(jsObject));
    if (jsObject->isGlobalObject())
        return JSC::jsCast<JSC::JSCallbackObject<JSC::JSAPIWrapperGlobalObject>*>(jsObject)->classRef();
    return JSC::jsCast<JSC::JSCallbackObject<JSC::JSAPIWrapperObject>*>(jsObject)->classRef();
}

static GRefPtr<JSCContext> jscContextForObject(JSC::JSObject* jsObject)
{
    ASSERT(isWrappedObject(jsObject));
    JSC::JSGlobalObject* globalObject = jsObject->globalObject();
    JSC::ExecState* exec = globalObject->globalExec();
    if (jsObject->isGlobalObject()) {
        JSC::VM& vm = globalObject->vm();
        if (auto* globalScopeExtension = vm.vmEntryGlobalObject(exec)->globalScopeExtension())
            exec = JSC::JSScope::objectAtScope(globalScopeExtension)->globalObject()->globalExec();
    }
    return jscContextGetOrCreate(toGlobalRef(exec));
}

static JSValueRef getProperty(JSContextRef callerContext, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSC::JSLockHolder locker(toJS(callerContext));
    auto* jsObject = toJS(object);
    if (!isWrappedObject(jsObject))
        return nullptr;

    auto context = jscContextForObject(jsObject);
    gpointer instance = jscContextWrappedObject(context.get(), object);
    if (!instance)
        return nullptr;

    VTableExceptionHandler exceptionHandler(context.get(), exception);

    JSClassRef jsClass = wrappedObjectClass(jsObject);
    for (auto* jscClass = jscContextGetRegisteredClass(context.get(), jsClass); jscClass; jscClass = jscClass->priv->parentClass) {
        if (!jscClass->priv->vtable)
            continue;

        if (auto* getPropertyFunction = jscClass->priv->vtable->get_property) {
            if (GRefPtr<JSCValue> value = adoptGRef(getPropertyFunction(jscClass, context.get(), instance, propertyName->string().utf8().data())))
                return jscValueGetJSValue(value.get());
        }
    }
    return nullptr;
}

static bool setProperty(JSContextRef callerContext, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    JSC::JSLockHolder locker(toJS(callerContext));
    auto* jsObject = toJS(object);
    if (!isWrappedObject(jsObject))
        return false;

    auto context = jscContextForObject(jsObject);
    gpointer instance = jscContextWrappedObject(context.get(), object);
    if (!instance)
        return false;

    VTableExceptionHandler exceptionHandler(context.get(), exception);

    GRefPtr<JSCValue> propertyValue;
    JSClassRef jsClass = wrappedObjectClass(jsObject);
    for (auto* jscClass = jscContextGetRegisteredClass(context.get(), jsClass); jscClass; jscClass = jscClass->priv->parentClass) {
        if (!jscClass->priv->vtable)
            continue;

        if (auto* setPropertyFunction = jscClass->priv->vtable->set_property) {
            if (!propertyValue)
                propertyValue = jscContextGetOrCreateValue(context.get(), value);
            if (setPropertyFunction(jscClass, context.get(), instance, propertyName->string().utf8().data(), propertyValue.get()))
                return true;
        }
    }
    return false;
}

static bool hasProperty(JSContextRef callerContext, JSObjectRef object, JSStringRef propertyName)
{
    JSC::JSLockHolder locker(toJS(callerContext));
    auto* jsObject = toJS(object);
    if (!isWrappedObject(jsObject))
        return false;

    auto context = jscContextForObject(jsObject);
    gpointer instance = jscContextWrappedObject(context.get(), object);
    if (!instance)
        return false;

    JSClassRef jsClass = wrappedObjectClass(jsObject);
    for (auto* jscClass = jscContextGetRegisteredClass(context.get(), jsClass); jscClass; jscClass = jscClass->priv->parentClass) {
        if (!jscClass->priv->vtable)
            continue;

        if (auto* hasPropertyFunction = jscClass->priv->vtable->has_property) {
            if (hasPropertyFunction(jscClass, context.get(), instance, propertyName->string().utf8().data()))
                return true;
        }
    }

    return false;
}

static bool deleteProperty(JSContextRef callerContext, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSC::JSLockHolder locker(toJS(callerContext));
    auto* jsObject = toJS(object);
    if (!isWrappedObject(jsObject))
        return false;

    auto context = jscContextForObject(jsObject);
    gpointer instance = jscContextWrappedObject(context.get(), object);
    if (!instance)
        return false;

    VTableExceptionHandler exceptionHandler(context.get(), exception);

    JSClassRef jsClass = wrappedObjectClass(jsObject);
    for (auto* jscClass = jscContextGetRegisteredClass(context.get(), jsClass); jscClass; jscClass = jscClass->priv->parentClass) {
        if (!jscClass->priv->vtable)
            continue;

        if (auto* deletePropertyFunction = jscClass->priv->vtable->delete_property) {
            if (deletePropertyFunction(jscClass, context.get(), instance, propertyName->string().utf8().data()))
                return true;
        }
    }
    return false;
}

static void getPropertyNames(JSContextRef callerContext, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames)
{
    JSC::JSLockHolder locker(toJS(callerContext));
    auto* jsObject = toJS(object);
    if (!isWrappedObject(jsObject))
        return;

    auto context = jscContextForObject(jsObject);
    gpointer instance = jscContextWrappedObject(context.get(), object);
    if (!instance)
        return;

    JSClassRef jsClass = wrappedObjectClass(jsObject);
    for (auto* jscClass = jscContextGetRegisteredClass(context.get(), jsClass); jscClass; jscClass = jscClass->priv->parentClass) {
        if (!jscClass->priv->vtable)
            continue;

        if (auto* enumeratePropertiesFunction = jscClass->priv->vtable->enumerate_properties) {
            GUniquePtr<char*> properties(enumeratePropertiesFunction(jscClass, context.get(), instance));
            if (properties) {
                unsigned i = 0;
                while (const auto* name = properties.get()[i++]) {
                    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString(name));
                    JSPropertyNameAccumulatorAddName(propertyNames, propertyName.get());
                }
            }
        }
    }
}

static void jscClassGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    JSCClass* jscClass = JSC_CLASS(object);

    switch (propID) {
    case PROP_CONTEXT:
        g_value_set_object(value, jscClass->priv->context);
        break;
    case PROP_NAME:
        g_value_set_string(value, jscClass->priv->name.data());
        break;
    case PROP_PARENT:
        g_value_set_object(value, jscClass->priv->parentClass);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void jscClassSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    JSCClass* jscClass = JSC_CLASS(object);

    switch (propID) {
    case PROP_CONTEXT:
        jscClass->priv->context = JSC_CONTEXT(g_value_get_object(value));
        break;
    case PROP_NAME:
        jscClass->priv->name = g_value_get_string(value);
        break;
    case PROP_PARENT:
        if (auto* parent = g_value_get_object(value))
            jscClass->priv->parentClass = JSC_CLASS(parent);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void jscClassDispose(GObject* object)
{
    JSCClass* jscClass = JSC_CLASS(object);
    if (jscClass->priv->jsClass) {
        JSClassRelease(jscClass->priv->jsClass);
        jscClass->priv->jsClass = nullptr;
    }

    G_OBJECT_CLASS(jsc_class_parent_class)->dispose(object);
}

static void jsc_class_class_init(JSCClassClass* klass)
{
    GObjectClass* objClass = G_OBJECT_CLASS(klass);
    objClass->dispose = jscClassDispose;
    objClass->get_property = jscClassGetProperty;
    objClass->set_property = jscClassSetProperty;

    /**
     * JSCClass:context:
     *
     * The #JSCContext in which the class was registered.
     */
    g_object_class_install_property(objClass,
        PROP_CONTEXT,
        g_param_spec_object(
            "context",
            "JSCContext",
            "JSC Context",
            JSC_TYPE_CONTEXT,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * JSCClass:name:
     *
     * The name of the class.
     */
    g_object_class_install_property(objClass,
        PROP_NAME,
        g_param_spec_string(
            "name",
            "Name",
            "The class name",
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * JSCClass:parent:
     *
     * The parent class or %NULL in case of final classes.
     */
    g_object_class_install_property(objClass,
        PROP_PARENT,
        g_param_spec_object(
            "parent",
            "Partent",
            "The parent class",
            JSC_TYPE_CLASS,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

/**
 * JSCClassGetPropertyFunction:
 * @jsc_class: a #JSCClass
 * @context: a #JSCContext
 * @instance: the @jsc_class instance
 * @name: the property name
 *
 * The type of get_property in #JSCClassVTable. This is only required when you need to handle
 * external properties not added to the prototype.
 *
 * Returns: (transfer full) (nullable): a #JSCValue or %NULL to forward the request to
 *    the parent class or prototype chain
 */

/**
 * JSCClassSetPropertyFunction:
 * @jsc_class: a #JSCClass
 * @context: a #JSCContext
 * @instance: the @jsc_class instance
 * @name: the property name
 * @value: the #JSCValue to set
 *
 * The type of set_property in #JSCClassVTable. This is only required when you need to handle
 * external properties not added to the prototype.
 *
 * Returns: %TRUE if handled or %FALSE to forward the request to the parent class or prototype chain.
 */

/**
 * JSCClassHasPropertyFunction:
 * @jsc_class: a #JSCClass
 * @context: a #JSCContext
 * @instance: the @jsc_class instance
 * @name: the property name
 *
 * The type of has_property in #JSCClassVTable. This is only required when you need to handle
 * external properties not added to the prototype.
 *
 * Returns: %TRUE if @instance has a property with @name or %FALSE to forward the request
 *    to the parent class or prototype chain.
 */

/**
 * JSCClassDeletePropertyFunction:
 * @jsc_class: a #JSCClass
 * @context: a #JSCContext
 * @instance: the @jsc_class instance
 * @name: the property name
 *
 * The type of delete_property in #JSCClassVTable. This is only required when you need to handle
 * external properties not added to the prototype.
 *
 * Returns: %TRUE if handled or %FALSE to to forward the request to the parent class or prototype chain.
 */

/**
 * JSCClassEnumeratePropertiesFunction:
 * @jsc_class: a #JSCClass
 * @context: a #JSCContext
 * @instance: the @jsc_class instance
 *
 * The type of enumerate_properties in #JSCClassVTable. This is only required when you need to handle
 * external properties not added to the prototype.
 *
 * Returns: (array zero-terminated=1) (transfer full) (nullable): a %NULL-terminated array of strings
 *    containing the property names, or %NULL if @instance doesn't have enumerable properties.
 */

/**
 * JSCClassVTable:
 * @get_property: a #JSCClassGetPropertyFunction for getting a property.
 * @set_property: a #JSCClassSetPropertyFunction for setting a property.
 * @has_property: a #JSCClassHasPropertyFunction for querying a property.
 * @delete_property: a #JSCClassDeletePropertyFunction for deleting a property.
 * @enumerate_properties: a #JSCClassEnumeratePropertiesFunction for enumerating properties.
 *
 * Virtual table for a JSCClass. This can be optionally used when registering a #JSCClass in a #JSCContext
 * to provide a custom implementation for the class. All virtual functions are optional and can be set to
 * %NULL to fallback to the default implementation.
 */

GRefPtr<JSCClass> jscClassCreate(JSCContext* context, const char* name, JSCClass* parentClass, JSCClassVTable* vtable, GDestroyNotify destroyFunction)
{
    GRefPtr<JSCClass> jscClass = adoptGRef(JSC_CLASS(g_object_new(JSC_TYPE_CLASS, "context", context, "name", name, "parent", parentClass, nullptr)));

    JSCClassPrivate* priv = jscClass->priv;
    priv->vtable = vtable;
    priv->destroyFunction = destroyFunction;

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = priv->name.data();

#define SET_IMPL_IF_NEEDED(definitionFunc, vtableFunc) \
    for (auto* klass = jscClass.get(); klass; klass = klass->priv->parentClass) { \
        if (klass->priv->vtable && klass->priv->vtable->vtableFunc) { \
            definition.definitionFunc = definitionFunc; \
            break; \
        } \
    }

    SET_IMPL_IF_NEEDED(getProperty, get_property);
    SET_IMPL_IF_NEEDED(setProperty, set_property);
    SET_IMPL_IF_NEEDED(hasProperty, has_property);
    SET_IMPL_IF_NEEDED(deleteProperty, delete_property);
    SET_IMPL_IF_NEEDED(getPropertyNames, enumerate_properties);

#undef SET_IMPL_IF_NEEDED

    priv->jsClass = JSClassCreate(&definition);

    GUniquePtr<char> prototypeName(g_strdup_printf("%sPrototype", priv->name.data()));
    JSClassDefinition prototypeDefinition = kJSClassDefinitionEmpty;
    prototypeDefinition.className = prototypeName.get();
    JSClassRef prototypeClass = JSClassCreate(&prototypeDefinition);
    priv->prototype = jscContextGetOrCreateJSWrapper(priv->context, prototypeClass);
    JSClassRelease(prototypeClass);

    if (priv->parentClass)
        JSObjectSetPrototype(jscContextGetJSContext(priv->context), toRef(priv->prototype.get()), toRef(priv->parentClass->priv->prototype.get()));
    return jscClass;
}

JSClassRef jscClassGetJSClass(JSCClass* jscClass)
{
    return jscClass->priv->jsClass;
}

JSC::JSObject* jscClassGetOrCreateJSWrapper(JSCClass* jscClass, gpointer wrappedObject)
{
    JSCClassPrivate* priv = jscClass->priv;
    return jscContextGetOrCreateJSWrapper(priv->context, priv->jsClass, toRef(priv->prototype.get()), wrappedObject, priv->destroyFunction);
}

JSGlobalContextRef jscClassCreateContextWithJSWrapper(JSCClass* jscClass, gpointer wrappedObject)
{
    JSCClassPrivate* priv = jscClass->priv;
    return jscContextCreateContextWithJSWrapper(priv->context, priv->jsClass, toRef(priv->prototype.get()), wrappedObject, priv->destroyFunction);
}

void jscClassInvalidate(JSCClass* jscClass)
{
    jscClass->priv->context = nullptr;
}

/**
 * jsc_class_get_name:
 * @jsc_class: a @JSCClass
 *
 * Get the class name of @jsc_class
 *
 * Returns: (transfer none): the name of @jsc_class
 */
const char* jsc_class_get_name(JSCClass* jscClass)
{
    g_return_val_if_fail(JSC_IS_CLASS(jscClass), nullptr);

    return jscClass->priv->name.data();
}

/**
 * jsc_class_get_parent:
 * @jsc_class: a @JSCClass
 *
 * Get the parent class of @jsc_class
 *
 * Returns: (transfer none): the parent class of @jsc_class
 */
JSCClass* jsc_class_get_parent(JSCClass* jscClass)
{
    g_return_val_if_fail(JSC_IS_CLASS(jscClass), nullptr);

    return jscClass->priv->parentClass;
}

static GRefPtr<JSCValue> jscClassCreateConstructor(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, Optional<Vector<GType>>&& parameters)
{
    JSCClassPrivate* priv = jscClass->priv;
    GRefPtr<GClosure> closure = adoptGRef(g_cclosure_new(callback, userData, reinterpret_cast<GClosureNotify>(reinterpret_cast<GCallback>(destroyNotify))));
    JSC::ExecState* exec = toJS(jscContextGetJSContext(priv->context));
    JSC::VM& vm = exec->vm();
    JSC::JSLockHolder locker(vm);
    auto* functionObject = JSC::JSCCallbackFunction::create(vm, exec->lexicalGlobalObject(), String::fromUTF8(name),
        JSC::JSCCallbackFunction::Type::Constructor, jscClass, WTFMove(closure), returnType, WTFMove(parameters));
    auto constructor = jscContextGetOrCreateValue(priv->context, toRef(functionObject));
    GRefPtr<JSCValue> prototype = jscContextGetOrCreateValue(priv->context, toRef(priv->prototype.get()));
    auto nonEnumerable = static_cast<JSCValuePropertyFlags>(JSC_VALUE_PROPERTY_CONFIGURABLE | JSC_VALUE_PROPERTY_WRITABLE);
    jsc_value_object_define_property_data(constructor.get(), "prototype", nonEnumerable, prototype.get());
    jsc_value_object_define_property_data(prototype.get(), "constructor", nonEnumerable, constructor.get());
    priv->constructors.set(name, functionObject);
    return constructor;
}

/**
 * jsc_class_add_constructor: (skip)
 * @jsc_class: a #JSCClass
 * @name: (nullable): the constructor name or %NULL
 * @callback: (scope async): a #GCallback to be called to create an instance of @jsc_class
 * @user_data: (closure): user data to pass to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the constructor return value
 * @n_params: the number of parameter types to follow or 0 if constructor doesn't receive parameters.
 * @...: a list of #GType<!-- -->s, one for each parameter.
 *
 * Add a constructor to @jsc_class. If @name is %NULL, the class name will be used. When <function>new</function>
 * is used with the constructor or jsc_value_constructor_call() is called, @callback is invoked receiving the
 * parameters and @user_data as the last parameter. When the constructor object is cleared in the #JSCClass context,
 * @destroy_notify is called with @user_data as parameter.
 *
 * This function creates the constructor, which needs to be added to an object as a property to be able to use it. Use
 * jsc_context_set_value() to make the constructor available in the global object.
 *
 * Returns: (transfer full): a #JSCValue representing the class constructor.
 */
JSCValue* jsc_class_add_constructor(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, unsigned paramCount, ...)
{
    g_return_val_if_fail(JSC_IS_CLASS(jscClass), nullptr);
    g_return_val_if_fail(callback, nullptr);

    JSCClassPrivate* priv = jscClass->priv;
    g_return_val_if_fail(priv->context, nullptr);

    if (!name)
        name = priv->name.data();

    va_list args;
    va_start(args, paramCount);
    Vector<GType> parameters;
    if (paramCount) {
        parameters.reserveInitialCapacity(paramCount);
        for (unsigned i = 0; i < paramCount; ++i)
            parameters.uncheckedAppend(va_arg(args, GType));
    }
    va_end(args);

    return jscClassCreateConstructor(jscClass, name ? name : priv->name.data(), callback, userData, destroyNotify, returnType, WTFMove(parameters)).leakRef();

}

/**
 * jsc_class_add_constructorv: (rename-to jsc_class_add_constructor)
 * @jsc_class: a #JSCClass
 * @name: (nullable): the constructor name or %NULL
 * @callback: (scope async): a #GCallback to be called to create an instance of @jsc_class
 * @user_data: (closure): user data to pass to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the constructor return value
 * @n_parameters: the number of parameters
 * @parameter_types: (nullable) (array length=n_parameters) (element-type GType): a list of #GType<!-- -->s, one for each parameter, or %NULL
 *
 * Add a constructor to @jsc_class. If @name is %NULL, the class name will be used. When <function>new</function>
 * is used with the constructor or jsc_value_constructor_call() is called, @callback is invoked receiving the
 * parameters and @user_data as the last parameter. When the constructor object is cleared in the #JSCClass context,
 * @destroy_notify is called with @user_data as parameter.
 *
 * This function creates the constructor, which needs to be added to an object as a property to be able to use it. Use
 * jsc_context_set_value() to make the constructor available in the global object.
 *
 * Returns: (transfer full): a #JSCValue representing the class constructor.
 */
JSCValue* jsc_class_add_constructorv(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, unsigned parametersCount, GType* parameterTypes)
{
    g_return_val_if_fail(JSC_IS_CLASS(jscClass), nullptr);
    g_return_val_if_fail(callback, nullptr);
    g_return_val_if_fail(!parametersCount || parameterTypes, nullptr);

    JSCClassPrivate* priv = jscClass->priv;
    g_return_val_if_fail(priv->context, nullptr);

    if (!name)
        name = priv->name.data();

    Vector<GType> parameters;
    if (parametersCount) {
        parameters.reserveInitialCapacity(parametersCount);
        for (unsigned i = 0; i < parametersCount; ++i)
            parameters.uncheckedAppend(parameterTypes[i]);
    }

    return jscClassCreateConstructor(jscClass, name ? name : priv->name.data(), callback, userData, destroyNotify, returnType, WTFMove(parameters)).leakRef();
}

/**
 * jsc_class_add_constructor_variadic:
 * @jsc_class: a #JSCClass
 * @name: (nullable): the constructor name or %NULL
 * @callback: (scope async): a #GCallback to be called to create an instance of @jsc_class
 * @user_data: (closure): user data to pass to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the constructor return value
 *
 * Add a constructor to @jsc_class. If @name is %NULL, the class name will be used. When <function>new</function>
 * is used with the constructor or jsc_value_constructor_call() is called, @callback is invoked receiving
 * a #GPtrArray of #JSCValue<!-- -->s as arguments and @user_data as the last parameter. When the constructor object
 * is cleared in the #JSCClass context, @destroy_notify is called with @user_data as parameter.
 *
 * This function creates the constructor, which needs to be added to an object as a property to be able to use it. Use
 * jsc_context_set_value() to make the constructor available in the global object.
 *
 * Returns: (transfer full): a #JSCValue representing the class constructor.
 */
JSCValue* jsc_class_add_constructor_variadic(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType)
{
    g_return_val_if_fail(JSC_IS_CLASS(jscClass), nullptr);
    g_return_val_if_fail(callback, nullptr);

    JSCClassPrivate* priv = jscClass->priv;
    g_return_val_if_fail(jscClass->priv->context, nullptr);

    if (!name)
        name = priv->name.data();

    return jscClassCreateConstructor(jscClass, name ? name : priv->name.data(), callback, userData, destroyNotify, returnType, WTF::nullopt).leakRef();
}

static void jscClassAddMethod(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, Optional<Vector<GType>>&& parameters)
{
    JSCClassPrivate* priv = jscClass->priv;
    GRefPtr<GClosure> closure = adoptGRef(g_cclosure_new(callback, userData, reinterpret_cast<GClosureNotify>(reinterpret_cast<GCallback>(destroyNotify))));
    JSC::ExecState* exec = toJS(jscContextGetJSContext(priv->context));
    JSC::VM& vm = exec->vm();
    JSC::JSLockHolder locker(vm);
    auto* functionObject = toRef(JSC::JSCCallbackFunction::create(vm, exec->lexicalGlobalObject(), String::fromUTF8(name),
        JSC::JSCCallbackFunction::Type::Method, jscClass, WTFMove(closure), returnType, WTFMove(parameters)));
    auto method = jscContextGetOrCreateValue(priv->context, functionObject);
    GRefPtr<JSCValue> prototype = jscContextGetOrCreateValue(priv->context, toRef(priv->prototype.get()));
    auto nonEnumerable = static_cast<JSCValuePropertyFlags>(JSC_VALUE_PROPERTY_CONFIGURABLE | JSC_VALUE_PROPERTY_WRITABLE);
    jsc_value_object_define_property_data(prototype.get(), name, nonEnumerable, method.get());
}

/**
 * jsc_class_add_method: (skip)
 * @jsc_class: a #JSCClass
 * @name: the method name
 * @callback: (scope async): a #GCallback to be called to invoke method @name of @jsc_class
 * @user_data: (closure): user data to pass to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the method return value, or %G_TYPE_NONE if the method is void.
 * @n_params: the number of parameter types to follow or 0 if the method doesn't receive parameters.
 * @...: a list of #GType<!-- -->s, one for each parameter.
 *
 * Add method with @name to @jsc_class. When the method is called by JavaScript or jsc_value_object_invoke_method(),
 * @callback is called receiving the class instance as first parameter, followed by the method parameters and then
 * @user_data as last parameter. When the method is cleared in the #JSCClass context, @destroy_notify is called with
 * @user_data as parameter.
 */
void jsc_class_add_method(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, unsigned paramCount, ...)
{
    g_return_if_fail(JSC_IS_CLASS(jscClass));
    g_return_if_fail(name);
    g_return_if_fail(callback);
    g_return_if_fail(jscClass->priv->context);

    va_list args;
    va_start(args, paramCount);
    Vector<GType> parameters;
    if (paramCount) {
        parameters.reserveInitialCapacity(paramCount);
        for (unsigned i = 0; i < paramCount; ++i)
            parameters.uncheckedAppend(va_arg(args, GType));
    }
    va_end(args);

    jscClassAddMethod(jscClass, name, callback, userData, destroyNotify, returnType, WTFMove(parameters));
}

/**
 * jsc_class_add_methodv: (rename-to jsc_class_add_method)
 * @jsc_class: a #JSCClass
 * @name: the method name
 * @callback: (scope async): a #GCallback to be called to invoke method @name of @jsc_class
 * @user_data: (closure): user data to pass to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the method return value, or %G_TYPE_NONE if the method is void.
 * @n_parameters: the number of parameter types to follow or 0 if the method doesn't receive parameters.
 * @parameter_types: (nullable) (array length=n_parameters) (element-type GType): a list of #GType<!-- -->s, one for each parameter, or %NULL
 *
 * Add method with @name to @jsc_class. When the method is called by JavaScript or jsc_value_object_invoke_method(),
 * @callback is called receiving the class instance as first parameter, followed by the method parameters and then
 * @user_data as last parameter. When the method is cleared in the #JSCClass context, @destroy_notify is called with
 * @user_data as parameter.
 */
void jsc_class_add_methodv(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType, unsigned parametersCount, GType *parameterTypes)
{
    g_return_if_fail(JSC_IS_CLASS(jscClass));
    g_return_if_fail(name);
    g_return_if_fail(callback);
    g_return_if_fail(!parametersCount || parameterTypes);
    g_return_if_fail(jscClass->priv->context);

    Vector<GType> parameters;
    if (parametersCount) {
        parameters.reserveInitialCapacity(parametersCount);
        for (unsigned i = 0; i < parametersCount; ++i)
            parameters.uncheckedAppend(parameterTypes[i]);
    }

    jscClassAddMethod(jscClass, name, callback, userData, destroyNotify, returnType, WTFMove(parameters));
}

/**
 * jsc_class_add_method_variadic:
 * @jsc_class: a #JSCClass
 * @name: the method name
 * @callback: (scope async): a #GCallback to be called to invoke method @name of @jsc_class
 * @user_data: (closure): user data to pass to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 * @return_type: the #GType of the method return value, or %G_TYPE_NONE if the method is void.
 *
 * Add method with @name to @jsc_class. When the method is called by JavaScript or jsc_value_object_invoke_method(),
 * @callback is called receiving the class instance as first parameter, followed by a #GPtrArray of #JSCValue<!-- -->s
 * with the method arguments and then @user_data as last parameter. When the method is cleared in the #JSCClass context,
 * @destroy_notify is called with @user_data as parameter.
 */
void jsc_class_add_method_variadic(JSCClass* jscClass, const char* name, GCallback callback, gpointer userData, GDestroyNotify destroyNotify, GType returnType)
{
    g_return_if_fail(JSC_IS_CLASS(jscClass));
    g_return_if_fail(name);
    g_return_if_fail(callback);
    g_return_if_fail(jscClass->priv->context);

    jscClassAddMethod(jscClass, name, callback, userData, destroyNotify, returnType, WTF::nullopt);
}

/**
 * jsc_class_add_property:
 * @jsc_class: a #JSCClass
 * @name: the property name
 * @property_type: the #GType of the property value
 * @getter: (scope async) (nullable): a #GCallback to be called to get the property value
 * @setter: (scope async) (nullable): a #GCallback to be called to set the property value
 * @user_data: (closure): user data to pass to @getter and @setter
 * @destroy_notify: (nullable): destroy notifier for @user_data
 *
 * Add a property with @name to @jsc_class. When the property value needs to be getted, @getter is called
 * receiving the the class instance as first parameter and @user_data as last parameter. When the property
 * value needs to be set, @setter is called receiving the the class instance as first parameter, followed
 * by the value to be set and then @user_data as the last parameter. When the property is cleared in the
 * #JSCClass context, @destroy_notify is called with @user_data as parameter.
 */
void jsc_class_add_property(JSCClass* jscClass, const char* name, GType propertyType, GCallback getter, GCallback setter, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_if_fail(JSC_IS_CLASS(jscClass));
    g_return_if_fail(name);
    g_return_if_fail(propertyType != G_TYPE_INVALID && propertyType != G_TYPE_NONE);
    g_return_if_fail(getter || setter);

    JSCClassPrivate* priv = jscClass->priv;
    g_return_if_fail(priv->context);

    GRefPtr<JSCValue> prototype = jscContextGetOrCreateValue(priv->context, toRef(priv->prototype.get()));
    jsc_value_object_define_property_accessor(prototype.get(), name, JSC_VALUE_PROPERTY_CONFIGURABLE, propertyType, getter, setter, userData, destroyNotify);
}
