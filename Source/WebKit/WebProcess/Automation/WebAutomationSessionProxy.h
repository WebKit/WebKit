/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "CoordinateSystem.h"
#include <JavaScriptCore/JSBase.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
struct Cookie;
class Element;
}

namespace WebKit {

class WebFrame;
class WebPage;

class WebAutomationSessionProxy : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebAutomationSessionProxy(const String& sessionIdentifier);
    ~WebAutomationSessionProxy();

    String sessionIdentifier() const { return m_sessionIdentifier; }

    void didClearWindowObjectForFrame(WebFrame&);

    void didEvaluateJavaScriptFunction(WebCore::FrameIdentifier, uint64_t callbackID, const String& result, const String& errorType);

private:
    JSObjectRef scriptObjectForFrame(WebFrame&);
    WebCore::Element* elementForNodeHandle(WebFrame&, const String&);

    // Implemented in generated WebAutomationSessionProxyMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by WebAutomationSessionProxy messages
    void evaluateJavaScriptFunction(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, const String& function, Vector<String> arguments, bool expectsImplicitCallbackArgument, int callbackTimeout, uint64_t callbackID);
    void resolveChildFrameWithOrdinal(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, uint32_t ordinal, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&&);
    void resolveChildFrameWithNodeHandle(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, const String& nodeHandle, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&&);
    void resolveChildFrameWithName(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, const String& name, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&&);
    void resolveParentFrame(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&&);
    void focusFrame(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>);
    void computeElementLayout(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, String nodeHandle, bool scrollIntoViewIfNeeded, CoordinateSystem, CompletionHandler<void(Optional<String>, WebCore::IntRect, Optional<WebCore::IntPoint>, bool)>&&);
    void selectOptionElement(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, String nodeHandle, CompletionHandler<void(Optional<String>)>&&);
    void takeScreenshot(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, String nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, uint64_t callbackID);
    void getCookiesForFrame(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, CompletionHandler<void(Optional<String>, Vector<WebCore::Cookie>)>&&);
    void deleteCookie(WebCore::PageIdentifier, Optional<WebCore::FrameIdentifier>, String cookieName, CompletionHandler<void(Optional<String>)>&&);

    String m_sessionIdentifier;

    HashMap<WebCore::FrameIdentifier, JSObjectRef> m_webFrameScriptObjectMap;
    HashMap<WebCore::FrameIdentifier, Vector<uint64_t>> m_webFramePendingEvaluateJavaScriptCallbacksMap;
};

} // namespace WebKit
