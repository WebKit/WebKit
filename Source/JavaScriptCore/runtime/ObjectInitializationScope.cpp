/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ObjectInitializationScope.h"

#include "JSCInlines.h"
#include "JSObject.h"
#include "Operations.h"

namespace JSC {

#ifndef NDEBUG
ObjectInitializationScope::ObjectInitializationScope(VM& vm)
    : m_vm(vm)
    , m_disallowGC(false)
    , m_disallowVMReentry(false)
{
}

ObjectInitializationScope::~ObjectInitializationScope()
{
    if (!m_object)
        return;
    verifyPropertiesAreInitialized(m_object);
}

void ObjectInitializationScope::notifyAllocated(JSObject* object, bool wasCreatedUninitialized)
{
    if (wasCreatedUninitialized) {
        m_disallowGC.enable();
        m_disallowVMReentry.enable();
        m_object = object;
    } else
        verifyPropertiesAreInitialized(object);
}

void ObjectInitializationScope::verifyPropertiesAreInitialized(JSObject* object)
{
    Butterfly* butterfly = object->butterfly();
    IndexingType indexingType = object->structure(m_vm)->indexingType();
    unsigned vectorLength = butterfly->vectorLength();
    if (UNLIKELY(hasUndecided(indexingType))) {
        // Nothing to verify.
    } else if (LIKELY(!hasAnyArrayStorage(indexingType))) {
        auto data = butterfly->contiguous().data();
        for (unsigned i = 0; i < vectorLength; ++i) {
            if (isScribbledValue(data[i].get())) {
                dataLog("Found scribbled value at i = ", i, "\n");
                ASSERT_NOT_REACHED();
            }
        }
    } else {
        ArrayStorage* storage = butterfly->arrayStorage();
        for (unsigned i = 0; i < vectorLength; ++i) {
            if (isScribbledValue(storage->m_vector[i].get())) {
                dataLog("Found scribbled value at i = ", i, "\n");
                ASSERT_NOT_REACHED();
            }
        }
    }
}
#endif

} // namespace JSC
