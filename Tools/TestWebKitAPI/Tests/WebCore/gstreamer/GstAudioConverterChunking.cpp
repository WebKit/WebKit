/*
 * Copyright (C) 2026 Igalia, S.L.
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

#if USE(GSTREAMER)

#include "GStreamerTest.h"
#include "Helpers/Test.h"
#include <WebCore/GStreamerCommon.h>
#include <WebCore/GUniquePtrGStreamer.h>
#include <array>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using WebCore::gstAudioConverterInputFramesForOutput;

// The rate and chunk size the outgoing WebRTC audio path converts to; see LibWebRTCAudioFormat.
static constexpr int outputSampleRate = 48000;
static constexpr size_t outputChunkFrames = 480;

// Capture devices commonly deliver these; the mock microphone defaults to 44100. The fractional
// ratios (11025, 22050) alternate their per-chunk input count, which is where the phase estimate
// and the converter's real capacity disagree.
static constexpr std::array inputSampleRates { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000 };

class ChunkingConverter {
public:
    explicit ChunkingConverter(int inputSampleRate)
    {
        gst_audio_info_set_format(&m_inputInfo, GST_AUDIO_FORMAT_F32, inputSampleRate, 1, nullptr);
        gst_audio_info_set_format(&m_outputInfo, GST_AUDIO_FORMAT_S16, outputSampleRate, 1, nullptr);
        m_converter.reset(gst_audio_converter_new(GST_AUDIO_CONVERTER_FLAG_IN_WRITABLE, &m_inputInfo, &m_outputInfo, nullptr));
    }

    GstAudioConverter* get() const { return m_converter.get(); }
    size_t bufferedFrames() const { return m_input.size(); }

    std::optional<size_t> inputFramesForChunk(size_t availableFrames) { return gstAudioConverterInputFramesForOutput(get(), outputChunkFrames, availableFrames); }
    size_t phaseEstimate() { return gst_audio_converter_get_in_frames(get(), outputChunkFrames); }
    bool canProduceChunkFrom(size_t inputFrames) { return gst_audio_converter_get_out_frames(get(), inputFrames) >= outputChunkFrames; }

    bool convertChunk(size_t inputFrames)
    {
        gpointer in[1] = { m_input.mutableSpan().data() };
        gpointer out[1] = { m_output.mutableSpan().data() };
        return gst_audio_converter_samples(get(), static_cast<GstAudioConverterFlags>(0), in, inputFrames, out, outputChunkFrames);
    }

private:
    GstAudioInfo m_inputInfo;
    GstAudioInfo m_outputInfo;
    GUniquePtr<GstAudioConverter> m_converter;
    Vector<float> m_input { FillWith { }, 8192, 0 };
    Vector<int16_t> m_output { FillWith { }, outputChunkFrames, 0 };
};

// The converter's own capacity report is the contract: hand out a chunk only when it can be filled,
// never larger than the frames buffered, and never smaller than the converter needs, otherwise the
// resampler reads past the end of its input. Walk the availability boundary at each priming stage,
// since the first chunk also has to cover the filter history.
TEST_F(GStreamerTest, audioConverterInputChunkMatchesConverterCapacity)
{
    for (auto inputSampleRate : inputSampleRates) {
        ChunkingConverter converter(inputSampleRate);
        ASSERT_TRUE(converter.get());

        for (unsigned stage = 0; stage < 8; stage++) {
            auto estimate = converter.phaseEstimate();
            for (size_t available = estimate > 8 ? estimate - 8 : 0; available <= estimate + 8; available++) {
                auto frames = converter.inputFramesForChunk(available);
                if (!converter.canProduceChunkFrom(available)) {
                    EXPECT_FALSE(frames.has_value()) << "rate " << inputSampleRate << " stage " << stage << " available " << available;
                    continue;
                }
                ASSERT_TRUE(frames.has_value()) << "rate " << inputSampleRate << " stage " << stage << " available " << available;
                EXPECT_LE(*frames, available) << "rate " << inputSampleRate << " stage " << stage;
                EXPECT_TRUE(converter.canProduceChunkFrom(*frames)) << "rate " << inputSampleRate << " stage " << stage << " chunk " << *frames;
            }

            auto frames = converter.inputFramesForChunk(converter.bufferedFrames());
            ASSERT_TRUE(frames.has_value());
            ASSERT_TRUE(converter.convertChunk(*frames));
        }
    }
}

// Once the filter history is primed, every chunk has to match the phase estimate; a larger one would
// consume input faster than the sample rates allow.
TEST_F(GStreamerTest, audioConverterInputChunkSettlesOnPhaseEstimate)
{
    for (auto inputSampleRate : inputSampleRates) {
        ChunkingConverter converter(inputSampleRate);
        ASSERT_TRUE(converter.get());

        auto first = converter.inputFramesForChunk(converter.bufferedFrames());
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(converter.convertChunk(*first));

        for (unsigned chunk = 0; chunk < 8; chunk++) {
            auto frames = converter.inputFramesForChunk(converter.bufferedFrames());
            ASSERT_TRUE(frames.has_value());
            EXPECT_EQ(*frames, converter.phaseEstimate()) << "rate " << inputSampleRate << " chunk " << chunk;
            ASSERT_TRUE(converter.convertChunk(*frames));
        }
    }
}

} // namespace TestWebKitAPI

#endif // USE(GSTREAMER)
