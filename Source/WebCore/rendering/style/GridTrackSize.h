/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
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

#if ENABLE(CSS_GRID_LAYOUT)

#include "GridLength.h"

namespace WebCore {

enum GridTrackSizeType {
    LengthTrackSizing,
    MinMaxTrackSizing
};

class GridTrackSize {
public:
    GridTrackSize(const GridLength& length)
        : m_type(LengthTrackSizing)
        , m_minTrackBreadth(length)
        , m_maxTrackBreadth(length)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(const GridLength& minTrackBreadth, const GridLength& maxTrackBreadth)
        : m_type(MinMaxTrackSizing)
        , m_minTrackBreadth(minTrackBreadth)
        , m_maxTrackBreadth(maxTrackBreadth)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    const GridLength& length() const
    {
        ASSERT(m_type == LengthTrackSizing);
        ASSERT(m_minTrackBreadth == m_maxTrackBreadth);
        const GridLength& minTrackBreadth = m_minTrackBreadth;
        return minTrackBreadth;
    }

    const GridLength& minTrackBreadth() const { return m_minTrackBreadth; }

    const GridLength& maxTrackBreadth() const { return m_maxTrackBreadth; }

    GridTrackSizeType type() const { return m_type; }

    bool isContentSized() const { return m_minTrackBreadth.isContentSized() || m_maxTrackBreadth.isContentSized(); }

    bool isPercentage() const { return m_type == LengthTrackSizing && length().isLength() && length().length().isPercentOrCalculated(); }

    bool operator==(const GridTrackSize& other) const
    {
        return m_type == other.m_type && m_minTrackBreadth == other.m_minTrackBreadth && m_maxTrackBreadth == other.m_maxTrackBreadth;
    }

    void cacheMinMaxTrackBreadthTypes()
    {
        m_minTrackBreadthIsAuto = minTrackBreadth().isLength() && minTrackBreadth().length().isAuto();
        m_minTrackBreadthIsMinContent = minTrackBreadth().isLength() && minTrackBreadth().length().isMinContent();
        m_minTrackBreadthIsMaxContent = minTrackBreadth().isLength() && minTrackBreadth().length().isMaxContent();
        m_maxTrackBreadthIsMaxContent = maxTrackBreadth().isLength() && maxTrackBreadth().length().isMaxContent();
        m_maxTrackBreadthIsMinContent = maxTrackBreadth().isLength() && maxTrackBreadth().length().isMinContent();
        m_maxTrackBreadthIsAuto = maxTrackBreadth().isLength() && maxTrackBreadth().length().isAuto();
    }

    bool hasIntrinsicMinTrackBreadth() const { return m_minTrackBreadthIsMaxContent || m_minTrackBreadthIsMinContent || m_minTrackBreadthIsAuto; }
    bool hasMinOrMaxContentMinTrackBreadth() const { return m_minTrackBreadthIsMaxContent || m_minTrackBreadthIsMinContent; }
    bool hasAutoMinTrackBreadth() const { return m_minTrackBreadthIsAuto; }
    bool hasAutoMaxTrackBreadth() const { return m_maxTrackBreadthIsAuto; }
    bool hasMaxContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent; }
    bool hasMaxContentOrAutoMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsAuto; }
    bool hasMinContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMinContent; }
    bool hasMinOrMaxContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsMinContent; }
    bool hasMaxContentMinTrackBreadth() const { return m_minTrackBreadthIsMaxContent; }
    bool hasMinContentMinTrackBreadth() const { return m_minTrackBreadthIsMinContent; }
    bool hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth() const { return m_minTrackBreadthIsMaxContent && m_maxTrackBreadthIsMaxContent; }
    bool hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth() const { return (m_minTrackBreadthIsMinContent || m_minTrackBreadthIsAuto) && (m_maxTrackBreadthIsAuto || hasMinOrMaxContentMaxTrackBreadth()); }

private:
    GridTrackSizeType m_type;
    GridLength m_minTrackBreadth;
    GridLength m_maxTrackBreadth;
    bool m_minTrackBreadthIsAuto;
    bool m_minTrackBreadthIsMaxContent;
    bool m_minTrackBreadthIsMinContent;
    bool m_maxTrackBreadthIsAuto;
    bool m_maxTrackBreadthIsMaxContent;
    bool m_maxTrackBreadthIsMinContent;
};

} // namespace WebCore

#endif /* ENABLE(CSS_GRID_LAYOUT) */

#endif // GridTrackSize_h
