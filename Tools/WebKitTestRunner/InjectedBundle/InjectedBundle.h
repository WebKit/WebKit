/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "EventSendingController.h"
#include "GCController.h"
#include "TestRunner.h"
#include "TextInputController.h"
#include <WebKit/WKBase.h>
#include <WebKit/WKRetainPtr.h>
#include <sstream>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if HAVE(ACCESSIBILITY)
#include "AccessibilityController.h"
#endif

namespace WTR {

class InjectedBundlePage;

class InjectedBundle {
public:
    static InjectedBundle& singleton();

    // Initialize the InjectedBundle.
    void initialize(WKBundleRef, WKTypeRef initializationUserData);

    WKBundleRef bundle() const { return m_bundle; }
    WKBundlePageGroupRef pageGroup() const { return m_pageGroup; }

    TestRunner* testRunner() { return m_testRunner.get(); }
    GCController* gcController() { return m_gcController.get(); }
    EventSendingController* eventSendingController() { return m_eventSendingController.get(); }
    TextInputController* textInputController() { return m_textInputController.get(); }
#if HAVE(ACCESSIBILITY)
    AccessibilityController* accessibilityController() { return m_accessibilityController.get(); }
#endif

    InjectedBundlePage* page() const;
    WKBundlePageRef pageRef() const;
    size_t pageCount() const { return m_pages.size(); }
    void closeOtherPages();

    void dumpBackForwardListsForAllPages(StringBuilder&);

    void done();
    void setAudioResult(WKDataRef audioData) { m_audioResult = audioData; }
    void setPixelResult(WKImageRef image) { m_pixelResult = image; m_pixelResultIsPending = false; }
    void setPixelResultIsPending(bool isPending) { m_pixelResultIsPending = isPending; }
    void setRepaintRects(WKArrayRef rects) { m_repaintRects = rects; }

    bool isTestRunning() { return m_state == Testing; }

    WKBundleFrameRef topLoadingFrame() { return m_topLoadingFrame; }
    void setTopLoadingFrame(WKBundleFrameRef frame) { m_topLoadingFrame = frame; }

    bool shouldDumpPixels() const { return m_dumpPixels; }
    bool dumpJSConsoleLogInStdErr() const { return m_dumpJSConsoleLogInStdErr; };

    void outputText(const String&);
    void dumpToStdErr(const String&);
    void postNewBeforeUnloadReturnValue(bool);
    void postAddChromeInputField();
    void postRemoveChromeInputField();
    void postFocusWebView();
    void postSetBackingScaleFactor(double);
    void postSetWindowIsKey(bool);
    void postSetViewSize(double width, double height);
    void postSimulateWebNotificationClick(uint64_t notificationID);
    void postSetAddsVisitedLinks(bool);

    // Geolocation.
    void setGeolocationPermission(bool);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy, Optional<double> altitude, Optional<double> altitudeAccuracy, Optional<double> heading, Optional<double> speed, Optional<double> floorLevel);
    void setMockGeolocationPositionUnavailableError(WKStringRef errorMessage);
    bool isGeolocationProviderActive() const;

    // MediaStream.
    void setUserMediaPermission(bool);
    void resetUserMediaPermission();
    void setUserMediaPersistentPermissionForOrigin(bool permission, WKStringRef origin, WKStringRef parentOrigin);
    unsigned userMediaPermissionRequestCountForOrigin(WKStringRef origin, WKStringRef parentOrigin) const;
    void resetUserMediaPermissionRequestCountForOrigin(WKStringRef origin, WKStringRef parentOrigin);

    // Policy delegate.
    void setCustomPolicyDelegate(bool enabled, bool permissive);

    // Page Visibility.
    void setHidden(bool);

    // Cache.
    void setCacheModel(int);

    // Work queue.
    bool shouldProcessWorkQueue() const;
    void processWorkQueue();
    void queueBackNavigation(unsigned howFarBackward);
    void queueForwardNavigation(unsigned howFarForward);
    void queueLoad(WKStringRef url, WKStringRef target, bool shouldOpenExternalURLs = false);
    void queueLoadHTMLString(WKStringRef content, WKStringRef baseURL = 0, WKStringRef unreachableURL = 0);
    void queueReload();
    void queueLoadingScript(WKStringRef script);
    void queueNonLoadingScript(WKStringRef script);

    bool isAllowedHost(WKStringRef);

    unsigned imageCountInGeneralPasteboard() const;

    void setAllowsAnySSLCertificate(bool);

    bool statisticsNotifyObserver();

    void textDidChangeInTextField();
    void textFieldDidBeginEditing();
    void textFieldDidEndEditing();

    void reportLiveDocuments(WKBundlePageRef);

    void resetUserScriptInjectedCount() { m_userScriptInjectedCount = 0; }
    void increaseUserScriptInjectedCount() { ++m_userScriptInjectedCount; }
    size_t userScriptInjectedCount() const { return m_userScriptInjectedCount; }

private:
    InjectedBundle() = default;
    ~InjectedBundle();

    static void didCreatePage(WKBundleRef, WKBundlePageRef, const void* clientInfo);
    static void willDestroyPage(WKBundleRef, WKBundlePageRef, const void* clientInfo);
    static void didInitializePageGroup(WKBundleRef, WKBundlePageGroupRef, const void* clientInfo);
    static void didReceiveMessage(WKBundleRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo);
    static void didReceiveMessageToPage(WKBundleRef, WKBundlePageRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo);

    void didCreatePage(WKBundlePageRef);
    void willDestroyPage(WKBundlePageRef);
    void didInitializePageGroup(WKBundlePageGroupRef);
    void didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody);
    void didReceiveMessageToPage(WKBundlePageRef, WKStringRef messageName, WKTypeRef messageBody);

    void setUpInjectedBundleClients(WKBundlePageRef);

    void platformInitialize(WKTypeRef initializationUserData);

    enum class BegingTestingMode { New, Resume };
    void beginTesting(WKDictionaryRef initialSettings, BegingTestingMode);

    WKBundleRef m_bundle { nullptr };
    WKBundlePageGroupRef m_pageGroup { nullptr };
    Vector<std::unique_ptr<InjectedBundlePage>> m_pages;

#if HAVE(ACCESSIBILITY)
    RefPtr<AccessibilityController> m_accessibilityController;
#endif
    RefPtr<TestRunner> m_testRunner;
    RefPtr<GCController> m_gcController;
    RefPtr<EventSendingController> m_eventSendingController;
    RefPtr<TextInputController> m_textInputController;

    WKBundleFrameRef m_topLoadingFrame { nullptr };

    enum State {
        Idle,
        Testing,
        Stopping
    };
    State m_state { Idle };

    bool m_dumpPixels { false };
    bool m_useWorkQueue { false };
    bool m_pixelResultIsPending { false };
    bool m_dumpJSConsoleLogInStdErr { false };
    bool m_accessibilityIsolatedTreeMode { false };

    WTF::Seconds m_timeout;

    WKRetainPtr<WKDataRef> m_audioResult;
    WKRetainPtr<WKImageRef> m_pixelResult;
    WKRetainPtr<WKArrayRef> m_repaintRects;

    Vector<String> m_allowedHosts;

    size_t m_userScriptInjectedCount { 0 };
};

void postMessage(const char* name);
void postMessage(const char* name, bool value);
void postMessage(const char* name, int value);
void postMessage(const char* name, unsigned value);
void postMessage(const char* name, uint64_t value);
void postMessage(const char* name, const char* value);
void postMessage(const char* name, JSStringRef value);
void postMessage(const char* name, const void* value) = delete;

void postSynchronousMessage(const char* name);
void postSynchronousMessage(const char* name, bool value);
void postSynchronousMessage(const char* name, int value);
void postSynchronousMessage(const char* name, unsigned value);
void postSynchronousMessage(const char* name, uint64_t value);
void postSynchronousMessage(const char* name, double value);
void postSynchronousMessage(const char* name, const char* value);
void postSynchronousMessage(const char* name, const void* value) = delete;

void postPageMessage(const char* name);
void postPageMessage(const char* name, bool value);
void postPageMessage(const char* name, const char* value);
void postPageMessage(const char* name, WKStringRef value);
void postPageMessage(const char* name, const void* value) = delete;

void postSynchronousPageMessage(const char* name);
void postSynchronousPageMessage(const char* name, bool value);
void postSynchronousPageMessage(const char* name, const char* value);
void postSynchronousPageMessage(const char* name, const void* value) = delete;

template<typename T> void postMessage(const char* name, const WKRetainPtr<T>& value)
{
    WKBundlePostMessage(InjectedBundle::singleton().bundle(), toWK(name).get(), value.get());
}

template<typename T> void postSynchronousMessage(const char* name, const WKRetainPtr<T>& value)
{
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK(name).get(), value.get(), nullptr);
}

template<typename T> void postPageMessage(const char* name, const WKRetainPtr<T>& value)
{
    if (auto page = InjectedBundle::singleton().pageRef())
        WKBundlePagePostMessage(page, toWK(name).get(), value.get());
}

template<typename T> void postSynchronousPageMessage(const char* name, const WKRetainPtr<T>& value)
{
    if (auto page = InjectedBundle::singleton().pageRef())
        WKBundlePagePostSynchronousMessageForTesting(page, toWK(name).get(), value.get(), nullptr);
}

} // namespace WTR
