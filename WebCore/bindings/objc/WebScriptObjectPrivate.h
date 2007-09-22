/*
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.    
*/
#ifndef _WEB_SCRIPT_OBJECT_PRIVATE_H_
#define _WEB_SCRIPT_OBJECT_PRIVATE_H_

#import "WebScriptObject.h"

#include <JavaScriptCore/internal.h>
#include <JavaScriptCore/object.h>
#include <JavaScriptCore/runtime_root.h>
#include <JavaScriptCore/APICast.h>

namespace WebCore {
    NSObject* getJSWrapper(KJS::JSObject*);
    void addJSWrapper(NSObject* wrapper, KJS::JSObject*);
    void removeJSWrapper(KJS::JSObject*);
    id createJSWrapper(KJS::JSObject*, PassRefPtr<KJS::Bindings::RootObject> origin, PassRefPtr<KJS::Bindings::RootObject> root);
}

@interface WebScriptObject (Private)
+ (id)_convertValueToObjcValue:(KJS::JSValue*)value originRootObject:(KJS::Bindings::RootObject*)originRootObject rootObject:(KJS::Bindings::RootObject*)rootObject;
+ (id)scriptObjectForJSObject:(JSObjectRef)jsObject originRootObject:(KJS::Bindings::RootObject*)originRootObject rootObject:(KJS::Bindings::RootObject*)rootObject;
- (id)_init;
- (id)_initWithJSObject:(KJS::JSObject*)imp originRootObject:(PassRefPtr<KJS::Bindings::RootObject>)originRootObject rootObject:(PassRefPtr<KJS::Bindings::RootObject>)rootObject;
- (void)_setImp:(KJS::JSObject*)imp originRootObject:(PassRefPtr<KJS::Bindings::RootObject>)originRootObject rootObject:(PassRefPtr<KJS::Bindings::RootObject>)rootObject;
- (void)_setOriginRootObject:(PassRefPtr<KJS::Bindings::RootObject>)originRootObject andRootObject:(PassRefPtr<KJS::Bindings::RootObject>)rootObject;
- (void)_initializeScriptDOMNodeImp;
- (KJS::JSObject *)_imp;
- (BOOL)_hasImp;
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
