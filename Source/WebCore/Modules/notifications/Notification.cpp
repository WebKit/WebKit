/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009, 2011, 2012, 2016, 2022 Apple Inc. All rights reserved.
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

#if ENABLE(NOTIFICATIONS)

#include "Notification.h"

#include "Event.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "NotificationClient.h"
#include "NotificationData.h"
#include "NotificationPermissionCallback.h"
#include "WindowEventLoop.h"
#include "WindowFocusAllowedIndicator.h"
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Notification);

Ref<Notification> Notification::create(ScriptExecutionContext& context, const String& title, const Options& options)
{
    auto notification = adoptRef(*new Notification(context, title, options));
    notification->suspendIfNeeded();
    notification->showSoon();
    return notification;
}

Notification::Notification(ScriptExecutionContext& context, const String& title, const Options& options)
    : ActiveDOMObject(&context)
    , m_title(title.isolatedCopy())
    , m_direction(options.dir)
    , m_lang(options.lang.isolatedCopy())
    , m_body(options.body.isolatedCopy())
    , m_tag(options.tag.isolatedCopy())
    , m_state(Idle)
{
    if (context.isDocument())
        m_notificationSource = NotificationSource::Document;
    else if (context.isServiceWorkerGlobalScope())
        m_notificationSource = NotificationSource::ServiceWorker;
    else
        RELEASE_ASSERT_NOT_REACHED();

    if (!options.icon.isEmpty()) {
        auto iconURL = context.completeURL(options.icon);
        if (iconURL.isValid())
            m_icon = iconURL;
    }
}

Notification::~Notification() = default;


void Notification::showSoon()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::UserInteraction, [this] {
        show();
    });
}

void Notification::show()
{
    // prevent double-showing
    if (m_state != Idle)
        return;

    auto* client = clientFromContext();
    if (!client)
        return;

    if (client->checkPermission(scriptExecutionContext()) != Permission::Granted) {
        dispatchErrorEvent();
        return;
    }
    if (client->show(*this))
        m_state = Showing;
}

void Notification::close()
{
    switch (m_state) {
    case Idle:
        break;
    case Showing: {
        if (auto* client = clientFromContext())
            client->cancel(*this);
        break;
    }
    case Closed:
        break;
    }
}

NotificationClient* Notification::clientFromContext()
{
    if (auto* context = scriptExecutionContext())
        return context->notificationClient();
    return nullptr;
}

const char* Notification::activeDOMObjectName() const
{
    return "Notification";
}

void Notification::stop()
{
    ActiveDOMObject::stop();

    if (auto* client = clientFromContext())
        client->notificationObjectDestroyed(*this);
}

void Notification::suspend(ReasonForSuspension)
{
    close();
}

void Notification::finalize()
{
    if (m_state == Closed)
        return;
    m_state = Closed;
}

void Notification::dispatchShowEvent()
{
    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, Event::create(eventNames().showEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void Notification::dispatchClickEvent()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::UserInteraction, [this] {
        WindowFocusAllowedIndicator windowFocusAllowed;
        dispatchEvent(Event::create(eventNames().clickEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void Notification::dispatchCloseEvent()
{
    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    finalize();
}

void Notification::dispatchErrorEvent()
{
    ASSERT(isMainThread());
    ASSERT(m_notificationSource == NotificationSource::Document);

    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

auto Notification::permission(ScriptExecutionContext& context) -> Permission
{
    auto* client = context.notificationClient();
    if (!client)
        return Permission::Default;

    if (!context.isSecureContext())
        return Permission::Denied;

    return client->checkPermission(&context);
}

void Notification::requestPermission(Document& document, RefPtr<NotificationPermissionCallback>&& callback, Ref<DeferredPromise>&& promise)
{
    auto resolvePromiseAndCallback = [document = Ref { document }, callback = WTFMove(callback), promise = WTFMove(promise)](Permission permission) mutable {
        document->eventLoop().queueTask(TaskSource::DOMManipulation, [callback = WTFMove(callback), promise = WTFMove(promise), permission]() mutable {
            if (callback)
                callback->handleEvent(permission);
            promise->resolve<IDLEnumeration<NotificationPermission>>(permission);
        });
    };

    auto* client = static_cast<ScriptExecutionContext&>(document).notificationClient();
    if (!client)
        return resolvePromiseAndCallback(Permission::Denied);

    if (!document.isSecureContext()) {
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, "The Notification permission may only be requested in a secure context."_s);
        return resolvePromiseAndCallback(Permission::Denied);
    }

    client->requestPermission(document, WTFMove(resolvePromiseAndCallback));
}

void Notification::eventListenersDidChange()
{
    m_hasRelevantEventListener = hasEventListeners(eventNames().clickEvent)
        || hasEventListeners(eventNames().closeEvent)
        || hasEventListeners(eventNames().errorEvent)
        || hasEventListeners(eventNames().showEvent);
}

// A Notification object must not be garbage collected while the list of notifications contains its corresponding
// notification and it has an event listener whose type is click, show, close, or error.
// See https://notifications.spec.whatwg.org/#garbage-collection
bool Notification::virtualHasPendingActivity() const
{
    return m_state == Showing && m_hasRelevantEventListener;
}

NotificationData Notification::data() const
{
    auto sessionID = scriptExecutionContext()->sessionID();
    RELEASE_ASSERT(sessionID);

    return {
        m_title.isolatedCopy(),
        m_body.isolatedCopy(),
        m_icon.string().isolatedCopy(),
        m_tag,
        m_lang,
        m_direction,
        scriptExecutionContext()->securityOrigin()->toString().isolatedCopy(),
        identifier(),
        *sessionID,
    };
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
