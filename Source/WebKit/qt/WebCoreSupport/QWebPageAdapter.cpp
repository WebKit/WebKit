/*
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QWebPageAdapter.h"

#include "Chrome.h"
#include "ChromeClientQt.h"
#include "NotificationPresenterClientQt.h"
#include "QWebFrameAdapter.h"
#include "UndoStepQt.h"
#include "qwebpluginfactory.h"
#include "qwebsettings.h"
#include <Page.h>
#include <QNetworkAccessManager>

using namespace WebCore;

bool QWebPageAdapter::drtRun = false;

QWebPageAdapter::QWebPageAdapter()
    : settings(0)
    , page(0)
    , pluginFactory(0)
    , forwardUnsupportedContent(false)
    , insideOpenCall(false)
    , networkManager(0)
{
}

QWebPageAdapter::~QWebPageAdapter()
{
    delete page;
    delete settings;

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    NotificationPresenterClientQt::notificationPresenter()->removeClient();
#endif
}

void QWebPageAdapter::init(Page* page)
{
    this->page = page;
    settings = new QWebSettings(page->settings());

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebCore::provideNotification(page, NotificationPresenterClientQt::notificationPresenter());
#endif
}

void QWebPageAdapter::deletePage()
{
    delete page;
    page = 0;
}

QWebPageAdapter* QWebPageAdapter::kit(Page* page)
{
    return static_cast<ChromeClientQt*>(page->chrome()->client())->m_webPage;
}

ViewportArguments QWebPageAdapter::viewportArguments()
{
    return page ? page->viewportArguments() : WebCore::ViewportArguments();
}


void QWebPageAdapter::registerUndoStep(WTF::PassRefPtr<WebCore::UndoStep> step)
{
    createUndoStep(QSharedPointer<UndoStepQt>(new UndoStepQt(step)));
}

void QWebPageAdapter::setNetworkAccessManager(QNetworkAccessManager *manager)
{
    if (manager == networkManager)
        return;
    if (networkManager && networkManager->parent() == handle())
        delete networkManager;
    networkManager = manager;
}

QNetworkAccessManager* QWebPageAdapter::networkAccessManager()
{
    if (!networkManager)
        networkManager = new QNetworkAccessManager(handle());
    return networkManager;
}
