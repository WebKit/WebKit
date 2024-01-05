/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "DFGDesiredObjectProperties.h"

#if ENABLE(DFG_JIT)

#include "JSObjectInlines.h"

namespace JSC { namespace DFG {

bool DesiredObjectProperties::addLazily(JSObject* object, PropertyOffset offset, JSValue value, Structure* structure)
{
    auto result = m_properties.add(std::tuple { object, offset }, std::tuple { value, structure });
    if (result.isNewEntry)
        return true;
    auto [expectedValue, expectedStructure] = result.iterator->value;
    return expectedValue == value && expectedStructure == structure;
}

// Ultimately, we can use the code when
// 1. the main thread's object is holding an expected structure.
// 2. the main thread's object's property is expected one.
bool DesiredObjectProperties::areStillValidOnMainThread(VM&)
{
    for (auto& [key, values] : m_properties) {
        auto [object, offset] = key;
        auto [expectedValue, expectedStructure] = values;

        Structure* structure = object->structure();

        if (UNLIKELY(structure != expectedStructure))
            return false;

        if (UNLIKELY(!structure->isValidOffset(offset)))
            return false;

        JSValue value = object->getDirect(offset);
        if (UNLIKELY(value != expectedValue))
            return false;
    }
    return true;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

