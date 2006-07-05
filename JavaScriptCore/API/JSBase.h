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

#ifndef JSBase_h
#define JSBase_h

/* JS runtime interface types */
typedef struct __JSContext* JSContextRef;
typedef struct __JSCharBuffer* JSCharBufferRef;
typedef struct __JSClass* JSClassRef;
typedef struct __JSPropertyList* JSPropertyListRef;
typedef struct __JSPropertyEnumerator* JSPropertyEnumeratorRef;

/* Base type of all JS values, and polymorphic functions on them */
typedef void* JSValueRef;

typedef struct __JSObject* JSObjectRef;

#ifdef __cplusplus
extern "C" {
#endif

// Evaluation
/*!
  @function JSEvaluate
  Evaluates a string of JavaScript
  @param context            execution context to use
  @param script             a character buffer containing the JavaScript to evaluate
  @param thisValue          object to use as "this," or NULL to use the global object as "this"
  @param sourceURL          URL to the file containing the JavaScript, or NULL - this is only used for error reporting
  @param startingLineNumber the JavaScript's starting line number in the file located at sourceURL - this is only used for error reporting
  @param exception          pointer to a JSValueRef in which to store an uncaught exception, if any; can be NULL
  @result                   result of evaluation, or NULL if an uncaught exception was thrown
*/
JSValueRef JSEvaluate(JSContextRef context, JSCharBufferRef script, JSValueRef thisValue, JSCharBufferRef sourceURL, int startingLineNumber, JSValueRef* exception);

/*!
  @function JSCheckSyntax
  Check for syntax errors in a string of JavaScript
  @param context            execution context to use
  @param script             a character buffer containing the JavaScript to evaluate
  @param sourceURL          URL to the file containing the JavaScript, or NULL - this is only used for error reporting
  @param startingLineNumber the JavaScript's starting line number in the file located at sourceURL - this is only used for error reporting
  @param exception          pointer to a JSValueRef in which to store a syntax error, if any; can be NULL
  @result                   true if the script is syntactically correct, false otherwise

*/
bool JSCheckSyntax(JSContextRef context, JSCharBufferRef script, JSCharBufferRef sourceURL, int startingLineNumber, JSValueRef* exception);

// Garbage collection
/*!
  @function JSGCProtect
  Protect a JavaScript value from garbage collection; a value may be
  protected multiple times and must be unprotected an equal number of
  times to become collectable again.
*/
void JSGCProtect(JSValueRef value);

/*!
  @function JSGCProtect
  Stop protecting a JavaScript value from garbage collection; a value may be
  protected multiple times and must be unprotected an equal number of
  times to become collectable again.
*/
void JSGCUnprotect(JSValueRef value);

/*! 
  @function JSGCCollect
  Immediately perform a JavaScript garbage collection. JavaScript
  values that are on the machine stack, in a register, protected, set
  as the global object of any interpreter, or reachable from any such
  value will not be collected. It is not normally necessary to call
  this function directly; the JS runtime will garbage collect as
  needed.
*/
void JSGCCollect(void);

#ifdef __cplusplus
}
#endif

#endif // JSBase_h
