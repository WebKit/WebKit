/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaSession_h
#define MediaSession_h

#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaSessionClient;

class MediaSession {
public:
    static std::unique_ptr<MediaSession> create(MediaSessionClient&);

    MediaSession(MediaSessionClient&);
    virtual ~MediaSession();

    enum MediaType {
        None = 0,
        Video,
        Audio,
        WebAudio,
    };
    MediaType mediaType() const;
    MediaType presentationType() const;

    enum State {
        Idle,
        Playing,
        Paused,
        Interrupted,
    };
    State state() const { return m_state; }
    void setState(State);

    enum InterruptionType {
        SystemSleep,
        EnteringBackground,
        SystemInterruption,
    };
    enum EndInterruptionFlags {
        NoFlags = 0,
        MayResumePlaying = 1 << 0,
    };
    void beginInterruption(InterruptionType);
    void endInterruption(EndInterruptionFlags);

    void applicationWillEnterForeground() const;
    void applicationWillEnterBackground() const;

    bool clientWillBeginPlayback();
    bool clientWillPausePlayback();

    void pauseSession();
    
    void visibilityChanged();

    String title() const;
    double duration() const;
    double currentTime() const;

    enum RemoteControlCommandType {
        NoCommand,
        PlayCommand,
        PauseCommand,
        StopCommand,
        TogglePlayPauseCommand,
        BeginSeekingBackwardCommand,
        EndSeekingBackwardCommand,
        BeginSeekingForwardCommand,
        EndSeekingForwardCommand,
    };
    bool canReceiveRemoteControlCommands() const;
    void didReceiveRemoteControlCommand(RemoteControlCommandType);

protected:
    MediaSessionClient& client() const { return m_client; }

private:
    void updateClientDataBuffering();

    MediaSessionClient& m_client;
    State m_state;
    State m_stateToRestore;
    bool m_notifyingClient;
};

class MediaSessionClient {
    WTF_MAKE_NONCOPYABLE(MediaSessionClient);
public:
    MediaSessionClient() { }
    
    virtual MediaSession::MediaType mediaType() const = 0;
    virtual MediaSession::MediaType presentationType() const = 0;

    virtual void resumePlayback() = 0;
    virtual void pausePlayback() = 0;

    virtual String mediaSessionTitle() const;
    virtual double mediaSessionDuration() const;
    virtual double mediaSessionCurrentTime() const;
    
    virtual bool canReceiveRemoteControlCommands() const = 0;
    virtual void didReceiveRemoteControlCommand(MediaSession::RemoteControlCommandType) = 0;

    virtual void setShouldBufferData(bool) { }
    virtual bool elementIsHidden() const { return false; }

    virtual bool overrideBackgroundPlaybackRestriction() const = 0;

protected:
    virtual ~MediaSessionClient() { }
};

}

#endif // MediaSession_h
