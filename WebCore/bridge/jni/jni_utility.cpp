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
#include "jni_utility.h"

#if ENABLE(MAC_JAVA_BRIDGE)

#include "jni_runtime.h"
#include "runtime_array.h"
#include "runtime_object.h"
#include <runtime/JSArray.h>
#include <runtime/JSLock.h>
#include <dlfcn.h>

namespace JSC {

namespace Bindings {

static jint KJS_GetCreatedJavaVMs(JavaVM** vmBuf, jsize bufLen, jsize* nVMs)
{
    static void* javaVMFramework = 0;
    if (!javaVMFramework)
        javaVMFramework = dlopen("/System/Library/Frameworks/JavaVM.framework/JavaVM", RTLD_LAZY);
    if (!javaVMFramework)
        return JNI_ERR;

    static jint(*functionPointer)(JavaVM**, jsize, jsize *) = 0;
    if (!functionPointer)
        functionPointer = (jint(*)(JavaVM**, jsize, jsize *))dlsym(javaVMFramework, "JNI_GetCreatedJavaVMs");
    if (!functionPointer)
        return JNI_ERR;
    return functionPointer(vmBuf, bufLen, nVMs);
}

static JavaVM *jvm = 0;

// Provide the ability for an outside component to specify the JavaVM to use
// If the jvm value is set, the getJavaVM function below will just return. 
// In getJNIEnv(), if AttachCurrentThread is called to a VM that is already
// attached, the result is a no-op.
void setJavaVM(JavaVM *javaVM)
{
    jvm = javaVM;
}

JavaVM *getJavaVM()
{
    if (jvm)
        return jvm;

    JavaVM *jvmArray[1];
    jsize bufLen = 1;
    jsize nJVMs = 0;
    jint jniError = 0;

    // Assumes JVM is already running ..., one per process
    jniError = KJS_GetCreatedJavaVMs(jvmArray, bufLen, &nJVMs);
    if ( jniError == JNI_OK && nJVMs > 0 ) {
        jvm = jvmArray[0];
    }
    else 
        fprintf(stderr, "%s: JNI_GetCreatedJavaVMs failed, returned %ld\n", __PRETTY_FUNCTION__, (long)jniError);
        
    return jvm;
}

JNIEnv* getJNIEnv()
{
    union {
        JNIEnv* env;
        void* dummy;
    } u;
    jint jniError = 0;

    jniError = (getJavaVM())->AttachCurrentThread(&u.dummy, NULL);
    if (jniError == JNI_OK)
        return u.env;
    else
        fprintf(stderr, "%s: AttachCurrentThread failed, returned %ld\n", __PRETTY_FUNCTION__, (long)jniError);
    return NULL;
}

jmethodID getMethodID (jobject obj, const char *name, const char *sig)
{
    JNIEnv *env = getJNIEnv();
    jmethodID mid = 0;
        
    if ( env != NULL) {
    jclass cls = env->GetObjectClass(obj);
    if ( cls != NULL ) {
            mid = env->GetMethodID(cls, name, sig);
            if (!mid) {
                env->ExceptionClear();
                mid = env->GetStaticMethodID(cls, name, sig);
                if (!mid) {
                    env->ExceptionClear();
                }
            }
        }
        env->DeleteLocalRef(cls);
    }
    return mid;
}

const char *getCharactersFromJString (jstring aJString)
{
    return getCharactersFromJStringInEnv (getJNIEnv(), aJString);
}

void releaseCharactersForJString (jstring aJString, const char *s)
{
    releaseCharactersForJStringInEnv (getJNIEnv(), aJString, s);
}

const char *getCharactersFromJStringInEnv (JNIEnv *env, jstring aJString)
{
    jboolean isCopy;
    const char *s = env->GetStringUTFChars((jstring)aJString, &isCopy);
    if (!s) {
        env->ExceptionDescribe();
        env->ExceptionClear();
                fprintf (stderr, "\n");
    }
    return s;
}

void releaseCharactersForJStringInEnv (JNIEnv *env, jstring aJString, const char *s)
{
    env->ReleaseStringUTFChars (aJString, s);
}

const jchar *getUCharactersFromJStringInEnv (JNIEnv *env, jstring aJString)
{
    jboolean isCopy;
    const jchar *s = env->GetStringChars((jstring)aJString, &isCopy);
    if (!s) {
        env->ExceptionDescribe();
        env->ExceptionClear();
                fprintf (stderr, "\n");
    }
    return s;
}

void releaseUCharactersForJStringInEnv (JNIEnv *env, jstring aJString, const jchar *s)
{
    env->ReleaseStringChars (aJString, s);
}

JNIType JNITypeFromClassName(const char *name)
{
    JNIType type;
    
    if (strcmp("byte",name) == 0)
        type = byte_type;
    else if (strcmp("short",name) == 0)
        type = short_type;
    else if (strcmp("int",name) == 0)
        type = int_type;
    else if (strcmp("long",name) == 0)
        type = long_type;
    else if (strcmp("float",name) == 0)
        type = float_type;
    else if (strcmp("double",name) == 0)
        type = double_type;
    else if (strcmp("char",name) == 0)
        type = char_type;
    else if (strcmp("boolean",name) == 0)
        type = boolean_type;
    else if (strcmp("void",name) == 0)
        type = void_type;
    else if ('[' == name[0]) 
        type = array_type;
    else
        type = object_type;
        
    return type;
}

const char *signatureFromPrimitiveType(JNIType type)
{
    switch (type){
        case void_type: 
            return "V";
        
        case array_type:
            return "[";
        
        case object_type:
            return "L";
        
        case boolean_type:
            return "Z";
        
        case byte_type:
            return "B";
            
        case char_type:
            return "C";
        
        case short_type:
            return "S";
        
        case int_type:
            return "I";
        
        case long_type:
            return "J";
        
        case float_type:
            return "F";
        
        case double_type:
            return "D";

        case invalid_type:
        default:
        break;
    }
    return "";
}

JNIType JNITypeFromPrimitiveType(char type)
{
    switch (type){
        case 'V': 
            return void_type;
        
        case 'L':
            return object_type;
            
        case '[':
            return array_type;
        
        case 'Z':
            return boolean_type;
        
        case 'B':
            return byte_type;
            
        case 'C':
            return char_type;
        
        case 'S':
            return short_type;
        
        case 'I':
            return int_type;
        
        case 'J':
            return long_type;
        
        case 'F':
            return float_type;
        
        case 'D':
            return double_type;

        default:
        break;
    }
    return invalid_type;
}

jvalue getJNIField( jobject obj, JNIType type, const char *name, const char *signature)
{
    JavaVM *jvm = getJavaVM();
    JNIEnv *env = getJNIEnv();
    jvalue result;

    bzero (&result, sizeof(jvalue));
    if ( obj != NULL && jvm != NULL && env != NULL) {
        jclass cls = env->GetObjectClass(obj);
        if ( cls != NULL ) {
            jfieldID field = env->GetFieldID(cls, name, signature);
            if ( field != NULL ) {
                switch (type) {
                case array_type:
                case object_type:
                    result.l = env->functions->GetObjectField(env, obj, field);
                    break;
                case boolean_type:
                    result.z = env->functions->GetBooleanField(env, obj, field);
                    break;
                case byte_type:
                    result.b = env->functions->GetByteField(env, obj, field);
                    break;
                case char_type:
                    result.c = env->functions->GetCharField(env, obj, field);
                    break;
                case short_type:
                    result.s = env->functions->GetShortField(env, obj, field);
                    break;
                case int_type:
                    result.i = env->functions->GetIntField(env, obj, field);
                    break;
                case long_type:
                    result.j = env->functions->GetLongField(env, obj, field);
                    break;
                case float_type:
                    result.f = env->functions->GetFloatField(env, obj, field);
                    break;
                case double_type:
                    result.d = env->functions->GetDoubleField(env, obj, field);
                    break;
                default:
                    fprintf(stderr, "%s: invalid field type (%d)\n", __PRETTY_FUNCTION__, (int)type);
                }
            }
            else
            {
                fprintf(stderr, "%s: Could not find field: %s\n", __PRETTY_FUNCTION__, name);
                env->ExceptionDescribe();
                env->ExceptionClear();
                fprintf (stderr, "\n");
            }

            env->DeleteLocalRef(cls);
        }
        else {
            fprintf(stderr, "%s: Could not find class for object\n", __PRETTY_FUNCTION__);
        }
    }

    return result;
}

static jobject convertArrayInstanceToJavaArray(ExecState* exec, JSArray* jsArray, const char* javaClassName)
{
    JNIEnv *env = getJNIEnv();
    // As JS Arrays can contain a mixture of objects, assume we can convert to
    // the requested Java Array type requested, unless the array type is some object array
    // other than a string.
    unsigned length = jsArray->length();
    jobjectArray jarray = 0;
    
    // Build the correct array type
    switch (JNITypeFromPrimitiveType(javaClassName[1])) { 
        case object_type: {
        // Only support string object types
        if (0 == strcmp("[Ljava.lang.String;", javaClassName)) {
            jarray = (jobjectArray)env->NewObjectArray(length,
                env->FindClass("java/lang/String"),
                env->NewStringUTF(""));
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                UString stringValue = item.toString(exec);
                env->SetObjectArrayElement(jarray,i,
                    env->functions->NewString(env, (const jchar *)stringValue.data(), stringValue.size()));
                }
            }
            break;
        }
        
        case boolean_type: {
            jarray = (jobjectArray)env->NewBooleanArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                jboolean value = (jboolean)item.toNumber(exec);
                env->SetBooleanArrayRegion((jbooleanArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }
        
        case byte_type: {
            jarray = (jobjectArray)env->NewByteArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                jbyte value = (jbyte)item.toNumber(exec);
                env->SetByteArrayRegion((jbyteArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }

        case char_type: {
            jarray = (jobjectArray)env->NewCharArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                UString stringValue = item.toString(exec);
                jchar value = 0;
                if (stringValue.size() > 0)
                    value = ((const jchar*)stringValue.data())[0];
                env->SetCharArrayRegion((jcharArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }

        case short_type: {
            jarray = (jobjectArray)env->NewShortArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                jshort value = (jshort)item.toNumber(exec);
                env->SetShortArrayRegion((jshortArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }

        case int_type: {
            jarray = (jobjectArray)env->NewIntArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                jint value = (jint)item.toNumber(exec);
                env->SetIntArrayRegion((jintArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }

        case long_type: {
            jarray = (jobjectArray)env->NewLongArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                jlong value = (jlong)item.toNumber(exec);
                env->SetLongArrayRegion((jlongArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }

        case float_type: {
            jarray = (jobjectArray)env->NewFloatArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                jfloat value = (jfloat)item.toNumber(exec);
                env->SetFloatArrayRegion((jfloatArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }
    
        case double_type: {
            jarray = (jobjectArray)env->NewDoubleArray(length);
            for(unsigned i = 0; i < length; i++) {
                JSValue item = jsArray->get(exec, i);
                jdouble value = (jdouble)item.toNumber(exec);
                env->SetDoubleArrayRegion((jdoubleArray)jarray, (jsize)i, (jsize)1, &value);
            }
            break;
        }
        
        case array_type: // don't handle embedded arrays
        case void_type: // Don't expect arrays of void objects
        case invalid_type: // Array of unknown objects
            break;
    }
    
    // if it was not one of the cases handled, then null is returned
    return jarray;
}


jvalue convertValueToJValue(ExecState* exec, JSValue value, JNIType _JNIType, const char* javaClassName)
{
    JSLock lock(false);
    
    jvalue result;
   
    switch (_JNIType){
        case array_type:
        case object_type: {
            result.l = (jobject)0;
            
            // First see if we have a Java instance.
            if (value.isObject()){
                JSObject* objectImp = asObject(value);
                if (objectImp->classInfo() == &RuntimeObjectImp::s_info) {
                    RuntimeObjectImp* imp = static_cast<RuntimeObjectImp*>(objectImp);
                    JavaInstance *instance = static_cast<JavaInstance*>(imp->getInternalInstance());
                    if (instance)
                        result.l = instance->javaInstance();
                }
                else if (objectImp->classInfo() == &RuntimeArray::s_info) {
                // Input is a JavaScript Array that was originally created from a Java Array
                    RuntimeArray* imp = static_cast<RuntimeArray*>(objectImp);
                    JavaArray *array = static_cast<JavaArray*>(imp->getConcreteArray());
                    result.l = array->javaArray();
                } 
                else if (objectImp->classInfo() == &JSArray::info) {
                    // Input is a Javascript Array. We need to create it to a Java Array.
                    result.l = convertArrayInstanceToJavaArray(exec, asArray(value), javaClassName);
                }
            }
            
            // Now convert value to a string if the target type is a java.lang.string, and we're not
            // converting from a Null.
            if (result.l == 0 && strcmp(javaClassName, "java.lang.String") == 0) {
#ifdef CONVERT_NULL_TO_EMPTY_STRING
                if (value->isNull()) {
                    JNIEnv *env = getJNIEnv();
                    jchar buf[2];
                    jobject javaString = env->functions->NewString (env, buf, 0);
                    result.l = javaString;
                }
                else 
#else
                if (!value.isNull())
#endif
                {
                    UString stringValue = value.toString(exec);
                    JNIEnv *env = getJNIEnv();
                    jobject javaString = env->functions->NewString (env, (const jchar *)stringValue.data(), stringValue.size());
                    result.l = javaString;
                }
            } else if (result.l == 0) 
                bzero (&result, sizeof(jvalue)); // Handle it the same as a void case
        }
        break;
        
        case boolean_type: {
            result.z = (jboolean)value.toNumber(exec);
        }
        break;
            
        case byte_type: {
            result.b = (jbyte)value.toNumber(exec);
        }
        break;
        
        case char_type: {
            result.c = (jchar)value.toNumber(exec);
        }
        break;

        case short_type: {
            result.s = (jshort)value.toNumber(exec);
        }
        break;

        case int_type: {
            result.i = (jint)value.toNumber(exec);
        }
        break;

        case long_type: {
            result.j = (jlong)value.toNumber(exec);
        }
        break;

        case float_type: {
            result.f = (jfloat)value.toNumber(exec);
        }
        break;

        case double_type: {
            result.d = (jdouble)value.toNumber(exec);
        }
        break;
            
        break;

        case invalid_type:
        default:
        case void_type: {
            bzero (&result, sizeof(jvalue));
        }
        break;
    }
    return result;
}

}  // end of namespace Bindings

} // end of namespace JSC

#endif // ENABLE(MAC_JAVA_BRIDGE)
