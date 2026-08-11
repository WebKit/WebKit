#ifndef _TCUTHREADUTIL_HPP
#define _TCUTHREADUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Thread test utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deSharedPtr.hpp"
#include "deMutex.hpp"
#include "deSemaphore.hpp"
#include "deThread.hpp"
#include "deRandom.hpp"

#include <vector>
#include <sstream>

namespace tcu
{
namespace ThreadUtil
{
// Event object for synchronizing threads
class Event
{
public:
    enum Result
    {
        RESULT_NOT_READY = 0,
        RESULT_OK,
        RESULT_FAILED
    };

    Event(void);
    ~Event(void);
    void setResult(Result result);
    Result waitReady(void);
    Result getResult(void) const
    {
        return m_result;
    }

private:
    volatile Result m_result;
    volatile int m_waiterCount;
    de::Semaphore m_waiters;
    de::Mutex m_lock;

    // Disabled
    Event(const Event &);
    Event &operator=(const Event &);
};

// Base class for objects which modifications should be tracked between threads
class Object
{
public:
    Object(const char *type, de::SharedPtr<Event> createEvent);
    virtual ~Object(void);
    const char *getType(void) const
    {
        return m_type;
    }

    // Used by class Operation only
    void read(de::SharedPtr<Event> event, std::vector<de::SharedPtr<Event>> &deps);
    void modify(de::SharedPtr<Event> event, std::vector<de::SharedPtr<Event>> &deps);

private:
    const char *m_type;
    de::SharedPtr<Event> m_modify;
    std::vector<de::SharedPtr<Event>> m_reads;

    // Disabled
    Object(const Object &);
    Object &operator=(const Object &);
};

class Thread;

class MessageBuilder
{
public:
    MessageBuilder(Thread &thread) : m_thread(thread)
    {
    }
    MessageBuilder(const MessageBuilder &other) : m_thread(other.m_thread), m_stream(other.m_stream.str())
    {
    }
    template <class T>
    MessageBuilder &operator<<(const T &t)
    {
        m_stream << t;
        return *this;
    }

    class EndToken
    {
    public:
        EndToken(void)
        {
        }
    };

    void operator<<(const EndToken &);

private:
    Thread &m_thread;
    std::stringstream m_stream;
};

class Message
{
public:
    Message(uint64_t time, const char *message) : m_time(time), m_message(message)
    {
    }

    uint64_t getTime(void) const
    {
        return m_time;
    }
    const std::string &getMessage(void) const
    {
        return m_message;
    }

    static const MessageBuilder::EndToken End;

private:
    uint64_t m_time;
    std::string m_message;
};

// Base class for operations executed by threads
class Operation
{
public:
    Operation(const char *name);
    virtual ~Operation(void);

    const char *getName(void) const
    {
        return m_name;
    }
    de::SharedPtr<Event> getEvent(void)
    {
        return m_event;
    }

    void readObject(de::SharedPtr<Object> object)
    {
        object->read(m_event, m_deps);
    }
    void modifyObject(de::SharedPtr<Object> object)
    {
        object->modify(m_event, m_deps);
    }

    virtual void exec(Thread &thread) = 0; //!< Overwritten by inherited class to perform actual operation
    virtual void execute(
        Thread &thread); //!< May Be overwritten by inherited class to change how syncronization is done

protected:
    const char *m_name;
    std::vector<de::SharedPtr<Event>> m_deps;
    de::SharedPtr<Event> m_event;

    Operation(const Operation &);
    Operation &operator=(const Operation &);
};

class Thread : public de::Thread
{
public:
    enum ThreadStatus
    {
        THREADSTATUS_NOT_STARTED = 0,
        THREADSTATUS_INIT_FAILED,
        THREADSTATUS_RUNNING,
        THREADSTATUS_READY,
        THREADSTATUS_FAILED,
        THREADSTATUS_NOT_SUPPORTED
    };
    Thread(uint32_t seed);
    ~Thread(void);

    virtual void init(void)
    {
    } //!< Called first before any Operation

    // \todo [mika] Should the result of execution be passed to deinit?
    virtual void deinit(void)
    {
    } //!< Called after after operation

    void addOperation(Operation *operation);

    void exec(void);

    uint8_t *getUnusedData(
        size_t size); //!< Return data pointer that contains at least size bytes. Valid until next call

    ThreadStatus getStatus(void) const
    {
        de::ScopedLock lock(m_statusLock);
        return m_status;
    }
    void setStatus(ThreadStatus status)
    {
        de::ScopedLock lock(m_statusLock);
        m_status = status;
    }

    MessageBuilder newMessage(void)
    {
        return MessageBuilder(*this);
    }
    de::Random &getRandom(void)
    {
        return m_random;
    }

    // Used to by test case to read log messages
    int getMessageCount(void) const;
    Message getMessage(int index) const;

    // Used by message builder
    void pushMessage(const std::string &str);

private:
    virtual void run(void);

    std::vector<Operation *> m_operations;
    de::Random m_random;

    mutable de::Mutex m_messageLock;
    std::vector<Message> m_messages;
    mutable de::Mutex m_statusLock;
    ThreadStatus m_status;
    std::vector<uint8_t> m_unusedData;

    // Disabled
    Thread(const Thread &);
    Thread operator=(const Thread &);
};

class DataBlock : public Object
{
public:
    DataBlock(de::SharedPtr<Event> event);

    void setData(size_t size, const void *data);
    const uint8_t *getData(void) const
    {
        return &(m_data[0]);
    }
    size_t getSize(void) const
    {
        return m_data.size();
    }

private:
    std::vector<uint8_t> m_data;
};

class CompareData : public Operation
{
public:
    CompareData(de::SharedPtr<DataBlock> a, de::SharedPtr<DataBlock> b);
    void exec(Thread &thread);

private:
    de::SharedPtr<DataBlock> m_a;
    de::SharedPtr<DataBlock> m_b;
};

} // namespace ThreadUtil
} // namespace tcu

#endif // _TCUTHREADUTIL_HPP
