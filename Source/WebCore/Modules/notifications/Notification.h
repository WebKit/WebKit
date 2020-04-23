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

#pragma once

#if ENABLE(NOTIFICATIONS)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "NotificationDirection.h"
#include "NotificationPermission.h"
#include "SuspendableTimer.h"
#include <wtf/URL.h>
#include "WritingMode.h"

namespace WebCore {

class Document;
class NotificationPermissionCallback;

class Notification final : public RefCounted<Notification>, public ActiveDOMObject, public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(Notification, WEBCORE_EXPORT);
public:
    using Permission = NotificationPermission;
    using Direction = NotificationDirection;

    struct Options {
        Direction dir;
        String lang;
        String body;
        String tag;
        String icon;
    };
    static Ref<Notification> create(Document&, const String& title, const Options&);
    
    WEBCORE_EXPORT virtual ~Notification();

    void show();
    void close();

    const String& title() const { return m_title; }
    Direction dir() const { return m_direction; }
    const String& body() const { return m_body; }
    const String& lang() const { return m_lang; }
    const String& tag() const { return m_tag; }
    const URL& icon() const { return m_icon; }

    TextDirection direction() const { return m_direction == Direction::Rtl ? TextDirection::RTL : TextDirection::LTR; }

    WEBCORE_EXPORT void dispatchClickEvent();
    WEBCORE_EXPORT void dispatchCloseEvent();
    WEBCORE_EXPORT void dispatchErrorEvent();
    WEBCORE_EXPORT void dispatchShowEvent();

    WEBCORE_EXPORT void finalize();

    static Permission permission(Document&);
    static void requestPermission(Document&, RefPtr<NotificationPermissionCallback>&&);

    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

private:
    Notification(Document&, const String& title, const Options&);

    Document* document() const;
    EventTargetInterface eventTargetInterface() const final { return NotificationEventTargetInterfaceType; }

    void queueTask(Function<void()>&&);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    void suspend(ReasonForSuspension);
    void stop() final;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    String m_title;
    Direction m_direction;
    String m_lang;
    String m_body;
    String m_tag;
    URL m_icon;

    enum State { Idle, Showing, Closed };
    State m_state { Idle };

    SuspendableTimer m_showNotificationTimer;
};

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
