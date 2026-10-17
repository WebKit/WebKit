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

#include <WebCore/LayoutRect.h>
#include <WebCore/LayoutUnit.h>
#include <wtf/CheckedRef.h>
#include <wtf/WeakHashMap.h>

namespace WebCore {

class RenderBlockFlow;

namespace Layout {
class BlockLayoutState;
class BoxGeometry;
struct ConstraintsForInlineContent;
}

namespace LayoutIntegration {

class PaintInvalidation {
    WTF_MAKE_NONCOPYABLE(PaintInvalidation);
public:
    PaintInvalidation() = default;

    class InlineContentScope {
        WTF_MAKE_NONCOPYABLE(InlineContentScope);
    public:
        InlineContentScope(PaintInvalidation&, RenderBlockFlow&, bool isFullLayout);
        ~InlineContentScope();

        void setDamagedRect(std::optional<LayoutRect> damagedRect) { m_damagedRect = damagedRect; }

    private:
        PaintInvalidation& m_paintInvalidation;
        CheckedRef<RenderBlockFlow> m_blockContainer;
        bool m_isFullLayout { false };
        std::pair<float, float> m_contentTopAndBottomBeforeLayout;
        std::optional<LayoutRect> m_damagedRect;
    };

    struct LogicalTopAndBottom {
        LayoutUnit logicalTop;
        LayoutUnit logicalBottom;
    };
    std::optional<LogicalTopAndBottom> damagedInlineContentLogicalTopAndBottom(RenderBlockFlow&, LayoutUnit borderBoxBlockSizeFromContent);

private:
    struct InlineContent {
        bool isFullLayout { false };
        std::optional<LayoutRect> damagedRect;
        std::pair<float, float> contentTopAndBottomBeforeLayout;
    };
    SingleThreadWeakHashMap<RenderBlockFlow, InlineContent> m_inlineContent;
};

class BlockIntegrationUtils {
public:
    explicit BlockIntegrationUtils(PaintInvalidation&);

    PaintInvalidation& paintInvalidation() const { return m_paintInvalidation; }

    struct InlineContentLayoutResult {
        // From the content box top to the bottom of the last line box (CSS 2.2 §10.6.3).
        LayoutUnit contentBlockSize;
        // Only line boxes separate margins: content whose line boxes are all phantom does not (CSS 2.2 §8.3.1, css-inline-3).
        bool hasLineBoxes { false };
    };
    InlineContentLayoutResult layoutInlineContent(const RenderBlockFlow&, const Layout::ConstraintsForInlineContent&, const Layout::BoxGeometry&, const Layout::BlockLayoutState&);

private:
    PaintInvalidation& m_paintInvalidation;
};

} // namespace LayoutIntegration
} // namespace WebCore
