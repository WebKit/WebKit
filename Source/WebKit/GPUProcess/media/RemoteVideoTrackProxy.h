/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#if ENABLE(GPU_PROCESS)

#include "MessageReceiver.h"
#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/TrackBase.h>
#include <WebCore/VideoTrackPrivate.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class GPUConnectionToWebProcess;
struct VideoTrackPrivateRemoteConfiguration;

class RemoteVideoTrackProxy final
    : public ThreadSafeRefCounted<RemoteVideoTrackProxy, WTF::DestructionThread::Main>
    , private WebCore::VideoTrackPrivateClient {
public:
    static Ref<RemoteVideoTrackProxy> create(GPUConnectionToWebProcess& connectionToWebProcess, TrackPrivateRemoteIdentifier identifier, WebCore::VideoTrackPrivate& trackPrivate, WebCore::MediaPlayerIdentifier mediaPlayerIdentifier)
    {
        return adoptRef(*new RemoteVideoTrackProxy(connectionToWebProcess, identifier, trackPrivate, mediaPlayerIdentifier));
    }

    virtual ~RemoteVideoTrackProxy();

    TrackPrivateRemoteIdentifier identifier() const { return m_identifier; };
    void setSelected(bool selected)
    {
        m_selected = selected;
        m_trackPrivate->setSelected(selected);
    }

private:
    RemoteVideoTrackProxy(GPUConnectionToWebProcess&, TrackPrivateRemoteIdentifier, WebCore::VideoTrackPrivate&, WebCore::MediaPlayerIdentifier);

    // VideoTrackPrivateClient
    void selectedChanged(bool) final;
    void configurationChanged(const WebCore::PlatformVideoTrackConfiguration&) final { updateConfiguration(); }

    // TrackPrivateBaseClient
    void idChanged(const AtomString&) final;
    void labelChanged(const AtomString&) final;
    void languageChanged(const AtomString&) final;
    void willRemove() final;

    VideoTrackPrivateRemoteConfiguration configuration();
    void updateConfiguration();

    WeakPtr<GPUConnectionToWebProcess> m_connectionToWebProcess;
    TrackPrivateRemoteIdentifier m_identifier;
    Ref<WebCore::VideoTrackPrivate> m_trackPrivate;
    WebCore::MediaPlayerIdentifier m_mediaPlayerIdentifier;
    bool m_selected { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
