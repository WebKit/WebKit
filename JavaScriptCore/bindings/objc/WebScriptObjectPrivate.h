/*
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/
#ifndef _WEB_SCRIPT_OBJECT_PRIVATE_H_
#define _WEB_SCRIPT_OBJECT_PRIVATE_H_

#import <JavaScriptCore/WebScriptObject.h>

#include <JavaScriptCore/internal.h>
#include <JavaScriptCore/runtime_root.h>

@interface WebScriptObject (Private)
+ (id)_convertValueToObjcValue:(KJS::JSValue *)value originExecutionContext:(const KJS::Bindings::RootObject *)originExecutionContext executionContext:(const KJS::Bindings::RootObject *)executionContext;
- _init;
- _initWithJSObject:(KJS::JSObject *)imp originExecutionContext:(const KJS::Bindings::RootObject *)originExecutionContext executionContext:(const KJS::Bindings::RootObject *)executionContext ;
- (void)_initializeWithObjectImp:(KJS::JSObject *)imp originExecutionContext:(const KJS::Bindings::RootObject *)originExecutionContext executionContext:(const KJS::Bindings::RootObject *)executionContext ;
- (void)_initializeScriptDOMNodeImp;
- (KJS::JSObject *)_imp;
- (void)_setExecutionContext:(const KJS::Bindings::RootObject *)context;
- (const KJS::Bindings::RootObject *)_executionContext;
- (void)_setOriginExecutionContext:(const KJS::Bindings::RootObject *)originExecutionContext;
- (const KJS::Bindings::RootObject *)_originExecutionContext;
@end

@interface WebScriptObjectPrivate : NSObject
{
@public
    KJS::JSObject *imp;
    const KJS::Bindings::RootObject *executionContext;
    const KJS::Bindings::RootObject *originExecutionContext;
    BOOL isCreatedByDOMWrapper;
}
@end


#endif
