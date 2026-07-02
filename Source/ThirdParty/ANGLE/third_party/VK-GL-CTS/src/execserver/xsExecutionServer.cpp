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
 * \brief Test Execution Server.
 *//*--------------------------------------------------------------------*/

#include "xsExecutionServer.hpp"
#include "deClock.h"

#include <cstdio>

using std::string;
using std::vector;

#if 1
#define DBG_PRINT(X) printf X
#else
#define DBG_PRINT(X)
#endif

namespace xs
{

inline bool MessageBuilder::isComplete(void) const
{
    if (m_buffer.size() < MESSAGE_HEADER_SIZE)
        return false;
    else
        return m_buffer.size() == getMessageSize();
}

const uint8_t *MessageBuilder::getMessageData(void) const
{
    return m_buffer.size() > MESSAGE_HEADER_SIZE ? &m_buffer[MESSAGE_HEADER_SIZE] : nullptr;
}

size_t MessageBuilder::getMessageDataSize(void) const
{
    DE_ASSERT(isComplete());
    return m_buffer.size() - MESSAGE_HEADER_SIZE;
}

void MessageBuilder::read(ByteBuffer &src)
{
    // Try to get header.
    if (m_buffer.size() < MESSAGE_HEADER_SIZE)
    {
        while (m_buffer.size() < MESSAGE_HEADER_SIZE && src.getNumElements() > 0)
            m_buffer.push_back(src.popBack());

        DE_ASSERT(m_buffer.size() <= MESSAGE_HEADER_SIZE);

        if (m_buffer.size() == MESSAGE_HEADER_SIZE)
        {
            // Got whole header, parse it.
            Message::parseHeader(&m_buffer[0], (int)m_buffer.size(), m_messageType, m_messageSize);
        }
    }

    if (m_buffer.size() >= MESSAGE_HEADER_SIZE)
    {
        // We have header.
        size_t msgSize      = getMessageSize();
        size_t numBytesLeft = msgSize - m_buffer.size();
        size_t numToRead    = (size_t)de::min(src.getNumElements(), (int)numBytesLeft);

        if (numToRead > 0)
        {
            int curBufPos = (int)m_buffer.size();
            m_buffer.resize(curBufPos + numToRead);
            src.popBack(&m_buffer[curBufPos], (int)numToRead);
        }
    }
}

void MessageBuilder::clear(void)
{
    m_buffer.clear();
    m_messageType = MESSAGETYPE_NONE;
    m_messageSize = 0;
}

ExecutionServer::ExecutionServer(xs::TestProcess *testProcess, deSocketFamily family, int port, RunMode runMode)
    : TcpServer(family, port)
    , m_testDriver(testProcess)
    , m_runMode(runMode)
{
}

ExecutionServer::~ExecutionServer(void)
{
}

TestDriver *ExecutionServer::acquireTestDriver(void)
{
    if (!m_testDriverLock.tryLock())
        throw Error("Failed to acquire test driver");

    return &m_testDriver;
}

void ExecutionServer::releaseTestDriver(TestDriver *driver)
{
    DE_ASSERT(&m_testDriver == driver);
    DE_UNREF(driver);
    m_testDriverLock.unlock();
}

ConnectionHandler *ExecutionServer::createHandler(de::Socket *socket, const de::SocketAddress &clientAddress)
{
    printf("ExecutionServer: New connection from %s:%d\n", clientAddress.getHost(), clientAddress.getPort());
    return new ExecutionRequestHandler(this, socket);
}

void ExecutionServer::connectionDone(ConnectionHandler *handler)
{
    if (m_runMode == RUNMODE_SINGLE_EXEC)
        m_socket.close();

    TcpServer::connectionDone(handler);
}

ExecutionRequestHandler::ExecutionRequestHandler(ExecutionServer *server, de::Socket *socket)
    : ConnectionHandler(server, socket)
    , m_execServer(server)
    , m_testDriver(nullptr)
    , m_bufferIn(RECV_BUFFER_SIZE)
    , m_bufferOut(SEND_BUFFER_SIZE)
    , m_run(false)
    , m_sendRecvTmpBuf(SEND_RECV_TMP_BUFFER_SIZE)
{
    // Set flags.
    m_socket->setFlags(DE_SOCKET_NONBLOCKING | DE_SOCKET_KEEPALIVE | DE_SOCKET_CLOSE_ON_EXEC);

    // Init protocol keepalives.
    initKeepAlives();
}

ExecutionRequestHandler::~ExecutionRequestHandler(void)
{
    if (m_testDriver)
        m_execServer->releaseTestDriver(m_testDriver);
}

void ExecutionRequestHandler::handle(void)
{
    DBG_PRINT(("ExecutionRequestHandler::handle()\n"));

    try
    {
        // Process execution session.
        processSession();
    }
    catch (const std::exception &e)
    {
        printf("ExecutionRequestHandler::run(): %s\n", e.what());
    }

    DBG_PRINT(("ExecutionRequestHandler::handle(): Done!\n"));

    // Release test driver.
    if (m_testDriver)
    {
        try
        {
            m_testDriver->reset();
        }
        catch (...)
        {
        }
        m_execServer->releaseTestDriver(m_testDriver);
        m_testDriver = nullptr;
    }

    // Close connection.
    if (m_socket->isConnected())
        m_socket->shutdown();
}

void ExecutionRequestHandler::acquireTestDriver(void)
{
    DE_ASSERT(!m_testDriver);

    // Try to acquire test driver - may fail.
    m_testDriver = m_execServer->acquireTestDriver();
    DE_ASSERT(m_testDriver);
    m_testDriver->reset();
}

void ExecutionRequestHandler::processSession(void)
{
    m_run = true;

    uint64_t lastIoTime = deGetMicroseconds();

    while (m_run)
    {
        bool anyIO = false;

        // Read from socket to buffer.
        anyIO = receive() || anyIO;

        // Send bytes in buffer.
        anyIO = send() || anyIO;

        // Process incoming data.
        if (m_bufferIn.getNumElements() > 0)
        {
            DE_ASSERT(!m_msgBuilder.isComplete());
            m_msgBuilder.read(m_bufferIn);
        }

        if (m_msgBuilder.isComplete())
        {
            // Process message.
            processMessage(m_msgBuilder.getMessageType(), m_msgBuilder.getMessageData(),
                           m_msgBuilder.getMessageDataSize());

            m_msgBuilder.clear();
        }

        // Keepalives, anyone?
        pollKeepAlives();

        // Poll test driver for IO.
        if (m_testDriver)
            anyIO = getTestDriver()->poll(m_bufferOut) || anyIO;

        // If no IO happens in a reasonable amount of time, go to sleep.
        {
            uint64_t curTime = deGetMicroseconds();
            if (anyIO)
                lastIoTime = curTime;
            else if (curTime - lastIoTime > SERVER_IDLE_THRESHOLD * 1000)
                deSleep(SERVER_IDLE_SLEEP); // Too long since last IO, sleep for a while.
            else
                deYield(); // Just give other threads chance to run.
        }
    }
}

void ExecutionRequestHandler::processMessage(MessageType type, const uint8_t *data, size_t dataSize)
{
    switch (type)
    {
    case MESSAGETYPE_HELLO:
    {
        HelloMessage msg(data, dataSize);
        DBG_PRINT(("HelloMessage: version = %d\n", msg.version));
        if (msg.version != PROTOCOL_VERSION)
            throw ProtocolError("Unsupported protocol version");
        break;
    }

    case MESSAGETYPE_TEST:
    {
        TestMessage msg(data, dataSize);
        DBG_PRINT(("TestMessage: '%s'\n", msg.test.c_str()));
        break;
    }

    case MESSAGETYPE_KEEPALIVE:
    {
        KeepAliveMessage msg(data, dataSize);
        DBG_PRINT(("KeepAliveMessage\n"));
        keepAliveReceived();
        break;
    }

    case MESSAGETYPE_EXECUTE_BINARY:
    {
        ExecuteBinaryMessage msg(data, dataSize);
        DBG_PRINT(("ExecuteBinaryMessage: '%s', '%s', '%s', '%s'\n", msg.name.c_str(), msg.params.c_str(),
                   msg.workDir.c_str(), msg.caseList.substr(0, 10).c_str()));
        getTestDriver()->startProcess(msg.name.c_str(), msg.params.c_str(), msg.workDir.c_str(), msg.caseList.c_str());
        keepAliveReceived(); // \todo [2011-10-11 pyry] Remove this once Candy is fixed.
        break;
    }

    case MESSAGETYPE_STOP_EXECUTION:
    {
        StopExecutionMessage msg(data, dataSize);
        DBG_PRINT(("StopExecutionMessage\n"));
        getTestDriver()->stopProcess();
        break;
    }

    default:
        throw ProtocolError("Unsupported message");
    }
}

void ExecutionRequestHandler::initKeepAlives(void)
{
    uint64_t curTime        = deGetMicroseconds();
    m_lastKeepAliveSent     = curTime;
    m_lastKeepAliveReceived = curTime;
}

void ExecutionRequestHandler::keepAliveReceived(void)
{
    m_lastKeepAliveReceived = deGetMicroseconds();
}

void ExecutionRequestHandler::pollKeepAlives(void)
{
    uint64_t curTime = deGetMicroseconds();

    // Check that we've got keepalives in timely fashion.
    if (curTime - m_lastKeepAliveReceived > KEEPALIVE_TIMEOUT * 1000)
        throw ProtocolError("Keepalive timeout occurred");

    // Send some?
    if (curTime - m_lastKeepAliveSent > KEEPALIVE_SEND_INTERVAL * 1000 &&
        m_bufferOut.getNumFree() >= MESSAGE_HEADER_SIZE)
    {
        vector<uint8_t> buf;
        KeepAliveMessage().write(buf);
        m_bufferOut.pushFront(&buf[0], (int)buf.size());

        m_lastKeepAliveSent = deGetMicroseconds();
    }
}

bool ExecutionRequestHandler::receive(void)
{
    size_t maxLen = de::min(m_sendRecvTmpBuf.size(), (size_t)m_bufferIn.getNumFree());

    if (maxLen > 0)
    {
        size_t numRecv;
        deSocketResult result = m_socket->receive(&m_sendRecvTmpBuf[0], maxLen, &numRecv);

        if (result == DE_SOCKETRESULT_SUCCESS)
        {
            DE_ASSERT(numRecv > 0);
            m_bufferIn.pushFront(&m_sendRecvTmpBuf[0], (int)numRecv);
            return true;
        }
        else if (result == DE_SOCKETRESULT_CONNECTION_CLOSED)
        {
            m_run = false;
            return true;
        }
        else if (result == DE_SOCKETRESULT_WOULD_BLOCK)
            return false;
        else if (result == DE_SOCKETRESULT_CONNECTION_TERMINATED)
            throw ConnectionError("Connection terminated");
        else
            throw ConnectionError("receive() failed");
    }
    else
        return false;
}

bool ExecutionRequestHandler::send(void)
{
    size_t maxLen = de::min(m_sendRecvTmpBuf.size(), (size_t)m_bufferOut.getNumElements());

    if (maxLen > 0)
    {
        m_bufferOut.peekBack(&m_sendRecvTmpBuf[0], (int)maxLen);

        size_t numSent;
        deSocketResult result = m_socket->send(&m_sendRecvTmpBuf[0], maxLen, &numSent);

        if (result == DE_SOCKETRESULT_SUCCESS)
        {
            DE_ASSERT(numSent > 0);
            m_bufferOut.popBack((int)numSent);
            return true;
        }
        else if (result == DE_SOCKETRESULT_CONNECTION_CLOSED)
        {
            m_run = false;
            return true;
        }
        else if (result == DE_SOCKETRESULT_WOULD_BLOCK)
            return false;
        else if (result == DE_SOCKETRESULT_CONNECTION_TERMINATED)
            throw ConnectionError("Connection terminated");
        else
            throw ConnectionError("send() failed");
    }
    else
        return false;
}

} // namespace xs
