/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#if USE(AUDIO_SESSION)

#include <memory>
#include <wtf/CompletionHandler.h>
#include <wtf/EnumTraits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Observer.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class RouteSharingPolicy : uint8_t {
    Default,
    LongFormAudio,
    Independent,
    LongFormVideo
};

enum class AudioSessionCategory : uint8_t {
    None,
    AmbientSound,
    SoloAmbientSound,
    MediaPlayback,
    RecordAudio,
    PlayAndRecord,
    AudioProcessing,
};

class AudioSessionRoutingArbitrationClient;

class WEBCORE_EXPORT AudioSession {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AudioSession);
    friend class UniqueRef<AudioSession>;
    friend UniqueRef<AudioSession> WTF::makeUniqueRefWithoutFastMallocCheck<AudioSession>();
public:
    static UniqueRef<AudioSession> create();
    static void setSharedSession(UniqueRef<AudioSession>&&);
    static AudioSession& sharedSession();

    using ChangedObserver = WTF::Observer<void(AudioSession&)>;
    static void addAudioSessionChangedObserver(const ChangedObserver&);

    virtual ~AudioSession();

    using CategoryType = AudioSessionCategory;
    virtual void setCategory(CategoryType, RouteSharingPolicy);
    virtual CategoryType category() const;

    virtual void setCategoryOverride(CategoryType);
    virtual CategoryType categoryOverride() const;

    virtual RouteSharingPolicy routeSharingPolicy() const;
    virtual String routingContextUID() const;

    virtual float sampleRate() const;
    virtual size_t bufferSize() const;
    virtual size_t numberOfOutputChannels() const;
    virtual size_t maximumNumberOfOutputChannels() const;

    bool tryToSetActive(bool);

    virtual size_t preferredBufferSize() const;
    virtual void setPreferredBufferSize(size_t);

    class ConfigurationChangeObserver : public CanMakeWeakPtr<ConfigurationChangeObserver> {
    public:
        virtual ~ConfigurationChangeObserver() = default;

        virtual void hardwareMutedStateDidChange(const AudioSession&) = 0;
        virtual void bufferSizeDidChange(const AudioSession&) { }
        virtual void sampleRateDidChange(const AudioSession&) { }
    };

    virtual void addConfigurationChangeObserver(ConfigurationChangeObserver&);
    virtual void removeConfigurationChangeObserver(ConfigurationChangeObserver&);

    virtual void audioOutputDeviceChanged();
    virtual void setIsPlayingToBluetoothOverride(std::optional<bool>);

    virtual bool isMuted() const { return false; }
    virtual void handleMutedStateChange() { }

    virtual void beginInterruption();
    enum class MayResume { No, Yes };
    virtual void endInterruption(MayResume);

    virtual void beginInterruptionForTesting() { beginInterruption(); }
    virtual void endInterruptionForTesting() { endInterruption(MayResume::Yes); }

    class InterruptionObserver : public CanMakeWeakPtr<InterruptionObserver> {
    public:
        virtual ~InterruptionObserver() = default;

        virtual void beginAudioSessionInterruption() = 0;
        virtual void endAudioSessionInterruption(MayResume) = 0;
        virtual void activeStateChanged() { }
    };
    virtual void addInterruptionObserver(InterruptionObserver&);
    virtual void removeInterruptionObserver(InterruptionObserver&);

    virtual bool isActive() const { return m_active; }

    virtual void setRoutingArbitrationClient(WeakPtr<AudioSessionRoutingArbitrationClient>&& client) { m_routingArbitrationClient = client; }

    static bool shouldManageAudioSessionCategory() { return s_shouldManageAudioSessionCategory; }
    static void setShouldManageAudioSessionCategory(bool flag) { s_shouldManageAudioSessionCategory = flag; }

    virtual void setHostProcessAttribution(audit_token_t) { };
    virtual void setPresentingProcesses(Vector<audit_token_t>&&) { };

    bool isInterrupted() const { return m_isInterrupted; }

protected:
    friend class NeverDestroyed<AudioSession>;
    AudioSession();

    virtual bool tryToSetActiveInternal(bool);
    void activeStateChanged();

    WeakHashSet<InterruptionObserver> m_interruptionObservers;

    WeakPtr<AudioSessionRoutingArbitrationClient> m_routingArbitrationClient;
    AudioSession::CategoryType m_categoryOverride { AudioSession::CategoryType::None };
    bool m_active { false }; // Used only for testing.
    bool m_isInterrupted { false };

    static bool s_shouldManageAudioSessionCategory;
};

class WEBCORE_EXPORT AudioSessionRoutingArbitrationClient : public CanMakeWeakPtr<AudioSessionRoutingArbitrationClient> {
public:
    virtual ~AudioSessionRoutingArbitrationClient() = default;

    enum class RoutingArbitrationError : uint8_t { None, Failed, Cancelled };
    enum class DefaultRouteChanged : bool { No, Yes };

    using ArbitrationCallback = CompletionHandler<void(RoutingArbitrationError, DefaultRouteChanged)>;

    virtual void beginRoutingArbitrationWithCategory(AudioSession::CategoryType, ArbitrationCallback&&) = 0;
    virtual void leaveRoutingAbritration() = 0;

    using WeakValueType = AudioSessionRoutingArbitrationClient;
};

WEBCORE_EXPORT String convertEnumerationToString(RouteSharingPolicy);
WEBCORE_EXPORT String convertEnumerationToString(AudioSession::CategoryType);
WEBCORE_EXPORT String convertEnumerationToString(AudioSessionRoutingArbitrationClient::RoutingArbitrationError);
WEBCORE_EXPORT String convertEnumerationToString(AudioSessionRoutingArbitrationClient::DefaultRouteChanged);

} // namespace WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::RouteSharingPolicy> {
    using values = EnumValues<
    WebCore::RouteSharingPolicy,
    WebCore::RouteSharingPolicy::Default,
    WebCore::RouteSharingPolicy::LongFormAudio,
    WebCore::RouteSharingPolicy::Independent,
    WebCore::RouteSharingPolicy::LongFormVideo
    >;
};

template <> struct EnumTraits<WebCore::AudioSession::CategoryType> {
    using values = EnumValues <
    WebCore::AudioSession::CategoryType,
    WebCore::AudioSession::CategoryType::None,
    WebCore::AudioSession::CategoryType::AmbientSound,
    WebCore::AudioSession::CategoryType::SoloAmbientSound,
    WebCore::AudioSession::CategoryType::MediaPlayback,
    WebCore::AudioSession::CategoryType::RecordAudio,
    WebCore::AudioSession::CategoryType::PlayAndRecord,
    WebCore::AudioSession::CategoryType::AudioProcessing
    >;
};

template <> struct EnumTraits<WebCore::AudioSession::MayResume> {
    using values = EnumValues <
    WebCore::AudioSession::MayResume,
    WebCore::AudioSession::MayResume::No,
    WebCore::AudioSession::MayResume::Yes
    >;
};

template <> struct EnumTraits<WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError> {
    using values = EnumValues <
    WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError,
    WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError::None,
    WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Failed,
    WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Cancelled
    >;
};

template <> struct EnumTraits<WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged> {
    using values = EnumValues <
    WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged,
    WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged::No,
    WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged::Yes
    >;
};

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::RouteSharingPolicy> {
    static String toString(const WebCore::RouteSharingPolicy policy)
    {
        return convertEnumerationToString(policy);
    }
};

template <>
struct LogArgument<WebCore::AudioSession::CategoryType> {
    static String toString(const WebCore::AudioSession::CategoryType category)
    {
        return convertEnumerationToString(category);
    }
};

template <>
struct LogArgument<WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError> {
    static String toString(const WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError error)
    {
        return convertEnumerationToString(error);
    }
};
template <>
struct LogArgument<WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged> {
    static String toString(const WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged changed)
    {
        return convertEnumerationToString(changed);
    }
};

} // namespace WTF

#endif // USE(AUDIO_SESSION)
