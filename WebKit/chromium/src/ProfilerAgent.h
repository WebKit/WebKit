/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ProfilerAgent_h
#define ProfilerAgent_h

#include "DevToolsRPC.h"

namespace WebKit {

// Profiler agent provides API for retrieving profiler data.
// These methods are handled on the IO thread, so profiler can
// operate while a script on a page performs heavy work.
#define PROFILER_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4, METHOD5) \
    /* Requests current profiler state. */ \
    METHOD0(getActiveProfilerModules) \
    \
    /* Retrieves portion of profiler log. */ \
    METHOD1(getLogLines, int /* position */)

DEFINE_RPC_CLASS(ProfilerAgent, PROFILER_AGENT_STRUCT)

#define PROFILER_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4, METHOD5) \
    /* Response to getActiveProfilerModules. */ \
    METHOD1(didGetActiveProfilerModules, int /* flags */) \
    \
    /* Response to getLogLines. */ \
    METHOD2(didGetLogLines, int /* position */, String /* log */)

DEFINE_RPC_CLASS(ProfilerAgentDelegate, PROFILER_AGENT_DELEGATE_STRUCT)

} // namespace WebKit

#endif
