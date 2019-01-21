/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009, 2011, 2012, 2016 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "NotificationClient.h"
#include "NotificationController.h"
#include "NotificationPermissionCallback.h"
#include "WindowFocusAllowedIndicator.h"

namespace WebCore {

Ref<Notification> Notification::create(Document& context, const String& title, const Options& options)
{
    auto notification = adoptRef(*new Notification(context, title, options));
    notification->suspendIfNeeded();
    return notification;
}

Notification::Notification(Document& document, const String& title, const Options& options)
    : ActiveDOMObject(&document)
    , m_title(title)
    , m_direction(options.dir)
    , m_lang(options.lang)
    , m_body(options.body)
    , m_tag(options.tag)
    , m_state(Idle)
    , m_taskTimer(std::make_unique<Timer>([this] () { show(); }))
{
    if (!options.icon.isEmpty()) {
        auto iconURL = document.completeURL(options.icon);
        if (iconURL.isValid())
            m_icon = iconURL;
    }

    m_taskTimer->startOneShot(0_s);
}

Notification::~Notification()  = default;

void Notification::show()
{
    // prevent double-showing
    if (m_state != Idle)
        return;

    auto* page = downcast<Document>(*scriptExecutionContext()).page();
    if (!page)
        return;

    auto& client = NotificationController::from(page)->client();

    if (client.checkPermission(scriptExecutionContext()) != Permission::Granted) {
        dispatchErrorEvent();
        return;
    }
    if (client.show(this)) {
        m_state = Showing;
        setPendingActivity(*this);
    }
}

void Notification::close()
{
    switch (m_state) {
    case Idle:
        break;
    case Showing: {
        auto* page = downcast<Document>(*scriptExecutionContext()).page();
        if (page)
            NotificationController::from(page)->client().cancel(this);
        break;
    }
    case Closed:
        break;
    }
}

const char* Notification::activeDOMObjectName() const
{
    return "Notification";
}

bool Notification::canSuspendForDocumentSuspension() const
{
    // We can suspend if the Notification is not shown yet or after it is closed.
    return m_state == Idle || m_state == Closed;
}

void Notification::stop()
{
    ActiveDOMObject::stop();

    auto* page = downcast<Document>(*scriptExecutionContext()).page();
    if (page)
        NotificationController::from(page)->client().notificationObjectDestroyed(this);
}

void Notification::finalize()
{
    if (m_state == Closed)
        return;
    m_state = Closed;
    unsetPendingActivity(*this);
}

void Notification::dispatchShowEvent()
{
    dispatchEvent(Event::create(eventNames().showEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void Notification::dispatchClickEvent()
{
    WindowFocusAllowedIndicator windowFocusAllowed;
    dispatchEvent(Event::create(eventNames().clickEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void Notification::dispatchCloseEvent()
{
    dispatchEvent(Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    finalize();
}

void Notification::dispatchErrorEvent()
{
    dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

auto Notification::permission(Document& document) -> Permission
{
    return NotificationController::from(document.page())->client().checkPermission(&document);
}

void Notification::requestPermission(Document& document, RefPtr<NotificationPermissionCallback>&& callback)
{
    NotificationController::from(document.page())->client().requestPermission(&document, WTFMove(callback));
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
