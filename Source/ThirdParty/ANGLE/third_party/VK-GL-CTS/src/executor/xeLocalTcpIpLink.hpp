#ifndef _XELOCALTCPIPLINK_HPP
#define _XELOCALTCPIPLINK_HPP
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

#include "xeDefs.hpp"
#include "xeCommLink.hpp"
#include "xeTcpIpLink.hpp"
#include "deProcess.h"

namespace xe
{

class LocalTcpIpLink : public CommLink
{
public:
    LocalTcpIpLink(void);
    ~LocalTcpIpLink(void);

    // LocalTcpIpLink -specific API
    void start(const char *execServerPath, const char *workDir, int port);
    void stop(void);

    // CommLink API
    void reset(void);

    CommLinkState getState(void) const;
    CommLinkState getState(std::string &error) const;

    void setCallbacks(StateChangedFunc stateChangedCallback, LogDataFunc testLogDataCallback,
                      LogDataFunc infoLogDataCallback, void *userPtr);

    void startTestProcess(const char *name, const char *params, const char *workingDir, const char *caseList);
    void stopTestProcess(void);

private:
    TcpIpLink m_link;
    deProcess *m_process;
};

} // namespace xe

#endif // _XELOCALTCPIPLINK_HPP
