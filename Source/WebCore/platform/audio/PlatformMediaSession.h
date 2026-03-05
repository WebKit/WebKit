/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/PlatformMediaSessionInterface.h>

namespace WebCore {

class WEBCORE_EXPORT PlatformMediaSession : public PlatformMediaSessionInterface {
    WTF_MAKE_TZONE_ALLOCATED(PlatformMediaSession);
public:
    static Ref<PlatformMediaSession> create(PlatformMediaSessionClient& client)
    {
        return adoptRef(*new PlatformMediaSession(client));
    }

    virtual ~PlatformMediaSession();

    void setActive(bool) final;

    State state() const override;
    void setState(State) override;

    State stateToRestore() const final { return m_stateToRestore; }

    InterruptionType interruptionType() const final;

    void clientCharacteristicsChanged(bool) override;

    void beginInterruption(InterruptionType) final;
    void endInterruption(OptionSet<EndInterruptionFlags>) final;

    void clientWillBeginAutoplaying() override;
    void clientWillBeginPlayback(CompletionHandler<void(bool)>&&) override;
    bool clientWillPausePlayback() override;

    void clientWillBeDOMSuspended() final;

    void pauseSession() final;
    void stopSession() final;

    void didReceiveRemoteControlCommand(RemoteControlCommandType, const RemoteCommandArgument&) override;

    bool isPlayingToWirelessPlaybackTarget() const override { return m_isPlayingToWirelessPlaybackTarget; }
    void isPlayingToWirelessPlaybackTargetChanged(bool) final;

    bool blockedBySystemInterruption() const final;
    bool activeAudioSessionRequired() const final;
    void canProduceAudioChanged() final;

    bool preparingToPlay() const final { return m_preparingToPlay; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const override;
    uint64_t logIdentifier() const override;
    ASCIILiteral logClassName() const override { return "PlatformMediaSession"_s; }
    WTFLogChannel& logChannel() const override;
#endif

    bool canPlayConcurrently(const PlatformMediaSessionInterface&) const final;

    WeakPtr<PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSessionInterface>>&, PlaybackControlsPurpose) override;

    bool isActiveNowPlayingSession() const final { return m_isActiveNowPlayingSession; }
    void setActiveNowPlayingSession(bool) final;

#if !RELEASE_LOG_DISABLED
    String description() const override;
#endif

protected:
    PlatformMediaSession(PlatformMediaSessionClient& client)
        : PlatformMediaSessionInterface(client)
    {
    }

private:
    bool processClientWillPausePlayback(DelayCallingUpdateNowPlaying);
    size_t activeInterruptionCount() const;

    State m_state { State::Idle };
    State m_stateToRestore { State::Idle };
    struct Interruption {
        InterruptionType type { InterruptionType::NoInterruption };
        bool ignored { false };
    };
    Vector<Interruption> m_interruptionStack;
    bool m_active { false };
    bool m_notifyingClient { false };
    bool m_isPlayingToWirelessPlaybackTarget { false };
    bool m_preparingToPlay { false };
    bool m_isActiveNowPlayingSession { false };
};

inline PlatformMediaSession::State PlatformMediaSession::state() const { return m_state; }

String convertEnumerationToString(PlatformMediaSession::State);
String convertEnumerationToString(PlatformMediaSession::InterruptionType);
String convertEnumerationToString(PlatformMediaSession::MediaType);
WEBCORE_EXPORT String convertEnumerationToString(PlatformMediaSession::RemoteControlCommandType);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::PlatformMediaSession::State> {
    static String toString(const WebCore::PlatformMediaSession::State state)
    {
        return convertEnumerationToString(state);
    }
};

template <>
struct LogArgument<WebCore::PlatformMediaSession::InterruptionType> {
    static String toString(const WebCore::PlatformMediaSession::InterruptionType state)
    {
        return convertEnumerationToString(state);
    }
};

template <>
struct LogArgument<WebCore::PlatformMediaSession::RemoteControlCommandType> {
    static String toString(const WebCore::PlatformMediaSession::RemoteControlCommandType command)
    {
        return convertEnumerationToString(command);
    }
};

template <>
struct LogArgument<WebCore::PlatformMediaSession::MediaType> {
    static String toString(const WebCore::PlatformMediaSession::MediaType mediaType)
    {
        return convertEnumerationToString(mediaType);
    }
};

} // namespace WTF
