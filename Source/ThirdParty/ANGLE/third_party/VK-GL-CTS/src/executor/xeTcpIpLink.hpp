#ifndef _XETCPIPLINK_HPP
#define _XETCPIPLINK_HPP
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

#include "xeDefs.hpp"
#include "xeCommLink.hpp"
#include "deSocket.hpp"
#include "deRingBuffer.hpp"
#include "deBlockBuffer.hpp"
#include "xsProtocol.hpp"
#include "deThread.hpp"
#include "deTimer.h"

#include <vector>

namespace xe
{

class TcpIpLinkState
{
public:
    TcpIpLinkState(CommLinkState initialState, const char *initialErr);
    ~TcpIpLinkState(void);

    CommLinkState getState(void) const;
    CommLinkState getState(std::string &error) const;

    void setCallbacks(CommLink::StateChangedFunc stateChangedCallback, CommLink::LogDataFunc testLogDataCallback,
                      CommLink::LogDataFunc infoLogDataCallback, void *userPtr);

    void setState(CommLinkState state, const char *error = "");
    void onTestLogData(const uint8_t *bytes, size_t numBytes) const;
    void onInfoLogData(const uint8_t *bytes, size_t numBytes) const;

    void onKeepaliveReceived(void);
    uint64_t getLastKeepaliveRecevied(void) const;

private:
    mutable de::Mutex m_lock;
    volatile CommLinkState m_state;
    std::string m_error;

    volatile uint64_t m_lastKeepaliveReceived;

    volatile CommLink::StateChangedFunc m_stateChangedCallback;
    volatile CommLink::LogDataFunc m_testLogDataCallback;
    volatile CommLink::LogDataFunc m_infoLogDataCallback;
    void *volatile m_userPtr;
};

class TcpIpSendThread : public de::Thread
{
public:
    TcpIpSendThread(de::Socket &socket, TcpIpLinkState &state);
    ~TcpIpSendThread(void);

    void start(void);
    void run(void);
    void stop(void);

    bool isRunning(void) const
    {
        return m_isRunning;
    }

    de::BlockBuffer<uint8_t> &getBuffer(void)
    {
        return m_buffer;
    }

private:
    de::Socket &m_socket;
    TcpIpLinkState &m_state;

    de::BlockBuffer<uint8_t> m_buffer;

    bool m_isRunning;
};

class TcpIpRecvThread : public de::Thread
{
public:
    TcpIpRecvThread(de::Socket &socket, TcpIpLinkState &state);
    ~TcpIpRecvThread(void);

    void start(void);
    void run(void);
    void stop(void);

    bool isRunning(void) const
    {
        return m_isRunning;
    }

private:
    void handleMessage(xs::MessageType messageType, const uint8_t *data, size_t dataSize);

    de::Socket &m_socket;
    TcpIpLinkState &m_state;

    std::vector<uint8_t> m_curMsgBuf;
    size_t m_curMsgPos;

    bool m_isRunning;
};

class TcpIpLink : public CommLink
{
public:
    TcpIpLink(void);
    ~TcpIpLink(void);

    // TcpIpLink -specific API
    void connect(const de::SocketAddress &address);
    void disconnect(void);

    // CommLink API
    void reset(void);

    CommLinkState getState(void) const;
    CommLinkState getState(std::string &error) const;

    void setCallbacks(StateChangedFunc stateChangedCallback, LogDataFunc testLogDataCallback,
                      LogDataFunc infoLogDataCallback, void *userPtr);

    void startTestProcess(const char *name, const char *params, const char *workingDir, const char *caseList);
    void stopTestProcess(void);

private:
    void closeConnection(void);

    static void keepaliveTimerCallback(void *ptr);

    de::Socket m_socket;
    TcpIpLinkState m_state;

    TcpIpSendThread m_sendThread;
    TcpIpRecvThread m_recvThread;

    deTimer *m_keepaliveTimer;
};

} // namespace xe

#endif // _XETCPIPLINK_HPP
