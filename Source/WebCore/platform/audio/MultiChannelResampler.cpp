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
#include "SincResampler.h"
#include <functional>
#include <wtf/Algorithms.h>

namespace WebCore {

MultiChannelResampler::MultiChannelResampler(double scaleFactor, unsigned numberOfChannels, unsigned requestFrames, Function<void(AudioBus*, size_t framesToProcess)>&& provideInput)
    : m_numberOfChannels(numberOfChannels)
    , m_provideInput(WTFMove(provideInput))
    , m_multiChannelBus(AudioBus::create(numberOfChannels, requestFrames))
{
    // Create each channel's resampler.
    m_kernels = Vector<std::unique_ptr<SincResampler>>(numberOfChannels, [&](size_t channelIndex) {
        return makeUnique<SincResampler>(scaleFactor, requestFrames, std::bind(&MultiChannelResampler::provideInputForChannel, this, std::placeholders::_1, std::placeholders::_2, channelIndex));
    });
}

MultiChannelResampler::~MultiChannelResampler() = default;

void MultiChannelResampler::process(AudioBus* destination, size_t framesToProcess)
{
    ASSERT(m_numberOfChannels == destination->numberOfChannels());
    if (destination->numberOfChannels() == 1) {
        // Fast path when the bus is mono to avoid the chunking below.
        m_kernels[0]->process(destination->channel(0)->mutableSpan(), framesToProcess);
        return;
    }

    // We need to ensure that SincResampler only calls provideInputForChannel() once for each channel or it will confuse the logic
    // inside provideInputForChannel(). To ensure this, we chunk the number of requested frames into SincResampler::chunkSize()
    // sized chunks. SincResampler guarantees it will only call provideInputForChannel() once once we resample this way.
    m_outputFramesReady = 0;
    while (m_outputFramesReady < framesToProcess) {
        size_t chunkSize = m_kernels[0]->chunkSize();
        size_t framesThisTime = std::min(framesToProcess - m_outputFramesReady, chunkSize);

        for (unsigned channelIndex = 0; channelIndex < m_numberOfChannels; ++channelIndex) {
            ASSERT(chunkSize == m_kernels[channelIndex]->chunkSize());
            auto* channel = destination->channel(channelIndex);
            m_kernels[channelIndex]->process(channel->mutableSpan().subspan(m_outputFramesReady), framesThisTime);
        }

        m_outputFramesReady += framesThisTime;
    }
}

void MultiChannelResampler::provideInputForChannel(std::span<float> buffer, size_t framesToProcess, unsigned channelIndex)
{
    ASSERT(channelIndex < m_multiChannelBus->numberOfChannels());
    ASSERT(framesToProcess <= m_multiChannelBus->length());

    if (!channelIndex)
        m_provideInput(m_multiChannelBus.get(), framesToProcess);

    // Copy the channel data from what we received from m_multiChannelProvider.
    memcpySpan(buffer.subspan(0, framesToProcess), m_multiChannelBus->channel(channelIndex)->span().subspan(0, framesToProcess));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
