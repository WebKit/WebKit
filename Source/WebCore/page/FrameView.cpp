/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "FrameView.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "ContainerNodeInlines.h"
#include "DocumentPage.h"
#include "FocusController.h"
#include "FrameInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderWidget.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameView);

int FrameView::headerHeight() const
{
    Ref frame = this->frame();
    if (!frame->isMainFrame())
        return 0;
    RefPtr page = frame->page();
    return page ? page->headerHeight() : 0;
}

int FrameView::footerHeight() const
{
    Ref frame = this->frame();
    if (!frame->isMainFrame())
        return 0;
    RefPtr page = frame->page();
    return page ? page->footerHeight() : 0;
}

FloatBoxExtent FrameView::obscuredContentInsets(InsetType type) const
{
    if (platformWidget() && type == InsetType::WebCoreOrPlatformInset)
        return platformContentInsets();

    Ref frame = this->frame();
    if (!frame->isMainFrame())
        return { };

    if (RefPtr page = frame->page())
        return page->obscuredContentInsets();

    return { };
}

CornerRadii FrameView::scrollbarAvoidanceCornerRadii() const
{
    Ref frame = this->frame();
    if (!frame->isMainFrame())
        return { };

    if (RefPtr page = frame->page())
        return page->chrome().client().scrollbarAvoidanceCornerRadii();

    return { };
}

float FrameView::visibleContentScaleFactor() const
{
    Ref frame = this->frame();
    if (!frame->isMainFrame())
        return 1;

    RefPtr page = frame->page();
    // FIXME: This !delegatesScaling() is confusing, and the opposite behavior to Frame::frameScaleFactor().
    // This function should probably be renamed to delegatedPageScaleFactor().
    if (!page || !page->delegatesScaling())
        return 1;

    return page->pageScaleFactor();
}

bool FrameView::isActive() const
{
    RefPtr page = frame().page();
    return page && page->focusController().isActive();
}

ScrollableArea* FrameView::enclosingScrollableArea() const
{
    Ref frame = this->frame();
    if (frame->isMainFrame())
        return nullptr;

    RefPtr ownerElement = frame->ownerElement();
    if (!ownerElement)
        return nullptr;

    auto* ownerRenderer = ownerElement->renderer();
    if (!ownerRenderer)
        return nullptr;

    CheckedPtr layer = ownerRenderer->enclosingLayer();
    if (!layer)
        return nullptr;

    CheckedPtr enclosingScrollableLayer = layer->enclosingScrollableLayer(IncludeSelfOrNot::IncludeSelf, CrossFrameBoundaries::No);
    if (!enclosingScrollableLayer)
        return nullptr;

    return enclosingScrollableLayer->scrollableArea();
}

void FrameView::invalidateRect(const IntRect& rect)
{
    Ref frame = this->frame();
    if (!parent()) {
        if (RefPtr page = frame->page())
            page->chrome().invalidateContentsAndRootView(rect);
        return;
    }

    CheckedPtr renderer = frame->ownerRenderer();
    if (!renderer)
        return;

    IntRect repaintRect = rect;
    repaintRect.moveBy(roundedIntPoint(renderer->contentBoxLocation()));
    renderer->repaintRectangle(repaintRect);
}

bool FrameView::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    RefPtr page = frame().page();
    return page && page->settings().scrollingPerformanceTestingEnabled();
}

IntRect FrameView::scrollableAreaBoundingBox(bool*) const
{
    RefPtr ownerRenderer = protectedFrame()->ownerRenderer();
    if (!ownerRenderer)
        return frameRect();

    return ownerRenderer->absoluteContentQuad().enclosingBoundingBox();
}

HostWindow* FrameView::hostWindow() const
{
    RefPtr page = frame().page();
    return page ? &page->chrome() : nullptr;
}

void FrameView::scrollbarStyleChanged(ScrollbarStyle newStyle, bool forceUpdate)
{
    Ref frame = this->frame();
    if (!frame->isMainFrame())
        return;

    if (RefPtr page = frame->page())
        page->chrome().client().recommendedScrollbarStyleDidChange(newStyle);

    ScrollView::scrollbarStyleChanged(newStyle, forceUpdate);
}

bool FrameView::scrollAnimatorEnabled() const
{
    if (auto* page = frame().page())
        return page->settings().scrollAnimatorEnabled();

    return false;
}

#if ENABLE(FORM_CONTROL_REFRESH)
bool FrameView::formControlRefreshEnabled() const
{
    if (RefPtr page = frame().page())
        return page->settings().formControlRefreshEnabled();

    return false;
}
#endif

// MARK: -

IntPoint FrameView::convertFromRendererToContainingView(const RenderElement* renderer, IntPoint rendererPoint) const
{
    auto point = roundedIntPoint(renderer->localToAbsolute(rendererPoint, UseTransforms));
    return contentsToView(point);
}

FloatPoint FrameView::convertFromRendererToContainingView(const RenderElement* renderer, FloatPoint rendererPoint) const
{
    auto point = renderer->localToAbsolute(rendererPoint, UseTransforms);
    return contentsToView(point);
}

IntRect FrameView::convertFromRendererToContainingView(const RenderElement* renderer, const IntRect& rendererRect) const
{
    auto rect = snappedIntRect(enclosingLayoutRect(renderer->localToAbsoluteQuad(FloatRect(rendererRect)).boundingBox()));
    return contentsToView(rect);
}

FloatRect FrameView::convertFromRendererToContainingView(const RenderElement* renderer, const FloatRect& rendererRect) const
{
    auto rect = renderer->localToAbsoluteQuad(FloatRect(rendererRect)).boundingBox();
    return contentsToView(rect);
}

// MARK: -

IntPoint FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, IntPoint viewPoint) const
{
    auto point = viewPoint;

    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrollingToNativeView())
        point = viewToContents(point);

    return roundedIntPoint(renderer->absoluteToLocal(point, UseTransforms));
}

FloatPoint FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, FloatPoint viewPoint) const
{
    auto point = viewPoint;

    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrollingToNativeView())
        point = viewToContents(point);

    return renderer->absoluteToLocal(point, UseTransforms);
}

DoublePoint FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, DoublePoint viewPoint) const
{
    // Convert from FrameView coords into page ("absolute") coordinates.
    if (!delegatesScrollingToNativeView())
        viewPoint = viewToContents(viewPoint);

    return renderer->absoluteToLocal(viewPoint, UseTransforms);
}

IntRect FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const IntRect& viewRect) const
{
    auto rect = viewToContents(viewRect);

    // FIXME: we don't have a way to map an absolute rect down to a local quad, so just
    // move the rect for now.
    rect.setLocation(roundedIntPoint(renderer->absoluteToLocal(rect.location(), UseTransforms)));
    return rect;
}

FloatRect FrameView::convertFromContainingViewToRenderer(const RenderElement* renderer, const FloatRect& viewRect) const
{
    auto rect = viewToContents(viewRect);

    return renderer->absoluteToLocalQuad(rect).boundingBox();
}

// MARK: -

FloatPoint FrameView::absoluteToLayoutViewportPoint(FloatPoint p) const
{
    Ref frame = this->frame();

    ASSERT(frame->settings().visualViewportEnabled());
    p.scale(1 / frame->frameScaleFactor());
    p.moveBy(-layoutViewportRect().location());
    return p;
}

FloatPoint FrameView::layoutViewportToAbsolutePoint(FloatPoint p) const
{
    Ref frame = this->frame();

    ASSERT(frame->settings().visualViewportEnabled());
    p.moveBy(layoutViewportRect().location());
    return p.scaled(frame->frameScaleFactor());
}

FloatRect FrameView::layoutViewportToAbsoluteRect(FloatRect rect) const
{
    Ref frame = this->frame();

    ASSERT(frame->settings().visualViewportEnabled());
    rect.moveBy(layoutViewportRect().location());
    rect.scale(frame->frameScaleFactor());
    return rect;
}

FloatRect FrameView::absoluteToLayoutViewportRect(FloatRect rect) const
{
    Ref frame = this->frame();

    ASSERT(frame->settings().visualViewportEnabled());
    rect.scale(1 / frame->frameScaleFactor());
    rect.moveBy(-layoutViewportRect().location());
    return rect;
}

// MARK: -

IntPoint FrameView::convertToContainingView(IntPoint localPoint) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return localPoint;

            auto point = localPoint;
            point.moveBy(roundedIntPoint(renderer->contentBoxLocation()));
            return parentView->convertFromRendererToContainingView(renderer, point);
        }
        return Widget::convertToContainingView(localPoint);
    }
    return localPoint;
}

FloatPoint FrameView::convertToContainingView(FloatPoint localPoint) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return localPoint;

            auto point = localPoint;
            point.moveBy(renderer->contentBoxLocation());
            return parentView->convertFromRendererToContainingView(renderer, point);
        }
        return Widget::convertToContainingView(localPoint);
    }
    return localPoint;
}

Ref<Frame> FrameView::protectedFrame() const
{
    return frame();
}

IntRect FrameView::convertToContainingView(const IntRect& localRect) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return localRect;

            auto rect = localRect;
            rect.moveBy(roundedIntPoint(renderer->contentBoxLocation()));
            return parentView->convertFromRendererToContainingView(renderer, rect);
        }
        return Widget::convertToContainingView(localRect);
    }
    return localRect;
}

FloatRect FrameView::convertToContainingView(const FloatRect& localRect) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return localRect;

            auto rect = localRect;
            rect.moveBy(renderer->contentBoxLocation());
            return parentView->convertFromRendererToContainingView(renderer, rect);
        }
        return Widget::convertToContainingView(localRect);
    }
    return localRect;
}

// MARK: -

IntPoint FrameView::convertFromContainingView(IntPoint parentPoint) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return parentPoint;

            auto point = parentView->convertFromContainingViewToRenderer(renderer, parentPoint);
            point.moveBy(-roundedIntPoint(renderer->contentBoxLocation()));
            return point;
        }
        return Widget::convertFromContainingView(parentPoint);
    }
    return parentPoint;
}

FloatPoint FrameView::convertFromContainingView(FloatPoint parentPoint) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return parentPoint;

            auto point = parentView->convertFromContainingViewToRenderer(renderer, parentPoint);
            point.moveBy(-renderer->contentBoxLocation());
            return point;
        }
        return Widget::convertFromContainingView(parentPoint);
    }
    return parentPoint;
}

DoublePoint FrameView::convertFromContainingView(DoublePoint parentPoint) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return parentPoint;

            auto point = parentView->convertFromContainingViewToRenderer(renderer.get(), parentPoint);
            auto contentBoxLocation = renderer->contentBoxLocation();
            point.move(-contentBoxLocation.x(), -contentBoxLocation.y());
            return point;
        }
        return Widget::convertFromContainingView(parentPoint);
    }
    return parentPoint;
}

IntRect FrameView::convertFromContainingView(const IntRect& parentRect) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return parentRect;

            auto rect = parentView->convertFromContainingViewToRenderer(renderer, parentRect);
            rect.moveBy(-roundedIntPoint(renderer->contentBoxLocation()));
            return rect;
        }
        return Widget::convertFromContainingView(parentRect);
    }
    return parentRect;
}

FloatRect FrameView::convertFromContainingView(const FloatRect& parentRect) const
{
    if (RefPtr parentScrollView = parent()) {
        if (RefPtr parentView = dynamicDowncast<FrameView>(*parentScrollView)) {
            // Get our renderer in the parent view
            RefPtr renderer = protectedFrame()->ownerRenderer();
            if (!renderer)
                return parentRect;

            auto rect = parentView->convertFromContainingViewToRenderer(renderer, parentRect);
            rect.moveBy(-renderer->contentBoxLocation());
            return rect;
        }
        return Widget::convertFromContainingView(parentRect);
    }
    return parentRect;
}

}
