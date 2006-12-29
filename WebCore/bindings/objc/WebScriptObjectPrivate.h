/*
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/
#ifndef _WEB_SCRIPT_OBJECT_PRIVATE_H_
#define _WEB_SCRIPT_OBJECT_PRIVATE_H_

#import "WebScriptObject.h"

#include <JavaScriptCore/internal.h>
#include <JavaScriptCore/runtime_root.h>

@interface WebScriptObject (Private)
+ (id)_convertValueToObjcValue:(KJS::JSValue*)value originRootObject:(const KJS::Bindings::RootObject*)originRootObject rootObject:(const KJS::Bindings::RootObject*)rootObject;
- _init;
- _initWithJSObject:(KJS::JSObject*)imp originRootObject:(const KJS::Bindings::RootObject*)originRootObject rootObject:(const KJS::Bindings::RootObject*)rootObject;
- (void)_initializeWithObjectImp:(KJS::JSObject*)imp originRootObject:(const KJS::Bindings::RootObject*)originRootObject rootObject:(const KJS::Bindings::RootObject*)rotObject;
- (void)_initializeScriptDOMNodeImp;
- (KJS::JSObject *)_imp;
- (void)_setrootObject:(const KJS::Bindings::RootObject *)context;
- (const KJS::Bindings::RootObject*)_rootObject;
- (void)_setOriginRootObject:(const KJS::Bindings::RootObject*)originRootObject;
- (const KJS::Bindings::RootObject*)_originRootObject;
@end

@interface WebScriptObjectPrivate : NSObject
{
@public
    KJS::JSObject *imp;
    const KJS::Bindings::RootObject* rootObject;
    const KJS::Bindings::RootObject* originRootObject;
    BOOL isCreatedByDOMWrapper;
}
@end


#endif
