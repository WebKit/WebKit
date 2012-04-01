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
#include "KURL.h"
#include "SharedBuffer.h"
#include "TextDirection.h"
#include "ThreadableLoaderClient.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicStringHash.h>

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
namespace WebCore {

class NotificationCenter;
class ResourceError;
class ResourceResponse;
class ScriptExecutionContext;
class ThreadableLoader;

typedef int ExceptionCode;

class Notification : public RefCounted<Notification>, public ActiveDOMObject, public ThreadableLoaderClient, public EventTarget {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Notification();
    static PassRefPtr<Notification> create(const KURL&, ScriptExecutionContext*, ExceptionCode&, PassRefPtr<NotificationCenter> provider);
    static PassRefPtr<Notification> create(const String& title, const String& body, const String& iconURI, ScriptExecutionContext*, ExceptionCode&, PassRefPtr<NotificationCenter> provider);
    
    virtual ~Notification();

    void show();
#if ENABLE(LEGACY_NOTIFICATIONS)
    void cancel() { close(); }
#endif
    void close();

    bool isHTML() const { return m_isHTML; }
    void setHTML(bool isHTML) { m_isHTML = isHTML; }
    
    KURL url() const { return m_notificationURL; }
    void setURL(KURL url) { m_notificationURL = url; }
    
    KURL iconURL() const { return m_icon; }
    String title() const { return m_title; }
    String body() const { return m_body; }

    String dir() const { return m_direction; }
    void setDir(const String& dir) { m_direction = dir; }

#if ENABLE(LEGACY_NOTIFICATIONS)
    String replaceId() const { return tag(); }
    void setReplaceId(const String& replaceId) { setTag(replaceId); }
#endif

    String tag() const { return m_tag; }
    void setTag(const String& tag) { m_tag = tag; }

    TextDirection direction() const { return dir() == "rtl" ? RTL : LTR; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(show);
#if ENABLE(LEGACY_NOTIFICATIONS)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(display);
#endif
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(click);
    
    void dispatchClickEvent();
    void dispatchCloseEvent();
    void dispatchErrorEvent();
    void dispatchShowEvent();

    using RefCounted<Notification>::ref;
    using RefCounted<Notification>::deref;

    // EventTarget interface
    virtual const AtomicString& interfaceName() const;
    virtual ScriptExecutionContext* scriptExecutionContext() const { return ActiveDOMObject::scriptExecutionContext(); }

    // ActiveDOMObject interface
    virtual void contextDestroyed();

    void stopLoading();

    SharedBuffer* iconData() { return m_iconData.get(); }
    void releaseIconData() { m_iconData = 0; }

    // Deprecated. Use functions from NotificationCenter.
    void detachPresenter() { }

    virtual void didReceiveResponse(unsigned long, const ResourceResponse&);
    virtual void didReceiveData(const char* data, int dataLength);
    virtual void didFinishLoading(unsigned long identifier, double finishTime);
    virtual void didFail(const ResourceError&);
    virtual void didFailRedirectCheck();

private:
    Notification(const KURL&, ScriptExecutionContext*, ExceptionCode&, PassRefPtr<NotificationCenter>);
    Notification(const String& title, const String& body, const String& iconURI, ScriptExecutionContext*, ExceptionCode&, PassRefPtr<NotificationCenter>);

    // EventTarget interface
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

    void startLoading();
    void finishLoading();

    bool m_isHTML;

    // Text notifications.
    KURL m_icon;
    String m_title;
    String m_body;
    // FIXME: Deprecate HTML Notifications.
    KURL m_notificationURL;

    String m_direction;
    String m_tag;

    enum NotificationState {
        Idle = 0,
        Loading = 1,
        Showing = 2,
        Cancelled = 3
    };

    NotificationState m_state;

    RefPtr<NotificationCenter> m_notificationCenter;
    
    EventTargetData m_eventTargetData;

    RefPtr<ThreadableLoader> m_loader;
    RefPtr<SharedBuffer> m_iconData;
};

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#endif // Notifications_h
