/*
 * Copyright (C) 2014 Haiku, Inc. All rights reserved.
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

#ifndef NotificationClientHaiku_h
#define NotificationClientHaiku_h

#include "NotificationClient.h"
#include "Notification.h"
#include "NotificationPermissionCallback.h"
#include <support/Locker.h>
#include <app/Notification.h>

#include <Bitmap.h>
#include <TranslationUtils.h>
#include <UrlProtocolRoster.h>
#include <UrlSynchronousRequest.h>

class BWebPage;

namespace WebCore {

class NotificationClientHaiku: public NotificationClient {
public:
    NotificationClientHaiku(BWebPage*) {}

    bool show(Notification* descriptor) override {

        // FIXME should call dispatch{Click/Close/Error/Show}Event
        fromDescriptor(descriptor).Send();
        return true;
    }

    void cancel(Notification* descriptor) override {
        fromDescriptor(descriptor).Send(0); // 0 timeout = destroy now
    }

    void notificationObjectDestroyed(Notification*) override {}
    void notificationControllerDestroyed() override {}

    void requestPermission(ScriptExecutionContext*, 
            RefPtr<NotificationPermissionCallback>&& callback) override {
        if (callback)
            callback->handleEvent(NotificationPermission::Granted);
    }
    void cancelRequestsForPermission(ScriptExecutionContext*) override {}
    bool hasPendingPermissionRequests(WebCore::ScriptExecutionContext*) const override { return false; }

    Permission checkPermission(ScriptExecutionContext*) override {
        return NotificationPermission::Granted;
    }

private:
    class SynchronousListener;

    BNotification fromDescriptor(Notification* descriptor);
};

}

#endif
