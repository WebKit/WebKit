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

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "NotificationClient.h"
#include "Timer.h"
#include "URL.h"
#include "WritingMode.h"

namespace WebCore {

class Document;
class NotificationCenter;
class NotificationPermissionCallback;

class Notification final : public RefCounted<Notification>, public ActiveDOMObject, public EventTargetWithInlineData {
    WTF_MAKE_FAST_ALLOCATED;
public:
#if ENABLE(LEGACY_NOTIFICATIONS)
    static ExceptionOr<Ref<Notification>> create(const String& title, const String& body, const String& iconURL, ScriptExecutionContext&, NotificationCenter&);
#endif

#if ENABLE(NOTIFICATIONS)
    enum class Direction { Auto, Ltr, Rtl };
    struct Options {
        Direction dir;
        String lang;
        String body;
        String tag;
        String icon;
    };
    static Ref<Notification> create(Document&, const String& title, const Options&);
#endif
    
    virtual ~Notification();

    void show();
    void close();

    const URL& iconURL() const { return m_icon; }
    const String& title() const { return m_title; }
    const String& body() const { return m_body; }
    const String& lang() const { return m_lang; }

    const String& dir() const { return m_direction; }
    void setDir(const String& dir) { m_direction = dir; }

    const String& replaceId() const { return m_tag; }
    void setReplaceId(const String& replaceId) { m_tag = replaceId; }

    const String& tag() const { return m_tag; }
    void setTag(const String& tag) { m_tag = tag; }

    TextDirection direction() const { return m_direction == "rtl" ? RTL : LTR; }

    WEBCORE_EXPORT void dispatchClickEvent();
    WEBCORE_EXPORT void dispatchCloseEvent();
    WEBCORE_EXPORT void dispatchErrorEvent();
    WEBCORE_EXPORT void dispatchShowEvent();

    WEBCORE_EXPORT void finalize();

#if ENABLE(NOTIFICATIONS)
    static String permission(Document&);
    WEBCORE_EXPORT static String permissionString(NotificationClient::Permission);
    static void requestPermission(Document&, RefPtr<NotificationPermissionCallback>&&);
#endif

    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

private:
#if ENABLE(LEGACY_NOTIFICATIONS)
    Notification(const String& title, const String& body, URL&& iconURL, ScriptExecutionContext&, NotificationCenter&);
#endif

#if ENABLE(NOTIFICATIONS)
    Notification(Document&, const String& title);
#endif

    EventTargetInterface eventTargetInterface() const final { return NotificationEventTargetInterfaceType; }

    void contextDestroyed() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    URL m_icon;
    String m_title;
    String m_body;
    String m_direction;
    String m_lang;
    String m_tag;

    enum State { Idle, Showing, Closed };
    State m_state { Idle };

    RefPtr<NotificationCenter> m_notificationCenter;

#if ENABLE(NOTIFICATIONS)
    std::unique_ptr<Timer> m_taskTimer;
#endif
};

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
