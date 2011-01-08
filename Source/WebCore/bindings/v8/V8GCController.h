/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8GCController_h
#define V8GCController_h

#include <v8.h>

namespace WebCore {

#ifndef NDEBUG

#define GlobalHandleTypeList(V)   \
    V(PROXY)                      \
    V(NPOBJECT)                   \
    V(SCHEDULED_ACTION)           \
    V(EVENT_LISTENER)             \
    V(NODE_FILTER)                \
    V(SCRIPTINSTANCE)             \
    V(SCRIPTVALUE)                \
    V(DATASOURCE)


    // Host information of persistent handles.
    enum GlobalHandleType {
#define ENUM(name) name,
        GlobalHandleTypeList(ENUM)
#undef ENUM
    };

    class GlobalHandleInfo {
    public:
        GlobalHandleInfo(void* host, GlobalHandleType type) : m_host(host), m_type(type) { }
        void* m_host;
        GlobalHandleType m_type;
    };

#endif // NDEBUG

    class V8GCController {
    public:
#ifndef NDEBUG
        // For debugging and leak detection purpose.
        static void registerGlobalHandle(GlobalHandleType, void*, v8::Persistent<v8::Value>);
        static void unregisterGlobalHandle(void*, v8::Persistent<v8::Value>);
#endif

        static void gcPrologue();
        static void gcEpilogue();

        static void checkMemoryUsage();

    private:
        // Estimate of current working set.
        static int workingSetEstimateMB;
    };

}

#endif // V8GCController_h
