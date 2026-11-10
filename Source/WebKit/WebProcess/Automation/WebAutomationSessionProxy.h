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
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/PrivateName.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/IntRect.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBDRIVER_BIDI)
#include "IdentifierTypes.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <WebCore/AutomationInstrumentation.h>
#endif

namespace WebCore {
struct Cookie;
class AccessibilityObject;
class Element;
class ShareableBitmapHandle;
}

namespace WebKit {

class WebFrame;
class WebPage;
class WebAutomationDOMWindowObserver;

class WebAutomationSessionProxy : public IPC::MessageReceiver
#if ENABLE(WEBDRIVER_BIDI)
    , public WebCore::AutomationInstrumentationClient
#endif
    , public ThreadSafeRefCounted<WebAutomationSessionProxy> {
    WTF_MAKE_TZONE_ALLOCATED(WebAutomationSessionProxy);
public:
    static Ref<WebAutomationSessionProxy> create(StringImpl* sessionIdentifier);
    ~WebAutomationSessionProxy();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    RefPtr<StringImpl> sessionIdentifier() const { return m_sessionIdentifier; }

    void didClearWindowObjectForFrame(WebFrame&);
    void willDestroyGlobalObjectForFrame(WebCore::FrameIdentifier);

    struct JSCallbackIdentifierType { };
    using JSCallbackIdentifier = ObjectIdentifier<JSCallbackIdentifierType>;

    void didEvaluateJavaScriptFunction(WebCore::FrameIdentifier, JSCallbackIdentifier, StringImpl* result, StringImpl* errorType);

    void cancelPendingEvaluateJavaScriptCallbacks();

    bool isKnownReference(WebCore::FrameIdentifier, StringImpl* nodeHandle);
    void addKnownReference(WebCore::FrameIdentifier, StringImpl* nodeHandle);

private:
    explicit WebAutomationSessionProxy(StringImpl* sessionIdentifier);
    JSObjectRef scriptObject(JSGlobalContextRef);
    void setScriptObject(JSGlobalContextRef, JSObjectRef);
    JSObjectRef scriptObjectForFrame(WebFrame&);
    std::expected<Ref<WebCore::Element>, RefPtr<StringImpl>> elementForNodeHandle(WebFrame&, StringImpl*);
    static RefPtr<StringImpl> errorTypeFromJavaScriptExceptionName(StringImpl* exceptionName);
    WebCore::AccessibilityObject* getAccessibilityObjectForNode(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, RefPtr<StringImpl>& error);

    void ensureObserverForFrame(WebFrame&);

    // Implemented in generated WebAutomationSessionProxyMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by WebAutomationSessionProxy messages
    void evaluateJavaScriptFunction(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, StringImpl* function, Vector<RefPtr<StringImpl>> arguments, bool expectsImplicitCallbackArgument, bool forceUserGesture, std::optional<double> callbackTimeout, CompletionHandler<void(RefPtr<StringImpl>&&, RefPtr<StringImpl>&&)>&&);
    void evaluateBidiScript(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, StringImpl* expression, bool awaitPromise, int maxObjectDepth, std::optional<double> callbackTimeout, CompletionHandler<void(RefPtr<StringImpl>&&, RefPtr<StringImpl>&&)>&&);
    void resolveChildFrameWithOrdinal(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, uint32_t ordinal, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, std::optional<WebCore::FrameIdentifier>)>&&);
    void resolveChildFrameWithNodeHandle(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, StringImpl* nodeHandle, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, std::optional<WebCore::FrameIdentifier>)>&&);
    void resolveChildFrameWithName(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, StringImpl* name, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, std::optional<WebCore::FrameIdentifier>)>&&);
    void resolveParentFrame(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, std::optional<WebCore::FrameIdentifier>)>&&);
    void focusFrame(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, CompletionHandler<void(Inspector::CommandResult<void>)>&&);
    void computeElementLayout(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, bool scrollIntoViewIfNeeded, CoordinateSystem, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, WebCore::FloatRect, std::optional<WebCore::IntPoint>, bool)>&&);
    void frameViewportInLayoutViewport(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, std::optional<WebCore::FloatRect>)>&&);
    void getComputedRole(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, std::optional<RefPtr<StringImpl>>)>&&);
    void getComputedLabel(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, std::optional<RefPtr<StringImpl>>)>&&);
    void consumeUserActivation(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, bool)>&&);
    void selectOptionElement(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, CompletionHandler<void(std::optional<RefPtr<StringImpl>>)>&&);
    void setFilesForInputFileUpload(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, Vector<RefPtr<StringImpl>>&& filenames, CompletionHandler<void(std::optional<RefPtr<StringImpl>>)>&&);
    void takeScreenshot(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, CompletionHandler<void(std::optional<WebCore::ShareableBitmapHandle>&&, RefPtr<StringImpl>&&)>&&);
    void documentRectForScreenshot(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, WebCore::IntRect&&)>&&);
    void snapshotRectForScreenshot(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, WebCore::IntRect&&)>&&);
    void getCookiesForFrame(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, CompletionHandler<void(std::optional<RefPtr<StringImpl>>, Vector<WebCore::Cookie>)>&&);
    void deleteCookie(WebCore::PageIdentifier, std::optional<WebCore::FrameIdentifier>, RefPtr<StringImpl> cookieName, CompletionHandler<void(std::optional<RefPtr<StringImpl>>)>&&);

#if ENABLE(WEBDRIVER_BIDI)
    void addMessageToConsole(const JSC::MessageSource&, const JSC::MessageLevel&, StringImpl*, const JSC::MessageType&, const WallTime&) override;
    void scriptRealmCreated(WebCore::FrameIdentifier, const WebCore::SecurityOriginData&) override;
    void scriptRealmDestroyed(WebCore::FrameIdentifier) override;
    void ensureRealmForInitialEmptyDocument(WebCore::PageIdentifier);
#endif

    RefPtr<StringImpl> m_sessionIdentifier;
    JSC::PrivateName m_scriptObjectIdentifier;

    HashMap<WebCore::FrameIdentifier, HashMap<JSCallbackIdentifier, CompletionHandler<void(RefPtr<StringImpl>&&, RefPtr<StringImpl>&&)>>> m_webFramePendingEvaluateJavaScriptCallbacksMap;
    HashMap<WebCore::FrameIdentifier, Ref<WebAutomationDOMWindowObserver>> m_frameObservers;

    // Outlives the page's global object, which is where the JS-side maps live, so a
    // reference minted in a previous document can still be told apart from one this
    // navigable never issued.
    //
    // Only a cache in front of WebAutomationSession, which holds the authoritative copy
    // and survives the process swaps this cannot. Evicting an entry therefore costs one
    // synchronous lookup, never a wrong answer, so it is bounded far more tightly than
    // the UI-process copy.
    HashMap<WebCore::FrameIdentifier, ListHashSet<RefPtr<StringImpl>>> m_knownReferences;
#if ENABLE(WEBDRIVER_BIDI)
    HashMap<WebCore::FrameIdentifier, RealmIdentifier> m_frameToRealmIdentifier;
#endif
};

} // namespace WebKit
