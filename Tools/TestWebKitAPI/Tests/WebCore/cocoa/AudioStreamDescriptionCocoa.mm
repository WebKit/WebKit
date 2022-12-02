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

#import "config.h"

#if PLATFORM(COCOA)
#import "Test.h"
#import <WebCore/CAAudioStreamDescription.h>

namespace {

template<typename T> inline constexpr AudioFormatFlags formatFlag = 0;
template<> inline constexpr AudioFormatFlags formatFlag<float> = kAudioFormatFlagIsFloat;
template<> inline constexpr AudioFormatFlags formatFlag<double> = kAudioFormatFlagIsFloat;
template<> inline constexpr AudioFormatFlags formatFlag<int32_t> = kAudioFormatFlagIsSignedInteger;
template<> inline constexpr AudioFormatFlags formatFlag<int16_t> = kAudioFormatFlagIsSignedInteger;

template<typename T> inline constexpr WebCore::AudioStreamDescription::PCMFormat pcmFormat = WebCore::AudioStreamDescription::None;
template<> inline constexpr WebCore::AudioStreamDescription::PCMFormat pcmFormat<float> = WebCore::AudioStreamDescription::Float32;
template<> inline constexpr WebCore::AudioStreamDescription::PCMFormat pcmFormat<double> = WebCore::AudioStreamDescription::Float64;
template<> inline constexpr WebCore::AudioStreamDescription::PCMFormat pcmFormat<int16_t> = WebCore::AudioStreamDescription::Int16;
template<> inline constexpr WebCore::AudioStreamDescription::PCMFormat pcmFormat<int32_t> = WebCore::AudioStreamDescription::Int32;

template<typename T> AudioStreamBasicDescription createASBD(uint32_t numChannels = 1)
{
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mSampleRate = 44000;
    asbd.mFramesPerPacket = 1;
    asbd.mChannelsPerFrame = numChannels;
    asbd.mBitsPerChannel = 8 * sizeof(T);
    asbd.mBytesPerFrame = sizeof(T) * asbd.mChannelsPerFrame;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame;
    asbd.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeEndian) | static_cast<AudioFormatFlags>(kAudioFormatFlagIsPacked) | formatFlag<T>;
    asbd.mReserved = 0;
    return asbd;
}


template <typename T>
class EachSampleTypeTest : public testing::Test {

};

static constexpr uint32_t failPropertyIncrements[] = {
    1, static_cast<uint32_t>(-1), 8, static_cast<uint32_t>(-8), 16, static_cast<uint32_t>(-16), 32, static_cast<uint32_t>(-32), 60, 5000
};

}


TYPED_TEST_SUITE_P(EachSampleTypeTest);

TYPED_TEST_P(EachSampleTypeTest, CreateWorks)
{
    WebCore::CAAudioStreamDescription d = createASBD<TypeParam>();
    EXPECT_EQ(d.format(), pcmFormat<TypeParam>);
}

TYPED_TEST_P(EachSampleTypeTest, FormatFailsBytesPerFrame)
{
    for (auto increment : failPropertyIncrements) {
        auto asbd = createASBD<TypeParam>();
        asbd.mBytesPerFrame += increment;
        WebCore::CAAudioStreamDescription d = asbd;
        EXPECT_EQ(d.format(), WebCore::AudioStreamDescription::None) << increment;
    }
}

TYPED_TEST_P(EachSampleTypeTest, FormatFailsBitsPerChannel)
{
    for (auto increment : failPropertyIncrements) {
        auto asbd = createASBD<TypeParam>();
        asbd.mBitsPerChannel += increment;
        WebCore::CAAudioStreamDescription d = asbd;
        EXPECT_EQ(d.format(), WebCore::AudioStreamDescription::None) << increment;
    }
}

TYPED_TEST_P(EachSampleTypeTest, FormatFailsChannelsPerFrame)
{
    for (auto increment : failPropertyIncrements) {
        auto asbd = createASBD<TypeParam>();
        asbd.mChannelsPerFrame += increment;
        WebCore::CAAudioStreamDescription d = asbd;
        EXPECT_EQ(d.format(), WebCore::AudioStreamDescription::None) << increment;
    }
}


using SampleTypes = ::testing::Types<float, double, int16_t, int32_t>;

REGISTER_TYPED_TEST_SUITE_P(EachSampleTypeTest,
    CreateWorks, FormatFailsBytesPerFrame, FormatFailsBitsPerChannel, FormatFailsChannelsPerFrame);

INSTANTIATE_TYPED_TEST_SUITE_P(AudioStreamDescriptionCocoa, EachSampleTypeTest, SampleTypes);

#endif
