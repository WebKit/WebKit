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
#include "FrameInspectorTarget.h"

#include "UIProcessForwardingFrontendChannel.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/FrameInspectorController.h>
#include <WebCore/LocalFrame.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameInspectorTarget);

FrameInspectorTarget::FrameInspectorTarget(WebFrame& frame)
    : m_frame(frame)
{
}

FrameInspectorTarget::~FrameInspectorTarget() = default;

String FrameInspectorTarget::identifier() const
{
    return toTargetID(protect(m_frame)->frameID(), WebCore::Process::identifier());
}

void FrameInspectorTarget::connect(Inspector::FrontendChannel::ConnectionType connectionType)
{
    if (m_channel)
        return;

    Ref frame = m_frame.get();
    RefPtr page = frame->page();
    ASSERT(page);
    m_channel = makeUnique<UIProcessForwardingFrontendChannel>(*page, identifier(), connectionType);

    RefPtr coreFrame = frame->provisionalFrame() ?: frame->coreLocalFrame();
    if (coreFrame)
        protect(coreFrame->inspectorController())->connectFrontend(*m_channel);
}

void FrameInspectorTarget::disconnect()
{
    if (!m_channel)
        return;

    Ref frame = m_frame.get();
    RefPtr coreFrame = frame->provisionalFrame() ?: frame->coreLocalFrame();
    if (coreFrame)
        protect(coreFrame->inspectorController())->disconnectFrontend(*m_channel);

    m_channel.reset();
}

void FrameInspectorTarget::sendMessageToTargetBackend(const String& message)
{
    Ref frame = m_frame.get();
    RefPtr coreFrame = frame->provisionalFrame() ?: frame->coreLocalFrame();
    if (coreFrame)
        protect(coreFrame->inspectorController())->dispatchMessageFromFrontend(message);
}

String FrameInspectorTarget::toTargetID(WebCore::FrameIdentifier frameID, WebCore::ProcessIdentifier processID)
{
    return makeString("frame-"_s, frameID.toUInt64(), '-', processID.toUInt64());
}

} // namespace WebKit
