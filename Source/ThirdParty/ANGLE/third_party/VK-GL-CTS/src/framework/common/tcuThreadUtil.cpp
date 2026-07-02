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

#include "tcuThreadUtil.hpp"

#include "deClock.h"
#include "deMemory.h"

using de::SharedPtr;
using std::vector;

namespace tcu
{
namespace ThreadUtil
{

Event::Event(void) : m_result(RESULT_NOT_READY), m_waiterCount(0), m_waiters(0, 0)
{
}

Event::~Event(void)
{
}

void Event::setResult(Result result)
{
    m_lock.lock();
    DE_ASSERT(m_result == RESULT_NOT_READY);
    m_result = result;
    m_lock.unlock();

    for (int i = 0; i < m_waiterCount; i++)
        m_waiters.increment();
}

Event::Result Event::waitReady(void)
{
    m_lock.lock();

    if (m_result == RESULT_NOT_READY)
        m_waiterCount = m_waiterCount + 1;
    else
    {
        m_lock.unlock();
        return m_result;
    }

    m_lock.unlock();

    m_waiters.decrement();

    return m_result;
}

Object::Object(const char *type, SharedPtr<Event> e) : m_type(type), m_modify(e)
{
}

Object::~Object(void)
{
}

void Object::read(SharedPtr<Event> event, std::vector<SharedPtr<Event>> &deps)
{
    // Make call depend on last modifying call
    deps.push_back(m_modify);

    // Add read dependency
    m_reads.push_back(event);
}

void Object::modify(SharedPtr<Event> event, std::vector<SharedPtr<Event>> &deps)
{
    // Make call depend on all reads
    for (int readNdx = 0; readNdx < (int)m_reads.size(); readNdx++)
    {
        deps.push_back(m_reads[readNdx]);
    }
    deps.push_back(m_modify);

    // Update last modifying call
    m_modify = event;

    // Clear read dependencies of last "version" of this object
    m_reads.clear();
}

Operation::Operation(const char *name) : m_name(name), m_event(new Event)
{
}

Operation::~Operation(void)
{
}

void Operation::execute(Thread &thread)
{
    bool success = true;

    // Wait for dependencies and check that they succeeded
    for (int depNdx = 0; depNdx < (int)m_deps.size(); depNdx++)
    {
        if (m_deps[depNdx]->waitReady() != Event::RESULT_OK)
            success = false;
    }

    // Try execute operation
    if (success)
    {
        try
        {
            exec(thread);
        }
        catch (...)
        {
            // Got exception event failed
            m_event->setResult(Event::RESULT_FAILED);
            throw;
        }

        m_event->setResult(Event::RESULT_OK);
    }
    else
        // Some dependencies failed
        m_event->setResult(Event::RESULT_FAILED);

    // Release resources
    m_deps.clear();
    m_event = SharedPtr<Event>();
}

const MessageBuilder::EndToken Message::End = MessageBuilder::EndToken();

void MessageBuilder::operator<<(const EndToken &)
{
    m_thread.pushMessage(m_stream.str());
}

Thread::Thread(uint32_t seed) : m_random(seed), m_status(THREADSTATUS_NOT_STARTED)
{
}

Thread::~Thread(void)
{
    for (int operationNdx = 0; operationNdx < (int)m_operations.size(); operationNdx++)
        delete m_operations[operationNdx];

    m_operations.clear();
}

uint8_t *Thread::getUnusedData(size_t size)
{
    if (m_unusedData.size() < size)
    {
        m_unusedData.resize(size);
    }

    return &(m_unusedData[0]);
}

void Thread::addOperation(Operation *operation)
{
    m_operations.push_back(operation);
}

void Thread::run(void)
{
    setStatus(THREADSTATUS_RUNNING);
    bool initOk = false;

    // Reserve at least two messages for each operation
    m_messages.reserve(m_operations.size() * 2);
    try
    {
        init();
        initOk = true;
        for (int operationNdx = 0; operationNdx < (int)m_operations.size(); operationNdx++)
            m_operations[operationNdx]->execute(*this);

        deinit();
        setStatus(THREADSTATUS_READY);
    }
    catch (const tcu::NotSupportedError &e)
    {
        newMessage() << "tcu::NotSupportedError '" << e.what() << "'" << Message::End;
        deinit();
        setStatus(initOk ? THREADSTATUS_NOT_SUPPORTED : THREADSTATUS_INIT_FAILED);
    }
    catch (const tcu::Exception &e)
    {
        newMessage() << "tcu::Exception '" << e.what() << "'" << Message::End;
        deinit();
        setStatus(initOk ? THREADSTATUS_FAILED : THREADSTATUS_INIT_FAILED);
    }
    catch (const std::exception &error)
    {
        newMessage() << "std::exception '" << error.what() << "'" << Message::End;
        deinit();
        setStatus(initOk ? THREADSTATUS_FAILED : THREADSTATUS_INIT_FAILED);
    }
    catch (...)
    {
        newMessage() << "Unkown exception" << Message::End;
        deinit();
        setStatus(initOk ? THREADSTATUS_FAILED : THREADSTATUS_INIT_FAILED);
    }
}

void Thread::exec(void)
{
    start();
}

void Thread::pushMessage(const std::string &str)
{
    de::ScopedLock lock(m_messageLock);
    m_messages.push_back(Message(deGetMicroseconds(), str.c_str()));
}

int Thread::getMessageCount(void) const
{
    de::ScopedLock lock(m_messageLock);
    return (int)(m_messages.size());
}

Message Thread::getMessage(int index) const
{
    de::ScopedLock lock(m_messageLock);
    return m_messages[index];
}

DataBlock::DataBlock(SharedPtr<Event> event) : Object("DataBlock", event)
{
}

void DataBlock::setData(size_t size, const void *data)
{
    m_data = std::vector<uint8_t>(size);
    deMemcpy(&(m_data[0]), data, size);
}

CompareData::CompareData(SharedPtr<DataBlock> a, SharedPtr<DataBlock> b) : Operation("CompareData"), m_a(a), m_b(b)
{
    readObject(SharedPtr<Object>(a));
    readObject(SharedPtr<Object>(b));
}

void CompareData::exec(Thread &thread)
{
    bool result = true;
    DE_ASSERT(m_a->getSize() == m_b->getSize());

    thread.newMessage() << "Begin -- CompareData" << Message::End;

    for (int byteNdx = 0; byteNdx < (int)m_a->getSize(); byteNdx++)
    {
        if (m_a->getData()[byteNdx] != m_b->getData()[byteNdx])
        {
            result = false;
            thread.newMessage() << "CompareData failed at offset :" << byteNdx << Message::End;
            break;
        }
    }

    if (result)
        thread.newMessage() << "CompareData passed" << Message::End;
    else
        TCU_FAIL("Data comparision failed");

    thread.newMessage() << "End -- CompareData" << Message::End;
}

} // namespace ThreadUtil
} // namespace tcu
