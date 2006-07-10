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

#ifndef JSContextRef_h
#define JSContextRef_h

#include "JSObjectRef.h"
#include "JSValueRef.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
@function
@abstract Creates a JavaScript execution context.
@discussion JSContextCreate allocates a global object and populates it with all the
 built-in JavaScript objects, such as Object, Function, String, and Array.
@param globalObjectClass The class to use when creating the JSContext's global object.
 Pass NULL to use the default object class.
@result A JSContext with a global object of class globalObjectClass.
*/
JSContextRef JSContextCreate(JSClassRef globalObjectClass);

/*!
@function
@abstract       Destroys a JavaScript execution context, freeing its resources.
@param context  The JSContext to destroy.
*/
void JSContextDestroy(JSContextRef context);

/*!
@function
@abstract       Returns the global object of a JavaScript execution context.
@param context  The JSContext whose global object you want to retrieve.
@result         context's global object.
*/
JSObjectRef JSContextGetGlobalObject(JSContextRef context);

/*!
@function
@abstract       Returns the current exception in a JavaScript execution context.
@param context  The JSContext whose exception you want to retrieve.
@result         A JSValue representing context's exception, or NULL if no exception has been set.
*/
JSValueRef JSContextGetException(JSContextRef context);
/*!
@function
@abstract       Sets an exception in a JavaScript execution context.
@param context  The JSContext whose exception you want to set.
@param value    The exception you want to set.
*/
void JSContextSetException(JSContextRef context, JSValueRef value);
/*!
@function
@abstract       Clears the exception in a JavaScript execution context.
@param context  The JSContext whose exception you want to clear.
*/
void JSContextClearException(JSContextRef context);

// Evaluation
/*!
@function
@abstract                 Evaluates a string of JavaScript.
@param context            The execution context to use.
@param script             A JSInternalString containing the script to evaluate.
@param thisObject         The object to use as "this," or NULL to use the global object as "this."
@param sourceURL          A JSInternalString containing a URL for the script's source file. This is only used when reporting exceptions. Pass NULL if you do not care to include source file information in exceptions.
@param startingLineNumber An integer value specifying the script's starting line number in the file located at sourceURL. This is only used when reporting exceptions.
@param exception          A pointer to a JSValueRef in which to store an uncaught exception, if any. Pass NULL if you do not care to store an uncaught exception.
@result                   The JSValue that results from evaluating script, or NULL if an uncaught exception is thrown.
*/
JSValueRef JSEvaluate(JSContextRef context, JSInternalStringRef script, JSObjectRef thisObject, JSInternalStringRef sourceURL, int startingLineNumber, JSValueRef* exception);

/*!
@function JSCheckSyntax
@abstract                 Checks for syntax errors in a string of JavaScript.
@param context            The execution context to use.
@param script             A JSInternalString containing the JavaScript to check for syntax errors.
@param sourceURL          A JSInternalString containing a URL for the script's source file. This is only used when reporting exceptions. Pass NULL if you do not care to include source file information in exceptions.
@param startingLineNumber An integer value specifying the script's starting line number in the file located at sourceURL. This is only used when reporting exceptions.
@param exception          A pointer to a JSValueRef in which to store a syntax error exception, if any. Pass NULL if you do not care to store a syntax error exception.
@result                   true if the script is syntactically correct, otherwise false.
*/
bool JSCheckSyntax(JSContextRef context, JSInternalStringRef script, JSInternalStringRef sourceURL, int startingLineNumber, JSValueRef* exception);

#ifdef __cplusplus
}
#endif

#endif // JSContextRef_h
