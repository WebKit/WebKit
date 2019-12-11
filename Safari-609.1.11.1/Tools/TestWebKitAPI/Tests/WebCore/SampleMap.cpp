/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SOURCE)

#include "Test.h"
#include <WebCore/MediaSample.h>
#include <WebCore/SampleMap.h>

namespace WTF {
inline std::ostream& operator<<(std::ostream& os, const MediaTime& time)
{
    if (time.hasDoubleValue())
        os << "{ " << time.toDouble() << " }";
    else
        os << "{ " << time.timeValue() << " / " << time.timeScale() << ", " << time.toDouble() << " }";
    return os;
}
}

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
    AtomString trackID() const final { return m_trackID; }
    void setTrackID(const String& trackID) final { m_trackID = trackID; }
    size_t sizeInBytes() const final { return m_sizeInBytes; }
    FloatSize presentationSize() const final { return m_presentationSize; }
    void offsetTimestampsBy(const MediaTime& offset) final { m_presentationTime += offset; m_decodeTime += offset; }
    void setTimestamps(const MediaTime& presentationTime, const MediaTime& decodeTime) final {
        m_presentationTime = presentationTime;
        m_decodeTime = decodeTime;
    };
    bool isDivisable() const final { return false; }
    std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime& presentationTime) final { return { }; }
    Ref<MediaSample> createNonDisplayingCopy() const final {
        return create(m_presentationTime, m_decodeTime, m_duration, static_cast<SampleFlags>(m_flags | IsNonDisplaying));
    }
    SampleFlags flags() const final { return m_flags; }
    PlatformSample platformSample() final { return { PlatformSample::None, {nullptr}}; }
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
    AtomString m_trackID;
    size_t m_sizeInBytes { 0 };
    SampleFlags m_flags { None };
};

class SampleMapTest : public testing::Test {
public:
    void SetUp() final {
        map.addSample(TestSample::create(MediaTime(0, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::IsSync));
        map.addSample(TestSample::create(MediaTime(1, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(2, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(3, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(4, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(5, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::IsSync));
        map.addSample(TestSample::create(MediaTime(6, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(7, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(8, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(9, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        // Gap at MediaTime(10, 1) -> MediaTime(11, 1);
        map.addSample(TestSample::create(MediaTime(11, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::IsSync));
        map.addSample(TestSample::create(MediaTime(12, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(13, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(14, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(15, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::IsSync));
        map.addSample(TestSample::create(MediaTime(16, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(17, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(18, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
        map.addSample(TestSample::create(MediaTime(19, 1), MediaTime(0, 1), MediaTime(1, 1), MediaSample::None));
    }

    SampleMap map;
};

TEST_F(SampleMapTest, findSampleWithPresentationTime)
{
    auto& presentationMap = map.presentationOrder();
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleWithPresentationTime(MediaTime(0, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(19, 1), presentationMap.findSampleWithPresentationTime(MediaTime(19, 1))->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleWithPresentationTime(MediaTime(-1, 1)));
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleWithPresentationTime(MediaTime(10, 1)));
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleWithPresentationTime(MediaTime(20, 1)));
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleWithPresentationTime(MediaTime(1, 2)));
}

TEST_F(SampleMapTest, findSampleContainingPresentationTime)
{
    auto& presentationMap = map.presentationOrder();
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleContainingPresentationTime(MediaTime(0, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(19, 1), presentationMap.findSampleContainingPresentationTime(MediaTime(19, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleContainingPresentationTime(MediaTime(1, 2))->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleContainingPresentationTime(MediaTime(-1, 1)));
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleContainingPresentationTime(MediaTime(10, 1)));
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleContainingPresentationTime(MediaTime(20, 1)));
}

TEST_F(SampleMapTest, findSampleStartingOnOrAfterPresentationTime)
{
    auto& presentationMap = map.presentationOrder();
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleStartingOnOrAfterPresentationTime(MediaTime(0, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(19, 1), presentationMap.findSampleStartingOnOrAfterPresentationTime(MediaTime(19, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(1, 1), presentationMap.findSampleStartingOnOrAfterPresentationTime(MediaTime(1, 2))->second->presentationTime());
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleStartingOnOrAfterPresentationTime(MediaTime(-1, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(11, 1), presentationMap.findSampleStartingOnOrAfterPresentationTime(MediaTime(10, 1))->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleContainingPresentationTime(MediaTime(20, 1)));
}

TEST_F(SampleMapTest, findSampleContainingOrAfterPresentationTime)
{
    auto& presentationMap = map.presentationOrder();
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleContainingOrAfterPresentationTime(MediaTime(0, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(19, 1), presentationMap.findSampleContainingOrAfterPresentationTime(MediaTime(19, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleContainingOrAfterPresentationTime(MediaTime(1, 2))->second->presentationTime());
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleContainingOrAfterPresentationTime(MediaTime(-1, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(11, 1), presentationMap.findSampleContainingOrAfterPresentationTime(MediaTime(10, 1))->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleContainingOrAfterPresentationTime(MediaTime(20, 1)));
}

TEST_F(SampleMapTest, findSampleStartingAfterPresentationTime)
{
    auto& presentationMap = map.presentationOrder();
    EXPECT_EQ(MediaTime(1, 1), presentationMap.findSampleStartingAfterPresentationTime(MediaTime(0, 1))->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleStartingAfterPresentationTime(MediaTime(19, 1)));
    EXPECT_EQ(MediaTime(1, 1), presentationMap.findSampleStartingAfterPresentationTime(MediaTime(1, 2))->second->presentationTime());
    EXPECT_EQ(MediaTime(0, 1), presentationMap.findSampleStartingAfterPresentationTime(MediaTime(-1, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(11, 1), presentationMap.findSampleStartingAfterPresentationTime(MediaTime(10, 1))->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == presentationMap.findSampleStartingAfterPresentationTime(MediaTime(20, 1)));
}
TEST_F(SampleMapTest, findSamplesBetweenPresentationTimes)
{
    auto& presentationMap = map.presentationOrder();
    auto iterator_range = presentationMap.findSamplesBetweenPresentationTimes(MediaTime(0, 1), MediaTime(1, 1));
    EXPECT_EQ(MediaTime(0, 1), iterator_range.first->second->presentationTime());
    EXPECT_EQ(MediaTime(1, 1), iterator_range.second->second->presentationTime());

    iterator_range = presentationMap.findSamplesBetweenPresentationTimes(MediaTime(1, 2), MediaTime(3, 2));
    EXPECT_EQ(MediaTime(1, 1), iterator_range.first->second->presentationTime());
    EXPECT_EQ(MediaTime(2, 1), iterator_range.second->second->presentationTime());

    iterator_range = presentationMap.findSamplesBetweenPresentationTimes(MediaTime(9, 1), MediaTime(21, 1));
    EXPECT_EQ(MediaTime(9, 1), iterator_range.first->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);

    iterator_range = presentationMap.findSamplesBetweenPresentationTimes(MediaTime(-1, 1), MediaTime(0, 1));
    EXPECT_TRUE(presentationMap.end() == iterator_range.first);
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);

    iterator_range = presentationMap.findSamplesBetweenPresentationTimes(MediaTime(19, 2), MediaTime(10, 1));
    EXPECT_TRUE(presentationMap.end() == iterator_range.first);
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);

    iterator_range = presentationMap.findSamplesBetweenPresentationTimes(MediaTime(20, 1), MediaTime(21, 1));
    EXPECT_TRUE(presentationMap.end() == iterator_range.first);
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);
}

TEST_F(SampleMapTest, findSamplesBetweenPresentationTimesFromEnd)
{
    auto& presentationMap = map.presentationOrder();
    auto iterator_range = presentationMap.findSamplesBetweenPresentationTimesFromEnd(MediaTime(0, 1), MediaTime(1, 1));
    EXPECT_EQ(MediaTime(0, 1), iterator_range.first->second->presentationTime());
    EXPECT_EQ(MediaTime(1, 1), iterator_range.second->second->presentationTime());

    iterator_range = presentationMap.findSamplesBetweenPresentationTimesFromEnd(MediaTime(1, 2), MediaTime(3, 2));
    EXPECT_EQ(MediaTime(1, 1), iterator_range.first->second->presentationTime());
    EXPECT_EQ(MediaTime(2, 1), iterator_range.second->second->presentationTime());

    iterator_range = presentationMap.findSamplesBetweenPresentationTimesFromEnd(MediaTime(9, 1), MediaTime(21, 1));
    EXPECT_EQ(MediaTime(9, 1), iterator_range.first->second->presentationTime());
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);

    iterator_range = presentationMap.findSamplesBetweenPresentationTimesFromEnd(MediaTime(-1, 1), MediaTime(0, 1));
    EXPECT_TRUE(presentationMap.end() == iterator_range.first);
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);

    iterator_range = presentationMap.findSamplesBetweenPresentationTimesFromEnd(MediaTime(19, 2), MediaTime(10, 1));
    EXPECT_TRUE(presentationMap.end() == iterator_range.first);
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);

    iterator_range = presentationMap.findSamplesBetweenPresentationTimesFromEnd(MediaTime(20, 1), MediaTime(21, 1));
    EXPECT_TRUE(presentationMap.end() == iterator_range.first);
    EXPECT_TRUE(presentationMap.end() == iterator_range.second);
}

TEST_F(SampleMapTest, reverseFindSampleBeforePresentationTime)
{
    auto& presentationMap = map.presentationOrder();
    EXPECT_EQ(MediaTime(0, 1), presentationMap.reverseFindSampleBeforePresentationTime(MediaTime(0, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(9, 1), presentationMap.reverseFindSampleBeforePresentationTime(MediaTime(10, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(19, 1), presentationMap.reverseFindSampleBeforePresentationTime(MediaTime(19, 1))->second->presentationTime());
    EXPECT_EQ(MediaTime(19, 1), presentationMap.reverseFindSampleBeforePresentationTime(MediaTime(21, 1))->second->presentationTime());
    EXPECT_TRUE(presentationMap.rend() == presentationMap.reverseFindSampleBeforePresentationTime(MediaTime(-1, 1)));
}

}

#endif // ENABLE(MEDIA_SOURCE)
