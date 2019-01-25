/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#include "config.h"

#import "APICast.h"
#import "Completion.h"
#import "JSCInlines.h"
#import "JSContextInternal.h"
#import "JSContextPrivate.h"
#import "JSContextRefInternal.h"
#import "JSGlobalObject.h"
#import "JSInternalPromise.h"
#import "JSModuleLoader.h"
#import "JSValueInternal.h"
#import "JSVirtualMachineInternal.h"
#import "JSWrapperMap.h"
#import "JavaScriptCore.h"
#import "ObjcRuntimeExtras.h"
#import "StrongInlines.h"

#import <wtf/WeakObjCPtr.h>

#if JSC_OBJC_API_ENABLED

@implementation JSContext {
    JSVirtualMachine *m_virtualMachine;
    JSGlobalContextRef m_context;
    JSC::Strong<JSC::JSObject> m_exception;
    WeakObjCPtr<id <JSModuleLoaderDelegate>> m_moduleLoaderDelegate;
}

- (JSGlobalContextRef)JSGlobalContextRef
{
    return m_context;
}

- (void)ensureWrapperMap
{
    if (!toJS([self JSGlobalContextRef])->lexicalGlobalObject()->wrapperMap()) {
        // The map will be retained by the GlobalObject in initialization.
        [[[JSWrapperMap alloc] initWithGlobalContextRef:[self JSGlobalContextRef]] release];
    }
}

- (instancetype)init
{
    return [self initWithVirtualMachine:[[[JSVirtualMachine alloc] init] autorelease]];
}

- (instancetype)initWithVirtualMachine:(JSVirtualMachine *)virtualMachine
{
    self = [super init];
    if (!self)
        return nil;

    m_virtualMachine = [virtualMachine retain];
    m_context = JSGlobalContextCreateInGroup(getGroupFromVirtualMachine(virtualMachine), 0);

    self.exceptionHandler = ^(JSContext *context, JSValue *exceptionValue) {
        context.exception = exceptionValue;
    };

    [self ensureWrapperMap];
    [m_virtualMachine addContext:self forGlobalContextRef:m_context];

    return self;
}

- (void)dealloc
{
    m_exception.clear();
    JSGlobalContextRelease(m_context);
    [m_virtualMachine release];
    [_exceptionHandler release];
    [super dealloc];
}

- (JSValue *)evaluateScript:(NSString *)script
{
    return [self evaluateScript:script withSourceURL:nil];
}

- (JSValue *)evaluateScript:(NSString *)script withSourceURL:(NSURL *)sourceURL
{
    JSValueRef exceptionValue = nullptr;
    auto scriptJS = OpaqueJSString::tryCreate(script);
    auto sourceURLJS = OpaqueJSString::tryCreate([sourceURL absoluteString]);
    JSValueRef result = JSEvaluateScript(m_context, scriptJS.get(), nullptr, sourceURLJS.get(), 0, &exceptionValue);

    if (exceptionValue)
        return [self valueFromNotifyException:exceptionValue];

    return [JSValue valueWithJSValueRef:result inContext:self];
}

- (void)setException:(JSValue *)value
{
    JSC::ExecState* exec = toJS(m_context);
    JSC::VM& vm = exec->vm();
    JSC::JSLockHolder locker(vm);
    if (value)
        m_exception.set(vm, toJS(JSValueToObject(m_context, valueInternalValue(value), 0)));
    else
        m_exception.clear();
}

- (JSValue *)exception
{
    if (!m_exception)
        return nil;
    return [JSValue valueWithJSValueRef:toRef(m_exception.get()) inContext:self];
}

- (JSValue *)globalObject
{
    return [JSValue valueWithJSValueRef:JSContextGetGlobalObject(m_context) inContext:self];
}

+ (JSContext *)currentContext
{
    Thread& thread = Thread::current();
    CallbackData *entry = (CallbackData *)thread.m_apiData;
    return entry ? entry->context : nil;
}

+ (JSValue *)currentThis
{
    Thread& thread = Thread::current();
    CallbackData *entry = (CallbackData *)thread.m_apiData;
    if (!entry)
        return nil;
    return [JSValue valueWithJSValueRef:entry->thisValue inContext:[JSContext currentContext]];
}

+ (JSValue *)currentCallee
{
    Thread& thread = Thread::current();
    CallbackData *entry = (CallbackData *)thread.m_apiData;
    // calleeValue may be null if we are initializing a promise.
    if (!entry || !entry->calleeValue)
        return nil;
    return [JSValue valueWithJSValueRef:entry->calleeValue inContext:[JSContext currentContext]];
}

+ (NSArray *)currentArguments
{
    Thread& thread = Thread::current();
    CallbackData *entry = (CallbackData *)thread.m_apiData;

    if (!entry)
        return nil;

    if (!entry->currentArguments) {
        JSContext *context = [JSContext currentContext];
        size_t count = entry->argumentCount;
        JSValue * argumentArray[count];
        for (size_t i =0; i < count; ++i)
            argumentArray[i] = [JSValue valueWithJSValueRef:entry->arguments[i] inContext:context];
        entry->currentArguments = [[NSArray alloc] initWithObjects:argumentArray count:count];
    }

    return entry->currentArguments;
}

- (JSVirtualMachine *)virtualMachine
{
    return m_virtualMachine;
}

- (NSString *)name
{
    JSStringRef name = JSGlobalContextCopyName(m_context);
    if (!name)
        return nil;

    return CFBridgingRelease(JSStringCopyCFString(kCFAllocatorDefault, name));
}

- (void)setName:(NSString *)name
{
    JSGlobalContextSetName(m_context, OpaqueJSString::tryCreate(name).get());
}

- (BOOL)_remoteInspectionEnabled
{
    return JSGlobalContextGetRemoteInspectionEnabled(m_context);
}

- (void)_setRemoteInspectionEnabled:(BOOL)enabled
{
    JSGlobalContextSetRemoteInspectionEnabled(m_context, enabled);
}

- (BOOL)_includesNativeCallStackWhenReportingExceptions
{
    return JSGlobalContextGetIncludesNativeCallStackWhenReportingExceptions(m_context);
}

- (void)_setIncludesNativeCallStackWhenReportingExceptions:(BOOL)includeNativeCallStack
{
    JSGlobalContextSetIncludesNativeCallStackWhenReportingExceptions(m_context, includeNativeCallStack);
}

- (CFRunLoopRef)_debuggerRunLoop
{
    return JSGlobalContextGetDebuggerRunLoop(m_context);
}

- (void)_setDebuggerRunLoop:(CFRunLoopRef)runLoop
{
    JSGlobalContextSetDebuggerRunLoop(m_context, runLoop);
}

- (id<JSModuleLoaderDelegate>)moduleLoaderDelegate
{
    return m_moduleLoaderDelegate.getAutoreleased();
}

- (void)setModuleLoaderDelegate:(id<JSModuleLoaderDelegate>)moduleLoaderDelegate
{
    m_moduleLoaderDelegate = moduleLoaderDelegate;
}

@end

@implementation JSContext(SubscriptSupport)

- (JSValue *)objectForKeyedSubscript:(id)key
{
    return [self globalObject][key];
}

- (void)setObject:(id)object forKeyedSubscript:(NSObject <NSCopying> *)key
{
    [self globalObject][key] = object;
}

@end

@implementation JSContext (Internal)

- (instancetype)initWithGlobalContextRef:(JSGlobalContextRef)context
{
    self = [super init];
    if (!self)
        return nil;

    JSC::JSGlobalObject* globalObject = toJS(context)->lexicalGlobalObject();
    m_virtualMachine = [[JSVirtualMachine virtualMachineWithContextGroupRef:toRef(&globalObject->vm())] retain];
    ASSERT(m_virtualMachine);
    m_context = JSGlobalContextRetain(context);
    [self ensureWrapperMap];

    self.exceptionHandler = ^(JSContext *context, JSValue *exceptionValue) {
        context.exception = exceptionValue;
    };

    [m_virtualMachine addContext:self forGlobalContextRef:m_context];

    return self;
}

- (void)notifyException:(JSValueRef)exceptionValue
{
    self.exceptionHandler(self, [JSValue valueWithJSValueRef:exceptionValue inContext:self]);
}

- (JSValue *)valueFromNotifyException:(JSValueRef)exceptionValue
{
    [self notifyException:exceptionValue];
    return [JSValue valueWithUndefinedInContext:self];
}

- (BOOL)boolFromNotifyException:(JSValueRef)exceptionValue
{
    [self notifyException:exceptionValue];
    return NO;
}

- (void)beginCallbackWithData:(CallbackData *)callbackData calleeValue:(JSValueRef)calleeValue thisValue:(JSValueRef)thisValue argumentCount:(size_t)argumentCount arguments:(const JSValueRef *)arguments
{
    Thread& thread = Thread::current();
    [self retain];
    CallbackData *prevStack = (CallbackData *)thread.m_apiData;
    *callbackData = (CallbackData){ prevStack, self, [self.exception retain], calleeValue, thisValue, argumentCount, arguments, nil };
    thread.m_apiData = callbackData;
    self.exception = nil;
}

- (void)endCallbackWithData:(CallbackData *)callbackData
{
    Thread& thread = Thread::current();
    self.exception = callbackData->preservedException;
    [callbackData->preservedException release];
    [callbackData->currentArguments release];
    thread.m_apiData = callbackData->next;
    [self release];
}

- (JSValue *)wrapperForObjCObject:(id)object
{
    JSC::JSLockHolder locker(toJS(m_context));
    return [[self wrapperMap] jsWrapperForObject:object inContext:self];
}

- (JSWrapperMap *)wrapperMap
{
    return toJS(m_context)->lexicalGlobalObject()->wrapperMap();
}

- (JSValue *)wrapperForJSObject:(JSValueRef)value
{
    JSC::JSLockHolder locker(toJS(m_context));
    return [[self wrapperMap] objcWrapperForJSValueRef:value inContext:self];
}

+ (JSContext *)contextWithJSGlobalContextRef:(JSGlobalContextRef)globalContext
{
    JSVirtualMachine *virtualMachine = [JSVirtualMachine virtualMachineWithContextGroupRef:toRef(&toJS(globalContext)->vm())];
    JSContext *context = [virtualMachine contextForGlobalContextRef:globalContext];
    if (!context)
        context = [[[JSContext alloc] initWithGlobalContextRef:globalContext] autorelease];
    return context;
}

@end

#endif
