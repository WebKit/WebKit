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
#include <jni_utility.h>
#include <jni_runtime.h>
 
using namespace Bindings;

JavaField::JavaField (JNIEnv *env, jobject aField)
{
    // Get field type
    jobject fieldType = callJNIObjectMethod (aField, "getType", "()Ljava/lang/Class;");
    jstring fieldTypeName = (jstring)callJNIObjectMethod (fieldType, "toString", "()Ljava/lang/String;");
    _type = new JavaString(env, fieldTypeName);

    // Get field name
    jstring fieldName = (jstring)callJNIObjectMethod (aField, "getName", "()Ljava/lang/String;");
    _name = new JavaString(env, fieldName);
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

JavaClass::JavaClass (JNIEnv *env, const char *className)
{
    long i;

    _name = strdup (className);
    
    // Get the class
    jclass aClass = env->FindClass(_name);
    if (!aClass){   
        fprintf (stderr, "%s:  unable to find class %s\n", __PRETTY_FUNCTION__, _name);
        return;
    }

    // Get the fields
    jarray fields = (jarray)callJNIObjectMethod (aClass, "getFields", "()[Ljava/lang/reflect/Field;");
    _numFields = env->GetArrayLength (fields);    
    _fields = new JavaField[_numFields];
    for (i = 0; i < _numFields; i++) {
        jobject aField = env->GetObjectArrayElement ((jobjectArray)fields, i);
        _fields[i] = JavaField (env, aField);
    }
    
    // Get the methods
    jarray methods = (jarray)callJNIObjectMethod (aClass, "getMethods", "()[Ljava/lang/reflect/Method;");
    _numMethods = env->GetArrayLength (methods);    
    _methods = new JavaMethod[_numMethods];
    for (i = 0; i < _numMethods; i++) {
        jobject aMethod = env->GetObjectArrayElement ((jobjectArray)methods, i);
        _methods[i] = JavaMethod (env, aMethod);
    }

    // Get the constructors
    jarray constructors = (jarray)callJNIObjectMethod (aClass, "getConstructors", "()[Ljava/lang/reflect/Constructor;");
    _numConstructors = env->GetArrayLength (constructors);    
    _constructors = new JavaConstructor[_numConstructors];
    for (i = 0; i < _numConstructors; i++) {
        jobject aConstructor = env->GetObjectArrayElement ((jobjectArray)constructors, i);
        _constructors[i] = JavaConstructor (env, aConstructor);
    }
}
