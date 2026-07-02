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
 * \brief Block-based thread-safe queue.
 *//*--------------------------------------------------------------------*/

#include "deBlockBuffer.hpp"
#include "deRandom.hpp"
#include "deThread.hpp"
#include "deInt32.h"
#include "deMemory.h"

#include <vector>

namespace de
{

using std::vector;

namespace BlockBufferBasicTest
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

typedef BlockBuffer<Message> MessageBuffer;

class Consumer : public Thread
{
public:
    Consumer(MessageBuffer &buffer, int numProducers) : m_buffer(buffer)
    {
        m_lastPayload.resize(numProducers, 0);
        m_payloadSum.resize(numProducers, 0);
    }

    void run(void)
    {
        Random rnd((uint32_t)m_lastPayload.size());
        Message tmpBuf[64];
        bool consume = true;

        while (consume)
        {
            int numToRead = rnd.getInt(1, DE_LENGTH_OF_ARRAY(tmpBuf));
            int numRead   = m_buffer.tryRead(numToRead, &tmpBuf[0]);

            for (int ndx = 0; ndx < numRead; ndx++)
            {
                const Message &msg = tmpBuf[ndx];

                uint16_t threadId = msg.getThreadId();

                if (threadId == 0xffff)
                {
                    /* Feed back rest of messages to buffer (they are end messages) so other consumers wake up. */
                    if (ndx + 1 < numRead)
                    {
                        m_buffer.write(numRead - ndx - 1, &tmpBuf[ndx + 1]);
                        m_buffer.flush();
                    }

                    consume = false;
                    break;
                }
                else
                {
                    /* Verify message. */
                    DE_TEST_ASSERT(de::inBounds<int>(threadId, 0, (int)m_lastPayload.size()));
                    DE_TEST_ASSERT((m_lastPayload[threadId] == 0 && msg.getPayload() == 0) ||
                                   m_lastPayload[threadId] < msg.getPayload());

                    m_lastPayload[threadId] = msg.getPayload();
                    m_payloadSum[threadId] += (uint32_t)msg.getPayload();
                }
            }
        }
    }

    uint32_t getPayloadSum(uint16_t threadId) const
    {
        return m_payloadSum[threadId];
    }

private:
    MessageBuffer &m_buffer;
    vector<uint16_t> m_lastPayload;
    vector<uint32_t> m_payloadSum;
};

class Producer : public Thread
{
public:
    Producer(MessageBuffer &buffer, uint16_t threadId, int numMessages)
        : m_buffer(buffer)
        , m_threadId(threadId)
        , m_numMessages(numMessages)
    {
    }

    void run(void)
    {
        // Yield to give main thread chance to start other producers.
        deSleep(1);

        Random rnd(m_threadId);
        int msgNdx = 0;
        Message tmpBuf[64];

        while (msgNdx < m_numMessages)
        {
            int writeSize = rnd.getInt(1, de::min(m_numMessages - msgNdx, DE_LENGTH_OF_ARRAY(tmpBuf)));
            for (int ndx = 0; ndx < writeSize; ndx++)
                tmpBuf[ndx] = Message(m_threadId, (uint16_t)msgNdx++);

            m_buffer.write(writeSize, &tmpBuf[0]);
            if (rnd.getBool())
                m_buffer.flush();
        }
    }

private:
    MessageBuffer &m_buffer;
    uint16_t m_threadId;
    int m_numMessages;
};

void runTest(void)
{
    const int numIterations = 8;
    for (int iterNdx = 0; iterNdx < numIterations; iterNdx++)
    {
        Random rnd(iterNdx);
        int numBlocks    = rnd.getInt(2, 128);
        int blockSize    = rnd.getInt(1, 16);
        int numProducers = rnd.getInt(1, 16);
        int numConsumers = rnd.getInt(1, 16);
        int dataSize     = rnd.getInt(50, 200);
        MessageBuffer buffer(blockSize, numBlocks);
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
        const Message endMsg(0xffff, 0);
        for (int i = 0; i < numConsumers; i++)
            buffer.write(1, &endMsg);
        buffer.flush();

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

} // namespace BlockBufferBasicTest

namespace BlockBufferCancelTest
{

class Producer : public Thread
{
public:
    Producer(BlockBuffer<uint8_t> *buffer, uint32_t seed) : m_buffer(buffer), m_seed(seed)
    {
    }

    void run(void)
    {
        uint8_t tmp[1024];
        Random rnd(m_seed);

        deMemset(tmp, 0, DE_LENGTH_OF_ARRAY(tmp));

        for (;;)
        {
            int blockSize = rnd.getInt(1, DE_LENGTH_OF_ARRAY(tmp));

            try
            {
                m_buffer->write(blockSize, &tmp[0]);

                if (rnd.getBool())
                    m_buffer->flush();
            }
            catch (const BlockBuffer<uint8_t>::CanceledException &)
            {
                break;
            }
        }
    }

private:
    BlockBuffer<uint8_t> *m_buffer;
    uint32_t m_seed;
};

class Consumer : public Thread
{
public:
    Consumer(BlockBuffer<uint8_t> *buffer, uint32_t seed) : m_buffer(buffer), m_seed(seed)
    {
    }

    void run(void)
    {
        uint8_t tmp[1024];
        Random rnd(m_seed);

        for (;;)
        {
            int blockSize = rnd.getInt(1, DE_LENGTH_OF_ARRAY(tmp));

            try
            {
                m_buffer->read(blockSize, &tmp[0]);
            }
            catch (const BlockBuffer<uint8_t>::CanceledException &)
            {
                break;
            }
        }
    }

private:
    BlockBuffer<uint8_t> *m_buffer;
    uint32_t m_seed;
};

void runTest(void)
{
    BlockBuffer<uint8_t> buffer(64, 16);
    const int numIterations = 8;

    for (int iterNdx = 0; iterNdx < numIterations; iterNdx++)
    {
        Random rnd(deInt32Hash(iterNdx));
        int numThreads = rnd.getInt(1, 16);
        int sleepMs    = rnd.getInt(1, 200);
        vector<Thread *> threads;

        for (int i = 0; i < numThreads; i++)
        {
            if (rnd.getBool())
                threads.push_back(new Consumer(&buffer, rnd.getUint32()));
            else
                threads.push_back(new Producer(&buffer, rnd.getUint32()));
        }

        // Start threads.
        for (vector<Thread *>::iterator i = threads.begin(); i != threads.end(); i++)
            (*i)->start();

        // Sleep for a while.
        deSleep(sleepMs);

        // Cancel buffer.
        buffer.cancel();

        // Wait for threads to finish.
        for (vector<Thread *>::iterator i = threads.begin(); i != threads.end(); i++)
            (*i)->join();

        // Reset buffer.
        buffer.clear();

        // Delete threads
        for (vector<Thread *>::iterator thread = threads.begin(); thread != threads.end(); ++thread)
            delete *thread;
    }
}

} // namespace BlockBufferCancelTest

void BlockBuffer_selfTest(void)
{
    BlockBufferBasicTest::runTest();
    BlockBufferCancelTest::runTest();
}

} // namespace de
