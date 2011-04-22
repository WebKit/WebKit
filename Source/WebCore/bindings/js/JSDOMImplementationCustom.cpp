/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDOMImplementation.h"

#include "DOMImplementation.h"

using namespace JSC;

namespace WebCore {

class JSDOMImplementationOwner : public JSC::WeakHandleOwner {
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

bool JSDOMImplementationOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    JSDOMImplementation* jsDOMImplementation = static_cast<JSDOMImplementation*>(handle.get().asCell());
    if (!jsDOMImplementation->hasCustomProperties())
        return false;
    return visitor.containsOpaqueRoot(jsDOMImplementation->impl()->document());
}

void JSDOMImplementationOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSDOMImplementation* jsDOMImplementation = static_cast<JSDOMImplementation*>(handle.get().asCell());
    DOMWrapperWorld* world = static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsDOMImplementation->impl(), jsDOMImplementation);
}

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld*, DOMImplementation*)
{
    DEFINE_STATIC_LOCAL(JSDOMImplementationOwner, jsDOMImplementationOwner, ());
    return &jsDOMImplementationOwner;
}

inline void* wrapperContext(DOMWrapperWorld* world, DOMImplementation*)
{
    return world;
}

JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMImplementation* impl)
{
    return wrap<JSDOMImplementation>(exec, globalObject, impl);
}

}
