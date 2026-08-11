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

#include <JavaScriptCore/InspectorTarget.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebKit {

class UIProcessForwardingFrontendChannel;
class WebPage;

class PageInspectorTarget final : public Inspector::InspectorTarget {
    WTF_MAKE_TZONE_ALLOCATED(PageInspectorTarget);
    WTF_MAKE_NONCOPYABLE(PageInspectorTarget);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageInspectorTarget);
public:
    explicit PageInspectorTarget(WebPage&);
    virtual ~PageInspectorTarget();

    Inspector::InspectorTargetType type() const final { return Inspector::InspectorTargetType::Page; }

    String identifier() const final;

    void connect(Inspector::FrontendChannel::ConnectionType) override;
    void disconnect() override;
    void sendMessageToTargetBackend(const String&) override;

    static String toTargetID(WebCore::PageIdentifier);

private:
    WeakRef<WebPage> m_page;
    std::unique_ptr<UIProcessForwardingFrontendChannel> m_channel;
};

} // namespace WebKit
