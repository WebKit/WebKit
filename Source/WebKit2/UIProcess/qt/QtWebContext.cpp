/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QtWebContext.h"

#include "MutableArray.h"
#include "QtDownloadManager.h"
#include "QtWebIconDatabaseClient.h"
#include "WKAPICast.h"
#include "WebContext.h"
#include "WebPageProxy.h"
#include <WKArray.h>
#include <WKPage.h>
#include <WKString.h>
#include <WKType.h>

namespace WebKit {

static uint64_t generateContextID()
{
    static uint64_t uniqueContextID = 1;
    return uniqueContextID++;
}

static HashMap<uint64_t, QtWebContext*> contextMap;

QtWebContext* QtWebContext::s_defaultContext = 0;

QtWebContext::QtWebContext(WebContext* context)
    : m_context(context)
{
    m_contextID = generateContextID();
    contextMap.set(m_contextID, this);
}

QtWebContext::~QtWebContext()
{
    if (s_defaultContext == this)
        s_defaultContext = 0;
    contextMap.remove(m_contextID);
}

// Used only by WebKitTestRunner. It avoids calling initialize(), so that we don't register any clients.
PassRefPtr<QtWebContext> QtWebContext::create(WebContext* context)
{
    return adoptRef(new QtWebContext(context));
}

PassRefPtr<QtWebContext> QtWebContext::defaultContext()
{
    if (s_defaultContext)
        return PassRefPtr<QtWebContext>(s_defaultContext);

    RefPtr<WebContext> context = WebContext::sharedProcessContext();
    RefPtr<QtWebContext> defaultContext = QtWebContext::create(context.get());
    s_defaultContext = defaultContext.get();
    defaultContext->initialize();

    return defaultContext.release();
}

PassRefPtr<WebPageProxy> QtWebContext::createWebPage(PageClient* client, WebPageGroup* pageGroup)
{
    return m_context->createWebPage(client, pageGroup);
}

void QtWebContext::setNavigatorQtObjectEnabled(WebPageProxy* webPageProxy, bool enabled)
{
    static String messageName("SetNavigatorQtObjectEnabled");
    RefPtr<MutableArray> body = MutableArray::create();
    body->append(webPageProxy);
    RefPtr<WebBoolean> webEnabled = WebBoolean::create(enabled);
    body->append(webEnabled.get());
    m_context->postMessageToInjectedBundle(messageName, body.get());
}

void QtWebContext::postMessageToNavigatorQtObject(WebPageProxy* webPageProxy, const QString& message)
{
    static String messageName("MessageToNavigatorQtObject");
    RefPtr<MutableArray> body = MutableArray::create();
    body->append(webPageProxy);
    RefPtr<WebString> contents = WebString::create(String(message));
    body->append(contents.get());
    m_context->postMessageToInjectedBundle(messageName, body.get());
}

QtWebContext* QtWebContext::contextByID(uint64_t id)
{
    return contextMap.get(id);
}

void QtWebContext::initialize()
{
    m_downloadManager = adoptPtr(new QtDownloadManager(m_context.get()));
    m_iconDatabase = adoptPtr(new QtWebIconDatabaseClient(this));
    initializeContextInjectedBundleClient();
}

void QtWebContext::initializeContextInjectedBundleClient()
{
    WKContextInjectedBundleClient injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(WKContextInjectedBundleClient));
    injectedBundleClient.version = kWKContextInjectedBundleClientCurrentVersion;
    injectedBundleClient.clientInfo = this;
    injectedBundleClient.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;
    WKContextSetInjectedBundleClient(toAPI(m_context.get()), &injectedBundleClient);
}

static QtWebContext* toQtWebContext(const void* clientInfo)
{
    ASSERT(clientInfo);
    return reinterpret_cast<QtWebContext*>(const_cast<void*>(clientInfo));
}

void QtWebContext::didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    toQtWebContext(clientInfo)->didReceiveMessageFromInjectedBundle(messageName, messageBody);
}

void QtWebContext::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (!WKStringIsEqualToUTF8CString(messageName, "MessageFromNavigatorQtObject"))
        return;

    ASSERT(messageBody);
    ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());

    WKArrayRef body = static_cast<WKArrayRef>(messageBody);
    ASSERT(WKArrayGetSize(body) == 2);
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 0)) == WKPageGetTypeID());
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 1)) == WKStringGetTypeID());

    WKPageRef page = static_cast<WKPageRef>(WKArrayGetItemAtIndex(body, 0));
    WKStringRef str = static_cast<WKStringRef>(WKArrayGetItemAtIndex(body, 1));

    toImpl(page)->didReceiveMessageFromNavigatorQtObject(toImpl(str)->string());
}

}
