#ifndef _XSPOSIXTESTPROCESS_HPP
#define _XSPOSIXTESTPROCESS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Execution Server
 * ---------------------------------------------
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
 * \brief TestProcess implementation for Unix-like systems.
 *//*--------------------------------------------------------------------*/

#include "xsDefs.hpp"
#include "xsTestProcess.hpp"
#include "xsPosixFileReader.hpp"
#include "deProcess.hpp"
#include "deThread.hpp"

#include <vector>
#include <string>

namespace xs
{
namespace posix
{

class CaseListWriter : public de::Thread
{
public:
    CaseListWriter(void);
    ~CaseListWriter(void);

    void start(const char *caseList, deFile *dst);
    void stop(void);

    void run(void);

private:
    deFile *m_file;
    std::vector<char> m_caseList;
    bool m_run;
};

class PipeReader : public de::Thread
{
public:
    PipeReader(ThreadedByteBuffer *dst);
    ~PipeReader(void);

    void start(deFile *file);
    void stop(void);

    void run(void);

private:
    deFile *m_file;
    ThreadedByteBuffer *m_buf;
};

} // namespace posix

class PosixTestProcess : public TestProcess
{
public:
    PosixTestProcess(void);
    virtual ~PosixTestProcess(void);

    virtual void start(const char *name, const char *params, const char *workingDir, const char *caseList);
    virtual void terminate(void);
    virtual void cleanup(void);

    virtual bool isRunning(void);

    virtual int getExitCode(void) const;

    virtual int readTestLog(uint8_t *dst, int numBytes);
    virtual int readInfoLog(uint8_t *dst, int numBytes)
    {
        return m_infoBuffer.tryRead(numBytes, dst);
    }

private:
    PosixTestProcess(const PosixTestProcess &other);
    PosixTestProcess &operator=(const PosixTestProcess &other);

    de::Process *m_process;
    uint64_t m_processStartTime; //!< Used for determining log file timeout.
    std::string m_logFileName;
    ThreadedByteBuffer m_infoBuffer;

    // Threads.
    posix::CaseListWriter m_caseListWriter;
    posix::PipeReader m_stdOutReader;
    posix::PipeReader m_stdErrReader;
    posix::FileReader m_logReader;
};

} // namespace xs

#endif // _XSPOSIXTESTPROCESS_HPP
