/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#import "ProcessAssertion.h"

#if USE(RUNNINGBOARD)

#import "AssertionCapability.h"
#import "ExtensionKitSPI.h"
#import "Logging.h"
#import "ProcessStateMonitor.h"
#import "RunningBoardServicesSPI.h"
#import "WebProcessPool.h"
#import <wtf/HashMap.h>
#import <wtf/RunLoop.h>
#import <wtf/SoftLinking.h>
#import <wtf/ThreadSafeWeakHashSet.h>
#import <wtf/Vector.h>
#import <wtf/WeakHashSet.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WorkQueue.h>

#if USE(EXTENSIONKIT)
#import <BrowserEngineKit/BENetworkingProcess.h>
#import <BrowserEngineKit/BERenderingProcess.h>
#import <BrowserEngineKit/BEWebContentProcess.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIApplication.h>

using WebKit::ProcessAndUIAssertion;
#endif

static WorkQueue& assertionsWorkQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> workQueue(WorkQueue::create("ProcessAssertion Queue"_s, WorkQueue::QOS::UserInitiated));
    return workQueue.get();
}

// This gives some time to our child processes to process the ProcessWillSuspendImminently IPC but makes sure we release
// the background task before the UIKit timeout (We get killed if we do not release the background task within 5 seconds
// on the expiration handler getting called).
#if PLATFORM(IOS_FAMILY)
static const Seconds releaseBackgroundTaskAfterExpirationDelay { 2_s };

static bool processHasActiveRunTimeLimitation()
{
    return [RBSProcessHandle currentProcess].activeLimitations.runTime != RBSProcessTimeLimitationNone;
}

@interface WKProcessAssertionBackgroundTaskManager
    : NSObject <RBSAssertionObserving>

+ (WKProcessAssertionBackgroundTaskManager *)shared;

- (void)addAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;
- (void)removeAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;
- (void)setProcessStateMonitorEnabled:(BOOL)enabled;

@end

@implementation WKProcessAssertionBackgroundTaskManager
{
    RetainPtr<RBSAssertion> _backgroundTask;
    std::atomic<bool> _backgroundTaskWasInvalidated;
    ThreadSafeWeakHashSet<ProcessAndUIAssertion> _assertionsNeedingBackgroundTask;
    dispatch_block_t _pendingTaskReleaseTask;
    RefPtr<WebKit::ProcessStateMonitor> m_processStateMonitor;
}

+ (WKProcessAssertionBackgroundTaskManager *)shared
{
    static WKProcessAssertionBackgroundTaskManager *shared = [WKProcessAssertionBackgroundTaskManager new];
    return shared;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    // FIXME: Stop relying on UIApplication notifications as this does not work as expected for daemons or ViewServices.
    // We should likely use ProcessTaskStateObserver to monitor suspension state.
    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:^(NSNotification *) {
        [self _cancelPendingReleaseTask];
        [self _updateBackgroundTask];
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:^(NSNotification *) {
        if (![self _hasBackgroundTask])
            WebKit::WebProcessPool::notifyProcessPoolsApplicationIsAboutToSuspend();
    }];

    return self;
}

- (void)dealloc
{
    [self _releaseBackgroundTask];
    [super dealloc];
}

- (void)addAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion
{
    _assertionsNeedingBackgroundTask.add(assertion);
    [self _updateBackgroundTask];
}

- (void)removeAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion
{
    _assertionsNeedingBackgroundTask.remove(assertion);
}

- (void)_notifyAssertionsOfImminentSuspension
{
    ASSERT(RunLoop::isMain());

    // Note that we don't expect clients to register new assertions when getting notified that the UI assertion will expire imminently.
    // If clients were to do so, then those new assertions would not get notified of the imminent suspension.
    _assertionsNeedingBackgroundTask.forEach([] (auto& assertion) {
        assertion.uiAssertionWillExpireImminently();
    });
}


- (void)_scheduleReleaseTask
{
    ASSERT(!_pendingTaskReleaseTask);
    if (_pendingTaskReleaseTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: _scheduleReleaseTask because the expiration handler has been called", self);
    WorkQueue::main().dispatchAfter(releaseBackgroundTaskAfterExpirationDelay, [self, retainedSelf = retainPtr(self)] {
        _pendingTaskReleaseTask = nil;
        [self _releaseBackgroundTask];
    });
}

- (void)_cancelPendingReleaseTask
{
    if (!_pendingTaskReleaseTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: _cancelPendingReleaseTask because the application is foreground again", self);
    dispatch_block_cancel(_pendingTaskReleaseTask);
    _pendingTaskReleaseTask = nil;
}

- (BOOL)_hasBackgroundTask
{
    return !!_backgroundTask;
}

- (void)_updateBackgroundTask
{
    if (!_assertionsNeedingBackgroundTask.isEmptyIgnoringNullReferences() && (![self _hasBackgroundTask] || _backgroundTaskWasInvalidated)) {
        if (processHasActiveRunTimeLimitation()) {
            RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: Ignored request to start a new background task because RunningBoard has already started the expiration timer", self);
            return;
        }
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: beginBackgroundTaskWithName", self);
        RBSTarget *target = [RBSTarget currentProcess];
        RBSDomainAttribute *domainAttribute = [RBSDomainAttribute attributeWithDomain:@"com.apple.common" name:@"FinishTaskInterruptable"];
        _backgroundTask = adoptNS([[RBSAssertion alloc] initWithExplanation:@"WebKit UIProcess background task" target:target attributes:@[domainAttribute]]);
        [_backgroundTask addObserver:self];

        _backgroundTaskWasInvalidated = false;
        [_backgroundTask acquireWithInvalidationHandler:nil];
        RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: Took a FinishTaskInterruptable assertion for own process");
    } else if (_assertionsNeedingBackgroundTask.isEmptyIgnoringNullReferences()) {
        // Release the background task asynchronously because releasing the background task may destroy the ProcessThrottler and we don't
        // want it to get destroyed while in the middle of updating its assertion.
        RunLoop::main().dispatch([self, strongSelf = retainPtr(self)] {
            if (_assertionsNeedingBackgroundTask.isEmptyIgnoringNullReferences())
                [self _releaseBackgroundTask];
        });
    }
}

- (void)assertionWillInvalidate:(RBSAssertion *)assertion
{
    ASSERT(assertion == _backgroundTask.get());
    [self _handleBackgroundTaskExpiration];
}

- (void)assertion:(RBSAssertion *)assertion didInvalidateWithError:(NSError *)error
{
    ASSERT(assertion == _backgroundTask.get());
    RELEASE_LOG_ERROR(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: FinishTaskInterruptable assertion was invalidated, error: %{public}@", error);
    _backgroundTaskWasInvalidated = true;
}

- (void)_handleBackgroundTaskExpiration
{
    auto remainingTime = [RBSProcessHandle currentProcess].activeLimitations.runTime;
    RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: Background task expired while holding WebKit ProcessAssertion (isMainThread=%d, remainingTime=%g).", RunLoop::isMain(), remainingTime);

    callOnMainRunLoopAndWait([self] {
        [self _handleBackgroundTaskExpirationOnMainThread];
    });
}

- (void)_handleBackgroundTaskExpirationOnMainThread
{
    ASSERT(RunLoop::isMain());

    auto remainingTime = [RBSProcessHandle currentProcess].activeLimitations.runTime;
    RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: _handleBackgroundTaskExpirationOnMainThread (remainingTime=%g).", remainingTime);

    // If there is no time limitation, then it means that the process is now allowed to run again and the expiration notification
    // is outdated (e.g. we did not have time to process the expiration notification before suspending and thus only process it
    // upon resuming, or the user reactivated the app shortly after expiration).
    if (remainingTime == RBSProcessTimeLimitationNone) {
        [self _releaseBackgroundTask];
        RunLoop::main().dispatch([self, strongSelf = retainPtr(self)] {
            [self _updateBackgroundTask];
        });
        return;
    }

    [self _notifyAssertionsOfImminentSuspension];
    [self _scheduleReleaseTask];
}

- (void)_releaseBackgroundTask
{
    if (![self _hasBackgroundTask])
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: endBackgroundTask", self);
    if (processHasActiveRunTimeLimitation()) {
#if PLATFORM(IOS_FAMILY)
        WebKit::WebProcessPool::notifyProcessPoolsApplicationIsAboutToSuspend();
#endif
        if (RefPtr processStateMonitor = m_processStateMonitor)
            processStateMonitor->processWillBeSuspendedImmediately();
    }

    [_backgroundTask removeObserver:self];
    [_backgroundTask invalidate];
    _backgroundTask = nullptr;
}

- (void)setProcessStateMonitorEnabled:(BOOL)enabled
{
    if (!enabled) {
        m_processStateMonitor = nullptr;
        return;
    }

    if (!m_processStateMonitor) {
        m_processStateMonitor = WebKit::ProcessStateMonitor::create([](bool suspended) {
            for (auto& processPool : WebKit::WebProcessPool::allProcessPools())
                processPool->setProcessesShouldSuspend(suspended);
        });
    }
}

@end
#endif // PLATFORM(IOS_FAMILY)

typedef void(^RBSAssertionInvalidationCallbackType)();

@interface WKRBSAssertionDelegate : NSObject<RBSAssertionObserving>
@property (copy) RBSAssertionInvalidationCallbackType prepareForInvalidationCallback;
@property (copy) RBSAssertionInvalidationCallbackType invalidationCallback;
@end

@implementation WKRBSAssertionDelegate
- (void)dealloc
{
    [_prepareForInvalidationCallback release];
    [_invalidationCallback release];
    [super dealloc];
}

- (void)assertionWillInvalidate:(RBSAssertion *)assertion
{
    RELEASE_LOG(ProcessSuspension, "%p - WKRBSAssertionDelegate: assertionWillInvalidate", self);

    RunLoop::main().dispatch([weakSelf = WeakObjCPtr<WKRBSAssertionDelegate>(self)] {
        auto strongSelf = weakSelf.get();
        if (strongSelf && strongSelf.get().prepareForInvalidationCallback)
            strongSelf.get().prepareForInvalidationCallback();
    });
}

- (void)assertion:(RBSAssertion *)assertion didInvalidateWithError:(NSError *)error
{
    RELEASE_LOG(ProcessSuspension, "%p - WKRBSAssertionDelegate: assertion was invalidated, error: %{public}@", self, error);

    RunLoop::main().dispatch([weakSelf = WeakObjCPtr<WKRBSAssertionDelegate>(self)] {
        auto strongSelf = weakSelf.get();
        if (strongSelf && strongSelf.get().invalidationCallback)
            strongSelf.get().invalidationCallback();
    });
}
@end

namespace WebKit {

static ASCIILiteral runningBoardNameForAssertionType(ProcessAssertionType assertionType)
{
    switch (assertionType) {
    case ProcessAssertionType::NearSuspended:
        return "Suspended"_s; // FIXME: This name is confusing since it doesn't cause suspension.
    case ProcessAssertionType::Background:
#if PLATFORM(MAC)
        // The background assertions time out after 30 seconds on iOS but not macOS.
        return "IndefiniteBackground"_s;
#else
        return "Background"_s;
#endif
    case ProcessAssertionType::UnboundedNetworking:
        return "UnboundedNetworking"_s;
    case ProcessAssertionType::Foreground:
        return "Foreground"_s;
    case ProcessAssertionType::MediaPlayback:
        return "MediaPlayback"_s;
    case ProcessAssertionType::FinishTaskCanSleep:
        return "FinishTaskCanSleep"_s;
    case ProcessAssertionType::FinishTaskInterruptable:
        return "FinishTaskInterruptable"_s;
    case ProcessAssertionType::BoostedJetsam:
        return "BoostedJetsam"_s;
    }
}

static ASCIILiteral runningBoardDomainForAssertionType(ProcessAssertionType assertionType)
{
    switch (assertionType) {
    case ProcessAssertionType::NearSuspended:
    case ProcessAssertionType::Background:
    case ProcessAssertionType::UnboundedNetworking:
    case ProcessAssertionType::Foreground:
    case ProcessAssertionType::MediaPlayback:
    case ProcessAssertionType::BoostedJetsam:
        return "com.apple.webkit"_s;
    case ProcessAssertionType::FinishTaskCanSleep:
    case ProcessAssertionType::FinishTaskInterruptable:
        return "com.apple.common"_s;
    }
}

#if USE(EXTENSIONKIT)
Lock ProcessAssertion::s_capabilityLock;
#endif

ProcessAssertion::ProcessAssertion(pid_t pid, const String& reason, ProcessAssertionType assertionType, const String& environmentIdentifier)
    : m_assertionType(assertionType)
    , m_pid(pid)
    , m_reason(reason)
{
    init(environmentIdentifier);
}

ProcessAssertion::ProcessAssertion(AuxiliaryProcessProxy& process, const String& reason, ProcessAssertionType assertionType)
    : m_assertionType(assertionType)
    , m_pid(process.processID())
    , m_reason(reason)
{
#if USE(EXTENSIONKIT)
    if (process.extensionProcess()) {
        ASCIILiteral runningBoardAssertionName = runningBoardNameForAssertionType(m_assertionType);
        ASCIILiteral runningBoardDomain = runningBoardDomainForAssertionType(m_assertionType);
        auto didInvalidateBlock = [weakThis = ThreadSafeWeakPtr { *this }, runningBoardAssertionName] () {
            RunLoop::main().dispatch([weakThis = WTFMove(weakThis), runningBoardAssertionName = WTFMove(runningBoardAssertionName)] {
                auto strongThis = weakThis.get();
                RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion: RBS %{public}s assertion for process with PID=%d was invalidated", strongThis.get(), runningBoardAssertionName.characters(), strongThis ? strongThis->m_pid : 0);
                if (strongThis)
                    strongThis->processAssertionWasInvalidated();
            });
        };
        auto willInvalidateBlock = [weakThis = ThreadSafeWeakPtr { *this }, runningBoardAssertionName] () {
            RunLoop::main().dispatch([weakThis = WTFMove(weakThis), runningBoardAssertionName = WTFMove(runningBoardAssertionName)] {
                auto strongThis = weakThis.get();
                RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() RBS %{public}s assertion for process with PID=%d will be invalidated", strongThis.get(), runningBoardAssertionName.characters(), strongThis ? strongThis->m_pid : 0);
                if (strongThis)
                    strongThis->processAssertionWillBeInvalidated();
            });
        };
        m_capability = AssertionCapability { process.environmentIdentifier(), runningBoardDomain, runningBoardAssertionName, WTFMove(willInvalidateBlock), WTFMove(didInvalidateBlock) };
        m_process = process.extensionProcess();
        if (m_capability && m_capability->hasPlatformCapability())
            return;
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() Failed to create capability %s", this, runningBoardAssertionName.characters());
    }
#endif
    init(process.environmentIdentifier());
}

void ProcessAssertion::init(const String& environmentIdentifier)
{
    ASCIILiteral runningBoardAssertionName = runningBoardNameForAssertionType(m_assertionType);
    ASSERT(!runningBoardAssertionName.isEmpty());
    if (m_pid <= 0) {
        RELEASE_LOG_ERROR(ProcessSuspension, "%p - ProcessAssertion: Failed to acquire RBS %{public}s assertion '%{public}s' for process because PID %d is invalid", this, runningBoardAssertionName.characters(), m_reason.utf8().data(), m_pid);
        m_wasInvalidated = true;
        return;
    }

    RBSTarget *target = nil;
    if (environmentIdentifier.isEmpty())
        target = [RBSTarget targetWithPid:m_pid];
    else
        target = [RBSTarget targetWithPid:m_pid environmentIdentifier:environmentIdentifier];

    RBSDomainAttribute *domainAttribute = [RBSDomainAttribute attributeWithDomain:String(runningBoardDomainForAssertionType(m_assertionType)) name:String(runningBoardAssertionName)];
    m_rbsAssertion = adoptNS([[RBSAssertion alloc] initWithExplanation:m_reason target:target attributes:@[domainAttribute]]);

    m_delegate = adoptNS([[WKRBSAssertionDelegate alloc] init]);
    [m_rbsAssertion addObserver:m_delegate.get()];
    m_delegate.get().invalidationCallback = ^{
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion: RBS %{public}s assertion for process with PID=%d was invalidated", this, runningBoardAssertionName.characters(), m_pid);
        processAssertionWasInvalidated();
    };
    m_delegate.get().prepareForInvalidationCallback = ^{
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() RBS %{public}s assertion for process with PID=%d will be invalidated", this, runningBoardAssertionName.characters(), m_pid);
        processAssertionWillBeInvalidated();
    };
}

double ProcessAssertion::remainingRunTimeInSeconds(ProcessID pid)
{
    RBSProcessIdentifier *processIdentifier = [RBSProcessIdentifier identifierWithPid:pid];
    if (!processIdentifier) {
        RELEASE_LOG_ERROR(ProcessSuspension, "ProcessAssertion::remainingRunTimeInSeconds failed to get identifier for process with PID=%d", pid);
        return 0;
    }

    RBSProcessHandle *processHandle = [RBSProcessHandle handleForIdentifier:processIdentifier error:nil];
    if (!processHandle) {
        RELEASE_LOG_ERROR(ProcessSuspension, "ProcessAssertion::remainingRunTimeInSeconds failed to get handle for process with PID=%d", pid);
        return 0;
    }

    return processHandle.activeLimitations.runTime;
}

void ProcessAssertion::acquireAsync(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());
    assertionsWorkQueue().dispatch([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        protectedThis->acquireSync();
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            if (completionHandler)
                completionHandler();
        });
    });
}

void ProcessAssertion::acquireSync()
{
    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::acquireSync Trying to take RBS assertion '%{public}s' for process with PID=%d", this, m_reason.utf8().data(), m_pid);
#if USE(EXTENSIONKIT)
    if (m_process && m_capability && m_capability->hasPlatformCapability()) {
        Locker locker { s_capabilityLock };
        auto grant = m_process->grantCapability(m_capability->platformCapability(), m_capability->didInvalidateBlock());
        m_grant.setPlatformGrant(WTFMove(grant));
        if (m_grant.isValid()) {
            RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() Successfully granted capability", this);
            return;
        }
        ASCIILiteral runningBoardAssertionName = runningBoardNameForAssertionType(m_assertionType);
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() Failed to grant capability %s", this, runningBoardAssertionName.characters());
    }
#endif
    NSError *acquisitionError = nil;
    if (![m_rbsAssertion acquireWithError:&acquisitionError]) {
        RELEASE_LOG_ERROR(ProcessSuspension, "%p - ProcessAssertion::acquireSync Failed to acquire RBS assertion '%{public}s' for process with PID=%d, error: %{public}@", this, m_reason.utf8().data(), m_pid, acquisitionError);
        RunLoop::main().dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
            if (auto protectedThis = weakThis.get())
                protectedThis->processAssertionWasInvalidated();
        });
    } else
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::acquireSync Successfully took RBS assertion '%{public}s' for process with PID=%d", this, m_reason.utf8().data(), m_pid);
}

ProcessAssertion::~ProcessAssertion()
{
    RELEASE_LOG(ProcessSuspension, "%p - ~ProcessAssertion: Releasing process assertion '%{public}s' for process with PID=%d", this, m_reason.utf8().data(), m_pid);

    if (m_rbsAssertion) {
        m_delegate.get().invalidationCallback = nil;
        m_delegate.get().prepareForInvalidationCallback = nil;
        [m_rbsAssertion removeObserver:m_delegate.get()];
        m_delegate = nil;
        [m_rbsAssertion invalidate];
    }

#if USE(EXTENSIONKIT)
    Locker locker { s_capabilityLock };
    m_grant.invalidate();
#endif
}

void ProcessAssertion::processAssertionWillBeInvalidated()
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::processAssertionWillBeInvalidated() PID=%d", this, m_pid);

    if (m_prepareForInvalidationHandler)
        m_prepareForInvalidationHandler();
}

void ProcessAssertion::processAssertionWasInvalidated()
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::processAssertionWasInvalidated() PID=%d", this, m_pid);

    m_wasInvalidated = true;
    if (m_invalidationHandler)
        m_invalidationHandler();
}

bool ProcessAssertion::isValid() const
{
    return !m_wasInvalidated;
}

ProcessAndUIAssertion::ProcessAndUIAssertion(AuxiliaryProcessProxy& process, const String& reason, ProcessAssertionType assertionType)
    : ProcessAssertion(process, reason, assertionType)
{
#if PLATFORM(IOS_FAMILY)
    updateRunInBackgroundCount();
#endif
}

ProcessAndUIAssertion::~ProcessAndUIAssertion()
{
#if PLATFORM(IOS_FAMILY)
    if (m_isHoldingBackgroundTask)
        [[WKProcessAssertionBackgroundTaskManager shared] _updateBackgroundTask];
#endif
}

#if PLATFORM(IOS_FAMILY)
void ProcessAndUIAssertion::updateRunInBackgroundCount()
{
    bool shouldHoldBackgroundTask = isValid() && type() != ProcessAssertionType::NearSuspended;
    if (m_isHoldingBackgroundTask == shouldHoldBackgroundTask)
        return;

    if (shouldHoldBackgroundTask)
        [[WKProcessAssertionBackgroundTaskManager shared] addAssertionNeedingBackgroundTask:*this];
    else {
        [[WKProcessAssertionBackgroundTaskManager shared] removeAssertionNeedingBackgroundTask:*this];
        [[WKProcessAssertionBackgroundTaskManager shared] _updateBackgroundTask];
    }

    m_isHoldingBackgroundTask = shouldHoldBackgroundTask;
}

void ProcessAndUIAssertion::setProcessStateMonitorEnabled(bool enabled)
{
    [[WKProcessAssertionBackgroundTaskManager shared] setProcessStateMonitorEnabled:enabled];
}

void ProcessAndUIAssertion::uiAssertionWillExpireImminently()
{
    if (m_uiAssertionExpirationHandler)
        m_uiAssertionExpirationHandler();
}

void ProcessAndUIAssertion::processAssertionWasInvalidated()
{
    ASSERT(RunLoop::isMain());

    ThreadSafeWeakPtr weakThis { *this };
    ProcessAssertion::processAssertionWasInvalidated();

    // Calling ProcessAssertion::processAssertionWasInvalidated() may have destroyed |this|.
    if (auto protectedThis = weakThis.get())
        updateRunInBackgroundCount();
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace WebKit

#endif // USE(RUNNINGBOARD)
