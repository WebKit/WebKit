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
#include "WebFrameInspectorTarget.h"

#include "WebFrame.h"
#include "WebFrameInspectorTargetFrontendChannel.h"
#include <WebCore/FrameInspectorController.h>
#include <WebCore/InspectorController.h>
#include <WebCore/LocalFrame.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebFrameInspectorTarget);

WebFrameInspectorTarget::WebFrameInspectorTarget(WebFrame& frame)
    : m_frame(frame)
{
}

WebFrameInspectorTarget::~WebFrameInspectorTarget() = default;

Ref<WebFrame> WebFrameInspectorTarget::protectedFrame()
{
    return m_frame.get();
}

String WebFrameInspectorTarget::identifier() const
{
    return toTargetID(m_frame->frameID());
}

void WebFrameInspectorTarget::connect(Inspector::FrontendChannel::ConnectionType connectionType)
{
    if (m_channel)
        return;

    Ref frame = m_frame.get();
    m_channel = makeUnique<WebFrameInspectorTargetFrontendChannel>(frame, identifier(), connectionType);

    if (RefPtr coreFrame = frame->coreLocalFrame())
        coreFrame->protectedInspectorController()->connectFrontend(*m_channel);
}

void WebFrameInspectorTarget::disconnect()
{
    if (!m_channel)
        return;

    if (RefPtr coreFrame = protectedFrame()->coreLocalFrame())
        coreFrame->protectedInspectorController()->disconnectFrontend(*m_channel);

    m_channel.reset();
}

void WebFrameInspectorTarget::sendMessageToTargetBackend(const String& message)
{
    if (RefPtr coreFrame = protectedFrame()->coreLocalFrame())
        coreFrame->protectedInspectorController()->dispatchMessageFromFrontend(message);
}

String WebFrameInspectorTarget::toTargetID(WebCore::FrameIdentifier frameID)
{
    return makeString("frame-"_s, frameID.toUInt64());
}

} // namespace WebKit
