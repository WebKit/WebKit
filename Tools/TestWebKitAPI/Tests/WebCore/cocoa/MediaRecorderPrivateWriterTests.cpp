/*
 * Copyright (C) 2026 Fady Farag. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_RECORDER)

#include "Test.h"
#include <WebCore/MediaRecorderPrivateWriter.h>
#include <WebCore/MediaSamplesBlock.h>
#include <wtf/Deque.h>
#include <wtf/MediaTime.h>
#include <wtf/NativePromise.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace WebCore;

class TestWriter final : public MediaRecorderPrivateWriter {
public:
    Deque<Result> plannedResults;
    Vector<MediaTime> writtenTimes;

private:
    bool segmentsMustStartWithKeyframe() const final { return false; }
    std::optional<uint8_t> addAudioTrack(const AudioInfo&) final { return 1; }
    std::optional<uint8_t> addVideoTrack(const VideoInfo&, const std::optional<CGAffineTransform>&) final { return 2; }
    bool allTracksAdded() final { return true; }
    Result writeFrame(const MediaSamplesBlock& block) final
    {
        auto result = plannedResults.isEmpty() ? Result::Success : plannedResults.takeFirst();
        if (result == Result::Success)
            writtenTimes.append(block.presentationTime());
        return result;
    }
    void forceNewSegment(const MediaTime&) final { }
    Ref<GenericPromise> close(Deque<UniqueRef<MediaSamplesBlock>>&&, const MediaTime&) final { return GenericPromise::createAndResolve(); }
};

static UniqueRef<MediaSamplesBlock> makeSampleBlock(const MediaTime& presentationTime)
{
    MediaSamplesBlock::SamplesVector items;
    items.append({ .presentationTime = presentationTime });
    return makeUniqueRef<MediaSamplesBlock>(nullptr, WTF::move(items));
}

TEST(MediaRecorderPrivateWriter, WritesAllFramesInOrder)
{
    TestWriter writer;

    Deque<UniqueRef<MediaSamplesBlock>> samples;
    for (size_t i = 0; i < 3; ++i)
        samples.append(makeSampleBlock(MediaTime(i, 1)));
    writer.writeFrames(WTF::move(samples), MediaTime(3, 1));

    ASSERT_EQ(writer.writtenTimes.size(), 3uz);
    for (size_t i = 0; i < 3; ++i)
        EXPECT_EQ(writer.writtenTimes[i], MediaTime(i, 1));
}

TEST(MediaRecorderPrivateWriter, NoFrameIsLostWhenWriterIsNotReady)
{
    TestWriter writer;
    writer.plannedResults.append(TestWriter::Result::Success);
    writer.plannedResults.append(TestWriter::Result::NotReady);

    Deque<UniqueRef<MediaSamplesBlock>> samples;
    for (size_t i = 0; i < 3; ++i)
        samples.append(makeSampleBlock(MediaTime(i, 1)));
    writer.writeFrames(WTF::move(samples), MediaTime(3, 1));

    ASSERT_EQ(writer.writtenTimes.size(), 1uz);

    writer.writeFrames({ }, MediaTime(3, 1));

    ASSERT_EQ(writer.writtenTimes.size(), 3uz);
    for (size_t i = 0; i < 3; ++i)
        EXPECT_EQ(writer.writtenTimes[i], MediaTime(i, 1));
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_RECORDER)
