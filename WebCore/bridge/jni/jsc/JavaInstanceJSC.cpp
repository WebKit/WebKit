/*
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JavaInstanceJSC.h"

#if ENABLE(MAC_JAVA_BRIDGE)

#include "JavaRuntimeObject.h"
#include "JNIBridgeJSC.h"
#include "JNIUtility.h"
#include "JNIUtilityPrivate.h"
#include "JavaClassJSC.h"
#include "Logging.h"
#include "runtime_method.h"
#include "runtime_object.h"
#include "runtime_root.h"
#include <runtime/ArgList.h>
#include <runtime/Error.h>
#include <runtime/JSLock.h>

using namespace JSC::Bindings;
using namespace JSC;
using namespace WebCore;

JavaInstance::JavaInstance(jobject instance, PassRefPtr<RootObject> rootObject)
    : Instance(rootObject)
{
    m_instance = new JObjectWrapper(instance);
    m_class = 0;
}

JavaInstance::~JavaInstance()
{
    delete m_class;
}

RuntimeObject* JavaInstance::newRuntimeObject(ExecState* exec)
{
    return new (exec) JavaRuntimeObject(exec, this);
}

#define NUM_LOCAL_REFS 64

void JavaInstance::virtualBegin()
{
    getJNIEnv()->PushLocalFrame(NUM_LOCAL_REFS);
}

void JavaInstance::virtualEnd()
{
    getJNIEnv()->PopLocalFrame(0);
}

Class* JavaInstance::getClass() const
{
    if (!m_class)
        m_class = new JavaClass (m_instance->m_instance);
    return m_class;
}

JSValue JavaInstance::stringValue(ExecState* exec) const
{
    JSLock lock(SilenceAssertionsOnly);

    jstring stringValue = (jstring)callJNIMethod<jobject>(m_instance->m_instance, "toString", "()Ljava/lang/String;");

    // Should throw a JS exception, rather than returning ""? - but better than a null dereference.
    if (!stringValue)
        return jsString(exec, UString());

    JNIEnv* env = getJNIEnv();
    const jchar* c = getUCharactersFromJStringInEnv(env, stringValue);
    UString u((const UChar*)c, (int)env->GetStringLength(stringValue));
    releaseUCharactersForJStringInEnv(env, stringValue, c);
    return jsString(exec, u);
}

JSValue JavaInstance::numberValue(ExecState* exec) const
{
    jdouble doubleValue = callJNIMethod<jdouble>(m_instance->m_instance, "doubleValue", "()D");
    return jsNumber(exec, doubleValue);
}

JSValue JavaInstance::booleanValue() const
{
    jboolean booleanValue = callJNIMethod<jboolean>(m_instance->m_instance, "booleanValue", "()Z");
    return jsBoolean(booleanValue);
}

class JavaRuntimeMethod : public RuntimeMethod {
public:
    JavaRuntimeMethod(ExecState* exec, const Identifier& name, Bindings::MethodList& list)
        : RuntimeMethod(exec, name, list)
    {
    }

    virtual const ClassInfo* classInfo() const { return &s_info; }

    static const ClassInfo s_info;
};

const ClassInfo JavaRuntimeMethod::s_info = { "JavaRuntimeMethod", &RuntimeMethod::s_info, 0, 0 };

JSValue JavaInstance::getMethod(ExecState* exec, const Identifier& propertyName)
{
    MethodList methodList = getClass()->methodsNamed(propertyName, this);
    return new (exec) JavaRuntimeMethod(exec, propertyName, methodList);
}

JSValue JavaInstance::invokeMethod(ExecState* exec, RuntimeMethod* runtimeMethod, const ArgList &args)
{
    if (!asObject(runtimeMethod)->inherits(&JavaRuntimeMethod::s_info))
        return throwError(exec, TypeError, "Attempt to invoke non-Java method on Java object.");

    const MethodList& methodList = *runtimeMethod->methods();

    int i;
    int count = args.size();
    JSValue resultValue;
    Method* method = 0;
    size_t numMethods = methodList.size();

    // Try to find a good match for the overloaded method.  The
    // fundamental problem is that JavaScript doesn't have the
    // notion of method overloading and Java does.  We could
    // get a bit more sophisticated and attempt to does some
    // type checking as we as checking the number of parameters.
    for (size_t methodIndex = 0; methodIndex < numMethods; methodIndex++) {
        Method* aMethod = methodList[methodIndex];
        if (aMethod->numParameters() == count) {
            method = aMethod;
            break;
        }
    }
    if (!method) {
        LOG(LiveConnect, "JavaInstance::invokeMethod unable to find an appropiate method");
        return jsUndefined();
    }

    const JavaMethod* jMethod = static_cast<const JavaMethod*>(method);
    LOG(LiveConnect, "JavaInstance::invokeMethod call %s %s on %p", UString(jMethod->name()).UTF8String().c_str(), jMethod->signature(), m_instance->m_instance);

    Vector<jvalue> jArgs(count);

    for (i = 0; i < count; i++) {
        JavaParameter* aParameter = jMethod->parameterAt(i);
        jArgs[i] = convertValueToJValue(exec, m_rootObject.get(), args.at(i), aParameter->getJNIType(), aParameter->type());
        LOG(LiveConnect, "JavaInstance::invokeMethod arg[%d] = %s", i, args.at(i).toString(exec).ascii());
    }

    jvalue result;

    // Try to use the JNI abstraction first, otherwise fall back to
    // normal JNI.  The JNI dispatch abstraction allows the Java plugin
    // to dispatch the call on the appropriate internal VM thread.
    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return jsUndefined();

    bool handled = false;
    if (rootObject->nativeHandle()) {
        jobject obj = m_instance->m_instance;
        JSValue exceptionDescription;
        const char *callingURL = 0; // FIXME, need to propagate calling URL to Java
        handled = dispatchJNICall(exec, rootObject->nativeHandle(), obj, jMethod->isStatic(), jMethod->JNIReturnType(), jMethod->methodID(obj), jArgs.data(), result, callingURL, exceptionDescription);
        if (exceptionDescription) {
            throwError(exec, GeneralError, exceptionDescription.toString(exec));
            return jsUndefined();
        }
    }

#ifdef BUILDING_ON_TIGER
    if (!handled) {
        jobject obj = m_instance->m_instance;
        switch (jMethod->JNIReturnType()) {
        case void_type:
            callJNIMethodIDA<void>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case object_type:
            result.l = callJNIMethodIDA<jobject>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case boolean_type:
            result.z = callJNIMethodIDA<jboolean>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case byte_type:
            result.b = callJNIMethodIDA<jbyte>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case char_type:
            result.c = callJNIMethodIDA<jchar>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case short_type:
            result.s = callJNIMethodIDA<jshort>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case int_type:
            result.i = callJNIMethodIDA<jint>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case long_type:
            result.j = callJNIMethodIDA<jlong>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case float_type:
            result.f = callJNIMethodIDA<jfloat>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case double_type:
            result.d = callJNIMethodIDA<jdouble>(obj, jMethod->methodID(obj), jArgs.data());
            break;
        case array_type:
        case invalid_type:
            break;
        }
    }
#endif

    switch (jMethod->JNIReturnType()) {
    case void_type:
        {
            resultValue = jsUndefined();
        }
        break;

    case object_type:
        {
            if (result.l) {
                // FIXME: array_type return type is handled below, can we actually get an array here?
                const char* arrayType = jMethod->returnType();
                if (arrayType[0] == '[')
                    resultValue = JavaArray::convertJObjectToArray(exec, result.l, arrayType, rootObject);
                else {
                    jobject classOfInstance = callJNIMethod<jobject>(result.l, "getClass", "()Ljava/lang/Class;");
                    jstring className = static_cast<jstring>(callJNIMethod<jobject>(classOfInstance, "getName", "()Ljava/lang/String;"));
                    if (!strcmp(JavaString(className).UTF8String(), "sun.plugin.javascript.webkit.JSObject")) {
                        // Pull the nativeJSObject value from the Java instance.  This is a pointer to the JSObject.
                        JNIEnv* env = getJNIEnv();
                        jfieldID fieldID = env->GetFieldID(static_cast<jclass>(classOfInstance), "nativeJSObject", "J");
                        jlong nativeHandle = env->GetLongField(result.l, fieldID);
                        // FIXME: Handling of undefined values differs between functions in JNIUtilityPrivate.cpp and those in those in jni_jsobject.mm,
                        // and so it does between different versions of LiveConnect spec. There should not be multiple code paths to do the same work.
                        if (nativeHandle == 1 /* UndefinedHandle */)
                            return jsUndefined();
                        return static_cast<JSObject*>(jlong_to_ptr(nativeHandle));
                    } else
                        return JavaInstance::create(result.l, rootObject)->createRuntimeObject(exec);
                }
            } else
                return jsUndefined();
        }
        break;

    case boolean_type:
        {
            resultValue = jsBoolean(result.z);
        }
        break;

    case byte_type:
        {
            resultValue = jsNumber(exec, result.b);
        }
        break;

    case char_type:
        {
            resultValue = jsNumber(exec, result.c);
        }
        break;

    case short_type:
        {
            resultValue = jsNumber(exec, result.s);
        }
        break;

    case int_type:
        {
            resultValue = jsNumber(exec, result.i);
        }
        break;

    case long_type:
        {
            resultValue = jsNumber(exec, result.j);
        }
        break;

    case float_type:
        {
            resultValue = jsNumber(exec, result.f);
        }
        break;

    case double_type:
        {
            resultValue = jsNumber(exec, result.d);
        }
        break;

    case array_type:
        {
            const char* arrayType = jMethod->returnType();
            ASSERT(arrayType[0] == '[');
            resultValue = JavaArray::convertJObjectToArray(exec, result.l, arrayType, rootObject);
        }
        break;

    case invalid_type:
        {
            resultValue = jsUndefined();
        }
        break;
    }

    return resultValue;
}

JSValue JavaInstance::defaultValue(ExecState* exec, PreferredPrimitiveType hint) const
{
    if (hint == PreferString)
        return stringValue(exec);
    if (hint == PreferNumber)
        return numberValue(exec);
    JavaClass* aClass = static_cast<JavaClass*>(getClass());
    if (aClass->isStringClass())
        return stringValue(exec);
    if (aClass->isNumberClass())
        return numberValue(exec);
    if (aClass->isBooleanClass())
        return booleanValue();
    return valueOf(exec);
}

JSValue JavaInstance::valueOf(ExecState* exec) const
{
    return stringValue(exec);
}

JObjectWrapper::JObjectWrapper(jobject instance)
    : m_refCount(0)
{
    ASSERT(instance);

    // Cache the JNIEnv used to get the global ref for this java instance.
    // It'll be used to delete the reference.
    m_env = getJNIEnv();

    m_instance = m_env->NewGlobalRef(instance);

    LOG(LiveConnect, "JObjectWrapper ctor new global ref %p for %p", m_instance, instance);

    if (!m_instance)
        LOG_ERROR("Could not get GlobalRef for %p", instance);
}

JObjectWrapper::~JObjectWrapper()
{
    LOG(LiveConnect, "JObjectWrapper dtor deleting global ref %p", m_instance);
    m_env->DeleteGlobalRef(m_instance);
}

#endif // ENABLE(MAC_JAVA_BRIDGE)
