/*
 * Copyright (C) 2003, 2004, 2005, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright 2010, The Android Open Source Project
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
#include "JNIBridgeJSC.h"

#if ENABLE(MAC_JAVA_BRIDGE)

#include "JNIUtilityPrivate.h"
#include "Logging.h"
#include "runtime_array.h"
#include "runtime_object.h"
#include <runtime/Error.h>

using namespace JSC;
using namespace JSC::Bindings;
using namespace WebCore;

JavaField::JavaField(JNIEnv* env, jobject aField)
{
    // Get field type name
    jstring fieldTypeName = 0;
    if (jobject fieldType = callJNIMethod<jobject>(aField, "getType", "()Ljava/lang/Class;"))
        fieldTypeName = static_cast<jstring>(callJNIMethod<jobject>(fieldType, "getName", "()Ljava/lang/String;"));
    if (!fieldTypeName)
        fieldTypeName = env->NewStringUTF("<Unknown>");
    m_type = JavaString(env, fieldTypeName);

    m_JNIType = JNITypeFromClassName(m_type.UTF8String());

    // Get field name
    jstring fieldName = static_cast<jstring>(callJNIMethod<jobject>(aField, "getName", "()Ljava/lang/String;"));
    if (!fieldName)
        fieldName = env->NewStringUTF("<Unknown>");
    m_name = JavaString(env, fieldName);

    m_field = new JObjectWrapper(aField);
}

JSValue JavaArray::convertJObjectToArray(ExecState* exec, jobject anObject, const char* type, PassRefPtr<RootObject> rootObject)
{
    if (type[0] != '[')
        return jsUndefined();

    return new (exec) RuntimeArray(exec, new JavaArray(anObject, type, rootObject));
}

jvalue JavaField::dispatchValueFromInstance(ExecState* exec, const JavaInstance* instance, const char* name, const char* sig, JNIType returnType) const
{
    jobject jinstance = instance->javaInstance();
    jobject fieldJInstance = m_field->m_instance;
    JNIEnv* env = getJNIEnv();
    jvalue result;

    memset(&result, 0, sizeof(jvalue));
    jclass cls = env->GetObjectClass(fieldJInstance);
    if (cls) {
        jmethodID mid = env->GetMethodID(cls, name, sig);
        if (mid) {
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
    const JavaInstance* instance = static_cast<const JavaInstance*>(i);

    JSValue jsresult = jsUndefined();

    switch (m_JNIType) {
    case array_type:
    case object_type:
        {
            jvalue result = dispatchValueFromInstance(exec, instance, "get", "(Ljava/lang/Object;)Ljava/lang/Object;", object_type);
            jobject anObject = result.l;

            if (!anObject)
                return jsNull();

            const char* arrayType = type();
            if (arrayType[0] == '[')
                jsresult = JavaArray::convertJObjectToArray(exec, anObject, arrayType, instance->rootObject());
            else if (anObject)
                jsresult = JavaInstance::create(anObject, instance->rootObject())->createRuntimeObject(exec);
        }
        break;

    case boolean_type:
        jsresult = jsBoolean(dispatchValueFromInstance(exec, instance, "getBoolean", "(Ljava/lang/Object;)Z", boolean_type).z);
        break;

    case byte_type:
    case char_type:
    case short_type:

    case int_type:
        {
            jint value;
            jvalue result = dispatchValueFromInstance(exec, instance, "getInt", "(Ljava/lang/Object;)I", int_type);
            value = result.i;
            jsresult = jsNumber(exec, static_cast<int>(value));
        }
        break;

    case long_type:
    case float_type:
    case double_type:
        {
            jdouble value;
            jvalue result = dispatchValueFromInstance(exec, instance, "getDouble", "(Ljava/lang/Object;)D", double_type);
            value = result.i;
            jsresult = jsNumber(exec, static_cast<double>(value));
        }
        break;
    default:
        break;
    }

    LOG(LiveConnect, "JavaField::valueFromInstance getting %s = %s", UString(name()).UTF8String().c_str(), jsresult.toString(exec).ascii());

    return jsresult;
}

void JavaField::dispatchSetValueToInstance(ExecState* exec, const JavaInstance* instance, jvalue javaValue, const char* name, const char* sig) const
{
    jobject jinstance = instance->javaInstance();
    jobject fieldJInstance = m_field->m_instance;
    JNIEnv* env = getJNIEnv();

    jclass cls = env->GetObjectClass(fieldJInstance);
    if (cls) {
        jmethodID mid = env->GetMethodID(cls, name, sig);
        if (mid) {
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
    const JavaInstance* instance = static_cast<const JavaInstance*>(i);
    jvalue javaValue = convertValueToJValue(exec, i->rootObject(), aValue, m_JNIType, type());

    LOG(LiveConnect, "JavaField::setValueToInstance setting value %s to %s", UString(name()).UTF8String().c_str(), aValue.toString(exec).ascii());

    switch (m_JNIType) {
    case array_type:
    case object_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "set", "(Ljava/lang/Object;Ljava/lang/Object;)V");
        }
        break;

    case boolean_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setBoolean", "(Ljava/lang/Object;Z)V");
        }
        break;

    case byte_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setByte", "(Ljava/lang/Object;B)V");
        }
        break;

    case char_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setChar", "(Ljava/lang/Object;C)V");
        }
        break;

    case short_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setShort", "(Ljava/lang/Object;S)V");
        }
        break;

    case int_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setInt", "(Ljava/lang/Object;I)V");
        }
        break;

    case long_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setLong", "(Ljava/lang/Object;J)V");
        }
        break;

    case float_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setFloat", "(Ljava/lang/Object;F)V");
        }
        break;

    case double_type:
        {
            dispatchSetValueToInstance(exec, instance, javaValue, "setDouble", "(Ljava/lang/Object;D)V");
        }
        break;
    default:
        break;
    }
}

JavaArray::JavaArray(jobject array, const char* type, PassRefPtr<RootObject> rootObject)
    : Array(rootObject)
{
    m_array = new JObjectWrapper(array);
    // Java array are fixed length, so we can cache length.
    JNIEnv* env = getJNIEnv();
    m_length = env->GetArrayLength(static_cast<jarray>(m_array->m_instance));
    m_type = strdup(type);
}

JavaArray::~JavaArray()
{
    free(const_cast<char*>(m_type));
}

RootObject* JavaArray::rootObject() const
{
    return m_rootObject && m_rootObject->isValid() ? m_rootObject.get() : 0;
}

void JavaArray::setValueAt(ExecState* exec, unsigned index, JSValue aValue) const
{
    JNIEnv* env = getJNIEnv();
    char* javaClassName = 0;

    JNIType arrayType = JNITypeFromPrimitiveType(m_type[1]);
    if (m_type[1] == 'L') {
        // The type of the array will be something like:
        // "[Ljava.lang.string;".  This is guaranteed, so no need
        // for extra sanity checks.
        javaClassName = strdup(&m_type[2]);
        javaClassName[strchr(javaClassName, ';')-javaClassName] = 0;
    }
    jvalue aJValue = convertValueToJValue(exec, m_rootObject.get(), aValue, arrayType, javaClassName);

    switch (arrayType) {
    case object_type:
        {
            env->SetObjectArrayElement(static_cast<jobjectArray>(javaArray()), index, aJValue.l);
            break;
        }

    case boolean_type:
        {
            env->SetBooleanArrayRegion(static_cast<jbooleanArray>(javaArray()), index, 1, &aJValue.z);
            break;
        }

    case byte_type:
        {
            env->SetByteArrayRegion(static_cast<jbyteArray>(javaArray()), index, 1, &aJValue.b);
            break;
        }

    case char_type:
        {
            env->SetCharArrayRegion(static_cast<jcharArray>(javaArray()), index, 1, &aJValue.c);
            break;
        }

    case short_type:
        {
            env->SetShortArrayRegion(static_cast<jshortArray>(javaArray()), index, 1, &aJValue.s);
            break;
        }

    case int_type:
        {
            env->SetIntArrayRegion(static_cast<jintArray>(javaArray()), index, 1, &aJValue.i);
            break;
        }

    case long_type:
        {
            env->SetLongArrayRegion(static_cast<jlongArray>(javaArray()), index, 1, &aJValue.j);
        }

    case float_type:
        {
            env->SetFloatArrayRegion(static_cast<jfloatArray>(javaArray()), index, 1, &aJValue.f);
            break;
        }

    case double_type:
        {
            env->SetDoubleArrayRegion(static_cast<jdoubleArray>(javaArray()), index, 1, &aJValue.d);
            break;
        }
    default:
        break;
    }

    if (javaClassName)
        free(const_cast<char*>(javaClassName));
}


JSValue JavaArray::valueAt(ExecState* exec, unsigned index) const
{
    JNIEnv* env = getJNIEnv();
    JNIType arrayType = JNITypeFromPrimitiveType(m_type[1]);
    switch (arrayType) {
    case object_type:
        {
            jobjectArray objectArray = static_cast<jobjectArray>(javaArray());
            jobject anObject;
            anObject = env->GetObjectArrayElement(objectArray, index);

            // No object?
            if (!anObject)
                return jsNull();

            // Nested array?
            if (m_type[1] == '[')
                return JavaArray::convertJObjectToArray(exec, anObject, m_type + 1, rootObject());
            // or array of other object type?
            return JavaInstance::create(anObject, rootObject())->createRuntimeObject(exec);
        }

    case boolean_type:
        {
            jbooleanArray booleanArray = static_cast<jbooleanArray>(javaArray());
            jboolean aBoolean;
            env->GetBooleanArrayRegion(booleanArray, index, 1, &aBoolean);
            return jsBoolean(aBoolean);
        }

    case byte_type:
        {
            jbyteArray byteArray = static_cast<jbyteArray>(javaArray());
            jbyte aByte;
            env->GetByteArrayRegion(byteArray, index, 1, &aByte);
            return jsNumber(exec, aByte);
        }

    case char_type:
        {
            jcharArray charArray = static_cast<jcharArray>(javaArray());
            jchar aChar;
            env->GetCharArrayRegion(charArray, index, 1, &aChar);
            return jsNumber(exec, aChar);
            break;
        }

    case short_type:
        {
            jshortArray shortArray = static_cast<jshortArray>(javaArray());
            jshort aShort;
            env->GetShortArrayRegion(shortArray, index, 1, &aShort);
            return jsNumber(exec, aShort);
        }

    case int_type:
        {
            jintArray intArray = static_cast<jintArray>(javaArray());
            jint anInt;
            env->GetIntArrayRegion(intArray, index, 1, &anInt);
            return jsNumber(exec, anInt);
        }

    case long_type:
        {
            jlongArray longArray = static_cast<jlongArray>(javaArray());
            jlong aLong;
            env->GetLongArrayRegion(longArray, index, 1, &aLong);
            return jsNumber(exec, aLong);
        }

    case float_type:
        {
            jfloatArray floatArray = static_cast<jfloatArray>(javaArray());
            jfloat aFloat;
            env->GetFloatArrayRegion(floatArray, index, 1, &aFloat);
            return jsNumber(exec, aFloat);
        }

    case double_type:
        {
            jdoubleArray doubleArray = static_cast<jdoubleArray>(javaArray());
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
    return m_length;
}

#endif // ENABLE(MAC_JAVA_BRIDGE)
