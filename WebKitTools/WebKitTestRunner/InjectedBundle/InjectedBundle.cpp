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
#include "StringFunctions.h"
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WebKit2.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WTR {

InjectedBundle& InjectedBundle::shared()
{
    static InjectedBundle& shared = *new InjectedBundle;
    return shared;
}

InjectedBundle::InjectedBundle()
    : m_bundle(0)
    , m_state(Idle)
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

void InjectedBundle::didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didReceiveMessage(messageName, messageBody);
}

void InjectedBundle::initialize(WKBundleRef bundle)
{
    m_bundle = bundle;

    WKBundleClient client = {
        0,
        this,
        didCreatePage,
        willDestroyPage,
        didReceiveMessage
    };
    WKBundleSetClient(m_bundle, &client);

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

InjectedBundlePage* InjectedBundle::page() const
{
    // It might be better to have the UI process send over a reference to the main
    // page instead of just assuming it's the first one.
    return m_pages[0].get();
}

void InjectedBundle::didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "BeginTest")) {
        ASSERT(!messageBody);

        WKRetainPtr<WKStringRef> ackMessageName(AdoptWK, WKStringCreateWithUTF8CString("Ack"));
        WKRetainPtr<WKStringRef> ackMessageBody(AdoptWK, WKStringCreateWithUTF8CString("BeginTest"));
        WKBundlePostMessage(m_bundle, ackMessageName.get(), ackMessageBody.get());

        beginTesting();
        return;
    }

    WKRetainPtr<WKStringRef> errorMessageName(AdoptWK, WKStringCreateWithUTF8CString("Error"));
    WKRetainPtr<WKStringRef> errorMessageBody(AdoptWK, WKStringCreateWithUTF8CString("Unknown"));
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

    page()->reset();
}

void InjectedBundle::done()
{
    m_state = Stopping;

    page()->stopLoading();

    WKRetainPtr<WKStringRef> doneMessageName(AdoptWK, WKStringCreateWithUTF8CString("Done"));
    WKRetainPtr<WKStringRef> doneMessageBody(AdoptWK, WKStringCreateWithUTF8CString(m_outputStream.str().c_str()));

    WKBundlePostMessage(m_bundle, doneMessageName.get(), doneMessageBody.get());

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

} // namespace WTR
