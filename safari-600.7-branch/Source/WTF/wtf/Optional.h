/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef Optional_h
#define Optional_h

#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

// WTF::Optional is a class based on std::optional, described here:
// http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2013/n3527.html
// If this ends up in a C++ standard, we should replace our implementation with it.

namespace WTF {

enum InPlaceTag { InPlace };
enum NulloptTag { Nullopt };

template<typename T>
class Optional {
public:
    Optional()
        : m_isEngaged(false)
    {
    }

    Optional(NulloptTag)
        : m_isEngaged(false)
    {
    }

    Optional(const T& value)
        : m_isEngaged(true)
        , m_value(value)
    {
    }

    Optional(const Optional& other)
        : m_isEngaged(other.m_isEngaged)
    {
        if (m_isEngaged)
            new (NotNull, std::addressof(m_value)) T(other.m_value);
    }

    Optional(Optional&& other)
        : m_isEngaged(other.m_isEngaged)
    {
        if (m_isEngaged)
            new (NotNull, std::addressof(m_value)) T(WTF::move(other.m_value));
    }

    Optional(T&& value)
        : m_isEngaged(true)
        , m_value(WTF::move(value))
    {
    }

    template<typename... Args>
    Optional(InPlaceTag, Args&&... args)
        : m_isEngaged(true)
        , m_value(std::forward<Args>(args)...)
    {
    }

    ~Optional()
    {
        if (m_isEngaged)
            m_value.~T();
    }

    Optional& operator=(NulloptTag)
    {
        if (m_isEngaged) {
            m_value.~T();
            m_isEngaged = false;
        }
        return *this;
    }

    Optional& operator=(const Optional& other)
    {
        if (m_isEngaged == other.m_isEngaged) {
            if (m_isEngaged)
                m_value = other.m_value;
            return *this;
        }

        if (m_isEngaged)
            m_value.~T();
        else
            new (NotNull, std::addressof(m_value)) T(other.m_value);
        m_isEngaged = other.m_isEngaged;

        return *this;
    }

    Optional& operator=(Optional&& other)
    {
        if (m_isEngaged == other.m_isEngaged) {
            if (m_isEngaged)
                m_value = WTF::move(other.m_value);
            return *this;
        }

        if (m_isEngaged)
            m_value.~T();
        else
            new (NotNull, std::addressof(m_value)) T(WTF::move(other.m_value));
        m_isEngaged = other.m_isEngaged;

        return *this;
    }

    template<typename U>
    Optional& operator=(U&& u)
    {
        if (m_isEngaged) {
            m_value = std::forward<U>(u);
            return *this;
        }

        new (NotNull, std::addressof(m_value)) T(std::forward<U>(u));
        m_isEngaged = true;
        return *this;
    }

    explicit operator bool() const { return m_isEngaged; }

    T& value()
    {
        ASSERT(m_isEngaged);
        return m_value;
    }

    const T& value() const
    {
        ASSERT(m_isEngaged);
        return m_value;
    }

    template<typename U>
    T valueOr(U&& value) const
    {
        if (m_isEngaged)
            return m_value;

        return std::forward<U>(value);
    }

private:
    bool m_isEngaged;
    union {
        T m_value;
    };
};

} // namespace WTF

using WTF::InPlace;
using WTF::Nullopt;
using WTF::Optional;

#endif // Optional_h
