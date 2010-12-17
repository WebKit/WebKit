/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "ChunkedUpdateDrawingAreaProxy.h"

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "UpdateChunk.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

PassOwnPtr<ChunkedUpdateDrawingAreaProxy> ChunkedUpdateDrawingAreaProxy::create(PlatformWebView* webView, WebPageProxy* webPageProxy)
{
    return adoptPtr(new ChunkedUpdateDrawingAreaProxy(webView, webPageProxy));
}

ChunkedUpdateDrawingAreaProxy::ChunkedUpdateDrawingAreaProxy(PlatformWebView* webView, WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaInfo::ChunkedUpdate, webPageProxy)
    , m_isWaitingForDidSetFrameNotification(false)
    , m_isVisible(true)
    , m_forceRepaintWhenResumingPainting(false)
    , m_webView(webView)
{
}

ChunkedUpdateDrawingAreaProxy::~ChunkedUpdateDrawingAreaProxy()
{
}

void ChunkedUpdateDrawingAreaProxy::paint(const IntRect& rect, PlatformDrawingContext context)
{
    if (m_isWaitingForDidSetFrameNotification) {
        WebPageProxy* page = this->page();
        if (!page->isValid())
            return;
        
        if (page->process()->isLaunching())
            return;

        OwnPtr<CoreIPC::ArgumentDecoder> arguments = page->process()->connection()->waitFor(DrawingAreaProxyMessage::DidSetSize, page->pageID(), 0.04);
        if (arguments)
            didReceiveMessage(page->process()->connection(), CoreIPC::MessageID(DrawingAreaProxyMessage::DidSetSize), arguments.get());
    }

    platformPaint(rect, context);
}

void ChunkedUpdateDrawingAreaProxy::sizeDidChange()
{
    WebPageProxy* page = this->page();
    if (!page->isValid())
        return;

    if (m_size.isEmpty())
        return;

    m_lastSetViewSize = m_size;

    if (m_isWaitingForDidSetFrameNotification)
        return;
    m_isWaitingForDidSetFrameNotification = true;

    page->process()->responsivenessTimer()->start();
    page->process()->send(DrawingAreaMessage::SetSize, page->pageID(), CoreIPC::In(info().identifier, m_size));
}

void ChunkedUpdateDrawingAreaProxy::setPageIsVisible(bool isVisible)
{
    WebPageProxy* page = this->page();

    if (isVisible == m_isVisible)
        return;
    
    m_isVisible = isVisible;
    if (!page->isValid())
        return;

    if (!m_isVisible) {
        // Tell the web process that it doesn't need to paint anything for now.
        page->process()->send(DrawingAreaMessage::SuspendPainting, page->pageID(), CoreIPC::In(info().identifier));
        return;
    }
    
    // The page is now visible, resume painting.
    page->process()->send(DrawingAreaMessage::ResumePainting, page->pageID(), CoreIPC::In(info().identifier, m_forceRepaintWhenResumingPainting));
    m_forceRepaintWhenResumingPainting = false;
}
    
void ChunkedUpdateDrawingAreaProxy::didSetSize(UpdateChunk* updateChunk)
{
    ASSERT(m_isWaitingForDidSetFrameNotification);
    m_isWaitingForDidSetFrameNotification = false;

    IntSize viewSize = updateChunk->rect().size();

    if (viewSize != m_lastSetViewSize)
        setSize(m_lastSetViewSize);

    invalidateBackingStore();
    if (!updateChunk->isEmpty())
        drawUpdateChunkIntoBackingStore(updateChunk);

    WebPageProxy* page = this->page();
    page->process()->responsivenessTimer()->stop();
}

void ChunkedUpdateDrawingAreaProxy::update(UpdateChunk* updateChunk)
{
    if (!m_isVisible) {
        // We got an update request that must have been sent before we told the web process to suspend painting.
        // Don't paint this into the backing store, because that could leave the backing store in an inconsistent state.
        // Instead, we will just tell the drawing area to repaint everything when we resume painting.
        m_forceRepaintWhenResumingPainting = true;
    } else {
        // Just paint into backing store.
        drawUpdateChunkIntoBackingStore(updateChunk);
    }

    WebPageProxy* page = this->page();
    page->process()->send(DrawingAreaMessage::DidUpdate, page->pageID(), CoreIPC::In(info().identifier));
}

void ChunkedUpdateDrawingAreaProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<DrawingAreaProxyMessage::Kind>()) {
        case DrawingAreaProxyMessage::Update: {
            UpdateChunk updateChunk;
            if (!arguments->decode(updateChunk))
                return;

            update(&updateChunk);
            break;
        }
        case DrawingAreaProxyMessage::DidSetSize: {
            UpdateChunk updateChunk;
            if (!arguments->decode(CoreIPC::Out(updateChunk)))
                return;

            didSetSize(&updateChunk);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit
