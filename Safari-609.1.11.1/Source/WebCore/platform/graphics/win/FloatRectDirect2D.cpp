/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
#include "FloatRect.h"

#if PLATFORM(WIN)

#include "FloatPoint.h"
#include <d2d1.h>

namespace WebCore {

static bool isInfiniteRect(const D2D1_RECT_F& rect)
{
    if (!std::isinf(rect.top) && rect.top != -std::numeric_limits<float>::max())
        return false;

    if (!std::isinf(rect.left) && rect.left != -std::numeric_limits<float>::max())
        return false;

    if (!std::isinf(rect.bottom) && rect.bottom != std::numeric_limits<float>::max())
        return false;

    if (!std::isinf(rect.right) && rect.right != std::numeric_limits<float>::max())
        return false;

    return true;
}

FloatRect::FloatRect(const RECT& rect)
    : m_location(rect.left, rect.top)
    , m_size(rect.right - rect.left, rect.bottom - rect.top)
{
}

FloatRect::FloatRect(const D2D1_RECT_F& r)
{
    // Infinite Rect case:
    if (isInfiniteRect(r)) {
        *this = infiniteRect();
        return;
    }

    m_location = FloatPoint(r.left, r.top);
    m_size = FloatSize(r.right - r.left, r.bottom - r.top);
}

FloatRect::operator D2D1_RECT_F() const
{
    return D2D1::RectF(x(), y(), maxX(), maxY());
}

}

#endif // PLATFORM(WIN)
