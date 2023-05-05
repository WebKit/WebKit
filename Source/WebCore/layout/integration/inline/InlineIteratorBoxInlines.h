/**
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "InlineIteratorBox.h"
#include "InlineIteratorBoxModernPathInlines.h"

namespace WebCore {
namespace InlineIterator {

inline float Box::logicalBottom() const { return logicalRectIgnoringInlineDirection().maxY(); }
inline float Box::logicalHeight() const { return logicalRectIgnoringInlineDirection().height(); }
inline float Box::logicalLeftIgnoringInlineDirection() const { return logicalRectIgnoringInlineDirection().x(); }
inline float Box::logicalRightIgnoringInlineDirection() const { return logicalRectIgnoringInlineDirection().maxX(); }
inline float Box::logicalTop() const { return logicalRectIgnoringInlineDirection().y(); }
inline float Box::logicalWidth() const { return logicalRectIgnoringInlineDirection().width(); }

inline bool Box::isHorizontal() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isHorizontal();
    });
}

inline FloatRect Box::logicalRectIgnoringInlineDirection() const
{
    auto rect = this->visualRectIgnoringBlockDirection();
    return isHorizontal() ? rect : rect.transposedRect();
}

}
}
