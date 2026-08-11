#ifndef _DEPROCESS_HPP
#define _DEPROCESS_HPP
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
 * \brief deProcess C++ wrapper.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deProcess.h"

#include <string>
#include <stdexcept>

namespace de
{

class ProcessError : public std::runtime_error
{
public:
    ProcessError(const std::string &message) : std::runtime_error(message)
    {
    }
};

class Process
{
public:
    Process(void);
    ~Process(void);

    void start(const char *commandLine, const char *workingDirectory);

    void waitForFinish(void);
    void terminate(void);
    void kill(void);

    bool isRunning(void)
    {
        return deProcess_isRunning(m_process) == true;
    }
    int getExitCode(void) const
    {
        return deProcess_getExitCode(m_process);
    }

    deFile *getStdIn(void)
    {
        return deProcess_getStdIn(m_process);
    }
    deFile *getStdOut(void)
    {
        return deProcess_getStdOut(m_process);
    }
    deFile *getStdErr(void)
    {
        return deProcess_getStdErr(m_process);
    }

    void closeStdIn(void);
    void closeStdOut(void);
    void closeStdErr(void);

private:
    Process(const Process &other);
    Process &operator=(const Process &other);

    deProcess *m_process;
};

} // namespace de

#endif // _DEPROCESS_HPP
