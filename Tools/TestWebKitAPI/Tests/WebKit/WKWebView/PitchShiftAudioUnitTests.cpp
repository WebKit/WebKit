/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#import "Helpers/PlatformUtilities.h"
#import <WebCore/AudioBus.h>
#import <WebCore/CAAudioStreamDescription.h>
#import <WebCore/PitchShiftAudioUnit.h>
#import <cmath>
#import <pal/cf/CoreAudioExtras.h>

using namespace WebCore;

static constexpr size_t kSampleRate = 44100;
static constexpr size_t kNumChannels = 1;
static constexpr size_t kFrameCount = 1024;
static constexpr double kTestFrequency = 440.0; // A4 note

// Helper to generate a sine wave at a given frequency
static void generateSineWave(AudioBus& bus, size_t numFrames, double frequency, double sampleRate, size_t startingFrame)
{
    for (unsigned channel = 0; channel < bus.numberOfChannels(); ++channel) {
        auto* channelData = bus.channel(channel)->mutableData();
        for (size_t frame = 0; frame < numFrames; ++frame) {
            auto currentFrame = frame + startingFrame;
            double phase = 2.0 * M_PI * frequency * currentFrame / sampleRate;
            channelData[frame] = static_cast<Float32>(std::sin(phase));
        }
    }
}

// Helper to count zero crossings (approximates frequency)
static size_t countZeroCrossings(std::span<const Float32> buffer)
{
    size_t crossings = 0;
    for (size_t i = 1; i < buffer.size(); ++i) {
        if ((buffer[i - 1] < 0.0f && buffer[i] >= 0.0f) || (buffer[i - 1] >= 0.0f && buffer[i] < 0.0f))
            ++crossings;
    }
    return crossings;
}

static AudioStreamBasicDescription audioFormat()
{
    return {
        .mSampleRate = kSampleRate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeFloatPacked) | static_cast<AudioFormatFlags>(kAudioFormatFlagIsNonInterleaved),
        .mBytesPerPacket = sizeof(Float32),
        .mFramesPerPacket = 1,
        .mBytesPerFrame = sizeof(Float32),
        .mChannelsPerFrame = kNumChannels,
        .mBitsPerChannel = 8 * sizeof(Float32),
        .mReserved = 0,
    };
}

TEST(PitchShiftAudioUnit, BasicProcessing)
{
    // Create pitch shifter
    auto pitchShifter = makeUnique<PitchShiftAudioUnit>(audioFormat());
    ASSERT_TRUE(pitchShifter);

    pitchShifter->setRate(2);

    // Set up input callback to generate 440Hz sine wave
    size_t frameCounter = 0;
    pitchShifter->setInputCallback([&frameCounter](AudioBus& bus, size_t numberOfFrames) {
        generateSineWave(bus, numberOfFrames, kTestFrequency, kSampleRate, frameCounter);
        frameCounter += numberOfFrames;
        return true;
    });

    // Allocate output AudioBus
    auto outputBus = AudioBus::create(kNumChannels, kFrameCount);

    // Process audio through pitch shifter
    for (int i = 0; i < 10; ++i) {
        bool success = pitchShifter->render(outputBus, kFrameCount);
        ASSERT_TRUE(success);
    }

    // Verify output is not silent (has some energy)
    auto* outputData = outputBus->channel(0)->data();
    float sumSquares = 0.0f;
    for (size_t i = 0; i < kFrameCount; ++i)
        sumSquares += outputData[i] * outputData[i];

    float rms = std::sqrt(sumSquares / kFrameCount);
    EXPECT_GT(rms, 0.1f); // Should have significant energy

    // Verify the output has approximately the same frequency as input
    // (pitch shifting should have preserved the 440Hz frequency)
    auto outputSpan = std::span<const Float32>(outputData, kFrameCount);
    size_t outputCrossings = countZeroCrossings(outputSpan);

    // Expected zero crossings for 440Hz at 44.1kHz over 1024 samples
    // = 2 * 440 * 1024 / 44100 ≈ 20.4 crossings
    // Allow 20% tolerance
    size_t expectedCrossings = static_cast<size_t>(2.0 * kTestFrequency * kFrameCount / kSampleRate);
    double ratio = static_cast<double>(outputCrossings) / static_cast<double>(expectedCrossings);
    EXPECT_GT(ratio, 0.8);
    EXPECT_LT(ratio, 1.2);
}
