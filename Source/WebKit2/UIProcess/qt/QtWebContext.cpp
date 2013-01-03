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

#include "QtDownloadManager.h"
#include "QtWebIconDatabaseClient.h"
#include "WKAPICast.h"
#include "WebContext.h"
#include "WebInspectorServer.h"
#include "WebPageProxy.h"
#include <WKArray.h>
#include <WKPage.h>
#include <WKString.h>
#include <WKType.h>

namespace WebKit {

static WebContext* s_defaultWebContext = 0;
static QtWebContext* s_defaultQtWebContext = 0;
static OwnPtr<QtDownloadManager> s_downloadManager;
static OwnPtr<QtWebIconDatabaseClient> s_iconDatabase;

static void initInspectorServer()
{
#if ENABLE(INSPECTOR_SERVER)
    QString inspectorEnv = QString::fromUtf8(qgetenv("QTWEBKIT_INSPECTOR_SERVER"));
    if (!inspectorEnv.isEmpty()) {
        QString bindAddress = QLatin1String("127.0.0.1");
        QString portStr = inspectorEnv;
        int port = 0;

        int portColonPos = inspectorEnv.lastIndexOf(':');
        if (portColonPos != -1) {
            portStr = inspectorEnv.mid(portColonPos + 1);
            bindAddress = inspectorEnv.mid(0, portColonPos);
        }

        bool ok = false;
        port = portStr.toInt(&ok);
        if (!ok) {
            qWarning("Non numeric port for the inspector server \"%s\". Examples of valid input: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's interface).", qPrintable(portStr));
            return;
        }

        bool success = WebInspectorServer::shared().listen(bindAddress, port);
        if (success) {
            QString inspectorServerUrl = QString::fromLatin1("http://%1:%2").arg(bindAddress).arg(port);
            qWarning("Inspector server started successfully. Try pointing a WebKit browser to %s", qPrintable(inspectorServerUrl));
        } else
            qWarning("Couldn't start the inspector server on bind address \"%s\" and port \"%d\". In case of invalid input, try something like: \"12345\" or \"192.168.2.14:12345\" (with the address of one of this host's interface).", qPrintable(bindAddress), port);
    }
#endif
}

static void globalInitialization()
{
    static bool initialized = false;
    if (initialized)
        return;

    initInspectorServer();
    initialized = true;
}

static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void*)
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

static void initializeContextInjectedBundleClient(WebContext* context)
{
    WKContextInjectedBundleClient injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(WKContextInjectedBundleClient));
    injectedBundleClient.version = kWKContextInjectedBundleClientCurrentVersion;
    injectedBundleClient.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;
    WKContextSetInjectedBundleClient(toAPI(context), &injectedBundleClient);
}

QtWebContext::QtWebContext(WebContext* context)
    : m_context(context)
{
}

QtWebContext::~QtWebContext()
{
    ASSERT(!s_defaultQtWebContext || s_defaultQtWebContext == this);
    s_defaultQtWebContext = 0;
}

// Used directly only by WebKitTestRunner.
PassRefPtr<QtWebContext> QtWebContext::create(WebContext* context)
{
    globalInitialization();
    // The lifetime of WebContext is a bit special, it is bound to the reference held
    // by QtWebContext at first and then enters a circular dependency with WebProcessProxy
    // once the first page is created until the web process exits. Because of this we can't
    // assume that destroying the last QtWebContext will destroy the WebContext and we
    // have to make sure that WebContext's clients follow its lifetime and aren't attached
    // to QtWebContext. QtWebContext itself is only attached to QQuickWebView.
    // Since we only support one WebContext at a time, initialize those clients globally
    // here. They have to be available to views spawned by WebKitTestRunner as well.
    if (!s_downloadManager)
        s_downloadManager = adoptPtr(new QtDownloadManager(context));
    if (!s_iconDatabase)
        s_iconDatabase = adoptPtr(new QtWebIconDatabaseClient(context));
    return adoptRef(new QtWebContext(context));
}

PassRefPtr<QtWebContext> QtWebContext::defaultContext()
{
    // Keep local references until we can return a ref to QtWebContext holding the WebContext.
    RefPtr<WebContext> webContext(s_defaultWebContext);
    RefPtr<QtWebContext> qtWebContext(s_defaultQtWebContext);

    if (!webContext) {
        webContext = WebContext::create(String());
        s_defaultWebContext = webContext.get();
        // Make sure for WebKitTestRunner that the injected bundle client isn't initialized
        // and that the page cache isn't enabled (defaultContext isn't used there).
        initializeContextInjectedBundleClient(webContext.get());
        // A good all-around default.
        webContext->setCacheModel(CacheModelDocumentBrowser);
    }

    if (!qtWebContext) {
        qtWebContext = QtWebContext::create(webContext.get());
        s_defaultQtWebContext = qtWebContext.get();
    }

    return qtWebContext.release();
}

PassRefPtr<WebPageProxy> QtWebContext::createWebPage(PageClient* client, WebPageGroup* pageGroup)
{
    return m_context->createWebPage(client, pageGroup);
}

QtDownloadManager* QtWebContext::downloadManager()
{
    ASSERT(s_downloadManager);
    return s_downloadManager.get();
}

QtWebIconDatabaseClient* QtWebContext::iconDatabase()
{
    ASSERT(s_iconDatabase);
    return s_iconDatabase.get();
}

void QtWebContext::invalidateContext(WebContext* context)
{
    UNUSED_PARAM(context);
    ASSERT(!s_defaultQtWebContext);
    ASSERT(!s_defaultWebContext || s_defaultWebContext == context);
    s_downloadManager.clear();
    s_iconDatabase.clear();
    s_defaultWebContext = 0;
}

} // namespace WebKit

