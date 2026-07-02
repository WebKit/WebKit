#ifndef _XECOMMLINK_HPP
#define _XECOMMLINK_HPP
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
 * \brief Communication link abstraction.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "xeCallQueue.hpp"

namespace xe
{

enum CommLinkState
{
    COMMLINKSTATE_READY,                  //!< CommLink is ready to accept commands.
    COMMLINKSTATE_TEST_PROCESS_LAUNCHING, //!< Test process is launching. Allowed commands: stop process
    COMMLINKSTATE_TEST_PROCESS_RUNNING,   //!< Test process is running: Allowed commands: stop process
    COMMLINKSTATE_TEST_PROCESS_FINISHED,  //!< Test process is finished. Allowed commands: reset

    COMMLINKSTATE_TEST_PROCESS_LAUNCH_FAILED, //!< Test process launch failed. Allowed commands: reset
    COMMLINKSTATE_ERROR,                      //!< Other error occurred: Allowed commands: reset

    COMMLINKSTATE_LAST
};

const char *getCommLinkStateName(CommLinkState state);

class CommLink
{
public:
    typedef void (*StateChangedFunc)(void *userPtr, CommLinkState state, const char *message);
    typedef void (*LogDataFunc)(void *userPtr, const uint8_t *bytes, size_t numBytes);

    CommLink(void);
    virtual ~CommLink(void);

    virtual void reset(void)                                 = 0;
    virtual CommLinkState getState(void) const               = 0;
    virtual CommLinkState getState(std::string &error) const = 0;

    virtual void setCallbacks(StateChangedFunc stateChangedCallback, LogDataFunc testLogDataCallback,
                              LogDataFunc infoLogDataCallback, void *userPtr) = 0;

    virtual void startTestProcess(const char *name, const char *params, const char *workingDir,
                                  const char *caseList) = 0;
    virtual void stopTestProcess(void)                  = 0;
};

} // namespace xe

#endif // _XECOMMLINK_HPP
