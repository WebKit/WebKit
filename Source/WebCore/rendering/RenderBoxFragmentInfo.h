/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

#include "RenderOverflow.h"

namespace WebCore {

class RenderBoxFragmentInfo {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RenderBoxFragmentInfo);
public:
    RenderBoxFragmentInfo(LayoutUnit logicalLeft, LayoutUnit logicalWidth, bool isShifted)
        : m_logicalLeft(logicalLeft)
        , m_logicalWidth(logicalWidth)
        , m_isShifted(isShifted)
    { }

    LayoutUnit logicalLeft() const { return m_logicalLeft; }
    LayoutUnit logicalWidth() const { return m_logicalWidth; }
    
    void shiftLogicalLeft(LayoutUnit delta) { m_logicalLeft += delta; m_isShifted = true; }

    bool isShifted() const { return m_isShifted; }

    void createOverflow(const LayoutRect& layoutOverflow, const LayoutRect& visualOverflow) { m_overflow = adoptRef(new RenderOverflow(layoutOverflow, visualOverflow)); }
    RenderOverflow* overflow() const { return m_overflow.get(); }
    void clearOverflow()
    {
        if (m_overflow)
            m_overflow = nullptr;
    }

private:
    LayoutUnit m_logicalLeft;
    LayoutUnit m_logicalWidth;
    bool m_isShifted;
    RefPtr<RenderOverflow> m_overflow;
};

} // namespace WebCore
