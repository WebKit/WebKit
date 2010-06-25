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

#include "InjectedBundlePage.h"
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundlePage.h>

namespace WTR {

InjectedBundle& InjectedBundle::shared()
{
    static InjectedBundle& shared = *new InjectedBundle;
    return shared;
}

InjectedBundle::InjectedBundle()
    : m_bundle(0)
    , m_layoutTestController(LayoutTestController::create(std::string("")))
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

void InjectedBundle::_didRecieveMessage(WKBundleRef bundle, WKStringRef message, const void *clientInfo)
{
    static_cast<InjectedBundle*>(const_cast<void*>(clientInfo))->didRecieveMessage(message);
}

void InjectedBundle::initialize(WKBundleRef bundle)
{
    m_bundle = bundle;

    WKBundleClient client = {
        0,
        this,
        _didCreatePage,
        _willDestroyPage,
        _didRecieveMessage
    };
    WKBundleSetClient(m_bundle, &client);
}

void InjectedBundle::didCreatePage(WKBundlePageRef page)
{
    m_pages.add(page, new InjectedBundlePage(page));
}

void InjectedBundle::willDestroyPage(WKBundlePageRef page)
{
    delete m_pages.take(page);
}

void InjectedBundle::didRecieveMessage(WKStringRef message)
{
}

} // namespace WTR
