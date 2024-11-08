/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationFormattingContextLayout.h"

#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"

namespace WebCore {
namespace LayoutIntegration {

void layoutWithFormattingContextForBox(const Layout::ElementBox& box, std::optional<LayoutUnit> widthConstraint, Layout::LayoutState& layoutState)
{
    auto& renderer = downcast<RenderBox>(*box.rendererForIntegration());

    if (widthConstraint) {
        renderer.setOverridingLogicalWidthLength({ *widthConstraint, LengthType::Fixed });
        renderer.setNeedsLayout(MarkOnlyThis);
    }

    renderer.layoutIfNeeded();

    if (widthConstraint)
        renderer.clearOverridingLogicalWidthLength();

    auto rootLayoutBox = [&]() -> const Layout::ElementBox& {
        auto* ancestor = &box.parent();
        while (!ancestor->isInitialContainingBlock()) {
            if (ancestor->establishesFormattingContext())
                break;
            ancestor = &ancestor->parent();
        }
        return *ancestor;
    };
    auto updater = BoxGeometryUpdater { layoutState, rootLayoutBox() };
    updater.updateBoxGeometryAfterIntegrationLayout(box, widthConstraint.value_or(renderer.containingBlock()->availableLogicalWidth()));
}

LayoutUnit formattingContextRootLogicalWidthForType(const Layout::ElementBox& box, LogicalWidthType logicalWidthType)
{
    ASSERT(box.establishesFormattingContext());

    auto& renderer = downcast<RenderBox>(*box.rendererForIntegration());
    switch (logicalWidthType) {
    case LogicalWidthType::PreferredMaximum:
        return renderer.maxPreferredLogicalWidth();
    case LogicalWidthType::PreferredMinimum:
        return renderer.minPreferredLogicalWidth();
    case LogicalWidthType::MaxContent:
    case LogicalWidthType::MinContent: {
        auto minimunLogicalWidth = LayoutUnit { };
        auto maximumLogicalWidth = LayoutUnit { };
        renderer.computeIntrinsicLogicalWidths(minimunLogicalWidth, maximumLogicalWidth);
        return logicalWidthType == LogicalWidthType::MaxContent ? maximumLogicalWidth : minimunLogicalWidth;
    }
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

LayoutUnit formattingContextRootLogicalHeightForType(const Layout::ElementBox& box, LogicalHeightType logicalHeightType)
{
    ASSERT(box.establishesFormattingContext());

    auto& renderer = downcast<RenderBox>(*box.rendererForIntegration());
    switch (logicalHeightType) {
    case LogicalHeightType::MinContent: {
        // Since currently we can't ask RenderBox for content height, this is limited to flex items
        // where the legacy flex layout "fixed" this by caching the content height in RenderBox::updateLogicalHeight
        // before additional height constraints applied.
        if (auto* flexContainer = dynamicDowncast<RenderFlexibleBox>(renderer.parent()))
            return flexContainer->cachedFlexItemIntrinsicContentLogicalHeight(renderer);
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

}
}
