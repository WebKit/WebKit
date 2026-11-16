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

#include "xsPosixTestProcess.hpp"
#include "deFilePath.hpp"
#include "deClock.h"

#include <string.h>
#include <stdio.h>

using std::string;
using std::vector;

namespace xs
{

namespace posix
{

CaseListWriter::CaseListWriter(void) : m_file(nullptr), m_run(false)
{
}

CaseListWriter::~CaseListWriter(void)
{
}

void CaseListWriter::start(const char *caseList, deFile *dst)
{
    DE_ASSERT(!isStarted());
    m_file = dst;
    m_run  = true;

    int caseListSize = (int)strlen(caseList) + 1;
    m_caseList.resize(caseListSize);
    std::copy(caseList, caseList + caseListSize, m_caseList.begin());

    // Set to non-blocking mode.
    if (!deFile_setFlags(m_file, DE_FILE_NONBLOCKING))
        XS_FAIL("Failed to set non-blocking mode");

    de::Thread::start();
}

void CaseListWriter::run(void)
{
    int64_t pos = 0;

    while (m_run && pos < (int64_t)m_caseList.size())
    {
        int64_t numWritten  = 0;
        deFileResult result = deFile_write(m_file, &m_caseList[0] + pos, m_caseList.size() - pos, &numWritten);

        if (result == DE_FILERESULT_SUCCESS)
            pos += numWritten;
        else if (result == DE_FILERESULT_WOULD_BLOCK)
            deSleep(1); // Yield.
        else
            break; // Error.
    }
}

void CaseListWriter::stop(void)
{
    if (!isStarted())
        return; // Nothing to do.

    m_run = false;

    // Join thread.
    join();

    m_file = nullptr;
}

PipeReader::PipeReader(ThreadedByteBuffer *dst) : m_file(nullptr), m_buf(dst)
{
}

PipeReader::~PipeReader(void)
{
}

void PipeReader::start(deFile *file)
{
    DE_ASSERT(!isStarted());

    // Set to non-blocking mode.
    if (!deFile_setFlags(file, DE_FILE_NONBLOCKING))
        XS_FAIL("Failed to set non-blocking mode");

    m_file = file;

    de::Thread::start();
}

void PipeReader::run(void)
{
    std::vector<uint8_t> tmpBuf(FILEREADER_TMP_BUFFER_SIZE);
    int64_t numRead = 0;

    while (!m_buf->isCanceled())
    {
        deFileResult result = deFile_read(m_file, &tmpBuf[0], (int64_t)tmpBuf.size(), &numRead);

        if (result == DE_FILERESULT_SUCCESS)
        {
            // Write to buffer.
            try
            {
                m_buf->write((int)numRead, &tmpBuf[0]);
                m_buf->flush();
            }
            catch (const ThreadedByteBuffer::CanceledException &)
            {
                // Canceled.
                break;
            }
        }
        else if (result == DE_FILERESULT_END_OF_FILE || result == DE_FILERESULT_WOULD_BLOCK)
        {
            // Wait for more data.
            deSleep(FILEREADER_IDLE_SLEEP);
        }
        else
            break; // Error.
    }
}

void PipeReader::stop(void)
{
    if (!isStarted())
        return; // Nothing to do.

    // Buffer must be in canceled state or otherwise stopping reader might block.
    DE_ASSERT(m_buf->isCanceled());

    // Join thread.
    join();

    m_file = nullptr;
}

} // namespace posix

PosixTestProcess::PosixTestProcess(void)
    : m_process(nullptr)
    , m_processStartTime(0)
    , m_infoBuffer(INFO_BUFFER_BLOCK_SIZE, INFO_BUFFER_NUM_BLOCKS)
    , m_stdOutReader(&m_infoBuffer)
    , m_stdErrReader(&m_infoBuffer)
    , m_logReader(LOG_BUFFER_BLOCK_SIZE, LOG_BUFFER_NUM_BLOCKS)
{
}

PosixTestProcess::~PosixTestProcess(void)
{
    delete m_process;
}

void PosixTestProcess::start(const char *name, const char *params, const char *workingDir, const char *caseList)
{
    bool hasCaseList = strlen(caseList) > 0;

    XS_CHECK(!m_process);

    de::FilePath logFilePath = de::FilePath::join(workingDir, "TestResults.qpa");
    m_logFileName            = logFilePath.getPath();

    // Remove old file if such exists.
    if (deFileExists(m_logFileName.c_str()))
    {
        if (!deDeleteFile(m_logFileName.c_str()) || deFileExists(m_logFileName.c_str()))
            throw TestProcessException(string("Failed to remove '") + m_logFileName + "'");
    }

    // Construct command line.
    string cmdLine = de::FilePath(name).isAbsolutePath() ? name : de::FilePath::join(workingDir, name).getPath();
    cmdLine += string(" --deqp-log-filename=") + logFilePath.getBaseName();

    if (hasCaseList)
        cmdLine += " --deqp-stdin-caselist";

    if (strlen(params) > 0)
        cmdLine += string(" ") + params;

    DE_ASSERT(!m_process);
    m_process = new de::Process();

    try
    {
        m_process->start(cmdLine.c_str(), strlen(workingDir) > 0 ? workingDir : nullptr);
    }
    catch (const de::ProcessError &e)
    {
        delete m_process;
        m_process = nullptr;
        throw TestProcessException(e.what());
    }

    m_processStartTime = deGetMicroseconds();

    // Create stdout & stderr readers.
    if (m_process->getStdOut())
        m_stdOutReader.start(m_process->getStdOut());

    if (m_process->getStdErr())
        m_stdErrReader.start(m_process->getStdErr());

    // Start case list writer.
    if (hasCaseList)
    {
        deFile *dst = m_process->getStdIn();
        if (dst)
            m_caseListWriter.start(caseList, dst);
        else
        {
            cleanup();
            throw TestProcessException("Failed to write case list");
        }
    }
}

void PosixTestProcess::terminate(void)
{
    if (m_process)
    {
        try
        {
            m_process->kill();
        }
        catch (const std::exception &e)
        {
            printf("PosixTestProcess::terminate(): Failed to kill process: %s\n", e.what());
        }
    }
}

void PosixTestProcess::cleanup(void)
{
    m_caseListWriter.stop();
    m_logReader.stop();

    // \note Info buffer must be canceled before stopping pipe readers.
    m_infoBuffer.cancel();

    m_stdErrReader.stop();
    m_stdOutReader.stop();

    // Reset info buffer.
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
        catch (const de::ProcessError &e)
        {
            printf("PosixTestProcess::stop(): Failed to kill process: %s\n", e.what());
        }

        delete m_process;
        m_process = nullptr;
    }
}

bool PosixTestProcess::isRunning(void)
{
    if (m_process)
        return m_process->isRunning();
    else
        return false;
}

int PosixTestProcess::getExitCode(void) const
{
    if (m_process)
        return m_process->getExitCode();
    else
        return -1;
}

int PosixTestProcess::readTestLog(uint8_t *dst, int numBytes)
{
    if (!m_logReader.isRunning())
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
        m_logReader.start(m_logFileName.c_str());
    }

    DE_ASSERT(m_logReader.isRunning());
    return m_logReader.read(dst, numBytes);
}

} // namespace xs
