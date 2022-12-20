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

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)

#include "Test.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace TestWebKitAPI {

class CARingBufferTest : public testing::Test {
public:

    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }

    // CAAudioStreamDescription(double sampleRate, UInt32 numChannels, PCMFormat format, bool isInterleaved, size_t capacity)
    void setup(double sampleRate, UInt32 numChannels, CAAudioStreamDescription::PCMFormat format, bool isInterleaved, size_t capacity)
    {
        m_description = CAAudioStreamDescription(sampleRate, numChannels, format, isInterleaved);
        m_capacity = capacity;
        size_t listSize = offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * std::max<uint32_t>(1, m_description->numberOfChannelStreams()));
        m_bufferList = std::unique_ptr<AudioBufferList>(static_cast<AudioBufferList*>(::operator new (listSize)));
        m_ringBuffer = InProcessCARingBuffer::allocate(*m_description, capacity);
    }

    void setListDataBuffer(uint8_t* bufferData, size_t sampleCount)
    {
        size_t bufferCount = m_description->numberOfChannelStreams();
        size_t channelCount = m_description->numberOfInterleavedChannels();
        size_t bytesPerChannel = sampleCount * m_description->bytesPerFrame();

        m_bufferList->mNumberBuffers = bufferCount;
        for (unsigned i = 0; i < bufferCount; ++i) {
            m_bufferList->mBuffers[i].mNumberChannels = channelCount;
            m_bufferList->mBuffers[i].mDataByteSize = bytesPerChannel;
            m_bufferList->mBuffers[i].mData = bufferData;
            if (bufferData)
                bufferData = bufferData + bytesPerChannel;
        }
    }

    const CAAudioStreamDescription& description() const { return *m_description; }
    AudioBufferList& bufferList() const { return *m_bufferList.get(); }
    CARingBuffer& ringBuffer() const { return *m_ringBuffer.get(); }
    size_t capacity() const { return m_capacity; }

private:

    std::unique_ptr<AudioBufferList> m_bufferList;
    std::unique_ptr<InProcessCARingBuffer> m_ringBuffer;
    std::optional<CAAudioStreamDescription> m_description;
    size_t m_capacity = { 0 };
};

static CARingBuffer::TimeBounds makeBounds(uint64_t start, uint64_t end)
{
    return { start, end };
}

TEST_F(CARingBufferTest, Basics)
{
    const size_t capacity = 32;

    setup(44100, 1, CAAudioStreamDescription::PCMFormat::Float32, true, capacity);

    auto fetchBounds = ringBuffer().getFetchTimeBounds();
    EXPECT_EQ(fetchBounds, makeBounds(0, 0));

    float sourceBuffer[capacity];
    for (size_t i = 0; i < capacity; i++)
        sourceBuffer[i] = i + 0.5;

    setListDataBuffer(reinterpret_cast<uint8_t*>(sourceBuffer), capacity);

    // Fill the first half of the buffer ...
    uint64_t sampleCount = capacity / 2;
    CARingBuffer::Error err = ringBuffer().store(&bufferList(), sampleCount, 0);
    EXPECT_EQ(err, CARingBuffer::Error::Ok);

    fetchBounds = ringBuffer().getFetchTimeBounds();
    EXPECT_EQ(fetchBounds, makeBounds(0, sampleCount));

    float scratchBuffer[capacity];
    setListDataBuffer(reinterpret_cast<uint8_t*>(scratchBuffer), capacity);

    ringBuffer().fetch(&bufferList(), sampleCount, 0);
    EXPECT_TRUE(!memcmp(sourceBuffer, scratchBuffer, sampleCount * description().sampleWordSize()));

    // ... and the second half.
    err = ringBuffer().store(&bufferList(), capacity / 2, capacity / 2);
    EXPECT_EQ(err, CARingBuffer::Error::Ok);

    fetchBounds = ringBuffer().getFetchTimeBounds();
    EXPECT_EQ(fetchBounds, makeBounds(0, capacity));

    memset(scratchBuffer, 0, sampleCount * description().sampleWordSize());
    ringBuffer().fetch(&bufferList(), sampleCount, 0);
    EXPECT_TRUE(!memcmp(sourceBuffer, scratchBuffer, sampleCount * description().sampleWordSize()));

    // Force the buffer to wrap around
    err = ringBuffer().store(&bufferList(), capacity, capacity - 1);
    EXPECT_EQ(err, CARingBuffer::Error::Ok);

    fetchBounds = ringBuffer().getFetchTimeBounds();
    EXPECT_EQ(fetchBounds, makeBounds(capacity - 1, capacity - 1 + capacity));

    // Make sure it returns an error when asked to store too much ...
    err = ringBuffer().store(&bufferList(), capacity * 3, capacity / 2);
    EXPECT_EQ(err, CARingBuffer::Error::TooMuch);

    // ... and doesn't modify the buffer
    fetchBounds = ringBuffer().getFetchTimeBounds();
    EXPECT_EQ(fetchBounds, makeBounds(capacity - 1, capacity - 1 + capacity));
}

TEST_F(CARingBufferTest, SmallBufferListForFetch)
{
    const int capacity = 32;
    const int halfCapacity = capacity / 2;
    setup(44100, 1, CAAudioStreamDescription::PCMFormat::Float32, true, capacity);

    float sourceBuffer[capacity];
    for (int i = 0; i < capacity; i++)
        sourceBuffer[i] = i + 0.5;
    setListDataBuffer(reinterpret_cast<uint8_t*>(sourceBuffer), capacity);
    CARingBuffer::Error err = ringBuffer().store(&bufferList(), capacity, 0);
    EXPECT_EQ(err, CARingBuffer::Error::Ok);

    float destinationBuffer[halfCapacity];
    setListDataBuffer(reinterpret_cast<uint8_t*>(destinationBuffer), halfCapacity);
    int bufferCount = bufferList().mNumberBuffers;
    EXPECT_GE(bufferCount, 1);
    size_t listDataByteSizeBeforeFetch = bufferList().mBuffers[0].mDataByteSize;

    ringBuffer().fetch(&bufferList(), capacity, 0);
    EXPECT_LE(bufferList().mBuffers[0].mDataByteSize, listDataByteSizeBeforeFetch);
}

template <typename type>
class MixingTest {
public:
    static void run(CARingBufferTest& test)
    {
        const int sampleCount = 441;

        CAAudioStreamDescription::PCMFormat format;
        if (std::is_same<type, float>::value)
            format = CAAudioStreamDescription::PCMFormat::Float32;
        else if (std::is_same<type, double>::value)
            format = CAAudioStreamDescription::PCMFormat::Float64;
        else if (std::is_same<type, int32_t>::value)
            format = CAAudioStreamDescription::PCMFormat::Int32;
        else if (std::is_same<type, int16_t>::value)
            format = CAAudioStreamDescription::PCMFormat::Int16;
        else
            ASSERT_NOT_REACHED();

        test.setup(44100, 1, format, true, sampleCount);

        type referenceBuffer[sampleCount];
        type sourceBuffer[sampleCount];
        type readBuffer[sampleCount];

        for (int i = 0; i < sampleCount; i++) {
            sourceBuffer[i] = i * 0.5;
            referenceBuffer[i] = sourceBuffer[i];
        }

        test.setListDataBuffer(reinterpret_cast<uint8_t*>(sourceBuffer), sampleCount);
        CARingBuffer::Error err = test.ringBuffer().store(&test.bufferList(), sampleCount, 0);
        EXPECT_EQ(err, CARingBuffer::Error::Ok);

        memset(readBuffer, 0, sampleCount * test.description().sampleWordSize());
        test.setListDataBuffer(reinterpret_cast<uint8_t*>(readBuffer), sampleCount);
        auto mixFetchMode = CARingBuffer::fetchModeForMixing(test.description().format());
        test.ringBuffer().fetch(&test.bufferList(), sampleCount, 0, mixFetchMode);

        for (int i = 0; i < sampleCount; i++)
            EXPECT_EQ(readBuffer[i], referenceBuffer[i]) << "Ring buffer value differs at index " << i;

        test.ringBuffer().fetch(&test.bufferList(), sampleCount, 0, mixFetchMode);
        test.ringBuffer().fetch(&test.bufferList(), sampleCount, 0, mixFetchMode);
        test.ringBuffer().fetch(&test.bufferList(), sampleCount, 0, mixFetchMode);

        for (int i = 0; i < sampleCount; i++)
            referenceBuffer[i] += sourceBuffer[i] * 3;
        
        for (int i = 0; i < sampleCount; i++)
            EXPECT_EQ(readBuffer[i], referenceBuffer[i]) << "Ring buffer value differs at index " << i;

        test.ringBuffer().fetch(&test.bufferList(), sampleCount, 0, CARingBuffer::FetchMode::Copy);
        err = test.ringBuffer().store(&test.bufferList(), sampleCount, sampleCount);
        EXPECT_EQ(err, CARingBuffer::Error::Ok);

        test.ringBuffer().fetch(&test.bufferList(), sampleCount, sampleCount, CARingBuffer::FetchMode::Copy);
        test.ringBuffer().fetch(&test.bufferList(), sampleCount, sampleCount, mixFetchMode);
        test.ringBuffer().fetch(&test.bufferList(), sampleCount, sampleCount, mixFetchMode);

        for (int i = 0; i < sampleCount; i++)
            referenceBuffer[i] = sourceBuffer[i] * 3;

        for (int i = 0; i < sampleCount; i++)
            EXPECT_EQ(readBuffer[i], referenceBuffer[i]) << "Ring buffer value differs at index " << i;
    }
};

TEST_F(CARingBufferTest, FloatMixing)
{
    MixingTest<float>::run(*this);
}

TEST_F(CARingBufferTest, DoubleMixing)
{
    MixingTest<double>::run(*this);
}

TEST_F(CARingBufferTest, Int32Mixing)
{
    MixingTest<int32_t>::run(*this);
}

TEST_F(CARingBufferTest, Int16Mixing)
{
    MixingTest<int16_t>::run(*this);
}

}

#endif
