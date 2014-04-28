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

namespace WebCore {

RenderBox* OrderIterator::currentChild() const
{
    if (m_childrenIndex == m_children.size())
        return nullptr;

    return m_children[m_childrenIndex].first;
}

RenderBox* OrderIterator::first()
{
    m_childrenIndex = 0;
    return currentChild();
}

RenderBox* OrderIterator::next()
{
    ++m_childrenIndex;
    return currentChild();
}

static bool compareByOrderValueAndIndex(std::pair<RenderBox*, int> childAndIndex1, std::pair<RenderBox*, int> childAndIndex2)
{
    if (childAndIndex1.first->style().order() != childAndIndex2.first->style().order())
        return childAndIndex1.first->style().order() < childAndIndex2.first->style().order();
    return childAndIndex1.second < childAndIndex2.second;
}

OrderIteratorPopulator::~OrderIteratorPopulator()
{
    if (!m_allChildrenHaveDefaultOrderValue)
        std::sort(m_iterator.m_children.begin(), m_iterator.m_children.end(), compareByOrderValueAndIndex);
}

void OrderIteratorPopulator::collectChild(RenderBox& child)
{
    std::pair<RenderBox*, int> childAndIndex = { &child, m_childIndex++ };
    m_iterator.m_children.append(childAndIndex);

    if (m_allChildrenHaveDefaultOrderValue && child.style().order())
        m_allChildrenHaveDefaultOrderValue = false;
}


} // namespace WebCore
