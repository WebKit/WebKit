/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Thread-safe ring buffer template.
 *//*--------------------------------------------------------------------*/

#include "deThreadSafeRingBuffer.hpp"
#include "deRandom.hpp"
#include "deThread.hpp"

#include <vector>

using std::vector;

namespace de
{

namespace
{

struct Message
{
    uint32_t data;

    Message(uint16_t threadId, uint16_t payload) : data((threadId << 16) | payload)
    {
    }

    Message(void) : data(0)
    {
    }

    uint16_t getThreadId(void) const
    {
        return (uint16_t)(data >> 16);
    }
    uint16_t getPayload(void) const
    {
        return (uint16_t)(data & 0xffff);
    }
};

class Consumer : public Thread
{
public:
    Consumer(ThreadSafeRingBuffer<Message> &buffer, int numProducers) : m_buffer(buffer)
    {
        m_lastPayload.resize(numProducers, 0);
        m_payloadSum.resize(numProducers, 0);
    }

    void run(void)
    {
        for (;;)
        {
            Message msg = m_buffer.popBack();

            uint16_t threadId = msg.getThreadId();

            if (threadId == 0xffff)
                break;

            DE_TEST_ASSERT(de::inBounds<int>(threadId, 0, (int)m_lastPayload.size()));
            DE_TEST_ASSERT((m_lastPayload[threadId] == 0 && msg.getPayload() == 0) ||
                           m_lastPayload[threadId] < msg.getPayload());

            m_lastPayload[threadId] = msg.getPayload();
            m_payloadSum[threadId] += (uint32_t)msg.getPayload();
        }
    }

    uint32_t getPayloadSum(uint16_t threadId) const
    {
        return m_payloadSum[threadId];
    }

private:
    ThreadSafeRingBuffer<Message> &m_buffer;
    vector<uint16_t> m_lastPayload;
    vector<uint32_t> m_payloadSum;
};

class Producer : public Thread
{
public:
    Producer(ThreadSafeRingBuffer<Message> &buffer, uint16_t threadId, int dataSize)
        : m_buffer(buffer)
        , m_threadId(threadId)
        , m_dataSize(dataSize)
    {
    }

    void run(void)
    {
        // Yield to give main thread chance to start other producers.
        deSleep(1);

        for (int ndx = 0; ndx < m_dataSize; ndx++)
            m_buffer.pushFront(Message(m_threadId, (uint16_t)ndx));
    }

private:
    ThreadSafeRingBuffer<Message> &m_buffer;
    uint16_t m_threadId;
    int m_dataSize;
};

} // namespace

void ThreadSafeRingBuffer_selfTest(void)
{
    const int numIterations = 16;
    for (int iterNdx = 0; iterNdx < numIterations; iterNdx++)
    {
        Random rnd(iterNdx);
        int bufSize      = rnd.getInt(1, 2048);
        int numProducers = rnd.getInt(1, 16);
        int numConsumers = rnd.getInt(1, 16);
        int dataSize     = rnd.getInt(1000, 10000);
        ThreadSafeRingBuffer<Message> buffer(bufSize);
        vector<Producer *> producers;
        vector<Consumer *> consumers;

        for (int i = 0; i < numProducers; i++)
            producers.push_back(new Producer(buffer, (uint16_t)i, dataSize));

        for (int i = 0; i < numConsumers; i++)
            consumers.push_back(new Consumer(buffer, numProducers));

        // Start consumers.
        for (vector<Consumer *>::iterator i = consumers.begin(); i != consumers.end(); i++)
            (*i)->start();

        // Start producers.
        for (vector<Producer *>::iterator i = producers.begin(); i != producers.end(); i++)
            (*i)->start();

        // Wait for producers.
        for (vector<Producer *>::iterator i = producers.begin(); i != producers.end(); i++)
            (*i)->join();

        // Write end messages for consumers.
        for (int i = 0; i < numConsumers; i++)
            buffer.pushFront(Message(0xffff, 0));

        // Wait for consumers.
        for (vector<Consumer *>::iterator i = consumers.begin(); i != consumers.end(); i++)
            (*i)->join();

        // Verify payload sums.
        uint32_t refSum = 0;
        for (int i = 0; i < dataSize; i++)
            refSum += (uint32_t)(uint16_t)i;

        for (int i = 0; i < numProducers; i++)
        {
            uint32_t cmpSum = 0;
            for (int j = 0; j < numConsumers; j++)
                cmpSum += consumers[j]->getPayloadSum((uint16_t)i);
            DE_TEST_ASSERT(refSum == cmpSum);
        }

        // Free resources.
        for (vector<Producer *>::iterator i = producers.begin(); i != producers.end(); i++)
            delete *i;
        for (vector<Consumer *>::iterator i = consumers.begin(); i != consumers.end(); i++)
            delete *i;
    }
}

} // namespace de
