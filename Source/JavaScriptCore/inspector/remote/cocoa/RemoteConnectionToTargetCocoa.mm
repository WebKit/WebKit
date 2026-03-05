/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>

#if USE(WEB_THREAD)
#import <wtf/ios/WebCoreThread.h>
#endif

namespace Inspector {

static Lock rwiQueueMutex;
static RemoteTargetQueue* rwiQueue;

static RetainPtr<CFRunLoopSourceRef>& rwiRunLoopSource()
{
    static NeverDestroyed<RetainPtr<CFRunLoopSourceRef>> source;
    return source.get();
}

static void RemoteTargetHandleRunSourceGlobal(void*)
{
    ASSERT(CFRunLoopGetCurrent() == CFRunLoopGetMain());
    ASSERT(rwiRunLoopSource());
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
    ASSERT(rwiRunLoopSource());
    ASSERT(rwiQueue);

    {
        Locker locker { rwiQueueMutex };
        rwiQueue->append(WTF::move(function));
    }

    CFRunLoopSourceSignal(rwiRunLoopSource().get());
    CFRunLoopWakeUp(RetainPtr { CFRunLoopGetMain() }.get());
}

static void RemoteTargetInitializeGlobalQueue()
{
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        rwiQueue = new RemoteTargetQueue;

        CFRunLoopSourceContext runLoopSourceContext = { 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, RemoteTargetHandleRunSourceGlobal };
        rwiRunLoopSource() = adoptCF(CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &runLoopSourceContext));

        // Add to the default run loop mode for default handling, and the JSContext remote inspector run loop mode when paused.
        RetainPtr mainRunLoop = CFRunLoopGetMain();
        CFRunLoopAddSource(mainRunLoop.get(), rwiRunLoopSource().get(), kCFRunLoopDefaultMode);
        RetainPtr mode = JSGlobalObjectDebugger::runLoopModeSingleton();
        if (mode.get() != DefaultRunLoopMode)
            CFRunLoopAddSource(mainRunLoop.get(), rwiRunLoopSource().get(), mode.get());
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
    RefPtr target = m_target.get();
    return target ? std::optional<TargetID>(target->targetIdentifier()) : std::nullopt;
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
        queueTaskOnPrivateRunLoop(WTF::move(callback));
        return;
    }

#if USE(WEB_THREAD)
    if (WebCoreWebThreadIsEnabled && WebCoreWebThreadIsEnabled()) {
        __block auto blockCallback(WTF::move(callback));
        WebCoreWebThreadRun(^{
            blockCallback();
        });
        return;
    }
#endif

    RemoteTargetQueueTaskOnGlobalQueue(WTF::move(callback));
}

bool RemoteConnectionToTarget::setup(bool isAutomaticInspection, bool automaticallyPause)
{
    Locker locker { m_targetMutex };

    RefPtr target = m_target.get();
    if (!target)
        return false;

    auto targetIdentifier = this->targetIdentifier().value_or(0);

    dispatchAsyncOnTarget([this, targetIdentifier, isAutomaticInspection, automaticallyPause, protectedThis = Ref { *this }]() {
        RefPtr<RemoteControllableTarget> target;
        {
            Locker locker { m_targetMutex };
            target = m_target.get();
        }

        if (!target || !target->remoteControlAllowed()) {
            RemoteInspector::singleton().setupFailed(targetIdentifier);
            Locker locker { m_targetMutex };
            m_target = nullptr;
            return;
        }

        if (RefPtr inspectionTarget = dynamicDowncast<RemoteInspectionTarget>(target.get()))
            inspectionTarget->connect(*this, isAutomaticInspection, automaticallyPause);
        else if (RefPtr automationTarget = dynamicDowncast<RemoteAutomationTarget>(target.get()))
            automationTarget->connect(*this);

        m_connectionState = ConnectionState::Connected;
        RemoteInspector::singleton().updateTargetListing(targetIdentifier);
        RemoteInspector::singleton().setupCompleted(targetIdentifier);
    });

    return true;
}

void RemoteConnectionToTarget::targetClosed()
{
    Locker locker { m_targetMutex };
    m_connectionState = ConnectionState::Closed;
    m_target = nullptr;
}

void RemoteConnectionToTarget::close()
{
    Locker locker { m_targetMutex };
    RefPtr target = m_target.get();
    auto targetIdentifier = target ? target->targetIdentifier() : 0;

    if (auto* automationTarget = dynamicDowncast<RemoteAutomationTarget>(target.get()))
        automationTarget->setIsPendingTermination();
    
    dispatchAsyncOnTarget([this, targetIdentifier, protectedThis = Ref { *this }]() {
        Locker locker { m_targetMutex };
        if (RefPtr target = m_target.get()) {
            if (m_connectionState.exchange(ConnectionState::Closed) == ConnectionState::Connected)
                target->disconnect(*this);

            m_target = nullptr;
        }

        if (targetIdentifier)
            RemoteInspector::singleton().updateTargetListing(targetIdentifier);
    });
}

void RemoteConnectionToTarget::sendMessageToTarget(NSString *message)
{
    if (m_connectionState == ConnectionState::Closed)
        return;

    dispatchAsyncOnTarget([this, strongMessage = retainPtr(message), protectedThis = Ref { *this }]() {
        RefPtr<RemoteControllableTarget> target;
        {
            Locker locker { m_targetMutex };
            target = m_target.get();
        }
        if (target)
            target->dispatchMessageFromRemote(strongMessage.get());
    });
}

void RemoteConnectionToTarget::sendMessageToFrontend(const String& message)
{
    if (m_connectionState == ConnectionState::Closed)
        return;

    std::optional<TargetID> targetIdentifier;
    {
        Locker locker { m_targetMutex };
        targetIdentifier = this->targetIdentifier();
    }
    if (!targetIdentifier)
        return;

    RemoteInspector::singleton().sendMessageToRemote(targetIdentifier.value(), message);
}

void RemoteConnectionToTarget::setupRunLoop()
{
    RetainPtr<CFRunLoopRef> targetRunLoop;
    {
        Locker locker { m_targetMutex };
        RefPtr target = m_target.get();
        targetRunLoop = target->targetRunLoop();
    }
    if (!targetRunLoop) {
        RemoteTargetInitializeGlobalQueue();
        return;
    }

    m_runLoop = targetRunLoop;

    CFRunLoopSourceContext runLoopSourceContext = { 0, this, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, RemoteTargetHandleRunSourceWithInfo };
    m_runLoopSource = adoptCF(CFRunLoopSourceCreate(kCFAllocatorDefault, 1, &runLoopSourceContext));

    CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopDefaultMode);
    RetainPtr mode = JSGlobalObjectDebugger::runLoopModeSingleton();
    if (mode.get() != DefaultRunLoopMode)
        CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), mode.get());
}

void RemoteConnectionToTarget::teardownRunLoop()
{
    if (!m_runLoop)
        return;

    CFRunLoopRemoveSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopDefaultMode);
    RetainPtr mode = JSGlobalObjectDebugger::runLoopModeSingleton();
    if (mode.get() != DefaultRunLoopMode)
        CFRunLoopRemoveSource(m_runLoop.get(), m_runLoopSource.get(), mode.get());

    m_runLoop = nullptr;
    m_runLoopSource = nullptr;
}

void RemoteConnectionToTarget::queueTaskOnPrivateRunLoop(Function<void ()>&& function)
{
    ASSERT(m_runLoop);

    {
        Locker lock { m_queueMutex };
        m_queue.append(WTF::move(function));
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
