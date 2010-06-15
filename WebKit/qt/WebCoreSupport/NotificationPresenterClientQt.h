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

#ifndef NotificationPresenterClientQt_h
#define NotificationPresenterClientQt_h

#include "Notification.h"
#include "NotificationPresenter.h"
#include "QtPlatformPlugin.h"
#include "Timer.h"

#include "qwebkitplatformplugin.h"

#include <QMultiHash>
#include <QSystemTrayIcon>

namespace WebCore {

class Document;
class KURL;

class NotificationIconWrapper : public QObject, public QWebNotificationData {
    Q_OBJECT
public:
    NotificationIconWrapper();
    ~NotificationIconWrapper() {}

    void close();
    void close(Timer<NotificationIconWrapper>*);
    const QString title() const;
    const QString message() const;
    const QByteArray iconData() const;

public Q_SLOTS:
    void notificationClosed();

public:
#ifndef QT_NO_SYSTEMTRAYICON
    OwnPtr<QSystemTrayIcon> m_notificationIcon;
#endif

    OwnPtr<QWebNotificationPresenter> m_presenter;
    Timer<NotificationIconWrapper> m_closeTimer;
};

#if ENABLE(NOTIFICATIONS)

typedef QHash <Notification*, NotificationIconWrapper*> NotificationsQueue;

class NotificationPresenterClientQt : public NotificationPresenter {
public:
    NotificationPresenterClientQt();
    ~NotificationPresenterClientQt();

    /* WebCore::NotificationPresenter interface */
    virtual bool show(Notification*);
    virtual void cancel(Notification*);
    virtual void notificationObjectDestroyed(Notification*);
    virtual void requestPermission(SecurityOrigin*, PassRefPtr<VoidCallback>);
    virtual NotificationPresenter::Permission checkPermission(const KURL&);

    void cancel(NotificationIconWrapper*);

    void allowNotificationForOrigin(const QString& origin);

    static bool dumpNotification;

    void setReceiver(QObject* receiver) { m_receiver = receiver; }

    void addClient() { m_clientCount++; }
    void removeClient();
    static NotificationPresenterClientQt* notificationPresenter();

    Notification* notificationForWrapper(const NotificationIconWrapper*) const;

private:
    void sendEvent(Notification*, const AtomicString& eventName);
    void displayNotification(Notification*, const QByteArray&);
    void removeReplacedNotificationFromQueue(Notification*);
    void detachNotification(Notification*);
    void dumpReplacedIdText(Notification*);
    void dumpShowText(Notification*);

    int m_clientCount;
    QHash<QString,  QList<RefPtr<VoidCallback> > > m_pendingPermissionRequests;
    NotificationsQueue m_notifications;
    QObject* m_receiver;
    QtPlatformPlugin m_platformPlugin;
};

#endif

}

#endif
