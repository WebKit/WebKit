/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Jackjimmy. All rights reserved.
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

#if PLATFORM(COCOA)

#include <algorithm>
#include <pal/cf/CoreAudioExtras.h>
#include <wtf/FixedVector.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

// Smooths the output of a real-time render callback around a ring buffer underrun,
// for a reader whose read position holds while no data is available (so playback
// resumes exactly where it stopped):
//
// - fadeOutToSilence() is called instead of rendering when no data is available. It
//   overwrites the output with a linear ramp from the last rendered sample of each
//   channel down to zero over the callback, and pure silence on consecutive underruns.
// - didRender() is called after the callback's output has been filled with new data.
//   It modifies that output in place: after an underrun, the first fadeInFrameCount
//   frames are ramped in linearly from silence, continuing across callbacks shorter
//   than that, which reduces the discontinuity where the signal resumes.
//
// The fade-in has a fixed length rather than spanning the callback like the fade-out:
// the fade-out only fills what would otherwise be a gap, while the fade-in attenuates
// real signal, so a long ramp (a 1024-frame callback) would soften the attack of
// whatever resumes.
//
// Input contract: deinterleaved Float32, one AudioBuffer per channel, all buffers with
// the same capacity; the number of channels used for state is fixed at construction.
// A buffer list with no usable capacity is left untouched and does not advance the
// fade-in. Does not allocate or lock after construction; to be used from one render
// thread.
class AudioUnderrunDeclicker {
    WTF_MAKE_NONCOPYABLE(AudioUnderrunDeclicker);
public:
    static constexpr size_t fadeInFrameCount = 64;

    explicit AudioUnderrunDeclicker(size_t numberOfChannels)
        : m_lastRenderedSamples(FillWith { }, numberOfChannels, 0.f)
    {
    }

    void didRender(AudioBufferList& ioData, size_t numberOfFrames)
    {
        if (!numberOfFrames)
            return;

        auto buffers = PAL::span(ioData);
        if (m_fadeInPosition < fadeInFrameCount) {
            size_t rampedFrameCount = 0;
            for (auto& buffer : buffers) {
                auto samples = PAL::mutableSpan<float>(buffer);
                auto frameCount = std::min({ numberOfFrames, samples.size(), fadeInFrameCount - m_fadeInPosition });
                for (size_t frame = 0; frame < frameCount; ++frame)
                    samples[frame] *= static_cast<float>(m_fadeInPosition + frame + 1) / fadeInFrameCount;
                rampedFrameCount = std::max(rampedFrameCount, frameCount);
            }
            m_fadeInPosition += rampedFrameCount;
        }

        for (size_t channel = 0; channel < m_lastRenderedSamples.size(); ++channel) {
            if (channel >= buffers.size())
                break;
            auto samples = PAL::mutableSpan<float>(buffers[channel]);
            auto frameCount = std::min(numberOfFrames, samples.size());
            m_lastRenderedSamples[channel] = frameCount ? samples[frameCount - 1] : 0.f;
        }
    }

    void fadeOutToSilence(AudioBufferList& ioData, size_t numberOfFrames)
    {
        auto buffers = PAL::span(ioData);
        for (size_t channel = 0; channel < buffers.size(); ++channel) {
            auto samples = PAL::mutableSpan<float>(buffers[channel]);
            auto frameCount = std::min(numberOfFrames, samples.size());
            float startSample = channel < m_lastRenderedSamples.size() ? m_lastRenderedSamples[channel] : 0.f;
            if (startSample && frameCount) {
                // Linear ramp from the last rendered sample down to zero to declick.
                float step = startSample / frameCount;
                float value = startSample;
                for (size_t frame = 0; frame < frameCount; ++frame) {
                    value -= step;
                    samples[frame] = value;
                }
                zeroSpan(samples.subspan(frameCount));
            } else
                zeroSpan(samples);
        }
        // The output is now silent: consecutive underruns output pure silence, and the
        // next rendered data fades back in.
        m_lastRenderedSamples.fill(0.f);
        m_fadeInPosition = 0;
    }

private:
    FixedVector<float> m_lastRenderedSamples;
    // Frames of the current fade-in already output; fadeInFrameCount when not fading in.
    size_t m_fadeInPosition { fadeInFrameCount };
};

} // namespace WebCore

#endif // PLATFORM(COCOA)
