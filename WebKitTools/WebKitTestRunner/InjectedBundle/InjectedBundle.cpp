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
#include <WebKit2/WebKit2.h>
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKStringCF.h>
#include <wtf/RetainPtr.h>

namespace WTR {

InjectedBundle& InjectedBundle::shared()
{
    static InjectedBundle& shared = *new InjectedBundle;
    return shared;
}

InjectedBundle::InjectedBundle()
    : m_bundle(0)
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
}

void InjectedBundle::done()
{
    std::string output = m_outputStream.str();
    RetainPtr<CFStringRef> outputCFString(AdoptCF, CFStringCreateWithCString(0, output.c_str(), kCFStringEncodingUTF8));
    WKRetainPtr<WKStringRef> doneMessage(AdoptWK, WKStringCreateWithCFString(outputCFString.get()));
    WKBundlePostMessage(m_bundle, doneMessage.get());
}

void InjectedBundle::didCreatePage(WKBundlePageRef page)
{
    // FIXME: we really need the main page ref to be sent over from the ui process
    m_mainPage = new InjectedBundlePage(page);
    m_pages.add(page, m_mainPage);
}

void InjectedBundle::willDestroyPage(WKBundlePageRef page)
{
    delete m_pages.take(page);
}

void InjectedBundle::didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody)
{
    CFStringRef cfMessage = WKStringCopyCFString(0, messageName);
    if (CFEqual(cfMessage, CFSTR("BeginTest"))) {
        WKRetainPtr<WKStringRef> ackMessage(AdoptWK, WKStringCreateWithCFString(CFSTR("BeginTestAck")));
        WKBundlePostMessage(m_bundle, ackMessage.get());

        reset();
        return;
    }

    WKRetainPtr<WKStringRef> errorMessage(AdoptWK, WKStringCreateWithCFString(CFSTR("Error: Unknown.")));
    WKBundlePostMessage(m_bundle, errorMessage.get());
}

void InjectedBundle::reset()
{
    m_outputStream.str("");
    m_layoutTestController = LayoutTestController::create();
    WKBundleSetShouldTrackVisitedLinks(m_bundle, false);
    WKBundleRemoveAllVisitedLinks(m_bundle);
}

void InjectedBundle::setShouldTrackVisitedLinks()
{
    WKBundleSetShouldTrackVisitedLinks(m_bundle, true);
}

} // namespace WTR
