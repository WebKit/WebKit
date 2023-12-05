/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AncestorSubgridIterator.h"

#include "RenderIterator.h"

namespace WebCore {

AncestorSubgridIterator::AncestorSubgridIterator() { }

AncestorSubgridIterator::AncestorSubgridIterator(SingleThreadWeakPtr<RenderGrid> firstAncestorSubgrid, GridTrackSizingDirection direction)
    : m_firstAncestorSubgrid(firstAncestorSubgrid)
    , m_direction(direction)
{
    ASSERT_IMPLIES(firstAncestorSubgrid,  firstAncestorSubgrid->isSubgrid(direction));
}


AncestorSubgridIterator::AncestorSubgridIterator(SingleThreadWeakPtr<RenderGrid> firstAncestorSubgrid, SingleThreadWeakPtr<RenderGrid> currentAncestorSubgrid, GridTrackSizingDirection direction)
    : m_firstAncestorSubgrid(firstAncestorSubgrid)
    , m_currentAncestorSubgrid(currentAncestorSubgrid)
    , m_direction(direction)
{
    ASSERT_IMPLIES(firstAncestorSubgrid, firstAncestorSubgrid->isSubgrid(direction));
}

AncestorSubgridIterator::AncestorSubgridIterator(SingleThreadWeakPtr<RenderGrid> firstAncestorSubgrid, SingleThreadWeakPtr<RenderGrid> currentAncestorSubgrid, std::optional<GridTrackSizingDirection> direction)
    : m_firstAncestorSubgrid(firstAncestorSubgrid)
    , m_currentAncestorSubgrid(currentAncestorSubgrid)
    , m_direction(direction)
{
}

AncestorSubgridIterator AncestorSubgridIterator::begin()
{
    return AncestorSubgridIterator(m_firstAncestorSubgrid, m_firstAncestorSubgrid, m_direction);
}

AncestorSubgridIterator AncestorSubgridIterator::end()
{
    return AncestorSubgridIterator(m_firstAncestorSubgrid, nullptr, m_direction);
}

AncestorSubgridIterator& AncestorSubgridIterator::operator++()
{
    ASSERT(m_firstAncestorSubgrid && m_currentAncestorSubgrid && m_direction);

    if (m_firstAncestorSubgrid && m_currentAncestorSubgrid && m_direction) {
        auto nextAncestor = RenderTraversal::findAncestorOfType<RenderGrid>(*m_currentAncestorSubgrid);
        m_currentAncestorSubgrid = (nextAncestor && nextAncestor->isSubgrid(GridLayoutFunctions::flowAwareDirectionForChild(*nextAncestor, *m_firstAncestorSubgrid, m_direction.value()))) ? nextAncestor : nullptr;
    }
    return *this;
}

RenderGrid& AncestorSubgridIterator::operator*()
{
    ASSERT(m_currentAncestorSubgrid);
    return *m_currentAncestorSubgrid;
}

bool AncestorSubgridIterator::operator==(const AncestorSubgridIterator& other) const
{
    return m_currentAncestorSubgrid == other.m_currentAncestorSubgrid && m_firstAncestorSubgrid == other.m_firstAncestorSubgrid && m_direction == other.m_direction;
}

AncestorSubgridIterator ancestorSubgridsOfGridItem(const RenderBox& gridItem,  const GridTrackSizingDirection direction)
{
    ASSERT(gridItem.parent()->isRenderGrid());
    if (const auto* gridItemParent = dynamicDowncast<RenderGrid>(gridItem.parent()); gridItemParent && gridItemParent->isSubgrid(direction))
        return AncestorSubgridIterator(gridItemParent, direction);
    return AncestorSubgridIterator();
}

} // namespace WebCore
