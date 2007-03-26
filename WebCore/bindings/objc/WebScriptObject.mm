/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebScriptObjectPrivate.h"

#import "DOMInternal.h"
#import "WebCoreObjCExtras.h"
#import <JavaScriptCore/context.h>
#import <JavaScriptCore/objc_instance.h>
#import <JavaScriptCore/runtime_object.h>

using namespace KJS;
using namespace KJS::Bindings;

#define LOG_EXCEPTION(exec) \
    if (Interpreter::shouldPrintExceptions()) \
        printf("%s:%d:[%d]  JavaScript exception:  %s\n", __FILE__, __LINE__, getpid(), exec->exception()->toObject(exec)->get(exec, exec->propertyNames().message)->toString(exec).ascii());

@implementation WebScriptObjectPrivate

@end

@implementation WebScriptObject

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

static void _didExecute(WebScriptObject *obj)
{
    ASSERT(JSLock::lockCount() > 0);
    
    if (![obj _rootObject] || ![obj _rootObject]->isValid())
        return;

    ExecState* exec = [obj _rootObject]->interpreter()->globalExec();
    KJSDidExecuteFunctionPtr func = Instance::didExecuteFunction();
    if (func)
        func(exec, static_cast<JSObject*>([obj _rootObject]->interpreter()->globalObject()));
}

- (void)_setImp:(JSObject*)imp originRootObject:(PassRefPtr<RootObject>)originRootObject rootObject:(PassRefPtr<RootObject>)rootObject
{
    // This function should only be called once, as a (possibly lazy) initializer.
    ASSERT(!_private->imp);
    ASSERT(!_private->rootObject);
    ASSERT(!_private->originRootObject);

    _private->imp = imp;
    _private->rootObject = rootObject.releaseRef();
    _private->originRootObject = originRootObject.releaseRef();

    if(_private->rootObject)
        _private->rootObject->gcProtect(imp);
}

- (id)_initWithJSObject:(KJS::JSObject*)imp originRootObject:(PassRefPtr<KJS::Bindings::RootObject>)originRootObject rootObject:(PassRefPtr<KJS::Bindings::RootObject>)rootObject
{
    ASSERT(imp);

    self = [super init];
    _private = [[WebScriptObjectPrivate alloc] init];
    [self _setImp:imp originRootObject:originRootObject rootObject:rootObject];
    
    return self;
}

- (JSObject *)_imp
{
    // Associate the WebScriptObject with the JS wrapper for the ObjC DOM wrapper.
    // This is done on lazily, on demand.
    if (!_private->imp && _private->isCreatedByDOMWrapper)
        [self _initializeScriptDOMNodeImp];
    return _private->imp;
}

- (BOOL)_hasImp
{
    return _private->imp != nil;
}

- (RootObject*)_rootObject
{
    return _private->rootObject && _private->rootObject->isValid() ? _private->rootObject : 0;
}

- (RootObject *)_originRootObject
{
    return _private->originRootObject && _private->originRootObject->isValid() ? _private->originRootObject : 0;
}

- (BOOL)_isSafeScript
{
    if (!_private->originRootObject || !_private->rootObject)
        return true;

    if (!_private->originRootObject->isValid() || !_private->rootObject->isValid())
        return false;
        
    return _private->originRootObject->interpreter()->isSafeScript(_private->rootObject->interpreter());
}

- (void)dealloc
{
    if (_private->rootObject && _private->rootObject->isValid())
        _private->rootObject->gcUnprotect(_private->imp);

    if (_private->rootObject)
        _private->rootObject->deref();

    if (_private->originRootObject)
        _private->originRootObject->deref();

    [_private release];

    [super dealloc];
}

- (void)finalize
{
    if (_private->rootObject && _private->rootObject->isValid())
        _private->rootObject->gcUnprotect(_private->imp);

    if (_private->rootObject)
        _private->rootObject->deref();

    if (_private->originRootObject)
        _private->originRootObject->deref();

    [super finalize];
}

+ (BOOL)throwException:(NSString *)exceptionMessage
{
    JSLock lock;
    
    Interpreter *first, *interp = Interpreter::firstInterpreter();

    // This code assumes that we only ever have one running interpreter.  A
    // good assumption for now, as we depend on that elsewhere.  However,
    // in the future we may have the ability to run multiple interpreters,
    // in which case this will have to change.
    first = interp;
    do {
        if (!interp)
            return NO;

        // If the interpreter has a context, we set the exception.
        if (interp->context()) {
            ExecState *exec = interp->context()->execState();
            
            if (exec) {
                throwError(exec, GeneralError, exceptionMessage);
                return YES;
            }
        }
        interp = interp->nextInterpreter();
    } while (interp != first);
    
    return NO;
}

static List listFromNSArray(ExecState *exec, NSArray *array)
{
    int i, numObjects = array ? [array count] : 0;
    List aList;
    
    for (i = 0; i < numObjects; i++) {
        id anObject = [array objectAtIndex:i];
        aList.append(convertObjcValueToValue(exec, &anObject, ObjcObjectType));
    }
    return aList;
}

- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)args
{
    if (![self _rootObject])
        return nil;

    if (![self _isSafeScript])
        return nil;

    // Lookup the function object.
    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    
    JSValue *v = convertObjcValueToValue(exec, &name, ObjcObjectType);
    Identifier identifier(v->toString(exec));
    JSValue *func = [self _imp]->get(exec, identifier);

    if (!func || !func->isObject())
        // Maybe throw an exception here?
        return 0;

    // Call the function object.
    JSObject *funcImp = static_cast<JSObject*>(func);
    if (!funcImp->implementsCall())
        return 0;

    JSObject *thisObj = const_cast<JSObject*>([self _imp]);
    List argList = listFromNSArray(exec, args);
    JSValue *result = funcImp->call(exec, thisObj, argList);

    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        result = jsUndefined();
        exec->clearException();
    }

    // Convert and return the result of the function call.
    id resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];

    _didExecute(self);
        
    return resultObj;
}

- (id)evaluateWebScript:(NSString *)script
{
    if (![self _rootObject])
        return nil;
    
    if (![self _isSafeScript])
        return nil;
    
    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    ASSERT(!exec->hadException());

    JSValue *result;
    JSLock lock;
    
    JSValue *v = convertObjcValueToValue(exec, &script, ObjcObjectType);
    Completion completion = [self _rootObject]->interpreter()->evaluate(UString(), 0, v->toString(exec));
    ComplType type = completion.complType();
    
    if (type == Normal) {
        result = completion.value();
        if (!result)
            result = jsUndefined();
    } else
        result = jsUndefined();
    
    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        result = jsUndefined();
        exec->clearException();
    }
    
    id resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];
    
    _didExecute(self);
    
    return resultObj;
}

- (void)setValue:(id)value forKey:(NSString *)key
{
    if (![self _rootObject])
        return;

    if (![self _isSafeScript])
        return;

    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    JSValue *v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    [self _imp]->put(exec, Identifier(v->toString(exec)), convertObjcValueToValue(exec, &value, ObjcObjectType));

    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        exec->clearException();
    }

    _didExecute(self);
}

- (id)valueForKey:(NSString *)key
{
    if (![self _rootObject])
        return nil;
        
    if (![self _isSafeScript])
        return nil;

    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    JSValue *v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    JSValue *result = [self _imp]->get(exec, Identifier(v->toString(exec)));
    
    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        result = jsUndefined();
        exec->clearException();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];
    if ([resultObj isKindOfClass:[WebUndefined class]])
        resultObj = [super valueForKey:key];    // ensure correct not-applicable key behavior

    _didExecute(self);
    
    return resultObj;
}

- (void)removeWebScriptKey:(NSString *)key
{
    if (![self _rootObject])
        return;
        
    if (![self _isSafeScript])
        return;

    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    JSValue *v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    [self _imp]->deleteProperty(exec, Identifier(v->toString(exec)));

    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        exec->clearException();
    }

    _didExecute(self);
}

- (NSString *)stringRepresentation
{
    if (![self _rootObject])
        // This is a workaround for a gcc 3.3 internal compiler error.
        return @"Undefined";

    if (![self _isSafeScript])
        // This is a workaround for a gcc 3.3 internal compiler error.
        return @"Undefined";

    JSLock lock;
    JSObject *thisObj = const_cast<JSObject*>([self _imp]);
    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    
    id result = convertValueToObjcValue(exec, thisObj, ObjcObjectType).objectValue;

    id resultObj = [result description];

    _didExecute(self);

    return resultObj;
}

- (id)webScriptValueAtIndex:(unsigned)index
{
    if (![self _rootObject])
        return nil;

    if (![self _isSafeScript])
        return nil;

    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    JSValue *result = [self _imp]->get(exec, index);

    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        result = jsUndefined();
        exec->clearException();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];

    _didExecute(self);

    return resultObj;
}

- (void)setWebScriptValueAtIndex:(unsigned)index value:(id)value
{
    if (![self _rootObject])
        return;

    if (![self _isSafeScript])
        return;

    ExecState* exec = [self _rootObject]->interpreter()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    [self _imp]->put(exec, index, convertObjcValueToValue(exec, &value, ObjcObjectType));

    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        exec->clearException();
    }

    _didExecute(self);
}

- (void)setException:(NSString *)description
{
    if (![self _rootObject])
        return;

    JSLock lock;
    
    if ([self _rootObject]->interpreter()->context()) {
        ExecState *exec = [self _rootObject]->interpreter()->context()->execState();

        ASSERT(exec);
        throwError(exec, GeneralError, description);
    } else
        throwError([self _rootObject]->interpreter()->globalExec(), GeneralError, description);
}

+ (id)_convertValueToObjcValue:(JSValue*)value originRootObject:(RootObject*)originRootObject rootObject:(RootObject*)rootObject
{
    if (value->isObject()) {
        JSObject* object = static_cast<JSObject*>(value);
        Interpreter* interpreter = rootObject->interpreter();
        ExecState *exec = interpreter->globalExec();
        JSLock lock;
        
        if (object->classInfo() != &RuntimeObjectImp::info) {
            JSValue* runtimeObject = object->get(exec, "__apple_runtime_object");
            if (runtimeObject && runtimeObject->isObject())
                object = static_cast<RuntimeObjectImp*>(runtimeObject);
        }

        if (object->classInfo() == &RuntimeObjectImp::info) {
            RuntimeObjectImp* imp = static_cast<RuntimeObjectImp*>(object);
            ObjcInstance *instance = static_cast<ObjcInstance*>(imp->getInternalInstance());
            if (instance)
                return instance->getObject();
            return nil;
        }

        if (id domWrapper = WebCore::createDOMWrapper(object, originRootObject, rootObject))
            return domWrapper;

        return [[[WebScriptObject alloc] _initWithJSObject:object originRootObject:originRootObject rootObject:rootObject] autorelease];
    }

    if (value->isString()) {
        UString u = value->getString();
        return [NSString stringWithCharacters:(const unichar*)u.data() length:u.size()];
    }

    if (value->isNumber())
        return [NSNumber numberWithDouble:value->getNumber()];

    if (value->isBoolean())
        return [NSNumber numberWithBool:value->getBoolean()];

    if (value->isUndefined())
        return [WebUndefined undefined];

    // jsNull is not returned as NSNull because existing applications do not expect
    // that return value. Return as nil for compatibility. <rdar://problem/4651318> <rdar://problem/4701626>
    // Other types (e.g., UnspecifiedType) also return as nil.
    return nil;
}

@end

@interface WebScriptObject (WebKitCocoaBindings)

- (unsigned)_count;
- (id)objectAtIndex:(unsigned)index;

@end

@implementation WebScriptObject (WebKitCocoaBindings)

- (BOOL)_shouldRespondToCount
{
    if (_private->shouldRespondToCountSet)
        return _private->shouldRespondToCount;

    BOOL shouldRespondToCount = YES;
 
    @try {
        [self valueForKey:@"length"];
    } @catch (id e) {
        shouldRespondToCount = NO;
    }

    _private->shouldRespondToCount = shouldRespondToCount;
    _private->shouldRespondToCountSet = YES;

    return shouldRespondToCount;
}

- (IMP)methodForSelector:(SEL)selector
{
    if (selector == @selector(count:) && [self _shouldRespondToCount])
        selector = @selector(_count:);

    return [super methodForSelector:selector];
}

- (BOOL)respondsToSelector:(SEL)selector
{
    if (selector == @selector(count:) && [self _shouldRespondToCount])
        selector = @selector(_count:);

    return [super respondsToSelector:selector];
}


- (unsigned)_count
{
    id length = [self valueForKey:@"length"];
    if ([length respondsToSelector:@selector(intValue)])
        return [length intValue];
    else
        return 0;
}

- (id)objectAtIndex:(unsigned)index
{
    return [self webScriptValueAtIndex:index];
}

@end

@implementation WebUndefined

+ (id)allocWithZone:(NSZone *)zone
{
    static WebUndefined *sharedUndefined = 0;
    if (!sharedUndefined)
        sharedUndefined = [super allocWithZone:NULL];
    return sharedUndefined;
}

- (NSString *)description
{
    return @"undefined";
}

- (id)initWithCoder:(NSCoder *)coder
{
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
}

- (id)copyWithZone:(NSZone *)zone
{
    return self;
}

- (id)retain
{
    return self;
}

- (void)release
{
}

- (unsigned)retainCount
{
    return UINT_MAX;
}

- (id)autorelease
{
    return self;
}

- (void)dealloc
{
    ASSERT(false);
    return;
    [super dealloc]; // make -Wdealloc-check happy
}

+ (WebUndefined *)undefined
{
    return [WebUndefined allocWithZone:NULL];
}

@end
