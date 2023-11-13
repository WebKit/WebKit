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

#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LocalDOMWindow.h"
#include "MessagePort.h"
#include "NavigationScheduler.h"
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

void RemoteDOMWindow::close(Document&)
{
    // FIXME: <rdar://117381050> Add security checks here equivalent to LocalDOMWindow::close (both with and without Document& parameter).
    // Or refactor to share code.
    if (m_frame && m_frame->isMainFrame())
        m_frame->client().close();
}

bool RemoteDOMWindow::closed() const
{
    // FIXME: This is probably not completely correct. <rdar://116203970>
    return !m_frame;
}

void RemoteDOMWindow::focus(LocalDOMWindow&)
{
    // FIXME: Implemented this. <rdar://116203970>
}

void RemoteDOMWindow::blur()
{
    // FIXME: Implemented this. <rdar://116203970>
}

unsigned RemoteDOMWindow::length() const
{
    if (!m_frame)
        return 0;

    return m_frame->tree().childCount();
}

WindowProxy* RemoteDOMWindow::top() const
{
    if (!m_frame)
        return nullptr;

    return &m_frame->tree().top().windowProxy();
}

WindowProxy* RemoteDOMWindow::opener() const
{
    if (!m_frame)
        return nullptr;

    RefPtr openerFrame = m_frame->opener();
    if (!openerFrame)
        return nullptr;

    return &openerFrame->windowProxy();
}

void RemoteDOMWindow::setOpener(WindowProxy*)
{
    // FIXME: <rdar://118263373> Implement.
    // JSLocalDOMWindow::setOpener has some security checks. Are they needed here?
}

WindowProxy* RemoteDOMWindow::parent() const
{
    if (!m_frame)
        return nullptr;

    RefPtr parent = m_frame->tree().parent();
    if (!parent)
        return nullptr;

    return &parent->windowProxy();
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

void RemoteDOMWindow::setLocation(LocalDOMWindow& activeWindow, const URL& completedURL, SetLocationLocking locking)
{
    // FIXME: Add some or all of the security checks in LocalDOMWindow::setLocation. <rdar://116500603>
    // FIXME: Refactor this duplicate code to share with LocalDOMWindow::setLocation. <rdar://116500603>

    RefPtr activeDocument = activeWindow.document();
    if (!activeDocument)
        return;

    // We want a new history item if we are processing a user gesture.
    LockHistory lockHistory = (locking != SetLocationLocking::LockHistoryBasedOnGestureState || !UserGestureIndicator::processingUserGesture()) ? LockHistory::Yes : LockHistory::No;
    LockBackForwardList lockBackForwardList = (locking != SetLocationLocking::LockHistoryBasedOnGestureState) ? LockBackForwardList::Yes : LockBackForwardList::No;
    frame()->navigationScheduler().scheduleLocationChange(*activeDocument, activeDocument->securityOrigin(),
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, activeDocument->frame()->loader().outgoingReferrer(),
        lockHistory, lockBackForwardList);
}

} // namespace WebCore
