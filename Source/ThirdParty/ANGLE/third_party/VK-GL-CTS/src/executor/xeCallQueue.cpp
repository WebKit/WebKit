/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief Cross-thread function call dispatcher.
 *//*--------------------------------------------------------------------*/

#include "xeCallQueue.hpp"
#include "deInt32.h"
#include "deMemory.h"

using std::vector;

static inline int getNextQueueSize(int curSize, int minNewSize)
{
    return de::max(curSize * 2, 1 << deLog2Ceil32(minNewSize));
}

namespace xe
{

// CallQueue

CallQueue::CallQueue(void) : m_canceled(false), m_callSem(0), m_callQueue(64)
{
}

CallQueue::~CallQueue(void)
{
    // Destroy all calls.
    for (vector<Call *>::iterator i = m_calls.begin(); i != m_calls.end(); i++)
        delete *i;
}

void CallQueue::cancel(void)
{
    m_canceled = true;
    m_callSem.increment();
}

void CallQueue::callNext(void)
{
    Call *call = nullptr;

    // Wait for a call.
    m_callSem.decrement();

    if (m_canceled)
        return;

    // Acquire call from buffer.
    {
        de::ScopedLock lock(m_lock);
        call = m_callQueue.popBack();
    }

    try
    {
        // \note Enqueue lock is not held during call so it is possible to enqueue more work from dispatched call.
        CallReader reader(call);

        call->getFunction()(reader);

        // check callee consumed all
        DE_ASSERT(reader.isDataConsumed());
        call->clear();
    }
    catch (const std::exception &)
    {
        try
        {
            // Try to push call into free calls list.
            de::ScopedLock lock(m_lock);
            m_freeCalls.push_back(call);
        }
        catch (const std::exception &)
        {
            // We can't do anything but ignore this.
        }

        throw;
    }

    // Push back to free calls list.
    {
        de::ScopedLock lock(m_lock);
        m_freeCalls.push_back(call);
    }
}

Call *CallQueue::getEmptyCall(void)
{
    de::ScopedLock lock(m_lock);
    Call *call = nullptr;

    // Try to get from free calls list.
    if (!m_freeCalls.empty())
    {
        call = m_freeCalls.back();
        m_freeCalls.pop_back();
    }

    // If no free calls were available, create a new.
    if (!call)
    {
        m_calls.reserve(m_calls.size() + 1);
        call = new Call();
        m_calls.push_back(call);
    }

    return call;
}

void CallQueue::enqueue(Call *call)
{
    de::ScopedLock lock(m_lock);

    if (m_callQueue.getNumFree() == 0)
    {
        // Call queue must be grown.
        m_callQueue.resize(getNextQueueSize(m_callQueue.getSize(), m_callQueue.getSize() + 1));
    }

    m_callQueue.pushFront(call);
    m_callSem.increment();
}

void CallQueue::freeCall(Call *call)
{
    de::ScopedLock lock(m_lock);
    m_freeCalls.push_back(call);
}

// Call

Call::Call(void) : m_func(nullptr)
{
}

Call::~Call(void)
{
}

void Call::clear(void)
{
    m_func = nullptr;
    m_data.clear();
}

// CallReader

CallReader::CallReader(Call *call) : m_call(call), m_curPos(0)
{
}

void CallReader::read(uint8_t *bytes, size_t numBytes)
{
    DE_ASSERT(m_curPos + numBytes <= m_call->getDataSize());
    deMemcpy(bytes, m_call->getData() + m_curPos, numBytes);
    m_curPos += numBytes;
}

const uint8_t *CallReader::getDataBlock(size_t numBytes)
{
    DE_ASSERT(m_curPos + numBytes <= m_call->getDataSize());

    const uint8_t *ptr = m_call->getData() + m_curPos;
    m_curPos += numBytes;

    return ptr;
}

bool CallReader::isDataConsumed(void) const
{
    return m_curPos == m_call->getDataSize();
}

CallReader &operator>>(CallReader &reader, std::string &value)
{
    value.clear();
    for (;;)
    {
        char c;
        reader.read((uint8_t *)&c, sizeof(char));
        if (c != 0)
            value.push_back(c);
        else
            break;
    }

    return reader;
}

// CallWriter

CallWriter::CallWriter(CallQueue *queue, Call::Function function)
    : m_queue(queue)
    , m_call(queue->getEmptyCall())
    , m_enqueued(false)
{
    m_call->setFunction(function);
}

CallWriter::~CallWriter(void)
{
    if (!m_enqueued)
        m_queue->freeCall(m_call);
}

void CallWriter::write(const uint8_t *bytes, size_t numBytes)
{
    DE_ASSERT(!m_enqueued);
    size_t curPos = m_call->getDataSize();
    m_call->setDataSize(curPos + numBytes);
    deMemcpy(m_call->getData() + curPos, bytes, numBytes);
}

void CallWriter::enqueue(void)
{
    DE_ASSERT(!m_enqueued);
    m_queue->enqueue(m_call);
    m_enqueued = true;
}

CallWriter &operator<<(CallWriter &writer, const char *str)
{
    int pos = 0;
    for (;;)
    {
        writer.write((const uint8_t *)str + pos, sizeof(char));
        if (str[pos] == 0)
            break;
        pos += 1;
    }

    return writer;
}

} // namespace xe
