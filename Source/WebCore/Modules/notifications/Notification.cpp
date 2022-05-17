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

#include "DOMWindow.h"
#include "Event.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "MessagePort.h"
#include "NotificationClient.h"
#include "NotificationData.h"
#include "NotificationEvent.h"
#include "NotificationPermissionCallback.h"
#include "ServiceWorkerGlobalScope.h"
#include "WindowEventLoop.h"
#include "WindowFocusAllowedIndicator.h"
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Scope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Notification);

static ExceptionOr<Ref<SerializedScriptValue>> createSerializedScriptValue(ScriptExecutionContext& context, JSC::JSValue value)
{
    auto globalObject = context.globalObject();
    if (!globalObject)
        return Exception { TypeError, "Notification cannot be created without a global object"_s };

    Vector<RefPtr<MessagePort>> dummyPorts;
    return SerializedScriptValue::create(*globalObject, value, { }, dummyPorts);
}

ExceptionOr<Ref<Notification>> Notification::create(ScriptExecutionContext& context, String&& title, Options&& options)
{
    if (context.isServiceWorkerGlobalScope())
        return Exception { TypeError, "Notification cannot be directly created in a ServiceWorkerGlobalScope"_s };

    auto dataResult = createSerializedScriptValue(context, options.data);
    if (dataResult.hasException())
        return dataResult.releaseException();

    auto notification = adoptRef(*new Notification(context, UUID::createVersion4(), WTFMove(title), WTFMove(options), dataResult.releaseReturnValue()));
    notification->suspendIfNeeded();
    notification->showSoon();
    return notification;
}

ExceptionOr<Ref<Notification>> Notification::createForServiceWorker(ScriptExecutionContext& context, String&& title, Options&& options, const URL& serviceWorkerRegistrationURL)
{
    auto dataResult = createSerializedScriptValue(context, options.data);
    if (dataResult.hasException())
        return dataResult.releaseException();

    auto notification = adoptRef(*new Notification(context, UUID::createVersion4(), WTFMove(title), WTFMove(options), dataResult.releaseReturnValue()));
    notification->m_serviceWorkerRegistrationURL = serviceWorkerRegistrationURL;
    notification->suspendIfNeeded();
    return notification;
}

Ref<Notification> Notification::create(ScriptExecutionContext& context, NotificationData&& data)
{
    Options options { data.direction, WTFMove(data.language), WTFMove(data.body), WTFMove(data.tag), WTFMove(data.iconURL), JSC::jsNull() };

    auto notification = adoptRef(*new Notification(context, data.notificationID, WTFMove(data.title), WTFMove(options), SerializedScriptValue::createFromWireBytes(WTFMove(data.data))));
    notification->suspendIfNeeded();
    notification->m_serviceWorkerRegistrationURL = WTFMove(data.serviceWorkerRegistrationURL);
    return notification;
}

Notification::Notification(ScriptExecutionContext& context, UUID identifier, String&& title, Options&& options, Ref<SerializedScriptValue>&& dataForBindings)
    : ActiveDOMObject(&context)
    , m_identifier(identifier)
    , m_title(WTFMove(title).isolatedCopy())
    , m_direction(options.dir)
    , m_lang(WTFMove(options.lang).isolatedCopy())
    , m_body(WTFMove(options.body).isolatedCopy())
    , m_tag(WTFMove(options.tag).isolatedCopy())
    , m_dataForBindings(WTFMove(dataForBindings))
    , m_state(Idle)
    , m_contextIdentifier(context.identifier())
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

Notification::~Notification()
{
}

void Notification::showSoon()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::UserInteraction, [this] {
        show();
    });
}

void Notification::markAsShown()
{
    m_state = Showing;
}

void Notification::show(CompletionHandler<void()>&& callback)
{
    CompletionHandlerCallingScope scope { WTFMove(callback) };

    // prevent double-showing
    if (m_state != Idle)
        return;

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* client = context->notificationClient();
    if (!client)
        return;

    if (client->checkPermission(context) != Permission::Granted) {
        switch (m_notificationSource) {
        case NotificationSource::Document:
            dispatchErrorEvent();
            break;
        case NotificationSource::ServiceWorker:
            // We did a permission check when ServiceWorkerRegistration::showNotification() was called.
            // If permission has since been revoked, then silently failing here is expected behavior.
            break;
        }
        return;
    }

    if (client->show(*this, scope.release()))
        m_state = Showing;
}

void Notification::close()
{
    switch (m_state) {
    case Idle:
        break;
    case Showing:
        if (auto* client = clientFromContext())
            client->cancel(*this);
        break;
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

    if (!m_serviceWorkerRegistrationURL.isNull())
        return;

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
    ASSERT(isMainThread());
    ASSERT(!isPersistent());

    if (m_notificationSource != NotificationSource::Document)
        return;

    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, Event::create(eventNames().showEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void Notification::dispatchClickEvent()
{
    ASSERT(isMainThread());
    ASSERT(m_notificationSource == NotificationSource::Document);
    ASSERT(!isPersistent());

    queueTaskKeepingObjectAlive(*this, TaskSource::UserInteraction, [this] {
        WindowFocusAllowedIndicator windowFocusAllowed;
        dispatchEvent(Event::create(eventNames().clickEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void Notification::dispatchCloseEvent()
{
    ASSERT(isMainThread());
    ASSERT(m_notificationSource == NotificationSource::Document);
    ASSERT(!isPersistent());

    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    finalize();
}

void Notification::dispatchErrorEvent()
{
    ASSERT(isMainThread());
    ASSERT(m_notificationSource == NotificationSource::Document);
    ASSERT(!isPersistent());

    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

JSC::JSValue Notification::dataForBindings(JSC::JSGlobalObject& globalObject)
{
    return m_dataForBindings->deserialize(globalObject, &globalObject, SerializationErrorMode::NonThrowing);
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

    auto* window = document.frame() ? document.frame()->window() : nullptr;
    if (!window || !window->consumeTransientActivation()) {
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Notification prompting can only be done from a user gesture."_s);
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
    auto& context = *scriptExecutionContext();
    auto sessionID = context.sessionID();
    RELEASE_ASSERT(sessionID);

    return {
        m_title.isolatedCopy(),
        m_body.isolatedCopy(),
        m_icon.string().isolatedCopy(),
        m_tag,
        m_lang,
        m_direction,
        scriptExecutionContext()->securityOrigin()->toString().isolatedCopy(),
        m_serviceWorkerRegistrationURL.isolatedCopy(),
        identifier(),
        *sessionID,
        MonotonicTime::now(),
        m_dataForBindings->wireBytes()
    };
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
