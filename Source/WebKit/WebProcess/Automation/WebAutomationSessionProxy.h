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
#include <JavaScriptCore/PrivateName.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/IntRect.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
struct Cookie;
class AccessibilityObject;
class Element;
}

namespace WebKit {

class WebFrame;
class WebPage;
class WebAutomationDOMWindowObserver;

class WebAutomationSessionProxy : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebAutomationSessionProxy(const String& sessionIdentifier);
    ~WebAutomationSessionProxy();

    String sessionIdentifier() const { return m_sessionIdentifier; }

    void didClearWindowObjectForFrame(WebFrame&);
    void willDestroyGlobalObjectForFrame(WebCore::FrameIdentifier);

    void didEvaluateJavaScriptFunction(WebCore::FrameIdentifier, uint64_t callbackID, const String& result, const String& errorType);

private:
    JSObjectRef scriptObject(JSGlobalContextRef);
    void setScriptObject(JSGlobalContextRef, JSObjectRef);
    JSObjectRef scriptObjectForFrame(WebFrame&);
    WebCore::Element* elementForNodeHandle(WebFrame&, const String&);
    WebCore::AccessibilityObject* getAccessibilityObjectForNode(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, String& error);

    void ensureObserverForFrame(WebFrame&);

    // Implemented in generated WebAutomationSessionProxyMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by WebAutomationSessionProxy messages
    void evaluateJavaScriptFunction(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, const String& function, Vector<String> arguments, bool expectsImplicitCallbackArgument, std::optional<double> callbackTimeout, uint64_t callbackID);
    void resolveChildFrameWithOrdinal(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, uint32_t ordinal, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&&);
    void resolveChildFrameWithNodeHandle(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, const String& nodeHandle, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&&);
    void resolveChildFrameWithName(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, const String& name, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&&);
    void resolveParentFrame(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&&);
    void computeElementLayout(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, bool scrollIntoViewIfNeeded, CoordinateSystem, CompletionHandler<void(std::optional<String>, WebCore::FloatRect, std::optional<WebCore::IntPoint>, bool)>&&);
    void getComputedRole(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, CompletionHandler<void(std::optional<String>, std::optional<String>)>&&);
    void getComputedLabel(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, CompletionHandler<void(std::optional<String>, std::optional<String>)>&&);
    void selectOptionElement(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, CompletionHandler<void(std::optional<String>)>&&);
    void setFilesForInputFileUpload(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, Vector<String>&& filenames, CompletionHandler<void(std::optional<String>)>&&);
    void takeScreenshot(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, uint64_t callbackID);
    void snapshotRectForScreenshot(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, CompletionHandler<void(std::optional<String>, WebCore::IntRect&&)>&&);
    void getCookiesForFrame(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, CompletionHandler<void(std::optional<String>, Vector<WebCore::Cookie>)>&&);
    void deleteCookie(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, String cookieName, CompletionHandler<void(std::optional<String>)>&&);

    String m_sessionIdentifier;
    JSC::PrivateName m_scriptObjectIdentifier;

    HashMap<WebCore::FrameIdentifier, Vector<uint64_t>> m_webFramePendingEvaluateJavaScriptCallbacksMap;
    HashMap<WebCore::FrameIdentifier, RefPtr<WebAutomationDOMWindowObserver>> m_frameObservers;
};

} // namespace WebKit
