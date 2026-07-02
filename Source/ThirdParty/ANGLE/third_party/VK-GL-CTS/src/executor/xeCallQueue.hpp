#ifndef _XECALLQUEUE_HPP
#define _XECALLQUEUE_HPP
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

#include "xeDefs.hpp"
#include "deMutex.hpp"
#include "deSemaphore.hpp"
#include "deRingBuffer.hpp"

#include <vector>

namespace xe
{

class Call;
class CallReader;
class CallWriter;
class CallQueue;

// \todo [2012-07-10 pyry] Optimize memory management in Call
// \todo [2012-07-10 pyry] CallQueue API could be improved to match TestLog API more closely.
//                           In order to do that, reference counting system for call object management is needed.

class Call
{
public:
    typedef void (*Function)(CallReader &data);

    Call(void);
    ~Call(void);

    void clear(void);

    Function getFunction(void) const
    {
        return m_func;
    }
    void setFunction(Function func)
    {
        m_func = func;
    }

    size_t getDataSize(void) const
    {
        return m_data.size();
    }
    void setDataSize(size_t size)
    {
        m_data.resize(size);
    }

    const uint8_t *getData(void) const
    {
        return m_data.empty() ? nullptr : &m_data[0];
    }
    uint8_t *getData(void)
    {
        return m_data.empty() ? nullptr : &m_data[0];
    }

private:
    Function m_func;
    std::vector<uint8_t> m_data;
};

class CallReader
{
public:
    CallReader(Call *call);
    CallReader(void) : m_call(nullptr), m_curPos(0)
    {
    }

    void read(uint8_t *bytes, size_t numBytes);
    const uint8_t *getDataBlock(size_t numBytes); //!< \note Valid only during call.
    bool isDataConsumed(void) const;              //!< all data has been consumed

private:
    CallReader(const CallReader &other);            //!< disallowed
    CallReader &operator=(const CallReader &other); //!< disallowed

    Call *m_call;
    size_t m_curPos;
};

class CallWriter
{
public:
    CallWriter(CallQueue *queue, Call::Function function);
    ~CallWriter(void);

    void write(const uint8_t *bytes, size_t numBytes);
    void enqueue(void);

private:
    CallWriter(const CallWriter &other);
    CallWriter &operator=(const CallWriter &other);

    CallQueue *m_queue;
    Call *m_call;
    bool m_enqueued;
};

class CallQueue
{
public:
    CallQueue(void);
    ~CallQueue(void);

    void callNext(void); //!< Executes and removes first call in queue. Will block if queue is empty.

    Call *getEmptyCall(void);
    void enqueue(Call *call);
    void freeCall(Call *call);
    void cancel(void);

private:
    CallQueue(const CallQueue &other);
    CallQueue &operator=(const CallQueue &other);

    bool m_canceled;
    de::Semaphore m_callSem;

    de::Mutex m_lock;
    std::vector<Call *> m_calls;
    std::vector<Call *> m_freeCalls;
    de::RingBuffer<Call *> m_callQueue;
};

// Stream operators for call reader / writer.

CallReader &operator>>(CallReader &reader, std::string &value);
CallWriter &operator<<(CallWriter &writer, const char *str);

template <typename T>
CallReader &operator>>(CallReader &reader, T &value)
{
    reader.read((uint8_t *)&value, sizeof(T));
    return reader;
}

template <typename T>
CallWriter &operator<<(CallWriter &writer, T &value)
{
    writer.write((const uint8_t *)&value, sizeof(T));
    return writer;
}

} // namespace xe

#endif // _XECALLQUEUE_HPP
