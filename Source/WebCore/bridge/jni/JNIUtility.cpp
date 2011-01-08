/*
 * Copyright (C) 2003, 2004, 2005, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "JNIUtility.h"

#if ENABLE(JAVA_BRIDGE)

#include <dlfcn.h>

namespace JSC {

namespace Bindings {

static jint KJSGetCreatedJavaVMs(JavaVM** vmBuf, jsize bufLen, jsize* nVMs)
{
    static void* javaVMFramework = 0;
    if (!javaVMFramework)
        javaVMFramework = dlopen("/System/Library/Frameworks/JavaVM.framework/JavaVM", RTLD_LAZY);
    if (!javaVMFramework)
        return JNI_ERR;

    typedef jint(*FunctionPointerType)(JavaVM**, jsize, jsize*);
    static FunctionPointerType functionPointer = 0;
    if (!functionPointer)
        functionPointer = reinterpret_cast<FunctionPointerType>(dlsym(javaVMFramework, "JNI_GetCreatedJavaVMs"));
    if (!functionPointer)
        return JNI_ERR;
    return functionPointer(vmBuf, bufLen, nVMs);
}

static JavaVM* jvm = 0;

// Provide the ability for an outside component to specify the JavaVM to use
// If the jvm value is set, the getJavaVM function below will just return.
// In getJNIEnv(), if AttachCurrentThread is called to a VM that is already
// attached, the result is a no-op.
void setJavaVM(JavaVM* javaVM)
{
    jvm = javaVM;
}

JavaVM* getJavaVM()
{
    if (jvm)
        return jvm;

    JavaVM* jvmArray[1];
    jsize bufLen = 1;
    jsize nJVMs = 0;
    jint jniError = 0;

    // Assumes JVM is already running ..., one per process
    jniError = KJSGetCreatedJavaVMs(jvmArray, bufLen, &nJVMs);
    if (jniError == JNI_OK && nJVMs > 0)
        jvm = jvmArray[0];
    else
        LOG_ERROR("JNI_GetCreatedJavaVMs failed, returned %ld", static_cast<long>(jniError));

    return jvm;
}

JNIEnv* getJNIEnv()
{
    union {
        JNIEnv* env;
        void* dummy;
    } u;
    jint jniError = 0;

    jniError = getJavaVM()->AttachCurrentThread(&u.dummy, 0);
    if (jniError == JNI_OK)
        return u.env;
    LOG_ERROR("AttachCurrentThread failed, returned %ld", static_cast<long>(jniError));
    return 0;
}

jmethodID getMethodID(jobject obj, const char* name, const char* sig)
{
    JNIEnv* env = getJNIEnv();
    jmethodID mid = 0;

    if (env) {
        jclass cls = env->GetObjectClass(obj);
        if (cls) {
            mid = env->GetMethodID(cls, name, sig);
            if (!mid) {
                env->ExceptionClear();
                mid = env->GetStaticMethodID(cls, name, sig);
                if (!mid)
                    env->ExceptionClear();
            }
        }
        env->DeleteLocalRef(cls);
    }
    return mid;
}

const char* getCharactersFromJString(jstring aJString)
{
    return getCharactersFromJStringInEnv(getJNIEnv(), aJString);
}

void releaseCharactersForJString(jstring aJString, const char* s)
{
    releaseCharactersForJStringInEnv(getJNIEnv(), aJString, s);
}

const char* getCharactersFromJStringInEnv(JNIEnv* env, jstring aJString)
{
    jboolean isCopy;
    const char* s = env->GetStringUTFChars(aJString, &isCopy);
    if (!s) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        fprintf(stderr, "\n");
    }
    return s;
}

void releaseCharactersForJStringInEnv(JNIEnv* env, jstring aJString, const char* s)
{
    env->ReleaseStringUTFChars(aJString, s);
}

const jchar* getUCharactersFromJStringInEnv(JNIEnv* env, jstring aJString)
{
    jboolean isCopy;
    const jchar* s = env->GetStringChars(aJString, &isCopy);
    if (!s) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        fprintf(stderr, "\n");
    }
    return s;
}

void releaseUCharactersForJStringInEnv(JNIEnv* env, jstring aJString, const jchar* s)
{
    env->ReleaseStringChars(aJString, s);
}

JNIType JNITypeFromClassName(const char* name)
{
    JNIType type;

    if (!strcmp("byte", name))
        type = byte_type;
    else if (!strcmp("short", name))
        type = short_type;
    else if (!strcmp("int", name))
        type = int_type;
    else if (!strcmp("long", name))
        type = long_type;
    else if (!strcmp("float", name))
        type = float_type;
    else if (!strcmp("double", name))
        type = double_type;
    else if (!strcmp("char", name))
        type = char_type;
    else if (!strcmp("boolean", name))
        type = boolean_type;
    else if (!strcmp("void", name))
        type = void_type;
    else if ('[' == name[0])
        type = array_type;
    else
        type = object_type;

    return type;
}

const char* signatureFromPrimitiveType(JNIType type)
{
    switch (type) {
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
    switch (type) {
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

jvalue getJNIField(jobject obj, JNIType type, const char* name, const char* signature)
{
    JavaVM* jvm = getJavaVM();
    JNIEnv* env = getJNIEnv();
    jvalue result;

    memset(&result, 0, sizeof(jvalue));
    if (obj && jvm && env) {
        jclass cls = env->GetObjectClass(obj);
        if (cls) {
            jfieldID field = env->GetFieldID(cls, name, signature);
            if (field) {
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
                    LOG_ERROR("Invalid field type (%d)", static_cast<int>(type));
                }
            } else {
                LOG_ERROR("Could not find field: %s", name);
                env->ExceptionDescribe();
                env->ExceptionClear();
                fprintf(stderr, "\n");
            }

            env->DeleteLocalRef(cls);
        } else
            LOG_ERROR("Could not find class for object");
    }

    return result;
}

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(JAVA_BRIDGE)
