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

#include "config.h"
#include "NotificationPresenterImpl.h"

#if ENABLE(NOTIFICATIONS)

#include "KURL.h"
#include "Notification.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

#include "WebNotification.h"
#include "WebNotificationPermissionCallback.h"
#include "WebNotificationPresenter.h"
#include "platform/WebURL.h"

#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

class VoidCallbackClient : public WebNotificationPermissionCallback {
public:
    VoidCallbackClient(PassRefPtr<VoidCallback> callback)
        : m_callback(callback)
    {
    }

    virtual void permissionRequestComplete()
    {
        if (m_callback)
            m_callback->handleEvent();
        delete this;
    }

private:
    virtual ~VoidCallbackClient() { }

    RefPtr<VoidCallback> m_callback;
};

void NotificationPresenterImpl::initialize(WebNotificationPresenter* presenter)
{
    m_presenter = presenter;
}

bool NotificationPresenterImpl::isInitialized()
{
    return !!m_presenter;
}

bool NotificationPresenterImpl::show(Notification* notification)
{
    return m_presenter->show(PassRefPtr<Notification>(notification));
}

void NotificationPresenterImpl::cancel(Notification* notification)
{
    m_presenter->cancel(PassRefPtr<Notification>(notification));
}

void NotificationPresenterImpl::notificationObjectDestroyed(Notification* notification)
{
    m_presenter->objectDestroyed(PassRefPtr<Notification>(notification));
}

void NotificationPresenterImpl::notificationControllerDestroyed()
{
}

NotificationPresenter::Permission NotificationPresenterImpl::checkPermission(ScriptExecutionContext* context)
{
    int result = m_presenter->checkPermission(WebSecurityOrigin(context->securityOrigin()));
    return static_cast<NotificationPresenter::Permission>(result);
}

void NotificationPresenterImpl::requestPermission(ScriptExecutionContext* context, PassRefPtr<VoidCallback> callback)
{
    m_presenter->requestPermission(WebSecurityOrigin(context->securityOrigin()), new VoidCallbackClient(callback));
}

} // namespace WebKit

#endif // ENABLE(NOTIFICATIONS)
