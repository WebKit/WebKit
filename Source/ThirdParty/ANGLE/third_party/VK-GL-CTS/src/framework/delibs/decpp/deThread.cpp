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
 * \brief Thread base class.
 *//*--------------------------------------------------------------------*/

#include "deThread.hpp"
#include "deMemory.h"

#include <exception>
#include <stdexcept>
#include <new>

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Thread constructor.
 *//*--------------------------------------------------------------------*/
Thread::Thread(void) : m_thread(0)
{
    deMemset(&m_attribs, 0, sizeof(m_attribs));
}

/*--------------------------------------------------------------------*//*!
 * \brief Destroy thread.
 *
 * If the thread is currently running, OS is instructed to destroy it
 * but the actual behavior is unspecified.
 *//*--------------------------------------------------------------------*/
Thread::~Thread(void)
{
    if (m_thread)
        deThread_destroy(m_thread);
}

/*--------------------------------------------------------------------*//*!
 * \brief Set thread priority.
 * \param priority deThreadPriority as described in deThread.h. Currently
 *                   supported values are: DE_THREADPRIORITY_LOWEST,
 *                   DE_THREADPRIORITY_LOW, DE_THREADPRIORITY_NORMAL,
 *                   DE_THREADPRIORITY_HIGH, DE_THREADPRIORITY_HIGHEST.
 *
 * Sets priority for the thread start(). setPriority() has no effect
 * if the thread is already running.
 *//*--------------------------------------------------------------------*/
void Thread::setPriority(deThreadPriority priority)
{
    m_attribs.priority = priority;
}

static void threadFunc(void *arg)
{
    static_cast<Thread *>(arg)->run();
}

/*--------------------------------------------------------------------*//*!
 * \brief Start thread.
 *
 * Starts thread that will execute the virtual run() method.
 *
 * The function will fail if the thread is currently running or has finished
 * but no join() has been called.
 *//*--------------------------------------------------------------------*/
void Thread::start(void)
{
    DE_ASSERT(!m_thread);
    m_thread = deThread_create(threadFunc, this, &m_attribs);
    if (!m_thread)
        throw std::bad_alloc();
}

/*--------------------------------------------------------------------*//*!
 * \brief Wait for thread to finish and clean up current thread.
 *
 * This function will block until currently running thread has finished.
 * Once the thread has finished, current thread state will be cleaned
 * and thread can be re-launched using start().
 *
 * join() can only be called after a successful call to start().
 *//*--------------------------------------------------------------------*/
void Thread::join(void)
{
    DE_ASSERT(m_thread);
    if (!deThread_join(m_thread))
        throw std::runtime_error("Thread::join() failed");

    deThread_destroy(m_thread);
    m_thread = 0;
}

} // namespace de
