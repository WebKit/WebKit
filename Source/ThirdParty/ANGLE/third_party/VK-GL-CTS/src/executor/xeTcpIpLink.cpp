/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief Tcp/Ip communication link.
 *//*--------------------------------------------------------------------*/

#include "xeTcpIpLink.hpp"
#include "xsProtocol.hpp"
#include "deClock.h"
#include "deInt32.h"

namespace xe
{

enum
{
    SEND_BUFFER_BLOCK_SIZE = 1024,
    SEND_BUFFER_NUM_BLOCKS = 64
};

// Utilities for writing messages out.

static void writeMessageHeader(de::BlockBuffer<uint8_t> &dst, xs::MessageType type, int messageSize)
{
    uint8_t hdr[xs::MESSAGE_HEADER_SIZE];
    xs::Message::writeHeader(type, messageSize, &hdr[0], xs::MESSAGE_HEADER_SIZE);
    dst.write(xs::MESSAGE_HEADER_SIZE, &hdr[0]);
}

static void writeKeepalive(de::BlockBuffer<uint8_t> &dst)
{
    writeMessageHeader(dst, xs::MESSAGETYPE_KEEPALIVE, xs::MESSAGE_HEADER_SIZE);
    dst.flush();
}

static void writeExecuteBinary(de::BlockBuffer<uint8_t> &dst, const char *name, const char *params, const char *workDir,
                               const char *caseList)
{
    int nameSize     = (int)strlen(name) + 1;
    int paramsSize   = (int)strlen(params) + 1;
    int workDirSize  = (int)strlen(workDir) + 1;
    int caseListSize = (int)strlen(caseList) + 1;
    int totalSize    = xs::MESSAGE_HEADER_SIZE + nameSize + paramsSize + workDirSize + caseListSize;

    writeMessageHeader(dst, xs::MESSAGETYPE_EXECUTE_BINARY, totalSize);
    dst.write(nameSize, (const uint8_t *)name);
    dst.write(paramsSize, (const uint8_t *)params);
    dst.write(workDirSize, (const uint8_t *)workDir);
    dst.write(caseListSize, (const uint8_t *)caseList);
    dst.flush();
}

static void writeStopExecution(de::BlockBuffer<uint8_t> &dst)
{
    writeMessageHeader(dst, xs::MESSAGETYPE_STOP_EXECUTION, xs::MESSAGE_HEADER_SIZE);
    dst.flush();
}

// TcpIpLinkState

TcpIpLinkState::TcpIpLinkState(CommLinkState initialState, const char *initialErr)
    : m_state(initialState)
    , m_error(initialErr)
    , m_lastKeepaliveReceived(0)
    , m_stateChangedCallback(nullptr)
    , m_testLogDataCallback(nullptr)
    , m_infoLogDataCallback(nullptr)
    , m_userPtr(nullptr)
{
}

TcpIpLinkState::~TcpIpLinkState(void)
{
}

CommLinkState TcpIpLinkState::getState(void) const
{
    de::ScopedLock lock(m_lock);

    return m_state;
}

CommLinkState TcpIpLinkState::getState(std::string &error) const
{
    de::ScopedLock lock(m_lock);

    error = m_error;
    return m_state;
}

void TcpIpLinkState::setCallbacks(CommLink::StateChangedFunc stateChangedCallback,
                                  CommLink::LogDataFunc testLogDataCallback, CommLink::LogDataFunc infoLogDataCallback,
                                  void *userPtr)
{
    de::ScopedLock lock(m_lock);

    m_stateChangedCallback = stateChangedCallback;
    m_testLogDataCallback  = testLogDataCallback;
    m_infoLogDataCallback  = infoLogDataCallback;
    m_userPtr              = userPtr;
}

void TcpIpLinkState::setState(CommLinkState state, const char *error)
{
    CommLink::StateChangedFunc callback = nullptr;
    void *userPtr                       = nullptr;

    {
        de::ScopedLock lock(m_lock);

        m_state = state;
        m_error = error;

        callback = m_stateChangedCallback;
        userPtr  = m_userPtr;
    }

    if (callback)
        callback(userPtr, state, error);
}

void TcpIpLinkState::onTestLogData(const uint8_t *bytes, size_t numBytes) const
{
    CommLink::LogDataFunc callback = nullptr;
    void *userPtr                  = nullptr;

    m_lock.lock();
    callback = m_testLogDataCallback;
    userPtr  = m_userPtr;
    m_lock.unlock();

    if (callback)
        callback(userPtr, bytes, numBytes);
}

void TcpIpLinkState::onInfoLogData(const uint8_t *bytes, size_t numBytes) const
{
    CommLink::LogDataFunc callback = nullptr;
    void *userPtr                  = nullptr;

    m_lock.lock();
    callback = m_infoLogDataCallback;
    userPtr  = m_userPtr;
    m_lock.unlock();

    if (callback)
        callback(userPtr, bytes, numBytes);
}

void TcpIpLinkState::onKeepaliveReceived(void)
{
    de::ScopedLock lock(m_lock);
    m_lastKeepaliveReceived = deGetMicroseconds();
}

uint64_t TcpIpLinkState::getLastKeepaliveRecevied(void) const
{
    de::ScopedLock lock(m_lock);
    return m_lastKeepaliveReceived;
}

// TcpIpSendThread

TcpIpSendThread::TcpIpSendThread(de::Socket &socket, TcpIpLinkState &state)
    : m_socket(socket)
    , m_state(state)
    , m_buffer(SEND_BUFFER_BLOCK_SIZE, SEND_BUFFER_NUM_BLOCKS)
    , m_isRunning(false)
{
}

TcpIpSendThread::~TcpIpSendThread(void)
{
}

void TcpIpSendThread::start(void)
{
    DE_ASSERT(!m_isRunning);

    // Reset state.
    m_buffer.clear();
    m_isRunning = true;

    de::Thread::start();
}

void TcpIpSendThread::run(void)
{
    try
    {
        uint8_t buf[SEND_BUFFER_BLOCK_SIZE];

        while (!m_buffer.isCanceled())
        {
            size_t numToSend      = 0;
            size_t numSent        = 0;
            deSocketResult result = DE_SOCKETRESULT_LAST;

            try
            {
                // Wait for single byte and then try to read more.
                m_buffer.read(1, &buf[0]);
                numToSend = 1 + m_buffer.tryRead(DE_LENGTH_OF_ARRAY(buf) - 1, &buf[1]);
            }
            catch (const de::BlockBuffer<uint8_t>::CanceledException &)
            {
                // Handled in loop condition.
            }

            while (numSent < numToSend)
            {
                result = m_socket.send(&buf[numSent], numToSend - numSent, &numSent);

                if (result == DE_SOCKETRESULT_CONNECTION_CLOSED)
                    XE_FAIL("Connection closed");
                else if (result == DE_SOCKETRESULT_CONNECTION_TERMINATED)
                    XE_FAIL("Connection terminated");
                else if (result == DE_SOCKETRESULT_ERROR)
                    XE_FAIL("Socket error");
                else if (result == DE_SOCKETRESULT_WOULD_BLOCK)
                {
                    // \note Socket should not be in non-blocking mode.
                    DE_ASSERT(numSent == 0);
                    deYield();
                }
                else
                    DE_ASSERT(result == DE_SOCKETRESULT_SUCCESS);
            }
        }
    }
    catch (const std::exception &e)
    {
        m_state.setState(COMMLINKSTATE_ERROR, e.what());
    }
}

void TcpIpSendThread::stop(void)
{
    if (m_isRunning)
    {
        m_buffer.cancel();
        join();
        m_isRunning = false;
    }
}

// TcpIpRecvThread

TcpIpRecvThread::TcpIpRecvThread(de::Socket &socket, TcpIpLinkState &state)
    : m_socket(socket)
    , m_state(state)
    , m_curMsgPos(0)
    , m_isRunning(false)
{
}

TcpIpRecvThread::~TcpIpRecvThread(void)
{
}

void TcpIpRecvThread::start(void)
{
    DE_ASSERT(!m_isRunning);

    // Reset state.
    m_curMsgPos = 0;
    m_isRunning = true;

    de::Thread::start();
}

void TcpIpRecvThread::run(void)
{
    try
    {
        for (;;)
        {
            bool hasHeader              = m_curMsgPos >= xs::MESSAGE_HEADER_SIZE;
            bool hasPayload             = false;
            size_t messageSize          = 0;
            xs::MessageType messageType = (xs::MessageType)0;

            if (hasHeader)
            {
                xs::Message::parseHeader(&m_curMsgBuf[0], xs::MESSAGE_HEADER_SIZE, messageType, messageSize);
                hasPayload = m_curMsgPos >= messageSize;
            }

            if (hasPayload)
            {
                // Process message.
                handleMessage(messageType,
                              m_curMsgPos > xs::MESSAGE_HEADER_SIZE ? &m_curMsgBuf[xs::MESSAGE_HEADER_SIZE] : nullptr,
                              messageSize - xs::MESSAGE_HEADER_SIZE);
                m_curMsgPos = 0;
            }
            else
            {
                // Try to receive missing bytes.
                size_t curSize        = hasHeader ? messageSize : (size_t)xs::MESSAGE_HEADER_SIZE;
                size_t bytesToRecv    = curSize - m_curMsgPos;
                size_t numRecv        = 0;
                deSocketResult result = DE_SOCKETRESULT_LAST;

                if (m_curMsgBuf.size() < curSize)
                    m_curMsgBuf.resize(curSize);

                result = m_socket.receive(&m_curMsgBuf[m_curMsgPos], bytesToRecv, &numRecv);

                if (result == DE_SOCKETRESULT_CONNECTION_CLOSED)
                    XE_FAIL("Connection closed");
                else if (result == DE_SOCKETRESULT_CONNECTION_TERMINATED)
                    XE_FAIL("Connection terminated");
                else if (result == DE_SOCKETRESULT_ERROR)
                    XE_FAIL("Socket error");
                else if (result == DE_SOCKETRESULT_WOULD_BLOCK)
                {
                    // \note Socket should not be in non-blocking mode.
                    DE_ASSERT(numRecv == 0);
                    deYield();
                }
                else
                {
                    DE_ASSERT(result == DE_SOCKETRESULT_SUCCESS);
                    DE_ASSERT(numRecv <= bytesToRecv);
                    m_curMsgPos += numRecv;
                    // Continue receiving bytes / handle message in next iter.
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        m_state.setState(COMMLINKSTATE_ERROR, e.what());
    }
}

void TcpIpRecvThread::stop(void)
{
    if (m_isRunning)
    {
        // \note Socket must be closed before terminating receive thread.
        XE_CHECK(!m_socket.isReceiveOpen());

        join();
        m_isRunning = false;
    }
}

void TcpIpRecvThread::handleMessage(xs::MessageType messageType, const uint8_t *data, size_t dataSize)
{
    switch (messageType)
    {
    case xs::MESSAGETYPE_KEEPALIVE:
        m_state.onKeepaliveReceived();
        break;

    case xs::MESSAGETYPE_PROCESS_STARTED:
        XE_CHECK_MSG(m_state.getState() == COMMLINKSTATE_TEST_PROCESS_LAUNCHING, "Unexpected PROCESS_STARTED message");
        m_state.setState(COMMLINKSTATE_TEST_PROCESS_RUNNING);
        break;

    case xs::MESSAGETYPE_PROCESS_LAUNCH_FAILED:
    {
        xs::ProcessLaunchFailedMessage msg(data, dataSize);
        XE_CHECK_MSG(m_state.getState() == COMMLINKSTATE_TEST_PROCESS_LAUNCHING,
                     "Unexpected PROCESS_LAUNCH_FAILED message");
        m_state.setState(COMMLINKSTATE_TEST_PROCESS_LAUNCH_FAILED, msg.reason.c_str());
        break;
    }

    case xs::MESSAGETYPE_PROCESS_FINISHED:
    {
        XE_CHECK_MSG(m_state.getState() == COMMLINKSTATE_TEST_PROCESS_RUNNING, "Unexpected PROCESS_FINISHED message");
        xs::ProcessFinishedMessage msg(data, dataSize);
        m_state.setState(COMMLINKSTATE_TEST_PROCESS_FINISHED);
        DE_UNREF(msg); // \todo [2012-06-19 pyry] Report exit code.
        break;
    }

    case xs::MESSAGETYPE_PROCESS_LOG_DATA:
    case xs::MESSAGETYPE_INFO:
        // Ignore leading \0 if such is present. \todo [2012-06-19 pyry] Improve protocol.
        if (data[dataSize - 1] == 0)
            dataSize -= 1;

        if (messageType == xs::MESSAGETYPE_PROCESS_LOG_DATA)
        {
            XE_CHECK_MSG(m_state.getState() == COMMLINKSTATE_TEST_PROCESS_RUNNING,
                         "Unexpected PROCESS_LOG_DATA message");
            m_state.onTestLogData(&data[0], dataSize);
        }
        else
            m_state.onInfoLogData(&data[0], dataSize);
        break;

    default:
        XE_FAIL("Unknown message");
    }
}

// TcpIpLink

TcpIpLink::TcpIpLink(void)
    : m_state(COMMLINKSTATE_ERROR, "Not connected")
    , m_sendThread(m_socket, m_state)
    , m_recvThread(m_socket, m_state)
    , m_keepaliveTimer(nullptr)
{
    m_keepaliveTimer = deTimer_create(keepaliveTimerCallback, this);
    XE_CHECK(m_keepaliveTimer);
}

TcpIpLink::~TcpIpLink(void)
{
    try
    {
        closeConnection();
    }
    catch (...)
    {
        // Can't do much except to ignore error.
    }
    deTimer_destroy(m_keepaliveTimer);
}

void TcpIpLink::closeConnection(void)
{
    {
        deSocketState state = m_socket.getState();
        if (state != DE_SOCKETSTATE_DISCONNECTED && state != DE_SOCKETSTATE_CLOSED)
            m_socket.shutdown();
    }

    if (deTimer_isActive(m_keepaliveTimer))
        deTimer_disable(m_keepaliveTimer);

    if (m_sendThread.isRunning())
        m_sendThread.stop();

    if (m_recvThread.isRunning())
        m_recvThread.stop();

    if (m_socket.getState() != DE_SOCKETSTATE_CLOSED)
        m_socket.close();
}

void TcpIpLink::connect(const de::SocketAddress &address)
{
    XE_CHECK(m_socket.getState() == DE_SOCKETSTATE_CLOSED);
    XE_CHECK(m_state.getState() == COMMLINKSTATE_ERROR);
    XE_CHECK(!m_sendThread.isRunning());
    XE_CHECK(!m_recvThread.isRunning());

    m_socket.connect(address);

    try
    {
        // Clear error and set state to ready.
        m_state.setState(COMMLINKSTATE_READY, "");
        m_state.onKeepaliveReceived();

        // Launch threads.
        m_sendThread.start();
        m_recvThread.start();

        XE_CHECK(deTimer_scheduleInterval(m_keepaliveTimer, xs::KEEPALIVE_SEND_INTERVAL));
    }
    catch (const std::exception &e)
    {
        closeConnection();
        m_state.setState(COMMLINKSTATE_ERROR, e.what());
        throw;
    }
}

void TcpIpLink::disconnect(void)
{
    try
    {
        closeConnection();
        m_state.setState(COMMLINKSTATE_ERROR, "Not connected");
    }
    catch (const std::exception &e)
    {
        m_state.setState(COMMLINKSTATE_ERROR, e.what());
    }
}

void TcpIpLink::reset(void)
{
    // \note Just clears error state if we are connected.
    if (m_socket.getState() == DE_SOCKETSTATE_CONNECTED)
    {
        m_state.setState(COMMLINKSTATE_READY, "");

        // \todo [2012-07-10 pyry] Do we need to reset send/receive buffers?
    }
    else
        disconnect(); // Abnormal state/usage. Disconnect socket.
}

void TcpIpLink::keepaliveTimerCallback(void *ptr)
{
    TcpIpLink *link        = static_cast<TcpIpLink *>(ptr);
    uint64_t lastKeepalive = link->m_state.getLastKeepaliveRecevied();
    uint64_t curTime       = deGetMicroseconds();

    // Check for timeout.
    if ((int64_t)curTime - (int64_t)lastKeepalive > xs::KEEPALIVE_TIMEOUT * 1000)
        link->m_state.setState(COMMLINKSTATE_ERROR, "Keepalive timeout");

    // Enqueue new keepalive.
    try
    {
        writeKeepalive(link->m_sendThread.getBuffer());
    }
    catch (const de::BlockBuffer<uint8_t>::CanceledException &)
    {
        // Ignore. Can happen in connection teardown.
    }
}

CommLinkState TcpIpLink::getState(void) const
{
    return m_state.getState();
}

CommLinkState TcpIpLink::getState(std::string &message) const
{
    return m_state.getState(message);
}

void TcpIpLink::setCallbacks(StateChangedFunc stateChangedCallback, LogDataFunc testLogDataCallback,
                             LogDataFunc infoLogDataCallback, void *userPtr)
{
    m_state.setCallbacks(stateChangedCallback, testLogDataCallback, infoLogDataCallback, userPtr);
}

void TcpIpLink::startTestProcess(const char *name, const char *params, const char *workingDir, const char *caseList)
{
    XE_CHECK(m_state.getState() == COMMLINKSTATE_READY);

    m_state.setState(COMMLINKSTATE_TEST_PROCESS_LAUNCHING);
    writeExecuteBinary(m_sendThread.getBuffer(), name, params, workingDir, caseList);
}

void TcpIpLink::stopTestProcess(void)
{
    XE_CHECK(m_state.getState() != COMMLINKSTATE_ERROR);
    writeStopExecution(m_sendThread.getBuffer());
}

} // namespace xe
