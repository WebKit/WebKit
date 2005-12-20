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

#ifndef KJS_BINDINGS_OBJC_UTILITY_H
#define KJS_BINDINGS_OBJC_UTILITY_H

#include <CoreFoundation/CoreFoundation.h>

#include "object.h"
#include "objc_header.h"

#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif

namespace KJS {
namespace Bindings {

typedef union {
    ObjectStructPtr objectValue;
    bool booleanValue;
    char charValue;
    short shortValue;
    int intValue;
    long longValue;
    float floatValue;
    double doubleValue;
} ObjcValue;

typedef enum {
    ObjcVoidType,
    ObjcObjectType,
    ObjcCharType,
    ObjcShortType,
    ObjcIntType,
    ObjcLongType,
    ObjcFloatType,
    ObjcDoubleType,
    ObjcInvalidType
} ObjcValueType;

class RootObject;

ObjcValue convertValueToObjcValue(ExecState *exec, JSValue *value, ObjcValueType type);
JSValue *convertNSStringToString(NSString *nsstring);
JSValue *convertObjcValueToValue(ExecState *exec, void *buffer, ObjcValueType type);
ObjcValueType objcValueTypeForType(const char *type);

bool convertJSMethodNameToObjc(const char *JSName, char *buffer, size_t bufferSize);

void *createObjcInstanceForValue(JSValue *value, const RootObject *origin, const RootObject *current);

JSObject *throwError(ExecState *, ErrorType, NSString *message);

} // namespace Bindings
} // namespace KJS

#endif
