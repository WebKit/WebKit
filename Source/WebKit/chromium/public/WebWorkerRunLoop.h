/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebWorkerRunLoop_h
#define WebWorkerRunLoop_h

#include "platform/WebCommon.h"

namespace WebCore {
class WorkerRunLoop;
}

namespace WebKit {

class WebWorkerRunLoop {
public:
    class Task {
    public:
        virtual ~Task() { }
        virtual void Run() = 0;
    };
    WEBKIT_EXPORT void postTask(Task*);
    WEBKIT_EXPORT bool equals(const WebWorkerRunLoop&) const;
    WEBKIT_EXPORT bool lessThan(const WebWorkerRunLoop&) const;

#if WEBKIT_IMPLEMENTATION
    WebWorkerRunLoop(WebCore::WorkerRunLoop*);
#endif

private:
    WebCore::WorkerRunLoop* m_workerRunLoop;
};

inline bool operator==(const WebWorkerRunLoop& a, const WebWorkerRunLoop& b)
{
    return a.equals(b);
}

inline bool operator<(const WebWorkerRunLoop& a, const WebWorkerRunLoop& b)
{
    return a.lessThan(b);
}

}

#endif
