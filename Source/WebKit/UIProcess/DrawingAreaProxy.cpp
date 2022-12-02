/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include "DrawingAreaProxy.h"

#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#endif

namespace WebKit {
using namespace WebCore;

DrawingAreaProxy::DrawingAreaProxy(DrawingAreaType type, WebPageProxy& webPageProxy, WebProcessProxy& process)
    : m_type(type)
    , m_identifier(DrawingAreaIdentifier::generate())
    , m_webPageProxy(webPageProxy)
    , m_process(process)
    , m_size(webPageProxy.viewSize())
#if PLATFORM(MAC)
    , m_viewExposedRectChangedTimer(RunLoop::main(), this, &DrawingAreaProxy::viewExposedRectChangedTimerFired)
#endif
{
}

DrawingAreaProxy::~DrawingAreaProxy()
{
    if (m_startedReceivingMessages)
        process().removeMessageReceiver(Messages::DrawingAreaProxy::messageReceiverName(), m_identifier);
}

void DrawingAreaProxy::startReceivingMessages()
{
    m_startedReceivingMessages = true;
    process().addMessageReceiver(Messages::DrawingAreaProxy::messageReceiverName(), m_identifier, *this);
}

DelegatedScrollingMode DrawingAreaProxy::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::NotDelegated;
}

bool DrawingAreaProxy::setSize(const IntSize& size, const IntSize& scrollDelta)
{ 
    if (m_size == size && scrollDelta.isZero())
        return false;

    m_size = size;
    m_scrollOffset += scrollDelta;
    sizeDidChange();
    return true;
}

#if PLATFORM(COCOA)
MachSendRight DrawingAreaProxy::createFence()
{
    return MachSendRight();
}
#endif

IPC::Connection* DrawingAreaProxy::messageSenderConnection() const
{
    return process().connection();
}

bool DrawingAreaProxy::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions)
{
    return process().sendMessage(WTFMove(encoder), sendOptions);
}

bool DrawingAreaProxy::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return m_process->sendMessage(WTFMove(encoder), sendOptions, WTFMove(handler));
}

#if PLATFORM(MAC)
void DrawingAreaProxy::didChangeViewExposedRect()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    if (!m_viewExposedRectChangedTimer.isActive())
        m_viewExposedRectChangedTimer.startOneShot(0_s);
}

void DrawingAreaProxy::viewExposedRectChangedTimerFired()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    auto viewExposedRect = m_webPageProxy.viewExposedRect();
    if (viewExposedRect == m_lastSentViewExposedRect)
        return;

    send(Messages::DrawingArea::SetViewExposedRect(viewExposedRect));
    m_lastSentViewExposedRect = viewExposedRect;
}
#endif // PLATFORM(MAC)

} // namespace WebKit
