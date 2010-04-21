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

#include "Document.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "SecurityOrigin.h"

#include "qwebkitglobal.h"

#include <QtGui>

static bool dumpNotification = false;

void QWEBKIT_EXPORT qt_dump_notification(bool b)
{
    dumpNotification = b;
}

#if ENABLE(NOTIFICATIONS)

using namespace WebCore;

NotificationPresenterClientQt::NotificationPresenterClientQt()
{
}

bool NotificationPresenterClientQt::show(Notification* notification)
{
    if (dumpNotification) {
        if (notification->isHTML())
            printf("DESKTOP NOTIFICATION: contents at %s\n", QString(notification->url().string()).toUtf8().constData());
        else {
            printf("DESKTOP NOTIFICATION: icon %s, title %s, text %s\n", 
                QString(notification->contents().icon().string()).toUtf8().constData(), QString(notification->contents().title()).toUtf8().constData(), 
                QString(notification->contents().body()).toUtf8().constData());
        }
    }

#ifndef QT_NO_SYSTEMTRAYICON
    m_tray.show();
    m_tray.showMessage(notification->contents().title(), notification->contents().body(), QSystemTrayIcon::Information);
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

    notImplemented();
}

void NotificationPresenterClientQt::notificationObjectDestroyed(Notification* notification)
{
    notImplemented();
}

void NotificationPresenterClientQt::requestPermission(SecurityOrigin* origin, PassRefPtr<VoidCallback> callback)
{  
    if (dumpNotification)
      printf("DESKTOP NOTIFICATION PERMISSION REQUESTED: %s\n", QString(origin->toString()).toUtf8().constData());

    notImplemented();
}

NotificationPresenter::Permission NotificationPresenterClientQt::checkPermission(const KURL&)
{
    // FIXME Implement permission policy
    return NotificationPresenter::PermissionAllowed;
}

#endif // ENABLE(NOTIFICATIONS)
