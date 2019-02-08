/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <wtf/text/WTFString.h>

namespace WebCore {
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

    void didEvaluateJavaScriptFunction(uint64_t frameID, uint64_t callbackID, const String& result, const String& errorType);

private:
    JSObjectRef scriptObjectForFrame(WebFrame&);
    WebCore::Element* elementForNodeHandle(WebFrame&, const String&);

    // Implemented in generated WebAutomationSessionProxyMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by WebAutomationSessionProxy messages
    void evaluateJavaScriptFunction(uint64_t pageID, uint64_t frameID, const String& function, Vector<String> arguments, bool expectsImplicitCallbackArgument, int callbackTimeout, uint64_t callbackID);
    void resolveChildFrameWithOrdinal(uint64_t pageID, uint64_t frameID, uint32_t ordinal, uint64_t callbackID);
    void resolveChildFrameWithNodeHandle(uint64_t pageID, uint64_t frameID, const String& nodeHandle, uint64_t callbackID);
    void resolveChildFrameWithName(uint64_t pageID, uint64_t frameID, const String& name, uint64_t callbackID);
    void resolveParentFrame(uint64_t pageID, uint64_t frameID, uint64_t callbackID);
    void focusFrame(uint64_t pageID, uint64_t frameID);
    void computeElementLayout(uint64_t pageID, uint64_t frameID, String nodeHandle, bool scrollIntoViewIfNeeded, CoordinateSystem, uint64_t callbackID);
    void selectOptionElement(uint64_t pageID, uint64_t frameID, String nodeHandle, uint64_t callbackID);
    void takeScreenshot(uint64_t pageID, uint64_t frameID, String nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, uint64_t callbackID);
    void getCookiesForFrame(uint64_t pageID, uint64_t frameID, uint64_t callbackID);
    void deleteCookie(uint64_t pageID, uint64_t frameID, String cookieName, uint64_t callbackID);

    String m_sessionIdentifier;

    HashMap<uint64_t, JSObjectRef> m_webFrameScriptObjectMap;
    HashMap<uint64_t, Vector<uint64_t>> m_webFramePendingEvaluateJavaScriptCallbacksMap;
};

} // namespace WebKit
