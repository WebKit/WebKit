/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "InjectedBundle.h"

#include "ActivateFonts.h"
#include "InjectedBundlePage.h"
#include "StringFunctions.h"
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WebKit2_C.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/Vector.h>

namespace WTR {

InjectedBundle& InjectedBundle::shared()
{
    static InjectedBundle& shared = *new InjectedBundle;
    return shared;
}

InjectedBundle::InjectedBundle()
    : m_bundle(0)
    , m_topLoadingFrame(0)
    , m_state(Idle)
    , m_dumpPixels(false)
    , m_useWaitToDumpWatchdogTimer(true)
{
}

void InjectedBundle::didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didCreatePage(page);
}

void InjectedBundle::willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->willDestroyPage(page);
}

void InjectedBundle::didInitializePageGroup(WKBundleRef bundle, WKBundlePageGroupRef pageGroup, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didInitializePageGroup(pageGroup);
}

void InjectedBundle::didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didReceiveMessage(messageName, messageBody);
}

void InjectedBundle::initialize(WKBundleRef bundle, WKTypeRef initializationUserData)
{
    m_bundle = bundle;
    m_stringBuilder = WTF::adoptPtr(new WTF::StringBuilder());

    WKBundleClient client = {
        kWKBundleClientCurrentVersion,
        this,
        didCreatePage,
        willDestroyPage,
        didInitializePageGroup,
        didReceiveMessage
    };
    WKBundleSetClient(m_bundle, &client);

    platformInitialize(initializationUserData);

    activateFonts();
    WKBundleActivateMacFontAscentHack(m_bundle);
}

void InjectedBundle::didCreatePage(WKBundlePageRef page)
{
    m_pages.append(adoptPtr(new InjectedBundlePage(page)));
}

void InjectedBundle::willDestroyPage(WKBundlePageRef page)
{
    size_t size = m_pages.size();
    for (size_t i = 0; i < size; ++i) {
        if (m_pages[i]->page() == page) {
            m_pages.remove(i);
            break;
        }
    }
}

void InjectedBundle::didInitializePageGroup(WKBundlePageGroupRef pageGroup)
{
    m_pageGroup = pageGroup;
}

InjectedBundlePage* InjectedBundle::page() const
{
    // It might be better to have the UI process send over a reference to the main
    // page instead of just assuming it's the first one.
    return m_pages[0].get();
}

void InjectedBundle::resetLocalSettings()
{
    setlocale(LC_ALL, "");
}

void InjectedBundle::didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "BeginTest")) {
        ASSERT(messageBody);
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> dumpPixelsKey(AdoptWK, WKStringCreateWithUTF8CString("DumpPixels"));
        m_dumpPixels = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, dumpPixelsKey.get())));

        WKRetainPtr<WKStringRef> useWaitToDumpWatchdogTimerKey(AdoptWK, WKStringCreateWithUTF8CString("UseWaitToDumpWatchdogTimer"));
        m_useWaitToDumpWatchdogTimer = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, useWaitToDumpWatchdogTimerKey.get())));

        WKRetainPtr<WKStringRef> ackMessageName(AdoptWK, WKStringCreateWithUTF8CString("Ack"));
        WKRetainPtr<WKStringRef> ackMessageBody(AdoptWK, WKStringCreateWithUTF8CString("BeginTest"));
        WKBundlePostMessage(m_bundle, ackMessageName.get(), ackMessageBody.get());

        beginTesting(messageBodyDictionary);
        return;
    } else if (WKStringIsEqualToUTF8CString(messageName, "Reset")) {
        ASSERT(messageBody);
        ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
        WKDictionaryRef messageBodyDictionary = static_cast<WKDictionaryRef>(messageBody);

        WKRetainPtr<WKStringRef> shouldGCKey(AdoptWK, WKStringCreateWithUTF8CString("ShouldGC"));
        bool shouldGC = WKBooleanGetValue(static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(messageBodyDictionary, shouldGCKey.get())));

        if (shouldGC)
            WKBundleGarbageCollectJavaScriptObjects(m_bundle);

        m_state = Idle;
        m_dumpPixels = false;

        resetLocalSettings();

        return;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "CallAddChromeInputFieldCallback")) {
        m_testRunner->callAddChromeInputFieldCallback();
        return;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "CallRemoveChromeInputFieldCallback")) {
        m_testRunner->callRemoveChromeInputFieldCallback();
        return;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "CallFocusWebViewCallback")) {
        m_testRunner->callFocusWebViewCallback();
        return;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "CallSetBackingScaleFactorCallback")) {
        m_testRunner->callSetBackingScaleFactorCallback();
        return;
    }

    WKRetainPtr<WKStringRef> errorMessageName(AdoptWK, WKStringCreateWithUTF8CString("Error"));
    WKRetainPtr<WKStringRef> errorMessageBody(AdoptWK, WKStringCreateWithUTF8CString("Unknown"));
    WKBundlePostMessage(m_bundle, errorMessageName.get(), errorMessageBody.get());
}

bool InjectedBundle::booleanForKey(WKDictionaryRef dictionary, const char* key)
{
    WKRetainPtr<WKStringRef> wkKey(AdoptWK, WKStringCreateWithUTF8CString(key));
    WKTypeRef value = WKDictionaryGetItemForKey(dictionary, wkKey.get());
    if (WKGetTypeID(value) != WKBooleanGetTypeID()) {
        stringBuilder()->append("Boolean value for key \"");
        stringBuilder()->append(key);
        stringBuilder()->append("\" not found in dictionary\n");
        return false;
    }
    return WKBooleanGetValue(static_cast<WKBooleanRef>(value));
}

void InjectedBundle::beginTesting(WKDictionaryRef settings)
{
    m_state = Testing;

    m_pixelResult.clear();
    m_repaintRects.clear();
    m_stringBuilder->clear();

    m_testRunner = TestRunner::create();
    m_gcController = GCController::create();
    m_eventSendingController = EventSendingController::create();
    m_textInputController = TextInputController::create();
    m_accessibilityController = AccessibilityController::create();

    WKBundleSetShouldTrackVisitedLinks(m_bundle, false);
    WKBundleRemoveAllVisitedLinks(m_bundle);
    WKBundleSetAllowUniversalAccessFromFileURLs(m_bundle, m_pageGroup, true);
    WKBundleSetJavaScriptCanAccessClipboard(m_bundle, m_pageGroup, true);
    WKBundleSetPrivateBrowsingEnabled(m_bundle, m_pageGroup, false);
    WKBundleSwitchNetworkLoaderToNewTestingSession(m_bundle);
    WKBundleSetAuthorAndUserStylesEnabled(m_bundle, m_pageGroup, true);
    WKBundleSetFrameFlatteningEnabled(m_bundle, m_pageGroup, false);

    WKBundleRemoveAllUserContent(m_bundle, m_pageGroup);

    m_testRunner->setShouldDumpFrameLoadCallbacks(booleanForKey(settings, "DumpFrameLoadDelegates"));

    page()->prepare();

    WKBundleClearAllDatabases(m_bundle);
    WKBundleClearApplicationCache(m_bundle);
    WKBundleResetOriginAccessWhitelists(m_bundle);
}

void InjectedBundle::done()
{
    m_state = Stopping;

    page()->stopLoading();
    setTopLoadingFrame(0);

    m_accessibilityController->resetToConsistentState();

    WKRetainPtr<WKStringRef> doneMessageName(AdoptWK, WKStringCreateWithUTF8CString("Done"));
    WKRetainPtr<WKMutableDictionaryRef> doneMessageBody(AdoptWK, WKMutableDictionaryCreate());

    WKRetainPtr<WKStringRef> textOutputKey(AdoptWK, WKStringCreateWithUTF8CString("TextOutput"));
    WKRetainPtr<WKStringRef> textOutput(AdoptWK, WKStringCreateWithUTF8CString(m_stringBuilder->toString().utf8().data()));
    WKDictionaryAddItem(doneMessageBody.get(), textOutputKey.get(), textOutput.get());
    
    WKRetainPtr<WKStringRef> pixelResultKey = adoptWK(WKStringCreateWithUTF8CString("PixelResult"));
    WKDictionaryAddItem(doneMessageBody.get(), pixelResultKey.get(), m_pixelResult.get());

    WKRetainPtr<WKStringRef> repaintRectsKey = adoptWK(WKStringCreateWithUTF8CString("RepaintRects"));
    WKDictionaryAddItem(doneMessageBody.get(), repaintRectsKey.get(), m_repaintRects.get());

    WKBundlePostMessage(m_bundle, doneMessageName.get(), doneMessageBody.get());

    closeOtherPages();

    page()->resetAfterTest();

    m_state = Idle;
}

void InjectedBundle::closeOtherPages()
{
    Vector<WKBundlePageRef> pagesToClose;
    size_t size = m_pages.size();
    for (size_t i = 1; i < size; ++i)
        pagesToClose.append(m_pages[i]->page());
    size = pagesToClose.size();
    for (size_t i = 0; i < size; ++i)
        WKBundlePageClose(pagesToClose[i]);
}

void InjectedBundle::dumpBackForwardListsForAllPages()
{
    size_t size = m_pages.size();
    for (size_t i = 0; i < size; ++i)
        m_pages[i]->dumpBackForwardList();
}
    
void InjectedBundle::postNewBeforeUnloadReturnValue(bool value)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("BeforeUnloadReturnValue"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(value));
    WKBundlePostMessage(m_bundle, messageName.get(), messageBody.get());
}

void InjectedBundle::postAddChromeInputField()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("AddChromeInputField"));
    WKBundlePostMessage(m_bundle, messageName.get(), 0);
}

void InjectedBundle::postRemoveChromeInputField()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("RemoveChromeInputField"));
    WKBundlePostMessage(m_bundle, messageName.get(), 0);
}

void InjectedBundle::postFocusWebView()
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("FocusWebView"));
    WKBundlePostMessage(m_bundle, messageName.get(), 0);
}

void InjectedBundle::postSetBackingScaleFactor(double backingScaleFactor)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetBackingScaleFactor"));
    WKRetainPtr<WKDoubleRef> messageBody(AdoptWK, WKDoubleCreate(backingScaleFactor));
    WKBundlePostMessage(m_bundle, messageName.get(), messageBody.get());
}

void InjectedBundle::postSetWindowIsKey(bool isKey)
{
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString("SetWindowIsKey"));
    WKRetainPtr<WKBooleanRef> messageBody(AdoptWK, WKBooleanCreate(isKey));
    WKBundlePostSynchronousMessage(m_bundle, messageName.get(), messageBody.get(), 0);
}

} // namespace WTR
