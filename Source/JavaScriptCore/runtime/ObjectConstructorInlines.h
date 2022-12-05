/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ObjectConstructor.h"

namespace JSC {

ALWAYS_INLINE bool objectAssignFast(VM& vm, JSObject* target, JSObject* source, Vector<RefPtr<UniquedStringImpl>, 8>& properties, MarkedArgumentBuffer& values)
{
    // |source| Structure does not have any getters. And target can perform fast put.
    // So enumerating properties and putting properties are non observable.

    // FIXME: It doesn't seem like we should have to do this in two phases, but
    // we're running into crashes where it appears that source is transitioning
    // under us, and even ends up in a state where it has a null butterfly. My
    // leading hypothesis here is that we fire some value replacement watchpoint
    // that ends up transitioning the structure underneath us.
    // https://bugs.webkit.org/show_bug.cgi?id=187837

    // FIXME: This fast path is very similar to ObjectConstructor' one. But extracting it to a function caused performance
    // regression in object-assign-replace. Since the code is small and fast path, we keep both.

    // Do not clear since Vector::clear shrinks the backing store.
    properties.resize(0);
    values.clear();
    bool canUseFastPath = source->fastForEachPropertyWithSideEffectFreeFunctor(vm, [&](const PropertyTableEntry& entry) -> bool {
        if (entry.attributes() & PropertyAttribute::DontEnum)
            return true;

        PropertyName propertyName(entry.key());
        if (propertyName.isPrivateName())
            return true;

        properties.append(entry.key());
        values.appendWithCrashOnOverflow(source->getDirect(entry.offset()));

        return true;
    });
    if (!canUseFastPath)
        return false;

    for (size_t i = 0; i < properties.size(); ++i) {
        // FIXME: We could put properties in a batching manner to accelerate Object.assign more.
        // https://bugs.webkit.org/show_bug.cgi?id=185358
        PutPropertySlot putPropertySlot(target, true);
        target->putOwnDataProperty(vm, properties[i].get(), values.at(i), putPropertySlot);
    }
    return true;
}


} // namespace JSC
