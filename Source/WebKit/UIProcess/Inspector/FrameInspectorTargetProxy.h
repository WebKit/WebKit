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

#pragma once

#include "InspectorTargetProxy.h"
#include <JavaScriptCore/InspectorTarget.h>
#include <memory>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TypeCasts.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class ProvisionalFrameProxy;
class WebFrameProxy;

class FrameInspectorTargetProxy final : public InspectorTargetProxy {
    WTF_MAKE_TZONE_ALLOCATED(FrameInspectorTargetProxy);
    WTF_MAKE_NONCOPYABLE(FrameInspectorTargetProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameInspectorTargetProxy);
public:
    static std::unique_ptr<FrameInspectorTargetProxy> create(WebFrameProxy&, const String& targetId);
    static std::unique_ptr<FrameInspectorTargetProxy> create(ProvisionalFrameProxy&, const String& targetId);
    FrameInspectorTargetProxy(WebFrameProxy&, const String& targetId);
    ~FrameInspectorTargetProxy();

    void didCommitProvisionalTarget() override;
    bool isProvisional() const override;

    void connect(Inspector::FrontendChannel::ConnectionType) override;
    void disconnect() override;
    void sendMessageToTargetBackend(const String&) override;

private:
    WeakRef<WebFrameProxy> m_frame;
    WeakPtr<ProvisionalFrameProxy> m_provisionalFrame;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::FrameInspectorTargetProxy)
static bool isType(const WebKit::FrameInspectorTargetProxy& target)
{
    return target.type() == Inspector::InspectorTargetType::Frame;
}
SPECIALIZE_TYPE_TRAITS_END()
