/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "DrawingAreaProxyImpl.h"

#include "DrawingAreaMessages.h"
#include "Region.h"
#include "UpdateInfo.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

#ifndef __APPLE__
#error "This drawing area is not ready for use by other ports yet."
#endif

using namespace WebCore;

namespace WebKit {

PassOwnPtr<DrawingAreaProxyImpl> DrawingAreaProxyImpl::create(WebPageProxy* webPageProxy)
{
    return adoptPtr(new DrawingAreaProxyImpl(webPageProxy));
}

DrawingAreaProxyImpl::DrawingAreaProxyImpl(WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaInfo::Impl, webPageProxy)
    , m_isWaitingForDidSetSize(false)
{
}

DrawingAreaProxyImpl::~DrawingAreaProxyImpl()
{
}

void DrawingAreaProxyImpl::paint(BackingStore::PlatformGraphicsContext context, const IntRect& rect, Region& unpaintedRegion)
{
    unpaintedRegion = rect;

    if (!m_backingStore)
        return;

    m_backingStore->paint(context, rect);
    unpaintedRegion.subtract(IntRect(IntPoint(), m_backingStore->size()));
}

void DrawingAreaProxyImpl::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*)
{
    ASSERT_NOT_REACHED();
}

void DrawingAreaProxyImpl::didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*)
{
    ASSERT_NOT_REACHED();
}

bool DrawingAreaProxyImpl::paint(const WebCore::IntRect&, PlatformDrawingContext)
{
    ASSERT_NOT_REACHED();
    return false;
}

void DrawingAreaProxyImpl::sizeDidChange()
{
    sendSetSize();
}

void DrawingAreaProxyImpl::visibilityDidChange()
{
    if (!m_webPageProxy->isViewVisible()) {
        // Suspend painting.
        m_webPageProxy->process()->send(Messages::DrawingArea::SuspendPainting(), m_webPageProxy->pageID());
        return;
    }

    // Resume painting.
    m_webPageProxy->process()->send(Messages::DrawingArea::ResumePainting(), m_webPageProxy->pageID());
}

void DrawingAreaProxyImpl::setPageIsVisible(bool)
{
}

void DrawingAreaProxyImpl::attachCompositingContext(uint32_t contextID)
{
    ASSERT_NOT_REACHED();
}

void DrawingAreaProxyImpl::detachCompositingContext()
{
    ASSERT_NOT_REACHED();
}

void DrawingAreaProxyImpl::update(const UpdateInfo& updateInfo)
{
    // FIXME: Handle the case where the view is hidden.

    incorporateUpdate(updateInfo);
    m_webPageProxy->process()->send(Messages::DrawingArea::DidUpdate(), m_webPageProxy->pageID());
}

void DrawingAreaProxyImpl::didSetSize(const UpdateInfo& updateInfo)
{
    ASSERT(m_isWaitingForDidSetSize);
    m_isWaitingForDidSetSize = false;

    if (m_size != updateInfo.viewSize)
        sendSetSize();

    m_backingStore = nullptr;

    incorporateUpdate(updateInfo);
}

void DrawingAreaProxyImpl::incorporateUpdate(const UpdateInfo& updateInfo)
{
    if (updateInfo.updateRectBounds.isEmpty())
        return;

    if (!m_backingStore)
        m_backingStore = BackingStore::create(updateInfo.viewSize, m_webPageProxy);

    m_backingStore->incorporateUpdate(updateInfo);

    bool shouldScroll = !updateInfo.scrollRect.isEmpty();

    if (shouldScroll)
        m_webPageProxy->scrollView(updateInfo.scrollRect, updateInfo.scrollOffset);
    
    for (size_t i = 0; i < updateInfo.updateRects.size(); ++i)
        m_webPageProxy->setViewNeedsDisplay(updateInfo.updateRects[i]);

    if (shouldScroll)
        m_webPageProxy->displayView();
}

void DrawingAreaProxyImpl::sendSetSize()
{
    if (!m_webPageProxy->isValid())
        return;

    if (m_isWaitingForDidSetSize)
        return;

    m_isWaitingForDidSetSize = true;
    m_webPageProxy->process()->send(Messages::DrawingArea::SetSize(m_size), m_webPageProxy->pageID());
}

} // namespace WebKit
