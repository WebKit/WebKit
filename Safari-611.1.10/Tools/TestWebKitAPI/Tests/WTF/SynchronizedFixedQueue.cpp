/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <chrono>
#include <thread>

#include <wtf/ASCIICType.h>
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

static char const* textItem(size_t index)
{
    static char const* items[] = { "first", "second", "third", "fourth", "fifth", "sixth" };
    return index < sizeof(items) / sizeof(items[0]) ? items[index] : nullptr;
}

static CString toUpper(const CString& lower)
{
    CString upper = lower;

    for (char* buffer = upper.mutableData(); *buffer; ++buffer)
        *buffer = toASCIIUpper(*buffer);

    return upper;
}

template <size_t BufferSize>
class ToUpperConverter {
public:
    ToUpperConverter()
        : m_lowerQueue(SynchronizedFixedQueue<CString, BufferSize>::create())
        , m_upperQueue(SynchronizedFixedQueue<CString, BufferSize>::create())
    {
    }

    WorkQueue* produceQueue()
    {
        if (!m_produceQueue)
            m_produceQueue = WorkQueue::create("org.webkit.Produce");
        return m_produceQueue.get();
    }

    WorkQueue* consumeQueue()
    {
        if (!m_consumeQueue)
            m_consumeQueue = WorkQueue::create("org.webkit.Consume");
        return m_consumeQueue.get();
    }

    void startProducing()
    {
        if (isProducing())
            return;

        produceQueue()->dispatch([this] {
            CString lower;
            while (m_lowerQueue->dequeue(lower)) {
                m_upperQueue->enqueue(toUpper(lower));
                EXPECT_TRUE(lower == textItem(m_produceCount++));
#if PLATFORM(WIN)
                auto sleepAmount = std::chrono::milliseconds(20);
#else
                auto sleepAmount = std::chrono::milliseconds(10);
#endif
                std::this_thread::sleep_for(sleepAmount);
            }
            m_produceCloseSemaphore.signal();
        });
    }

    void startConsuming()
    {
        if (isConsuming())
            return;

        consumeQueue()->dispatch([this] {
            CString upper;
            while (m_upperQueue->dequeue(upper)) {
                EXPECT_TRUE(upper == toUpper(textItem(m_consumeCount++)));
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            m_consumeCloseSemaphore.signal();
        });
    }
    
    void start()
    {
        startProducing();
        startConsuming();
    }

    void stopProducing()
    {
        if (!isProducing())
            return;

        m_lowerQueue->close();
        m_produceCloseSemaphore.wait();
        m_produceQueue = nullptr;
    }
    
    void stopConsuming()
    {
        if (!isConsuming())
            return;

        m_upperQueue->close();
        m_consumeCloseSemaphore.wait();
        m_consumeQueue = nullptr;
    }
    
    void stop()
    {
        stopProducing();
        stopConsuming();
    }

    void enqueueLower(const CString& lower)
    {
        m_lowerQueue->enqueue(lower);
    }

    bool isProducing() { return m_produceQueue; }
    bool isConsuming() { return m_consumeQueue; }

    size_t produceCount() const { return m_produceCount; }
    size_t consumeCount() const { return m_consumeCount; }

private:
    Ref<SynchronizedFixedQueue<CString, BufferSize>> m_lowerQueue;
    Ref<SynchronizedFixedQueue<CString, BufferSize>> m_upperQueue;
    RefPtr<WorkQueue> m_produceQueue;
    RefPtr<WorkQueue> m_consumeQueue;
    BinarySemaphore m_produceCloseSemaphore;
    BinarySemaphore m_consumeCloseSemaphore;
    size_t m_produceCount { 0 };
    size_t m_consumeCount { 0 };
};

TEST(WTF_SynchronizedFixedQueue, Basic)
{
    ToUpperConverter<4U> converter;

    converter.start();
    EXPECT_TRUE(converter.isProducing() && converter.isConsuming());

    converter.stop();
    EXPECT_FALSE(converter.isProducing() || converter.isConsuming());

    EXPECT_EQ(converter.produceCount(), 0U);
    EXPECT_EQ(converter.consumeCount(), 0U);
}

TEST(WTF_SynchronizedFixedQueue, ProduceOnly)
{
    ToUpperConverter<4U> converter;
    
    converter.startProducing();
    EXPECT_TRUE(converter.isProducing() && !converter.isConsuming());

    size_t count = 0;
    while (char const* item = textItem(count)) {
        converter.enqueueLower(item);
        ++count;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    converter.stop();
    EXPECT_FALSE(converter.isProducing() || converter.isConsuming());
}

TEST(WTF_SynchronizedFixedQueue, ConsumeOnly)
{
    ToUpperConverter<4U> converter;
    
    converter.startConsuming();
    EXPECT_TRUE(!converter.isProducing() && converter.isConsuming());
    
    converter.stop();
    EXPECT_FALSE(converter.isProducing() || converter.isConsuming());
}

TEST(WTF_SynchronizedFixedQueue, Limits)
{
    ToUpperConverter<4U> converter;

    converter.start();
    EXPECT_TRUE(converter.isProducing() && converter.isConsuming());

    size_t count = 0;
    while (char const* item = textItem(count)) {
        converter.enqueueLower(item);
        ++count;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    converter.stop();
    EXPECT_FALSE(converter.isProducing() || converter.isConsuming());
    
    EXPECT_EQ(converter.produceCount(), count);
    EXPECT_EQ(converter.consumeCount(), count);
}

}
