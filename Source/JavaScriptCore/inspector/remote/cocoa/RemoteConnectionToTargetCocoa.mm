/*
 * Copyright (C) 2013-2019 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteConnectionToTarget.h"

#if ENABLE(REMOTE_INSPECTOR)

#import "JSGlobalObjectDebugger.h"
#import "RemoteAutomationTarget.h"
#import "RemoteInspectionTarget.h"
#import "RemoteInspector.h"
#import <dispatch/dispatch.h>
#import <wtf/RunLoop.h>

#if USE(WEB_THREAD)
#import <wtf/ios/WebCoreThread.h>
#endif

namespace Inspector {

static Lock rwiQueueMutex;
static CFRunLoopSourceRef rwiRunLoopSource;
static RemoteTargetQueue* rwiQueue;

static void RemoteTargetHandleRunSourceGlobal(void*)
{
    ASSERT(CFRunLoopGetCurrent() == CFRunLoopGetMain());
    ASSERT(rwiRunLoopSource);
    ASSERT(rwiQueue);

    RemoteTargetQueue queueCopy;
    {
        Locker locker { rwiQueueMutex };
        std::swap(queueCopy, *rwiQueue);
    }

    for (const auto& function : queueCopy)
        function();
}

static void RemoteTargetQueueTaskOnGlobalQueue(Function<void ()>&& function)
{
    ASSERT(rwiRunLoopSource);
    ASSERT(rwiQueue);

    {
        Locker locker { rwiQueueMutex };
        rwiQueue->append(WTFMove(function));
    }

    CFRunLoopSourceSignal(rwiRunLoopSource);
    CFRunLoopWakeUp(CFRunLoopGetMain());
}

static void RemoteTargetInitializeGlobalQueue()
{
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        rwiQueue = new RemoteTargetQueue;

        CFRunLoopSourceContext runLoopSourceContext = { 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, RemoteTargetHandleRunSourceGlobal };
        rwiRunLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &runLoopSourceContext);

        // Add to the default run loop mode for default handling, and the JSContext remote inspector run loop mode when paused.
        CFRunLoopAddSource(CFRunLoopGetMain(), rwiRunLoopSource, kCFRunLoopDefaultMode);
        auto mode = JSGlobalObjectDebugger::runLoopMode();
        if (mode != DefaultRunLoopMode)
            CFRunLoopAddSource(CFRunLoopGetMain(), rwiRunLoopSource, mode);
    });
}

static void RemoteTargetHandleRunSourceWithInfo(void* info)
{
    RemoteConnectionToTarget *connectionToTarget = static_cast<RemoteConnectionToTarget*>(info);

    RemoteTargetQueue queueCopy;
    {
        Locker locker { connectionToTarget->queueMutex() };
        queueCopy = connectionToTarget->takeQueue();
    }

    for (const auto& function : queueCopy)
        function();
}


RemoteConnectionToTarget::RemoteConnectionToTarget(RemoteControllableTarget* target, NSString *connectionIdentifier, NSString *destination)
    : m_target(target)
    , m_connectionIdentifier(connectionIdentifier)
    , m_destination(destination)
{
    setupRunLoop();
}

RemoteConnectionToTarget::~RemoteConnectionToTarget()
{
    teardownRunLoop();
}

std::optional<TargetID> RemoteConnectionToTarget::targetIdentifier() const
{
    return m_target ? std::optional<TargetID>(m_target->targetIdentifier()) : std::nullopt;
}

NSString *RemoteConnectionToTarget::connectionIdentifier() const
{
    return adoptNS([m_connectionIdentifier copy]).autorelease();
}

NSString *RemoteConnectionToTarget::destination() const
{
    return adoptNS([m_destination copy]).autorelease();
}

void RemoteConnectionToTarget::dispatchAsyncOnTarget(Function<void ()>&& callback)
{
    if (m_runLoop) {
        queueTaskOnPrivateRunLoop(WTFMove(callback));
        return;
    }

#if USE(WEB_THREAD)
    if (WebCoreWebThreadIsEnabled && WebCoreWebThreadIsEnabled()) {
        __block auto blockCallback(WTFMove(callback));
        WebCoreWebThreadRun(^{
            blockCallback();
        });
        return;
    }
#endif

    RemoteTargetQueueTaskOnGlobalQueue(WTFMove(callback));
}

bool RemoteConnectionToTarget::setup(bool isAutomaticInspection, bool automaticallyPause)
{
    Locker locker { m_targetMutex };

    if (!m_target)
        return false;

    auto targetIdentifier = this->targetIdentifier().value_or(0);

    dispatchAsyncOnTarget([this, targetIdentifier, isAutomaticInspection, automaticallyPause, strongThis = Ref { *this }]() {
        Locker locker { m_targetMutex };

        if (!m_target || !m_target->remoteControlAllowed()) {
            RemoteInspector::singleton().setupFailed(targetIdentifier);
            m_target = nullptr;
        } else if (is<RemoteInspectionTarget>(m_target)) {
            auto castedTarget = downcast<RemoteInspectionTarget>(m_target);
            castedTarget->connect(*this, isAutomaticInspection, automaticallyPause);
            m_connected = true;

            RemoteInspector::singleton().updateTargetListing(targetIdentifier);
        } else if (is<RemoteAutomationTarget>(m_target)) {
            auto castedTarget = downcast<RemoteAutomationTarget>(m_target);
            castedTarget->connect(*this);
            m_connected = true;

            RemoteInspector::singleton().updateTargetListing(targetIdentifier);
        }
    });

    return true;
}

void RemoteConnectionToTarget::targetClosed()
{
    Locker locker { m_targetMutex };

    m_target = nullptr;
}

void RemoteConnectionToTarget::close()
{
    auto targetIdentifier = m_target ? m_target->targetIdentifier() : 0;

    if (auto* automationTarget = dynamicDowncast<RemoteAutomationTarget>(m_target))
        automationTarget->setIsPendingTermination();
    
    dispatchAsyncOnTarget([this, targetIdentifier, strongThis = Ref { *this }]() {
        Locker locker { m_targetMutex };
        if (m_target) {
            if (m_connected)
                m_target->disconnect(*this);

            m_target = nullptr;
            if (targetIdentifier)
                RemoteInspector::singleton().updateTargetListing(targetIdentifier);
        }
    });
}

void RemoteConnectionToTarget::sendMessageToTarget(NSString *message)
{
    dispatchAsyncOnTarget([this, strongMessage = retainPtr(message), strongThis = Ref { *this }]() {
        RemoteControllableTarget* target = nullptr;
        {
            Locker locker { m_targetMutex };
            if (!m_target)
                return;
            target = m_target;
        }

        target->dispatchMessageFromRemote(strongMessage.get());
    });
}

void RemoteConnectionToTarget::sendMessageToFrontend(const String& message)
{
    if (!targetIdentifier())
        return;

    RemoteInspector::singleton().sendMessageToRemote(targetIdentifier().value(), message);
}

void RemoteConnectionToTarget::setupRunLoop()
{
    CFRunLoopRef targetRunLoop = m_target->targetRunLoop();
    if (!targetRunLoop) {
        RemoteTargetInitializeGlobalQueue();
        return;
    }

    m_runLoop = targetRunLoop;

    CFRunLoopSourceContext runLoopSourceContext = { 0, this, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, RemoteTargetHandleRunSourceWithInfo };
    m_runLoopSource = adoptCF(CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &runLoopSourceContext));

    CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopDefaultMode);
    auto mode = JSGlobalObjectDebugger::runLoopMode();
    if (mode != DefaultRunLoopMode)
        CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), mode);
}

void RemoteConnectionToTarget::teardownRunLoop()
{
    if (!m_runLoop)
        return;

    CFRunLoopRemoveSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopDefaultMode);
    auto mode = JSGlobalObjectDebugger::runLoopMode();
    if (mode != DefaultRunLoopMode)
        CFRunLoopRemoveSource(m_runLoop.get(), m_runLoopSource.get(), mode);

    m_runLoop = nullptr;
    m_runLoopSource = nullptr;
}

void RemoteConnectionToTarget::queueTaskOnPrivateRunLoop(Function<void ()>&& function)
{
    ASSERT(m_runLoop);

    {
        Locker lock { m_queueMutex };
        m_queue.append(WTFMove(function));
    }

    CFRunLoopSourceSignal(m_runLoopSource.get());
    CFRunLoopWakeUp(m_runLoop.get());
}

RemoteTargetQueue RemoteConnectionToTarget::takeQueue()
{
    return std::exchange(m_queue, { });
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
