#ifndef _XSEXECUTIONSERVER_HPP
#define _XSEXECUTIONSERVER_HPP
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

#include "xsDefs.hpp"
#include "xsTcpServer.hpp"
#include "xsTestDriver.hpp"
#include "xsProtocol.hpp"
#include "xsTestProcess.hpp"

#include <vector>

namespace xs
{

class ExecutionServer : public TcpServer
{
public:
    enum RunMode
    {
        RUNMODE_SINGLE_EXEC = 0,
        RUNMODE_FOREVER,

        RUNMODE_LAST
    };

    ExecutionServer(xs::TestProcess *testProcess, deSocketFamily family, int port, RunMode runMode);
    ~ExecutionServer(void);

    ConnectionHandler *createHandler(de::Socket *socket, const de::SocketAddress &clientAddress);

    TestDriver *acquireTestDriver(void);
    void releaseTestDriver(TestDriver *driver);

    void connectionDone(ConnectionHandler *handler);

private:
    TestDriver m_testDriver;
    de::Mutex m_testDriverLock;
    RunMode m_runMode;
};

class MessageBuilder
{
public:
    MessageBuilder(void)
    {
        clear();
    }
    ~MessageBuilder(void)
    {
    }

    void read(ByteBuffer &buffer);
    void clear(void);

    bool isComplete(void) const;
    MessageType getMessageType(void) const
    {
        return m_messageType;
    }
    size_t getMessageSize(void) const
    {
        return m_messageSize;
    }
    const uint8_t *getMessageData(void) const;
    size_t getMessageDataSize(void) const;

private:
    std::vector<uint8_t> m_buffer;
    MessageType m_messageType;
    size_t m_messageSize;
};

class ExecutionRequestHandler : public ConnectionHandler
{
public:
    ExecutionRequestHandler(ExecutionServer *server, de::Socket *socket);
    ~ExecutionRequestHandler(void);

protected:
    void handle(void);

private:
    ExecutionRequestHandler(const ExecutionRequestHandler &handler);
    ExecutionRequestHandler &operator=(const ExecutionRequestHandler &handler);

    void processSession(void);
    void processMessage(MessageType type, const uint8_t *data, size_t dataSize);

    inline TestDriver *getTestDriver(void)
    {
        if (!m_testDriver)
            acquireTestDriver();
        return m_testDriver;
    }
    void acquireTestDriver(void);

    void initKeepAlives(void);
    void keepAliveReceived(void);
    void pollKeepAlives(void);

    bool receive(void);
    bool send(void);

    ExecutionServer *m_execServer;
    TestDriver *m_testDriver;

    ByteBuffer m_bufferIn;
    ByteBuffer m_bufferOut;

    bool m_run;
    MessageBuilder m_msgBuilder;

    // \todo [2011-09-30 pyry] Move to some watchdog class instead.
    uint64_t m_lastKeepAliveSent;
    uint64_t m_lastKeepAliveReceived;

    std::vector<uint8_t> m_sendRecvTmpBuf;
};

} // namespace xs

#endif // _XSEXECUTIONSERVER_HPP
