/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#include <JavaScriptCore/internal.h>
#include <JavaScriptCore/list.h>
#include <JavaScriptCore/value.h>

#include <objc_jsobject.h>
#include <objc_instance.h>
#include <objc_utility.h>

#include <runtime_object.h>
#include <runtime_root.h>

using namespace KJS;
using namespace KJS::Bindings;


#ifdef JAVASCRIPT_OBJECT_IN_WEBCORE
static RootObject *rootForView(void *v)
{
    NSView *aView = (NSView *)v;
    WebCoreBridge *aBridge = [[WebCoreViewFactory sharedFactory] bridgeForView:aView];
    if (aBridge) {
        KWQKHTMLPart *part = [aBridge part];
        RootObject *root = new RootObject(v);    // The root gets deleted by JavaScriptCore.
        
        root->setRootObjectImp (static_cast<KJS::ObjectImp *>(KJS::Window::retrieveWindow(part)));
        root->setInterpreter (KJSProxy::proxy(part)->interpreter());
        part->addPluginRootObject (root);
            
        return root;
    }
    return 0;
}

JavaScriptObject *windowJavaScriptObject(RootObject *root)
{
    return [[[JavaScriptObject alloc] initWithObjectImp:root->rootObjectImp() root:root] autorelease];
}

typedef enum {
    ObjectiveCLanaguage = 1,
    CLanaguage
}

void *NPN_GetJavaScriptObjectForWindow (NPNativeLanguage lang)
void *NPN_GetJavaScriptObjectForSelf (NPNativeLanguage lang)

windowJavaScriptObject(root));

#endif

@interface JavaScriptObjectPrivate : NSObject
{
    KJS::ObjectImp *imp;
    const Bindings::RootObject *root;
}
@end

@implementation JavaScriptObjectPrivate
@end

@interface JavaScriptObject (ReallyPrivate)
- initWithObjectImp:(KJS::ObjectImp *)imp root:(const Bindings::RootObject *)root;
@end

@implementation JavaScriptObject

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

- initWithObjectImp:(KJS::ObjectImp *)imp root:(const Bindings::RootObject *)root
{
    assert (imp != 0);
    //assert (root != 0);

    self = [super init];

    _private = [[JavaScriptObjectPrivate alloc] init];
    _private->imp = imp;
    _private->root = root;    

    addNativeReference (root, imp);
    
    return self;
}

- (void)dealloc
{
    removeNativeReference (_private->imp);
    [_private release];
    [super dealloc];
}

- (KJS::ObjectImp *)imp
{
    return _private->imp;
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
        // Convert to a JavaScriptObject
        else {
            result = [[[JavaScriptObject alloc] initWithObjectImp:objectImp root:root] autorelease];
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

- (id)call:(NSString *)methodName arguments:(NSArray *)args
{
    // Lookup the function object.
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    
    Value v = convertObjcValueToValue(exec, &methodName, ObjcObjectType);
    Identifier identifier(v.toString(exec));
    Value func = _private->imp->get (exec, identifier);
    Interpreter::unlock();
    if (func.isNull() || func.type() == UndefinedType) {
        // Maybe throw an exception here?
        return 0;
    }

    // Call the function object.
    ObjectImp *funcImp = static_cast<ObjectImp*>(func.imp());
    Object thisObj = Object(const_cast<ObjectImp*>(_private->imp));
    List argList = listFromNSArray(exec, args);
    Interpreter::lock();
    Value result = funcImp->call (exec, thisObj, argList);
    Interpreter::unlock();

    // Convert and return the result of the function call.
    return [JavaScriptObject _convertValueToObjcValue:result root:_private->root];
}

- (id)evaluate:(NSString *)script
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Object thisObj = Object(const_cast<ObjectImp*>(_private->imp));
    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &script, ObjcObjectType);
    KJS::Value result = _private->root->interpreter()->evaluate(v.toString(exec)).value();
    Interpreter::unlock();
    return [JavaScriptObject _convertValueToObjcValue:result root:_private->root];
}

- (id)getMember:(NSString *)name
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &name, ObjcObjectType);
    Value result = _private->imp->get (exec, Identifier (v.toString(exec)));
    Interpreter::unlock();
    return [JavaScriptObject _convertValueToObjcValue:result root:_private->root];
}

- (void)setMember:(NSString *)name value:(id)value
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &name, ObjcObjectType);
   _private->imp->put (exec, Identifier (v.toString(exec)), (convertObjcValueToValue(exec, &value, ObjcObjectType)));
    Interpreter::unlock();
}

- (void)removeMember:(NSString *)name
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    Value v = convertObjcValueToValue(exec, &name, ObjcObjectType);
    _private->imp->deleteProperty (exec, Identifier (v.toString(exec)));
    Interpreter::unlock();
}

- (NSString *)toString
{
    Interpreter::lock();
    Object thisObj = Object(const_cast<ObjectImp*>(_private->imp));
    ExecState *exec = _private->root->interpreter()->globalExec();
    
    id result = convertValueToObjcValue(exec, thisObj, ObjcObjectType).objectValue;

    Interpreter::unlock();
    
    return [result description];
}

- (id)getSlot:(unsigned int)index
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = _private->imp->get (exec, (unsigned)index);
    Interpreter::unlock();

    return [JavaScriptObject _convertValueToObjcValue:result root:_private->root];
}

- (void)setSlot:(unsigned int)index value:(id)value
{
    ExecState *exec = _private->root->interpreter()->globalExec();
    Interpreter::lock();
    _private->imp->put (exec, (unsigned)index, (convertObjcValueToValue(exec, &value, ObjcObjectType)));
    Interpreter::unlock();
}

@end
