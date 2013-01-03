/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JavaScript.h>

#if JS_OBJC_API_ENABLED

@class JSVirtualMachine, JSValue;

// An instance of JSContext represents a JavaScript execution environment. All
// JavaScript execution takes place within a context.
// JSContext is also used to manage the life-cycle of objects within the
// JavaScript virtual machine. Every instance of JSValue is associated with a
// JSContext via a weak reference. The JSValue will keep the JavaScript value it
// references alive so long as the JSContext is also retained. When an instance
// of JSContext is deallocated and the weak references to it are cleared the
// JSValues that had been associated with this context will be invalidated, and
// will cease to keep the values within the JavaScript engine alive.

NS_CLASS_AVAILABLE(10_9, NA)
@interface JSContext : NSObject

// Create a JSContext.
- (id)init;
// Create a JSContext in the specified virtual machine.
- (id)initWithVirtualMachine:(JSVirtualMachine *)virtualMachine;

// Evaluate a string of JavaScript code.
- (JSValue *)evaluateScript:(NSString *)script;

// This method retrieves the global object of the JavaScript execution context.
// Instances of JSContext originating from WebKit will return a reference to the
// WindowProxy object.
- (JSValue *)globalObject;

// This method may be called from within an Objective-C block or method invoked
// as a callback from JavaScript to retrieve the callback's context. Outside of
// a callback from JavaScript this method will return nil.
+ (JSContext *)currentContext;
// This method may be called from within an Objective-C block or method invoked
// as a callback from JavaScript to retrieve the callback's this value. Outside
// of a callback from JavaScript this method will return nil.
+ (JSValue *)currentThis;
// This method may be called from within an Objective-C block or method invoked
// as a callback from JavaScript to retrieve the callback's arguments, objects
// in the returned array are instances of JSValue. Outside of a callback from
// JavaScript this method will return nil.
+ (NSArray *)currentArguments;

// The "exception" property may be used to throw an exception to JavaScript.
// Before a callback is made from JavaScript to an Objective-C block or method,
// the prior value of the exception property will be preserved and the property
// will be set to nil. After the callback has completed the new value of the
// exception property will be read, and prior value restored. If the new value
// of exception is not nil, the callback will result in that value being thrown.
// This property may also be used to check for uncaught exceptions arising from
// API function calls (since the default behaviour of "exceptionHandler" is to
// assign ant uncaught exception to this property).
// If a JSValue orginating from a different JSVirtualMachine this this context
// is assigned to this property, an Objective-C exception will be raised.
@property(retain) JSValue *exception;

// If a call to an API function results in an uncaught JavaScript exception, the
// "exceptionHandler" block will be invoked. The default implementation for the
// exception handler will store the exception to the exception property on
// context. As a consequence the default behaviour is for unhandled exceptions
// occuring within a callback from JavaScript to be rethrown upon return.
// Setting this value to nil will result in all uncaught exceptions thrown from
// the API being silently consumed.
@property(copy) void(^exceptionHandler)(JSContext *context, JSValue *exception);

// All instances of JSContext are associated with a single JSVirtualMachine. The
// virtual machine provides an "object space" or set of execution resources.
@property(readonly, retain) JSVirtualMachine *virtualMachine;

@end

// Instances of JSContext implement the following methods in order to enable
// support for subscript access by key and index, for example:
//
//    JSContext *context;
//    JSValue *v = context[@"X"]; // Get value for "X" from the global object.
//    context[@"Y"] = v;          // Assign 'v' to "Y" on the global object.
//
// An object key passed as a subscript will be converted to a JavaScipt value,
// and then the value converted to a string used to resolve a property of the
// global object.
@interface JSContext(SubscriptSupport)

- (JSValue *)objectForKeyedSubscript:(id)key;
- (void)setObject:(id)object forKeyedSubscript:(NSObject <NSCopying> *)key;

@end

#endif
