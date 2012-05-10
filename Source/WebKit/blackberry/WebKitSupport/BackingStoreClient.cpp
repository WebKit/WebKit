/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "BackingStoreClient.h"

#include "BackingStore.h"
#include "BackingStore_p.h"
#include "FloatPoint.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "RenderBox.h"
#include "WebPage_p.h"

// FIXME: Leaving the below lines commented out as a reference for us to soon be sure if we need these
// methods and class variables be moved from WebPage to BackingStoreClient.
// Notification methods that deliver changes to the real geometry of the device as specified above.
// void notifyTransformChanged();
// void notifyTransformedContentsSizeChanged();
// void notifyTransformedScrollChanged();
// m_overflowExceedsContentsSize = true;
// haspendingscrollevent

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

static inline IntSize pointToSize(const IntPoint& point)
{
    return IntSize(point.x(), point.y());
}

BackingStoreClient* BackingStoreClient::create(Frame* frame, Frame* parentFrame, WebPage* parentPage)
{
    ASSERT(parentPage);
    ASSERT(frame->view());

    // FIXME: We do not support inner frames for now.
    if (parentFrame)
        return 0;

    BackingStoreClient* parentBackingStoreClient
        = parentFrame
        ? parentPage->d->backingStoreClientForFrame(parentFrame)
        : 0;

    // If this frame has a parent with no backingstore then just stop since
    // our frame heirarchy is done.
    if (parentFrame && !parentBackingStoreClient)
        return 0;

    BackingStoreClient* it = new BackingStoreClient(frame, parentFrame, parentPage);
    ASSERT(it);

    // Frame -> BackingStoreClient mapping is controlled by the Page.
    parentPage->d->addBackingStoreClientForFrame(frame, it);

    // Add the backing store client to the child list of its parent.
    if (parentBackingStoreClient)
        parentBackingStoreClient->addChild(it);

    return it;
}

BackingStoreClient::BackingStoreClient(Frame* frame, Frame* parentFrame, WebPage* parentPage)
    : m_frame(frame)
    , m_webPage(parentPage)
    , m_backingStore(0)
    , m_parent(0)
    , m_isClientGeneratedScroll(false)
    , m_isScrollNotificationSuppressed(false)
{
    UNUSED_PARAM(parentFrame);
    m_backingStore = new BackingStore(m_webPage, this);
}

BackingStoreClient::~BackingStoreClient()
{
    m_webPage->d->removeBackingStoreClientForFrame(m_frame);

    delete m_backingStore;
    m_backingStore = 0;
    m_frame = 0;
}

void BackingStoreClient::addChild(BackingStoreClient* child)
{
    ASSERT(child);
    child->m_parent = this;
}

WTF::Vector <BackingStoreClient*> BackingStoreClient::children() const
{
    WTF::Vector<BackingStoreClient*> children;
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        BlackBerry::WebKit::BackingStoreClient* client =
            m_webPage->d->backingStoreClientForFrame(child);

        if (client)
            children.append(client);
    }

    return children;
}

IntRect BackingStoreClient::absoluteRect() const
{
    IntRect rect = IntRect(IntPoint::zero(), viewportSize());

    if (!isMainFrame()) {
        // It is possible that the owner HTML element has been removed at this point,
        // especially when the frame is loading a JavaScript URL.
        if (Element* elt = m_frame->ownerElement()) {
            if (RenderBox* obj = elt->renderBox())
                rect.move(obj->borderLeft() + obj->paddingLeft(), obj->borderTop() + obj->paddingTop());
        }
    }

    Frame* frame = m_frame;
    while (frame) {
        if (Element* element = static_cast<Element*>(frame->ownerElement())) {
            do {
                rect.move(element->offsetLeft(), element->offsetTop());
            } while ((element = element->offsetParent()));
        }

        if ((frame = frame->tree()->parent()))
            rect.move((-frame->view()->scrollOffset()));
    }

    return rect;
}

IntRect BackingStoreClient::transformedAbsoluteRect() const
{
    return m_webPage->d->mapToTransformed(absoluteRect());
}

IntPoint BackingStoreClient::absoluteLocation() const
{
    return absoluteRect().location();
}

IntPoint BackingStoreClient::transformedAbsoluteLocation() const
{
    return m_webPage->d->mapToTransformed(transformedAbsoluteRect()).location();
}

IntPoint BackingStoreClient::scrollPosition() const
{
    ASSERT(m_frame);
    if (!m_frame->view())
        return IntPoint();

    return m_frame->view()->scrollPosition() - pointToSize(m_frame->view()->minimumScrollPosition());
}

IntPoint BackingStoreClient::transformedScrollPosition() const
{
    return m_webPage->d->mapToTransformed(scrollPosition());
}

void BackingStoreClient::setScrollPosition(const IntPoint& pos)
{
    ASSERT(m_frame);
    if (!m_frame->view())
        return;

    if (pos == scrollPosition())
        return;

    // We set a flag here to note that this scroll operation was originated
    // within the BlackBerry-specific layer of WebKit and not by WebCore.
    // This flag is checked in checkOriginOfCurrentScrollOperation() to decide
    // whether to notify the client of the current scroll operation. This is
    // why it is important that all scroll operations that originate within
    // BlackBerry-specific code are encapsulated here and that callers of this
    // method also directly or indirectly call notifyTransformedScrollChanged().
    m_isScrollNotificationSuppressed = true;
    m_frame->view()->setScrollPosition(pos + pointToSize(m_frame->view()->minimumScrollPosition()));
    m_isScrollNotificationSuppressed = false;
}

IntPoint BackingStoreClient::maximumScrollPosition() const
{
    ASSERT(m_frame);
    if (!m_frame->view())
        return IntPoint();

    return m_frame->view()->maximumScrollPosition() - pointToSize(m_frame->view()->minimumScrollPosition());
}

IntPoint BackingStoreClient::transformedMaximumScrollPosition() const
{
    return m_webPage->d->mapToTransformed(maximumScrollPosition());
}

IntSize BackingStoreClient::actualVisibleSize() const
{
    return m_webPage->d->mapFromTransformed(transformedActualVisibleSize());
}

IntSize BackingStoreClient::transformedActualVisibleSize() const
{
    if (isMainFrame())
        return m_webPage->d->transformedActualVisibleSize();

    return transformedViewportSize();
}

IntSize BackingStoreClient::viewportSize() const
{
    ASSERT(m_frame);
    if (!m_frame->view())
        return IntSize();

    if (isMainFrame())
        return m_webPage->d->viewportSize();

    return m_frame->view()->visibleContentRect().size();
}

IntSize BackingStoreClient::transformedViewportSize() const
{
    ASSERT(m_frame);
    if (!m_frame->view())
        return IntSize();

    if (isMainFrame())
        return m_webPage->d->transformedViewportSize();

    const IntSize untransformedViewportSize = m_frame->view()->visibleContentRect().size();
    const FloatPoint transformedBottomRight = m_webPage->d->m_transformationMatrix->mapPoint(
        FloatPoint(untransformedViewportSize.width(), untransformedViewportSize.height()));
    return IntSize(floorf(transformedBottomRight.x()), floorf(transformedBottomRight.y()));
}

IntRect BackingStoreClient::visibleContentsRect() const
{
    ASSERT(m_frame);
    if (!m_frame->view())
        return IntRect();

    IntRect visibleContentRect = m_frame->view()->visibleContentRect();
    if (isMainFrame())
        return visibleContentRect;

    IntPoint offset = absoluteLocation();
    visibleContentRect.move(offset.x(), offset.y());
    if (m_parent)
        visibleContentRect.intersect(m_parent->visibleContentsRect());

    return visibleContentRect;
}

IntRect BackingStoreClient::transformedVisibleContentsRect() const
{
    // Usually this would be mapToTransformed(visibleContentsRect()), but
    // that results in rounding errors because we already set the WebCore
    // viewport size from our original transformedViewportSize().
    // Instead, we only transform the scroll position and take the
    // viewport size as it is, which ensures that e.g. blitting operations
    // always cover the whole widget/screen.
    IntRect visibleContentsRect = IntRect(transformedScrollPosition(), transformedViewportSize());
    if (isMainFrame())
        return visibleContentsRect;

    IntPoint offset = transformedAbsoluteLocation();
    visibleContentsRect.move(offset.x(), offset.y());
    return visibleContentsRect;
}

IntSize BackingStoreClient::contentsSize() const
{
    ASSERT(m_frame);
    if (!m_frame->view())
        return IntSize();

    return m_frame->view()->contentsSize();
}

IntSize BackingStoreClient::transformedContentsSize() const
{
    // mapToTransformed() functions use this method to crop their results,
    // so we can't make use of them here. While we want rounding inside page
    // boundaries to extend rectangles and round points, we need to crop the
    // contents size to the floored values so that we don't try to display
    // or report points that are not fully covered by the actual float-point
    // contents rectangle.
    const IntSize untransformedContentsSize = contentsSize();
    const FloatPoint transformedBottomRight = m_webPage->d->m_transformationMatrix->mapPoint(
        FloatPoint(untransformedContentsSize.width(), untransformedContentsSize.height()));
    return IntSize(floorf(transformedBottomRight.x()), floorf(transformedBottomRight.y()));
}

void BackingStoreClient::clipToTransformedContentsRect(IntRect& rect) const
{
    // FIXME: Needs to proper translate coordinates here?
    rect.intersect(IntRect(IntPoint::zero(), transformedContentsSize()));
}

IntPoint BackingStoreClient::mapFromContentsToViewport(const IntPoint& point) const
{
    const IntPoint scrollPosition = this->scrollPosition();
    return IntPoint(point.x() - scrollPosition.x(), point.y() - scrollPosition.y());
}

IntPoint BackingStoreClient::mapFromViewportToContents(const IntPoint& point) const
{
    const IntPoint scrollPosition = this->scrollPosition();
    return IntPoint(point.x() + scrollPosition.x(), point.y() + scrollPosition.y());
}

IntRect BackingStoreClient::mapFromContentsToViewport(const IntRect& rect) const
{
    return IntRect(mapFromContentsToViewport(rect.location()), rect.size());
}

IntRect BackingStoreClient::mapFromViewportToContents(const IntRect& rect) const
{
    return IntRect(mapFromViewportToContents(rect.location()), rect.size());
}

IntPoint BackingStoreClient::mapFromTransformedContentsToTransformedViewport(const IntPoint& point) const
{
    const IntPoint scrollPosition = transformedScrollPosition();
    return IntPoint(point.x() - scrollPosition.x(), point.y() - scrollPosition.y());
}

IntPoint BackingStoreClient::mapFromTransformedViewportToTransformedContents(const IntPoint& point) const
{
    const IntPoint scrollPosition = transformedScrollPosition();
    return IntPoint(point.x() + scrollPosition.x(), point.y() + scrollPosition.y());
}

IntRect BackingStoreClient::mapFromTransformedContentsToTransformedViewport(const IntRect& rect) const
{
    return IntRect(mapFromTransformedContentsToTransformedViewport(rect.location()), rect.size());
}

IntRect BackingStoreClient::mapFromTransformedViewportToTransformedContents(const IntRect& rect) const
{
    return IntRect(mapFromTransformedViewportToTransformedContents(rect.location()), rect.size());
}

WebPagePrivate::LoadState BackingStoreClient::loadState() const
{
    // FIXME: Does it need to call WebPage's?
    return m_webPage->d->loadState();
}

bool BackingStoreClient::isLoading() const
{
    // FIXME: Does it need to call WebPage's?
    return m_webPage->d->isLoading();
}

bool BackingStoreClient::isFocused() const
{
    return m_frame && m_frame->page() && m_frame->page()->focusController()
        && m_frame->page()->focusController()->focusedFrame() == m_frame;
}

bool BackingStoreClient::scrollsHorizontally() const
{
    return transformedActualVisibleSize().width() < transformedContentsSize().width();
}

bool BackingStoreClient::scrollsVertically() const
{
    return transformedActualVisibleSize().height() < transformedContentsSize().height();
}

bool BackingStoreClient::isClientGeneratedScroll() const
{
    return m_isClientGeneratedScroll;
}

void BackingStoreClient::setIsClientGeneratedScroll(bool flag)
{
    m_isClientGeneratedScroll = flag;
}

bool BackingStoreClient::isScrollNotificationSuppressed() const
{
    return m_isScrollNotificationSuppressed;
}

void BackingStoreClient::setIsScrollNotificationSuppressed(bool flag)
{
    m_isScrollNotificationSuppressed = flag;
}

void BackingStoreClient::checkOriginOfCurrentScrollOperation()
{
    // This is called via ChromeClientBlackBerry::scroll in order to check the origin
    // of the current scroll operation to decide whether to notify the client.
    // If the current scroll operation was initiated internally by WebCore itself
    // either via JavaScript, back/forward or otherwise then we need to go ahead
    // and notify the client of this change.
    if (isScrollNotificationSuppressed())
        return;

    if (isMainFrame())
        m_webPage->d->notifyTransformedScrollChanged();
    else
        m_backingStore->d->scrollChanged(transformedScrollPosition());
}

}
}
