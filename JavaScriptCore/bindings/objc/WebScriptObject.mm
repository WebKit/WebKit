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

#import <JavaScriptCore/WebScriptObjectPrivate.h>

#include <JavaScriptCore/internal.h>
#include <JavaScriptCore/list.h>
#include <JavaScriptCore/value.h>

#include <objc_jsobject.h>
#include <objc_instance.h>
#include <objc_utility.h>

#include <runtime_object.h>
#include <runtime_root.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_3

@interface NSObject (WebExtras)
- (void)finalize;
@end

#endif

using namespace KJS;
using namespace KJS::Bindings;

#define LOG_EXCEPTION(exec) \
    if (Interpreter::shouldPrintExceptions()) \
        printf("%s:%d:[%d]  JavaScript exception:  %s\n", __FILE__, __LINE__, getpid(), exec->exception().toObject(exec).get(exec, messagePropertyName).toString(exec).ascii());

@implementation WebScriptObjectPrivate

@end

@implementation WebScriptObject

static void _didExecute(WebScriptObject *obj)
{
    ExecState *exec = [obj _executionContext]->interpreter()->globalExec();
    KJSDidExecuteFunctionPtr func = Instance::didExecuteFunction();
    if (func)
        func (exec, static_cast<KJS::ObjectImp*>([obj _executionContext]->rootObjectImp()));
}

- (void)_initializeWithObjectImp:(KJS::ObjectImp *)imp originExecutionContext:(const Bindings::RootObject *)originExecutionContext executionContext:(const Bindings::RootObject *)executionContext
{
    _private->imp = imp;
    _private->executionContext = executionContext;    
    _private->originExecutionContext = originExecutionContext;    

    addNativeReference (executionContext, imp);
}

- _initWithObjectImp:(KJS::ObjectImp *)imp originExecutionContext:(const Bindings::RootObject *)originExecutionContext executionContext:(const Bindings::RootObject *)executionContext
{
    assert (imp != 0);
    //assert (root != 0);

    self = [super init];

    _private = [[WebScriptObjectPrivate alloc] init];

    [self _initializeWithObjectImp:imp originExecutionContext:originExecutionContext executionContext:executionContext];
    
    return self;
}

- (KJS::ObjectImp *)_imp
{
    if (!_private->imp && _private->isCreatedByDOMWrapper) {
        // Associate the WebScriptObject with the JS wrapper for the ObjC DOM
        // wrapper.  This is done on lazily, on demand.
        [self _initializeScriptDOMNodeImp];
    }
    return _private->imp;
}

- (const KJS::Bindings::RootObject *)_executionContext
{
    return _private->executionContext;
}

- (void)_setExecutionContext:(const KJS::Bindings::RootObject *)context
{
    _private->executionContext = context;
}


- (const KJS::Bindings::RootObject *)_originExecutionContext
{
    return _private->originExecutionContext;
}

- (void)_setOriginExecutionContext:(const KJS::Bindings::RootObject *)originExecutionContext
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
            Object err = Error::create(exec, GeneralError, [exceptionMessage UTF8String]);
            exec->setException (err);
            return YES;
        }
        interp = interp->nextInterpreter();
    } while (interp != first);
    
    return NO;
}

static KJS::List listFromNSArray(ExecState *exec, NSArray *array)
{
    long i, numObjects = array ? [array count] : 0;
    KJS::List aList;
    
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

    Interpreter::lock();
    
    Value v = convertObjcValueToValue(exec, &name, ObjcObjectType);
    Identifier identifier(v.toString(exec));
    Value func = [self _imp]->get (exec, identifier);
    Interpreter::unlock();
    if (func.isNull() || func.type() == UndefinedType) {
        // Maybe throw an exception here?
        return 0;
    }

    // Call the function object.    
    Interpreter::lock();
    ObjectImp *funcImp = static_cast<ObjectImp*>(func.imp());
    Object thisObj = Object(const_cast<ObjectImp*>([self _imp]));
    List argList = listFromNSArray(exec, args);
    Value result = funcImp->call (exec, thisObj, argList);
    Interpreter::unlock();

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = Undefined();
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

    Object thisObj = Object(const_cast<ObjectImp*>([self _imp]));
    Value result;
    
    Interpreter::lock();
    
    Value v = convertObjcValueToValue(exec, &script, ObjcObjectType);
    Completion completion = [self _executionContext]->interpreter()->evaluate(UString(), 0, v.toString(exec));
    ComplType type = completion.complType();
    
    if (type == Normal) {
        result = completion.value();
        if (result.isNull()) {
            result = Undefined();
        }
    }
    else
        result = Undefined();

    Interpreter::unlock();
    
    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = Undefined();
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

    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    [self _imp]->put (exec, Identifier (v.toString(exec)), (convertObjcValueToValue(exec, &value, ObjcObjectType)));
    Interpreter::unlock();

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

    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    Value result = [self _imp]->get (exec, Identifier (v.toString(exec)));
    Interpreter::unlock();
    
    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = Undefined();
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

    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    [self _imp]->deleteProperty (exec, Identifier (v.toString(exec)));
    Interpreter::unlock();

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
    }

    _didExecute(self);
}

- (NSString *)stringRepresentation
{
    if (![self _isSafeScript])
        // This is a workaround for a gcc 3.3 internal compiler error.
	return [NSString stringWithCString:"Undefined" encoding:NSASCIIStringEncoding];

    Interpreter::lock();
    Object thisObj = Object(const_cast<ObjectImp*>([self _imp]));
    ExecState *exec = [self _executionContext]->interpreter()->globalExec();
    
    id result = convertValueToObjcValue(exec, thisObj, ObjcObjectType).objectValue;

    Interpreter::unlock();
    
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
    Interpreter::lock();
    Value result = [self _imp]->get (exec, (unsigned)index);
    Interpreter::unlock();

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = Undefined();
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
    Interpreter::lock();
    [self _imp]->put (exec, (unsigned)index, (convertObjcValueToValue(exec, &value, ObjcObjectType)));
    Interpreter::unlock();

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
    }

    _didExecute(self);
}

- (void)setException: (NSString *)description
{
    if (![self _executionContext])
        return;

    ExecState *exec = [self _executionContext]->interpreter()->globalExec();
    Object err = Error::create(exec, GeneralError, [description UTF8String]);
    exec->setException (err);
}

+ (id)_convertValueToObjcValue:(KJS::Value)value originExecutionContext:(const Bindings::RootObject *)originExecutionContext executionContext:(const Bindings::RootObject *)executionContext
{
    id result = 0;

    // First see if we have a ObjC instance.
    if (value.type() == KJS::ObjectType){
        ObjectImp *objectImp = static_cast<ObjectImp*>(value.imp());
	Interpreter *intepreter = executionContext->interpreter();
	ExecState *exec = intepreter->globalExec();

	if (objectImp->classInfo() != &KJS::RuntimeObjectImp::info) {
	    Value runtimeObject = objectImp->get(exec, "__apple_runtime_object");
	    if (!runtimeObject.isNull() && runtimeObject.type() == KJS::ObjectType)
		objectImp = static_cast<RuntimeObjectImp*>(runtimeObject.imp());
	}

        if (objectImp->classInfo() == &KJS::RuntimeObjectImp::info) {
            RuntimeObjectImp *imp = static_cast<RuntimeObjectImp *>(objectImp);
            ObjcInstance *instance = static_cast<ObjcInstance*>(imp->getInternalInstance());
            if (instance)
                result = instance->getObject();
        }
        // Convert to a WebScriptObject
        else {
	    result = (id)intepreter->createLanguageInstanceForValue (exec, Instance::ObjectiveCLanguage, value.toObject(exec), originExecutionContext, executionContext);
        }
    }
    
    // Convert JavaScript String value to NSString?
    else if (value.type() == KJS::StringType) {
        StringImp *s = static_cast<KJS::StringImp*>(value.imp());
        UString u = s->value();
        
        NSString *string = [NSString stringWithCharacters:(const unichar*)u.data() length:u.size()];
        result = string;
    }
    
    // Convert JavaScript Number value to NSNumber?
    else if (value.type() == KJS::NumberType) {
        Number n = Number::dynamicCast(value);
        result = [NSNumber numberWithDouble:n.value()];
    }
    
    else if (value.type() == KJS::BooleanType) {
        KJS::BooleanImp *b = static_cast<KJS::BooleanImp*>(value.imp());
        result = [NSNumber numberWithBool:b->value()];
    }
    
    // Convert JavaScript Undefined types to WebUndefined
    else if (value.type() == KJS::UndefinedType) {
        result = [WebUndefined undefined];
    }
    
    // Other types (UnspecifiedType and NullType) converted to 0.
    
    return result;
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
