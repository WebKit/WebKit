/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "MultiChannelResampler.h"

#include "AudioBus.h"

namespace WebCore {

// ChannelProvider provides a single channel of audio data (one channel at a time) for each channel
// of data provided to us in a multi-channel provider.
class MultiChannelResampler::ChannelProvider {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ChannelProvider(unsigned numberOfChannels, unsigned requestFrames)
        : m_multiChannelBus(AudioBus::create(numberOfChannels, requestFrames))
    {
    }

    void setProvider(AudioSourceProvider* multiChannelProvider)
    {
        m_multiChannelProvider = multiChannelProvider;
    }

    // provideInput() will be called once for each channel, starting with the first channel.
    // Each time it's called, it will provide the next channel of data.
    void provideInputForChannel(AudioBus* bus, size_t framesToProcess, unsigned channelIndex)
    {
        ASSERT(channelIndex < m_multiChannelBus->numberOfChannels());
        ASSERT(framesToProcess <= m_multiChannelBus->length());
        if (framesToProcess > m_multiChannelBus->length())
            return;

        bool isBusGood = bus && bus->numberOfChannels() == 1;
        ASSERT(isBusGood);
        if (!isBusGood)
            return;

        // Get the data from the multi-channel provider when the first channel asks for it.
        // For subsequent channels, we can just dish out the channel data from that (stored in m_multiChannelBus).
        if (!channelIndex) {
            m_framesToProcess = framesToProcess;
            m_multiChannelProvider->provideInput(m_multiChannelBus.get(), framesToProcess);
        }

        // All channels must ask for the same amount. This should always be the case, but let's just make sure.
        bool isGood = framesToProcess == m_framesToProcess;
        ASSERT(isGood);
        if (!isGood)
            return;

        // Copy the channel data from what we received from m_multiChannelProvider.
        memcpy(bus->channel(0)->mutableData(), m_multiChannelBus->channel(channelIndex)->data(), sizeof(float) * framesToProcess);
    }

private:
    AudioSourceProvider* m_multiChannelProvider { nullptr };
    RefPtr<AudioBus> m_multiChannelBus;
    size_t m_framesToProcess { 0 }; // Used to verify that all channels ask for the same amount.
};

MultiChannelResampler::MultiChannelResampler(double scaleFactor, unsigned numberOfChannels, unsigned requestFrames)
    : m_numberOfChannels(numberOfChannels)
    , m_channelProvider(makeUnique<ChannelProvider>(m_numberOfChannels, requestFrames))
{
    // Create each channel's resampler.
    for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex)
        m_kernels.append(makeUnique<SincResampler>(scaleFactor, requestFrames));
}

MultiChannelResampler::~MultiChannelResampler() = default;

void MultiChannelResampler::process(AudioSourceProvider* provider, AudioBus* destination, size_t framesToProcess)
{
    ASSERT(m_numberOfChannels == destination->numberOfChannels());

    // The provider can provide us with multi-channel audio data. But each of our single-channel resamplers (kernels)
    // below requires a provider which provides a single unique channel of data.
    // channelProvider wraps the original multi-channel provider and dishes out one channel at a time.
    m_channelProvider->setProvider(provider);

    // We need to ensure that SincResampler only calls provideInput() once for each channel or it will confuse the logic
    // inside ChannelProvider. To ensure this, we chunk the number of requested frames into SincResampler::chunkSize()
    // sized chunks. SincResampler guarantees it will only call provideInput() once once we resample this way.
    m_outputFramesReady = 0;
    while (m_outputFramesReady < framesToProcess) {
        size_t chunkSize = m_kernels[0]->chunkSize();
        size_t framesThisTime = std::min(framesToProcess - m_outputFramesReady, chunkSize);

        for (unsigned channelIndex = 0; channelIndex < m_numberOfChannels; ++channelIndex) {
            ASSERT(chunkSize == m_kernels[channelIndex]->chunkSize());
            bool wasProvideInputCalled = false;
            m_kernels[channelIndex]->process(destination->channel(channelIndex)->mutableData() + m_outputFramesReady, framesThisTime, [this, channelIndex, &wasProvideInputCalled](AudioBus* bus, size_t framesToProcess) {
                ASSERT_WITH_MESSAGE(!wasProvideInputCalled, "provideInputForChannel should only be called once");
                wasProvideInputCalled = true;
                m_channelProvider->provideInputForChannel(bus, framesToProcess, channelIndex);
            });
        }

        m_outputFramesReady += framesThisTime;
    }

    m_channelProvider->setProvider(nullptr);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
