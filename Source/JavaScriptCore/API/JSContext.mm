/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

#import "config.h"

#import "APICast.h"
#import "Completion.h"
#import "IntegrityInlines.h"
#import "JSAPIGlobalObject.h"
#import "JSBaseInternal.h"
#import "JSCInlines.h"
#import "JSContextInternal.h"
#import "JSContextPrivate.h"
#import "JSContextRefInternal.h"
#import "JSGlobalObject.h"
#import "JSInternalPromise.h"
#import "JSModuleLoader.h"
#import "JSRetainPtr.h"
#import "JSScriptInternal.h"
#import "JSValueInternal.h"
#import "JSVirtualMachineInternal.h"
#import "JSWrapperMap.h"
#import "JavaScriptCore.h"
#import "ObjcRuntimeExtras.h"
#import "StrongInlines.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if JSC_OBJC_API_ENABLED

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

@implementation JSContext {
    RetainPtr<JSVirtualMachine> m_virtualMachine;
    JSGlobalContextRef m_context;
    RetainPtr<JSValue> m_exception;
    WeakObjCPtr<id <JSModuleLoaderDelegate>> m_moduleLoaderDelegate;
}

- (JSGlobalContextRef)JSGlobalContextRef
{
    return m_context;
}

- (void)ensureWrapperMap
{
    if (!toJS([self JSGlobalContextRef])->wrapperMap()) {
        // The map will be retained by the GlobalObject in initialization.
        [[[JSWrapperMap alloc] initWithGlobalContextRef:[self JSGlobalContextRef]] release];
    }
}

- (instancetype)init
{
    return [self initWithVirtualMachine:adoptNS([[JSVirtualMachine alloc] init]).get()];
}

- (instancetype)initWithVirtualMachine:(JSVirtualMachine *)virtualMachine
{
    self = [super init];
    if (!self)
        return nil;

    m_virtualMachine = virtualMachine;
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
    m_exception = nil;
    JSGlobalContextRelease(m_context);
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

- (JSValue *)evaluateJSScript:(JSScript *)script
{
    JSC::JSGlobalObject* globalObject = toJS(m_context);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    if (script.type == kJSScriptTypeProgram) {
        JSValueRef exceptionValue = nullptr;
        JSC::SourceCode sourceCode = [script sourceCode];
        JSValueRef result = JSEvaluateScriptInternal(locker, m_context, nullptr, sourceCode, &exceptionValue);

        if (exceptionValue)
            return [self valueFromNotifyException:exceptionValue];
        return [JSValue valueWithJSValueRef:result inContext:self];
    }

    auto* apiGlobalObject = JSC::jsDynamicCast<JSC::JSAPIGlobalObject*>(globalObject);
    if (!apiGlobalObject)
        return [JSValue valueWithNewPromiseRejectedWithReason:[JSValue valueWithNewErrorFromMessage:@"Context does not support module loading" inContext:self] inContext:self];

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    JSC::JSValue result = apiGlobalObject->loadAndEvaluateJSScriptModule(locker, script);
    if (scope.exception()) {
        JSValueRef exceptionValue = toRef(apiGlobalObject, scope.exception()->value());
        scope.clearException();
        // FIXME: We should not clearException if it is the TerminationException.
        // https://bugs.webkit.org/show_bug.cgi?id=220821
        return [JSValue valueWithNewPromiseRejectedWithReason:[JSValue valueWithJSValueRef:exceptionValue inContext:self] inContext:self];
    }
    return [JSValue valueWithJSValueRef:toRef(vm, result) inContext:self];
}

- (JSValue *)dependencyIdentifiersForModuleJSScript:(JSScript *)script
{
    JSC::JSGlobalObject* globalObject = toJS(m_context);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    if (script.type != kJSScriptTypeModule) {
        self.exceptionHandler(self, [JSValue valueWithNewErrorFromMessage:@"script is not a module" inContext:self]);
        return [JSValue valueWithUndefinedInContext:self];
    }

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    JSC::JSArray* result = globalObject->moduleLoader()->dependencyKeysIfEvaluated(globalObject, JSC::jsString(vm, String([[script sourceURL] absoluteString])));
    if (scope.exception()) {
        JSValueRef exceptionValue = toRef(globalObject, scope.exception()->value());
        scope.clearException();
        return [self valueFromNotifyException:exceptionValue];
    }

    if (!result) {
        self.exceptionHandler(self, [JSValue valueWithNewErrorFromMessage:@"script has not run in context or was not evaluated successfully" inContext:self]);
        return [JSValue valueWithUndefinedInContext:self];
    }
    return [JSValue valueWithJSValueRef:toRef(vm, result) inContext:self];
}

- (void)_setITMLDebuggableType
{
    JSC::JSGlobalObject* globalObject = toJS(m_context);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);

    globalObject->setIsITML();
}

- (void)setException:(JSValue *)value
{
    JSC::JSGlobalObject* globalObject = toJS(m_context);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    m_exception = value;
}

- (JSValue *)exception
{
    return m_exception.get();
}

- (JSValue *)globalObject
{
    return [JSValue valueWithJSValueRef:JSContextGetGlobalObject(m_context) inContext:self];
}

+ (JSContext *)currentContext
{
    auto& thread = Thread::currentSingleton();
    CallbackData *entry = (CallbackData *)thread.m_apiData;
    return entry ? entry->context.get() : nil;
}

+ (JSValue *)currentThis
{
    auto& thread = Thread::currentSingleton();
    CallbackData *entry = (CallbackData *)thread.m_apiData;
    if (!entry)
        return nil;
    return [JSValue valueWithJSValueRef:entry->thisValue inContext:[JSContext currentContext]];
}

+ (JSValue *)currentCallee
{
    auto& thread = Thread::currentSingleton();
    CallbackData *entry = (CallbackData *)thread.m_apiData;
    // calleeValue may be null if we are initializing a promise.
    if (!entry || !entry->calleeValue)
        return nil;
    return [JSValue valueWithJSValueRef:entry->calleeValue inContext:[JSContext currentContext]];
}

+ (NSArray *)currentArguments
{
    auto& thread = Thread::currentSingleton();
    CallbackData *entry = (CallbackData *)thread.m_apiData;

    if (!entry)
        return nil;

    if (!entry->currentArguments) {
        JSContext *context = [JSContext currentContext];
        size_t count = entry->argumentCount;
        auto arguments = adoptNS([[NSMutableArray alloc] initWithCapacity:count]);
        for (size_t i = 0; i < count; ++i)
            [arguments setObject:[JSValue valueWithJSValueRef:entry->arguments[i] inContext:context] atIndexedSubscript:i];
        entry->currentArguments = WTF::move(arguments);
    }

    return entry->currentArguments.get();
}

- (JSVirtualMachine *)virtualMachine
{
    return m_virtualMachine.get();
}

- (NSString *)name
{
    // FIXME: This looks like a static analysis false positive (rdar://145661220).
    SUPPRESS_UNCOUNTED_ARG auto name = adopt(JSGlobalContextCopyName(m_context));
    if (!name)
        return nil;

    // FIXME: This looks like a static analysis false positive (rdar://145661220).
    SUPPRESS_UNCOUNTED_ARG return adoptCF(JSStringCopyCFString(kCFAllocatorDefault, name.get())).bridgingAutorelease();
}

- (void)setName:(NSString *)name
{
    JSGlobalContextSetName(m_context, OpaqueJSString::tryCreate(name).get());
}

- (BOOL)isInspectable
{
    return JSGlobalContextIsInspectable(m_context);
}

- (void)setInspectable:(BOOL)inspectable
{
    JSGlobalContextSetInspectable(m_context, inspectable);
}

- (BOOL)_remoteInspectionEnabled
{
    return self.inspectable;
}

- (void)_setRemoteInspectionEnabled:(BOOL)enabled
{
    self.inspectable = enabled;
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

    JSC::JSGlobalObject* globalObject = toJS(context);
    m_virtualMachine = [JSVirtualMachine virtualMachineWithContextGroupRef:toRef(&globalObject->vm())];
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
    auto& thread = Thread::currentSingleton();
    [self retain];
    CallbackData *prevStack = (CallbackData *)thread.m_apiData;
    *callbackData = CallbackData { prevStack, self, self.exception, calleeValue, thisValue, argumentCount, arguments, nil };
    thread.m_apiData = callbackData;
    self.exception = nil;
}

- (void)endCallbackWithData:(CallbackData *)callbackData
{
    auto& thread = Thread::currentSingleton();
    self.exception = callbackData->preservedException.get();
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
    return toJS(m_context)->wrapperMap();
}

- (JSValue *)wrapperForJSObject:(JSValueRef)value
{
    JSC::JSLockHolder locker(toJS(m_context));
    return [[self wrapperMap] objcWrapperForJSValueRef:value inContext:self];
}

+ (JSContext *)contextWithJSGlobalContextRef:(JSGlobalContextRef)globalContext
{
    JSVirtualMachine *virtualMachine = [JSVirtualMachine virtualMachineWithContextGroupRef:toRef(&toJS(globalContext)->vm())];
    auto context = retainPtr([virtualMachine contextForGlobalContextRef:globalContext]);
    if (!context)
        context = adoptNS([[JSContext alloc] initWithGlobalContextRef:globalContext]);
    return context.autorelease();
}

@end

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif
