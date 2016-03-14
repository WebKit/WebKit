/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef Notification_h  
#define Notification_h

#include "ActiveDOMObject.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "NotificationClient.h"
#include "ThreadableLoaderClient.h"
#include "URL.h"
#include "WritingMode.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicStringHash.h>

#if ENABLE(NOTIFICATIONS)
#include "Timer.h"
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
namespace WebCore {

class Dictionary;
class Document;
class NotificationCenter;
class NotificationPermissionCallback;
class ResourceError;
class ResourceResponse;
class ThreadableLoader;

typedef int ExceptionCode;

class Notification final : public RefCounted<Notification>, public ActiveDOMObject, public EventTargetWithInlineData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT Notification();
#if ENABLE(LEGACY_NOTIFICATIONS)
    static Ref<Notification> create(const String& title, const String& body, const String& iconURI, ScriptExecutionContext&, ExceptionCode&, PassRefPtr<NotificationCenter> provider);
#endif
#if ENABLE(NOTIFICATIONS)
    static Ref<Notification> create(Document&, const String& title, const Dictionary& options);
#endif
    
    WEBCORE_EXPORT virtual ~Notification();

    void show();
#if ENABLE(LEGACY_NOTIFICATIONS)
    void cancel() { close(); }
#endif
    void close();

    URL iconURL() const { return m_icon; }
    void setIconURL(const URL& url) { m_icon = url; }

    String title() const { return m_title; }
    String body() const { return m_body; }

    String lang() const { return m_lang; }
    void setLang(const String& lang) { m_lang = lang; }

    String dir() const { return m_direction; }
    void setDir(const String& dir) { m_direction = dir; }

#if ENABLE(LEGACY_NOTIFICATIONS)
    String replaceId() const { return tag(); }
    void setReplaceId(const String& replaceId) { setTag(replaceId); }
#endif

    String tag() const { return m_tag; }
    void setTag(const String& tag) { m_tag = tag; }

    TextDirection direction() const { return dir() == "rtl" ? RTL : LTR; }

    WEBCORE_EXPORT void dispatchClickEvent();
    WEBCORE_EXPORT void dispatchCloseEvent();
    WEBCORE_EXPORT void dispatchErrorEvent();
    WEBCORE_EXPORT void dispatchShowEvent();

    using RefCounted<Notification>::ref;
    using RefCounted<Notification>::deref;

    // EventTarget interface
    EventTargetInterface eventTargetInterface() const override { return NotificationEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }

    void stopLoadingIcon();

    // Deprecated. Use functions from NotificationCenter.
    void detachPresenter() { }

    WEBCORE_EXPORT void finalize();

#if ENABLE(NOTIFICATIONS)
    static const String permission(Document&);
    WEBCORE_EXPORT static const String permissionString(NotificationClient::Permission);
    static void requestPermission(Document&, PassRefPtr<NotificationPermissionCallback> = nullptr);
#endif

private:
#if ENABLE(LEGACY_NOTIFICATIONS)
    Notification(const String& title, const String& body, const String& iconURI, ScriptExecutionContext&, ExceptionCode&, PassRefPtr<NotificationCenter>);
#endif
#if ENABLE(NOTIFICATIONS)
    Notification(ScriptExecutionContext&, const String& title);
#endif

    void setBody(const String& body) { m_body = body; }

    // ActiveDOMObject API.
    void contextDestroyed() override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;

    // EventTarget API.
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    void startLoadingIcon();
    void finishLoadingIcon();

#if ENABLE(NOTIFICATIONS)
    void taskTimerFired();
#endif

    // Text notifications.
    URL m_icon;
    String m_title;
    String m_body;
    String m_direction;
    String m_lang;
    String m_tag;

    enum NotificationState {
        Idle = 0,
        Showing = 1,
        Closed = 2,
    };

    NotificationState m_state;

    RefPtr<NotificationCenter> m_notificationCenter;

#if ENABLE(NOTIFICATIONS)
    std::unique_ptr<Timer> m_taskTimer;
#endif
};

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#endif // Notifications_h
