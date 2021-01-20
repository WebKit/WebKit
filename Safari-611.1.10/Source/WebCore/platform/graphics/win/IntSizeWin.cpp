/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "IntSize.h"

#include <d2d1.h>
#include <windows.h>

#include <wtf/MathExtras.h>

namespace WebCore {

IntSize::IntSize(const SIZE& s)
    : m_width(s.cx)
    , m_height(s.cy)
{
}

IntSize::operator SIZE() const
{
    SIZE s = {m_width, m_height};
    return s;
}

IntSize::IntSize(const D2D1_SIZE_U& s)
    : m_width(s.width)
    , m_height(s.height)
{
}

IntSize::IntSize(const D2D1_SIZE_F& s)
    : m_width(clampToInteger(s.width))
    , m_height(clampToInteger(s.height))
{
}

IntSize::operator D2D1_SIZE_U() const
{
    return D2D1::SizeU(m_width, m_height);
}

IntSize::operator D2D1_SIZE_F() const
{
    return D2D1::SizeF(m_width, m_height);
}

}
