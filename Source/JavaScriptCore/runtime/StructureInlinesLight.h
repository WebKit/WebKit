/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/PropertyTable.h>
#include <JavaScriptCore/Structure.h>
#include <wtf/Threading.h>

namespace JSC {

inline JSObject* Structure::storedPrototypeObject() const
{
    ASSERT(hasMonoProto());
    JSValue value = m_prototype.get();
    if (value.isNull())
        return nullptr;
    return asObject(value);
}

inline Structure* Structure::storedPrototypeStructure() const
{
    ASSERT(hasMonoProto());
    JSObject* object = storedPrototypeObject();
    if (!object)
        return nullptr;
    return object->structure();
}

ALWAYS_INLINE JSValue Structure::storedPrototype(const JSObject* object) const
{
    ASSERT(isCompilationThread() || Thread::mayBeGCThread() || object->structure() == this);
    if (hasMonoProto())
        return storedPrototype();
    return object->getDirect(knownPolyProtoOffset);
}

ALWAYS_INLINE JSObject* Structure::storedPrototypeObject(const JSObject* object) const
{
    ASSERT(isCompilationThread() || Thread::mayBeGCThread() || object->structure() == this);
    if (hasMonoProto())
        return storedPrototypeObject();
    JSValue proto = object->getDirect(knownPolyProtoOffset);
    if (proto.isNull())
        return nullptr;
    return asObject(proto);
}

ALWAYS_INLINE Structure* Structure::storedPrototypeStructure(const JSObject* object) const
{
    if (JSObject* proto = storedPrototypeObject(object))
        return proto->structure();
    return nullptr;
}

ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, PropertyName propertyName)
{
    unsigned attributes;
    return get(vm, propertyName, attributes);
}

ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, PropertyName propertyName, unsigned& attributes)
{
    ASSERT(!isCompilationThread());
    ASSERT(structure()->classInfoForCells() == info());

    if (m_seenProperties.ruleOut(CompactPtr<UniquedStringImpl>::encode(propertyName.uid())))
        return invalidOffset;

    PropertyTable* propertyTable = ensurePropertyTableIfNotEmpty(vm);
    if (!propertyTable)
        return invalidOffset;

    auto [offset, entryAttributes] = propertyTable->get(propertyName.uid());
    if (offset != invalidOffset)
        attributes = entryAttributes;
    return offset;
}

inline bool Structure::hasIndexingHeader(const JSCell* cell) const
{
    if (hasIndexedProperties(indexingType()))
        return true;

    if (!isTypedView(m_blob.type()))
        return false;

    TypedArrayMode mode = uncheckedDowncast<JSArrayBufferView>(cell)->mode();
    return isWastefulTypedArray(mode);
}

inline void Structure::addTransitionWatchpoint(Watchpoint* watchpoint) const
{
    ASSERT(transitionWatchpointSetIsStillValid());
    m_transitionWatchpointSet.add(watchpoint);
}

} // namespace JSC
