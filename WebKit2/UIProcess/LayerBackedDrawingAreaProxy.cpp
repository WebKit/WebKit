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

#if USE(ACCELERATED_COMPOSITING)

#include "LayerBackedDrawingAreaProxy.h"

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

PassOwnPtr<LayerBackedDrawingAreaProxy> LayerBackedDrawingAreaProxy::create(PlatformWebView* webView)
{
    return adoptPtr(new LayerBackedDrawingAreaProxy(webView));
}

LayerBackedDrawingAreaProxy::LayerBackedDrawingAreaProxy(PlatformWebView* webView)
    : DrawingAreaProxy(LayerBackedDrawingAreaType)
    , m_isWaitingForDidSetFrameNotification(false)
    , m_isVisible(true)
    , m_webView(webView)
{
}

LayerBackedDrawingAreaProxy::~LayerBackedDrawingAreaProxy()
{
}

void LayerBackedDrawingAreaProxy::paint(const IntRect& rect, PlatformDrawingContext context)
{
}

void LayerBackedDrawingAreaProxy::setSize(const IntSize& viewSize)
{
    WebPageProxy* page = this->page();
    if (!page->isValid())
        return;

    if (viewSize.isEmpty())
        return;

    m_viewSize = viewSize;
    m_lastSetViewSize = viewSize;

    platformSetSize();

    if (m_isWaitingForDidSetFrameNotification)
        return;
    m_isWaitingForDidSetFrameNotification = true;

    page->process()->responsivenessTimer()->start();
    page->process()->connection()->send(DrawingAreaMessage::SetSize, page->pageID(), CoreIPC::In(info().id, viewSize));
}

#if !PLATFORM(MAC)
void LayerBackedDrawingAreaProxy::platformSetSize()
{
}
#endif

void LayerBackedDrawingAreaProxy::setPageIsVisible(bool isVisible)
{
    WebPageProxy* page = this->page();

    if (isVisible == m_isVisible)
        return;
    
    m_isVisible = isVisible;
    if (!page->isValid())
        return;

    if (!m_isVisible) {
        // Tell the web process that it doesn't need to paint anything for now.
        page->process()->connection()->send(DrawingAreaMessage::SuspendPainting, page->pageID(), CoreIPC::In(info().id));
        return;
    }
    
    // The page is now visible.
    page->process()->connection()->send(DrawingAreaMessage::ResumePainting, page->pageID(), CoreIPC::In(info().id));
    
    // FIXME: We should request a full repaint here if needed.
}
    
void LayerBackedDrawingAreaProxy::didSetSize()
{
    m_isWaitingForDidSetFrameNotification = false;

    WebPageProxy* page = this->page();
    page->process()->responsivenessTimer()->stop();
}

void LayerBackedDrawingAreaProxy::update()
{
    WebPageProxy* page = this->page();
    page->process()->connection()->send(DrawingAreaMessage::DidUpdate, page->pageID(), CoreIPC::In(info().id));
}

void LayerBackedDrawingAreaProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<DrawingAreaProxyMessage::Kind>()) {
        case DrawingAreaProxyMessage::Update: {
            update();
            break;
        }
        case DrawingAreaProxyMessage::DidSetSize: {
            didSetSize();
            break;
        }
        default:
            ASSERT_NOT_REACHED();
    }
}

void LayerBackedDrawingAreaProxy::didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder*)
{
    switch (messageID.get<DrawingAreaProxyMessage::Kind>()) {
#if USE(ACCELERATED_COMPOSITING)
        case DrawingAreaProxyMessage::AttachCompositingContext: {
            uint32_t contextID;
            if (!arguments->decode(CoreIPC::Out(contextID)))
                return;
            attachCompositingContext(contextID);
            break;
        }
#endif
        default:
            ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)
