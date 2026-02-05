/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFrameInspectorTargetProxy.h"

#include "InspectorTargetProxy.h"
#include "MessageSenderInlines.h"
#include "ProvisionalFrameProxy.h"
#include "WebFrameMessages.h"
#include "WebFrameProxy.h"
#include "WebProcessProxy.h"
#include <JavaScriptCore/InspectorTarget.h>
#include <memory>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebFrameInspectorTargetProxy);

std::unique_ptr<WebFrameInspectorTargetProxy> WebFrameInspectorTargetProxy::create(WebFrameProxy& frame, const String& targetId)
{
    return makeUnique<WebFrameInspectorTargetProxy>(frame, targetId);
}

std::unique_ptr<WebFrameInspectorTargetProxy> WebFrameInspectorTargetProxy::create(ProvisionalFrameProxy& provisionalFrame, const String& targetId)
{
    Ref frame { provisionalFrame.frame() };
    std::unique_ptr target = WebFrameInspectorTargetProxy::create(frame.get(), targetId);
    target->m_provisionalFrame = provisionalFrame;
    return target;
}

WebFrameInspectorTargetProxy::WebFrameInspectorTargetProxy(WebFrameProxy& frame, const String& targetId)
    : InspectorTargetProxy(targetId, Inspector::InspectorTargetType::Frame)
    , m_frame(frame)
{
}

WebFrameInspectorTargetProxy::~WebFrameInspectorTargetProxy() = default;

void WebFrameInspectorTargetProxy::connect(Inspector::FrontendChannel::ConnectionType connectionType)
{
    if (RefPtr provisionalFrame = m_provisionalFrame.get()) {
        protect(provisionalFrame->process())->send(Messages::WebFrame::ConnectInspector(connectionType), protect(provisionalFrame->frame())->frameID());
        return;
    }

    Ref frame = m_frame.get();
    protect(frame->process())->send(Messages::WebFrame::ConnectInspector(connectionType), frame->frameID());
}

void WebFrameInspectorTargetProxy::disconnect()
{
    if (isPaused())
        resume();

    if (RefPtr provisionalFrame = m_provisionalFrame.get()) {
        protect(provisionalFrame->process())->send(Messages::WebFrame::DisconnectInspector(), protect(provisionalFrame->frame())->frameID());
        return;
    }

    Ref frame = m_frame.get();
    protect(frame->process())->send(Messages::WebFrame::DisconnectInspector(), frame->frameID());
}

void WebFrameInspectorTargetProxy::sendMessageToTargetBackend(const String& message)
{
    if (RefPtr provisionalFrame = m_provisionalFrame.get()) {
        protect(provisionalFrame->process())->send(Messages::WebFrame::SendMessageToInspectorTarget(message), protect(provisionalFrame->frame())->frameID());
        return;
    }

    Ref frame = m_frame.get();
    protect(frame->process())->send(Messages::WebFrame::SendMessageToInspectorTarget(message), frame->frameID());
}

void WebFrameInspectorTargetProxy::didCommitProvisionalTarget()
{
    m_provisionalFrame = nullptr;
}

bool WebFrameInspectorTargetProxy::isProvisional() const
{
    return !!m_provisionalFrame;
}

} // namespace WebKit
