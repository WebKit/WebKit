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
#include "JSMediaList.h"

#include "JSNode.h"
#include "MediaList.h"

using namespace JSC;

namespace WebCore {

class JSMediaListOwner : public JSC::WeakHandleOwner {
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::MarkStack&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

bool JSMediaListOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, MarkStack& markStack)
{
    JSMediaList* jsMediaList = static_cast<JSMediaList*>(handle.get().asCell());
    if (!jsMediaList->hasCustomProperties())
        return false;
    return markStack.containsOpaqueRoot(root(jsMediaList->impl()));
}

void JSMediaListOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSMediaList* jsMediaList = static_cast<JSMediaList*>(handle.get().asCell());
    DOMWrapperWorld* world = static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsMediaList->impl(), jsMediaList);
}

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld*, MediaList*)
{
    DEFINE_STATIC_LOCAL(JSMediaListOwner, jsMediaListOwner, ());
    return &jsMediaListOwner;
}

inline void* wrapperContext(DOMWrapperWorld* world, MediaList*)
{
    return world;
}

JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, MediaList* rule)
{
    return wrap<JSMediaList>(exec, globalObject, rule);
}

} // namespace WebCore
