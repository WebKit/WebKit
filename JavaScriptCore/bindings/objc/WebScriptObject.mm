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
        NSLog (@"%s:%d:  JavaScript exception:  %s\n", __FILE__, __LINE__, exec->exception().toObject(exec).get(exec, messagePropertyName).toString(exec).ascii());

@implementation WebScriptObjectPrivate

@end

@implementation WebScriptObject

static void _didExecute(WebScriptObject *obj)
{
    ExecState *exec = obj->_private->root->interpreter()->globalExec();
    KJSDidExecuteFunctionPtr func = Instance::didExecuteFunction();
    if (func)
        func (exec, static_cast<KJS::ObjectImp*>(obj->_private->root->rootObjectImp()));
}

- (void)_initializeWithObjectImp:(KJS::ObjectImp *)imp root:(const Bindings::RootObject *)root
{
    _private->imp = imp;
    _private->root = root;    

    addNativeReference (root, imp);
}

- _initWithObjectImp:(KJS::ObjectImp *)imp root:(const Bindings::RootObject *)root
{
    assert (imp != 0);
    //assert (root != 0);

    self = [super init];

    _private = [[WebScriptObjectPrivate alloc] init];

    [self _initializeWithObjectImp:imp root:root];
    
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
    NSLog (@"%s:%d:  not yet implemented", __PRETTY_FUNCTION__, __LINE__);
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
    // Lookup the function object.
    ExecState *exec = _private->root->interpreter()->globalExec();
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
    id resultObj = [WebScriptObject _convertValueToObjcValue:result root:_private->root];

    _didExecute(self);
        
    return resultObj;
}

- (id)evaluateWebScript:(NSString *)script
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Object thisObj = Object(const_cast<ObjectImp*>([self _imp]));
    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &script, ObjcObjectType);
    KJS::Value result = _private->root->interpreter()->evaluate(UString(), 0, v.toString(exec)).value();
    Interpreter::unlock();
    
    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = Undefined();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result root:_private->root];

    _didExecute(self);
    
    return resultObj;
}

- (void)setValue:(id)value forKey:(NSString *)key
{
    ExecState *exec = _private->root->interpreter()->globalExec();
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
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &key, ObjcObjectType);
    Value result = [self _imp]->get (exec, Identifier (v.toString(exec)));
    Interpreter::unlock();
    
    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = Undefined();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result root:_private->root];

    _didExecute(self);
    
    return resultObj;
}

- (void)removeWebScriptKey:(NSString *)key;
{
    ExecState *exec = _private->root->interpreter()->globalExec();
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
    Interpreter::lock();
    Object thisObj = Object(const_cast<ObjectImp*>([self _imp]));
    ExecState *exec = _private->root->interpreter()->globalExec();
    
    id result = convertValueToObjcValue(exec, thisObj, ObjcObjectType).objectValue;

    Interpreter::unlock();
    
    id resultObj = [result description];

    _didExecute(self);

    return resultObj;
}

- (id)webScriptValueAtIndex:(unsigned int)index;
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = [self _imp]->get (exec, (unsigned)index);
    Interpreter::unlock();

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
        result = Undefined();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result root:_private->root];

    _didExecute(self);

    return resultObj;
}

- (void)setWebScriptValueAtIndex:(unsigned int)index value:(id)value;
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    [self _imp]->put (exec, (unsigned)index, (convertObjcValueToValue(exec, &value, ObjcObjectType)));
    Interpreter::unlock();

    if (exec->hadException()) {
        LOG_EXCEPTION (exec);
    }

    _didExecute(self);
}

- (void)setException: (NSString *)description;
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Object err = Error::create(exec, GeneralError, [description UTF8String]);
    exec->setException (err);
}

+ (id)_convertValueToObjcValue:(KJS::Value)value root:(const Bindings::RootObject *)root
{
    id result = 0;

    // First see if we have a ObjC instance.
    if (value.type() == KJS::ObjectType){
        ObjectImp *objectImp = static_cast<ObjectImp*>(value.imp());
        if (strcmp(objectImp->classInfo()->className, "RuntimeObject") == 0) {
            RuntimeObjectImp *imp = static_cast<RuntimeObjectImp *>(value.imp());
            ObjcInstance *instance = static_cast<ObjcInstance*>(imp->getInternalInstance());
            if (instance)
                result = instance->getObject();
        }
        // Convert to a WebScriptObject
        else {
            result = [[[WebScriptObject alloc] _initWithObjectImp:objectImp root:root] autorelease];
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
    
    // Boolean?
    return result;
}

@end


@implementation WebUndefined

static WebUndefined *sharedUndefined = 0;

+ (WebUndefined *)undefined
{
    if (!sharedUndefined)
        sharedUndefined = [[WebUndefined alloc] init];
    return sharedUndefined;
}

- (id)initWithCoder:(NSCoder *)coder
{
    return [WebUndefined undefined];
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
}

- (id)copyWithZone:(NSZone *)zone
{
    return [WebUndefined undefined];
}

- (id)retain {
    return [WebUndefined undefined];
}

- (void)release {
}

- (unsigned)retainCount {
    return 0xFFFFFFFF;
}

- (id)autorelease {
    return [WebUndefined undefined];
}

- (void)dealloc {
}

- (id)copy {
    return [WebUndefined undefined];
}

- (id)replacementObjectForPortCoder:(NSPortCoder *)encoder {
    return [WebUndefined undefined];
}

@end
