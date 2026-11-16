#ifndef _XSTESTDRIVER_HPP
#define _XSTESTDRIVER_HPP
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

#include "xsDefs.hpp"
#include "xsProtocol.hpp"
#include "xsTestProcess.hpp"

#include <vector>

namespace xs
{

class TestDriver
{
public:
    TestDriver(xs::TestProcess *testProcess);
    ~TestDriver(void);

    void reset(void);

    void startProcess(const char *name, const char *params, const char *workingDir, const char *caseList);
    void stopProcess(void);

    bool poll(ByteBuffer &messageBuffer);

private:
    enum State
    {
        STATE_NOT_STARTED = 0,
        STATE_PROCESS_LAUNCH_FAILED,
        STATE_PROCESS_STARTED,
        STATE_PROCESS_RUNNING,
        STATE_READING_DATA,
        STATE_PROCESS_FINISHED,

        STATE_LAST
    };

    bool pollLogFile(ByteBuffer &messageBuffer);
    bool pollInfo(ByteBuffer &messageBuffer);
    bool pollBuffer(ByteBuffer &messageBuffer, MessageType msgType);

    bool writeMessage(ByteBuffer &messageBuffer, const Message &message);

    State m_state;

    std::string m_lastLaunchFailure;
    int m_lastExitCode;

    xs::TestProcess *m_process;
    uint64_t m_lastProcessDataTime;

    std::vector<uint8_t> m_dataMsgTmpBuf;
};

} // namespace xs

#endif // _XSTESTDRIVER_HPP
