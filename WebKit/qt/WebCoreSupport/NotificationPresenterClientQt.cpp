/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NotificationPresenterClientQt.h"

#include "DumpRenderTreeSupportQt.h"
#include "Document.h"
#include "EventNames.h"
#include "KURL.h"
#include "SecurityOrigin.h"

#include "qwebframe.h"
#include "qwebkitglobal.h"
#include "qwebpage.h"
#include <QtGui>


#if ENABLE(NOTIFICATIONS)

using namespace WebCore;

bool NotificationPresenterClientQt::dumpNotification = false;

NotificationIconWrapper::NotificationIconWrapper()
{
#ifndef QT_NO_SYSTEMTRAYICON
    m_notificationIcon = 0;
#endif
}

NotificationIconWrapper::~NotificationIconWrapper()
{
#ifndef QT_NO_SYSTEMTRAYICON
    delete m_notificationIcon;
#endif
}

NotificationPresenterClientQt::NotificationPresenterClientQt(QWebPage* page) : m_page(page)
{
}

bool NotificationPresenterClientQt::show(Notification* notification)
{
    QHash <Notification*, NotificationIconWrapper*>::Iterator end = m_notifications.end();
    QHash <Notification*, NotificationIconWrapper*>::Iterator iter = m_notifications.begin();

    if (!notification->replaceId().isEmpty()) {
        while (iter != end) {
            Notification* existingNotification = iter.key();
            if (existingNotification->replaceId() == notification->replaceId() && existingNotification->url().protocol() == notification->url().protocol() && existingNotification->url().host() == notification->url().host())
                break;
            iter++;
        }
    } else
        iter = end;

    if (dumpNotification) {
        if (iter != end) {
            Notification* oldNotification = iter.key();
            printf("REPLACING NOTIFICATION %s\n", oldNotification->isHTML() ? QString(oldNotification->url().string()).toUtf8().constData() : QString(oldNotification->contents().title()).toUtf8().constData());
        }
        if (notification->isHTML())
            printf("DESKTOP NOTIFICATION: contents at %s\n", QString(notification->url().string()).toUtf8().constData());
        else {
            printf("DESKTOP NOTIFICATION:%s icon %s, title %s, text %s\n",
                notification->dir() == "rtl" ? "(RTL)" : "",
                QString(notification->contents().icon().string()).toUtf8().constData(), QString(notification->contents().title()).toUtf8().constData(), 
                QString(notification->contents().body()).toUtf8().constData());
        }
    }

    if (iter != end) {
        sendEvent(iter.key(), eventNames().closeEvent);
        delete m_notifications.take(iter.key());
    }
    NotificationIconWrapper* wrapper = new NotificationIconWrapper();
    m_notifications.insert(notification, wrapper);
    sendEvent(notification, "display");

#ifndef QT_NO_SYSTEMTRAYICON
    wrapper->m_notificationIcon = new QSystemTrayIcon;
    wrapper->m_notificationIcon->show();
    wrapper->m_notificationIcon->showMessage(notification->contents().title(), notification->contents().body());
#endif
    return true;
}

void NotificationPresenterClientQt::cancel(Notification* notification)
{  
    if (dumpNotification) {
        if (notification->isHTML())
            printf("DESKTOP NOTIFICATION CLOSED: %s\n", QString(notification->url().string()).toUtf8().constData());
        else
            printf("DESKTOP NOTIFICATION CLOSED: %s\n", QString(notification->contents().title()).toUtf8().constData());
    }

    QHash <Notification*, NotificationIconWrapper*>::Iterator iter = m_notifications.find(notification);
    if (iter != m_notifications.end())
        sendEvent(iter.key(), eventNames().closeEvent);
}

void NotificationPresenterClientQt::notificationObjectDestroyed(Notification* notification)
{
    // Called from ~Notification(), Remove the entry from the notifications list and delete the icon.
    QHash <Notification*, NotificationIconWrapper*>::Iterator iter = m_notifications.find(notification);
    if (iter != m_notifications.end())
        delete m_notifications.take(notification);
}

void NotificationPresenterClientQt::requestPermission(SecurityOrigin* origin, PassRefPtr<VoidCallback> callback)
{  
    if (dumpNotification)
      printf("DESKTOP NOTIFICATION PERMISSION REQUESTED: %s\n", QString(origin->toString()).toUtf8().constData());

    QString originString = origin->toString();
    QHash<QString, QList<RefPtr<VoidCallback> > >::iterator iter = m_pendingPermissionRequests.find(originString);
    if (iter != m_pendingPermissionRequests.end())
        iter.value().append(callback);
    else {
        QList<RefPtr<VoidCallback> > callbacks;
        RefPtr<VoidCallback> cb = callback;
        callbacks.append(cb);
        m_pendingPermissionRequests.insert(originString, callbacks);
        if (requestPermissionFunction)
            requestPermissionFunction(m_receiver, m_page, originString);
    }
}

NotificationPresenter::Permission NotificationPresenterClientQt::checkPermission(const KURL& url)
{
    NotificationPermission permission = NotificationNotAllowed;
    QString origin = url.string();
    if (checkPermissionFunction)
        checkPermissionFunction(m_receiver, origin, permission);
    switch (permission) {
    case NotificationAllowed:
        return NotificationPresenter::PermissionAllowed;
    case NotificationNotAllowed:
        return NotificationPresenter::PermissionNotAllowed;
    case NotificationDenied:
        return NotificationPresenter::PermissionDenied;
    }
    ASSERT_NOT_REACHED();
    return NotificationPresenter::PermissionNotAllowed;
}

void NotificationPresenterClientQt::allowNotificationForOrigin(const QString& origin)
{
    QHash<QString, QList<RefPtr<VoidCallback> > >::iterator iter = m_pendingPermissionRequests.find(origin);
    if (iter != m_pendingPermissionRequests.end()) {
        QList<RefPtr<VoidCallback> >& callbacks = iter.value();
        for (int i = 0; i < callbacks.size(); i++)
            callbacks.at(i)->handleEvent();
        m_pendingPermissionRequests.remove(origin);
    }
}

void NotificationPresenterClientQt::clearNotificationsList()
{
    m_pendingPermissionRequests.clear();
    while (!m_notifications.isEmpty()) {
        QHash <Notification*, NotificationIconWrapper*>::Iterator iter = m_notifications.begin();
        delete m_notifications.take(iter.key());
    }
}

void NotificationPresenterClientQt::sendEvent(Notification* notification, const AtomicString& eventName)
{
    RefPtr<Event> event = Event::create(eventName, false, true);
    notification->dispatchEvent(event.release());
}

#endif // ENABLE(NOTIFICATIONS)
