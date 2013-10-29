/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef Nix_Color_h
#define Nix_Color_h

#include <algorithm>
#include <cstddef>

namespace Nix {

class Color {
public:
    typedef u_int32_t ARGB32; // ARGB quadruplet

    static const ARGB32 black = 0xFF000000;
    static const ARGB32 white = 0xFFFFFFFF;
    static const ARGB32 red = 0xFFFF0000;
    static const ARGB32 green = 0xFF00FF00;
    static const ARGB32 blue = 0xFF0000FF;
    static const ARGB32 darkGray = 0xFF808080;
    static const ARGB32 gray = 0xFFA0A0A0;
    static const ARGB32 lightGray = 0xFFC0C0C0;
    static const ARGB32 transparent = 0x00000000;

    Color(ARGB32 rgba)
        : m_argb(rgba)
    {
    }

    Color(int r, int g, int b, int a = 255)
        : m_argb(boundValue(a) << 24 | boundValue(r) << 16 | boundValue(g) << 8 | boundValue(b))
    {
    }

    operator ARGB32() const
    {
        return m_argb;
    }

    unsigned char r() const
    {
        return (m_argb & 0x00FF0000) >> 16;
    }

    unsigned char g() const
    {
        return (m_argb & 0x0000FF00) >> 8;
    }

    unsigned char b() const
    {
        return (m_argb & 0x000000FF);
    }

    unsigned char a() const
    {
        return (m_argb & 0xFF000000) >> 24;
    }

    ARGB32 argb32() const
    {
        return m_argb;
    }

    bool operator==(const Color& other)
    {
        return m_argb == other.m_argb;
    }

    bool operator!=(const Color& other)
    {
        return m_argb != other.m_argb;
    }

private:
    ARGB32 m_argb;
    static inline ARGB32 boundValue(int value)
    {
        return std::max(0, std::min(value, 255));
    }
};

} // namespace Nix

#endif // Nix_Color_h
