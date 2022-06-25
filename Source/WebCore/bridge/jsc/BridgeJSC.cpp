/*
 * Copyright (C) 2003, 2006, 2008, 2015 Apple Inc. All rights reserved.
 * Copyright 2010, The Android Open Source Project
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
#include "BridgeJSC.h"

#include "DOMWindow.h"
#include "JSDOMWindowBase.h"
#include "runtime_object.h"
#include "runtime_root.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ObjectPrototype.h>

namespace JSC {

namespace Bindings {

Array::Array(RefPtr<RootObject>&& rootObject)
    : m_rootObject(WTFMove(rootObject))
{
    ASSERT(m_rootObject);
}

Array::~Array() = default;

Instance::Instance(RefPtr<RootObject>&& rootObject)
    : m_rootObject(WTFMove(rootObject))
{
    ASSERT(m_rootObject);
}

Instance::~Instance()
{
    ASSERT(!m_runtimeObject);
}

void Instance::begin()
{
    virtualBegin();
}

void Instance::end()
{
    virtualEnd();
}

JSObject* Instance::createRuntimeObject(JSGlobalObject* lexicalGlobalObject)
{
    ASSERT(m_rootObject);
    ASSERT(m_rootObject->isValid());
    if (RuntimeObject* existingObject = m_runtimeObject.get())
        return existingObject;

    JSLockHolder lock(lexicalGlobalObject);
    RuntimeObject* newObject = newRuntimeObject(lexicalGlobalObject);
    m_runtimeObject = JSC::Weak<RuntimeObject>(newObject);
    m_rootObject->addRuntimeObject(lexicalGlobalObject->vm(), newObject);
    return newObject;
}

RuntimeObject* Instance::newRuntimeObject(JSGlobalObject* lexicalGlobalObject)
{
    JSLockHolder lock(lexicalGlobalObject);

    // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object.
    return RuntimeObject::create(lexicalGlobalObject->vm(), WebCore::deprecatedGetDOMStructure<RuntimeObject>(lexicalGlobalObject), this);
}

void Instance::willInvalidateRuntimeObject()
{
    m_runtimeObject.clear();
}

RootObject* Instance::rootObject() const
{
    return m_rootObject && m_rootObject->isValid() ? m_rootObject.get() : 0;
}

} // namespace Bindings

} // namespace JSC
