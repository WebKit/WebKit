/*
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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

#include "GridLayoutFunctions.h"
#include "RenderStyleConstants.h"
#include "WritingMode.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

// These classes are used to implement the Baseline Alignment logic, as described in the CSS Box Alignment
// specification.
// https://drafts.csswg.org/css-align/#baseline-terms
//
// A baseline-sharing group is composed of boxes that participate in baseline alignment together. This is
// possible only if they:
//
//   * Share an alignment context along an axis perpendicular to their baseline alignment axis.
//   * Have compatible baseline alignment preferences (i.e., the baselines that want to align are on the same
//     side of the alignment context).
//
// Once the BaselineGroup is instantiated, defined by a 'block-direction' (WritingMode) and a 'baseline-preference'
// (first/last baseline), it's ready to collect the items that will participate in the Baseline Alignment logic.
//
class BaselineGroup {
public:
    // It stores an item (if not already present) and update the max_ascent associated to this
    // baseline-sharing group.
    void update(const RenderBox&, LayoutUnit ascent);
    LayoutUnit maxAscent() const { return m_maxAscent; }
    int size() const { return m_items.size(); }

private:
    friend class BaselineContext;
    BaselineGroup(WritingMode blockFlow, ItemPosition childPreference);

    // Determines whether a baseline-sharing group is compatible with an item, based on its 'block-flow' and
    // 'baseline-preference'
    bool isCompatible(WritingMode, ItemPosition) const;

    // Determines whether the baseline-sharing group's associated block-flow is opposite (LR vs RL) to particular
    // item's writing-mode.
    bool isOppositeBlockFlow(WritingMode blockFlow) const;

    // Determines whether the baseline-sharing group's associated block-flow is orthogonal (vertical vs horizontal)
    // to particular item's writing-mode.
    bool isOrthogonalBlockFlow(WritingMode blockFlow) const;

    WritingMode m_blockFlow;
    ItemPosition m_preference;
    LayoutUnit m_maxAscent;
    HashSet<const RenderBox*> m_items;
};

// https://drafts.csswg.org/css-align-3/#shared-alignment-context
// Boxes share an alignment context along a particular axis when they are:
//
//  * table cells in the same row, along the table's row (inline) axis
//  * table cells in the same column, along the table's column (block) axis
//  * grid items in the same row, along the grid's row (inline) axis
//  * grid items in the same column, along the grid's colum (block) axis
//  * flex items in the same flex line, along the flex container's main axis
//
// https://drafts.csswg.org/css-align-3/#baseline-sharing-group
// A Baseline alignment-context may handle several baseline-sharing groups. In order to create an instance, we
// need to pass the required data to define the first baseline-sharing group; a Baseline Context must have at
// least one baseline-sharing group.
//
// By adding new items to a Baseline Context, the baseline-sharing groups it handles are automatically updated,
// if there is one that is compatible with such item. Otherwise, a new baseline-sharing group is created,
// compatible with the new item.
class BaselineContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BaselineContext(const RenderBox& child, ItemPosition preference, LayoutUnit ascent);
    const BaselineGroup& sharedGroup(const RenderBox& child, ItemPosition preference) const;

    // Updates the baseline-sharing group compatible with the item.
    // We pass the item's baseline-preference to avoid dependencies with the LayoutGrid class, which is the one
    // managing the alignment behavior of the Grid Items.
    void updateSharedGroup(const RenderBox& child, ItemPosition preference, LayoutUnit ascent);

private:
    // Returns the baseline-sharing group compatible with an item.
    // We pass the item's baseline-preference to avoid dependencies with the LayoutGrid class, which is the one
    // managing the alignment behavior of the Grid Items.
    // FIXME: Properly implement baseline-group compatibility.
    // See https://github.com/w3c/csswg-drafts/issues/721
    BaselineGroup& findCompatibleSharedGroup(const RenderBox& child, ItemPosition preference);

    Vector<BaselineGroup> m_sharedGroups;
};

enum AllowedBaseLine {FirstLine, LastLine, BothLines};

static inline bool isBaselinePosition(ItemPosition position)
{
    return position == ItemPosition::Baseline || position == ItemPosition::LastBaseline;
}

static inline bool isFirstBaselinePosition(ItemPosition position)
{
    return position == ItemPosition::Baseline;
}

// This is the class that implements the Baseline Alignment logic, using internally the BaselineContext and
// BaselineGroupd classes (described above).
//
// The first phase is to collect the items that will participate in baseline alignment together. During this
// phase the required baseline-sharing groups will be created for each Baseline alignment-context shared by
// the items participating in the baseline alignment.
//
// Additionally, the baseline-sharing groups' offsets, max-ascend and max-descent will be computed and stored.
// This class also computes the baseline offset for a particular item, based on the max-ascent for its associated
// baseline-sharing group.
class GridBaselineAlignment {
public:
    // Collects the items participating in baseline alignment and updates the corresponding baseline-sharing
    // group of the Baseline Context the items belongs to.
    // All the baseline offsets are updated accordingly based on the added item.
    void updateBaselineAlignmentContext(ItemPosition, unsigned sharedContext, const RenderBox&, GridAxis);

    // Returns the baseline offset of a particular item, based on the max-ascent for its associated
    // baseline-sharing group
    LayoutUnit baselineOffsetForChild(ItemPosition, unsigned sharedContext, const RenderBox&, GridAxis) const;

    // Sets the Grid Container's writing-mode so that we can avoid the dependecy of the LayoutGrid class for
    // determining whether a grid item is orthogonal or not.
    void setBlockFlow(WritingMode blockFlow) { m_blockFlow = blockFlow; };

    // Clearing the Baseline Alignment context and their internal classes and data structures.
    void clear(GridAxis);

private:
    const BaselineGroup& baselineGroupForChild(ItemPosition, unsigned sharedContext, const RenderBox&, GridAxis) const;
    LayoutUnit marginOverForChild(const RenderBox&, GridAxis) const;
    LayoutUnit marginUnderForChild(const RenderBox&, GridAxis) const;
    LayoutUnit logicalAscentForChild(const RenderBox&, GridAxis, ItemPosition) const;
    LayoutUnit ascentForChild(const RenderBox&, GridAxis, ItemPosition) const;
    LayoutUnit descentForChild(const RenderBox&, LayoutUnit, GridAxis) const;
    bool isDescentBaselineForChild(const RenderBox&, GridAxis) const;
    bool isHorizontalBaselineAxis(GridAxis) const;
    bool isOrthogonalChildForBaseline(const RenderBox&) const;
    bool isParallelToBaselineAxisForChild(const RenderBox&, GridAxis) const;

    typedef HashMap<unsigned, std::unique_ptr<BaselineContext>, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> BaselineContextsMap;

    // Grid Container's WritingMode, used to determine grid item's orthogonality.
    WritingMode m_blockFlow;
    BaselineContextsMap m_rowAxisAlignmentContext;
    BaselineContextsMap m_colAxisAlignmentContext;
};

} // namespace WebCore
