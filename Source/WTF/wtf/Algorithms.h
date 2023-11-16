/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include <cstring>
#include <span>
#include <type_traits>
#include <wtf/Assertions.h>

namespace WTF {

template<typename ContainerType, typename ForEachFunction>
void forEach(ContainerType&& container, ForEachFunction forEachFunction)
{
    for (auto& value : container)
        forEachFunction(value);
}

template<typename ContainerType, typename AnyOfFunction>
bool anyOf(ContainerType&& container, AnyOfFunction anyOfFunction)
{
    for (auto& value : container) {
        if (anyOfFunction(value))
            return true;
    }
    return false;
}

template<typename ContainerType, typename AllOfFunction>
bool allOf(ContainerType&& container, AllOfFunction allOfFunction)
{
    for (auto& value : container) {
        if (!allOfFunction(value))
            return false;
    }
    return true;
}

template<typename T, typename U>
std::span<T> spanReinterpretCast(std::span<U> span)
{
    RELEASE_ASSERT(!(span.size_bytes() % sizeof(T)));
    static_assert(std::is_const_v<T> || (!std::is_const_v<T> && !std::is_const_v<U>), "spanCast will not remove constness from source");
    return std::span<T> { reinterpret_cast<T*>(const_cast<std::remove_const_t<U>*>(span.data())), span.size_bytes() / sizeof(T) };
}

template<typename T, typename U>
void memcpySpan(std::span<T> destination, std::span<U> source)
{
    RELEASE_ASSERT(destination.size() == source.size());
    static_assert(sizeof(T) == sizeof(U));
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_copyable_v<U>);
    memcpy(destination.data(), source.data(), destination.size() * sizeof(T));
}

template<typename T>
void memsetSpan(std::span<T> destination, uint8_t byte)
{
    static_assert(std::is_trivially_copyable_v<T>);
    memset(destination.data(), byte, destination.size() * sizeof(T));
}

} // namespace WTF

using WTF::spanReinterpretCast;
using WTF::memcpySpan;
using WTF::memsetSpan;
