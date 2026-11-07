/*
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

#include "config.h"

#if PLATFORM(COCOA)

#include "Helpers/Test.h"
#include <WebCore/AudioUnderrunDeclicker.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <pal/cf/CoreAudioExtras.h>
#include <vector>

namespace TestWebKitAPI {

using namespace WebCore;

static constexpr size_t fadeInFrames = AudioUnderrunDeclicker::fadeInFrameCount;

class TestBuffers {
public:
    TestBuffers(size_t channelCount, size_t frameCount)
        : m_samples(channelCount, std::vector<float>(frameCount, 0.f))
        , m_list(PAL::createAudioBufferList(channelCount, PAL::ShouldZeroMemory::Yes))
    {
        auto buffers = PAL::span(*m_list);
        for (size_t channel = 0; channel < channelCount; ++channel) {
            buffers[channel].mNumberChannels = 1;
            buffers[channel].mDataByteSize = frameCount * sizeof(float);
            buffers[channel].mData = m_samples[channel].data();
        }
    }

    AudioBufferList& list() { return *m_list; }
    std::vector<float>& channel(size_t index) { return m_samples[index]; }
    size_t frameCount() const { return m_samples.empty() ? 0 : m_samples[0].size(); }

    void fill(size_t channel, float value) { std::ranges::fill(m_samples[channel], value); }
    void fillAll(float value)
    {
        for (auto& samples : m_samples)
            std::ranges::fill(samples, value);
    }
    void setCapacity(size_t channel, size_t frameCount) { PAL::span(*m_list)[channel].mDataByteSize = frameCount * sizeof(float); }

private:
    std::vector<std::vector<float>> m_samples;
    std::unique_ptr<AudioBufferList, WTF::SystemFree<AudioBufferList>> m_list;
};

// A non-constant test signal, so that a gain error at any frame shows up.
static float waveform(size_t frame)
{
    return 0.4f * std::sin(0.05f * frame) + 0.1f;
}

static float fadeInGain(size_t rampFrame)
{
    return rampFrame < fadeInFrames ? static_cast<float>(rampFrame + 1) / fadeInFrames : 1.f;
}

static void fillWithWaveform(TestBuffers& buffers, size_t firstFrame)
{
    for (size_t channel = 0; channel < buffers.list().mNumberBuffers; ++channel) {
        auto& samples = buffers.channel(channel);
        for (size_t frame = 0; frame < samples.size(); ++frame)
            samples[frame] = waveform(firstFrame + frame);
    }
}

TEST(AudioUnderrunDeclicker, RenderedSamplesPassThroughWithoutUnderrun)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, 512);
    fillWithWaveform(buffers, 0);
    declicker.didRender(buffers.list(), 512);
    for (size_t frame = 0; frame < 512; ++frame)
        EXPECT_EQ(buffers.channel(0)[frame], waveform(frame));
}

TEST(AudioUnderrunDeclicker, UnderrunFadesOutFromLastRenderedSample)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, 8);
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 8);

    buffers.fill(0, 0.9f); // Stale contents must not leak through.
    declicker.fadeOutToSilence(buffers.list(), 8);
    auto& samples = buffers.channel(0);
    EXPECT_FLOAT_EQ(samples[0], 0.5f - 0.5f / 8);
    for (size_t frame = 1; frame < 8; ++frame)
        EXPECT_LT(samples[frame], samples[frame - 1]);
    EXPECT_NEAR(samples[7], 0.f, 1e-6);
}

TEST(AudioUnderrunDeclicker, ConsecutiveUnderrunsAreSilent)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, 8);
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 8);
    declicker.fadeOutToSilence(buffers.list(), 8);

    buffers.fill(0, 0.9f);
    declicker.fadeOutToSilence(buffers.list(), 8);
    for (auto sample : buffers.channel(0))
        EXPECT_EQ(sample, 0.f);
}

TEST(AudioUnderrunDeclicker, ResumeAfterUnderrunFadesInOverFixedLength)
{
    // Callbacks shorter than, equal to and longer than the fade-in.
    for (size_t callbackFrames : std::array<size_t, 6> { 1, 32, 64, 128, 512, 1024 }) {
        SCOPED_TRACE(callbackFrames);
        AudioUnderrunDeclicker declicker(1);
        TestBuffers buffers(1, callbackFrames);
        fillWithWaveform(buffers, 0);
        declicker.didRender(buffers.list(), callbackFrames);
        declicker.fadeOutToSilence(buffers.list(), callbackFrames);

        // Playback resumes where it stopped; follow it for two fade-in lengths.
        size_t resumedFrames = 0;
        while (resumedFrames < 2 * fadeInFrames) {
            fillWithWaveform(buffers, resumedFrames);
            declicker.didRender(buffers.list(), callbackFrames);
            for (size_t frame = 0; frame < callbackFrames; ++frame)
                EXPECT_FLOAT_EQ(buffers.channel(0)[frame], waveform(resumedFrames + frame) * fadeInGain(resumedFrames + frame));
            resumedFrames += callbackFrames;
        }
    }
}

TEST(AudioUnderrunDeclicker, FirstRenderAfterInitialUnderrunFadesIn)
{
    // The first render callback of a stream usually finds no data yet.
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, fadeInFrames);
    declicker.fadeOutToSilence(buffers.list(), fadeInFrames);
    for (auto sample : buffers.channel(0))
        EXPECT_EQ(sample, 0.f);

    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), fadeInFrames);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f / fadeInFrames);
    EXPECT_FLOAT_EQ(buffers.channel(0)[fadeInFrames - 1], 0.5f);
}

TEST(AudioUnderrunDeclicker, UnderrunDuringFadeInFadesOutFromOutputSampleAndRestartsFadeIn)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, fadeInFrames / 2);
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), fadeInFrames / 2);
    declicker.fadeOutToSilence(buffers.list(), fadeInFrames / 2);

    // Halfway into the fade-in...
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), fadeInFrames / 2);
    float lastOutput = buffers.channel(0).back();
    EXPECT_FLOAT_EQ(lastOutput, 0.5f * fadeInGain(fadeInFrames / 2 - 1));

    // ...another underrun fades out from what was actually output...
    declicker.fadeOutToSilence(buffers.list(), fadeInFrames / 2);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], lastOutput - lastOutput / (fadeInFrames / 2));

    // ...and the fade-in starts over.
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), fadeInFrames / 2);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f * fadeInGain(0));
}

TEST(AudioUnderrunDeclicker, UnderrunAfterResumeFadesOutFromRenderedSample)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, 2 * fadeInFrames);
    fillWithWaveform(buffers, 0);
    declicker.didRender(buffers.list(), 2 * fadeInFrames);
    declicker.fadeOutToSilence(buffers.list(), 2 * fadeInFrames);

    fillWithWaveform(buffers, 0);
    declicker.didRender(buffers.list(), 2 * fadeInFrames);
    float lastOutput = buffers.channel(0).back();
    EXPECT_EQ(lastOutput, waveform(2 * fadeInFrames - 1)); // Past the fade-in: untouched.

    declicker.fadeOutToSilence(buffers.list(), 2 * fadeInFrames);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], lastOutput - lastOutput / (2 * fadeInFrames));

    fillWithWaveform(buffers, 2 * fadeInFrames);
    declicker.didRender(buffers.list(), 2 * fadeInFrames);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], waveform(2 * fadeInFrames) * fadeInGain(0));
}

TEST(AudioUnderrunDeclicker, ChannelsFadeIndependently)
{
    AudioUnderrunDeclicker declicker(2);
    TestBuffers buffers(2, 8);
    buffers.fill(0, 0.5f);
    buffers.fill(1, -0.25f);
    declicker.didRender(buffers.list(), 8);

    declicker.fadeOutToSilence(buffers.list(), 8);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f - 0.5f / 8);
    EXPECT_FLOAT_EQ(buffers.channel(1)[0], -0.25f + 0.25f / 8);

    buffers.fill(0, 0.5f);
    buffers.fill(1, -0.25f);
    declicker.didRender(buffers.list(), 8);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f * fadeInGain(0));
    EXPECT_FLOAT_EQ(buffers.channel(1)[0], -0.25f * fadeInGain(0));
    EXPECT_FLOAT_EQ(buffers.channel(0)[7], 0.5f * fadeInGain(7));
    EXPECT_FLOAT_EQ(buffers.channel(1)[7], -0.25f * fadeInGain(7));
}

TEST(AudioUnderrunDeclicker, ChannelsBeyondConfiguredCountAreSilencedOnUnderrun)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(2, 8);
    buffers.fillAll(0.5f);
    declicker.didRender(buffers.list(), 8);

    buffers.fillAll(0.9f);
    declicker.fadeOutToSilence(buffers.list(), 8);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f - 0.5f / 8);
    for (auto sample : buffers.channel(1))
        EXPECT_EQ(sample, 0.f);
}

TEST(AudioUnderrunDeclicker, RenderWithoutSamplesDoesNotAdvanceFadeIn)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, 8);
    declicker.fadeOutToSilence(buffers.list(), 8);

    // No frames requested.
    declicker.didRender(buffers.list(), 0);

    // Frames requested, but no capacity to hold them.
    buffers.setCapacity(0, 0);
    declicker.didRender(buffers.list(), 8);
    buffers.setCapacity(0, 8);

    // No buffers at all.
    auto emptyList = PAL::createAudioBufferList(0, PAL::ShouldZeroMemory::Yes);
    declicker.didRender(*emptyList, 8);

    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 8);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f * fadeInGain(0));
}

TEST(AudioUnderrunDeclicker, ZeroFrameUnderrunSilencesAndArmsFadeIn)
{
    // fadeOutToSilence() leaves the output silent whatever the frame count, so the
    // next rendered data fades in.
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, 8);
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 8);

    declicker.fadeOutToSilence(buffers.list(), 0);
    for (auto sample : buffers.channel(0))
        EXPECT_EQ(sample, 0.f);

    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 8);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f * fadeInGain(0));
}

TEST(AudioUnderrunDeclicker, FrameCountIsClampedToBufferSize)
{
    AudioUnderrunDeclicker declicker(1);
    TestBuffers buffers(1, 8);
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 32);
    declicker.fadeOutToSilence(buffers.list(), 32);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f - 0.5f / 8);
    EXPECT_NEAR(buffers.channel(0)[7], 0.f, 1e-6);

    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 32);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f * fadeInGain(0));
    EXPECT_FLOAT_EQ(buffers.channel(0)[7], 0.5f * fadeInGain(7));

    // Only the frames actually output count toward the fade-in.
    buffers.fill(0, 0.5f);
    declicker.didRender(buffers.list(), 32);
    EXPECT_FLOAT_EQ(buffers.channel(0)[0], 0.5f * fadeInGain(8));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
