/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include <WebCore/MediaReorderQueue.h>

using namespace WebCore;

namespace TestWebKitAPI {

class TestSample : public MediaSample {
public:
    static Ref<TestSample> create(const MediaTime& presentationTime, const MediaTime& decodeTime, const MediaTime& duration, SampleFlags flags)
    {
        return adoptRef(*new TestSample(presentationTime, decodeTime, duration, flags));
    }

    MediaTime presentationTime() const final { return m_presentationTime; }
    MediaTime decodeTime() const final { return m_decodeTime; }
    MediaTime duration() const final { return m_duration; }
    TrackID trackID() const final { return m_trackID; }
    size_t sizeInBytes() const final { return m_sizeInBytes; }
    FloatSize presentationSize() const final { return m_presentationSize; }
    void offsetTimestampsBy(const MediaTime& offset) final { m_presentationTime += offset; m_decodeTime += offset; }
    void setTimestamps(const MediaTime& presentationTime, const MediaTime& decodeTime) final
    {
        m_presentationTime = presentationTime;
        m_decodeTime = decodeTime;
    };
    Ref<MediaSample> createNonDisplayingCopy() const final
    {
        return create(m_presentationTime, m_decodeTime, m_duration, static_cast<SampleFlags>(m_flags | IsNonDisplaying));
    }
    SampleFlags flags() const final { return m_flags; }
    PlatformSample platformSample() const final { return { PlatformSample::None, { nullptr } }; }
    PlatformSample::Type platformSampleType() const final { return PlatformSample::None; }

    void dump(PrintStream&) const final { }

private:
    TestSample(const MediaTime& presentationTime, const MediaTime& decodeTime, const MediaTime& duration, SampleFlags flags)
        : m_presentationTime(presentationTime)
        , m_decodeTime(decodeTime)
        , m_duration(duration)
        , m_flags(flags)
    {
    }

    MediaTime m_presentationTime;
    MediaTime m_decodeTime;
    MediaTime m_duration;
    FloatSize m_presentationSize;
    TrackID m_trackID;
    size_t m_sizeInBytes { 0 };
    SampleFlags m_flags { None };
};

TEST(MediaReorderQueue, MediaSample)
{
    MediaSampleReorderQueue queue;
    queue.append(TestSample::create(MediaTime(0, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::IsSync));
    queue.append(TestSample::create(MediaTime(3, 1), MediaTime(1, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(2, 1), MediaTime(2, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(1, 1), MediaTime(3, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(4, 1), MediaTime(4, 1), MediaTime(1, 1), MediaSample::IsSync));
    queue.append(TestSample::create(MediaTime(8, 1), MediaTime(5, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(7, 1), MediaTime(6, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(6, 1), MediaTime(7, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(5, 1), MediaTime(8, 1), MediaTime(1, 1), MediaSample::None));

    ASSERT_FALSE(queue.isEmpty());
    RefPtr currentSample = queue.takeFirst();
    while (!queue.isEmpty()) {
        RefPtr nextSample = queue.takeFirst();
        ASSERT_LE(currentSample->presentationTime(), nextSample->presentationTime());
        currentSample = WTFMove(nextSample);
    }
}

TEST(MediaReorderQueue, MediaSampleKeepOrderIfEqual)
{
    MediaSampleReorderQueue queue;
    queue.append(TestSample::create(MediaTime(0, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::IsSync));
    queue.append(TestSample::create(MediaTime(0, 1), MediaTime(1, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(2, 1), MediaTime(2, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(1, 1), MediaTime(3, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(4, 1), MediaTime(4, 1), MediaTime(1, 1), MediaSample::IsSync));
    queue.append(TestSample::create(MediaTime(7, 1), MediaTime(5, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(7, 1), MediaTime(6, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(7, 1), MediaTime(7, 1), MediaTime(1, 1), MediaSample::None));
    queue.append(TestSample::create(MediaTime(6, 1), MediaTime(8, 1), MediaTime(1, 1), MediaSample::None));

    ASSERT_FALSE(queue.isEmpty());
    auto current = queue.takeFirst();
    while (!queue.isEmpty()) {
        auto next = queue.takeFirst();
        if (current->presentationTime() == next->presentationTime())
            ASSERT_LT(current->decodeTime(), next->decodeTime());
        ASSERT_LE(current->presentationTime(), next->presentationTime());
        current = next;
    }
}


TEST(MediaReorderQueue, Int)
{
    MediaReorderQueue<int> queue;
    queue.append(0);
    queue.append(3);
    queue.append(2);
    queue.append(1);
    queue.append(4);
    queue.append(8);
    queue.append(7);
    queue.append(6);
    queue.append(5);

    ASSERT_FALSE(queue.isEmpty());
    auto current = queue.takeFirst();
    while (!queue.isEmpty()) {
        auto next = queue.takeFirst();
        ASSERT_LT(current, next);
        current = next;
    }
}

TEST(MediaReorderQueue, Pair)
{
    MediaReorderQueue<std::pair<int, int>> queue;
    queue.append({ 0, 1 });
    queue.append({ 0, 2 });
    queue.append({ 2, 3 });
    queue.append({ 1, 4 });
    queue.append({ 4, 5 });
    queue.append({ 8, 6 });
    queue.append({ 7, 7 });
    queue.append({ 6, 8 });
    queue.append({ 5, 9 });

    ASSERT_FALSE(queue.isEmpty());
    auto current = queue.takeFirst();
    while (!queue.isEmpty()) {
        auto next = queue.takeFirst();
        if (current.first == next.first)
            ASSERT_LT(current.second, next.second);
        ASSERT_LE(current, next);
        current = next;
    }
}

TEST(MediaReorderQueue, takeIfAvailable)
{
    MediaReorderQueue<int> queue;
    queue.setReorderSize(4);
    bool moreObjectsAvailable = false;
    auto object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_TRUE(!object);
    queue.append(0);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_TRUE(!object);
    queue.append(3);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_TRUE(!object);
    queue.append(2);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_TRUE(!object);
    queue.append(1);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_TRUE(!object);
    queue.append(4);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_FALSE(!object);
    ASSERT_EQ(0, *object);
    queue.append(8);
    queue.append(7);
    queue.append(6);
    queue.append(5);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_TRUE(moreObjectsAvailable);
    ASSERT_FALSE(!object);
    ASSERT_EQ(1, *object);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_TRUE(moreObjectsAvailable);
    ASSERT_FALSE(!object);
    ASSERT_EQ(2, *object);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_TRUE(moreObjectsAvailable);
    ASSERT_FALSE(!object);
    ASSERT_EQ(3, *object);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_FALSE(!object);
    ASSERT_EQ(4, *object);
    object = queue.takeIfAvailable(moreObjectsAvailable);
    ASSERT_FALSE(moreObjectsAvailable);
    ASSERT_TRUE(!object);
}

} // namespace TestWebKitAPI
