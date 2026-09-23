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

#include <array>
#include <chrono>
#include <thread>

#include <wtf/ASCIICType.h>
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

static ASCIILiteral textItem(size_t index)
{
    static constexpr std::array items { "first"_s, "second"_s, "third"_s, "fourth"_s, "fifth"_s, "sixth"_s };
    return index < items.size() ? items[index] : nullptr;
}

static ASCIICString toUpper(const ASCIICString& lower)
{
    ASCIICString upper = lower;
    for (auto& character : upper.mutableSpan())
        character = toASCIIUpper(character);
    return upper;
}

template <size_t BufferSize>
class ToUpperConverter {
public:
    ToUpperConverter()
        : m_lowerQueue(SynchronizedFixedQueue<ASCIICString, BufferSize>::create())
        , m_upperQueue(SynchronizedFixedQueue<ASCIICString, BufferSize>::create())
    {
    }

    WorkQueue* produceQueue()
    {
        if (!m_produceQueue)
            m_produceQueue = WorkQueue::create("org.webkit.Produce"_s);
        return m_produceQueue.get();
    }

    WorkQueue* consumeQueue()
    {
        if (!m_consumeQueue)
            m_consumeQueue = WorkQueue::create("org.webkit.Consume"_s);
        return m_consumeQueue.get();
    }

    void startProducing()
    {
        if (isProducing())
            return;

        produceQueue()->dispatch([this] {
            ASCIICString lower;
            while (m_lowerQueue->dequeue(lower)) {
                m_upperQueue->enqueue(toUpper(lower));
                EXPECT_TRUE(lower == textItem(m_produceCount++));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            m_produceCloseSemaphore.signal();
        });
    }

    void startConsuming()
    {
        if (isConsuming())
            return;

        consumeQueue()->dispatch([this] {
            ASCIICString upper;
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

    void enqueueLower(const ASCIICString& lower)
    {
        m_lowerQueue->enqueue(lower);
    }

    bool isProducing() { return m_produceQueue; }
    bool isConsuming() { return m_consumeQueue; }

    const std::atomic<size_t>& produceCount() const { return m_produceCount; }
    const std::atomic<size_t>& consumeCount() const { return m_consumeCount; }

private:
    Ref<SynchronizedFixedQueue<ASCIICString, BufferSize>> m_lowerQueue;
    Ref<SynchronizedFixedQueue<ASCIICString, BufferSize>> m_upperQueue;
    RefPtr<WorkQueue> m_produceQueue;
    RefPtr<WorkQueue> m_consumeQueue;
    BinarySemaphore m_produceCloseSemaphore;
    BinarySemaphore m_consumeCloseSemaphore;
    std::atomic<size_t> m_produceCount { 0 };
    std::atomic<size_t> m_consumeCount { 0 };
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
    ToUpperConverter<8U> converter;
    
    converter.startProducing();
    EXPECT_TRUE(converter.isProducing() && !converter.isConsuming());

    size_t count = 0;
    for (auto item = textItem(0); !item.isNull(); item = textItem(++count)) {
        converter.enqueueLower(item);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (converter.produceCount() < count)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    converter.stop();
    EXPECT_FALSE(converter.isProducing() || converter.isConsuming());

    EXPECT_EQ(converter.produceCount(), count);
    EXPECT_EQ(converter.consumeCount(), 0U);
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
    for (auto item = textItem(0); !item.isNull(); item = textItem(++count)) {
        converter.enqueueLower(item);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (converter.consumeCount() < count)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    converter.stop();
    EXPECT_FALSE(converter.isProducing() || converter.isConsuming());
    
    EXPECT_EQ(converter.produceCount(), count);
    EXPECT_EQ(converter.consumeCount(), count);
}

}
