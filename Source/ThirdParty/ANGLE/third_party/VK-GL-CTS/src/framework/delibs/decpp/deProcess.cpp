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

#include "deProcess.hpp"

#include <new>

namespace de
{

Process::Process(void) : m_process(deProcess_create())
{
    if (!m_process)
        throw std::bad_alloc();
}

Process::~Process(void)
{
    deProcess_destroy(m_process);
}

void Process::start(const char *commandLine, const char *workingDirectory)
{
    if (!deProcess_start(m_process, commandLine, workingDirectory))
        throw ProcessError(deProcess_getLastError(m_process));
}

void Process::waitForFinish(void)
{
    if (!deProcess_waitForFinish(m_process))
        throw ProcessError(deProcess_getLastError(m_process));
}

void Process::terminate(void)
{
    if (!deProcess_terminate(m_process))
        throw ProcessError(deProcess_getLastError(m_process));
}

void Process::kill(void)
{
    if (!deProcess_kill(m_process))
        throw ProcessError(deProcess_getLastError(m_process));
}

void Process::closeStdIn(void)
{
    if (!deProcess_closeStdIn(m_process))
        throw ProcessError(deProcess_getLastError(m_process));
}

void Process::closeStdOut(void)
{
    if (!deProcess_closeStdOut(m_process))
        throw ProcessError(deProcess_getLastError(m_process));
}

void Process::closeStdErr(void)
{
    if (!deProcess_closeStdErr(m_process))
        throw ProcessError(deProcess_getLastError(m_process));
}

} // namespace de
