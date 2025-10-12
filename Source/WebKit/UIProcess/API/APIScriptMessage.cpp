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

#include "config.h"
#include "APIScriptMessage.h"

#include "APIContentWorld.h"
#include "APIFrameInfo.h"
#include "JavaScriptEvaluationResult.h"
#include "WebPageProxy.h"

namespace API {

ScriptMessage::~ScriptMessage() = default;

#if PLATFORM(COCOA)
ScriptMessage::ScriptMessage(RetainPtr<id>&& body, WebKit::WebPageProxy& page, Ref<API::FrameInfo>&& frame, RetainPtr<NSString>&& name, Ref<API::ContentWorld>&& world)
    : m_body(WTFMove(body))
    , m_cocoaName(WTFMove(name))
    , m_page(page)
    , m_frame(WTFMove(frame))
    , m_world(WTFMove(world)) { }
#endif

ScriptMessage::ScriptMessage(RefPtr<API::Object>&& body, WebKit::WebPageProxy& page, Ref<API::FrameInfo>&& frame, const WTF::String& name, Ref<API::ContentWorld>&& world)
    : m_apiBody(WTFMove(body))
    , m_page(page)
    , m_frame(WTFMove(frame))
    , m_name(name)
    , m_world(WTFMove(world)) { }

WebKit::WebPageProxy* ScriptMessage::page() const
{
    return m_page.get();
}

}
