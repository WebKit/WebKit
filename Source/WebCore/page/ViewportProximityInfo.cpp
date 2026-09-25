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

#include "config.h"
#include "ViewportProximityInfo.h"

#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "FloatRect.h"
#include "HTMLFrameOwnerElement.h"
#include "LocalFrameView.h"
#include "Node.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "SimpleRange.h"
#include "Text.h"
#include "VisibleRectContext.h"
#include <cmath>

namespace WebCore {

static LayoutRect visibleAbsoluteRect(const RenderObject& renderer)
{
    return renderer.clippedOverflowRect(nullptr, {
        .options = {
            VisibleRectContext::Option::ApplyCompositedClips,
            VisibleRectContext::Option::ApplyCompositedContainerScrolls,
        },
    });
}

static std::optional<FloatRect> visibleViewportRectInRootView(const LocalFrameView& view)
{
    auto contentRect = view.unobscuredContentRect();
    if (contentRect.isEmpty())
        return std::nullopt;

    auto rectInRootView = view.contentsToRootView(contentRect);
    if (rectInRootView.isEmpty())
        return std::nullopt;

    return FloatRect { rectInRootView };
}

static ViewportProximityInfo viewportProximityInfo(const FloatRect& visibleBounds, const FloatRect& viewport)
{
    // FIXME: For cross-origin iframes, the proximity and relation is calculated with respect to its own viewport instead of the outermost viewport.
    if (visibleBounds.isEmpty())
        return { ViewportRelation::ClippedByAncestor, 0 };

    if (visibleBounds.intersects(viewport)) {
        float viewportCoverage = 0;
        if (!viewport.isEmpty())
            viewportCoverage = intersection(visibleBounds, viewport).area() / viewport.area();
        return { ViewportRelation::Intersecting, 0, viewportCoverage };
    }

    auto gapX = std::max({ 0.f, viewport.x() - visibleBounds.maxX(), visibleBounds.x() - viewport.maxX() });
    auto gapY = std::max({ 0.f, viewport.y() - visibleBounds.maxY(), visibleBounds.y() - viewport.maxY() });

    float viewportSizedDistance = 0;
    if (!viewport.isEmpty())
        viewportSizedDistance = std::hypot(gapX / viewport.width(), gapY / viewport.height());

    return { ViewportRelation::Offscreen, viewportSizedDistance };
}

struct AbsoluteTextBounds {
    IntRect unclipped;
    IntRect visible;
};

static AbsoluteTextBounds absoluteTextBounds(const SimpleRange& range)
{
    AbsoluteTextBounds bounds;
    for (Ref node : intersectingNodes(range)) {
        RefPtr text = dynamicDowncast<Text>(node.get());
        if (!text)
            continue;

        CheckedPtr renderer = text->renderer();
        if (!renderer)
            continue;

        auto offsets = characterDataOffsetRange(range, *text);
        auto nodeRange = SimpleRange {
            { *text, std::min(offsets.start, text->length()) },
            { *text, std::min(offsets.end, text->length()) }
        };

        auto rects = RenderObject::absoluteTextRects(nodeRange);
        if (rects.isEmpty())
            continue;

        auto clip = enclosingIntRect(visibleAbsoluteRect(*renderer));
        for (auto& rect : rects) {
            bounds.unclipped.unite(rect);
            bounds.visible.unite(intersection(rect, clip));
        }
    }
    return bounds;
}

static std::optional<FloatRect> ancestorFrameClipInRootView(const Document& document)
{
    std::optional<FloatRect> clip;
    for (RefPtr owner = document.ownerElement(); owner; owner = owner->document().ownerElement()) {
        CheckedPtr renderer = owner->renderer();
        RefPtr ownerView = owner->document().view();
        if (!renderer || !ownerView)
            return FloatRect { };

        auto frameRectInRootView = FloatRect { ownerView->contentsToRootView(enclosingIntRect(visibleAbsoluteRect(*renderer))) };
        if (clip)
            clip->intersect(frameRectInRootView);
        else
            clip = frameRectInRootView;
    }
    return clip;
}

std::optional<ViewportProximityInfo> viewportProximityInfoForRange(const SimpleRange& range)
{
    Ref document = range.startContainer().document();
    RefPtr view = document->view();
    if (!view)
        return std::nullopt;

    RefPtr rootView = dynamicDowncast<LocalFrameView>(view->root());
    auto viewport = visibleViewportRectInRootView(rootView ? *rootView : *view);
    if (!viewport)
        return std::nullopt;

    auto bounds = absoluteTextBounds(range);
    if (bounds.unclipped.isEmpty())
        return std::nullopt;

    auto visibleBounds = FloatRect { view->contentsToRootView(bounds.visible) };
    if (auto frameClip = ancestorFrameClipInRootView(document))
        visibleBounds.intersect(*frameClip);

    return viewportProximityInfo(visibleBounds, *viewport);
}

} // namespace WebCore
