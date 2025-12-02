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

#include "APIObject.h"
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

namespace WebKit {
class JavaScriptEvaluationResult;
class WebPageProxy;
}

namespace API {

class ContentWorld;
class FrameInfo;

class ScriptMessage final : public ObjectImpl<Object::Type::ScriptMessage> {
public:
    template <typename... Args> static Ref<ScriptMessage> create(Args&&... args) { return adoptRef(*new ScriptMessage(std::forward<Args>(args)...)); }

    virtual ~ScriptMessage();

#if PLATFORM(COCOA)
    const RetainPtr<id>& body() const { return m_body; }
    NSString *cocoaName() const { return m_cocoaName.get(); }
#endif
    API::Object* apiBody() const { return m_apiBody.get(); }
    WebKit::WebPageProxy* page() const;
    API::FrameInfo& frame() const { return m_frame.get(); }
    const WTF::String& name() const { return m_name; }
    API::ContentWorld& world() const { return m_world.get(); }

private:
    ScriptMessage(RefPtr<API::Object>&&, WebKit::WebPageProxy&, Ref<API::FrameInfo>&&, const WTF::String&, Ref<API::ContentWorld>&&);
#if PLATFORM(COCOA)
    ScriptMessage(RetainPtr<id>&&, WebKit::WebPageProxy&, Ref<API::FrameInfo>&&, RetainPtr<NSString>&&, Ref<API::ContentWorld>&&);

    const RetainPtr<id> m_body;
    const RetainPtr<NSString> m_cocoaName;
#endif
    const RefPtr<API::Object> m_apiBody;
    const WeakPtr<WebKit::WebPageProxy> m_page;
    const Ref<API::FrameInfo> m_frame;
    const WTF::String m_name;
    const Ref<API::ContentWorld> m_world;
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(ScriptMessage);
