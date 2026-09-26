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
#include "JSCJSValueInlines.h"
#include "JSONAtomStringCache.h"
#include "JSString.h"
#include "SmallStrings.h"
#include "VM.h"
#include <wtf/UnalignedAccess.h>

namespace JSC {

template<typename CharacterType>
ALWAYS_INLINE Ref<AtomStringImpl> JSONAtomStringCache::makeIdentifier(std::span<const CharacterType> characters)
{
    if (characters.empty())
        return *emptyAtom().impl();

    auto firstCharacter = characters.front();
    if (characters.size() == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return vm().smallStrings.singleCharacterStringRep(firstCharacter);
    } else if (characters.size() > maxStringLengthForCache) [[unlikely]]
        return AtomStringImpl::add(characters).releaseNonNull();

    auto lastCharacter = characters.back();
    unsigned index = cacheIndex(firstCharacter, lastCharacter, characters.size());
    auto& slot = m_cache[index];
    if (slot.m_length != characters.size() || !equal(slot.m_buffer, characters)) [[unlikely]] {
        auto result = AtomStringImpl::add(characters);
        slot.m_impl = result;
        slot.m_length = characters.size();
        WTF::copyElements(std::span<char16_t> { slot.m_buffer }, characters);
        m_jsStrings[index] = nullptr;
        return result.releaseNonNull();
    }

    return *slot.m_impl;
}

template<typename CharacterType>
ALWAYS_INLINE AtomStringImpl* JSONAtomStringCache::existingIdentifier(std::span<const CharacterType> characters)
{
    if (characters.empty())
        return emptyAtom().impl();

    auto firstCharacter = characters.front();
    if (characters.size() == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return vm().smallStrings.existingSingleCharacterStringRep(firstCharacter);
    } else if (characters.size() > maxStringLengthForCache) [[unlikely]]
        return nullptr;

    auto lastCharacter = characters.back();
    auto& slot = cacheSlot(firstCharacter, lastCharacter, characters.size());
    if (slot.m_length != characters.size() || !equal(slot.m_buffer, characters)) [[unlikely]]
        return nullptr;

    return slot.m_impl.get();
}

template<typename CharacterType>
ALWAYS_INLINE JSString* JSONAtomStringCache::makeJSString(std::span<const CharacterType> characters)
{
    VM& vm = this->vm();
    if (characters.empty())
        return jsEmptyString(vm);

    constexpr unsigned maxAtomizeStringLength = 16;
    auto firstCharacter = characters.front();
    if (characters.size() == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return jsSingleCharacterString(vm, firstCharacter);
    } else if (characters.size() > maxAtomizeStringLength)
        return makeLongJSString(characters);

    auto lastCharacter = characters.back();
    unsigned index = cacheIndex(firstCharacter, lastCharacter, characters.size());
    auto& slot = m_cache[index];
    if (slot.m_length == characters.size() && equal(slot.m_buffer, characters)) [[likely]] {
        if (JSString* cached = m_jsStrings[index])
            return cached;
        JSString* result = jsString(vm, String { slot.m_impl.get() });
        m_jsStrings[index] = result;
        return result;
    }

    auto impl = AtomStringImpl::add(characters);
    slot.m_impl = impl;
    slot.m_length = characters.size();
    WTF::copyElements(std::span<char16_t> { slot.m_buffer }, characters);
    JSString* result = jsString(vm, String { WTF::move(impl) });
    m_jsStrings[index] = result;
    return result;
}

// Long values such as URLs and type names repeat heavily in real JSON, so they are shared without
// atomizing. Called only for lengths above maxAtomizeStringLength, so both 8-byte loads are in bounds.
template<typename CharacterType>
inline JSString* JSONAtomStringCache::makeLongJSString(std::span<const CharacterType> characters)
{
    VM& vm = this->vm();
    auto create = [&] {
        if constexpr (std::is_same_v<CharacterType, char16_t>)
            return jsNontrivialString(vm, String(StringImpl::create8BitIfPossible(characters)));
        else
            return jsNontrivialString(vm, String(characters));
    };

    if (characters.size() > maxLongStringLength)
        return create();

    auto bytes = asBytes(characters);
    uint64_t head = WTF::unalignedLoad<uint64_t>(bytes.data());
    uint64_t tail = WTF::unalignedLoad<uint64_t>(bytes.last(sizeof(uint64_t)).data());
    uint64_t hash = (head * 0x9E3779B97F4A7C15ULL) ^ (tail * 0xC2B2AE3D27D4EB4FULL) ^ characters.size();
    hash ^= hash >> 32;
    unsigned index = static_cast<unsigned>(hash * 0x9E3779B97F4A7C15ULL >> (64 - longStringCapacityLog2));

    JSString*& slot = m_longJSStrings[index];
    if (JSString* cached = slot) {
        SUPPRESS_UNCOUNTED_LOCAL StringImpl* impl = cached->getValueImpl();
        if (impl->length() == characters.size()) {
            if (impl->is8Bit()) {
                if (equal(impl->span8().data(), characters))
                    return cached;
            } else if (equal(impl->span16().data(), characters))
                return cached;
        }
    }

    JSString* result = create();
    slot = result;
    return result;
}

ALWAYS_INLINE VM& JSONAtomStringCache::vm() const
{
    return *std::bit_cast<VM*>(std::bit_cast<uintptr_t>(this) - OBJECT_OFFSETOF(VM, jsonAtomStringCache));
}

} // namespace JSC
