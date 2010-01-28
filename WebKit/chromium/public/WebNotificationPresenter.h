/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebNotificationPresenter_h
#define WebNotificationPresenter_h

#include "WebString.h"

namespace WebKit {

class WebDocument;
class WebNotification;
class WebNotificationPermissionCallback;
class WebURL;

// Provides the services to show desktop notifications to the user.
class WebNotificationPresenter {
public:
    enum Permission {
        PermissionAllowed,     // User has allowed permission to the origin.
        PermissionNotAllowed,  // User has not taken an action on the origin (defaults to not allowed).
        PermissionDenied       // User has explicitly denied permission from the origin.
    };

    // Shows a notification.
    virtual bool show(const WebNotification&) = 0;

    // Cancels a notification previously shown, and removes it if being shown.
    virtual void cancel(const WebNotification&) = 0;

    // Indiciates that the notification object subscribed to events for a previously shown notification is
    // being destroyed.  Does _not_ remove the notification if being shown, but detaches it from receiving events.
    virtual void objectDestroyed(const WebNotification&) = 0;

    // Checks the permission level for the given URL. If the URL is being displayed in a document
    // (as opposed to a worker or other ScriptExecutionContext), |document| will also be provided.
    virtual Permission checkPermission(const WebURL& url, WebDocument* document) = 0;

    // Requests permission for a given origin.  This operation is asynchronous and the callback provided
    // will be invoked when the permission decision is made.  Callback pointer must remain
    // valid until called.
    virtual void requestPermission(const WebString& origin, WebNotificationPermissionCallback* callback) = 0;
};

} // namespace WebKit

#endif
