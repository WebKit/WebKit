/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "WebScriptObjectPrivate.h"

#import "internal.h"
#import "list.h"
#import "value.h"

#import "objc_jsobject.h"
#import "objc_instance.h"
#import "objc_utility.h"

#import "runtime_object.h"
#import "runtime_root.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_3

@interface NSObject (WebExtras)
- (void)finalize;
@end

#endif

using namespace KJS;
using namespace KJS::Bindings;

#define LOG_EXCEPTION(exec) \
    if (Interpreter::shouldPrintExceptions()) \
        printf("%s:%d:[%d]  JavaScript exception:  %s\n", __FILE__, __LINE__, getpid(), exec->exception()->toObject(exec)->get(exec, messagePropertyName)->toString(exec).ascii());

@implementation WebScriptObjectPrivate

@end

@implementation WebScriptObject

static void _didExecute(WebScriptObject *obj)
{
    ExecState *exec = [obj _executionContext]->interpreter()->globalExec();
    KJSDidExecuteFunctionPtr func = Instance::didExecuteFunction();
    if (func)
        func (exec, static_cast<JSObject*>([obj _executionContext]->rootObjectImp()));
}

- (void)_initializeWithObjectImp:(JSObject *)imp originExecutionContext:(const RootObject *)originExecutionContext executionContext:(const RootObject *)executionContext
{
    _private->imp = imp;
    _private->executionContext = executionContext;    
    _private->originExecutionContext = originExecutionContext;    

    addNativeReference (executionContext, imp);
}

- _initWithJSObject:(JSObject *)imp originExecutionContext:(const RootObject *)originExecutionContext executionContext:(const RootObject *)executionContext
{
    assert (imp != 0);
    //assert (root != 0);

    self = [super init];

    _private = [[WebScriptObjectPrivate alloc] init];

    [self _initializeWithObjectImp:imp originExecutionContext:originExecutionContext executionContext:executionContext];
    
    return self;
}

- (JSObject *)_imp
{
    if (!_private->imp && _private->isCreatedByDOMWrapper) {
        // Associate the WebScriptObject with the JS wrapper for the ObjC DOM
        // wrapper.  This is done on lazily, on demand.
        [self _initializeScriptDOMNodeImp];
    }
    return _private->imp;
}

- (const RootObject *)_executionContext
{
    return _private->executionContext;
}

- (void)_setExecutionContext:(const RootObject *)context
{
    _private->executionContext = context;
}


- (const RootObject *)_originExecutionContext
{
    return _private->originExecutionContext;
}

- (void)_setOriginExecutionContext:(const RootObject *)originExecutionContext
{
    _private->originExecutionContext = originExecutionContext;
}

- (BOOL)_isSafeScript
{
    if ([self _originExecutionContext]) {
	Interpreter *originInterpreter = [self _originExecutionContext]->interpreter();
	if (originInterpreter) {
	    return originInterpreter->isSafeScript ([self _executionContext]->interpreter());
	}
    }
    return true;
}

- (void)dealloc
{
    removeNativeReference(_private->imp);
    [_private release];
        
    [super dealloc];
}

- (void)finalize
{
    removeNativeReference(_private->imp);
        
    [super finalize];
}

+ (BOOL)throwException:(NSString *)exceptionMessage
{
    InterpreterImp *first, *interp = InterpreterImp::firstInterpreter();

    // This code assumes that we only ever have one running interpreter.  A
    // good assumption for now, as we depend on that elsewhere.  However,
    // in the future we may have the ability to run multiple interpreters,
    // in which case this will have to change.
    first = interp;
    do {
        ExecState *exec = interp->globalExec();
        // If the interpreter has a context, we set the exception.
        if (interp->context()) {
            throwError(exec, GeneralError, exceptionMessage);
            return YES;
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
        aList.append (convertObjcValueToValue(exec, &anObject, ObjcObjectType));
    }
    return aList;
}

- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)args
{
    if (![self _executionContext])
        return nil;

    if (![self _isSafeScript])
	return nil;

    // Lookup the function object.
    ExecState *exec = [self _executionContext]->interpreter()->globalExec();

    JSLock lock;
    
    JSValue *v = convertObjcValueToValue(exec, &name, ObjcObjectType);
    Identifier identifier(v->toString(exec));
    JSValue *func = [self _imp]->get (exec, identifier);

    if (!func || func->isUndefined()) {
        // Maybe throw an exception here?
        return 0;
    }

    // Call the function object.    
    JSObject *funcImp = static_cast<JSObject*>(func);
    JSObject *thisObj = const_cast<JSObject*>([self _imp]);
    List argList = listFromNSArray(exec, args);
    JSValue *result = funcImp->call (exec, thisObj, argList);

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = jsUndefined();
    }

    // Convert and return the result of the function call.
    id resultObj = [WebScriptObject _convertValueToObjcValue:result originExecutionContext:[self _originExecutionContext] executionContext:[self _executionContext]];

    _didExecute(self);
        
    return resultObj;
}

- (id)evaluateWebScript:(NSString *)script
{
    if (![self _executionContext])
        return nil;
    
    if (![self _isSafeScript])
	return nil;
    
    ExecState *exec = [self _executionContext]->interpreter()->globalExec();
    JSValue *result;
    
    JSLock lock;
    
    JSValue *v = convertObjcValueToValue(exec, &script, ObjcObjectType);
    Completion completion = [self _executionContext]->interpreter()->evaluate(UString(), 0, v->toString(exec));
    ComplType type = completion.complType();
    
    if (type == Normal) {
        result = completion.value();
        if (!result)
            result = jsUndefined();
    } else
        result = jsUndefined();
    
    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = jsUndefined();
    }
    
    id resultObj = [WebScriptObject _convertValueToObjcValue:result originExecutionContext:[self _originExecutionContext] executionContext:[self _executionContext]];
    
    _didExecute(self);
    
    return resultObj;
}

- (void)setValue:(id)value forKey:(NSString *)key
{
    if (![self _executionContext])
        return;

    if (![self _isSafeScript])
	return;

    ExecState *exec = [self _executionContext]->interpreter()->globalExec();

    JSLock lock;
    JSValue *v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    [self _imp]->put (exec, Identifier (v->toString(exec)), (convertObjcValueToValue(exec, &value, ObjcObjectType)));

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
    }

    _didExecute(self);
}

- (id)valueForKey:(NSString *)key
{
    if (![self _executionContext])
        return nil;
        
    if (![self _isSafeScript])
	return nil;

    ExecState *exec = [self _executionContext]->interpreter()->globalExec();

    JSLock lock;
    JSValue *v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    JSValue *result = [self _imp]->get (exec, Identifier (v->toString(exec)));
    
    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = jsUndefined();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result originExecutionContext:[self _originExecutionContext] executionContext:[self _executionContext]];

    _didExecute(self);
    
    return resultObj;
}

- (void)removeWebScriptKey:(NSString *)key
{
    if (![self _executionContext])
        return;
        
    if (![self _isSafeScript])
	return;

    ExecState *exec = [self _executionContext]->interpreter()->globalExec();

    JSLock lock;
    JSValue *v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    [self _imp]->deleteProperty (exec, Identifier (v->toString(exec)));

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
    }

    _didExecute(self);
}

- (NSString *)stringRepresentation
{
    if (![self _isSafeScript])
        // This is a workaround for a gcc 3.3 internal compiler error.
	return @"Undefined";

    JSLock lock;
    JSObject *thisObj = const_cast<JSObject*>([self _imp]);
    ExecState *exec = [self _executionContext]->interpreter()->globalExec();
    
    id result = convertValueToObjcValue(exec, thisObj, ObjcObjectType).objectValue;

    id resultObj = [result description];

    _didExecute(self);

    return resultObj;
}

- (id)webScriptValueAtIndex:(unsigned int)index
{
    if (![self _executionContext])
        return nil;

    if (![self _isSafeScript])
	return nil;

    ExecState *exec = [self _executionContext]->interpreter()->globalExec();
    JSLock lock;
    JSValue *result = [self _imp]->get (exec, (unsigned)index);

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = jsUndefined();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result originExecutionContext:[self _originExecutionContext] executionContext:[self _executionContext]];

    _didExecute(self);

    return resultObj;
}

- (void)setWebScriptValueAtIndex:(unsigned int)index value:(id)value
{
    if (![self _executionContext])
        return;

    if (![self _isSafeScript])
	return;

    ExecState *exec = [self _executionContext]->interpreter()->globalExec();
    JSLock lock;
    [self _imp]->put (exec, (unsigned)index, (convertObjcValueToValue(exec, &value, ObjcObjectType)));

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
    }

    _didExecute(self);
}

- (void)setException:(NSString *)description
{
    if (const RootObject *root = [self _executionContext])
        throwError(root->interpreter()->globalExec(), GeneralError, description);
}

+ (id)_convertValueToObjcValue:(JSValue *)value originExecutionContext:(const RootObject *)originExecutionContext executionContext:(const RootObject *)executionContext
{
    // First see if we have a ObjC instance.
    if (value->isObject()) {
        JSObject *objectImp = static_cast<JSObject*>(value);
	Interpreter *intepreter = executionContext->interpreter();
	ExecState *exec = intepreter->globalExec();
        JSLock lock;
	
        if (objectImp->classInfo() != &RuntimeObjectImp::info) {
	    JSValue *runtimeObject = objectImp->get(exec, "__apple_runtime_object");
	    if (runtimeObject && runtimeObject->isObject())
		objectImp = static_cast<RuntimeObjectImp*>(runtimeObject);
	}

        if (objectImp->classInfo() == &RuntimeObjectImp::info) {
            RuntimeObjectImp *imp = static_cast<RuntimeObjectImp *>(objectImp);
            ObjcInstance *instance = static_cast<ObjcInstance*>(imp->getInternalInstance());
            if (instance)
                return instance->getObject();
        } else
            // JS Object --> WebScriptObject
	    return (id)intepreter->createLanguageInstanceForValue (exec, Instance::ObjectiveCLanguage, value->toObject(exec), originExecutionContext, executionContext);
    } else if (value->isString()) {
        // JS String --> NSString
        UString u = value->getString();
        NSString *string = [NSString stringWithCharacters:(const unichar*)u.data() length:u.size()];
        return string;
    } else if (value->isNumber())
        // JS Number --> NSNumber
        return [NSNumber numberWithDouble:value->getNumber()];
    else if (value->isBoolean())
        // JS Boolean --> NSNumber
        return [NSNumber numberWithBool:value->getBoolean()];
    else if (value->isUndefined())
        // JS Undefined --> WebUndefined
        return [WebUndefined undefined];
    
    // Other types (UnspecifiedType and NullType) converted to 0.
    return 0;
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
    assert(false);
    return;
    [super dealloc]; // make -Wdealloc-check happy
}

+ (WebUndefined *)undefined
{
    return [WebUndefined allocWithZone:NULL];
}

@end
