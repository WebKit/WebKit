/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef UserMediaClientMock_h
#define UserMediaClientMock_h

#if ENABLE(MEDIA_STREAM)

#include "TimerEventBasedMock.h"
#include "UserMediaClient.h"
#include "UserMediaRequest.h"

namespace WebCore {

class UserMediaClientRequestNotifier : public MockNotifier {
public:
    UserMediaClientRequestNotifier(Ref<UserMediaRequest>&& request, bool requestSuccess)
        : m_request(WTF::move(request))
        , m_requestSuccess(requestSuccess)
    {
    }

    void fire() override
    {
        if (!m_requestSuccess) {
            m_request->userMediaAccessDenied();
            return;
        }

        auto audioDeviceUIDs = m_request->audioDeviceUIDs();
        auto videoDeviceUIDs  = m_request->videoDeviceUIDs();
        String allowedAudioUID = audioDeviceUIDs.size() ? audioDeviceUIDs.at(0) : emptyString();
        String videoAudioUID = videoDeviceUIDs.size() ? videoDeviceUIDs.at(0) : emptyString();

        m_request->userMediaAccessGranted(allowedAudioUID, videoAudioUID);
    }

private:
    Ref<UserMediaRequest> m_request;
    bool m_requestSuccess;
};

class UserMediaClientMock final : public UserMediaClient, public TimerEventBasedMock {
public:
    public:
    virtual void pageDestroyed() override
    {
        delete this;
    }

    virtual void requestUserMediaAccess(Ref<UserMediaRequest>&& request) override
    {
        RefPtr<UserMediaClientRequestNotifier> notifier = adoptRef(new UserMediaClientRequestNotifier(WTF::move(request), true));
        m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    }

    virtual void cancelUserMediaAccessRequest(UserMediaRequest& request) override
    {
        RefPtr<UserMediaClientRequestNotifier> notifier = adoptRef(new UserMediaClientRequestNotifier(request, false));
        m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
    }
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // UserMediaClientMock_h
