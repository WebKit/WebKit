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
 * \brief Tcp/Ip link that manages execserver process.
 *//*--------------------------------------------------------------------*/

#include "xeLocalTcpIpLink.hpp"
#include "deClock.h"
#include "deThread.h"

#include <sstream>

enum
{
    SERVER_START_TIMEOUT    = 1000,
    SERVER_START_IDLE_SLEEP = 50
};

namespace xe
{

LocalTcpIpLink::LocalTcpIpLink(void) : m_process(nullptr)
{
}

LocalTcpIpLink::~LocalTcpIpLink(void)
{
    stop();
}

void LocalTcpIpLink::start(const char *execServerPath, const char *workDir, int port)
{
    XE_CHECK(!m_process);

    std::ostringstream cmdLine;
    cmdLine << execServerPath << " --single --port=" << port;

    m_process = deProcess_create();
    XE_CHECK(m_process);

    if (deProcess_start(m_process, cmdLine.str().c_str(), workDir) != true)
    {
        std::string err = deProcess_getLastError(m_process);
        deProcess_destroy(m_process);
        m_process = nullptr;

        XE_FAIL((std::string("Failed to start ExecServer '") + execServerPath + "' : " + err).c_str());
    }

    try
    {
        de::SocketAddress address;
        address.setFamily(DE_SOCKETFAMILY_INET4);
        address.setProtocol(DE_SOCKETPROTOCOL_TCP);
        address.setHost("127.0.0.1");
        address.setPort(port);

        // Wait until server has started - \todo [2012-07-19 pyry] This could be improved by having server to signal when it is ready.
        uint64_t waitStart = deGetMicroseconds();
        for (;;)
        {
            if (!deProcess_isRunning(m_process))
                XE_FAIL("ExecServer died");

            try
            {
                m_link.connect(address);
                break;
            }
            catch (const de::SocketError &)
            {
                if (deGetMicroseconds() - waitStart > SERVER_START_TIMEOUT * 1000)
                    XE_FAIL("Server start timeout");

                deSleep(SERVER_START_IDLE_SLEEP);
            }
        }

        // Close stdout/stderr or otherwise process will hang once OS pipe buffers are full.
        // \todo [2012-07-19 pyry] Read and store stdout/stderr from execserver.
        XE_CHECK(deProcess_closeStdOut(m_process));
        XE_CHECK(deProcess_closeStdErr(m_process));
    }
    catch (const std::exception &)
    {
        stop();
        throw;
    }
}

void LocalTcpIpLink::stop(void)
{
    if (m_process)
    {
        try
        {
            m_link.disconnect();
        }
        catch (...)
        {
            // Silently ignore since this is called in destructor.
        }

        // \note --single flag is used so execserver should kill itself once one connection is handled.
        //         This is here to make sure it dies even in case of hang.
        deProcess_terminate(m_process);
        deProcess_waitForFinish(m_process);
        deProcess_destroy(m_process);

        m_process = nullptr;
    }
}

void LocalTcpIpLink::reset(void)
{
    m_link.reset();
}

CommLinkState LocalTcpIpLink::getState(void) const
{
    if (!m_process)
        return COMMLINKSTATE_ERROR;
    else
        return m_link.getState();
}

CommLinkState LocalTcpIpLink::getState(std::string &error) const
{
    if (!m_process)
    {
        error = "Not started";
        return COMMLINKSTATE_ERROR;
    }
    else
        return m_link.getState();
}

void LocalTcpIpLink::setCallbacks(StateChangedFunc stateChangedCallback, LogDataFunc testLogDataCallback,
                                  LogDataFunc infoLogDataCallback, void *userPtr)
{
    m_link.setCallbacks(stateChangedCallback, testLogDataCallback, infoLogDataCallback, userPtr);
}

void LocalTcpIpLink::startTestProcess(const char *name, const char *params, const char *workingDir,
                                      const char *caseList)
{
    if (m_process)
        m_link.startTestProcess(name, params, workingDir, caseList);
    else
        XE_FAIL("Not started");
}

void LocalTcpIpLink::stopTestProcess(void)
{
    if (m_process)
        m_link.stopTestProcess();
    else
        XE_FAIL("Not started");
}

} // namespace xe
