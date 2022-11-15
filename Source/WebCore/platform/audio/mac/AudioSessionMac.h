/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if USE(AUDIO_SESSION) && PLATFORM(MAC)

#include "AudioSession.h"
#include <pal/spi/cf/CoreAudioSPI.h>

typedef UInt32 AudioObjectID;
typedef struct AudioObjectPropertyAddress AudioObjectPropertyAddress;

namespace WebCore {

class AudioSessionMac final : public AudioSession {
public:
    AudioSessionMac() = default;
    virtual ~AudioSessionMac() = default;

private:
    void addSampleRateObserverIfNeeded() const;
    void addBufferSizeObserverIfNeeded() const;
    void addDefaultDeviceObserverIfNeeded() const;
    void addMuteChangeObserverIfNeeded() const;
    void removeMuteChangeObserverIfNeeded() const;

    float sampleRateWithoutCaching() const;
    std::optional<size_t> bufferSizeWithoutCaching() const;
    void removePropertyListenersForDefaultDevice() const;

    static OSStatus handleSampleRateChange(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData);
    void handleSampleRateChange() const;
    static OSStatus handleBufferSizeChange(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData);
    void handleBufferSizeChange() const;
    static OSStatus handleDefaultDeviceChange(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* inClientData);

    AudioDeviceID defaultDevice() const;
    static const AudioObjectPropertyAddress& defaultOutputDeviceAddress();
    static const AudioObjectPropertyAddress& nominalSampleRateAddress();
    static const AudioObjectPropertyAddress& bufferSizeAddress();
    static const AudioObjectPropertyAddress& muteAddress();

    // AudioSession
    CategoryType category() const final { return m_category; }
    RouteSharingPolicy routeSharingPolicy() const { return m_policy; }
    void audioOutputDeviceChanged() final;
    void setIsPlayingToBluetoothOverride(std::optional<bool>) final;
    void setCategory(CategoryType, RouteSharingPolicy) final;
    float sampleRate() const final;
    size_t bufferSize() const final;
    size_t numberOfOutputChannels() const final;
    size_t maximumNumberOfOutputChannels() const final;
    bool tryToSetActiveInternal(bool) final;
    String routingContextUID() const final;
    size_t preferredBufferSize() const final;
    void setPreferredBufferSize(size_t) final;
    bool isMuted() const final;
    void handleMutedStateChange() final;
    void addConfigurationChangeObserver(ConfigurationChangeObserver&) final;
    void removeConfigurationChangeObserver(ConfigurationChangeObserver&) final;

    std::optional<bool> m_lastMutedState;
    mutable WeakHashSet<ConfigurationChangeObserver> m_configurationChangeObservers;
    AudioSession::CategoryType m_category { AudioSession::CategoryType::None };
    RouteSharingPolicy m_policy { RouteSharingPolicy::Default };
#if ENABLE(ROUTING_ARBITRATION)
    bool m_setupArbitrationOngoing { false };
    bool m_inRoutingArbitration { false };
    std::optional<bool> m_playingToBluetooth;
    std::optional<bool> m_playingToBluetoothOverride;
#endif
    mutable bool m_hasSampleRateObserver { false };
    mutable bool m_hasBufferSizeObserver { false };
    mutable bool m_hasDefaultDeviceObserver { false };
    mutable bool m_hasMuteChangeObserver { false };
    mutable std::optional<double> m_sampleRate;
    mutable std::optional<size_t> m_bufferSize;
    mutable std::optional<AudioDeviceID> m_defaultDevice;
};

}

#endif
