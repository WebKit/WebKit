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
#include "RemotePageDrawingAreaProxy.h"

#include "DrawingAreaProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

RemotePageDrawingAreaProxy::RemotePageDrawingAreaProxy(DrawingAreaProxy& drawingArea, WebProcessProxy& process)
    : m_drawingArea(drawingArea)
    , m_identifier(drawingArea.identifier())
    , m_names(drawingArea.messageReceiverNames())
    , m_process(process)
{
    for (auto& name : m_names)
        process.addMessageReceiver(name, m_identifier, *this);
    drawingArea.addRemotePageDrawingAreaProxy(*this);
}

RemotePageDrawingAreaProxy::~RemotePageDrawingAreaProxy()
{
    for (auto& name : m_names)
        m_process->removeMessageReceiver(name, m_identifier);
    if (m_drawingArea)
        m_drawingArea->removeRemotePageDrawingAreaProxy(*this);
}

void RemotePageDrawingAreaProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (m_drawingArea)
        m_drawingArea->didReceiveMessage(connection, decoder);
}

bool RemotePageDrawingAreaProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (m_drawingArea)
        return m_drawingArea->didReceiveSyncMessage(connection, decoder, encoder);
    ASSERT_NOT_REACHED();
    return false;
}

}
