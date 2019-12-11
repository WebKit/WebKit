/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include <type_traits>
#include <utility>

namespace rtc {

template<typename T> class NeverDestroyed {
public:
    template<typename... Args> NeverDestroyed(Args&&... args)
    {
        new (storagePointer()) T(std::forward<Args>(args)...);
    }

    NeverDestroyed(NeverDestroyed&& other)
    {
        new (storagePointer()) T(std::move(*other.storagePointer()));
    }

    operator T&() { return *storagePointer(); }
    T& get() { return *storagePointer(); }

    operator const T&() const { return *storagePointer(); }
    const T& get() const { return *storagePointer(); }

private:
    NeverDestroyed(const NeverDestroyed&) = delete;
    NeverDestroyed& operator=(const NeverDestroyed&) = delete;

    using PointerType = typename std::remove_const<T>::type*;

    PointerType storagePointer() const { return const_cast<PointerType>(reinterpret_cast<const T*>(&m_storage)); }

    // FIXME: Investigate whether we should allocate a hunk of virtual memory
    // and hand out chunks of it to NeverDestroyed instead, to reduce fragmentation.
    typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type m_storage;
};

template<typename T> inline NeverDestroyed<T> makeNeverDestroyed(T&& argument)
{
    return NeverDestroyed<T>(std::move(argument));
}

} // namespace rtc
