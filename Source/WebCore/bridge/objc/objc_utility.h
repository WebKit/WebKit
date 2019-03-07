/*
 * Copyright (C) 2004-2019 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include "objc_header.h"
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSObject.h>

OBJC_CLASS NSString;

namespace JSC {
namespace Bindings {

typedef union {
    CFTypeRef objectValue;
    bool booleanValue;
    char charValue;
    short shortValue;
    int intValue;
    long longValue;
    long long longLongValue;
    float floatValue;
    double doubleValue;
} ObjcValue;

typedef enum {
    ObjcVoidType,
    ObjcObjectType,
    ObjcCharType,
    ObjcUnsignedCharType,
    ObjcShortType,
    ObjcUnsignedShortType,
    ObjcIntType,
    ObjcUnsignedIntType,
    ObjcLongType,
    ObjcUnsignedLongType,
    ObjcLongLongType,
    ObjcUnsignedLongLongType,
    ObjcFloatType,
    ObjcDoubleType,
    ObjcBoolType,
    ObjcInvalidType
} ObjcValueType;

class RootObject;

ObjcValue convertValueToObjcValue(ExecState*, JSValue, ObjcValueType);
JSValue convertNSStringToString(ExecState* exec, NSString *nsstring);
JSValue convertObjcValueToValue(ExecState*, void* buffer, ObjcValueType, RootObject*);
ObjcValueType objcValueTypeForType(const char *type);

Exception *throwError(ExecState*, ThrowScope&, NSString *message);

} // namespace Bindings
} // namespace JSC
