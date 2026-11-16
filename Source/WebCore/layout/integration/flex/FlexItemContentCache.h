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

#include <WebCore/LayoutUnit.h>
#include <WebCore/RenderBox.h>
#include <wtf/HashMap.h>
#include <wtf/WeakRef.h>

namespace WebCore {
namespace LayoutIntegration {

// FlexItemContentCache caches per-flex-item measurements for subsequent layouts.
class FlexItemContentCache {
    WTF_MAKE_TZONE_ALLOCATED(FlexItemContentCache);
public:
    // The item's inner main size, for flex items whose main axis is their block axis (column flex or orthogonal).
    std::optional<LayoutUnit> blockAxisSize(const RenderBox& flexItem) const { return m_blockAxisSize.getOptional(flexItem); }
    void setBlockAxisSize(const RenderBox& flexItem, LayoutUnit size) { m_blockAxisSize.set(flexItem, size); }
    void clearBlockAxisSize(const RenderBox& flexItem) { m_blockAxisSize.remove(flexItem); }

    // The item's content height on the cross axis, captured before min/max or a stretch override constrains it, so stretching does not need a relayout to recover it.
    std::optional<LayoutUnit> contentLogicalHeight(const RenderBox& flexItem) const { return m_contentLogicalHeights.getOptional(flexItem); }
    void setContentLogicalHeight(const RenderBox& flexItem, LayoutUnit height) { m_contentLogicalHeights.set(flexItem, height); }

    void remove(const RenderBox& flexItem)
    {
        m_blockAxisSize.remove(flexItem);
        m_contentLogicalHeights.remove(flexItem);
    }

private:
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_blockAxisSize;
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_contentLogicalHeights;
};

} // namespace LayoutIntegration
} // namespace WebCore
