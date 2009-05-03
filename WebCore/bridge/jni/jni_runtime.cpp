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
#include <jni_runtime.h>

#if ENABLE(MAC_JAVA_BRIDGE)

#include <jni_utility.h>

#include "runtime_array.h"
#include "runtime_object.h"
#include "runtime_root.h"
#include <runtime/Error.h>
#include <runtime/JSLock.h>

#ifdef NDEBUG
#define JS_LOG(formatAndArgs...) ((void)0)
#else
#define JS_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:%d -- %s:  ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}
#endif

using namespace JSC;
using namespace JSC::Bindings;


JavaParameter::JavaParameter (JNIEnv *env, jstring type)
{
    _type = JavaString (env, type);
    _JNIType = JNITypeFromClassName (_type.UTF8String());
}

JavaField::JavaField (JNIEnv *env, jobject aField)
{
    // Get field type
    jobject fieldType = callJNIMethod<jobject>(aField, "getType", "()Ljava/lang/Class;");
    jstring fieldTypeName = (jstring)callJNIMethod<jobject>(fieldType, "getName", "()Ljava/lang/String;");
    _type = JavaString(env, fieldTypeName);
    _JNIType = JNITypeFromClassName (_type.UTF8String());

    // Get field name
    jstring fieldName = (jstring)callJNIMethod<jobject>(aField, "getName", "()Ljava/lang/String;");
    _name = JavaString(env, fieldName);

    _field = new JObjectWrapper(aField);
}

JSValue JavaArray::convertJObjectToArray(ExecState* exec, jobject anObject, const char* type, PassRefPtr<RootObject> rootObject)
{
    if (type[0] != '[')
        return jsUndefined();

    return new (exec) RuntimeArray(exec, new JavaArray((jobject)anObject, type, rootObject));
}

jvalue JavaField::dispatchValueFromInstance(ExecState *exec, const JavaInstance *instance, const char *name, const char *sig, JNIType returnType) const
{
    jobject jinstance = instance->javaInstance();
    jobject fieldJInstance = _field->_instance;
    JNIEnv *env = getJNIEnv();
    jvalue result;

    bzero (&result, sizeof(jvalue));
    jclass cls = env->GetObjectClass(fieldJInstance);
    if ( cls != NULL ) {
        jmethodID mid = env->GetMethodID(cls, name, sig);
        if ( mid != NULL )
        {
            RootObject* rootObject = instance->rootObject();
            if (rootObject && rootObject->nativeHandle()) {
                JSValue exceptionDescription;
                jvalue args[1];
                
                args[0].l = jinstance;
                dispatchJNICall(exec, rootObject->nativeHandle(), fieldJInstance, false, returnType, mid, args, result, 0, exceptionDescription);
                if (exceptionDescription)
                    throwError(exec, GeneralError, exceptionDescription.toString(exec));
            }
        }
    }
    return result;
}

JSValue JavaField::valueFromInstance(ExecState* exec, const Instance* i) const 
{
    const JavaInstance *instance = static_cast<const JavaInstance *>(i);

    JSValue jsresult = jsUndefined();
    
    switch (_JNIType) {
        case array_type:
        case object_type: {
            jvalue result = dispatchValueFromInstance (exec, instance, "get", "(Ljava/lang/Object;)Ljava/lang/Object;", object_type);
            jobject anObject = result.l;

            const char *arrayType = type();
            if (arrayType[0] == '[') {
                jsresult = JavaArray::convertJObjectToArray(exec, anObject, arrayType, instance->rootObject());
            }
            else if (anObject != 0){
                jsresult = JavaInstance::create(anObject, instance->rootObject())->createRuntimeObject(exec);
            }
        }
        break;
            
        case boolean_type:
            jsresult = jsBoolean(dispatchValueFromInstance(exec, instance, "getBoolean", "(Ljava/lang/Object;)Z", boolean_type).z);
            break;
            
        case byte_type:
        case char_type:
        case short_type:
        
        case int_type: {
            jint value;
            jvalue result = dispatchValueFromInstance (exec, instance, "getInt", "(Ljava/lang/Object;)I", int_type);
            value = result.i;
            jsresult = jsNumber(exec, (int)value);
        }
        break;

        case long_type:
        case float_type:
        case double_type: {
            jdouble value;
            jvalue result = dispatchValueFromInstance (exec, instance, "getDouble", "(Ljava/lang/Object;)D", double_type);
            value = result.i;
            jsresult = jsNumber(exec, (double)value);
        }
        break;
        default:
        break;
    }

    JS_LOG ("getting %s = %s\n", UString(name()).UTF8String().c_str(), jsresult.toString(exec).ascii());
    
    return jsresult;
}

void JavaField::dispatchSetValueToInstance(ExecState *exec, const JavaInstance *instance, jvalue javaValue, const char *name, const char *sig) const
{
    jobject jinstance = instance->javaInstance();
    jobject fieldJInstance = _field->_instance;
    JNIEnv *env = getJNIEnv();

    jclass cls = env->GetObjectClass(fieldJInstance);
    if ( cls != NULL ) {
        jmethodID mid = env->GetMethodID(cls, name, sig);
        if ( mid != NULL )
        {
            RootObject* rootObject = instance->rootObject();
            if (rootObject && rootObject->nativeHandle()) {
                JSValue exceptionDescription;
                jvalue args[2];
                jvalue result;
                
                args[0].l = jinstance;
                args[1] = javaValue;
                dispatchJNICall(exec, rootObject->nativeHandle(), fieldJInstance, false, void_type, mid, args, result, 0, exceptionDescription);
                if (exceptionDescription)
                    throwError(exec, GeneralError, exceptionDescription.toString(exec));
            }
        }
    }
}

void JavaField::setValueToInstance(ExecState* exec, const Instance* i, JSValue aValue) const
{
    const JavaInstance *instance = static_cast<const JavaInstance *>(i);
    jvalue javaValue = convertValueToJValue (exec, aValue, _JNIType, type());

    JS_LOG ("setting value %s to %s\n", UString(name()).UTF8String().c_str(), aValue.toString(exec).ascii());

    switch (_JNIType) {
        case array_type:
        case object_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "set", "(Ljava/lang/Object;Ljava/lang/Object;)V");
        }
        break;
            
        case boolean_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setBoolean", "(Ljava/lang/Object;Z)V");
        }
        break;
            
        case byte_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setByte", "(Ljava/lang/Object;B)V");
        }
        break;

        case char_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setChar", "(Ljava/lang/Object;C)V");
        }
        break;

        case short_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setShort", "(Ljava/lang/Object;S)V");
        }
        break;

        case int_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setInt", "(Ljava/lang/Object;I)V");
        }
        break;

        case long_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setLong", "(Ljava/lang/Object;J)V");
        }
        break;

        case float_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setFloat", "(Ljava/lang/Object;F)V");
        }
        break;

        case double_type: {
            dispatchSetValueToInstance (exec, instance, javaValue, "setDouble", "(Ljava/lang/Object;D)V");
        }
        break;
        default:
        break;
    }
}

JavaMethod::JavaMethod (JNIEnv *env, jobject aMethod)
{
    // Get return type
    jobject returnType = callJNIMethod<jobject>(aMethod, "getReturnType", "()Ljava/lang/Class;");
    jstring returnTypeName = (jstring)callJNIMethod<jobject>(returnType, "getName", "()Ljava/lang/String;");
    _returnType =JavaString (env, returnTypeName);
    _JNIReturnType = JNITypeFromClassName (_returnType.UTF8String());
    env->DeleteLocalRef (returnType);
    env->DeleteLocalRef (returnTypeName);

    // Get method name
    jstring methodName = (jstring)callJNIMethod<jobject>(aMethod, "getName", "()Ljava/lang/String;");
    _name = JavaString (env, methodName);
    env->DeleteLocalRef (methodName);

    // Get parameters
    jarray jparameters = (jarray)callJNIMethod<jobject>(aMethod, "getParameterTypes", "()[Ljava/lang/Class;");
    _numParameters = env->GetArrayLength (jparameters);
    _parameters = new JavaParameter[_numParameters];
    
    int i;
    for (i = 0; i < _numParameters; i++) {
        jobject aParameter = env->GetObjectArrayElement ((jobjectArray)jparameters, i);
        jstring parameterName = (jstring)callJNIMethod<jobject>(aParameter, "getName", "()Ljava/lang/String;");
        _parameters[i] = JavaParameter(env, parameterName);
        env->DeleteLocalRef (aParameter);
        env->DeleteLocalRef (parameterName);
    }
    env->DeleteLocalRef (jparameters);

    // Created lazily.
    _signature = 0;
    _methodID = 0;
    
    jclass modifierClass = env->FindClass("java/lang/reflect/Modifier");
    int modifiers = callJNIMethod<jint>(aMethod, "getModifiers", "()I");
    _isStatic = (bool)callJNIStaticMethod<jboolean>(modifierClass, "isStatic", "(I)Z", modifiers);
}

JavaMethod::~JavaMethod() 
{
    if (_signature)
        free(_signature);
    delete [] _parameters;
};

// JNI method signatures use '/' between components of a class name, but
// we get '.' between components from the reflection API.
static void appendClassName(UString& aString, const char* className)
{
    ASSERT(JSLock::lockCount() > 0);
    
    char *result, *cp = strdup(className);
    
    result = cp;
    while (*cp) {
        if (*cp == '.')
            *cp = '/';
        cp++;
    }
        
    aString.append(result);

    free (result);
}

const char *JavaMethod::signature() const 
{
    if (!_signature) {
        JSLock lock(false);

        UString signatureBuilder("(");
        for (int i = 0; i < _numParameters; i++) {
            JavaParameter* aParameter = parameterAt(i);
            JNIType _JNIType = aParameter->getJNIType();
            if (_JNIType == array_type)
                appendClassName(signatureBuilder, aParameter->type());
            else {
                signatureBuilder.append(signatureFromPrimitiveType(_JNIType));
                if (_JNIType == object_type) {
                    appendClassName(signatureBuilder, aParameter->type());
                    signatureBuilder.append(";");
                }
            }
        }
        signatureBuilder.append(")");
        
        const char *returnType = _returnType.UTF8String();
        if (_JNIReturnType == array_type) {
            appendClassName(signatureBuilder, returnType);
        } else {
            signatureBuilder.append(signatureFromPrimitiveType(_JNIReturnType));
            if (_JNIReturnType == object_type) {
                appendClassName(signatureBuilder, returnType);
                signatureBuilder.append(";");
            }
        }
        
        _signature = strdup(signatureBuilder.ascii());
    }
    
    return _signature;
}

JNIType JavaMethod::JNIReturnType() const
{
    return _JNIReturnType;
}

jmethodID JavaMethod::methodID (jobject obj) const
{
    if (_methodID == 0) {
        _methodID = getMethodID (obj, _name.UTF8String(), signature());
    }
    return _methodID;
}


JavaArray::JavaArray(jobject array, const char* type, PassRefPtr<RootObject> rootObject)
    : Array(rootObject)
{
    _array = new JObjectWrapper(array);
    // Java array are fixed length, so we can cache length.
    JNIEnv *env = getJNIEnv();
    _length = env->GetArrayLength((jarray)_array->_instance);
    _type = strdup(type);
    _rootObject = rootObject;
}

JavaArray::~JavaArray () 
{
    free ((void *)_type);
}

RootObject* JavaArray::rootObject() const 
{ 
    return _rootObject && _rootObject->isValid() ? _rootObject.get() : 0;
}

void JavaArray::setValueAt(ExecState* exec, unsigned index, JSValue aValue) const
{
    JNIEnv *env = getJNIEnv();
    char *javaClassName = 0;
    
    JNIType arrayType = JNITypeFromPrimitiveType(_type[1]);
    if (_type[1] == 'L'){
        // The type of the array will be something like:
        // "[Ljava.lang.string;".  This is guaranteed, so no need
        // for extra sanity checks.
        javaClassName = strdup(&_type[2]);
        javaClassName[strchr(javaClassName, ';')-javaClassName] = 0;
    }
    jvalue aJValue = convertValueToJValue (exec, aValue, arrayType, javaClassName);
    
    switch (arrayType) {
        case object_type: {
            env->SetObjectArrayElement((jobjectArray)javaArray(), index, aJValue.l);
            break;
        }
            
        case boolean_type: {
            env->SetBooleanArrayRegion((jbooleanArray)javaArray(), index, 1, &aJValue.z);
            break;
        }
            
        case byte_type: {
            env->SetByteArrayRegion((jbyteArray)javaArray(), index, 1, &aJValue.b);
            break;
        }
            
        case char_type: {
            env->SetCharArrayRegion((jcharArray)javaArray(), index, 1, &aJValue.c);
            break;
        }
            
        case short_type: {
            env->SetShortArrayRegion((jshortArray)javaArray(), index, 1, &aJValue.s);
            break;
        }
            
        case int_type: {
            env->SetIntArrayRegion((jintArray)javaArray(), index, 1, &aJValue.i);
            break;
        }
            
        case long_type: {
            env->SetLongArrayRegion((jlongArray)javaArray(), index, 1, &aJValue.j);
        }
            
        case float_type: {
            env->SetFloatArrayRegion((jfloatArray)javaArray(), index, 1, &aJValue.f);
            break;
        }
            
        case double_type: {
            env->SetDoubleArrayRegion((jdoubleArray)javaArray(), index, 1, &aJValue.d);
            break;
        }
        default:
        break;
    }
    
    if (javaClassName)
        free ((void *)javaClassName);
}


JSValue JavaArray::valueAt(ExecState* exec, unsigned index) const
{
    JNIEnv *env = getJNIEnv();
    JNIType arrayType = JNITypeFromPrimitiveType(_type[1]);
    switch (arrayType) {
        case object_type: {
            jobjectArray objectArray = (jobjectArray)javaArray();
            jobject anObject;
            anObject = env->GetObjectArrayElement(objectArray, index);

            // No object?
            if (!anObject) {
                return jsNull();
            }
            
            // Nested array?
            if (_type[1] == '[') {
                return JavaArray::convertJObjectToArray(exec, anObject, _type+1, rootObject());
            }
            // or array of other object type?
            return JavaInstance::create(anObject, rootObject())->createRuntimeObject(exec);
        }
            
        case boolean_type: {
            jbooleanArray booleanArray = (jbooleanArray)javaArray();
            jboolean aBoolean;
            env->GetBooleanArrayRegion(booleanArray, index, 1, &aBoolean);
            return jsBoolean(aBoolean);
        }
            
        case byte_type: {
            jbyteArray byteArray = (jbyteArray)javaArray();
            jbyte aByte;
            env->GetByteArrayRegion(byteArray, index, 1, &aByte);
            return jsNumber(exec, aByte);
        }
            
        case char_type: {
            jcharArray charArray = (jcharArray)javaArray();
            jchar aChar;
            env->GetCharArrayRegion(charArray, index, 1, &aChar);
            return jsNumber(exec, aChar);
            break;
        }
            
        case short_type: {
            jshortArray shortArray = (jshortArray)javaArray();
            jshort aShort;
            env->GetShortArrayRegion(shortArray, index, 1, &aShort);
            return jsNumber(exec, aShort);
        }
            
        case int_type: {
            jintArray intArray = (jintArray)javaArray();
            jint anInt;
            env->GetIntArrayRegion(intArray, index, 1, &anInt);
            return jsNumber(exec, anInt);
        }
            
        case long_type: {
            jlongArray longArray = (jlongArray)javaArray();
            jlong aLong;
            env->GetLongArrayRegion(longArray, index, 1, &aLong);
            return jsNumber(exec, aLong);
        }
            
        case float_type: {
            jfloatArray floatArray = (jfloatArray)javaArray();
            jfloat aFloat;
            env->GetFloatArrayRegion(floatArray, index, 1, &aFloat);
            return jsNumber(exec, aFloat);
        }
            
        case double_type: {
            jdoubleArray doubleArray = (jdoubleArray)javaArray();
            jdouble aDouble;
            env->GetDoubleArrayRegion(doubleArray, index, 1, &aDouble);
            return jsNumber(exec, aDouble);
        }
        default:
        break;
    }
    return jsUndefined();
}

unsigned int JavaArray::getLength() const
{
    return _length;
}

#endif // ENABLE(MAC_JAVA_BRIDGE)
