/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "InteractionRegion.h"

#include "Document.h"
#include "Frame.h"
#include "FrameSnapshotting.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "HTMLAnchorElement.h"
#include "HTMLFormControlElement.h"
#include "HitTestResult.h"
#include "Page.h"
#include "PathUtilities.h"
#include "PlatformMouseEvent.h"
#include "RenderBox.h"
#include "SimpleRange.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

InteractionRegion::~InteractionRegion() = default;

static FloatRect absoluteBoundingRectForRange(const SimpleRange& range)
{
    return unionRectIgnoringZeroRects(RenderObject::absoluteBorderAndTextRects(range, {
        RenderObject::BoundingRectBehavior::RespectClipping,
        RenderObject::BoundingRectBehavior::UseVisibleBounds,
        RenderObject::BoundingRectBehavior::IgnoreTinyRects,
    }));
}

static std::optional<InteractionRegion> regionForElement(Element& element)
{
    auto& frameView = *element.document().frame()->view();
    auto& mainFrameView = *element.document().frame()->mainFrame().view();
    
    IntRect frameClipRect;
#if PLATFORM(IOS_FAMILY)
    frameClipRect = enclosingIntRect(frameView.exposedContentRect());
#else
    if (auto viewExposedRect = frameView.viewExposedRect())
        frameClipRect = enclosingIntRect(*viewExposedRect);
    else
        frameClipRect = frameView.visibleContentRect();
#endif

    auto* renderer = element.renderer();
    if (!renderer)
        return std::nullopt;

    Vector<FloatRect> rectsInContentsCoordinates;
    
    InteractionRegion region;

    auto linkRange = makeRangeSelectingNode(element);
    
    if (linkRange)
        region.hasLightBackground = estimatedBackgroundColorForRange(*linkRange, *element.document().frame()).luminance() > 0.5;
    
    if (linkRange && renderer->isInline() && !renderer->isReplacedOrInlineBlock()) {
        region.isInline = true;

        OptionSet<RenderObject::BoundingRectBehavior> behavior { RenderObject::BoundingRectBehavior::RespectClipping };
        rectsInContentsCoordinates = RenderObject::absoluteTextRects(*linkRange, behavior).map([&](auto rect) -> FloatRect {
            return rect;
        });

        if (rectsInContentsCoordinates.isEmpty()) {
            auto boundingRectForRange = absoluteBoundingRectForRange(*linkRange);
            if (!boundingRectForRange.isEmpty())
                rectsInContentsCoordinates = { boundingRectForRange };
        }
    }

    if (rectsInContentsCoordinates.isEmpty())
        rectsInContentsCoordinates = { renderer->absoluteBoundingBoxRect() };
    
    if (is<RenderBox>(*renderer)) {
        RoundedRect::Radii borderRadii = downcast<RenderBox>(*renderer).borderRadii();
        region.borderRadius = borderRadii.minimumRadius();
    }

    region.rectsInContentCoordinates = rectsInContentsCoordinates.map([&](auto rect) {
        auto contentsRect = rect;

        if (&frameView != &mainFrameView)
            contentsRect.intersect(frameClipRect);

        return contentsRect;
    });
        
    return region;
}

static CursorType cursorTypeForElement(Element& element)
{
    auto* renderer = element.renderer();
    auto* style = renderer ? &renderer->style() : nullptr;
    auto cursorType = style ? style->cursor() : CursorType::Auto;

    if (cursorType == CursorType::Auto && element.enclosingLinkEventParentOrSelf() && element.isLink())
        cursorType = CursorType::Pointer;

    return cursorType;
}

Vector<InteractionRegion> interactionRegions(Page& page, FloatRect rect)
{
    Ref frame(page.mainFrame());
    RefPtr frameView = frame->view();
    
    if (!frameView)
        return { };

    frameView->updateLayoutAndStyleIfNeededRecursive();
    
    RefPtr document = frame->document();
    if (!document)
        return { };

    auto result = HitTestResult { LayoutRect(rect) };
    HitTestRequest request({
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::AllowVisibleChildFrameContentOnly,
        HitTestRequest::Type::CollectMultipleElements
    });
    document->hitTest(request, result);

    Vector<InteractionRegion> regions;

    for (const auto& node : result.listBasedTestResult()) {
        if (!is<Element>(node.get()))
            continue;
        auto& element = downcast<Element>(node.get());

        if (!element.willRespondToMouseClickEvents() && !element.willRespondToTouchEvents())
            continue;

        if (cursorTypeForElement(element) != CursorType::Pointer && !is<HTMLFormControlElement>(element))
            continue;

        auto region = regionForElement(element);
        if (region)
            regions.append(*region);
    }

    return regions;
}

}
