/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "InjectedBundle.h"

#include "ActivateFonts.h"
#include "InjectedBundlePage.h"
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKStringCF.h>
#include <WebKit2/WebKit2.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WTR {

InjectedBundle& InjectedBundle::shared()
{
    static InjectedBundle& shared = *new InjectedBundle;
    return shared;
}

InjectedBundle::InjectedBundle()
    : m_bundle(0)
    , m_mainPage(0)
    , m_state(Idle)
{
}

void InjectedBundle::_didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didCreatePage(page);
}

void InjectedBundle::_willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->willDestroyPage(page);
}

void InjectedBundle::_didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didReceiveMessage(messageName, messageBody);
}

void InjectedBundle::initialize(WKBundleRef bundle)
{
    m_bundle = bundle;

    WKBundleClient client = {
        0,
        this,
        _didCreatePage,
        _willDestroyPage,
        _didReceiveMessage
    };
    WKBundleSetClient(m_bundle, &client);

    activateFonts();
    WKBundleActivateMacFontAscentHack(m_bundle);
}

void InjectedBundle::didCreatePage(WKBundlePageRef page)
{
    // FIXME: we really need the main page ref to be sent over from the ui process
    OwnPtr<InjectedBundlePage> pageWrapper = adoptPtr(new InjectedBundlePage(page));
    if (!m_mainPage)
        m_mainPage = pageWrapper.release();
    else
        m_otherPages.add(page, pageWrapper.leakPtr());
}

void InjectedBundle::willDestroyPage(WKBundlePageRef page)
{
    if (m_mainPage && m_mainPage->page() == page)
        m_mainPage.clear();
    else
        delete m_otherPages.take(page);
}

void InjectedBundle::didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody)
{
    CFStringRef cfMessage = WKStringCopyCFString(0, messageName);
    if (CFEqual(cfMessage, CFSTR("BeginTest"))) {
        WKRetainPtr<WKStringRef> ackMessageName(AdoptWK, WKStringCreateWithCFString(CFSTR("Ack")));
        WKRetainPtr<WKStringRef> ackMessageBody(AdoptWK, WKStringCreateWithCFString(CFSTR("BeginTest")));
        WKBundlePostMessage(m_bundle, ackMessageName.get(), ackMessageBody.get());

        beginTesting();
        return;
    }

    WKRetainPtr<WKStringRef> errorMessageName(AdoptWK, WKStringCreateWithCFString(CFSTR("Error")));
    WKRetainPtr<WKStringRef> errorMessageBody(AdoptWK, WKStringCreateWithCFString(CFSTR("Unknown")));
    WKBundlePostMessage(m_bundle, errorMessageName.get(), errorMessageBody.get());
}

void InjectedBundle::beginTesting()
{
    m_state = Testing;

    m_outputStream.str("");

    m_layoutTestController = LayoutTestController::create();
    m_gcController = GCController::create();
    m_eventSendingController = EventSendingController::create();

    WKBundleSetShouldTrackVisitedLinks(m_bundle, false);
    WKBundleRemoveAllVisitedLinks(m_bundle);

    WKBundleRemoveAllUserContent(m_bundle);

    m_mainPage->reset();
}

void InjectedBundle::done()
{
    m_mainPage->stopLoading();

    WKRetainPtr<WKStringRef> doneMessageName(AdoptWK, WKStringCreateWithCFString(CFSTR("Done")));

    std::string output = m_outputStream.str();
    RetainPtr<CFStringRef> outputCFString(AdoptCF, CFStringCreateWithCString(0, output.c_str(), kCFStringEncodingUTF8));
    WKRetainPtr<WKStringRef> doneMessageBody(AdoptWK, WKStringCreateWithCFString(outputCFString.get()));

    WKBundlePostMessage(m_bundle, doneMessageName.get(), doneMessageBody.get());

    m_state = Idle;
}

void InjectedBundle::closeOtherPages()
{
    Vector<WKBundlePageRef> pages;
    copyKeysToVector(m_otherPages, pages);
    for (size_t i = 0; i < pages.size(); ++i)
        WKBundlePageClose(pages[i]);
}

} // namespace WTR
