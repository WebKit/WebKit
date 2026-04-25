/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/BoxSides.h>
#include <array>
#include <type_traits>
#include <wtf/text/TextStream.h>

namespace WebCore {

template<typename T> class RectCorners {
public:
    RectCorners() requires (std::is_default_constructible_v<T>)
        : m_corners { }
    {
    }

    RectCorners(const T& value)
        : m_corners { value, value, value, value }
    {
    }

    RectCorners(const RectCorners&) = default;
    RectCorners& operator=(const RectCorners&) = default;

    template<typename U>
    RectCorners(U&& topLeft, U&& topRight, U&& bottomLeft, U&& bottomRight)
        : m_corners({ { std::forward<U>(topLeft), std::forward<U>(topRight), std::forward<U>(bottomLeft), std::forward<U>(bottomRight) } })
    {
    }

    template<typename U>
    RectCorners(const RectCorners<U>& other)
        : RectCorners(other.topLeft(), other.topRight(), other.bottomLeft(), other.bottomRight())
    {
    }

    T& at(BoxCorner corner) { return m_corners[static_cast<size_t>(corner)]; }
    T& operator[](BoxCorner corner) { return m_corners[static_cast<size_t>(corner)]; }
    T& topLeft() { return at(BoxCorner::TopLeft); }
    T& topRight() { return at(BoxCorner::TopRight); }
    T& bottomLeft() { return at(BoxCorner::BottomLeft); }
    T& bottomRight() { return at(BoxCorner::BottomRight); }

    const T& at(BoxCorner corner) const { return m_corners[static_cast<size_t>(corner)]; }
    const T& operator[](BoxCorner corner) const { return m_corners[static_cast<size_t>(corner)]; }
    const T& topLeft() const { return at(BoxCorner::TopLeft); }
    const T& topRight() const { return at(BoxCorner::TopRight); }
    const T& bottomLeft() const { return at(BoxCorner::BottomLeft); }
    const T& bottomRight() const { return at(BoxCorner::BottomRight); }

    void setAt(BoxCorner corner, const T& v) { at(corner) = v; }
    void setTopLeft(const T& topLeft) { setAt(BoxCorner::TopLeft, topLeft); }
    void setTopRight(const T& topRight) { setAt(BoxCorner::TopRight, topRight); }
    void setBottomLeft(const T& bottomLeft) { setAt(BoxCorner::BottomLeft, bottomLeft); }
    void setBottomRight(const T& bottomRight) { setAt(BoxCorner::BottomRight, bottomRight); }

    template<typename F> bool anyOf(F&& functor) const
    {
        return std::ranges::any_of(m_corners, std::forward<F>(functor));
    }

    template<typename F> bool allOf(F&& functor) const
    {
        return std::ranges::all_of(m_corners, std::forward<F>(functor));
    }

    template<typename F> bool noneOf(F&& functor) const
    {
        return std::ranges::none_of(m_corners, std::forward<F>(functor));
    }

    bool isZero() const
    {
        return allOf([](auto& corner) { return !corner; });
    }

    bool operator==(const RectCorners<T>&) const = default;

private:
    std::array<T, 4> m_corners;
};

template<typename T>
TextStream& operator<<(TextStream& ts, const RectCorners<T>& corners)
{
    return ts << "[topLeft "_s << corners.topLeft() << " topRight "_s << corners.topRight() << " bottomLeft "_s << corners.bottomLeft() << " bottomRight "_s << corners.bottomRight() << ']';
}

} // namespace WebCore
