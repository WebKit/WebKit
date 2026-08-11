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
 * \brief Test Driver.
 *//*--------------------------------------------------------------------*/

#include "xsTestDriver.hpp"
#include "deClock.h"

#include <string>
#include <vector>
#include <cstdio>

using std::string;
using std::vector;

#if 0
#define DBG_PRINT(X) printf X
#else
#define DBG_PRINT(X)
#endif

namespace xs
{

TestDriver::TestDriver(xs::TestProcess *testProcess)
    : m_state(STATE_NOT_STARTED)
    , m_lastExitCode(0)
    , m_process(testProcess)
    , m_lastProcessDataTime(0)
    , m_dataMsgTmpBuf(SEND_RECV_TMP_BUFFER_SIZE)
{
}

TestDriver::~TestDriver(void)
{
    reset();
}

void TestDriver::reset(void)
{
    m_process->cleanup();

    m_state = STATE_NOT_STARTED;
}

void TestDriver::startProcess(const char *name, const char *params, const char *workingDir, const char *caseList)
{
    try
    {
        m_process->start(name, params, workingDir, caseList);
        m_state = STATE_PROCESS_STARTED;
    }
    catch (const TestProcessException &e)
    {
        printf("Failed to launch test process: %s\n", e.what());
        m_state             = STATE_PROCESS_LAUNCH_FAILED;
        m_lastLaunchFailure = e.what();
    }
}

void TestDriver::stopProcess(void)
{
    m_process->terminate();
}

bool TestDriver::poll(ByteBuffer &messageBuffer)
{
    switch (m_state)
    {
    case STATE_NOT_STARTED:
        return false; // Nothing to report.

    case STATE_PROCESS_LAUNCH_FAILED:
        DBG_PRINT(("  STATE_PROCESS_LAUNCH_FAILED\n"));
        if (writeMessage(messageBuffer, ProcessLaunchFailedMessage(m_lastLaunchFailure.c_str())))
        {
            m_state             = STATE_NOT_STARTED;
            m_lastLaunchFailure = "";
            return true;
        }
        else
            return false;

    case STATE_PROCESS_STARTED:
        DBG_PRINT(("  STATE_PROCESS_STARTED\n"));
        if (writeMessage(messageBuffer, ProcessStartedMessage()))
        {
            m_state = STATE_PROCESS_RUNNING;
            return true;
        }
        else
            return false;

    case STATE_PROCESS_RUNNING:
    {
        DBG_PRINT(("  STATE_PROCESS_RUNNING\n"));
        bool gotProcessData = false;

        // Poll log file and info buffer.
        gotProcessData = pollLogFile(messageBuffer) || gotProcessData;
        gotProcessData = pollInfo(messageBuffer) || gotProcessData;

        if (gotProcessData)
            return true; // Got IO.

        if (!m_process->isRunning())
        {
            // Process died.
            m_state               = STATE_READING_DATA;
            m_lastExitCode        = m_process->getExitCode();
            m_lastProcessDataTime = deGetMicroseconds();

            return true; // Got state change.
        }

        return false; // Nothing to report.
    }

    case STATE_READING_DATA:
    {
        DBG_PRINT(("  STATE_READING_DATA\n"));
        bool gotProcessData = false;

        // Poll log file and info buffer.
        gotProcessData = pollLogFile(messageBuffer) || gotProcessData;
        gotProcessData = pollInfo(messageBuffer) || gotProcessData;

        if (gotProcessData)
        {
            // Got data.
            m_lastProcessDataTime = deGetMicroseconds();
            return true;
        }
        else if (deGetMicroseconds() - m_lastProcessDataTime > READ_DATA_TIMEOUT * 1000)
        {
            // Read timeout occurred.
            m_state = STATE_PROCESS_FINISHED;
            return true; // State change.
        }
        else
            return false; // Still waiting for data.
    }

    case STATE_PROCESS_FINISHED:
        DBG_PRINT(("  STATE_PROCESS_FINISHED\n"));
        if (writeMessage(messageBuffer, ProcessFinishedMessage(m_lastExitCode)))
        {
            // Signal TestProcess to clean up any remaining resources.
            m_process->cleanup();

            m_state        = STATE_NOT_STARTED;
            m_lastExitCode = 0;
            return true;
        }
        else
            return false;

    default:
        DE_ASSERT(false);
        return false;
    }
}

bool TestDriver::pollLogFile(ByteBuffer &messageBuffer)
{
    return pollBuffer(messageBuffer, MESSAGETYPE_PROCESS_LOG_DATA);
}

bool TestDriver::pollInfo(ByteBuffer &messageBuffer)
{
    return pollBuffer(messageBuffer, MESSAGETYPE_INFO);
}

bool TestDriver::pollBuffer(ByteBuffer &messageBuffer, MessageType msgType)
{
    const int minBytesAvailable = MESSAGE_HEADER_SIZE + MIN_MSG_PAYLOAD_SIZE;

    if (messageBuffer.getNumFree() < minBytesAvailable)
        return false; // Not enough space in message buffer.

    const int maxMsgSize = de::min((int)m_dataMsgTmpBuf.size(), messageBuffer.getNumFree());
    int numRead          = 0;
    int msgSize          = MESSAGE_HEADER_SIZE + 1; // One byte is reserved for terminating 0.

    // Fill in data \note Last byte is reserved for 0.
    numRead = msgType == MESSAGETYPE_PROCESS_LOG_DATA ?
                  m_process->readTestLog(&m_dataMsgTmpBuf[MESSAGE_HEADER_SIZE], maxMsgSize - MESSAGE_HEADER_SIZE - 1) :
                  m_process->readInfoLog(&m_dataMsgTmpBuf[MESSAGE_HEADER_SIZE], maxMsgSize - MESSAGE_HEADER_SIZE - 1);

    if (numRead <= 0)
        return false; // Didn't get any data.

    msgSize += numRead;

    // Terminate with 0.
    m_dataMsgTmpBuf[msgSize - 1] = 0;

    // Write header.
    Message::writeHeader(msgType, msgSize, &m_dataMsgTmpBuf[0], MESSAGE_HEADER_SIZE);

    // Write to messagebuffer.
    messageBuffer.pushFront(&m_dataMsgTmpBuf[0], msgSize);

    DBG_PRINT(("  wrote %d bytes of %s data\n", msgSize, msgType == MESSAGETYPE_INFO ? "info" : "log"));

    return true;
}

bool TestDriver::writeMessage(ByteBuffer &messageBuffer, const Message &message)
{
    vector<uint8_t> buf;
    message.write(buf);

    if (messageBuffer.getNumFree() < (int)buf.size())
        return false;

    messageBuffer.pushFront(&buf[0], (int)buf.size());
    return true;
}

} // namespace xs
