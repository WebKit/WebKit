/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JNIUtilityPrivate.h"

#if ENABLE(JAVA_BRIDGE)

#include "JavaInstanceV8.h"
#include "JavaNPObjectV8.h"
#include <wtf/text/CString.h>

namespace JSC {

namespace Bindings {

jvalue convertNPVariantToJValue(NPVariant value, const WTF::String& javaClass)
{
    CString javaClassName = javaClass.utf8();
    JavaType javaType = javaTypeFromClassName(javaClassName.data());
    jvalue result;
    NPVariantType type = value.type;

    switch (javaType) {
    case JavaTypeArray:
    case JavaTypeObject:
        {
            result.l = static_cast<jobject>(0);

            // First see if we have a Java instance.
            if (type == NPVariantType_Object) {
                NPObject* objectImp = NPVARIANT_TO_OBJECT(value);
                if (JavaInstance* instance = ExtractJavaInstance(objectImp))
                    result.l = instance->javaInstance();
            }

            // Now convert value to a string if the target type is a java.lang.string, and we're not
            // converting from a Null.
            if (!result.l && !strcmp(javaClassName.data(), "java.lang.String")) {
#ifdef CONVERT_NULL_TO_EMPTY_STRING
                if (type == NPVariantType_Null) {
                    JNIEnv* env = getJNIEnv();
                    jchar buf[2];
                    jobject javaString = env->functions->NewString(env, buf, 0);
                    result.l = javaString;
                } else
#else
                if (type == NPVariantType_String)
#endif
                {
                    NPString src = NPVARIANT_TO_STRING(value);
                    JNIEnv* env = getJNIEnv();
                    jobject javaString = env->NewStringUTF(src.UTF8Characters);
                    result.l = javaString;
                }
            } else if (!result.l)
                memset(&result, 0, sizeof(jvalue)); // Handle it the same as a void case
        }
        break;

    case JavaTypeBoolean:
        {
            if (type == NPVariantType_Bool)
                result.z = NPVARIANT_TO_BOOLEAN(value);
            else
                memset(&result, 0, sizeof(jvalue)); // as void case
        }
        break;

    case JavaTypeByte:
        {
            if (type == NPVariantType_Int32)
                result.b = static_cast<jbyte>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.b = static_cast<jbyte>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case JavaTypeChar:
        {
            if (type == NPVariantType_Int32)
                result.c = static_cast<char>(NPVARIANT_TO_INT32(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case JavaTypeShort:
        {
            if (type == NPVariantType_Int32)
                result.s = static_cast<jshort>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.s = static_cast<jshort>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case JavaTypeInt:
        {
            if (type == NPVariantType_Int32)
                result.i = static_cast<jint>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.i = static_cast<jint>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case JavaTypeLong:
        {
            if (type == NPVariantType_Int32)
                result.j = static_cast<jlong>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.j = static_cast<jlong>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case JavaTypeFloat:
        {
            if (type == NPVariantType_Int32)
                result.f = static_cast<jfloat>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.f = static_cast<jfloat>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

    case JavaTypeDouble:
        {
            if (type == NPVariantType_Int32)
                result.d = static_cast<jdouble>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.d = static_cast<jdouble>(NPVARIANT_TO_DOUBLE(value));
            else
                memset(&result, 0, sizeof(jvalue));
        }
        break;

        break;

    case JavaTypeInvalid:
    default:
    case JavaTypeVoid:
        {
            memset(&result, 0, sizeof(jvalue));
        }
        break;
    }
    return result;
}


void convertJValueToNPVariant(jvalue value, JavaType javaType, const char* javaTypeName, NPVariant* result)
{
    switch (javaType) {
    case JavaTypeVoid:
        {
            VOID_TO_NPVARIANT(*result);
        }
        break;

    case JavaTypeObject:
        {
            if (value.l) {
                if (!strcmp(javaTypeName, "java.lang.String")) {
                    const char* v = getCharactersFromJString(static_cast<jstring>(value.l));
                    // s is freed in NPN_ReleaseVariantValue (see npruntime.cpp)
                    const char* s = strdup(v);
                    releaseCharactersForJString(static_cast<jstring>(value.l), v);
                    STRINGZ_TO_NPVARIANT(s, *result);
                } else
                    OBJECT_TO_NPVARIANT(JavaInstanceToNPObject(new JavaInstance(value.l)), *result);
            } else
                VOID_TO_NPVARIANT(*result);
        }
        break;

    case JavaTypeBoolean:
        {
            BOOLEAN_TO_NPVARIANT(value.z, *result);
        }
        break;

    case JavaTypeByte:
        {
            INT32_TO_NPVARIANT(value.b, *result);
        }
        break;

    case JavaTypeChar:
        {
            INT32_TO_NPVARIANT(value.c, *result);
        }
        break;

    case JavaTypeShort:
        {
            INT32_TO_NPVARIANT(value.s, *result);
        }
        break;

    case JavaTypeInt:
        {
            INT32_TO_NPVARIANT(value.i, *result);
        }
        break;

        // TODO: Check if cast to double is needed.
    case JavaTypeLong:
        {
            DOUBLE_TO_NPVARIANT(value.j, *result);
        }
        break;

    case JavaTypeFloat:
        {
            DOUBLE_TO_NPVARIANT(value.f, *result);
        }
        break;

    case JavaTypeDouble:
        {
            DOUBLE_TO_NPVARIANT(value.d, *result);
        }
        break;

    case JavaTypeInvalid:
    default:
        {
            VOID_TO_NPVARIANT(*result);
        }
        break;
    }
}

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(JAVA_BRIDGE)
