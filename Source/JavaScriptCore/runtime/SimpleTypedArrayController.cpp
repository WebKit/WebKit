/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "SimpleTypedArrayController.h"

#include "ArrayBuffer.h"
#include "JSArrayBuffer.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"

namespace JSC {

SimpleTypedArrayController::SimpleTypedArrayController(bool allowAtomicsWait)
    : m_allowAtomicsWait(allowAtomicsWait)
{
}

SimpleTypedArrayController::~SimpleTypedArrayController() { }

JSArrayBuffer* SimpleTypedArrayController::toJS(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, ArrayBuffer* native)
{
    UNUSED_PARAM(lexicalGlobalObject);
    if (JSArrayBuffer* buffer = native->m_wrapper.get())
        return buffer;

    // The JSArrayBuffer::create function will register the wrapper in finishCreation.
    JSArrayBuffer* result = JSArrayBuffer::create(globalObject->vm(), globalObject->arrayBufferStructure(native->sharingMode()), native);
    return result;
}

void SimpleTypedArrayController::registerWrapper(JSGlobalObject*, ArrayBuffer* native, JSArrayBuffer* wrapper)
{
    ASSERT(!native->m_wrapper);
    native->m_wrapper = Weak<JSArrayBuffer>(wrapper, &m_owner);
}

bool SimpleTypedArrayController::isAtomicsWaitAllowedOnCurrentThread()
{
    return m_allowAtomicsWait;
}

bool SimpleTypedArrayController::JSArrayBufferOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, JSC::AbstractSlotVisitor& visitor, const char** reason)
{
    if (UNLIKELY(reason))
        *reason = "JSArrayBuffer is opaque root";
    auto& wrapper = *JSC::jsCast<JSC::JSArrayBuffer*>(handle.slot()->asCell());
    return visitor.containsOpaqueRoot(wrapper.impl());
}

void SimpleTypedArrayController::JSArrayBufferOwner::finalize(JSC::Handle<JSC::Unknown>, void*) { }

} // namespace JSC

