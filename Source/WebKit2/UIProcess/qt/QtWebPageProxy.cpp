/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "QtWebPageProxy.h"

#include <QtQuick/qquickcanvas.h>
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"
#include "qwebpreferences_p.h"
#include "qwebpreferences_p_p.h"

#include "QtPageClient.h"
#include "QtWebContext.h"
#include "WebBackForwardList.h"
#include "WKStringQt.h"
#include <WebKit2/WKFrame.h>
#include <WebKit2/WKPageGroup.h>
#include <WebKit2/WKRetainPtr.h>

using namespace WebKit;
using namespace WebCore;

QtWebPageProxy::QtWebPageProxy(QQuickWebView* qmlWebView, QtPageClient *pageClient, PassRefPtr<QtWebContext> context, WKPageGroupRef pageGroupRef)
    : m_qmlWebView(qmlWebView)
    , m_context(context)
    , m_navigatorQtObjectEnabled(false)
{
    m_webPageProxy = m_context->createWebPage(pageClient, toImpl(pageGroupRef));
}

void QtWebPageProxy::init()
{
    m_webPageProxy->initializeWebPage();
}

QtWebPageProxy::~QtWebPageProxy()
{
    m_webPageProxy->close();
}

void QtWebPageProxy::showContextMenu(QSharedPointer<QMenu> menu)
{
    // Remove the active menu in case this function is called twice.
    if (activeMenu)
        activeMenu->hide();

    if (menu->isEmpty())
        return;

    QWindow* window = m_qmlWebView->canvas();
    if (!window)
        return;

    activeMenu = menu;

    activeMenu->window()->winId(); // Ensure that the menu has a window
    Q_ASSERT(activeMenu->window()->windowHandle());
    activeMenu->window()->windowHandle()->setTransientParent(window);

    QPoint menuPositionInScene = m_qmlWebView->mapToScene(menu->pos()).toPoint();
    menu->exec(window->mapToGlobal(menuPositionInScene));
    // The last function to get out of exec() clear the local copy.
    if (activeMenu == menu)
        activeMenu.clear();
}

void QtWebPageProxy::hideContextMenu()
{
    if (activeMenu)
        activeMenu->hide();
}

WKPageRef QtWebPageProxy::pageRef() const
{
    return toAPI(m_webPageProxy.get());;
}

QWebPreferences* QtWebPageProxy::preferences() const
{
    if (!m_preferences)
        m_preferences = adoptPtr(QWebPreferencesPrivate::createPreferences(const_cast<QtWebPageProxy*>(this)));
    return m_preferences.get();
}

void QtWebPageProxy::setCustomUserAgent(const QString& userAgent)
{
    WKRetainPtr<WKStringRef> wkUserAgent(WKStringCreateWithQString(userAgent));
    WKPageSetCustomUserAgent(pageRef(), wkUserAgent.get());
}

QString QtWebPageProxy::customUserAgent() const
{
    return WKStringCopyQString(WKPageCopyCustomUserAgent(pageRef()));
}

void QtWebPageProxy::setNavigatorQtObjectEnabled(bool enabled)
{
    ASSERT(enabled != m_navigatorQtObjectEnabled);
    // FIXME: Currently we have to keep this information in both processes and the setting is asynchronous.
    m_navigatorQtObjectEnabled = enabled;
    m_context->setNavigatorQtObjectEnabled(m_webPageProxy.get(), enabled);
}

void QtWebPageProxy::postMessageToNavigatorQtObject(const QString& message)
{
    m_context->postMessageToNavigatorQtObject(m_webPageProxy.get(), message);
}

qreal QtWebPageProxy::textZoomFactor() const
{
    return WKPageGetTextZoomFactor(pageRef());
}

void QtWebPageProxy::setTextZoomFactor(qreal zoomFactor)
{
    WKPageSetTextZoomFactor(pageRef(), zoomFactor);
}

qreal QtWebPageProxy::pageZoomFactor() const
{
    return WKPageGetPageZoomFactor(pageRef());
}

void QtWebPageProxy::setPageZoomFactor(qreal zoomFactor)
{
    WKPageSetPageZoomFactor(pageRef(), zoomFactor);
}

void QtWebPageProxy::setPageAndTextZoomFactors(qreal pageZoomFactor, qreal textZoomFactor)
{
    WKPageSetPageAndTextZoomFactors(pageRef(), pageZoomFactor, textZoomFactor);
}

#include "moc_QtWebPageProxy.cpp"
