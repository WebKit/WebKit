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

#ifndef NotificationCenter_h
#define NotificationCenter_h

#include "Notification.h"
#include "NotificationContents.h"
#include "WorkerThread.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(NOTIFICATIONS)

namespace WebCore {

    class ScriptExecutionContext;

    class NotificationCenter : public RefCounted<NotificationCenter>, public ActiveDOMObject { 
    public:
#if USE(V8)
        static void setIsAvailable(bool);
        static bool isAvailable();
#endif
        static PassRefPtr<NotificationCenter> create(ScriptExecutionContext* context, NotificationPresenter* presenter) { return adoptRef(new NotificationCenter(context, presenter)); }

        Notification* createHTMLNotification(const String& URI, ExceptionCode& ec)
        {
            return Notification::create(KURL(ParsedURLString, URI), context(), ec, presenter());
        }

        Notification* createNotification(const String& iconURI, const String& title, const String& body, ExceptionCode& ec)
        {
            NotificationContents contents(iconURI, title, body);
            return Notification::create(contents, context(), ec, presenter());
        }

        ScriptExecutionContext* context() const { return m_scriptExecutionContext; }
        NotificationPresenter* presenter() const { return m_notificationPresenter; }

        int checkPermission();
        void requestPermission(PassRefPtr<VoidCallback> callback);

    private:
        NotificationCenter(ScriptExecutionContext*, NotificationPresenter*);

        ScriptExecutionContext* m_scriptExecutionContext;
        NotificationPresenter* m_notificationPresenter;
    };

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)

#endif // NotificationCenter_h
