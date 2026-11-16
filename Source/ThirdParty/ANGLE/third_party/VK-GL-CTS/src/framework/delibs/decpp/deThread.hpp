#ifndef _DETHREAD_HPP
#define _DETHREAD_HPP
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

#include "deDefs.hpp"
#include "deThread.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Base class for threads
 *
 * Thread provides base class for implementing threads. To leverage that
 * functionality, inherit Thread in your class and implement virtual run()
 * method.
 *
 * \note Thread class is not thread-safe, i.e. thread control functions
 *         such as start() or join() can not be called from multiple threads
 *         concurrently.
 *//*--------------------------------------------------------------------*/
class Thread
{
public:
    Thread(void);
    virtual ~Thread(void);

    void setPriority(deThreadPriority priority);
    void start(void);
    void join(void);

    bool isStarted(void) const;

    /** Thread entry point. */
    virtual void run(void) = 0;

private:
    Thread(const Thread &other);            // Not allowed!
    Thread &operator=(const Thread &other); // Not allowed!

    deThreadAttributes m_attribs;
    deThread m_thread;
};

/*--------------------------------------------------------------------*//*!
 * \brief Get thread launch status.
 * \return true if thread has been started and no join() has been called,
 *           false otherwise.
 *//*--------------------------------------------------------------------*/
inline bool Thread::isStarted(void) const
{
    return m_thread != 0;
}

} // namespace de

#endif // _DETHREAD_HPP
