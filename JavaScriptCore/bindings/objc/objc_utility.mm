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
#include <Foundation/Foundation.h>

#include <JavaScriptCore/internal.h>

#include <objc_instance.h>
#include <objc_utility.h>

#include <runtime_array.h>
#include <runtime_object.h>
#include <runtime_root.h>

#include <WebScriptObjectPrivate.h>


using namespace KJS;
using namespace KJS::Bindings;

/*
    The default name concatenates the components of the
    ObjectiveC selector name and replaces ':' with '_'.  '_' characters
    are escaped with an additional '$', i.e. '_' becomes "$_".  '$' are
    also escaped, i.e.
        ObjectiveC name         Default script name
        moveTo::                move__
        moveTo_                 moveTo$_
        moveTo$_                moveTo$$$_
    @result Returns the name to be used to represent the specificed selector in the
*/
void Bindings::JSMethodNameToObjCMethodName(const char *name, char *buffer, unsigned int len)
{
    const char *np = name;
    char *bp;

    if (strlen(name)*2+1 > len){
        *buffer = 0;
    }

    bp = buffer;
    while (*np) {
        if (*np == '$') {
            np++;
            *bp++ = *np++;
            continue;
        }
        
        if (*np == '_') {
            np++;
            *bp++ = ':';
        }
        else
            *bp++ = *np++;
    }
    *bp++ = 0;
}

/*

    JavaScript to   ObjC
    Number          coerced to char, short, int, long, float, double, or NSNumber, as appropriate
    String          NSString
    wrapper         id
    Object          WebScriptObject
    [], other       exception

*/
ObjcValue Bindings::convertValueToObjcValue (ExecState *exec, ValueImp *value, ObjcValueType type)
{
    ObjcValue result;
    double d = 0;

    if (value->isNumber() || value->isString() || value->isBoolean())
	d = value->toNumber(exec);
	
    switch (type){
        case ObjcObjectType: {
	    Interpreter *originInterpreter = exec->interpreter();
            const Bindings::RootObject *originExecutionContext = rootForInterpreter(originInterpreter);

	    Interpreter *interpreter = 0;
	    if (originInterpreter->isGlobalObject(value)) {
		interpreter = originInterpreter->interpreterForGlobalObject (value);
	    }

	    if (!interpreter)
		interpreter = originInterpreter;
		
            const Bindings::RootObject *executionContext = rootForInterpreter(interpreter);
            if (!executionContext) {
                Bindings::RootObject *newExecutionContext = new Bindings::RootObject(0);
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

ValueImp *Bindings::convertNSStringToString(NSString *nsstring)
{
    unichar *chars;
    unsigned int length = [nsstring length];
    chars = (unichar *)malloc(sizeof(unichar)*length);
    [nsstring getCharacters:chars];
    UString u((const UChar*)chars, length);
    ValueImp *aValue = String (u);
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
ValueImp *Bindings::convertObjcValueToValue (ExecState *exec, void *buffer, ObjcValueType type)
{
    ValueImp *aValue = NULL;

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
                    NSString *string = (NSString *)*obj;
                    aValue = convertNSStringToString (string);
                }
                else if (*obj == [WebUndefined undefined]) {
                    return Undefined();
                }
                else if ((CFBooleanRef)*obj == kCFBooleanTrue) {
                    aValue = Boolean(true);
                }
                else if ((CFBooleanRef)*obj == kCFBooleanFalse) {
                    aValue = Boolean(false);
                }
                else if ([*obj isKindOfClass:[NSNumber class]]) {
                    aValue = Number([*obj doubleValue]);
                }
                else if ([*obj isKindOfClass:[NSArray class]]) {
                    aValue = new RuntimeArrayImp(exec, new ObjcArray (*obj));
                }
                else if ([*obj isKindOfClass:[WebScriptObject class]]) {
                    WebScriptObject *jsobject = (WebScriptObject *)*obj;
                    aValue = [jsobject _imp];
                }
                else if (*obj == 0) {
                    return Undefined();
                }
                else {
		    aValue = Instance::createRuntimeObject(Instance::ObjectiveCLanguage, (void *)*obj);
                }
            }
            break;
        case ObjcCharType:
            {
                char *objcVal = (char *)buffer;
                aValue = Number ((short)*objcVal);
            }
            break;
        case ObjcShortType:
            {
                short *objcVal = (short *)buffer;
                aValue = Number ((short)*objcVal);
            }
            break;
        case ObjcIntType:
            {
                int *objcVal = (int *)buffer;
                aValue = Number ((int)*objcVal);
            }
            break;
        case ObjcLongType:
            {
                long *objcVal = (long *)buffer;
                aValue = Number ((long)*objcVal);
            }
            break;
        case ObjcFloatType:
            {
                float *objcVal = (float *)buffer;
                aValue = Number ((float)*objcVal);
            }
            break;
        case ObjcDoubleType:
            {
                double *objcVal = (double *)buffer;
                aValue = Number ((double)*objcVal);
            }
            break;
        default:
            // Should never get here.  Argument types are filtered (and
            // the assert above should have fired in the impossible case
            // of an invalid type anyway).
            fprintf (stderr, "%s:  invalid type (%d)\n", __PRETTY_FUNCTION__, (int)type);
            assert (true);
    }
    
    return aValue;
}


ObjcValueType Bindings::objcValueTypeForType (const char *type)
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


void *Bindings::createObjcInstanceForValue (ObjectImp *value, const RootObject *origin, const RootObject *current)
{
    if (!value->isObject())
	return 0;

    ObjectImp *imp = static_cast<ObjectImp*>(value);
    
    return [[[WebScriptObject alloc] _initWithObjectImp:imp originExecutionContext:origin executionContext:current] autorelease];
}
