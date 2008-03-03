/*
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef _WEB_SCRIPT_OBJECT_PRIVATE_H_
#define _WEB_SCRIPT_OBJECT_PRIVATE_H_

#import "WebScriptObject.h"

#import <wtf/PassRefPtr.h>

namespace KJS {
    
    class JSObject;
    class JSValue;
    
    namespace Bindings {
        class RootObject;
    }
}
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
