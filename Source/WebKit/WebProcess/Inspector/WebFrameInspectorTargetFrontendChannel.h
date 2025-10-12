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

#pragma once

#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebFrame;

class WebFrameInspectorTargetFrontendChannel final : public Inspector::FrontendChannel {
    WTF_MAKE_TZONE_ALLOCATED(WebFrameInspectorTargetFrontendChannel);
    WTF_MAKE_NONCOPYABLE(WebFrameInspectorTargetFrontendChannel);
public:
    WebFrameInspectorTargetFrontendChannel(WebFrame&, const String& targetId, Inspector::FrontendChannel::ConnectionType);
    virtual ~WebFrameInspectorTargetFrontendChannel() = default;

private:
    Ref<WebFrame> protectedFrame();

    ConnectionType connectionType() const override { return m_connectionType; }
    void sendMessageToFrontend(const String& message) override;

    WeakRef<WebFrame> m_frame;
    String m_targetId;
    Inspector::FrontendChannel::ConnectionType m_connectionType { Inspector::FrontendChannel::ConnectionType::Remote };
};

} // namespace WebKit
