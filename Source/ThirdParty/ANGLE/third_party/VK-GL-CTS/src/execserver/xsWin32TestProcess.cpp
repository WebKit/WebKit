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

#include "xsWin32TestProcess.hpp"
#include "deFilePath.hpp"
#include "deString.h"
#include "deMemory.h"
#include "deClock.h"
#include "deFile.h"

#include <sstream>
#include <string.h>

using std::string;
using std::vector;

namespace xs
{

enum
{
    MAX_OLD_LOGFILE_DELETE_ATTEMPTS = 20, //!< How many times execserver tries to delete old log file
    LOGFILE_DELETE_SLEEP_MS         = 50  //!< Sleep time (in ms) between log file delete attempts
};

namespace win32
{

// Error

static std::string formatErrMsg(DWORD error, const char *msg)
{
    std::ostringstream str;
    LPSTR msgBuf;

#if defined(UNICODE)
#error Unicode not supported.
#endif

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msgBuf, 0, nullptr) > 0)
        str << msg << ", error " << error << ": " << msgBuf;
    else
        str << msg << ", error " << error;

    return str.str();
}

Error::Error(DWORD error, const char *msg) : std::runtime_error(formatErrMsg(error, msg)), m_error(error)
{
}

// Event

Event::Event(bool manualReset, bool initialState) : m_handle(0)
{
    m_handle = CreateEvent(NULL, manualReset ? TRUE : FALSE, initialState ? TRUE : FALSE, NULL);
    if (!m_handle)
        throw Error(GetLastError(), "CreateEvent() failed");
}

Event::~Event(void)
{
    CloseHandle(m_handle);
}

void Event::setSignaled(void)
{
    if (!SetEvent(m_handle))
        throw Error(GetLastError(), "SetEvent() failed");
}

void Event::reset(void)
{
    if (!ResetEvent(m_handle))
        throw Error(GetLastError(), "ResetEvent() failed");
}

// CaseListWriter

CaseListWriter::CaseListWriter(void) : m_dst(INVALID_HANDLE_VALUE), m_cancelEvent(true, false)
{
}

CaseListWriter::~CaseListWriter(void)
{
}

void CaseListWriter::start(const char *caseList, HANDLE dst)
{
    DE_ASSERT(!isStarted());

    m_dst = dst;

    int caseListSize = (int)strlen(caseList) + 1;
    m_caseList.resize(caseListSize);
    std::copy(caseList, caseList + caseListSize, m_caseList.begin());

    de::Thread::start();
}

void CaseListWriter::run(void)
{
    try
    {
        Event ioEvent(true, false); // Manual reset, non-signaled state.
        HANDLE waitHandles[] = {ioEvent.getHandle(), m_cancelEvent.getHandle()};
        OVERLAPPED overlapped;
        int curPos = 0;

        deMemset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = ioEvent.getHandle();

        while (curPos < (int)m_caseList.size())
        {
            const int maxWriteSize = 4096;
            const int numToWrite   = de::min(maxWriteSize, (int)m_caseList.size() - curPos);
            DWORD waitRes          = 0;

            if (!WriteFile(m_dst, &m_caseList[curPos], (DWORD)numToWrite, NULL, &overlapped))
            {
                DWORD err = GetLastError();
                if (err != ERROR_IO_PENDING)
                    throw Error(err, "WriteFile() failed");
            }

            waitRes = WaitForMultipleObjects(DE_LENGTH_OF_ARRAY(waitHandles), &waitHandles[0], FALSE, INFINITE);

            if (waitRes == WAIT_OBJECT_0)
            {
                DWORD numBytesWritten = 0;

                // \note GetOverlappedResult() will fail with ERROR_IO_INCOMPLETE if IO event is not complete (should be).
                if (!GetOverlappedResult(m_dst, &overlapped, &numBytesWritten, FALSE))
                    throw Error(GetLastError(), "GetOverlappedResult() failed");

                if (numBytesWritten == 0)
                    throw Error(GetLastError(), "Writing to pipe failed (pipe closed?)");

                curPos += (int)numBytesWritten;
            }
            else if (waitRes == WAIT_OBJECT_0 + 1)
            {
                // Cancel.
                if (!CancelIo(m_dst))
                    throw Error(GetLastError(), "CancelIo() failed");
                break;
            }
            else
                throw Error(GetLastError(), "WaitForMultipleObjects() failed");
        }
    }
    catch (const std::exception &e)
    {
        // \todo [2013-08-13 pyry] What to do about this?
        printf("win32::CaseListWriter::run(): %s\n", e.what());
    }
}

void CaseListWriter::stop(void)
{
    if (!isStarted())
        return; // Nothing to do.

    m_cancelEvent.setSignaled();

    // Join thread.
    join();

    m_cancelEvent.reset();

    m_dst = INVALID_HANDLE_VALUE;
}

// FileReader

FileReader::FileReader(ThreadedByteBuffer *dst)
    : m_dstBuf(dst)
    , m_handle(INVALID_HANDLE_VALUE)
    , m_cancelEvent(false, false)
{
}

FileReader::~FileReader(void)
{
}

void FileReader::start(HANDLE file)
{
    DE_ASSERT(!isStarted());

    m_handle = file;

    de::Thread::start();
}

void FileReader::run(void)
{
    try
    {
        Event ioEvent(true, false); // Manual reset, not signaled state.
        HANDLE waitHandles[] = {ioEvent.getHandle(), m_cancelEvent.getHandle()};
        OVERLAPPED overlapped;
        std::vector<uint8_t> tmpBuf(FILEREADER_TMP_BUFFER_SIZE);
        uint64_t offset = 0; // Overlapped IO requires manual offset keeping.

        deMemset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = ioEvent.getHandle();

        for (;;)
        {
            DWORD numBytesRead = 0;
            DWORD waitRes;

            overlapped.Offset     = (DWORD)(offset & 0xffffffffu);
            overlapped.OffsetHigh = (DWORD)(offset >> 32);

            if (!ReadFile(m_handle, &tmpBuf[0], (DWORD)tmpBuf.size(), NULL, &overlapped))
            {
                DWORD err = GetLastError();

                if (err == ERROR_BROKEN_PIPE)
                    break;
                else if (err == ERROR_HANDLE_EOF)
                {
                    if (m_dstBuf->isCanceled())
                        break;

                    deSleep(FILEREADER_IDLE_SLEEP);

                    if (m_dstBuf->isCanceled())
                        break;
                    else
                        continue;
                }
                else if (err != ERROR_IO_PENDING)
                    throw Error(err, "ReadFile() failed");
            }

            waitRes = WaitForMultipleObjects(DE_LENGTH_OF_ARRAY(waitHandles), &waitHandles[0], FALSE, INFINITE);

            if (waitRes == WAIT_OBJECT_0)
            {
                // \note GetOverlappedResult() will fail with ERROR_IO_INCOMPLETE if IO event is not complete (should be).
                if (!GetOverlappedResult(m_handle, &overlapped, &numBytesRead, FALSE))
                {
                    DWORD err = GetLastError();

                    if (err == ERROR_HANDLE_EOF)
                    {
                        // End of file - for now.
                        // \note Should check for end of buffer here, or otherwise may end up in infinite loop.
                        if (m_dstBuf->isCanceled())
                            break;

                        deSleep(FILEREADER_IDLE_SLEEP);

                        if (m_dstBuf->isCanceled())
                            break;
                        else
                            continue;
                    }
                    else if (err == ERROR_BROKEN_PIPE)
                        break;
                    else
                        throw Error(err, "GetOverlappedResult() failed");
                }

                if (numBytesRead == 0)
                    throw Error(GetLastError(), "Reading from file failed");
                else
                    offset += (uint64_t)numBytesRead;
            }
            else if (waitRes == WAIT_OBJECT_0 + 1)
            {
                // Cancel.
                if (!CancelIo(m_handle))
                    throw Error(GetLastError(), "CancelIo() failed");
                break;
            }
            else
                throw Error(GetLastError(), "WaitForMultipleObjects() failed");

            try
            {
                m_dstBuf->write((int)numBytesRead, &tmpBuf[0]);
                m_dstBuf->flush();
            }
            catch (const ThreadedByteBuffer::CanceledException &)
            {
                // Canceled.
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        // \todo [2013-08-13 pyry] What to do?
        printf("win32::FileReader::run(): %s\n", e.what());
    }
}

void FileReader::stop(void)
{
    if (!isStarted())
        return; // Nothing to do.

    m_cancelEvent.setSignaled();

    // Join thread.
    join();

    m_cancelEvent.reset();

    m_handle = INVALID_HANDLE_VALUE;
}

// TestLogReader

TestLogReader::TestLogReader(void)
    : m_logBuffer(LOG_BUFFER_BLOCK_SIZE, LOG_BUFFER_NUM_BLOCKS)
    , m_logFile(INVALID_HANDLE_VALUE)
    , m_reader(&m_logBuffer)
{
}

TestLogReader::~TestLogReader(void)
{
    if (m_logFile != INVALID_HANDLE_VALUE)
        CloseHandle(m_logFile);
}

void TestLogReader::start(const char *filename)
{
    DE_ASSERT(m_logFile == INVALID_HANDLE_VALUE && !m_reader.isStarted());

    m_logFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

    if (m_logFile == INVALID_HANDLE_VALUE)
        throw Error(GetLastError(), "Failed to open log file");

    m_reader.start(m_logFile);
}

void TestLogReader::stop(void)
{
    if (!m_reader.isStarted())
        return; // Nothing to do.

    m_logBuffer.cancel();
    m_reader.stop();

    CloseHandle(m_logFile);
    m_logFile = INVALID_HANDLE_VALUE;

    m_logBuffer.clear();
}

// Process

Process::Process(void)
    : m_state(STATE_NOT_STARTED)
    , m_exitCode(0)
    , m_standardIn(INVALID_HANDLE_VALUE)
    , m_standardOut(INVALID_HANDLE_VALUE)
    , m_standardErr(INVALID_HANDLE_VALUE)
{
    deMemset(&m_procInfo, 0, sizeof(m_procInfo));
}

Process::~Process(void)
{
    try
    {
        if (isRunning())
        {
            kill();
            waitForFinish();
        }
    }
    catch (...)
    {
    }

    cleanupHandles();
}

void Process::cleanupHandles(void)
{
    DE_ASSERT(!isRunning());

    if (m_standardErr != INVALID_HANDLE_VALUE)
        CloseHandle(m_standardErr);

    if (m_standardOut != INVALID_HANDLE_VALUE)
        CloseHandle(m_standardOut);

    if (m_standardIn != INVALID_HANDLE_VALUE)
        CloseHandle(m_standardIn);

    if (m_procInfo.hProcess)
        CloseHandle(m_procInfo.hProcess);

    if (m_procInfo.hThread)
        CloseHandle(m_procInfo.hThread);

    m_standardErr = INVALID_HANDLE_VALUE;
    m_standardOut = INVALID_HANDLE_VALUE;
    m_standardIn  = INVALID_HANDLE_VALUE;

    deMemset(&m_procInfo, 0, sizeof(m_procInfo));
}

__declspec(thread) static int t_pipeNdx = 0;

static void createPipeWithOverlappedIO(HANDLE *readHandleOut, HANDLE *writeHandleOut, uint32_t readMode,
                                       uint32_t writeMode, SECURITY_ATTRIBUTES *securityAttr)
{
    const int defaultBufSize = 4096;
    char pipeName[128];
    HANDLE readHandle;
    HANDLE writeHandle;

    DE_ASSERT(((readMode | writeMode) & ~FILE_FLAG_OVERLAPPED) == 0);

    snprintf(pipeName, sizeof(pipeName), "\\\\.\\Pipe\\dEQP-ExecServer-%08x-%08x-%08x", GetCurrentProcessId(),
             GetCurrentThreadId(), t_pipeNdx++);

    readHandle = CreateNamedPipe(pipeName,                       /* Pipe name.                */
                                 PIPE_ACCESS_INBOUND | readMode, /* Open mode.                */
                                 PIPE_TYPE_BYTE | PIPE_WAIT,     /* Pipe flags.                */
                                 1,                              /* Max number of instances.    */
                                 defaultBufSize,                 /* Output buffer size.        */
                                 defaultBufSize,                 /* Input buffer size.        */
                                 0,                              /* Use default timeout.        */
                                 securityAttr);

    if (readHandle == INVALID_HANDLE_VALUE)
        throw Error(GetLastError(), "CreateNamedPipe() failed");

    writeHandle = CreateFile(pipeName, GENERIC_WRITE,           /* Access mode.                */
                             0,                                 /* No sharing.                */
                             securityAttr, OPEN_EXISTING,       /* Assume existing object.    */
                             FILE_ATTRIBUTE_NORMAL | writeMode, /* Open mode / flags.        */
                             nullptr /* Template file.            */);

    if (writeHandle == INVALID_HANDLE_VALUE)
    {
        DWORD openErr = GetLastError();
        CloseHandle(readHandle);
        throw Error(openErr, "Failed to open created pipe, CreateFile() failed");
    }

    *readHandleOut  = readHandle;
    *writeHandleOut = writeHandle;
}

void Process::start(const char *commandLine, const char *workingDirectory)
{
    // Pipes.
    HANDLE stdInRead   = INVALID_HANDLE_VALUE;
    HANDLE stdInWrite  = INVALID_HANDLE_VALUE;
    HANDLE stdOutRead  = INVALID_HANDLE_VALUE;
    HANDLE stdOutWrite = INVALID_HANDLE_VALUE;
    HANDLE stdErrRead  = INVALID_HANDLE_VALUE;
    HANDLE stdErrWrite = INVALID_HANDLE_VALUE;

    if (m_state == STATE_RUNNING)
        throw std::runtime_error("Process already running");
    else if (m_state == STATE_FINISHED)
    {
        // Process finished, clean up old cruft.
        cleanupHandles();
        m_state = STATE_NOT_STARTED;
    }

    // Create pipes
    try
    {
        SECURITY_ATTRIBUTES securityAttr;
        STARTUPINFO startInfo;

        deMemset(&startInfo, 0, sizeof(startInfo));
        deMemset(&securityAttr, 0, sizeof(securityAttr));

        // Security attributes for inheriting handle.
        securityAttr.nLength              = sizeof(SECURITY_ATTRIBUTES);
        securityAttr.bInheritHandle       = TRUE;
        securityAttr.lpSecurityDescriptor = nullptr;

        createPipeWithOverlappedIO(&stdInRead, &stdInWrite, 0, FILE_FLAG_OVERLAPPED, &securityAttr);
        createPipeWithOverlappedIO(&stdOutRead, &stdOutWrite, FILE_FLAG_OVERLAPPED, 0, &securityAttr);
        createPipeWithOverlappedIO(&stdErrRead, &stdErrWrite, FILE_FLAG_OVERLAPPED, 0, &securityAttr);

        if (!SetHandleInformation(stdInWrite, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(stdOutRead, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(stdErrRead, HANDLE_FLAG_INHERIT, 0))
            throw Error(GetLastError(), "SetHandleInformation() failed");

        // Startup info for process.
        startInfo.cb         = sizeof(startInfo);
        startInfo.hStdError  = stdErrWrite;
        startInfo.hStdOutput = stdOutWrite;
        startInfo.hStdInput  = stdInRead;
        startInfo.dwFlags |= STARTF_USESTDHANDLES;

        if (!CreateProcess(nullptr, (LPTSTR)commandLine, nullptr, nullptr, TRUE /* inherit handles */, 0, nullptr,
                           workingDirectory, &startInfo, &m_procInfo))
            throw Error(GetLastError(), "CreateProcess() failed");
    }
    catch (...)
    {
        if (stdInRead != INVALID_HANDLE_VALUE)
            CloseHandle(stdInRead);
        if (stdInWrite != INVALID_HANDLE_VALUE)
            CloseHandle(stdInWrite);
        if (stdOutRead != INVALID_HANDLE_VALUE)
            CloseHandle(stdOutRead);
        if (stdOutWrite != INVALID_HANDLE_VALUE)
            CloseHandle(stdOutWrite);
        if (stdErrRead != INVALID_HANDLE_VALUE)
            CloseHandle(stdErrRead);
        if (stdErrWrite != INVALID_HANDLE_VALUE)
            CloseHandle(stdErrWrite);
        throw;
    }

    // Store handles to be kept.
    m_standardIn  = stdInWrite;
    m_standardOut = stdOutRead;
    m_standardErr = stdErrRead;

    // Close other ends of handles.
    CloseHandle(stdErrWrite);
    CloseHandle(stdOutWrite);
    CloseHandle(stdInRead);

    m_state = STATE_RUNNING;
}

bool Process::isRunning(void)
{
    if (m_state == STATE_RUNNING)
    {
        int exitCode;
        BOOL result = GetExitCodeProcess(m_procInfo.hProcess, (LPDWORD)&exitCode);

        if (result != TRUE)
            throw Error(GetLastError(), "GetExitCodeProcess() failed");

        if (exitCode == STILL_ACTIVE)
            return true;
        else
        {
            // Done.
            m_exitCode = exitCode;
            m_state    = STATE_FINISHED;
            return false;
        }
    }
    else
        return false;
}

void Process::waitForFinish(void)
{
    if (m_state == STATE_RUNNING)
    {
        if (WaitForSingleObject(m_procInfo.hProcess, INFINITE) != WAIT_OBJECT_0)
            throw Error(GetLastError(), "Waiting for process failed, WaitForSingleObject() failed");

        if (isRunning())
            throw std::runtime_error("Process is still alive");
    }
    else
        throw std::runtime_error("Process is not running");
}

void Process::stopProcess(bool kill)
{
    if (m_state == STATE_RUNNING)
    {
        if (!TerminateProcess(m_procInfo.hProcess, kill ? -1 : 0))
            throw Error(GetLastError(), "TerminateProcess() failed");
    }
    else
        throw std::runtime_error("Process is not running");
}

void Process::terminate(void)
{
    stopProcess(false);
}

void Process::kill(void)
{
    stopProcess(true);
}

} // namespace win32

Win32TestProcess::Win32TestProcess(void)
    : m_process(nullptr)
    , m_processStartTime(0)
    , m_infoBuffer(INFO_BUFFER_BLOCK_SIZE, INFO_BUFFER_NUM_BLOCKS)
    , m_stdOutReader(&m_infoBuffer)
    , m_stdErrReader(&m_infoBuffer)
{
}

Win32TestProcess::~Win32TestProcess(void)
{
    delete m_process;
}

void Win32TestProcess::start(const char *name, const char *params, const char *workingDir, const char *caseList)
{
    bool hasCaseList = strlen(caseList) > 0;

    XS_CHECK(!m_process);

    de::FilePath logFilePath = de::FilePath::join(workingDir, "TestResults.qpa");
    m_logFileName            = logFilePath.getPath();

    // Remove old file if such exists.
    // \note Sometimes on Windows the test process dies slowly and may not release handle to log file
    //         until a bit later.
    // \todo [2013-07-15 pyry] This should be solved by improving deProcess and killing all child processes as well.
    {
        int tryNdx = 0;
        while (tryNdx < MAX_OLD_LOGFILE_DELETE_ATTEMPTS && deFileExists(m_logFileName.c_str()))
        {
            if (deDeleteFile(m_logFileName.c_str()))
                break;
            deSleep(LOGFILE_DELETE_SLEEP_MS);
            tryNdx += 1;
        }

        if (deFileExists(m_logFileName.c_str()))
            throw TestProcessException(string("Failed to remove '") + m_logFileName + "'");
    }

    // Construct command line.
    string cmdLine =
        de::FilePath(name).isAbsolutePath() ? name : de::FilePath::join(workingDir, name).normalize().getPath();
    cmdLine += string(" --deqp-log-filename=") + logFilePath.getBaseName();

    if (hasCaseList)
        cmdLine += " --deqp-stdin-caselist";

    if (strlen(params) > 0)
        cmdLine += string(" ") + params;

    DE_ASSERT(!m_process);
    m_process = new win32::Process();

    try
    {
        m_process->start(cmdLine.c_str(), strlen(workingDir) > 0 ? workingDir : nullptr);
    }
    catch (const std::exception &e)
    {
        delete m_process;
        m_process = nullptr;
        throw TestProcessException(e.what());
    }

    m_processStartTime = deGetMicroseconds();

    // Create stdout & stderr readers.
    m_stdOutReader.start(m_process->getStdOut());
    m_stdErrReader.start(m_process->getStdErr());

    // Start case list writer.
    if (hasCaseList)
        m_caseListWriter.start(caseList, m_process->getStdIn());
}

void Win32TestProcess::terminate(void)
{
    if (m_process)
    {
        try
        {
            m_process->kill();
        }
        catch (const std::exception &e)
        {
            printf("Win32TestProcess::terminate(): Failed to kill process: %s\n", e.what());
        }
    }
}

void Win32TestProcess::cleanup(void)
{
    m_caseListWriter.stop();

    // \note Buffers must be canceled before stopping readers.
    m_infoBuffer.cancel();

    m_stdErrReader.stop();
    m_stdOutReader.stop();
    m_testLogReader.stop();

    // Reset buffers.
    m_infoBuffer.clear();

    if (m_process)
    {
        try
        {
            if (m_process->isRunning())
            {
                m_process->kill();
                m_process->waitForFinish();
            }
        }
        catch (const std::exception &e)
        {
            printf("Win32TestProcess::cleanup(): Failed to kill process: %s\n", e.what());
        }

        delete m_process;
        m_process = nullptr;
    }
}

int Win32TestProcess::readTestLog(uint8_t *dst, int numBytes)
{
    if (!m_testLogReader.isRunning())
    {
        if (deGetMicroseconds() - m_processStartTime > LOG_FILE_TIMEOUT * 1000)
        {
            // Timeout, kill process.
            terminate();
            return 0; // \todo [2013-08-13 pyry] Throw exception?
        }

        if (!deFileExists(m_logFileName.c_str()))
            return 0;

        // Start reader.
        m_testLogReader.start(m_logFileName.c_str());
    }

    DE_ASSERT(m_testLogReader.isRunning());
    return m_testLogReader.read(dst, numBytes);
}

bool Win32TestProcess::isRunning(void)
{
    if (m_process)
        return m_process->isRunning();
    else
        return false;
}

int Win32TestProcess::getExitCode(void) const
{
    if (m_process)
        return m_process->getExitCode();
    else
        return -1;
}

} // namespace xs
