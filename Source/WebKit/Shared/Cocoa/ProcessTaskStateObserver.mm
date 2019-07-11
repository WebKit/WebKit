/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ProcessTaskStateObserver.h"

#if PLATFORM(IOS_FAMILY)

#import "AssertionServicesSPI.h"
#import "Logging.h"
#import <wtf/Function.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssertionServices);
SOFT_LINK_CLASS(AssertionServices, BKSProcess);

typedef void(^TaskStateChangedCallbackType)(BKSProcessTaskState);

@interface WKProcessTaskStateObserverDelegate : NSObject<BKSProcessDelegate>
@property (copy) TaskStateChangedCallbackType taskStateChangedCallback;
@end

@implementation WKProcessTaskStateObserverDelegate
- (void)process:(BKSProcess *)process taskStateDidChange:(BKSProcessTaskState)newState
{
    RELEASE_LOG(ProcessSuspension, "%p -[WKProcessTaskStateObserverDelegate process:taskStateDidChange:], process(%p), newState(%d)", self, process, (int)newState);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.taskStateChangedCallback)
            self.taskStateChangedCallback(newState);
    });
}
@end

namespace WebKit {

static ProcessTaskStateObserver::TaskState toProcessTaskStateObserverTaskState(BKSProcessTaskState state)
{
    static_assert(static_cast<uint32_t>(BKSProcessTaskStateNone) == static_cast<uint32_t>(ProcessTaskStateObserver::None), "BKSProcessTaskState != ProcessTaskStateObserver::TaskState");
    static_assert(static_cast<uint32_t>(BKSProcessTaskStateRunning) == static_cast<uint32_t>(ProcessTaskStateObserver::Running), "BKSProcessTaskState != ProcessTaskStateObserver::TaskState");
    static_assert(static_cast<uint32_t>(BKSProcessTaskStateSuspended) == static_cast<uint32_t>(ProcessTaskStateObserver::Suspended), "BKSProcessTaskState != ProcessTaskStateObserver::TaskState");
    return static_cast<ProcessTaskStateObserver::TaskState>(state);
}

ProcessTaskStateObserver::ProcessTaskStateObserver()
    : m_process([getBKSProcessClass() currentProcess])
    , m_delegate(adoptNS([[WKProcessTaskStateObserverDelegate alloc] init]))
{
    RELEASE_LOG(ProcessSuspension, "%p - ProcessTaskStateObserver::ProcessTaskStateObserver(), m_process(%p)", this, m_process.get());
    m_delegate.get().taskStateChangedCallback = [this] (BKSProcessTaskState state) {
        setTaskState(toProcessTaskStateObserverTaskState(state));
    };
    m_process.get().delegate = m_delegate.get();
}

ProcessTaskStateObserver::ProcessTaskStateObserver(Client& client)
    : ProcessTaskStateObserver()
{
    setClient(client);
}

ProcessTaskStateObserver::~ProcessTaskStateObserver()
{
    m_delegate.get().taskStateChangedCallback = nil;
}

void ProcessTaskStateObserver::setTaskState(TaskState state)
{
    if (m_taskState == state)
        return;

    m_taskState = state;
    if (m_client)
        m_client->processTaskStateDidChange(state);
}

}

#endif // PLATFORM(IOS_FAMILY)

