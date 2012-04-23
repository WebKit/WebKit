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

#ifndef QtWebContext_h
#define QtWebContext_h

#include <QtGlobal>
#include <WKContext.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace WebKit {

class PageClient;
class QtDownloadManager;
class QtWebIconDatabaseClient;
class WebContext;
class WebPageGroup;
class WebPageProxy;

class QtWebContext : public RefCounted<QtWebContext> {
public:
    ~QtWebContext();

    static PassRefPtr<QtWebContext> create(WebContext*);
    static PassRefPtr<QtWebContext> defaultContext();

    PassRefPtr<WebPageProxy> createWebPage(PageClient*, WebPageGroup*);

    WebContext* context() { return m_context.get(); }
    QtDownloadManager* downloadManager() { return m_downloadManager.get(); }
    QtWebIconDatabaseClient* iconDatabase() { return m_iconDatabase.get(); }

    void setNavigatorQtObjectEnabled(WebPageProxy*, bool);
    void postMessageToNavigatorQtObject(WebPageProxy*, const QString&);

    uint64_t contextID() const { return m_contextID; }

    static QtWebContext* contextByID(uint64_t id);

private:
    explicit QtWebContext(WebContext*);

    void initializeContextInjectedBundleClient();

    static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void*);
    void didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);

    static QtWebContext* s_defaultContext;

    uint64_t m_contextID;
    RefPtr<WebContext> m_context;
    OwnPtr<QtDownloadManager> m_downloadManager;
    OwnPtr<QtWebIconDatabaseClient> m_iconDatabase;
};

} // namespace WebKit

#endif // QtWebContext_h
