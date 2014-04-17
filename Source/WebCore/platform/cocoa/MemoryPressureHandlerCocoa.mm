/*
 * Copyright (C) 2011-2014 Apple Inc. All Rights Reserved.
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
#import "MemoryPressureHandler.h"

#import "IOSurfacePool.h"
#import "GCController.h"
#import "LayerPool.h"
#import "Logging.h"
#import "WebCoreSystemInterface.h"
#import <mach/mach.h>
#import <mach/task_info.h>
#import <malloc/malloc.h>
#import <notify.h>
#import <wtf/CurrentTime.h>

#if PLATFORM(IOS)
#import "SystemMemory.h"
#import "WebCoreThread.h"
#import <dispatch/private.h>
#endif

namespace WebCore {

void MemoryPressureHandler::platformReleaseMemory(bool)
{
#if PLATFORM(MAC)
    {
        ReliefLogger log("Drain LayerPool");
        LayerPool::sharedPool()->drain();
    }
#endif
#if USE(IOSURFACE)
    {
        ReliefLogger log("Drain IOSurfacePool");
        IOSurfacePool::sharedPool().discardAllSurfaces();
    }
#endif
}

static dispatch_source_t _cache_event_source = 0;
static dispatch_source_t _timer_event_source = 0;
static int _notifyToken;

// Disable memory event reception for a minimum of s_minimumHoldOffTime
// seconds after receiving an event.  Don't let events fire any sooner than
// s_holdOffMultiplier times the last cleanup processing time.  Effectively 
// this is 1 / s_holdOffMultiplier percent of the time.
// These value seems reasonable and testing verifies that it throttles frequent
// low memory events, greatly reducing CPU usage.
static const unsigned s_minimumHoldOffTime = 5;
static const unsigned s_holdOffMultiplier = 20;

void MemoryPressureHandler::install()
{
    if (m_installed || _timer_event_source)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 80000
        _cache_event_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYSTATUS, 0, DISPATCH_MEMORYSTATUS_PRESSURE_WARN, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
#elif PLATFORM(MAC) && MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
        _cache_event_source = wkCreateMemoryStatusPressureCriticalDispatchOnMainQueue();
#else
        _cache_event_source = wkCreateVMPressureDispatchOnMainQueue();
#endif
        if (_cache_event_source) {
            dispatch_set_context(_cache_event_source, this);
            dispatch_source_set_event_handler(_cache_event_source, ^{
                memoryPressureHandler().respondToMemoryPressure();
            });
            dispatch_resume(_cache_event_source);
        }
    });

    // Allow simulation of memory pressure with "notifyutil -p org.WebKit.lowMemory"
    notify_register_dispatch("org.WebKit.lowMemory", &_notifyToken, dispatch_get_main_queue(), ^(int) {

        // We only do a synchronous GC when *simulating* memory pressure.
        // This gives us a more consistent picture of live objects at the end of testing.
        gcController().garbageCollectNow();

        memoryPressureHandler().respondToMemoryPressure();
        malloc_zone_pressure_relief(nullptr, 0);
    });

    m_installed = true;
}

void MemoryPressureHandler::uninstall()
{
    if (!m_installed)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (_cache_event_source) {
            dispatch_source_cancel(_cache_event_source);
            dispatch_release(_cache_event_source);
            _cache_event_source = 0;
        }

        if (_timer_event_source) {
            dispatch_source_cancel(_timer_event_source);
            dispatch_release(_timer_event_source);
            _timer_event_source = 0;
        }
    });

    m_installed = false;
    
    notify_cancel(_notifyToken);
}

void MemoryPressureHandler::holdOff(unsigned seconds)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        _timer_event_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
        if (_timer_event_source) {
            dispatch_set_context(_timer_event_source, this);
            dispatch_source_set_timer(_timer_event_source, dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 1 * s_minimumHoldOffTime);
            dispatch_source_set_event_handler(_timer_event_source, ^{
                if (_timer_event_source) {
                    dispatch_source_cancel(_timer_event_source);
                    dispatch_release(_timer_event_source);
                    _timer_event_source = 0;
                }
                memoryPressureHandler().install();
            });
            dispatch_resume(_timer_event_source);
        }
    });
}

void MemoryPressureHandler::respondToMemoryPressure()
{
    uninstall();

    double startTime = monotonicallyIncreasingTime();

    m_lowMemoryHandler(false);

    unsigned holdOffTime = (monotonicallyIncreasingTime() - startTime) * s_holdOffMultiplier;

    holdOff(std::max(holdOffTime, s_minimumHoldOffTime));
}

#if PLATFORM(IOS) || (PLATFORM(MAC) && MAC_OS_X_VERSION_MIN_REQUIRED >= 1090)
size_t MemoryPressureHandler::ReliefLogger::platformMemoryUsage()
{
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t err = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &vmInfo, &count);
    if (err != KERN_SUCCESS)
        return static_cast<size_t>(-1);

    return static_cast<size_t>(vmInfo.internal);
}

void MemoryPressureHandler::ReliefLogger::platformLog()
{
    size_t currentMemory = platformMemoryUsage();
    if (currentMemory == static_cast<size_t>(-1) || m_initialMemory == static_cast<size_t>(-1)) {
        NSLog(@"%s (Unable to get dirty memory information for process)\n", m_logString);
        return;
    }

    ssize_t memoryDiff = currentMemory - m_initialMemory;
    if (memoryDiff < 0)
        NSLog(@"Pressure relief: %s: -dirty %ld bytes (from %ld to %ld)\n", m_logString, (memoryDiff * -1), m_initialMemory, currentMemory);
    else if (memoryDiff > 0)
        NSLog(@"Pressure relief: %s: +dirty %ld bytes (from %ld to %ld)\n", m_logString, memoryDiff, m_initialMemory, currentMemory);
    else
        NSLog(@"Pressure relief: %s: =dirty (at %ld bytes)\n", m_logString, currentMemory);
}
#else
void MemoryPressureHandler::ReliefLogger::platformLog() { }
size_t MemoryPressureHandler::ReliefLogger::platformMemoryUsage() { return 0; }
#endif

#if PLATFORM(IOS)
static void respondToMemoryPressureCallback(CFRunLoopObserverRef observer, CFRunLoopActivity /*activity*/, void* /*info*/)
{
    memoryPressureHandler().respondToMemoryPressureIfNeeded();
    CFRunLoopObserverInvalidate(observer);
    CFRelease(observer);
}

void MemoryPressureHandler::installMemoryReleaseBlock(void (^releaseMemoryBlock)(), bool clearPressureOnMemoryRelease)
{
    if (m_installed)
        return;
    m_releaseMemoryBlock = Block_copy(releaseMemoryBlock);
    m_clearPressureOnMemoryRelease = clearPressureOnMemoryRelease;
    m_installed = true;
}

void MemoryPressureHandler::setReceivedMemoryPressure(MemoryPressureReason reason)
{
    m_underMemoryPressure = true;

    {
        MutexLocker locker(m_observerMutex);
        if (!m_observer) {
            m_observer = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeWaiting | kCFRunLoopExit, NO /* don't repeat */,
                0, WebCore::respondToMemoryPressureCallback, NULL);
            CFRunLoopAddObserver(WebThreadRunLoop(), m_observer, kCFRunLoopCommonModes);
            CFRunLoopWakeUp(WebThreadRunLoop());
        }
        m_memoryPressureReason |= reason;
    }
}

void MemoryPressureHandler::clearMemoryPressure()
{
    m_underMemoryPressure = false;

    {
        MutexLocker locker(m_observerMutex);
        m_memoryPressureReason = MemoryPressureReasonNone;
    }
}

bool MemoryPressureHandler::shouldWaitForMemoryClearMessage()
{
    MutexLocker locker(m_observerMutex);
    return m_memoryPressureReason & MemoryPressureReasonVMStatus;
}

void MemoryPressureHandler::respondToMemoryPressureIfNeeded()
{
    ASSERT(WebThreadIsLockedOrDisabled());

    {
        MutexLocker locker(m_observerMutex);
        m_observer = 0;
    }

    if (isUnderMemoryPressure()) {
        ASSERT(m_releaseMemoryBlock);
        LOG(MemoryPressure, "Handle memory pressure at %s", __PRETTY_FUNCTION__);
        m_releaseMemoryBlock();
        if (m_clearPressureOnMemoryRelease)
            clearMemoryPressure();
    }
}

#endif

} // namespace WebCore
