#ifndef _XSWIN32TESTPROCESS_HPP
#define _XSWIN32TESTPROCESS_HPP
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
 * \brief TestProcess implementation for Win32.
 *//*--------------------------------------------------------------------*/

#include "xsDefs.hpp"
#include "xsTestProcess.hpp"
#include "deThread.hpp"

#include <vector>
#include <string>

#if !defined(VC_EXTRALEAN)
#define VC_EXTRALEAN 1
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif
#include <windows.h>

namespace xs
{
namespace win32
{

class Error : public std::runtime_error
{
public:
    Error(DWORD error, const char *msg);

private:
    DWORD m_error;
};

class Event
{
public:
    Event(bool manualReset, bool initialState);
    ~Event(void);

    void setSignaled(void);
    void reset(void);

    HANDLE getHandle(void) const
    {
        return m_handle;
    }

private:
    Event(const Event &other);
    Event &operator=(const Event &other);

    HANDLE m_handle;
};

class CaseListWriter : public de::Thread
{
public:
    CaseListWriter(void);
    ~CaseListWriter(void);

    void start(const char *caseList, HANDLE dst);
    void stop(void);

    void run(void);

private:
    std::vector<char> m_caseList;
    HANDLE m_dst;
    Event m_cancelEvent;
};

class FileReader : public de::Thread
{
public:
    FileReader(ThreadedByteBuffer *dst);
    ~FileReader(void);

    void start(HANDLE file);
    void stop(void);

    void run(void);

private:
    ThreadedByteBuffer *m_dstBuf;
    HANDLE m_handle;
    Event m_cancelEvent;
};

class TestLogReader
{
public:
    TestLogReader(void);
    ~TestLogReader(void);

    void start(const char *filename);
    void stop(void);

    bool isRunning(void) const
    {
        return m_reader.isStarted();
    }

    int read(uint8_t *dst, int numBytes)
    {
        return m_logBuffer.tryRead(numBytes, dst);
    }

private:
    ThreadedByteBuffer m_logBuffer;
    HANDLE m_logFile;

    FileReader m_reader;
};

// \note deProcess uses anonymous pipes that don't have overlapped IO available.
//         For ExecServer purposes we need overlapped IO, and it makes the handles
//         incompatible with deFile. Thus separate Process implementation is used for now.
class Process
{
public:
    Process(void);
    ~Process(void);

    void start(const char *commandLine, const char *workingDirectory);

    void waitForFinish(void);
    void terminate(void);
    void kill(void);

    bool isRunning(void);
    int getExitCode(void) const
    {
        return m_exitCode;
    }

    HANDLE getStdIn(void) const
    {
        return m_standardIn;
    }
    HANDLE getStdOut(void) const
    {
        return m_standardOut;
    }
    HANDLE getStdErr(void) const
    {
        return m_standardErr;
    }

private:
    Process(const Process &other);
    Process &operator=(const Process &other);

    void stopProcess(bool kill);
    void cleanupHandles(void);

    enum State
    {
        STATE_NOT_STARTED = 0,
        STATE_RUNNING,
        STATE_FINISHED,

        STATE_LAST
    };

    State m_state;
    int m_exitCode;

    PROCESS_INFORMATION m_procInfo;
    HANDLE m_standardIn;
    HANDLE m_standardOut;
    HANDLE m_standardErr;
};

} // namespace win32

class Win32TestProcess : public TestProcess
{
public:
    Win32TestProcess(void);
    virtual ~Win32TestProcess(void);

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
    Win32TestProcess(const Win32TestProcess &other);
    Win32TestProcess &operator=(const Win32TestProcess &other);

    win32::Process *m_process;
    uint64_t m_processStartTime;
    std::string m_logFileName;

    ThreadedByteBuffer m_infoBuffer;

    // Threads.
    win32::CaseListWriter m_caseListWriter;
    win32::FileReader m_stdOutReader;
    win32::FileReader m_stdErrReader;
    win32::TestLogReader m_testLogReader;
};

} // namespace xs

#endif // _XSWIN32TESTPROCESS_HPP
