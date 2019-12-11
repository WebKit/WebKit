/*
 * Copyright (C) 2015 Igalia S.L. All rights reserved.
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

#pragma once

#include "RenderStyleConstants.h"

namespace WebCore {

class StyleSelfAlignmentData {
public:
    // Style data for Self-Aligment and Default-Alignment properties: align-{self, items}, justify-{self, items}.
    // [ <self-position> && <overflow-position>? ] | [ legacy && [ left | right | center ] ]
    StyleSelfAlignmentData(ItemPosition position, OverflowAlignment overflow, ItemPositionType positionType = ItemPositionType::NonLegacy)
        : m_position(static_cast<unsigned>(position))
        , m_positionType(static_cast<unsigned>(positionType))
        , m_overflow(static_cast<unsigned>(overflow))
    {
    }

    void setPosition(ItemPosition position) { m_position = static_cast<unsigned>(position); }
    void setPositionType(ItemPositionType positionType) { m_positionType = static_cast<unsigned>(positionType); }
    void setOverflow(OverflowAlignment overflow) { m_overflow = static_cast<unsigned>(overflow); }

    ItemPosition position() const { return static_cast<ItemPosition>(m_position); }
    ItemPositionType positionType() const { return static_cast<ItemPositionType>(m_positionType); }
    OverflowAlignment overflow() const { return static_cast<OverflowAlignment>(m_overflow); }

    bool operator==(const StyleSelfAlignmentData& o) const
    {
        return m_position == o.m_position && m_positionType == o.m_positionType && m_overflow == o.m_overflow;
    }

    bool operator!=(const StyleSelfAlignmentData& o) const
    {
        return !(*this == o);
    }

private:
    unsigned m_position : 4; // ItemPosition
    unsigned m_positionType: 1; // Whether or not alignment uses the 'legacy' keyword.
    unsigned m_overflow : 2; // OverflowAlignment
};

} // namespace WebCore
