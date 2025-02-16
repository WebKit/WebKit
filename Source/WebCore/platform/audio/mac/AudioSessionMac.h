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

#include "AudioSessionCocoa.h"
#include <pal/spi/cf/CoreAudioSPI.h>
#include <wtf/BlockPtr.h>
#include <wtf/TZoneMalloc.h>

typedef UInt32 AudioObjectID;
typedef struct AudioObjectPropertyAddress AudioObjectPropertyAddress;

namespace WebCore {

class AudioSessionMac final : public AudioSessionCocoa {
    WTF_MAKE_TZONE_ALLOCATED(AudioSessionMac);
public:
    static Ref<AudioSessionMac> create();
    ~AudioSessionMac();

private:
    AudioSessionMac();

    void addSampleRateObserverIfNeeded() const;
    void addBufferSizeObserverIfNeeded() const;
    void addDefaultDeviceObserverIfNeeded() const;
    void addMuteChangeObserverIfNeeded() const;
    void removeMuteChangeObserverIfNeeded() const;

    float sampleRateWithoutCaching() const;
    std::optional<size_t> bufferSizeWithoutCaching() const;
    void removePropertyListenersForDefaultDevice() const;

    void handleDefaultDeviceChange();
    void handleSampleRateChange() const;
    void handleBufferSizeChange() const;

    AudioDeviceID defaultDevice() const;
    static const AudioObjectPropertyAddress& defaultOutputDeviceAddress();
    static const AudioObjectPropertyAddress& nominalSampleRateAddress();
    static const AudioObjectPropertyAddress& bufferSizeAddress();
    static const AudioObjectPropertyAddress& muteAddress();

    bool hasSampleRateObserver() const { return !!m_handleSampleRateChangeBlock; };
    bool hasBufferSizeObserver() const { return !!m_handleBufferSizeChangeBlock; };
    bool hasDefaultDeviceObserver() const { return !!m_handleDefaultDeviceChangeBlock; };
    bool hasMuteChangeObserver() const { return !!m_handleMutedStateChangeBlock; };

    // AudioSession
    CategoryType category() const final { return m_category; }
    RouteSharingPolicy routeSharingPolicy() const { return m_policy; }
    void audioOutputDeviceChanged() final;
    void setIsPlayingToBluetoothOverride(std::optional<bool>) final;
    void setCategory(CategoryType, Mode, RouteSharingPolicy) final;
    float sampleRate() const final;
    size_t bufferSize() const final;
    size_t numberOfOutputChannels() const final;
    size_t maximumNumberOfOutputChannels() const final;
    String routingContextUID() const final;
    size_t preferredBufferSize() const final;
    void setPreferredBufferSize(size_t) final;
    size_t outputLatency() const final;
    bool isMuted() const final;
    void handleMutedStateChange() final;
    void addConfigurationChangeObserver(AudioSessionConfigurationChangeObserver&) final;
    void removeConfigurationChangeObserver(AudioSessionConfigurationChangeObserver&) final;

    WTFLogChannel& logChannel() const;
    uint64_t logIdentifier() const;

    std::optional<bool> m_lastMutedState;
    mutable WeakHashSet<AudioSessionConfigurationChangeObserver> m_configurationChangeObservers;
    AudioSession::CategoryType m_category { AudioSession::CategoryType::None };
    RouteSharingPolicy m_policy { RouteSharingPolicy::Default };
#if ENABLE(ROUTING_ARBITRATION)
    bool m_setupArbitrationOngoing { false };
    bool m_inRoutingArbitration { false };
    std::optional<bool> m_playingToBluetooth;
    std::optional<bool> m_playingToBluetoothOverride;
#endif
    mutable std::optional<double> m_sampleRate;
    mutable std::optional<size_t> m_bufferSize;
    mutable std::optional<AudioDeviceID> m_defaultDevice;

    mutable BlockPtr<void(unsigned, const struct AudioObjectPropertyAddress*)> m_handleDefaultDeviceChangeBlock;
    mutable BlockPtr<void(unsigned, const struct AudioObjectPropertyAddress*)> m_handleSampleRateChangeBlock;
    mutable BlockPtr<void(unsigned, const struct AudioObjectPropertyAddress*)> m_handleBufferSizeChangeBlock;
    mutable BlockPtr<void(unsigned, const struct AudioObjectPropertyAddress*)> m_handleMutedStateChangeBlock;
};

}

#endif
