/*
 * Copyright (C) 2004 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "runtime_root.h"

#include "BridgeJSC.h"
#include "runtime_object.h"
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/Weak.h>
#include <JavaScriptCore/WeakInlines.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Bindings {

// This code attempts to solve two problems: (1) plug-ins leaking references to 
// JS and the DOM; (2) plug-ins holding stale references to JS and the DOM. Previous 
// comments in this file claimed that problem #1 was an issue in Java, in particular, 
// because Java, allegedly, didn't always call finalize when collecting an object.

typedef HashSet<RootObject*> RootObjectSet;

static RootObjectSet& rootObjectSet()
{
    static NeverDestroyed<RootObjectSet> staticRootObjectSet;
    return staticRootObjectSet;
}

// FIXME:  These two functions are a potential performance problem.  We could 
// fix them by adding a JSObject to RootObject dictionary.

RootObject* findProtectingRootObject(JSObject* jsObject)
{
    RootObjectSet::const_iterator end = rootObjectSet().end();
    for (RootObjectSet::const_iterator it = rootObjectSet().begin(); it != end; ++it) {
        if ((*it)->gcIsProtected(jsObject))
            return *it;
    }
    return 0;
}

RootObject* findRootObject(JSGlobalObject* globalObject)
{
    RootObjectSet::const_iterator end = rootObjectSet().end();
    for (RootObjectSet::const_iterator it = rootObjectSet().begin(); it != end; ++it) {
        if ((*it)->globalObject() == globalObject)
            return *it;
    }
    return 0;
}

RootObject::InvalidationCallback::~InvalidationCallback() = default;

Ref<RootObject> RootObject::create(const void* nativeHandle, JSGlobalObject* globalObject)
{
    return adoptRef(*new RootObject(nativeHandle, globalObject));
}

RootObject::RootObject(const void* nativeHandle, JSGlobalObject* globalObject)
    : m_isValid(true)
    , m_nativeHandle(nativeHandle)
    , m_globalObject(globalObject->vm(), globalObject)
{
    ASSERT(globalObject);
    rootObjectSet().add(this);
}

RootObject::~RootObject()
{
    if (m_isValid)
        invalidate();
}

void RootObject::invalidate()
{
    if (!m_isValid)
        return;

    {
        // Get the objects from the keys; the values might be nulled.
        // Safe because finalized runtime objects are removed from m_runtimeObjects by RootObject::finalize.
        for (RuntimeObject* runtimeObject : m_runtimeObjects.keys())
            runtimeObject->invalidate();

        m_runtimeObjects.clear();
    }

    m_isValid = false;

    m_nativeHandle = 0;
    m_globalObject.clear();

    {
        HashSet<InvalidationCallback*>::iterator end = m_invalidationCallbacks.end();
        for (HashSet<InvalidationCallback*>::iterator iter = m_invalidationCallbacks.begin(); iter != end; ++iter)
            (**iter)(this);

        m_invalidationCallbacks.clear();
    }

    ProtectCountSet::iterator end = m_protectCountSet.end();
    for (ProtectCountSet::iterator it = m_protectCountSet.begin(); it != end; ++it)
        JSC::gcUnprotect(it->key);
    m_protectCountSet.clear();

    rootObjectSet().remove(this);
}

void RootObject::gcProtect(JSObject* jsObject)
{
    ASSERT(m_isValid);
    
    if (!m_protectCountSet.contains(jsObject)) {
        JSC::JSLockHolder holder(&globalObject()->vm());
        JSC::gcProtect(jsObject);
    }
    m_protectCountSet.add(jsObject);
}

void RootObject::gcUnprotect(JSObject* jsObject)
{
    ASSERT(m_isValid);
    
    if (!jsObject)
        return;

    if (m_protectCountSet.count(jsObject) == 1) {
        JSC::JSLockHolder holder(&globalObject()->vm());
        JSC::gcUnprotect(jsObject);
    }
    m_protectCountSet.remove(jsObject);
}

bool RootObject::gcIsProtected(JSObject* jsObject)
{
    ASSERT(m_isValid);
    return m_protectCountSet.contains(jsObject);
}

const void* RootObject::nativeHandle() const 
{ 
    ASSERT(m_isValid);
    return m_nativeHandle; 
}

JSGlobalObject* RootObject::globalObject() const
{
    ASSERT(m_isValid);
    return m_globalObject.get();
}

void RootObject::updateGlobalObject(JSGlobalObject* globalObject)
{
    m_globalObject.set(globalObject->vm(), globalObject);
}

void RootObject::addRuntimeObject(VM&, RuntimeObject* object)
{
    ASSERT(m_isValid);
    weakAdd(m_runtimeObjects, object, JSC::Weak<RuntimeObject>(object, this));
}

void RootObject::removeRuntimeObject(RuntimeObject* object)
{
    if (!m_isValid)
        return;
    weakRemove(m_runtimeObjects, object, object);
}

void RootObject::finalize(JSC::Handle<JSC::Unknown> handle, void*)
{
    RuntimeObject* object = static_cast<RuntimeObject*>(handle.slot()->asCell());

    Ref<RootObject> protectedThis(*this);
    object->invalidate();
    weakRemove(m_runtimeObjects, object, object);
}

} } // namespace JSC::Bindings
