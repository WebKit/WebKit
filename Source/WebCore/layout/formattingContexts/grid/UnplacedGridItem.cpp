/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "UnplacedGridItem.h"

#include "LayoutElementBox.h"

namespace WebCore {
namespace Layout {

size_t UnplacedGridItem::AxisPosition::span() const
{
    if (auto* definitePosition = std::get_if<DefinitePosition>(&m_position))
        return definitePosition->endLine - definitePosition->startLine;
    return std::get<AutoPosition>(m_position).span;
}

UnplacedGridItem::UnplacedGridItem(const ElementBox& layoutBox, AxisPosition nonAutoFlowPosition, AxisPosition autoFlowPosition)
    : m_layoutBox(layoutBox)
    , m_nonAutoFlowPosition(nonAutoFlowPosition)
    , m_autoFlowPosition(autoFlowPosition)
{
}

bool UnplacedGridItem::hasDefiniteAutoFlowPosition() const
{
    return m_autoFlowPosition.isDefinite();
}

bool UnplacedGridItem::hasDefiniteNonAutoFlowPosition() const
{
    return m_nonAutoFlowPosition.isDefinite();
}

size_t UnplacedGridItem::nonAutoFlowSpanSize() const
{
    return m_nonAutoFlowPosition.span();
}

size_t UnplacedGridItem::autoFlowSpanSize() const
{
    return m_autoFlowPosition.span();
}

WTF::Range<size_t> UnplacedGridItem::definiteAutoFlowRange() const
{
    auto& definitePosition = m_autoFlowPosition.definitePosition();
    return { definitePosition.startLine, definitePosition.endLine };
}

WTF::Range<size_t> UnplacedGridItem::definiteNonAutoFlowRange() const
{
    auto& definitePosition = m_nonAutoFlowPosition.definitePosition();
    return { definitePosition.startLine, definitePosition.endLine };
}

} // namespace Layout

} // namespace WebCore
