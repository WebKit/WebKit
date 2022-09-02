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

#pragma once

#include "DisplayBox.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Display {

class BoxModelBox;

// Container for a display box and its descendants, for boxes that participate in the CSS stacking
// algorithm <https://www.w3.org/TR/CSS22/zindex.html>, i.e. positioned boxes, and those that create
// CSS stacking context.

class StackingItem {
    WTF_MAKE_FAST_ALLOCATED;
    friend class TreeBuilder;
public:
    using StackingItemList = Vector<std::unique_ptr<StackingItem>>;

    explicit StackingItem(std::unique_ptr<BoxModelBox>&&);

    void addChildStackingItem(std::unique_ptr<StackingItem>&&);
    const BoxModelBox& box() const { return *m_box; }
    
    bool isStackingContext() const;

    // Bounds for the painted content of this stacking item (excluding descendant items), not taking clips from ancestors into account.
    UnadjustedAbsoluteFloatRect paintedContentBounds() const { return m_paintedContentBounds; }
    // Bounds for the painted content of this stacking item and descendant stacking items.
    UnadjustedAbsoluteFloatRect paintedBoundsIncludingDescendantItems() const { return m_paintedBoundsIncludingDescendantItems; }

    const StackingItemList& negativeZOrderList() const { return m_negativeZOrderList; }
    const StackingItemList& positiveZOrderList() const { return m_positiveZOrderList; }

private:
    void sortLists();

    void setPaintedContentBounds(const UnadjustedAbsoluteFloatRect& bounds) { m_paintedContentBounds = bounds; }
    void setPaintedBoundsIncludingDescendantItems(const UnadjustedAbsoluteFloatRect& bounds) { m_paintedBoundsIncludingDescendantItems = bounds; }

    std::unique_ptr<BoxModelBox> m_box;
    StackingItemList m_negativeZOrderList;
    StackingItemList m_positiveZOrderList;
    
    UnadjustedAbsoluteFloatRect m_paintedContentBounds;
    UnadjustedAbsoluteFloatRect m_paintedBoundsIncludingDescendantItems;
};

} // namespace Display
} // namespace WebCore


