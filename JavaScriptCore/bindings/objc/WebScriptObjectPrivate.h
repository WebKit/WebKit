/*
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/
#ifndef _WEB_SCRIPT_OBJECT_PRIVATE_H_
#define _WEB_SCRIPT_OBJECT_PRIVATE_H_

#import <JavaScriptCore/WebScriptObject.h>

#include <JavaScriptCore/internal.h>
#include <JavaScriptCore/list.h>
#include <JavaScriptCore/object.h>
#include <JavaScriptCore/runtime_root.h>
#include <JavaScriptCore/value.h>

@interface WebScriptObject (Private)
+ (id)_convertValueToObjcValue:(KJS::Value)value root:(const KJS::Bindings::RootObject *)root;
- _init;
- _initWithObjectImp:(KJS::ObjectImp *)imp root:(const KJS::Bindings::RootObject *)root;
- (void)_initializeWithObjectImp:(KJS::ObjectImp *)imp root:(const KJS::Bindings::RootObject *)root;
- (void)_initializeScriptDOMNodeImp;
- (KJS::ObjectImp *)_imp;
@end

@interface WebScriptObjectPrivate : NSObject
{
    KJS::ObjectImp *imp;
    const KJS::Bindings::RootObject *root;
    BOOL isCreatedByDOMWrapper;
}
@end


#endif
