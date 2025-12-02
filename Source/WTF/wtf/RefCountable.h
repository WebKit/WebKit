/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include <wtf/Ref.h>
#include <wtf/RetainReleaseSwift.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeTraits.h>

namespace WTF {

// RefCountable<T> makes any type reference-counted, including move-only types
// In Swift, you can use cell.pointee.method()
// FIXME: becomes largely unnecessary once rdar://162361370 is fixed in Swift/C++
// interop.
template<typename T>
class RefCountable : public ThreadSafeRefCounted<RefCountable<T>> {
public:
    template<typename... Arguments>
    static Ref<RefCountable> create(Arguments&&... arguments)
    {
        static_assert(!HasRefPtrMemberFunctions<T>::value, "T should not be RefCounted");
        return adoptRef(*new RefCountable(std::forward<Arguments>(arguments)...));
    }

    RefCountable(T&& value)
        : m_value(WTFMove(value))
    {
    }

#ifdef __swift__
    // FIXME: rdar://165684636 means we have to define these at this level of the
    // type hierarchy.
    void ref() const
    {
        ThreadSafeRefCounted<RefCountable<T>>::ref();
    }

    void deref() const
    {
        ThreadSafeRefCounted<RefCountable<T>>::deref();
    }
#endif

    T& operator*()
    {
        return m_value;
    }

    const T& operator*() const
    {
        return m_value;
    }

private:
    template<typename... Arguments>
    RefCountable(Arguments&&... arguments)
        : m_value(std::forward<Arguments>(arguments)...)
    {
        static_assert(!HasRefPtrMemberFunctions<T>::value, "T should not be RefCounted");
    }

    T m_value;
} SWIFT_SHARED_REFERENCE(.ref, .deref);

} // namespace WTF
