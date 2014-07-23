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
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO , PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessThrottler.h"

#include "WebProcessProxy.h"

namespace WebKit {
    
static const unsigned processSuspensionTimeout = 30;
    
ProcessThrottler::ForegroundActivityToken::ForegroundActivityToken(ProcessThrottler& throttler)
    : m_throttler(throttler.weakPtr())
{
    throttler.m_foregroundCount++;
    throttler.updateAssertion();
}

ProcessThrottler::ForegroundActivityToken::~ForegroundActivityToken()
{
    if (ProcessThrottler* throttler = m_throttler.get()) {
        throttler->m_foregroundCount--;
        throttler->updateAssertion();
    }
}

ProcessThrottler::BackgroundActivityToken::BackgroundActivityToken(ProcessThrottler& throttler)
    : m_throttler(throttler.weakPtr())
{
    throttler.m_backgroundCount++;
    throttler.updateAssertion();
}

ProcessThrottler::BackgroundActivityToken::~BackgroundActivityToken()
{
    if (ProcessThrottler* throttler = m_throttler.get()) {
        throttler->m_backgroundCount--;
        throttler->updateAssertion();
    }
}

ProcessThrottler::ProcessThrottler(WebProcessProxy* process)
    : m_process(process)
    , m_weakPtrFactory(this)
    , m_suspendTimer(RunLoop::main(), this, &ProcessThrottler::suspendTimerFired)
    , m_foregroundCount(0)
    , m_backgroundCount(0)
    , m_suspendMessageCount(0)
{
}
    
AssertionState ProcessThrottler::assertionState()
{
    ASSERT(!m_suspendTimer.isActive());
    
    if (m_foregroundCount)
        return AssertionState::Foreground;
    if (m_backgroundCount)
        return AssertionState::Background;
    return AssertionState::Suspended;
}
    
void ProcessThrottler::updateAssertionNow()
{
    m_suspendTimer.stop();
    if (m_assertion)
        m_assertion->setState(assertionState());
}
    
void ProcessThrottler::updateAssertion()
{
    // If the process is currently runnable but will be suspended then first give it a chance to complete what it was doing
    // and clean up - move it to the background and send it a message to notify. Schedule a timeout so it can't stay running
    // in the background for too long.
    if (m_assertion && m_assertion->state() != AssertionState::Suspended && !m_foregroundCount && !m_backgroundCount) {
        ++m_suspendMessageCount;
        m_process->sendProcessWillSuspend();
        m_suspendTimer.startOneShot(processSuspensionTimeout);
        m_assertion->setState(AssertionState::Background);
        return;
    }

    // If we're currently waiting for the Web process to do suspension cleanup, but no longer need to be suspended, tell the Web process to cancel the cleanup.
    if (m_suspendTimer.isActive() && (m_foregroundCount || m_backgroundCount))
        m_process->sendCancelProcessWillSuspend();

    updateAssertionNow();
}

void ProcessThrottler::didConnnectToProcess(pid_t pid)
{
    m_suspendTimer.stop();
    m_assertion = std::make_unique<ProcessAndUIAssertion>(pid, assertionState());
}
    
void ProcessThrottler::suspendTimerFired()
{
    updateAssertionNow();
}
    
void ProcessThrottler::processReadyToSuspend()
{
    if (!--m_suspendMessageCount)
        updateAssertionNow();
    ASSERT(m_suspendMessageCount >= 0);
}

void ProcessThrottler::didCancelProcessSuspension()
{
    if (!--m_suspendMessageCount)
        updateAssertionNow();
    ASSERT(m_suspendMessageCount >= 0);
}

}
