/*
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/
#ifndef _WEB_SCRIPT_OBJECT_PRIVATE_H_
#define _WEB_SCRIPT_OBJECT_PRIVATE_H_

#import "WebScriptObject.h"

#include <JavaScriptCore/internal.h>
#include <JavaScriptCore/runtime_root.h>

@interface WebScriptObject (Private)
+ (id)_convertValueToObjcValue:(KJS::JSValue*)value originRootObject:(KJS::Bindings::RootObject*)originRootObject rootObject:(KJS::Bindings::RootObject*)rootObject;
- _init;
- _initWithJSObject:(KJS::JSObject*)imp originRootObject:(PassRefPtr<KJS::Bindings::RootObject>)originRootObject rootObject:(PassRefPtr<KJS::Bindings::RootObject>)rootObject;
- (void)_initializeWithObjectImp:(KJS::JSObject*)imp originRootObject:(PassRefPtr<KJS::Bindings::RootObject>)originRootObject rootObject:(PassRefPtr<KJS::Bindings::RootObject>)rootObject;
- (void)_initializeScriptDOMNodeImp;
- (KJS::JSObject *)_imp;
- (KJS::Bindings::RootObject*)_rootObject;
- (KJS::Bindings::RootObject*)_originRootObject;
@end

@interface WebScriptObjectPrivate : NSObject
{
@public
    KJS::JSObject *imp;
    KJS::Bindings::RootObject* rootObject;
    KJS::Bindings::RootObject* originRootObject;
    BOOL isCreatedByDOMWrapper;
}
@end


#endif
