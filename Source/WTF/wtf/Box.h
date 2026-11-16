/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <wtf/RefCountable.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

// Box<T> is a reference-counted pointer to T that allocates T using FastMalloc and prepends a reference
// count to it. It's almost just RefPtr<RefCountable<T>>, but has convenience operators.
template<typename T>
class Box {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Box);
public:
    Box() = default;
    Box(Box&&) = default;
    Box(const Box&) = default;

    Box(std::nullptr_t)
    {
    }

    Box& operator=(Box&&) = default;
    Box& operator=(const Box&) = default;

    template<typename... Arguments>
    static Box create(Arguments&&... arguments)
    {
        Box result;
        result.m_data = RefCountable<T>::create(std::forward<Arguments>(arguments)...);
        return result;
    }

    bool isValid() const { return static_cast<bool>(m_data); }

    T* get() const LIFETIME_BOUND
    {
        if (!isValid())
            return nullptr;
        return &**m_data;
    }

    T& operator*() const LIFETIME_BOUND { RELEASE_ASSERT(isValid()); return **m_data; }
    T* operator->() const LIFETIME_BOUND { RELEASE_ASSERT(isValid()); return &**m_data; }

    explicit operator bool() const { return isValid(); }

private:
    RefPtr<RefCountable<T>> m_data;
};

} // namespace WTF

using WTF::Box;
