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
#include "FrameInspectorTargetProxy.h"

#include "FrameInspectorTarget.h"
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameInspectorTargetProxy);

FrameInspectorTargetProxy::FrameInspectorTargetProxy(WebCore::FrameIdentifier frameID, WebProcessProxy& process, bool isProvisional)
    : InspectorTargetProxy(FrameInspectorTarget::toTargetID(frameID, process.coreProcessIdentifier()), Inspector::InspectorTargetType::Frame)
    , m_targetProcess(process)
    , m_frameID(frameID)
    , m_isProvisional(isProvisional)
{
}

FrameInspectorTargetProxy::~FrameInspectorTargetProxy() = default;

void FrameInspectorTargetProxy::connect(Inspector::FrontendChannel::ConnectionType connectionType)
{
    protect(m_targetProcess.get())->send(Messages::WebFrame::ConnectInspector(connectionType), m_frameID);
}

void FrameInspectorTargetProxy::disconnect()
{
    if (isPaused())
        resume();

    protect(m_targetProcess.get())->send(Messages::WebFrame::DisconnectInspector(), m_frameID);
}

void FrameInspectorTargetProxy::sendMessageToTargetBackend(const String& message)
{
    protect(m_targetProcess.get())->send(Messages::WebFrame::SendMessageToInspectorTarget(message), m_frameID);
}

void FrameInspectorTargetProxy::didCommitProvisionalTarget()
{
    m_isProvisional = false;
}

bool FrameInspectorTargetProxy::isProvisional() const
{
    return m_isProvisional;
}

} // namespace WebKit
