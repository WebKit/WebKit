/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
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

#ifndef GenericWorkerTask_h
#define GenericWorkerTask_h

#if ENABLE(WORKERS)

#include "WorkerMessagingProxy.h"
#include "ScriptExecutionContext.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {
    class GenericWorkerTaskBase : public ScriptExecutionContext::Task {
    protected:
        GenericWorkerTaskBase(WorkerMessagingProxy* messagingProxy) : m_messagingProxy(messagingProxy)
        {
        }

        bool canPerformTask()
        {
            return !m_messagingProxy->askedToTerminate();
        }

        WorkerMessagingProxy* m_messagingProxy;
    };

    template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6>
    class GenericWorkerTask6 : public GenericWorkerTaskBase {
    public:
        typedef void (*Method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6);
        typedef GenericWorkerTask6<P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5, P6, MP6> GenericWorkerTask;

        static PassRefPtr<GenericWorkerTask> create(WorkerMessagingProxy* messagingProxy, Method method, const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
        {
            return adoptRef(new GenericWorkerTask(messagingProxy, method, parameter1, parameter2, parameter3, parameter4, parameter5, parameter6));
        }

    private:
        GenericWorkerTask6(WorkerMessagingProxy* messagingProxy, Method method, const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
            : GenericWorkerTaskBase(messagingProxy)
            , m_method(method)
            , m_parameter1(parameter1)
            , m_parameter2(parameter2)
            , m_parameter3(parameter3)
            , m_parameter4(parameter4)
            , m_parameter5(parameter5)
            , m_parameter6(parameter6)
        {
        }

        virtual void performTask(ScriptExecutionContext* context)
        {
            if (!canPerformTask())
                return;
            (*m_method)(context, m_parameter1, m_parameter2, m_parameter3, m_parameter4, m_parameter5, m_parameter6);
        }

    private:
        Method m_method;
        P1 m_parameter1;
        P2 m_parameter2;
        P3 m_parameter3;
        P4 m_parameter4;
        P5 m_parameter5;
        P6 m_parameter6;
    };

    template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6>
    PassRefPtr<GenericWorkerTask6<P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5, P6, MP6> > createCallbackTask(
        WorkerMessagingProxy* messagingProxy,
        void (*method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6),
        const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
    {
        return GenericWorkerTask6<P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5, P6, MP6>::create(messagingProxy, method, parameter1, parameter2, parameter3, parameter4, parameter5, parameter6);
    }

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // GenericWorkerTask_h
