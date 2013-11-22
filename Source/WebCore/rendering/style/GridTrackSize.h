/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef GridTrackSize_h
#define GridTrackSize_h

#include "GridLength.h"

namespace WebCore {

enum GridTrackSizeType {
    LengthTrackSizing,
    MinMaxTrackSizing
};

class GridTrackSize {
public:
    GridTrackSize(LengthType type = Undefined)
        : m_type(LengthTrackSizing)
        , m_minTrackBreadth(type)
        , m_maxTrackBreadth(type)
    {
    }

    const GridLength& length() const
    {
        ASSERT(m_type == LengthTrackSizing);
        ASSERT(m_minTrackBreadth == m_maxTrackBreadth);
        const GridLength& minTrackBreadth = m_minTrackBreadth;
        ASSERT(!minTrackBreadth.isLength() || !minTrackBreadth.length().isUndefined());
        return minTrackBreadth;
    }

    void setLength(const GridLength& length)
    {
        m_type = LengthTrackSizing;
        m_minTrackBreadth = length;
        m_maxTrackBreadth = length;
    }

    const GridLength& minTrackBreadth() const
    {
        ASSERT(!m_minTrackBreadth.isLength() || !m_minTrackBreadth.length().isUndefined());
        if (m_minTrackBreadth.isLength() && m_minTrackBreadth.length().isAuto()) {
            DEFINE_STATIC_LOCAL(GridLength, minContent, (MinContent));
            return minContent;
        }
        return m_minTrackBreadth;
    }

    const GridLength& maxTrackBreadth() const
    {
        ASSERT(!m_maxTrackBreadth.isLength() || !m_maxTrackBreadth.length().isUndefined());
        if (m_maxTrackBreadth.isLength() && m_maxTrackBreadth.length().isAuto()) {
            DEFINE_STATIC_LOCAL(GridLength, maxContent, (MaxContent));
            return maxContent;
        }
        return m_maxTrackBreadth;
    }

    void setMinMax(const GridLength& minTrackBreadth, const GridLength& maxTrackBreadth)
    {
        m_type = MinMaxTrackSizing;
        m_minTrackBreadth = minTrackBreadth;
        m_maxTrackBreadth = maxTrackBreadth;
    }

    GridTrackSizeType type() const { return m_type; }

    bool isContentSized() const { return m_minTrackBreadth.isContentSized() || m_maxTrackBreadth.isContentSized(); }

    bool operator==(const GridTrackSize& other) const
    {
        return m_type == other.m_type && m_minTrackBreadth == other.m_minTrackBreadth && m_maxTrackBreadth == other.m_maxTrackBreadth;
    }

    bool hasMinOrMaxContentMinTrackBreadth() const { return minTrackBreadth().isLength() && (minTrackBreadth().length().isMinContent() || minTrackBreadth().length().isMaxContent()); }
    bool hasMaxContentMinTrackBreadth() const { return minTrackBreadth().isLength() && minTrackBreadth().length().isMaxContent(); }
    bool hasMinOrMaxContentMaxTrackBreadth() const { return maxTrackBreadth().isLength() && (maxTrackBreadth().length().isMinContent() || maxTrackBreadth().length().isMaxContent()); }
    bool hasMaxContentMaxTrackBreadth() const { return maxTrackBreadth().isLength() && maxTrackBreadth().length().isMaxContent(); }

private:
    GridTrackSizeType m_type;
    GridLength m_minTrackBreadth;
    GridLength m_maxTrackBreadth;
};

} // namespace WebCore

#endif // GridTrackSize_h
