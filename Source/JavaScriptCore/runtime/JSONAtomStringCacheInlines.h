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

#include "Identifier.h"
#include "JSONAtomStringCache.h"
#include "SmallStrings.h"
#include "VM.h"

namespace JSC {

template<typename CharacterType>
ALWAYS_INLINE Ref<AtomStringImpl> JSONAtomStringCache::make(Type type, const CharacterType* characters, unsigned length)
{
    if (!length)
        return *static_cast<AtomStringImpl*>(StringImpl::empty());

    auto firstCharacter = characters[0];
    if (length == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return vm().smallStrings.singleCharacterStringRep(firstCharacter);
    } else if (length > maxStringLengthForCache)
        return AtomStringImpl::add(characters, length).releaseNonNull();

    auto lastCharacter = characters[length - 1];
    auto& slot = cacheSlot(type, firstCharacter, lastCharacter, length);
    if (!equal(slot.get(), characters, length)) {
        auto result = AtomStringImpl::add(characters, length);
        slot = result;
        return result.releaseNonNull();
    }

    return *slot;
}

ALWAYS_INLINE VM& JSONAtomStringCache::vm() const
{
    return *bitwise_cast<VM*>(bitwise_cast<uintptr_t>(this) - OBJECT_OFFSETOF(VM, jsonAtomStringCache));
}

} // namespace JSC
