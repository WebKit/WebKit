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

#include "runtime_array.h"
#include "runtime_object.h"

#include "WebScriptObjectPrivate.h"

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
    assert(JSName && buffer);
    
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
        assert(dp < end);
        
        if (*sp == 0) { // We finished converting JSName
            assert(strlen(JSName) < bufferSize);
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
    [], other       exception

*/
ObjcValue convertValueToObjcValue (ExecState *exec, JSValue *value, ObjcValueType type)
{
    ObjcValue result;
    double d = 0;

    if (value->isNumber() || value->isString() || value->isBoolean())
	d = value->toNumber(exec);
	
    switch (type){
        case ObjcObjectType: {
	    Interpreter *originInterpreter = exec->dynamicInterpreter();
            const RootObject *originExecutionContext = rootForInterpreter(originInterpreter);

	    Interpreter *interpreter = 0;
	    if (originInterpreter->isGlobalObject(value)) {
		interpreter = originInterpreter->interpreterForGlobalObject (value);
	    }

	    if (!interpreter)
		interpreter = originInterpreter;
		
            const RootObject *executionContext = rootForInterpreter(interpreter);
            if (!executionContext) {
                RootObject *newExecutionContext = new RootObject(0);
                newExecutionContext->setInterpreter (interpreter);
                executionContext = newExecutionContext;
            }
            result.objectValue = [WebScriptObject _convertValueToObjcValue:value originExecutionContext:originExecutionContext executionContext:executionContext ];
        }
        break;
        
        
        case ObjcCharType: {
            result.charValue = (char)d;
        }
        break;

        case ObjcShortType: {
            result.shortValue = (short)d;
        }
        break;

        case ObjcIntType: {
            result.intValue = (int)d;
        }
        break;

        case ObjcLongType: {
            result.longValue = (long)d;
        }
        break;

        case ObjcFloatType: {
            result.floatValue = (float)d;
        }
        break;

        case ObjcDoubleType: {
            result.doubleValue = (double)d;
        }
        break;

        case ObjcVoidType: {
            bzero (&result, sizeof(ObjcValue));
        }
        break;

        case ObjcInvalidType:
        default:
        {
            // FIXME:  throw an exception
        }
        break;
    }
    return result;
}

JSValue *convertNSStringToString(NSString *nsstring)
{
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
    char            Number
    short
    int
    long
    float
    double
    NSNumber        Number
    NSString        String
    NSArray         Array
    id              Object wrapper
    other           should not happen

*/
JSValue *convertObjcValueToValue (ExecState *exec, void *buffer, ObjcValueType type)
{
    JSValue *aValue = NULL;

    switch (type) {
        case ObjcObjectType:
            {
                ObjectStructPtr *obj = (ObjectStructPtr *)buffer;

                /*
                    NSNumber to Number
                    NSString to String
                    NSArray  to Array
                    id       to Object wrapper
                */
                if ([*obj isKindOfClass:[NSString class]]){
                    aValue = convertNSStringToString((NSString *)*obj);
                } else if (*obj == [WebUndefined undefined]) {
                    return jsUndefined();
                }  else if ((CFBooleanRef)*obj == kCFBooleanTrue) {
                    aValue = jsBoolean(true);
                } else if ((CFBooleanRef)*obj == kCFBooleanFalse) {
                    aValue = jsBoolean(false);
                } else if ([*obj isKindOfClass:[NSNumber class]]) {
                    aValue = jsNumber([*obj doubleValue]);
                } else if ([*obj isKindOfClass:[NSArray class]]) {
                    aValue = new RuntimeArray(exec, new ObjcArray(*obj));
                } else if ([*obj isKindOfClass:[WebScriptObject class]]) {
                    WebScriptObject *jsobject = (WebScriptObject *)*obj;
                    aValue = [jsobject _imp];
                } else if (*obj == 0) {
                    return jsUndefined();
                } else {
		    aValue = Instance::createRuntimeObject(Instance::ObjectiveCLanguage, (void *)*obj);
                }
            }
            break;
        case ObjcCharType:
            aValue = jsNumber(*(char *)buffer);
            break;
        case ObjcShortType:
            aValue = jsNumber(*(short *)buffer);
            break;
        case ObjcIntType:
            aValue = jsNumber(*(int *)buffer);
            break;
        case ObjcLongType:
            aValue = jsNumber(*(long *)buffer);
            break;
        case ObjcFloatType:
            aValue = jsNumber(*(float *)buffer);
            break;
        case ObjcDoubleType:
            aValue = jsNumber(*(double *)buffer);
            break;
        default:
            // Should never get here.  Argument types are filtered (and
            // the assert above should have fired in the impossible case
            // of an invalid type anyway).
            fprintf(stderr, "%s: invalid type (%d)\n", __PRETTY_FUNCTION__, (int)type);
            assert(false);
    }
    
    return aValue;
}


ObjcValueType objcValueTypeForType (const char *type)
{
    int typeLength = strlen(type);
    ObjcValueType objcValueType = ObjcInvalidType;
    
    if (typeLength == 1) {
        char typeChar = type[0];
        switch (typeChar){
            case _C_ID: {
                objcValueType = ObjcObjectType;
            }
            break;
            case _C_CHR: {
                objcValueType = ObjcCharType;
            }
            break;
            case _C_SHT: {
                objcValueType = ObjcShortType;
            }
            break;
            case _C_INT: {
                objcValueType = ObjcIntType;
            }
            break;
            case _C_LNG: {
                objcValueType = ObjcLongType;
            }
            break;
            case _C_FLT: {
                objcValueType = ObjcFloatType;
            }
            break;
            case _C_DBL: {
                objcValueType = ObjcDoubleType;
            }
            break;
            case _C_VOID: {
                objcValueType = ObjcVoidType;
            }
            break;
        }
    }
    return objcValueType;
}


void *createObjcInstanceForValue(JSValue *value, const RootObject *origin, const RootObject *current)
{
    if (!value->isObject())
	return 0;
    JSObject *object = static_cast<JSObject *>(value);
    return [[[WebScriptObject alloc] _initWithJSObject:object originExecutionContext:origin executionContext:current] autorelease];
}

JSObject *throwError(ExecState *exec, ErrorType type, NSString *message)
{
    assert(message);
    size_t length = [message length];
    unichar *buffer = new unichar[length];
    [message getCharacters:buffer];
    JSObject *error = throwError(exec, type, UString(reinterpret_cast<UChar *>(buffer), length));
    delete [] buffer;
    return error;
}

}
}
