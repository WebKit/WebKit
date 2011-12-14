/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ChildProcess_h
#define ChildProcess_h

#include "Connection.h"
#include "RunLoop.h"

namespace WebKit {

class ChildProcess : protected CoreIPC::Connection::Client {
    WTF_MAKE_NONCOPYABLE(ChildProcess);

public:
    // disable and enable termination of the process. when disableTermination is called, the
    // process won't terminate unless a corresponding disableTermination call is made.
    void disableTermination();
    void enableTermination();

    class LocalTerminationDisabler {
    public:
        explicit LocalTerminationDisabler(ChildProcess& childProcess)
            : m_childProcess(childProcess)
        {
            m_childProcess.disableTermination();
        }

        ~LocalTerminationDisabler()
        {
            m_childProcess.enableTermination();
        }

    private:
        ChildProcess& m_childProcess;
    };

protected:
    explicit ChildProcess(double terminationTimeout);
    ~ChildProcess();

    static void didCloseOnConnectionWorkQueue(WorkQueue&, CoreIPC::Connection*);

private:
    void terminationTimerFired();

    virtual bool shouldTerminate() = 0;
    virtual void terminate();

    // The timeout, in seconds, before this process will be terminated if termination
    // has been enabled. If the timeout is 0 seconds, the process will be terminated immediately.
    double m_terminationTimeout;

    // A termination counter; when the counter reaches zero, the process will be terminated
    // after a given period of time.
    unsigned m_terminationCounter;

    RunLoop::Timer<ChildProcess> m_terminationTimer;
};

} // namespace WebKit

#endif // ChildProcess_h
