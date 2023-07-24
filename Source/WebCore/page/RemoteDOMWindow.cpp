/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "RemoteDOMWindow.h"

#include "LocalDOMWindow.h"
#include "MessagePort.h"
#include "RemoteFrame.h"
#include "RemoteFrameClient.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RemoteDOMWindow);

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame, GlobalWindowIdentifier&& identifier)
    : DOMWindow(WTFMove(identifier))
    , m_frame(frame)
{
}

RemoteDOMWindow::~RemoteDOMWindow() = default;

WindowProxy* RemoteDOMWindow::self() const
{
    if (!m_frame)
        return nullptr;
    return &m_frame->windowProxy();
}

Location* RemoteDOMWindow::location() const
{
    // FIXME: Implemented this.
    return nullptr;
}

void RemoteDOMWindow::close(Document&)
{
    // FIXME: Implemented this.
}

bool RemoteDOMWindow::closed() const
{
    return !m_frame;
}

void RemoteDOMWindow::focus(LocalDOMWindow& incumbentWindow)
{
    UNUSED_PARAM(incumbentWindow);
    // FIXME: Implemented this.
}

void RemoteDOMWindow::blur()
{
    // FIXME: Implemented this.
}

unsigned RemoteDOMWindow::length() const
{
    // FIXME: Implemented this.
    return 0;
}

WindowProxy* RemoteDOMWindow::top() const
{
    if (!m_frame)
        return nullptr;

    // FIXME: Implemented this.
    return &m_frame->windowProxy();
}

WindowProxy* RemoteDOMWindow::opener() const
{
    if (!m_frame)
        return nullptr;

    auto* openerFrame = m_frame->opener();
    if (!openerFrame)
        return nullptr;

    return &openerFrame->windowProxy();
}

WindowProxy* RemoteDOMWindow::parent() const
{
    if (!m_frame)
        return nullptr;

    // FIXME: Implemented this.
    return &m_frame->windowProxy();
}

ExceptionOr<void> RemoteDOMWindow::postMessage(JSC::JSGlobalObject& lexicalGlobalObject, LocalDOMWindow& incumbentWindow, JSC::JSValue message, WindowPostMessageOptions&& options)
{
    RefPtr sourceDocument = incumbentWindow.document();
    if (!sourceDocument)
        return { };

    auto targetSecurityOrigin = createTargetOriginForPostMessage(options.targetOrigin, *sourceDocument);
    if (targetSecurityOrigin.hasException())
        return targetSecurityOrigin.releaseException();

    std::optional<SecurityOriginData> target;
    if (auto origin = targetSecurityOrigin.releaseReturnValue())
        target = origin->data();

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(lexicalGlobalObject, message, WTFMove(options.transfer), ports, SerializationForStorage::No, SerializationContext::WindowPostMessage);
    if (messageData.hasException())
        return messageData.releaseException();

    auto disentangledPorts = MessagePort::disentanglePorts(WTFMove(ports));
    if (disentangledPorts.hasException())
        return messageData.releaseException();

    MessageWithMessagePorts messageWithPorts { messageData.releaseReturnValue(), disentangledPorts.releaseReturnValue() };
    if (auto* remoteFrame = frame())
        remoteFrame->client().postMessageToRemote(remoteFrame->frameID(), target, messageWithPorts);
    return { };
}

} // namespace WebCore
