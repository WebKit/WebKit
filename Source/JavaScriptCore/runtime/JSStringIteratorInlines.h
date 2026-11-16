/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "JSString.h"
#include "JSStringIterator.h"
#include "StructureCreateInlines.h"

namespace JSC {

inline Structure* JSStringIterator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSStringIteratorType, StructureFlags), info());
}

ALWAYS_INLINE std::pair<JSString*, int32_t> JSStringIterator::advance(JSGlobalObject* globalObject, VM& vm, JSString* string, int32_t position)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    constexpr std::pair<JSString*, int32_t> done { nullptr, doneIndex };

    unsigned length = string->length();
    // Unsigned compare also catches position == doneIndex (-1).
    if (static_cast<unsigned>(position) >= length)
        return done;

    auto view = string->view(globalObject);
    RETURN_IF_EXCEPTION(scope, done);

    char16_t first = view[position];
    int32_t valueLength = 1;
    if (U16_IS_LEAD(first) && static_cast<unsigned>(position) + 1 < length) {
        char16_t second = view[position + 1];
        if (U16_IS_TRAIL(second))
            valueLength = 2;
    }

    JSString* value;
    if (valueLength == 1)
        value = jsSingleCharacterString(vm, first);
    else {
        value = jsSubstring(globalObject, vm, string, position, valueLength);
        RETURN_IF_EXCEPTION(scope, done);
    }

    return { value, position + valueLength };
}

ALWAYS_INLINE JSString* JSStringIterator::nextWithAdvance(JSGlobalObject* globalObject, VM& vm)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& indexSlot = internalField(Field::Index);
    int32_t position = indexSlot.get().asInt32();

    auto [value, nextPosition] = advance(globalObject, vm, asString(iteratedString()), position);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // No need for a barrier here because we know this is a primitive.
    indexSlot.setWithoutWriteBarrier(jsNumber(nextPosition));
    return value;
}

} // namespace JSC
