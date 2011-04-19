/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSNamedNodeMap.h"

#include "JSNode.h"
#include "Element.h"
#include "NamedNodeMap.h"

using namespace JSC;

namespace WebCore {

class JSNamedNodeMapOwner : public JSC::WeakHandleOwner {
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::MarkStack&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

bool JSNamedNodeMapOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, MarkStack& markStack)
{
    JSNamedNodeMap* jsNamedNodeMap = static_cast<JSNamedNodeMap*>(handle.get().asCell());
    if (!jsNamedNodeMap->hasCustomProperties())
        return false;
    Element* element = jsNamedNodeMap->impl()->element();
    if (!element)
        return false;
    return markStack.containsOpaqueRoot(root(element));
}

void JSNamedNodeMapOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSNamedNodeMap* jsNamedNodeMap = static_cast<JSNamedNodeMap*>(handle.get().asCell());
    DOMWrapperWorld* world = static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsNamedNodeMap->impl(), jsNamedNodeMap);
}

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld*, NamedNodeMap*)
{
    DEFINE_STATIC_LOCAL(JSNamedNodeMapOwner, jsNamedNodeMapOwner, ());
    return &jsNamedNodeMapOwner;
}

inline void* wrapperContext(DOMWrapperWorld* world, NamedNodeMap*)
{
    return world;
}

bool JSNamedNodeMap::canGetItemsForName(ExecState*, NamedNodeMap* impl, const Identifier& propertyName)
{
    return impl->getNamedItem(identifierToString(propertyName));
}

JSValue JSNamedNodeMap::nameGetter(ExecState* exec, JSValue slotBase, const Identifier& propertyName)
{
    JSNamedNodeMap* thisObj = static_cast<JSNamedNodeMap*>(asObject(slotBase));
    return toJS(exec, thisObj->impl()->getNamedItem(identifierToString(propertyName)));
}

void JSNamedNodeMap::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    // We need to keep the wrapper for our underlying NamedNodeMap's element
    // alive because NamedNodeMap and Attr rely on the element for data, and
    // don't know how to keep it alive correctly.
    // FIXME: Fix this lifetime issue in the DOM, and remove this.
    Element* element = impl()->element();
    if (!element)
        return;
    markStack.addOpaqueRoot(root(element));
}

JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, NamedNodeMap* impl)
{
    return wrap<JSNamedNodeMap>(exec, globalObject, impl);
}

} // namespace WebCore
