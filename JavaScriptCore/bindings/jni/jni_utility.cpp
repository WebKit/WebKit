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
 
#include "jni_utility.h"


static bool attachToJavaVM(JavaVM **jvm, JNIEnv **env)
{
    JavaVM *jvmArray[1];
    jsize bufLen = 1;
    jsize nJVMs = 0;
    jint jniError = 0;
    bool attached = false;

    // if a JVM is already running ...
    jniError = JNI_GetCreatedJavaVMs(jvmArray, bufLen, &nJVMs);
    if ( jniError == JNI_OK )
    {
        // how many JVMs?
        if ( nJVMs > 0 )
        {
            // connect to the Java VM
            // there can be only one per process  -- mhay  2002.09.30
            *jvm = jvmArray[0];
            jniError = (*jvm)->AttachCurrentThread((void**)env, (void *)NULL);
            if ( jniError == JNI_OK )
                attached = true;
            else
                fprintf(stderr, "%s: AttachCurrentThread failed, returned %d", __PRETTY_FUNCTION__, jniError);
        }
    }
    else 
        fprintf(stderr, "%s: JNI_GetCreatedJavaVMs failed, returned %d", __PRETTY_FUNCTION__, jniError);

    return attached;
}

typedef enum {
    void_function,
    object_function,
    boolean_function,
    byte_function,
    char_function,
    short_function,
    int_function,
    long_function,
    float_function,
    double_function
} JNIFunctionType;


static jvalue callJNIMethod( JNIFunctionType type, jobject obj, const char *name, const char *sig, va_list args)
{
    JavaVM *jvm = NULL;
    JNIEnv *env = NULL;
    jvalue result;

    if ( obj != NULL ) {
        if ( attachToJavaVM(&jvm, &env) ) {
            jclass cls = env->GetObjectClass(obj);
            if ( cls != NULL ) {
                jmethodID mid = env->GetMethodID(cls, name, sig);
                if ( mid != NULL )
                {
                    switch (type) {
                    case void_function:
                        env->functions->CallVoidMethodV(env, obj, mid, args);
                        break;
                    case object_function:
                        result.l = env->functions->CallObjectMethodV(env, obj, mid, args);
                        break;
                    case boolean_function:
                        result.z = env->functions->CallBooleanMethodV(env, obj, mid, args);
                        break;
                    case byte_function:
                        result.b = env->functions->CallByteMethodV(env, obj, mid, args);
                        break;
                    case char_function:
                        result.c = env->functions->CallCharMethodV(env, obj, mid, args);
                        break;
                    case short_function:
                        result.s = env->functions->CallShortMethodV(env, obj, mid, args);
                        break;
                    case int_function:
                        result.i = env->functions->CallIntMethodV(env, obj, mid, args);
                        break;
                    case long_function:
                        result.j = env->functions->CallLongMethodV(env, obj, mid, args);
                        break;
                    case float_function:
                        result.f = env->functions->CallFloatMethodV(env, obj, mid, args);
                        break;
                    case double_function:
                        result.d = env->functions->CallDoubleMethodV(env, obj, mid, args);
                        break;
                    default:
                        fprintf(stderr, "%s: invalid function type (%d)", __PRETTY_FUNCTION__, (int)type);
                    }
                }
                else
                {
                    fprintf(stderr, "%s: Could not find method: %s!", __PRETTY_FUNCTION__, name);
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }

                env->DeleteLocalRef(cls);
            }
            else {
                fprintf(stderr, "%s: Could not find class for object!", __PRETTY_FUNCTION__);
            }
        }
        else {
            fprintf(stderr, "%s: Could not attach to the VM!", __PRETTY_FUNCTION__);
        }
    }

    return result;
}

static jvalue callJNIMethodA( JNIFunctionType type, jobject obj, const char *name, const char *sig, jvalue *args)
{
    JavaVM *jvm = NULL;
    JNIEnv *env = NULL;
    jvalue result;
    
    if ( obj != NULL ) {
        if ( attachToJavaVM(&jvm, &env) ) {
            jclass cls = env->GetObjectClass(obj);
            if ( cls != NULL ) {
                jmethodID mid = env->GetMethodID(cls, name, sig);
                if ( mid != NULL )
                {
                    switch (type) {
                    case void_function:
                        env->functions->CallVoidMethodA(env, obj, mid, args);
                        break;
                    case object_function:
                        result.l = env->functions->CallObjectMethodA(env, obj, mid, args);
                        break;
                    case boolean_function:
                        result.z = env->functions->CallBooleanMethodA(env, obj, mid, args);
                        break;
                    case byte_function:
                        result.b = env->functions->CallByteMethodA(env, obj, mid, args);
                        break;
                    case char_function:
                        result.c = env->functions->CallCharMethodA(env, obj, mid, args);
                        break;
                    case short_function:
                        result.s = env->functions->CallShortMethodA(env, obj, mid, args);
                        break;
                    case int_function:
                        result.i = env->functions->CallIntMethodA(env, obj, mid, args);
                        break;
                    case long_function:
                        result.j = env->functions->CallLongMethodA(env, obj, mid, args);
                        break;
                    case float_function:
                        result.f = env->functions->CallFloatMethodA(env, obj, mid, args);
                        break;
                    case double_function:
                        result.d = env->functions->CallDoubleMethodA(env, obj, mid, args);
                        break;
                    default:
                        fprintf(stderr, "%s: invalid function type (%d)", __PRETTY_FUNCTION__, (int)type);
                    }
                }
                else
                {
                    fprintf(stderr, "%s: Could not find method: %s!", __PRETTY_FUNCTION__, name);
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }

                env->DeleteLocalRef(cls);
            }
            else {
                fprintf(stderr, "%s: Could not find class for object!", __PRETTY_FUNCTION__);
            }
        }
        else {
            fprintf(stderr, "%s: Could not attach to the VM!", __PRETTY_FUNCTION__);
        }
    }

    return result;
}

#define CALL_JNI_METHOD(function_type,obj,name,sig) \
    va_list args;\
    va_start (args, sig);\
    \
    jvalue result = callJNIMethod(function_type, obj, name, sig, args);\
    \
    va_end (args);

void callJNIVoidMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (void_function, obj, name, sig);
}

jobject callJNIObjectMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (object_function, obj, name, sig);
    return result.l;
}

jbyte callJNIByteMethod( jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (byte_function, obj, name, sig);
    return result.b;
}

jchar callJNICharMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (char_function, obj, name, sig);
    return result.c;
}

jshort callJNIShortMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (short_function, obj, name, sig);
    return result.s;
}

jint callJNIIntMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (int_function, obj, name, sig);
    return result.i;
}

jlong callJNILongMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (long_function, obj, name, sig);
    return result.j;
}

jfloat callJNIFloatMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (float_function, obj, name, sig);
    return result.f;
}

jdouble callJNIDoubleMethod (jobject obj, const char *name, const char *sig, ... )
{
    CALL_JNI_METHOD (double_function, obj, name, sig);
    return result.d;
}

void callJNIVoidMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (void_function, obj, name, sig, args);
}

jobject callJNIObjectMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (object_function, obj, name, sig, args);
    return result.l;
}

jbyte callJNIByteMethodA ( jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (byte_function, obj, name, sig, args);
    return result.b;
}

jchar callJNICharMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (char_function, obj, name, sig, args);
    return result.c;
}

jshort callJNIShortMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (short_function, obj, name, sig, args);
    return result.s;
}

jint callJNIIntMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (int_function, obj, name, sig, args);
    return result.i;
}

jlong callJNILongMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (long_function, obj, name, sig, args);
    return result.j;
}

jfloat callJNIFloatMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA  (float_function, obj, name, sig, args);
    return result.f;
}

jdouble callJNIDoubleMethodA (jobject obj, const char *name, const char *sig, jvalue *args)
{
    jvalue result = callJNIMethodA (double_function, obj, name, sig, args);
    return result.d;
}

const char *getCharactersFromJString (JNIEnv *env, jstring aJString)
{
    jboolean isCopy;
    const char *s = env->GetStringUTFChars((jstring)aJString, &isCopy);
    if (!s) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    return s;
}

void releaseCharactersForJString (JNIEnv *env, jstring aJString, const char *s)
{
    env->ReleaseStringUTFChars (aJString, s);
}
