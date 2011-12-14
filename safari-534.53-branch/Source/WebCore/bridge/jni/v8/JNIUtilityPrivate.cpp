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

#include "JavaInstanceJobjectV8.h"
#include "JavaNPObjectV8.h"
#include "JavaValueV8.h"
#include <wtf/text/CString.h>

namespace JSC {

namespace Bindings {

JavaValue convertNPVariantToJavaValue(NPVariant value, const String& javaClass)
{
    CString javaClassName = javaClass.utf8();
    JavaType javaType = javaTypeFromClassName(javaClassName.data());
    JavaValue result;
    result.m_type = javaType;
    NPVariantType type = value.type;

    switch (javaType) {
    case JavaTypeArray:
    case JavaTypeObject:
        {
            // See if we have a Java instance.
            if (type == NPVariantType_Object) {
                NPObject* objectImp = NPVARIANT_TO_OBJECT(value);
                result.m_objectValue = ExtractJavaInstance(objectImp);
            }
        }
        break;

    case JavaTypeString:
        {
#ifdef CONVERT_NULL_TO_EMPTY_STRING
            if (type == NPVariantType_Null) {
                result.m_type = JavaTypeString;
                result.m_stringValue = String::fromUTF8("");
            } else
#else
            if (type == NPVariantType_String)
#endif
            {
                NPString src = NPVARIANT_TO_STRING(value);
                result.m_type = JavaTypeString;
                result.m_stringValue = String::fromUTF8(src.UTF8Characters);
            }
        }
        break;

    case JavaTypeBoolean:
        {
            if (type == NPVariantType_Bool)
                result.m_booleanValue = NPVARIANT_TO_BOOLEAN(value);
        }
        break;

    case JavaTypeByte:
        {
            if (type == NPVariantType_Int32)
                result.m_byteValue = static_cast<signed char>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_byteValue = static_cast<signed char>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeChar:
        {
            if (type == NPVariantType_Int32)
                result.m_charValue = static_cast<unsigned short>(NPVARIANT_TO_INT32(value));
        }
        break;

    case JavaTypeShort:
        {
            if (type == NPVariantType_Int32)
                result.m_shortValue = static_cast<short>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_shortValue = static_cast<short>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeInt:
        {
            if (type == NPVariantType_Int32)
                result.m_intValue = static_cast<int>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_intValue = static_cast<int>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeLong:
        {
            if (type == NPVariantType_Int32)
                result.m_longValue = static_cast<long long>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_longValue = static_cast<long long>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeFloat:
        {
            if (type == NPVariantType_Int32)
                result.m_floatValue = static_cast<float>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_floatValue = static_cast<float>(NPVARIANT_TO_DOUBLE(value));
        }
        break;

    case JavaTypeDouble:
        {
            if (type == NPVariantType_Int32)
                result.m_doubleValue = static_cast<double>(NPVARIANT_TO_INT32(value));
            else if (type == NPVariantType_Double)
                result.m_doubleValue = static_cast<double>(NPVARIANT_TO_DOUBLE(value));
        }
        break;
    default:
        break;
    }
    return result;
}


void convertJavaValueToNPVariant(JavaValue value, NPVariant* result)
{
    switch (value.m_type) {
    case JavaTypeVoid:
        {
            VOID_TO_NPVARIANT(*result);
        }
        break;

    case JavaTypeObject:
        {
            // If the JavaValue is a String object, it should have type JavaTypeString.
            if (value.m_objectValue)
                OBJECT_TO_NPVARIANT(JavaInstanceToNPObject(value.m_objectValue.get()), *result);
            else
                VOID_TO_NPVARIANT(*result);
        }
        break;

    case JavaTypeString:
        {
            const char* utf8String = strdup(value.m_stringValue.utf8().data());
            // The copied string is freed in NPN_ReleaseVariantValue (see npruntime.cpp)
            STRINGZ_TO_NPVARIANT(utf8String, *result);
        }
        break;

    case JavaTypeBoolean:
        {
            BOOLEAN_TO_NPVARIANT(value.m_booleanValue, *result);
        }
        break;

    case JavaTypeByte:
        {
            INT32_TO_NPVARIANT(value.m_byteValue, *result);
        }
        break;

    case JavaTypeChar:
        {
            INT32_TO_NPVARIANT(value.m_charValue, *result);
        }
        break;

    case JavaTypeShort:
        {
            INT32_TO_NPVARIANT(value.m_shortValue, *result);
        }
        break;

    case JavaTypeInt:
        {
            INT32_TO_NPVARIANT(value.m_intValue, *result);
        }
        break;

        // TODO: Check if cast to double is needed.
    case JavaTypeLong:
        {
            DOUBLE_TO_NPVARIANT(value.m_longValue, *result);
        }
        break;

    case JavaTypeFloat:
        {
            DOUBLE_TO_NPVARIANT(value.m_floatValue, *result);
        }
        break;

    case JavaTypeDouble:
        {
            DOUBLE_TO_NPVARIANT(value.m_doubleValue, *result);
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

JavaValue jvalueToJavaValue(const jvalue& value, const JavaType& type)
{
    JavaValue result;
    result.m_type = type;
    switch (result.m_type) {
    case JavaTypeVoid:
        break;
    case JavaTypeObject:
        result.m_objectValue = new JavaInstanceJobject(value.l);
        break;
    case JavaTypeString:
        {
            jstring javaString = static_cast<jstring>(value.l);
            const UChar* a = getUCharactersFromJStringInEnv(getJNIEnv(), javaString);
            // We take a copy to allow the Java String to be released.
            result.m_stringValue = String(a).threadsafeCopy();
            releaseUCharactersForJStringInEnv(getJNIEnv(), javaString, a);
        }
        break;
    case JavaTypeBoolean:
        result.m_booleanValue = value.z == JNI_FALSE ? false : true;
        break;
    case JavaTypeByte:
        result.m_byteValue = value.b;
        break;
    case JavaTypeChar:
        result.m_charValue = value.c;
        break;
    case JavaTypeShort:
        result.m_shortValue = value.s;
        break;
    case JavaTypeInt:
        result.m_intValue = value.i;
        break;
    case JavaTypeLong:
        result.m_longValue = value.j;
        break;
    case JavaTypeFloat:
        result.m_floatValue = value.f;
        break;
    case JavaTypeDouble:
        result.m_doubleValue = value.d;
        break;
    default:
        ASSERT(false);
    }
    return result;
}

jvalue javaValueToJvalue(const JavaValue& value)
{
    jvalue result;
    memset(&result, 0, sizeof(jvalue));
    switch (value.m_type) {
    case JavaTypeVoid:
        break;
    case JavaTypeObject:
        if (value.m_objectValue) {
            // This method is used only by JavaInstanceJobject, so we know the
            // derived type of the object.
            result.l = static_cast<JavaInstanceJobject*>(value.m_objectValue.get())->javaInstance();
        }
        break;
    case JavaTypeString:
        // This creates a local reference to a new String object, which will
        // be released when the call stack returns to Java. Note that this
        // may cause leaks if invoked from a native message loop, as is the
        // case in workers.
        result.l = getJNIEnv()->NewString(value.m_stringValue.characters(), value.m_stringValue.length());
        break;
    case JavaTypeBoolean:
        result.z = value.m_booleanValue ? JNI_TRUE : JNI_FALSE;
        break;
    case JavaTypeByte:
        result.b = value.m_byteValue;
        break;
    case JavaTypeChar:
        result.c = value.m_charValue;
        break;
    case JavaTypeShort:
        result.s = value.m_shortValue;
        break;
    case JavaTypeInt:
        result.i = value.m_intValue;
        break;
    case JavaTypeLong:
        result.j = value.m_longValue;
        break;
    case JavaTypeFloat:
        result.f = value.m_floatValue;
        break;
    case JavaTypeDouble:
        result.d = value.m_doubleValue;
        break;
    default:
        ASSERT(false);
    }
    return result;
}

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(JAVA_BRIDGE)
