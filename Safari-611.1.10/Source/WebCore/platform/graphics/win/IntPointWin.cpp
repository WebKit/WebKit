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
#include "IntPoint.h"

#include <d2d1.h>
#include <windows.h>

#include <wtf/MathExtras.h>

namespace WebCore {

IntPoint::IntPoint(const POINT& p)
    : m_x(p.x)
    , m_y(p.y)
{
}

IntPoint::operator POINT() const
{
    POINT p = {m_x, m_y};
    return p;
}

IntPoint::IntPoint(const POINTS& p)
    : m_x(p.x)
    , m_y(p.y)
{
}

IntPoint::operator POINTS() const
{
    POINTS p = { static_cast<SHORT>(m_x), static_cast<SHORT>(m_y) };
    return p;
}

IntPoint::IntPoint(const D2D1_POINT_2F& p)
    : m_x(clampToInteger(p.x))
    , m_y(clampToInteger(p.y))
{
}

IntPoint::IntPoint(const D2D1_POINT_2U& p)
    : m_x(p.x)
    , m_y(p.y)
{
}

IntPoint::operator D2D1_POINT_2F() const
{
    return D2D1::Point2F(m_x, m_y);
}

IntPoint::operator D2D1_POINT_2U() const
{
    return D2D1::Point2U(m_x, m_y);
}

}
