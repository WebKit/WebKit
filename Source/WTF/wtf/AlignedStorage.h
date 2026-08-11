/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <cstddef>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename T, size_t alignment = std::alignment_of_v<T>>
class AlignedStorage {
public:
    AlignedStorage() = default;
    AlignedStorage(AlignedStorage&&) = delete;
    AlignedStorage(const AlignedStorage&) = delete;
    AlignedStorage& operator=(AlignedStorage&&) = delete;
    AlignedStorage& operator=(const AlignedStorage&) = delete;

    T* get() LIFETIME_BOUND { SUPPRESS_MEMORY_UNSAFE_CAST return reinterpret_cast_ptr<T*>(&m_storage); }
    const T* get() const LIFETIME_BOUND { SUPPRESS_MEMORY_UNSAFE_CAST return reinterpret_cast_ptr<const T*>(&m_storage); }

    T& operator*() { return *get(); }
    T* operator->() { return get(); }
    const T& operator*() const { return *get(); }
    const T* operator->() const { return get(); }

private:
    struct alignas(alignment) Storage {
        std::byte data[sizeof(T)];
    } m_storage;
};

} // namespace WTF

using WTF::AlignedStorage;
