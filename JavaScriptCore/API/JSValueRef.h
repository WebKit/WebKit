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
  A constant identifying the type of a JSValueRef.
  @constant kJSTypeUndefined the unique undefined value
  @constant kJSTypeNull the unique null value
  @constant kJSBoolean a primitive boolean value, one of true or false
  @constant kJSTypeNumber a primitive number value
  @constant kJSTypeString a primitive string value
  @constant kJSTypeObject an object (meaning that this JSValueRef is a JSObjectRef)
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
  @function JSValueGetType
  Get the type code for a particular JavaScript value
  @param value the JS value for which the type should be determined
  @result      a type code identifying the type
*/
JSTypeCode JSValueGetType(JSValueRef value);

/*!
  @function JSValueIsUndefined
  Determine if value is of type undefined
  @param value the JS value to check for undefined type
  @result      true if the value is undefined, false otherwise
*/
bool JSValueIsUndefined(JSValueRef value);

/*!
  @function JSValueIsNull
  Determine if value is of type null
  @param value the JS value to check for null type
  @result      true if the value is null, false otherwise
*/
bool JSValueIsNull(JSValueRef value);

/*!
  @function JSValueIsBoolean
  Determine if value is of type boolean
  @param value the JS value to check for boolean type
  @result      true if the value is a boolean, false otherwise
*/
bool JSValueIsBoolean(JSValueRef value);

/*!
  @function JSValueIsNumber
  Determine if value is of type number
  @param value the JS value to check for number type
  @result      true if the value is a number, false otherwise
*/
bool JSValueIsNumber(JSValueRef value);

/*!
  @function JSValueIsString
  Determine if value is of type string
  @param value the JS value to check for string type
  @result      true if the value is a string, false otherwise
*/
bool JSValueIsString(JSValueRef value);

/*!
  @function JSValueIsObject
  Determine if value is of type object
  @param value the JS value to check for object type
  @result      true if the value is an object, false otherwise
*/
bool JSValueIsObject(JSValueRef value);
bool JSValueIsObjectOfClass(JSValueRef value, JSClassRef jsClass);

// Comparing values

/*!
  @function JSValueIsEqual
  Check if two values are equal by JavaScript rules, as if compared by the JS == operator
  @param context the execution context to use 
  @param a       the first value to compare
  @param b       the second value to compare
  @result        true if the two values are equal, false otherwise
*/
bool JSValueIsEqual(JSContextRef context, JSValueRef a, JSValueRef b);

/*!
  @function JSValueIsStrictEqual
  Check if two values are strict equal by JavaScript rules, as if compared by the JS === operator
  @param context the execution context to use 
  @param a       the first value to compare
  @param b       the second value to compare
  @result        true if the two values are strict equal, false otherwise
*/
bool JSValueIsStrictEqual(JSContextRef context, JSValueRef a, JSValueRef b);

/*!
  @function JSValueIsInstanceOf
  Check if a value is an instance of a particular object; generally this means the object
  was used as the constructor for that instance
  @param context the execution context to use 
  @param value   the possible instance
  @param object  the possible constructor
  @result        true if value is an instance of object
*/
bool JSValueIsInstanceOf(JSContextRef context, JSValueRef value, JSObjectRef object);

// Creating values

/*!
  @function JSUndefinedMake
  Make a value of the undefined type.
  @result The unique undefined value.
*/
JSValueRef JSUndefinedMake(void);

/*!
  @function JSNullMake
  Make a value of the null type.
  @result the unique null value
*/
JSValueRef JSNullMake(void);

/*!
  @function JSBooleanMake
  Make a value of the boolean type.
  @param value whether the returned value should be true or false
  @result      a JS true or false boolean value, as appropriate
*/

JSValueRef JSBooleanMake(bool value);

/*!
  @function JSNumberMake
  Make a value of the number type.
  @param  value the numberic value of the number to make
  @result a JS number corresponding to value
*/
JSValueRef JSNumberMake(double value);

/*!
  @function JSStringMake
  Make a value of the string type.
  @param  buffer the internal string contents for the string value
  @result a JS string value that has the value of the buffer
*/
JSValueRef JSStringMake(JSCharBufferRef buffer);

// Converting to primitive values

/*!
  @function JSValueToBoolean
  Convert a JavaScript value to boolean and return the resulting boolean
  @param context the execution context to use 
  @param value   the value to convert
  @result        the boolean result of conversion
*/
bool JSValueToBoolean(JSContextRef context, JSValueRef value);

/*!
  @function JSValueToNumber
  Convert a JavaScript value to number and return the resulting number
  @param context the execution context to use 
  @param value   the value to convert
  @result        the numeric result of conversion, or NaN if conversion fails
*/
double JSValueToNumber(JSContextRef context, JSValueRef value);

/*!
  @function JSValueCopyStringValue
  Convert a JavaScript value to string and copy the resulting string into a newly allocated character buffer
  @param context the execution context to use
  @param value   the value to convert
  @result        a character buffer containing the result of conversion, or an empty character buffer if conversion fails
*/
JSCharBufferRef JSValueCopyStringValue(JSContextRef context, JSValueRef value);

/*!
  @function JSValueToObject
  Convert a JavaScript value to object and return the resulting object
  @param context the execution context to use 
  @param value   the value to convert
  @result        the object result of conversion, or NULL if conversion fails
*/
JSObjectRef JSValueToObject(JSContextRef context, JSValueRef value);

#ifdef __cplusplus
}
#endif

#endif // JSValueRef_h
