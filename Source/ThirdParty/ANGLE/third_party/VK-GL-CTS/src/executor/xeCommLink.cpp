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

#include "xeCommLink.hpp"

namespace xe
{

const char *getCommLinkStateName(CommLinkState state)
{
    switch (state)
    {
    case COMMLINKSTATE_READY:
        return "COMMLINKSTATE_READY";
    case COMMLINKSTATE_TEST_PROCESS_LAUNCHING:
        return "COMMLINKSTATE_PROCESS_LAUNCHING";
    case COMMLINKSTATE_TEST_PROCESS_RUNNING:
        return "COMMLINKSTATE_PROCESS_RUNNING";
    case COMMLINKSTATE_TEST_PROCESS_FINISHED:
        return "COMMLINKSTATE_FINISHED";
    case COMMLINKSTATE_TEST_PROCESS_LAUNCH_FAILED:
        return "COMMLINKSTATE_LAUNCH_FAILED";
    case COMMLINKSTATE_ERROR:
        return "COMMLINKSTATE_ERROR";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

CommLink::CommLink(void)
{
}

CommLink::~CommLink(void)
{
}

} // namespace xe
