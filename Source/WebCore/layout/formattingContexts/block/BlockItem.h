/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/CheckedRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBox;

namespace Style {
class ComputedStyle;
}

namespace Layout {

class BlockItem {
public:
    enum class Type : uint8_t {
        // A block container whose content takes part in this block formatting context: its children come between its start and end.
        BlockContainerStart,
        BlockContainerEnd,
        InlineContent,
        // A box whose content is laid out in a formatting context of its own; to this one it is a single box.
        IndependentFormattingContextRoot
    };
    BlockItem(const RenderBox&, Type);

    Type type() const { return m_type; }
    const RenderBox& renderer() const { return m_renderer.get(); }

private:
    CheckedRef<const RenderBox> m_renderer;
    Type m_type;
};

using BlockItemList = Vector<BlockItem>;

inline BlockItem::BlockItem(const RenderBox& renderer, Type type)
    : m_renderer(renderer)
    , m_type(type)
{
}

}
}
