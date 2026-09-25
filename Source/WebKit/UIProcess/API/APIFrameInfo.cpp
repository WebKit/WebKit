/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "APIFrameInfo.h"

#include "APIFrameHandle.h"
#include "FrameInfoData.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include <utility>

namespace API {

Ref<FrameInfo> FrameInfo::create(WebKit::FrameInfoData&& frameInfoData)
{
    return adoptRef(*new FrameInfo(WTF::move(frameInfoData)));
}

auto FrameInfo::stateSnapshot(WebCore::FrameIdentifier frameID) -> StateSnapshot
{
    RefPtr frame = WebKit::WebFrameProxy::webFrame(frameID);
    if (!frame)
        return { };
    RefPtr parent = frame->parentFrame();
    return StateSnapshot {
        frame->certificateInfo(),
        parent ? std::optional(parent->frameID()) : std::nullopt,
        frame->title()
    };
}

FrameInfo::FrameInfo(WebKit::FrameInfoData&& data)
    : m_data(WTF::move(data))
    , m_stateSnapshot(stateSnapshot(m_data.frameID)) { }

FrameInfo::~FrameInfo() = default;

Ref<FrameHandle> FrameInfo::handle() const
{
    return FrameHandle::create(m_data.frameID);
}

RefPtr<FrameHandle> FrameInfo::parentFrameHandle() const
{
    if (!m_stateSnapshot.parentFrameID)
        return nullptr;
    return FrameHandle::create(*m_stateSnapshot.parentFrameID);
}

const WebKit::WebPageProxy* FrameInfo::page() const
{
    return WebKit::WebPageProxy::fromIdentifier(m_data.webPageProxyID);
}

WebKit::WebPageProxy* FrameInfo::page()
{
    return WebKit::WebPageProxy::fromIdentifier(m_data.webPageProxyID);
}

} // namespace API
