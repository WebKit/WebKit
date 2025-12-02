/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ProvisionalFrameProxy.h"

#include "FrameProcess.h"
#include "ProvisionalFrameCreationParameters.h"
#include "VisitedLinkStore.h"
#include "WebFrameMessages.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProvisionalFrameProxy);

ProvisionalFrameProxy::ProvisionalFrameProxy(WebFrameProxy& frame, Ref<FrameProcess>&& frameProcess, CommitTiming commitTiming)
    : m_frame(frame)
    , m_frameProcess(WTFMove(frameProcess))
    , m_visitedLinkStore(frame.page()->visitedLinkStore())
{
    Ref process = this->process();
    process->markProcessAsRecentlyUsed();
    auto parameters = frame.provisionalFrameCreationParameters(std::nullopt, commitTiming);
    process->send(Messages::WebFrame::CreateProvisionalFrame(parameters), frame.frameID());
}

ProvisionalFrameProxy::~ProvisionalFrameProxy()
{
    if (!m_frameProcess)
        return;

    Ref frame = m_frame.get();
    Ref process = this->process();
    process->send(Messages::WebFrame::DestroyProvisionalFrame(), frame->frameID());
}

RefPtr<FrameProcess> ProvisionalFrameProxy::takeFrameProcess()
{
    ASSERT(m_frameProcess);
    return std::exchange(m_frameProcess, nullptr).releaseNonNull();
}

Ref<WebFrameProxy> ProvisionalFrameProxy::protectedFrame() const
{
    return m_frame.get();
}

WebProcessProxy& ProvisionalFrameProxy::process() const
{
    ASSERT(m_frameProcess);
    return m_frameProcess->process();
}

Ref<WebProcessProxy> ProvisionalFrameProxy::protectedProcess() const
{
    return process();
}

} // namespace WebKit
