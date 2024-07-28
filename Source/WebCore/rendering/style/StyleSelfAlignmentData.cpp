/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "StyleSelfAlignmentData.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, const StyleSelfAlignmentData& o)
{
    return ts << o.position() << " " << o.positionType() << " " << o.overflow();
}

bool StyleSelfAlignmentData::isStartward(TextDirection direction, TextDirection parentDirection) const
{
    switch (static_cast<ItemPosition>(m_position)) {
    case ItemPosition::Normal:
    case ItemPosition::Stretch:
    case ItemPosition::Baseline:
    case ItemPosition::Start:
    case ItemPosition::FlexStart:
        return true;
    case ItemPosition::SelfStart:
        return direction == parentDirection;
    case ItemPosition::End:
    case ItemPosition::FlexEnd:
    case ItemPosition::LastBaseline:
    case ItemPosition::Center:
        return false;
    case ItemPosition::SelfEnd:
        return direction != parentDirection;
    case ItemPosition::Left:
        return parentDirection == TextDirection::LTR;
    case ItemPosition::Right:
        return parentDirection == TextDirection::RTL;
    default:
        ASSERT("Invalid ItemPosition");
        return true;
    };
}

bool StyleSelfAlignmentData::isEndward(TextDirection direction, TextDirection parentDirection) const
{
    switch (static_cast<ItemPosition>(m_position)) {
    case ItemPosition::Normal:
    case ItemPosition::Stretch:
    case ItemPosition::Baseline:
    case ItemPosition::Start:
    case ItemPosition::FlexStart:
    case ItemPosition::Center:
        return false;
    case ItemPosition::SelfStart:
        return direction != parentDirection;
    case ItemPosition::End:
    case ItemPosition::FlexEnd:
    case ItemPosition::LastBaseline:
        return true;
    case ItemPosition::SelfEnd:
        return direction == parentDirection;
    case ItemPosition::Left:
        return parentDirection == TextDirection::RTL;
    case ItemPosition::Right:
        return parentDirection == TextDirection::LTR;
    default:
        ASSERT("Invalid ItemPosition");
        return false;
    };
}

bool StyleSelfAlignmentData::isCentered() const
{
    return position() == ItemPosition::Center;
}

} // namespace WebCore
