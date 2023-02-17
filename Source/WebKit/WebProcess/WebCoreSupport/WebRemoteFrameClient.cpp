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
#include "WebRemoteFrameClient.h"

#include <WebCore/FrameTree.h>
#include <WebCore/RemoteFrame.h>

namespace WebKit {

WebRemoteFrameClient::WebRemoteFrameClient(Ref<WebFrame>&& frame, ScopeExit<Function<void()>>&& frameInvalidator)
    : m_frame(WTFMove(frame))
    , m_frameInvalidator(WTFMove(frameInvalidator))
{
}

WebRemoteFrameClient::~WebRemoteFrameClient() = default;

void WebRemoteFrameClient::frameDetached()
{
    RefPtr coreFrame = m_frame->coreRemoteFrame();
    if (!coreFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (RefPtr parent = coreFrame->tree().parent()) {
        coreFrame->tree().detachFromParent();
        parent->tree().removeChild(*coreFrame);
    }
    m_frame->invalidate();
}

void WebRemoteFrameClient::sizeDidChange(WebCore::IntSize size)
{
    m_frame->updateRemoteFrameSize(size);
}

}
