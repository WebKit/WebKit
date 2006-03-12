/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "c_utility.h"
#include <unicode/ucnv.h>

#include "JSType.h"
#include "NP_jsobject.h"
#include "c_instance.h" 
#include "npruntime_impl.h"
#include "npruntime_priv.h"

namespace KJS { namespace Bindings {

// Requires free() of returned UTF16Chars.
void convertNPStringToUTF16(const NPString *string, NPUTF16 **UTF16Chars, unsigned int *UTF16Length)
{
    convertUTF8ToUTF16(string->UTF8Characters, string->UTF8Length, UTF16Chars, UTF16Length);
}

// Requires free() of returned UTF16Chars.
void convertUTF8ToUTF16(const NPUTF8 *UTF8Chars, int UTF8Length, NPUTF16 **UTF16Chars, unsigned int *UTF16Length)
{
    assert(UTF8Chars || UTF8Length == 0);
    assert(UTF16Chars);
    
    if (UTF8Length == -1)
        UTF8Length = strlen(UTF8Chars);
        
    // UTF16Length maximum length is the length of the UTF8 string, plus one to include terminator
    // Without the plus one, it will convert ok, but a warning is generated from the converter as
    // there is not enough room for a terminating character.
    *UTF16Length = UTF8Length + 1; 
        
    *UTF16Chars = 0;
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv = ucnv_open("utf8", &status);
    if (U_SUCCESS(status)) { 
        *UTF16Chars = (NPUTF16 *)malloc(sizeof(NPUTF16) * (*UTF16Length));
        ucnv_setToUCallBack(conv, UCNV_TO_U_CALLBACK_STOP, 0, 0, 0, &status);
        *UTF16Length = ucnv_toUChars(conv, *UTF16Chars, *UTF16Length, UTF8Chars, UTF8Length, &status); 
        ucnv_close(conv);
    } 
    
    // Check to see if the conversion was successful
    // Some plugins return invalid UTF-8 in NPVariantType_String, see <http://bugzilla.opendarwin.org/show_bug.cgi?id=5163>
    // There is no "bad data" for latin1. It is unlikely that the plugin was really sending text in this encoding,
    // but it should have used UTF-8, and now we are simply avoiding a crash.
    if (!U_SUCCESS(status)) {
        *UTF16Length = UTF8Length;
        
        if (!*UTF16Chars)   // If the memory wasn't allocated, allocate it.
            *UTF16Chars = (NPUTF16 *)malloc(sizeof(NPUTF16) * (*UTF16Length));
 
        for (unsigned i = 0; i < *UTF16Length; i++)
            (*UTF16Chars)[i] = UTF8Chars[i] & 0xFF;
    }
}

// Variant value must be released with NPReleaseVariantValue()
void coerceValueToNPVariantStringType(ExecState *exec, JSValue *value, NPVariant *result)
{
    UString ustring = value->toString(exec);
    CString cstring = ustring.UTF8String();
    NPString string = { (const NPUTF8 *)cstring.c_str(), cstring.size() };
    NPN_InitializeVariantWithStringCopy(result, &string);
}

// Variant value must be released with NPReleaseVariantValue()
void convertValueToNPVariant(ExecState *exec, JSValue *value, NPVariant *result)
{
    JSType type = value->type();
    
    if (type == StringType) {
        UString ustring = value->toString(exec);
        CString cstring = ustring.UTF8String();
        NPString string = { (const NPUTF8 *)cstring.c_str(), cstring.size() };
        NPN_InitializeVariantWithStringCopy(result, &string );
    }
    else if (type == NumberType) {
        NPN_InitializeVariantWithDouble(result, value->toNumber(exec));
    }
    else if (type == BooleanType) {
        NPN_InitializeVariantWithBool(result, value->toBoolean(exec));
    }
    else if (type == UnspecifiedType) {
        NPN_InitializeVariantAsUndefined(result);
    }
    else if (type == NullType) {
        NPN_InitializeVariantAsNull(result);
    }
    else if (type == ObjectType) {
        JSObject *objectImp = static_cast<JSObject*>(value);
        if (objectImp->classInfo() == &RuntimeObjectImp::info) {
            RuntimeObjectImp *imp = static_cast<RuntimeObjectImp *>(value);
            CInstance *instance = static_cast<CInstance*>(imp->getInternalInstance());
            NPN_InitializeVariantWithObject(result, instance->getObject());
        }
	else {

	    Interpreter *originInterpreter = exec->interpreter();
            const Bindings::RootObject *originExecutionContext = rootForInterpreter(originInterpreter);

	    Interpreter *interpreter = 0;
	    if (originInterpreter->isGlobalObject(value)) {
		interpreter = originInterpreter->interpreterForGlobalObject(value);
	    }

	    if (!interpreter)
		interpreter = originInterpreter;
		
            const Bindings::RootObject *executionContext = rootForInterpreter(interpreter);
            if (!executionContext) {
                Bindings::RootObject *newExecutionContext = new Bindings::RootObject(0);
                newExecutionContext->setInterpreter(interpreter);
                executionContext = newExecutionContext;
            }
    
	    NPObject *obj = (NPObject *)exec->interpreter()->createLanguageInstanceForValue(exec, Instance::CLanguage, value->toObject(exec), originExecutionContext, executionContext);
	    NPN_InitializeVariantWithObject(result, obj);
	    _NPN_ReleaseObject(obj);
	}
    }
    else
        NPN_InitializeVariantAsUndefined(result);
}

JSValue *convertNPVariantToValue(ExecState*, const NPVariant*variant)
{
    NPVariantType type = variant->type;

    if (type == NPVariantType_Bool) {
        NPBool aBool;
        if (NPN_VariantToBool(variant, &aBool))
            return jsBoolean(aBool);
        return jsBoolean(false);
    }
    else if (type == NPVariantType_Null) {
        return jsNull();
    }
    else if (type == NPVariantType_Void) {
        return jsUndefined();
    }
    else if (type == NPVariantType_Int32) {
        int32_t anInt;
        if (NPN_VariantToInt32(variant, &anInt))
            return jsNumber(anInt);
        return jsNumber(0);
    }
    else if (type == NPVariantType_Double) {
        double aDouble;
        if (NPN_VariantToDouble(variant, &aDouble))
            return jsNumber(aDouble);
        return jsNumber(0);
    }
    else if (type == NPVariantType_String) {
        NPUTF16 *stringValue;
        unsigned int UTF16Length;
        convertNPStringToUTF16(&variant->value.stringValue, &stringValue, &UTF16Length);    // requires free() of returned memory.
        UString resultString((const UChar *)stringValue,UTF16Length);
        free(stringValue);
        return jsString(resultString);
    }
    else if (type == NPVariantType_Object) {
        NPObject *obj = variant->value.objectValue;
        
        if (obj->_class == NPScriptObjectClass) {
            // Get JSObject from NP_JavaScriptObject.
            JavaScriptObject *o = (JavaScriptObject *)obj;
            return const_cast<JSObject*>(o->imp);
        }
        else {
            //  Wrap NPObject in a CInstance.
            return Instance::createRuntimeObject(Instance::CLanguage, obj);
        }
    }
    
    return jsUndefined();
}

} }
