/*
 * Copyright (C) 2023 Apple Inc.
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
#include "BaselineAlignment.h"

#include "BaselineAlignmentInlines.h"
#include "RenderBox.h"
#include "RenderStyleInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BaselineGroup);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BaselineAlignmentState);

BaselineGroup::BaselineGroup(FlowDirection blockFlow, ItemPosition childPreference)
    : m_maxAscent(0), m_items()
{
    m_blockFlow = blockFlow;
    m_preference = childPreference;
}

void BaselineGroup::update(const RenderBox& child, LayoutUnit ascent)
{
    if (m_items.add(child).isNewEntry)
        m_maxAscent = std::max(m_maxAscent, ascent);
}

bool BaselineGroup::isOppositeBlockFlow(FlowDirection blockFlow) const
{
    switch (blockFlow) {
    case FlowDirection::TopToBottom:
        return false;
    case FlowDirection::LeftToRight:
        return m_blockFlow == FlowDirection::RightToLeft;
    case FlowDirection::RightToLeft:
        return m_blockFlow == FlowDirection::LeftToRight;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool BaselineGroup::isOrthogonalBlockFlow(FlowDirection blockFlow) const
{
    switch (blockFlow) {
    case FlowDirection::TopToBottom:
        return m_blockFlow != FlowDirection::TopToBottom;
    case FlowDirection::LeftToRight:
    case FlowDirection::RightToLeft:
        return m_blockFlow == FlowDirection::TopToBottom;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool BaselineGroup::isCompatible(FlowDirection childBlockFlow, ItemPosition childPreference) const
{
    ASSERT(isBaselinePosition(childPreference));
    ASSERT(computeSize() > 0);
    return ((m_blockFlow == childBlockFlow || isOrthogonalBlockFlow(childBlockFlow)) && m_preference == childPreference) || (isOppositeBlockFlow(childBlockFlow) && m_preference != childPreference);
}

BaselineAlignmentState::BaselineAlignmentState(const RenderBox& child, ItemPosition preference, LayoutUnit ascent)
{
    ASSERT(isBaselinePosition(preference));
    updateSharedGroup(child, preference, ascent);
}

const BaselineGroup& BaselineAlignmentState::sharedGroup(const RenderBox& child, ItemPosition preference) const
{
    ASSERT(isBaselinePosition(preference));
    return const_cast<BaselineAlignmentState*>(this)->findCompatibleSharedGroup(child, preference);
}

Vector<BaselineGroup>& BaselineAlignmentState::sharedGroups()
{
    return m_sharedGroups;
}

void BaselineAlignmentState::updateSharedGroup(const RenderBox& child, ItemPosition preference, LayoutUnit ascent)
{
    ASSERT(isBaselinePosition(preference));
    BaselineGroup& group = findCompatibleSharedGroup(child, preference);
    group.update(child, ascent);
}

// FIXME: Properly implement baseline-group compatibility.
// See https://github.com/w3c/csswg-drafts/issues/721
BaselineGroup& BaselineAlignmentState::findCompatibleSharedGroup(const RenderBox& child, ItemPosition preference)
{
    auto blockFlowDirection = child.writingMode().blockDirection();
    for (auto& group : m_sharedGroups) {
        if (group.isCompatible(blockFlowDirection, preference))
            return group;
    }
    m_sharedGroups.insert(0, BaselineGroup(blockFlowDirection, preference));
    return m_sharedGroups[0];
}

} // namespace WebCore
