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
#include <value.h>
#include <internal.h>

#include <jni_utility.h>
#include <jni_runtime.h>
 
using namespace KJS;
using namespace Bindings;


JavaField::JavaField (JNIEnv *env, jobject aField)
{
    // Get field type
    jobject fieldType = callJNIObjectMethod (aField, "getType", "()Ljava/lang/Class;");
    jstring fieldTypeName = (jstring)callJNIObjectMethod (fieldType, "toString", "()Ljava/lang/String;");
    _type = new JavaString(env, fieldTypeName);
    _primitiveType = primitiveTypeFromClassName (_type->characters());

    // Get field name
    jstring fieldName = (jstring)callJNIObjectMethod (aField, "getName", "()Ljava/lang/String;");
    _name = new JavaString(env, fieldName);
    
    _field = new JavaInstance(aField);
}

KJS::Value JavaField::valueFromInstance(const Instance *i) const 
{
    const JavaInstance *instance = static_cast<const JavaInstance *>(i);
    jobject jinstance = instance->javaInstance();
    jobject fieldJInstance = _field->javaInstance();

    switch (_primitiveType) {
        case object_type: {
            //jobject value = callJNIObjectMethod(_field->javaInstance(), "get", "(Ljava/lang/Object;)Ljava/lang/Object;", jinstace);
            return KJS::Value(0);
        }
        break;
            
        case boolean_type: {
            jboolean value = callJNIBooleanMethod(fieldJInstance, "getBoolean", "(Ljava/lang/Object;)B", jinstance);
            return KJS::Boolean((bool)value);
        }
        break;
            
        case byte_type:
        case char_type:
        case short_type:
        case int_type:
        case long_type:
        case float_type:
        case double_type: {
            jdouble value;
            value = callJNIDoubleMethod(fieldJInstance, "getDouble", "(Ljava/lang/Object;)D", jinstance);
            return Number((double)value);
        }
        break;
        default:
        break;
    }
    return Undefined();
}


JavaConstructor::JavaConstructor (JNIEnv *env, jobject aConstructor)
{
    // Get parameters
    jarray jparameters = (jarray)callJNIObjectMethod (aConstructor, "getParameterTypes", "()[Ljava/lang/Class;");
    _numParameters = env->GetArrayLength (jparameters);
    _parameters = new JavaParameter[_numParameters];
    
    int i;
    for (i = 0; i < _numParameters; i++) {
        jobject aParameter = env->GetObjectArrayElement ((jobjectArray)jparameters, i);
        jstring parameterName = (jstring)callJNIObjectMethod (aParameter, "getName", "()Ljava/lang/String;");
        _parameters[i] = JavaParameter(env, parameterName);
    }
}

JavaMethod::JavaMethod (JNIEnv *env, jobject aMethod)
{
    // Get parameters
    jarray jparameters = (jarray)callJNIObjectMethod (aMethod, "getParameterTypes", "()[Ljava/lang/Class;");
    _numParameters = env->GetArrayLength (jparameters);
    _parameters = new JavaParameter[_numParameters];
    
    int i;
    for (i = 0; i < _numParameters; i++) {
        jobject aParameter = env->GetObjectArrayElement ((jobjectArray)jparameters, i);
        jstring parameterName = (jstring)callJNIObjectMethod (aParameter, "getName", "()Ljava/lang/String;");
        _parameters[i] = JavaParameter(env, parameterName);
    }

    // Get return type
    jobject returnType = callJNIObjectMethod (aMethod, "getReturnType", "()Ljava/lang/Class;");
    jstring returnTypeName = (jstring)callJNIObjectMethod (returnType, "getName", "()Ljava/lang/String;");
    _returnType = new JavaString (env, returnTypeName);

    // Get method name
    jstring methodName = (jstring)callJNIObjectMethod (aMethod, "getName", "()Ljava/lang/String;");
    _name = new JavaString (env, methodName);
}
