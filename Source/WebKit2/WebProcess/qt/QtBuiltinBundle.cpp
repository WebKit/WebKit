/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "QtBuiltinBundle.h"

#include "QtBuiltinBundlePage.h"
#include "WKArray.h"
#include "WKBundlePage.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WKStringQt.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {

QtBuiltinBundle::~QtBuiltinBundle()
{
    // For OwnPtr's sake.
}

QtBuiltinBundle& QtBuiltinBundle::shared()
{
    static QtBuiltinBundle& shared = *new QtBuiltinBundle;
    return shared;
}

void QtBuiltinBundle::initialize(WKBundleRef bundle)
{
    m_bundle = bundle;
    WKBundleClient client = {
        kWKBundleClientCurrentVersion,
        this,
        didCreatePage,
        willDestroyPage,
        0, // didInitializePageGroup
        didReceiveMessage
    };
    WKBundleSetClient(m_bundle, &client);
}

void QtBuiltinBundle::didCreatePage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<QtBuiltinBundle*>(const_cast<void*>(clientInfo))->didCreatePage(page);
}

void QtBuiltinBundle::willDestroyPage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    static_cast<QtBuiltinBundle*>(const_cast<void*>(clientInfo))->willDestroyPage(page);
}

void QtBuiltinBundle::didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo)
{
    static_cast<QtBuiltinBundle*>(const_cast<void*>(clientInfo))->didReceiveMessage(messageName, messageBody);
}

void QtBuiltinBundle::didCreatePage(WKBundlePageRef page)
{
    m_pages.add(page, adoptPtr(new QtBuiltinBundlePage(this, page)));
}

void QtBuiltinBundle::willDestroyPage(WKBundlePageRef page)
{
    m_pages.remove(page);
}

void QtBuiltinBundle::didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "MessageToNavigatorQtObject"))
        handleMessageToNavigatorQtObject(messageBody);
    else if (WKStringIsEqualToUTF8CString(messageName, "SetNavigatorQtObjectEnabled"))
        handleSetNavigatorQtObjectEnabled(messageBody);
}

void QtBuiltinBundle::handleMessageToNavigatorQtObject(WKTypeRef messageBody)
{
    ASSERT(messageBody);
    ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());

    WKArrayRef body = static_cast<WKArrayRef>(messageBody);
    ASSERT(WKArrayGetSize(body) == 2);
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 0)) == WKBundlePageGetTypeID());
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 1)) == WKStringGetTypeID());

    WKBundlePageRef page = static_cast<WKBundlePageRef>(WKArrayGetItemAtIndex(body, 0));
    WKStringRef contents = static_cast<WKStringRef>(WKArrayGetItemAtIndex(body, 1));

    QtBuiltinBundlePage* bundlePage = m_pages.get(page);
    if (!bundlePage)
        return;
    bundlePage->didReceiveMessageToNavigatorQtObject(contents);
}

void QtBuiltinBundle::handleSetNavigatorQtObjectEnabled(WKTypeRef messageBody)
{
    ASSERT(messageBody);
    ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());

    WKArrayRef body = static_cast<WKArrayRef>(messageBody);
    ASSERT(WKArrayGetSize(body) == 2);
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 0)) == WKBundlePageGetTypeID());
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 1)) == WKBooleanGetTypeID());

    WKBundlePageRef page = static_cast<WKBundlePageRef>(WKArrayGetItemAtIndex(body, 0));
    WKBooleanRef enabled = static_cast<WKBooleanRef>(WKArrayGetItemAtIndex(body, 1));

    QtBuiltinBundlePage* bundlePage = m_pages.get(page);
    if (!bundlePage)
        return;
    bundlePage->setNavigatorQtObjectEnabled(enabled);
}

} // namespace WebKit
