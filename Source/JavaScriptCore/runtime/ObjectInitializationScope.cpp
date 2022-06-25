/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "HeapInlines.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSObject.h"
#include "Scribble.h"

namespace JSC {

#if ASSERT_ENABLED

ObjectInitializationScope::ObjectInitializationScope(VM& vm)
    : m_vm(vm)
{
}

ObjectInitializationScope::~ObjectInitializationScope()
{
    m_vm.mutatorFence();
    if (!m_object)
        return;
    verifyPropertiesAreInitialized(m_object);
}

void ObjectInitializationScope::notifyAllocated(JSObject* object)
{
    ASSERT(!m_disallowGC);
    ASSERT(!m_disallowVMEntry);
    m_disallowGC.emplace();
    m_disallowVMEntry.emplace(m_vm);
    m_object = object;
}

void ObjectInitializationScope::notifyInitialized(JSObject* object)
{
    if (m_object) {
        m_disallowGC.reset();
        m_disallowVMEntry.reset();
        m_object = nullptr;
    }
    verifyPropertiesAreInitialized(object);
}

void ObjectInitializationScope::verifyPropertiesAreInitialized(JSObject* object)
{
    Butterfly* butterfly = object->butterfly();
    Structure* structure = object->structure();
    IndexingType indexingType = structure->indexingType();
    unsigned vectorLength = butterfly->vectorLength();
    if (UNLIKELY(hasUndecided(indexingType)) || !hasIndexedProperties(indexingType)) {
        // Nothing to verify.
    } else if (LIKELY(!hasAnyArrayStorage(indexingType))) {
        auto data = butterfly->contiguous().data();
        for (unsigned i = 0; i < vectorLength; ++i) {
            if (isScribbledValue(data[i].get())) {
                dataLogLn("Found scribbled value at i = ", i);
                ASSERT_NOT_REACHED();
            }
        }
    } else {
        ArrayStorage* storage = butterfly->arrayStorage();
        for (unsigned i = 0; i < vectorLength; ++i) {
            if (isScribbledValue(storage->m_vector[i].get())) {
                dataLogLn("Found scribbled value at i = ", i);
                ASSERT_NOT_REACHED();
            }
        }
    }

    auto isSafeEmptyValueForGCScanning = [] (JSValue value) {
#if USE(JSVALUE64)
        return !value;
#else
        return !value || !JSValue::encode(value);
#endif
    };

    for (int64_t i = 0; i < static_cast<int64_t>(structure->outOfLineCapacity()); i++) {
        // We rely on properties past the last offset be zero for concurrent GC.
        if (i + firstOutOfLineOffset > structure->maxOffset())
            ASSERT(isSafeEmptyValueForGCScanning(butterfly->propertyStorage()[-i - 1].get()));
        else if (isScribbledValue(butterfly->propertyStorage()[-i - 1].get())) {
            dataLogLn("Found scribbled property at i = ", -i - 1);
            ASSERT_NOT_REACHED();
        }
    }
}

#endif // ASSERT_ENABLED

} // namespace JSC
