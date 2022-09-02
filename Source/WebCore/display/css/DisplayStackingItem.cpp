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
#include "DisplayStackingItem.h"

#include "DisplayBoxModelBox.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Display {

StackingItem::StackingItem(std::unique_ptr<BoxModelBox>&& box)
    : m_box(WTFMove(box))
{
    ASSERT(m_box);
}

bool StackingItem::isStackingContext() const
{
    return m_box->style().isStackingContext();
}

void StackingItem::addChildStackingItem(std::unique_ptr<StackingItem>&& item)
{
    auto zIndex = item->box().style().zIndex().value_or(0);
    if (zIndex < 0)
        m_negativeZOrderList.append(WTFMove(item));
    else
        m_positiveZOrderList.append(WTFMove(item));
}

void StackingItem::sortLists()
{
    auto compareZIndex = [](const std::unique_ptr<StackingItem>& a, const std::unique_ptr<StackingItem>& b) {
        return a->box().style().zIndex().value_or(0) < b->box().style().zIndex().value_or(0);
    };

    std::stable_sort(m_positiveZOrderList.begin(), m_positiveZOrderList.end(), compareZIndex);
    std::stable_sort(m_negativeZOrderList.begin(), m_negativeZOrderList.end(), compareZIndex);
}


} // namespace Display
} // namespace WebCore

