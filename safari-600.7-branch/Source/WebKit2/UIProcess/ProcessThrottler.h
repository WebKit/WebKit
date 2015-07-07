/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef ProcessThrottler_h
#define ProcessThrottler_h

#include "ProcessAssertion.h"

#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
    
class WebProcessProxy;
    
class ProcessThrottler {
public:
    class ForegroundActivityToken {
    public:
        ForegroundActivityToken(ProcessThrottler&);
        ~ForegroundActivityToken();
        
    private:
        WeakPtr<ProcessThrottler> m_throttler;
    };
    
    class BackgroundActivityToken {
    public:
        BackgroundActivityToken(ProcessThrottler&);
        ~BackgroundActivityToken();
        
    private:
        WeakPtr<ProcessThrottler> m_throttler;
    };
    
    ProcessThrottler(WebProcessProxy*);
    
    void didConnnectToProcess(pid_t);
    void processReadyToSuspend();
    void didCancelProcessSuspension();
    
private:
    friend class ForegroundActivityToken;
    friend class BackgroundActivityToken;
    WeakPtr<ProcessThrottler> weakPtr() { return m_weakPtrFactory.createWeakPtr(); }
    
    AssertionState assertionState();
    void updateAssertion();
    void updateAssertionNow();
    void suspendTimerFired();
    
    WebProcessProxy* m_process;
    WeakPtrFactory<ProcessThrottler> m_weakPtrFactory;
    std::unique_ptr<ProcessAndUIAssertion> m_assertion;
    RunLoop::Timer<ProcessThrottler> m_suspendTimer;
    unsigned m_foregroundCount;
    unsigned m_backgroundCount;
    int m_suspendMessageCount;
};
    
}

#endif // ProcessThrottler_h
