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

#include "config.h"
#include "LayerBackedDrawingAreaProxy.h"

#if USE(ACCELERATED_COMPOSITING)

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

PassOwnPtr<LayerBackedDrawingAreaProxy> LayerBackedDrawingAreaProxy::create(PlatformWebView* webView, WebPageProxy* webPageProxy)
{
    return adoptPtr(new LayerBackedDrawingAreaProxy(webView, webPageProxy));
}

LayerBackedDrawingAreaProxy::LayerBackedDrawingAreaProxy(PlatformWebView* webView, WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaInfo::LayerBacked, webPageProxy)
    , m_isWaitingForDidSetFrameNotification(false)
    , m_isVisible(true)
    , m_webView(webView)
{
}

LayerBackedDrawingAreaProxy::~LayerBackedDrawingAreaProxy()
{
}

#if !PLATFORM(WIN) && !PLATFORM(MAC)
bool LayerBackedDrawingAreaProxy::paint(const IntRect& rect, PlatformDrawingContext context)
{
    return true;
}
#endif

void LayerBackedDrawingAreaProxy::sizeDidChange()
{
    WebPageProxy* page = this->page();
    if (!page->isValid())
        return;

    if (m_size.isEmpty())
        return;

    m_lastSetViewSize = m_size;

    platformSetSize();

    if (m_isWaitingForDidSetFrameNotification)
        return;

    m_isWaitingForDidSetFrameNotification = true;

    page->process()->deprecatedSend(DrawingAreaLegacyMessage::SetSize, page->pageID(), CoreIPC::In(info().identifier, m_size));
}

#if !PLATFORM(MAC) && !PLATFORM(WIN)
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
        page->process()->deprecatedSend(DrawingAreaLegacyMessage::SuspendPainting, page->pageID(), CoreIPC::In(info().identifier));
        return;
    }
    
    // The page is now visible.
    page->process()->deprecatedSend(DrawingAreaLegacyMessage::ResumePainting, page->pageID(), CoreIPC::In(info().identifier));
    
    // FIXME: We should request a full repaint here if needed.
}
    
void LayerBackedDrawingAreaProxy::didSetSize(const IntSize& size)
{
    m_isWaitingForDidSetFrameNotification = false;

    if (size != m_lastSetViewSize)
        setSize(m_lastSetViewSize, IntSize());
}

void LayerBackedDrawingAreaProxy::update()
{
    WebPageProxy* page = this->page();
    page->process()->deprecatedSend(DrawingAreaLegacyMessage::DidUpdate, page->pageID(), CoreIPC::In(info().identifier));
}

void LayerBackedDrawingAreaProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<DrawingAreaProxyLegacyMessage::Kind>()) {
        case DrawingAreaProxyLegacyMessage::Update: {
            update();
            break;
        }
        case DrawingAreaProxyLegacyMessage::DidSetSize: {
            IntSize size;
            if (!arguments->decode(CoreIPC::Out(size)))
                return;
            didSetSize(size);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
    }
}

void LayerBackedDrawingAreaProxy::didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder*)
{
    switch (messageID.get<DrawingAreaProxyLegacyMessage::Kind>()) {
#if USE(ACCELERATED_COMPOSITING)
        case DrawingAreaProxyLegacyMessage::AttachCompositingContext: {
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
