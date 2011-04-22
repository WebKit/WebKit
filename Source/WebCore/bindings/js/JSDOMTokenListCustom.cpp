/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "JSDOMTokenList.h"

#include "DOMTokenList.h"
#include "Element.h"
#include "JSNode.h"

using namespace JSC;

namespace WebCore {

class JSDOMTokenListOwner : public JSC::WeakHandleOwner {
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

bool JSDOMTokenListOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    JSDOMTokenList* jsDOMTokenList = static_cast<JSDOMTokenList*>(handle.get().asCell());
    if (!jsDOMTokenList->hasCustomProperties())
        return false;
    Element* element = jsDOMTokenList->impl()->element();
    if (!element)
        return false;
    return visitor.containsOpaqueRoot(root(element));
}

void JSDOMTokenListOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSDOMTokenList* jsDOMTokenList = static_cast<JSDOMTokenList*>(handle.get().asCell());
    DOMWrapperWorld* world = static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsDOMTokenList->impl(), jsDOMTokenList);
}

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld*, DOMTokenList*)
{
    DEFINE_STATIC_LOCAL(JSDOMTokenListOwner, jsDOMTokenListOwner, ());
    return &jsDOMTokenListOwner;
}

inline void* wrapperContext(DOMWrapperWorld* world, DOMTokenList*)
{
    return world;
}

JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMTokenList* impl)
{
    return wrap<JSDOMTokenList>(exec, globalObject, impl);
}

} // namespace WebCore
