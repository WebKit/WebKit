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
#import "Frame.h"
#import "PlatformString.h"
#import "WebCoreObjCExtras.h"
#import "WebCoreFrameBridge.h"
#import <JavaScriptCore/ExecState.h>
#import <JavaScriptCore/objc_instance.h>
#import <JavaScriptCore/runtime_object.h>
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/interpreter.h>

using namespace KJS;
using namespace KJS::Bindings;
using namespace WebCore;

#define LOG_EXCEPTION(exec) \
    if (Interpreter::shouldPrintExceptions()) \
        printf("%s:%d:[%d]  JavaScript exception:  %s\n", __FILE__, __LINE__, getpid(), exec->exception()->toObject(exec)->get(exec, exec->propertyNames().message)->toString(exec).ascii());

@interface WebFrame
- (WebCoreFrameBridge *)_bridge; // implemented in WebKit
@end

namespace WebCore {

typedef HashMap<JSObject*, NSObject*> JSWrapperMap;
static JSWrapperMap* JSWrapperCache;

NSObject* getJSWrapper(JSObject* impl)
{
    if (!JSWrapperCache)
        return nil;
    return JSWrapperCache->get(impl);
}

void addJSWrapper(NSObject* wrapper, JSObject* impl)
{
    if (!JSWrapperCache)
        JSWrapperCache = new JSWrapperMap;
    JSWrapperCache->set(impl, wrapper);
}

void removeJSWrapper(JSObject* impl)
{
    if (!JSWrapperCache)
        return;
    JSWrapperCache->remove(impl);
}

id createJSWrapper(KJS::JSObject* object, PassRefPtr<KJS::Bindings::RootObject> origin, PassRefPtr<KJS::Bindings::RootObject> root)
{
    if (id wrapper = getJSWrapper(object))
        return [[wrapper retain] autorelease];
    return [[[WebScriptObject alloc] _initWithJSObject:object originRootObject:origin rootObject:root] autorelease];
}

} // namespace WebCore

@implementation WebScriptObjectPrivate

@end

@implementation WebScriptObject

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

+ (id)scriptObjectForJSObject:(JSObjectRef)jsObject originRootObject:(RootObject*)originRootObject rootObject:(RootObject*)rootObject
{
    if (id domWrapper = WebCore::createDOMWrapper(toJS(jsObject), originRootObject, rootObject))
        return domWrapper;
    
    return WebCore::createJSWrapper(toJS(jsObject), originRootObject, rootObject);
}

static void _didExecute(WebScriptObject *obj)
{
    ASSERT(JSLock::lockCount() > 0);
    
    RootObject* root = [obj _rootObject];
    if (!root)
        return;

    ExecState* exec = root->globalObject()->globalExec();
    KJSDidExecuteFunctionPtr func = Instance::didExecuteFunction();
    if (func)
        func(exec, root->globalObject());
}

- (void)_setImp:(JSObject*)imp originRootObject:(PassRefPtr<RootObject>)originRootObject rootObject:(PassRefPtr<RootObject>)rootObject
{
    // This function should only be called once, as a (possibly lazy) initializer.
    ASSERT(!_private->imp);
    ASSERT(!_private->rootObject);
    ASSERT(!_private->originRootObject);
    ASSERT(imp);

    _private->imp = imp;
    _private->rootObject = rootObject.releaseRef();
    _private->originRootObject = originRootObject.releaseRef();

    WebCore::addJSWrapper(self, imp);

    if (_private->rootObject)
        _private->rootObject->gcProtect(imp);
}

- (void)_setOriginRootObject:(PassRefPtr<RootObject>)originRootObject andRootObject:(PassRefPtr<RootObject>)rootObject
{
    ASSERT(_private->imp);

    if (rootObject)
        rootObject->gcProtect(_private->imp);

    if (_private->rootObject && _private->rootObject->isValid())
        _private->rootObject->gcUnprotect(_private->imp);

    if (_private->rootObject)
        _private->rootObject->deref();

    if (_private->originRootObject)
        _private->originRootObject->deref();

    _private->rootObject = rootObject.releaseRef();
    _private->originRootObject = originRootObject.releaseRef();
}

- (id)_initWithJSObject:(KJS::JSObject*)imp originRootObject:(PassRefPtr<KJS::Bindings::RootObject>)originRootObject rootObject:(PassRefPtr<KJS::Bindings::RootObject>)rootObject
{
    ASSERT(imp);

    self = [super init];
    _private = [[WebScriptObjectPrivate alloc] init];
    [self _setImp:imp originRootObject:originRootObject rootObject:rootObject];
    
    return self;
}

- (JSObject*)_imp
{
    // Associate the WebScriptObject with the JS wrapper for the ObjC DOM wrapper.
    // This is done on lazily, on demand.
    if (!_private->imp && _private->isCreatedByDOMWrapper)
        [self _initializeScriptDOMNodeImp];
    return [self _rootObject] ? _private->imp : 0;
}

- (BOOL)_hasImp
{
    return _private->imp != nil;
}

// Node that DOMNode overrides this method. So you should almost always
// use this method call instead of _private->rootObject directly.
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
    RootObject *root = [self _rootObject];
    if (!root)
        return false;

    if (!_private->originRootObject)
        return true;

    if (!_private->originRootObject->isValid())
        return false;

    return root->globalObject()->allowsAccessFrom(_private->originRootObject->globalObject());
}

- (void)dealloc
{
    if (_private->imp)
        WebCore::removeJSWrapper(_private->imp);

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
    if (_private->imp)
        WebCore::removeJSWrapper(_private->imp);

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
    
    // This code assumes that we only ever have one running interpreter.  A
    // good assumption for now, as we depend on that elsewhere.  However,
    // in the future we may have the ability to run multiple interpreters,
    // in which case this will have to change.
    
    if (ExecState::activeExecStates().size()) {
        throwError(ExecState::activeExecStates().last(), GeneralError, exceptionMessage);
        return YES;
    }
    
    return NO;
}

static void getListFromNSArray(ExecState *exec, NSArray *array, RootObject* rootObject, List& aList)
{
    int i, numObjects = array ? [array count] : 0;
    
    for (i = 0; i < numObjects; i++) {
        id anObject = [array objectAtIndex:i];
        aList.append(convertObjcValueToValue(exec, &anObject, ObjcObjectType, rootObject));
    }
}

- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)args
{
    if (![self _isSafeScript])
        return nil;

    // Look up the function object.
    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    
    JSValue* func = [self _imp]->get(exec, String(name));

    if (!func || !func->isObject())
        // Maybe throw an exception here?
        return 0;

    // Call the function object.
    JSObject *funcImp = static_cast<JSObject*>(func);
    if (!funcImp->implementsCall())
        return 0;

    List argList;
    getListFromNSArray(exec, args, [self _rootObject], argList);

    if (![self _isSafeScript])
        return nil;

    [self _rootObject]->globalObject()->startTimeoutCheck();
    JSValue *result = funcImp->call(exec, [self _imp], argList);
    [self _rootObject]->globalObject()->stopTimeoutCheck();

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
    if (![self _isSafeScript])
        return nil;
    
    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    ASSERT(!exec->hadException());

    JSValue *result;
    JSLock lock;
    
    [self _rootObject]->globalObject()->startTimeoutCheck();
    Completion completion = Interpreter::evaluate([self _rootObject]->globalObject()->globalExec(), UString(), 0, String(script));
    [self _rootObject]->globalObject()->stopTimeoutCheck();
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
    if (![self _isSafeScript])
        return;

    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    [self _imp]->put(exec, String(key), convertObjcValueToValue(exec, &value, ObjcObjectType, [self _rootObject]));

    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        exec->clearException();
    }

    _didExecute(self);
}

- (id)valueForKey:(NSString *)key
{
    if (![self _isSafeScript])
        return nil;

    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    ASSERT(!exec->hadException());

    id resultObj;
    {
        // Need to scope this lock to ensure that we release the lock before calling
        // [super valueForKey:key] which might throw an exception and bypass the JSLock destructor,
        // leaving the lock permanently held
        JSLock lock;
        
        JSValue *result = [self _imp]->get(exec, String(key));
        
        if (exec->hadException()) {
            LOG_EXCEPTION(exec);
            result = jsUndefined();
            exec->clearException();
        }

        resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];
    }
    
    if ([resultObj isKindOfClass:[WebUndefined class]])
        resultObj = [super valueForKey:key];    // defaults to throwing an exception

    JSLock lock;
    _didExecute(self);
    
    return resultObj;
}

- (void)removeWebScriptKey:(NSString *)key
{
    if (![self _isSafeScript])
        return;

    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    [self _imp]->deleteProperty(exec, String(key));

    if (exec->hadException()) {
        LOG_EXCEPTION(exec);
        exec->clearException();
    }

    _didExecute(self);
}

- (NSString *)stringRepresentation
{
    if (![self _isSafeScript])
        // This is a workaround for a gcc 3.3 internal compiler error.
        return @"Undefined";

    JSLock lock;
    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    
    id result = convertValueToObjcValue(exec, [self _imp], ObjcObjectType).objectValue;

    NSString *description = [result description];

    _didExecute(self);

    return description;
}

- (id)webScriptValueAtIndex:(unsigned)index
{
    if (![self _isSafeScript])
        return nil;

    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
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
    if (![self _isSafeScript])
        return;

    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    ASSERT(!exec->hadException());

    JSLock lock;
    [self _imp]->put(exec, index, convertObjcValueToValue(exec, &value, ObjcObjectType, [self _rootObject]));

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

    ExecState* exec = 0;
    JSObject* globalObject = [self _rootObject]->globalObject();
    ExecStateStack::const_iterator end = ExecState::activeExecStates().end();
    for (ExecStateStack::const_iterator it = ExecState::activeExecStates().begin(); it != end; ++it)
        if ((*it)->dynamicGlobalObject() == globalObject)
            exec = *it;
            
    if (exec)
        throwError(exec, GeneralError, description);
}

- (JSObjectRef)JSObject
{
    if (![self _isSafeScript])
        return NULL;

    return toRef([self _imp]);
}

+ (id)_convertValueToObjcValue:(JSValue*)value originRootObject:(RootObject*)originRootObject rootObject:(RootObject*)rootObject
{
    if (value->isObject()) {
        JSObject* object = static_cast<JSObject*>(value);
        ExecState* exec = rootObject->globalObject()->globalExec();
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

        return [WebScriptObject scriptObjectForJSObject:toRef(object) originRootObject:originRootObject rootObject:rootObject];
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

- (id)objectAtIndex:(unsigned)index;

@end

@implementation WebScriptObject (WebKitCocoaBindings)

#if 0 
// FIXME: presence of 'count' method on WebScriptObject breaks Democracy player
//        http://bugs.webkit.org/show_bug.cgi?id=13129

- (unsigned)count
{
    id length = [self valueForKey:@"length"];
    if ([length respondsToSelector:@selector(intValue)])
        return [length intValue];
    else
        return 0;
}

#endif

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

