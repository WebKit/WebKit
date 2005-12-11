/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#include <jni_class.h>
#include <jni_instance.h>
#include <jni_runtime.h>
#include <jni_utility.h>
#include <runtime_object.h>
#include <runtime_root.h>

#ifdef NDEBUG
#define JS_LOG(formatAndArgs...) ((void)0)
#else
#define JS_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:%d -- %s:  ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}
#endif

using namespace KJS::Bindings;
using namespace KJS;

JavaInstance::JavaInstance (jobject instance, const RootObject *r) 
{
    _instance = new JObjectWrapper (instance);
    _class = 0;
    setExecutionContext (r);
};

JavaInstance::~JavaInstance () 
{
    _instance->deref();
    delete _class; 
}

#define NUM_LOCAL_REFS 64

void JavaInstance::begin()
{
    getJNIEnv()->PushLocalFrame (NUM_LOCAL_REFS);
}

void JavaInstance::end()
{
    getJNIEnv()->PopLocalFrame (NULL);
}

Class *JavaInstance::getClass() const 
{
    if (_class == 0)
        _class = new JavaClass (_instance->_instance);
    return _class;
}

JSValue *JavaInstance::stringValue() const
{
    jstring stringValue = (jstring)callJNIObjectMethod (_instance->_instance, "toString", "()Ljava/lang/String;");
    JNIEnv *env = getJNIEnv();
    const jchar *c = getUCharactersFromJStringInEnv(env, stringValue);
    UString u((const UChar *)c, (int)env->GetStringLength(stringValue));
    releaseUCharactersForJStringInEnv(env, stringValue, c);
    return jsString(u);
}

JSValue *JavaInstance::numberValue() const
{
    jdouble doubleValue = callJNIDoubleMethod (_instance->_instance, "doubleValue", "()D");
    return jsNumber(doubleValue);
}

JSValue *JavaInstance::booleanValue() const
{
    jboolean booleanValue = callJNIBooleanMethod (_instance->_instance, "booleanValue", "()Z");
    return jsBoolean(booleanValue);
}

JSValue *JavaInstance::invokeMethod (ExecState *exec, const MethodList &methodList, const List &args)
{
    int i, count = args.size();
    jvalue *jArgs;
    JSValue *resultValue;
    Method *method = 0;
    unsigned int numMethods = methodList.length();
    
    // Try to find a good match for the overloaded method.  The 
    // fundamental problem is that JavaScript doesn have the
    // notion of method overloading and Java does.  We could 
    // get a bit more sophisticated and attempt to does some
    // type checking as we as checking the number of parameters.
    unsigned int methodIndex;
    Method *aMethod;
    for (methodIndex = 0; methodIndex < numMethods; methodIndex++) {
        aMethod = methodList.methodAt (methodIndex);
        if (aMethod->numParameters() == count) {
            method = aMethod;
            break;
        }
    }
    if (method == 0) {
        JS_LOG ("unable to find an appropiate method\n");
        return jsUndefined();
    }
    
    const JavaMethod *jMethod = static_cast<const JavaMethod*>(method);
    JS_LOG ("call %s %s on %p\n", method->name(), jMethod->signature(), _instance->_instance);
    
    if (count > 0) {
        jArgs = (jvalue *)malloc (count * sizeof(jvalue));
    }
    else
        jArgs = 0;
        
    for (i = 0; i < count; i++) {
        JavaParameter *aParameter = static_cast<JavaParameter *>(jMethod->parameterAt(i));
        jArgs[i] = convertValueToJValue (exec, args.at(i), aParameter->getJNIType(), aParameter->type());
	JS_LOG("arg[%d] = %s\n", i, args.at(i)->toString(exec).ascii());
    }
        

    jvalue result;

    // Try to use the JNI abstraction first, otherwise fall back to
    // nornmal JNI.  The JNI dispatch abstraction allows the Java plugin
    // to dispatch the call on the appropriate internal VM thread.
    const RootObject *execContext = executionContext();
    bool handled = false;
    if (execContext && execContext->nativeHandle()) {
        jobject obj = _instance->_instance;
        JSValue *exceptionDescription = NULL;
        const char *callingURL = 0;  // FIXME, need to propagate calling URL to Java
        handled = dispatchJNICall (execContext->nativeHandle(), obj, jMethod->isStatic(), jMethod->JNIReturnType(), jMethod->methodID(obj), jArgs, result, callingURL, exceptionDescription);
        if (exceptionDescription) {
            throwError(exec, GeneralError, exceptionDescription->toString(exec));
            free (jArgs);
            return jsUndefined();
        }
    }
    
    // The following code can be conditionally removed once we have a Tiger update that
    // contains the new Java plugin.  It is needed for builds prior to Tiger.
    if (!handled) {    
        jobject obj = _instance->_instance;
        switch (jMethod->JNIReturnType()){
            case void_type: {
                callJNIVoidMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case object_type: {
                result.l = callJNIObjectMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case boolean_type: {
                result.z = callJNIBooleanMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case byte_type: {
                result.b = callJNIByteMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case char_type: {
                result.c = callJNICharMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case short_type: {
                result.s = callJNIShortMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case int_type: {
                result.i = callJNIIntMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case long_type: {
                result.j = callJNILongMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case float_type: {
                result.f = callJNIFloatMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;
            
            case double_type: {
                result.d = callJNIDoubleMethodIDA (obj, jMethod->methodID(obj), jArgs);
            }
            break;

            case invalid_type:
            default: {
            }
            break;
        }
    }
        
    switch (jMethod->JNIReturnType()){
        case void_type: {
            resultValue = jsUndefined();
        }
        break;
        
        case object_type: {
            if (result.l != 0) {
                const char *arrayType = jMethod->returnType();
                if (arrayType[0] == '[') {
                    resultValue = JavaArray::convertJObjectToArray (exec, result.l, arrayType, executionContext());
                }
                else {
                    resultValue = Instance::createRuntimeObject(Instance::JavaLanguage, result.l, executionContext());
                }
            }
            else {
                resultValue = jsUndefined();
            }
        }
        break;
        
        case boolean_type: {
            resultValue = jsBoolean(result.z);
        }
        break;
        
        case byte_type: {
            resultValue = jsNumber(result.b);
        }
        break;
        
        case char_type: {
            resultValue = jsNumber(result.c);
        }
        break;
        
        case short_type: {
            resultValue = jsNumber(result.s);
        }
        break;
        
        case int_type: {
            resultValue = jsNumber(result.i);
        }
        break;
        
        case long_type: {
            resultValue = jsNumber(result.j);
        }
        break;
        
        case float_type: {
            resultValue = jsNumber(result.f);
        }
        break;
        
        case double_type: {
            resultValue = jsNumber(result.d);
        }
        break;

        case invalid_type:
        default: {
            resultValue = jsUndefined();
        }
        break;
    }

    free (jArgs);

    return resultValue;
}

JSValue *JavaInstance::invokeDefaultMethod (ExecState *exec, const List &args)
{
    return jsUndefined();
}


JSValue *JavaInstance::defaultValue (Type hint) const
{
    if (hint == StringType) {
        return stringValue();
    }
    else if (hint == NumberType) {
        return numberValue();
    }
    else if (hint == BooleanType) {
        return booleanValue();
    }
    else if (hint == UnspecifiedType) {
        JavaClass *aClass = static_cast<JavaClass*>(getClass());
        if (aClass->isStringClass()) {
            return stringValue();
        }
        else if (aClass->isNumberClass()) {
            return numberValue();
        }
        else if (aClass->isBooleanClass()) {
            return booleanValue();
        }
    }
    
    return valueOf();
}

JSValue *JavaInstance::valueOf() const 
{
    return stringValue();
};

JObjectWrapper::JObjectWrapper(jobject instance)
{
    assert (instance != 0);

    _ref = 1;
    // Cache the JNIEnv used to get the global ref for this java instanace.
    // It'll be used to delete the reference.
    _env = getJNIEnv();
        
    _instance = _env->NewGlobalRef (instance);
    
	JS_LOG ("new global ref %p for %p\n", _instance, instance);
	
    if  (_instance == NULL) {
        fprintf (stderr, "%s:  could not get GlobalRef for %p\n", __PRETTY_FUNCTION__, instance);
    }
}

JObjectWrapper::~JObjectWrapper() {
	JS_LOG ("deleting global ref %p\n", _instance);
	_env->DeleteGlobalRef (_instance);
}
