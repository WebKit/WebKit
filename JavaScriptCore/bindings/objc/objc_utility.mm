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

#include <JavascriptCore/internal.h>

#include <runtime_object.h>
#include <objc_instance.h>
#include <objc_utility.h>


using namespace KJS;
using namespace KJS::Bindings;

/*
    Convert ObjectiveC method names to palatable JavaScript names.  ":" in
    ObjectiveC names are converted to "_".  "_" are escaped.
    
    For example:
    
    logLevel:message:
    
    would turn into
    
    logLevel_message_
    
    This name mapping can be overriden by the ObjectiveC class by implementing

    + (NSString *)JavaScriptNameForSelector:(SEL)aSelector;
    
    See objc_jsobject.h for more details.
*/
void KJS::Bindings::JSMethodNameToObjCMethodName(const char *name, char *buffer, unsigned int len)
{
    const char *np = name;
    char *bp;

    if (strlen(name)*2+1 > len){
        *buffer = 0;
    }

    bp = buffer;
    while (*np) {
        if (*np == '_') {
            if (*(np+1) == '_') {
                *bp++ = '_';
                np += 2;
            }
            else {
                *bp++ = ':';
                np++;
            }
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
    Object          JavaScriptObject
    [], other       exception

*/
ObjcValue KJS::Bindings::convertValueToObjcValue (KJS::ExecState *exec, KJS::Value value, ObjcValueType type)
{
    ObjcValue result;
    double d = 0;
   
    d = value.toNumber(exec);
    switch (type){
        case ObjcObjectType: {
            result.objectValue = 0;
            
            // First see if we have a ObjC instance.
            if (value.type() == KJS::ObjectType){
                KJS::ObjectImp *objectImp = static_cast<KJS::ObjectImp*>(value.imp());
                if (strcmp(objectImp->classInfo()->className, "RuntimeObject") == 0) {
                    KJS::RuntimeObjectImp *imp = static_cast<KJS::RuntimeObjectImp *>(value.imp());
                    ObjcInstance *instance = static_cast<ObjcInstance*>(imp->getInternalInstance());
                    if (instance)
                        result.objectValue = instance->getObject();
                }
            }
            
            // Convert JavaScript String value to NSString?
            else if (value.type() == KJS::StringType) {
                KJS::StringImp *s = static_cast<KJS::StringImp*>(value.imp());
                UString u = s->value();
                
                NSString *string = [NSString stringWithCharacters:(const unichar*)u.data() length:u.size()];
                result.objectValue = string;
            }
            
            // FIXME:  Convert scalars to NSNumber.
            
            // FIXME:  Deal with other Object types by creating a JavaScriptObjects
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

/*

    ObjC      to    JavaScript
    char            Number
    short
    int
    long
    float
    double
    NSNumber        Number
    NSString        string
    NSArray         []
    id              wrapper
    other           should not happen

*/
Value KJS::Bindings::convertObjcValueToValue (KJS::ExecState *exec, void *buffer, ObjcValueType type)
{
    Value aValue;

    switch (type) {
        case ObjcObjectType:
            {
                ObjectStructPtr *obj = (ObjectStructPtr *)buffer;
                
                if ([*obj isKindOfClass:[NSString class]]){
                    NSString *string = (NSString *)*obj;
                    unichar *chars;
                    unsigned int length = [string length];
                    chars = (unichar *)malloc(sizeof(unichar)*length);
                    [string getCharacters:chars];
                    UString u((const KJS::UChar*)chars, length);
                    aValue = String (u);
                    free((void *)chars);
                }
                else if ([*obj isKindOfClass:[NSArray class]]) {
                    // FIXME:  Deal with NSArray to Array conversions.
                }
                else if ([*obj isKindOfClass:[NSNumber class]]) {
                    // FIXME:  Deal with NSNumber to Number conversions.
                }
                else {
                    aValue = Object(new RuntimeObjectImp(new ObjcInstance (*obj)));
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


ObjcValueType KJS::Bindings::objcValueTypeForType (const char *type)
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


