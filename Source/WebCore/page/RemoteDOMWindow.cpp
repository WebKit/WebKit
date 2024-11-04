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
#include "Page.h"
#include "RemoteFrame.h"
#include "RemoteFrameClient.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RemoteDOMWindow);

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame, GlobalWindowIdentifier&& identifier)
    : DOMWindow(WTFMove(identifier), DOMWindowType::Remote)
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

void RemoteDOMWindow::closePage()
{
    if (!m_frame)
        return;
    m_frame->client().closePage();
}

void RemoteDOMWindow::frameDetached()
{
    m_frame = nullptr;
}

void RemoteDOMWindow::focus(LocalDOMWindow&)
{
    // FIXME(264713): Add security checks here equivalent to LocalDOMWindow::focus().
    if (m_frame && m_frame->isMainFrame())
        m_frame->client().focus();
}

void RemoteDOMWindow::blur()
{
    // FIXME(268121): Add security checks here equivalent to LocalDOMWindow::blur().
    if (m_frame && m_frame->isMainFrame())
        m_frame->client().unfocus();
}

unsigned RemoteDOMWindow::length() const
{
    if (!m_frame)
        return 0;

    return m_frame->tree().childCount();
}

ExceptionOr<void> RemoteDOMWindow::postMessage(JSC::JSGlobalObject& lexicalGlobalObject, LocalDOMWindow& incumbentWindow, JSC::JSValue message, WindowPostMessageOptions&& options)
{
    RefPtr sourceDocument = incumbentWindow.document();
    if (!sourceDocument)
        return { };

    RefPtr sourceFrame = incumbentWindow.frame();
    if (!sourceFrame)
        return { };

    auto targetSecurityOrigin = createTargetOriginForPostMessage(options.targetOrigin, *sourceDocument);
    if (targetSecurityOrigin.hasException())
        return targetSecurityOrigin.releaseException();

    std::optional<SecurityOriginData> target;
    if (auto origin = targetSecurityOrigin.releaseReturnValue())
        target = origin->data();

    Vector<Ref<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(lexicalGlobalObject, message, WTFMove(options.transfer), ports, SerializationForStorage::No, SerializationContext::WindowPostMessage);
    if (messageData.hasException())
        return messageData.releaseException();

    auto disentangledPorts = MessagePort::disentanglePorts(WTFMove(ports));
    if (disentangledPorts.hasException())
        return messageData.releaseException();

    // Capture the source of the message. We need to do this synchronously
    // in order to capture the source of the message correctly.
    auto sourceOrigin = sourceDocument->securityOrigin().toString();

    MessageWithMessagePorts messageWithPorts { messageData.releaseReturnValue(), disentangledPorts.releaseReturnValue() };
    if (auto* remoteFrame = frame())
        remoteFrame->client().postMessageToRemote(sourceFrame->frameID(), sourceOrigin, remoteFrame->frameID(), target, messageWithPorts);
    return { };
}

void RemoteDOMWindow::setLocation(LocalDOMWindow& activeWindow, const URL& completedURL, NavigationHistoryBehavior historyHandling, SetLocationLocking locking)
{
    // FIXME: Add some or all of the security checks in LocalDOMWindow::setLocation. <rdar://116500603>
    // FIXME: Refactor this duplicate code to share with LocalDOMWindow::setLocation. <rdar://116500603>

    RefPtr activeDocument = activeWindow.document();
    if (!activeDocument)
        return;

    RefPtr frame = this->frame();
    if (!activeDocument->canNavigate(frame.get(), completedURL))
        return;

    // We want a new history item if we are processing a user gesture.
    LockHistory lockHistory = (locking != SetLocationLocking::LockHistoryBasedOnGestureState || !UserGestureIndicator::processingUserGesture()) ? LockHistory::Yes : LockHistory::No;
    LockBackForwardList lockBackForwardList = (locking != SetLocationLocking::LockHistoryBasedOnGestureState) ? LockBackForwardList::Yes : LockBackForwardList::No;
    frame->protectedNavigationScheduler()->scheduleLocationChange(*activeDocument, activeDocument->securityOrigin(),
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, activeDocument->frame()->loader().outgoingReferrer(),
        lockHistory, lockBackForwardList,
        historyHandling);
}

} // namespace WebCore
