/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "OrderIterator.h"

#include "RenderBox.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <algorithm>

namespace WebCore {

bool OrderIterator::shouldSkipChild(const RenderObject& child) const
{
    return child.isOutOfFlowPositioned() || child.isExcludedFromNormalLayout();
}

OrderIteratorPopulator::~OrderIteratorPopulator()
{
    // Document order is already order-modified document order when every item has the initial order.
    if (!m_iterator.m_hasNonZeroOrder)
        return;

    std::ranges::stable_sort(m_iterator.m_gridItems, { }, &OrderIterator::GridItemAndOrder::order);
}

bool OrderIteratorPopulator::collectChild(RenderBox& child)
{
    if (m_iterator.shouldSkipChild(child))
        return false;

    auto order = child.style().order().value;
    if (order)
        m_iterator.m_hasNonZeroOrder = true;
    m_iterator.m_gridItems.append(OrderIterator::GridItemAndOrder { child, order });
    return true;
}


} // namespace WebCore
