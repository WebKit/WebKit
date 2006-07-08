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

/*!
@enum JSTypeCode
@abstract     A constant identifying the type of a JSValue.
@constant     kJSTypeUndefined  The unique undefined value.
@constant     kJSTypeNull       The unique null value.
@constant     kJSTypeBoolean    A primitive boolean value, one of true or false.
@constant     kJSTypeNumber     A primitive number value.
@constant     kJSTypeString     A primitive string value.
@constant     kJSTypeObject     An object value (meaning that this JSValueRef is a JSObjectRef).
*/
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

/*!
@function
@abstract       Returns a JavaScript value's type code.
@param value    The JSValue whose type you want to obtain.
@result         A value of type JSTypeCode that identifies value's type.
*/
JSTypeCode JSValueGetType(JSValueRef value);

/*!
@function
@abstract       Tests whether a JavaScript value's type is the undefined type.
@param value    The JSValue to test.
@result         true if value's type is the undefined type, otherwise false.
*/
bool JSValueIsUndefined(JSValueRef value);

/*!
@function
@abstract       Tests whether a JavaScript value's type is the null type.
@param value    The JSValue to test.
@result         true if value's type is the null type, otherwise false.
*/
bool JSValueIsNull(JSValueRef value);

/*!
@function
@abstract       Tests whether a JavaScript value's type is the boolean type.
@param value    The JSValue to test.
@result         true if value's type is the boolean type, otherwise false.
*/
bool JSValueIsBoolean(JSValueRef value);

/*!
@function
@abstract       Tests whether a JavaScript value's type is the number type.
@param value    The JSValue to test.
@result         true if value's type is the number type, otherwise false.
*/
bool JSValueIsNumber(JSValueRef value);

/*!
@function
@abstract       Tests whether a JavaScript value's type is the string type.
@param value    The JSValue to test.
@result         true if value's type is the string type, otherwise false.
*/
bool JSValueIsString(JSValueRef value);

/*!
@function
@abstract       Tests whether a JavaScript value's type is the object type.
@param value    The JSValue to test.
@result         true if value's type is the object type, otherwise false.
*/
bool JSValueIsObject(JSValueRef value);

/*!
@function
@abstract       Tests whether a JavaScript value is an object with a given 
 class in its class chain.
@param value    The JSValue to test.
 @result        true if value is an object and has jsClass in its class chain, 
 otherwise false.
*/
bool JSValueIsObjectOfClass(JSValueRef value, JSClassRef jsClass);

// Comparing values

/*!
@function
@abstract       Tests whether two JavaScript values are equal, as compared by the JS == operator.
@param context  The execution context to use.
@param a        The first value to test.
@param b        The second value to test.
@result         true if the two values are equal, otherwise false.
*/
bool JSValueIsEqual(JSContextRef context, JSValueRef a, JSValueRef b);

/*!
@function
@abstract       Tests whether two JavaScript values are strict equal, as compared by the JS === operator.
@param context  The execution context to use.
@param a        The first value to test.
@param b        The second value to test.
@result         true if the two values are strict equal, otherwise false.
*/
bool JSValueIsStrictEqual(JSContextRef context, JSValueRef a, JSValueRef b);

/*!
@function
@abstract       Tests whether a JavaScript value is an object constructed by
 a given constructor, as compared by the JS instanceof operator.
@param context  The execution context to use.
@param value    The JSValue to test.
@param object   The constructor to test against.
@result         true if value is an object constructed by constructor, as compared
 by the JS instanceof operator, otherwise false.
*/
bool JSValueIsInstanceOf(JSContextRef context, JSValueRef value, JSObjectRef constructor);

// Creating values

/*!
@function
@abstract   Creates a JavaScript value of the undefined type.
@result     The unique undefined value.
*/
JSValueRef JSUndefinedMake(void);

/*!
@function
@abstract   Creates a JavaScript value of the null type.
@result     The unique null value.
*/
JSValueRef JSNullMake(void);

/*!
@function
@abstract       Creates a JavaScript value of the boolean type.
@param value    The boolean value to assign to the newly created JSValue.
@result         A JSValue of the boolean type, representing the boolean value of value.
*/

JSValueRef JSBooleanMake(bool value);

/*!
@function
@abstract       Creates a JavaScript value of the number type.
@param value    The numeric value to assign to the newly created JSValue.
@result         A JSValue of the number type, representing the numeric value of value.
*/
JSValueRef JSNumberMake(double value);

/*!
@function
@abstract       Creates a JavaScript value of the string type.
@param buffer   The JSStringBuffer to assign to the newly created JSValue. The
 newly created JSValue retains buffer, and releases it upon garbage collection.
@result         A JSValue of the string type, representing the string value of buffer.
*/
JSValueRef JSStringMake(JSStringBufferRef buffer);

// Converting to primitive values

/*!
@function
@abstract       Converts a JavaScript value to boolean and returns the resulting boolean.
@param context  The execution context to use.
@param value    The JSValue to convert.
@result         The boolean result of conversion.
*/
bool JSValueToBoolean(JSContextRef context, JSValueRef value);

/*!
@function
@abstract       Converts a JavaScript value to number and returns the resulting number.
@param context  The execution context to use.
@param value    The JSValue to convert.
@result         The numeric result of conversion, or NaN if conversion fails.
*/
double JSValueToNumber(JSContextRef context, JSValueRef value);

/*!
@function
@abstract       Converts a JavaScript value to string and copies the resulting
 string into a newly allocated JavaScript string buffer.
@param context  The execution context to use.
@param value    The JSValue to convert.
@result         A JSStringBuffer containing the result of conversion, or an empty
 string buffer if conversion fails. Ownership follows the copy rule.
*/
JSStringBufferRef JSValueCopyStringValue(JSContextRef context, JSValueRef value);

/*!
@function
@abstract Converts a JavaScript value to object and returns the resulting object.
@param context  The execution context to use.
@param value    The JSValue to convert.
@result         The JSObject result of conversion, or NULL if conversion fails.
*/
JSObjectRef JSValueToObject(JSContextRef context, JSValueRef value);

// Garbage collection
/*!
@function
@abstract       Protects a JavaScript value from garbage collection.
@param value    The JSValue to protect.
@discussion     A value may be protected multiple times and must be unprotected an
 equal number of times before becoming eligible for garbage collection.
*/
void JSGCProtect(JSValueRef value);

/*!
@function
@abstract       Unprotects a JavaScript value from garbage collection.
@param value    The JSValue to unprotect.
@discussion     A value may be protected multiple times and must be unprotected an 
 equal number of times before becoming eligible for garbage collection.
*/
void JSGCUnprotect(JSValueRef value);

/*!
@function
@abstract Performs a JavaScript garbage collection. 
@discussion JavaScript values that are on the machine stack, in a register, 
 protected by JSGCProtect, set as the global object of an execution context, or reachable from any such
 value will not be collected. It is not normally necessary to call this function 
 directly; the JS runtime will garbage collect as needed.
*/
void JSGCCollect(void);

#ifdef __cplusplus
}
#endif

#endif // JSValueRef_h
