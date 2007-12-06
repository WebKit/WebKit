/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include "objc_utility.h"

#include "objc_instance.h"
#include "JSGlobalObject.h"
#include "runtime_array.h"
#include "runtime_object.h"
#include "WebScriptObject.h"
#include <wtf/Assertions.h>

#if !defined(_C_LNG_LNG)
#define _C_LNG_LNG 'q'
#endif

#if !defined(_C_ULNG_LNG)
#define _C_ULNG_LNG 'Q'
#endif

#if !defined(_C_CONST)
#define _C_CONST 'r'
#endif

#if !defined(_C_BYCOPY)
#define _C_BYCOPY 'O'
#endif

#if !defined(_C_BYREF)
#define _C_BYREF 'R'
#endif

#if !defined(_C_ONEWAY)
#define _C_ONEWAY 'V'
#endif

#if !defined(_C_GCINVISIBLE)
#define _C_GCINVISIBLE '!'
#endif

namespace KJS {
namespace Bindings {

/*
    By default, a JavaScript method name is produced by concatenating the 
    components of an ObjectiveC method name, replacing ':' with '_', and 
    escaping '_' and '$' with a leading '$', such that '_' becomes "$_" and 
    '$' becomes "$$". For example:

    ObjectiveC name         Default JavaScript name
        moveTo::                moveTo__
        moveTo_                 moveTo$_
        moveTo$_                moveTo$$$_

    This function performs the inverse of that operation.
 
    @result Fills 'buffer' with the ObjectiveC method name that corresponds to 'JSName'. 
            Returns true for success, false for failure. (Failure occurs when 'buffer' 
            is not big enough to hold the result.)
*/
bool convertJSMethodNameToObjc(const char *JSName, char *buffer, size_t bufferSize)
{
    ASSERT(JSName && buffer);
    
    const char *sp = JSName; // source pointer
    char *dp = buffer; // destination pointer
        
    char *end = buffer + bufferSize;
    while (dp < end) {
        if (*sp == '$') {
            ++sp;
            *dp = *sp;
        } else if (*sp == '_')
            *dp = ':';
        else
            *dp = *sp;

        // If a future coder puts funny ++ operators above, we might write off the end 
        // of the buffer in the middle of this loop. Let's make sure to check for that.
        ASSERT(dp < end);
        
        if (*sp == 0) { // We finished converting JSName
            ASSERT(strlen(JSName) < bufferSize);
            return true;
        }
        
        ++sp; 
        ++dp;
    }

    return false; // We ran out of buffer before converting JSName
}

/*

    JavaScript to   ObjC
    Number          coerced to char, short, int, long, float, double, or NSNumber, as appropriate
    String          NSString
    wrapper         id
    Object          WebScriptObject
    null            NSNull
    [], other       exception

*/
ObjcValue convertValueToObjcValue(ExecState *exec, JSValue *value, ObjcValueType type)
{
    ObjcValue result;
    double d = 0;

    if (value->isNumber() || value->isString() || value->isBoolean())
        d = value->toNumber(exec);

    switch (type) {
        case ObjcObjectType: {
            JSLock lock;
            
            JSGlobalObject *originGlobalObject = exec->dynamicGlobalObject();
            RootObject* originRootObject = findRootObject(originGlobalObject);

            JSGlobalObject* globalObject = 0;
            if (value->isObject() && static_cast<JSObject*>(value)->isGlobalObject())
                globalObject = static_cast<JSGlobalObject*>(value);

            if (!globalObject)
                globalObject = originGlobalObject;
                
            RootObject* rootObject = findRootObject(globalObject);
            result.objectValue =  rootObject
                ? [webScriptObjectClass() _convertValueToObjcValue:value originRootObject:originRootObject rootObject:rootObject]
                : nil;
        }
        break;

        case ObjcCharType:
        case ObjcUnsignedCharType:
            result.charValue = (char)d;
            break;
        case ObjcShortType:
        case ObjcUnsignedShortType:
            result.shortValue = (short)d;
            break;
        case ObjcIntType:
        case ObjcUnsignedIntType:
            result.intValue = (int)d;
            break;
        case ObjcLongType:
        case ObjcUnsignedLongType:
            result.longValue = (long)d;
            break;
        case ObjcLongLongType:
        case ObjcUnsignedLongLongType:
            result.longLongValue = (long long)d;
            break;
        case ObjcFloatType:
            result.floatValue = (float)d;
            break;
        case ObjcDoubleType:
            result.doubleValue = (double)d;
            break;
        case ObjcVoidType:
            bzero(&result, sizeof(ObjcValue));
            break;

        case ObjcInvalidType:
        default:
            // FIXME: throw an exception?
            break;
    }

    return result;
}

JSValue *convertNSStringToString(NSString *nsstring)
{
    JSLock lock;
    
    unichar *chars;
    unsigned int length = [nsstring length];
    chars = (unichar *)malloc(sizeof(unichar)*length);
    [nsstring getCharacters:chars];
    UString u((const UChar*)chars, length);
    JSValue *aValue = jsString(u);
    free((void *)chars);
    return aValue;
}

/*
    ObjC      to    JavaScript
    ----            ----------
    char            number
    short           number
    int             number
    long            number
    float           number
    double          number
    NSNumber        boolean or number
    NSString        string
    NSArray         array
    NSNull          null
    WebScriptObject underlying JavaScript object
    WebUndefined    undefined
    id              object wrapper
    other           should not happen
*/
JSValue* convertObjcValueToValue(ExecState* exec, void* buffer, ObjcValueType type, RootObject* rootObject)
{
    JSLock lock;
    
    switch (type) {
        case ObjcObjectType: {
            id obj = *(id*)buffer;
            if ([obj isKindOfClass:[NSString class]])
                return convertNSStringToString((NSString *)obj);
            if ([obj isKindOfClass:webUndefinedClass()])
                return jsUndefined();
            if ((CFBooleanRef)obj == kCFBooleanTrue)
                return jsBoolean(true);
            if ((CFBooleanRef)obj == kCFBooleanFalse)
                return jsBoolean(false);
            if ([obj isKindOfClass:[NSNumber class]])
                return jsNumber([obj doubleValue]);
            if ([obj isKindOfClass:[NSArray class]])
                return new RuntimeArray(exec, new ObjcArray(obj, rootObject));
            if ([obj isKindOfClass:webScriptObjectClass()]) {
                JSObject* imp = [obj _imp];
                return imp ? imp : jsUndefined();
            }
            if ([obj isKindOfClass:[NSNull class]])
                return jsNull();
            if (obj == 0)
                return jsUndefined();
            return Instance::createRuntimeObject(Instance::ObjectiveCLanguage, obj, rootObject);
        }
        case ObjcCharType:
            return jsNumber(*(char *)buffer);
        case ObjcUnsignedCharType:
            return jsNumber(*(unsigned char *)buffer);
        case ObjcShortType:
            return jsNumber(*(short *)buffer);
        case ObjcUnsignedShortType:
            return jsNumber(*(unsigned short *)buffer);
        case ObjcIntType:
            return jsNumber(*(int *)buffer);
        case ObjcUnsignedIntType:
            return jsNumber(*(unsigned int *)buffer);
        case ObjcLongType:
            return jsNumber(*(long *)buffer);
        case ObjcUnsignedLongType:
            return jsNumber(*(unsigned long *)buffer);
        case ObjcLongLongType:
            return jsNumber(*(long long *)buffer);
        case ObjcUnsignedLongLongType:
            return jsNumber(*(unsigned long long *)buffer);
        case ObjcFloatType:
            return jsNumber(*(float *)buffer);
        case ObjcDoubleType:
            return jsNumber(*(double *)buffer);
        default:
            // Should never get here. Argument types are filtered.
            fprintf(stderr, "%s: invalid type (%d)\n", __PRETTY_FUNCTION__, (int)type);
            ASSERT(false);
    }
    
    return 0;
}

ObjcValueType objcValueTypeForType(const char *type)
{
    int typeLength = strlen(type);
    ObjcValueType objcValueType = ObjcInvalidType;

    for (int i = 0; i < typeLength; ++i) {
        char typeChar = type[i];
        switch (typeChar) {
            case _C_CONST:
            case _C_BYCOPY:
            case _C_BYREF:
            case _C_ONEWAY:
            case _C_GCINVISIBLE:
                // skip these type modifiers
                break;
            case _C_ID:
                objcValueType = ObjcObjectType;
                break;
            case _C_CHR:
                objcValueType = ObjcCharType;
                break;
            case _C_UCHR:
                objcValueType = ObjcUnsignedCharType;
                break;
            case _C_SHT:
                objcValueType = ObjcShortType;
                break;
            case _C_USHT:
                objcValueType = ObjcUnsignedShortType;
                break;
            case _C_INT:
                objcValueType = ObjcIntType;
                break;
            case _C_UINT:
                objcValueType = ObjcUnsignedIntType;
                break;
            case _C_LNG:
                objcValueType = ObjcLongType;
                break;
            case _C_ULNG:
                objcValueType = ObjcUnsignedLongType;
                break;
            case _C_LNG_LNG:
                objcValueType = ObjcLongLongType;
                break;
            case _C_ULNG_LNG:
                objcValueType = ObjcUnsignedLongLongType;
                break;
            case _C_FLT:
                objcValueType = ObjcFloatType;
                break;
            case _C_DBL:
                objcValueType = ObjcDoubleType;
                break;
            case _C_VOID:
                objcValueType = ObjcVoidType;
                break;
            default:
                // Unhandled type. We don't handle C structs, unions, etc.
                // FIXME: throw an exception?
                ASSERT(false);
        }

        if (objcValueType != ObjcInvalidType)
            break;
    }

    return objcValueType;
}

JSObject *throwError(ExecState *exec, ErrorType type, NSString *message)
{
    ASSERT(message);
    size_t length = [message length];
    unichar *buffer = new unichar[length];
    [message getCharacters:buffer];
    JSObject *error = throwError(exec, type, UString(reinterpret_cast<UChar *>(buffer), length));
    delete [] buffer;
    return error;
}

}
}
