/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef LengthPoint_h
#define LengthPoint_h

#include "Length.h"

namespace WebCore {

struct LengthPoint {
public:
    LengthPoint()
    {
    }
    
    LengthPoint(Length x, Length y)
        : m_x(WTFMove(x))
        , m_y(WTFMove(y))
    {
    }

    bool operator==(const LengthPoint& o) const
    {
        return m_x == o.m_x && m_y == o.m_y;
    }

    bool operator!=(const LengthPoint& o) const
    {
        return !(*this == o);
    }

    void setX(Length x) { m_x = WTFMove(x); }
    const Length& x() const { return m_x; }

    void setY(Length y) { m_y = WTFMove(y); }
    const Length& y() const { return m_y; }

private:
    // FIXME: it would be nice to pack the two Lengths together better somehow (to avoid padding between them).
    Length m_x;
    Length m_y;
};

inline LengthPoint blend(const LengthPoint& from, const LengthPoint& to, double progress)
{
    return LengthPoint(blend(from.x(), to.x(), progress), blend(from.y(), to.y(), progress));
}

WTF::TextStream& operator<<(WTF::TextStream&, const LengthPoint&);

} // namespace WebCore

#endif // LengthPoint_h
