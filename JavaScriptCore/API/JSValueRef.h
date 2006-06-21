// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef JSValueRef_h
#define JSValueRef_h

#include "JSBase.h"

// Value types
typedef enum {
    kJSTypeUndefined,
    kJSTypeNull,
    kJSTypeBoolean,
    kJSTypeNumber,
    kJSTypeString,
    kJSTypeObject
} JSTypeCode;

#ifdef __cplusplus
extern "C" {
#endif

JSTypeCode JSValueGetType(JSValueRef value);

bool JSValueIsUndefined(JSValueRef value);
bool JSValueIsNull(JSValueRef value);
bool JSValueIsBoolean(JSValueRef value);
bool JSValueIsNumber(JSValueRef value);
bool JSValueIsString(JSValueRef value);
bool JSValueIsObject(JSValueRef value);

// Comparing values
bool JSValueIsEqual(JSContextRef context, JSValueRef a, JSValueRef b);
bool JSValueIsStrictEqual(JSContextRef context, JSValueRef a, JSValueRef b);
bool JSValueIsInstanceOf(JSValueRef value, JSObjectRef object);

// Creating values
JSValueRef JSUndefinedMake(void);
JSValueRef JSNullMake(void);
JSValueRef JSBooleanMake(bool value);
JSValueRef JSNumberMake(double value);
JSValueRef JSStringMake(JSCharBufferRef buffer);

// Converting to primitive values
bool JSValueToBoolean(JSContextRef context, JSValueRef value);
double JSValueToNumber(JSContextRef context, JSValueRef value); // 0.0 if conversion fails
JSObjectRef JSValueToObject(JSContextRef context, JSValueRef value); // Error object if conversion fails
JSCharBufferRef JSValueCopyStringValue(JSContextRef context, JSValueRef value); // Empty string if conversion fails

#ifdef __cplusplus
}
#endif

#endif // JSValueRef_h
