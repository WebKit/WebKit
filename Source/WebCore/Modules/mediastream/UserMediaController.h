/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "FeaturePolicy.h"
#include "Page.h"
#include "UserMediaClient.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

class UserMediaRequest;

class UserMediaController : public Supplement<Page> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit UserMediaController(UserMediaClient*);
    ~UserMediaController();

    UserMediaClient* client() const { return m_client; }

    void requestUserMediaAccess(UserMediaRequest&);
    void cancelUserMediaAccessRequest(UserMediaRequest&);

    void enumerateMediaDevices(Document&, UserMediaClient::EnumerateDevicesCallback&&);

    UserMediaClient::DeviceChangeObserverToken addDeviceChangeObserver(Function<void()>&&);
    void removeDeviceChangeObserver(UserMediaClient::DeviceChangeObserverToken);

    void logGetUserMediaDenial(Document&);
    void logGetDisplayMediaDenial(Document&);
    void logEnumerateDevicesDenial(Document&);

    WEBCORE_EXPORT static const char* supplementName();
    static UserMediaController* from(Page* page) { return static_cast<UserMediaController*>(Supplement<Page>::from(page, supplementName())); }

private:
    UserMediaClient* m_client;
};

inline void UserMediaController::requestUserMediaAccess(UserMediaRequest& request)
{
    m_client->requestUserMediaAccess(request);
}

inline void UserMediaController::cancelUserMediaAccessRequest(UserMediaRequest& request)
{
    m_client->cancelUserMediaAccessRequest(request);
}

inline void UserMediaController::enumerateMediaDevices(Document& document, UserMediaClient::EnumerateDevicesCallback&& completionHandler)
{
    m_client->enumerateMediaDevices(document, WTFMove(completionHandler));
}


inline UserMediaClient::DeviceChangeObserverToken UserMediaController::addDeviceChangeObserver(Function<void()>&& observer)
{
    return m_client->addDeviceChangeObserver(WTFMove(observer));
}

inline void UserMediaController::removeDeviceChangeObserver(UserMediaClient::DeviceChangeObserverToken token)
{
    m_client->removeDeviceChangeObserver(token);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
