/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/HasOwnPropertyCache.h>
#include <JavaScriptCore/StructureArrayStorageInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

ALWAYS_INLINE void HasOwnPropertyCache::tryAdd(PropertySlot& slot, JSObject* object, PropertyName propName, bool result)
{
    if (!slot.isCacheable() && !slot.isUnset())
        return;

    if (object->type() == GlobalProxyType)
        return;

    Structure* structure = object->structure();
    if (parseIndex(propName)) {
        // A plain object without indexed storage can only gain an indexed property through a structure
        // transition, so its absence is a property of the structure.
        if (result || object->type() != FinalObjectType || hasIndexedProperties(object->indexingType()) || structure->mayInterceptIndexedAccesses())
            return;
    }

    if (!structure->typeInfo().prohibitsPropertyCaching()
        && structure->propertyAccessesAreCacheable()
        && (!slot.isUnset() || structure->propertyAccessesAreCacheableForAbsence())) {
        if (structure->isDictionary()) {
            // FIXME: We should be able to flatten a dictionary object again.
            // https://bugs.webkit.org/show_bug.cgi?id=163092
            return;
        }

        ASSERT(!result == slot.isUnset());

        RefPtr impl = propName.uid();
        StructureID id = structure->id();
        uint32_t index = HasOwnPropertyCache::hash(id, impl.get()) & mask;
        std::bit_cast<Entry*>(this)[index] = Entry { WTF::move(impl), id, result };
    }
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
