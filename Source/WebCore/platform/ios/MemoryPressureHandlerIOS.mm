/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
#import "Logging.h"
#import "SystemMemory.h"
#import "WebCoreThread.h"
#import <libkern/OSAtomic.h>

namespace WebCore {

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
    OSAtomicTestAndSet(0, &m_receivedMemoryPressure);

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

bool MemoryPressureHandler::hasReceivedMemoryPressure()
{
    return OSAtomicOr32(0, &m_receivedMemoryPressure);
}

void MemoryPressureHandler::clearMemoryPressure()
{
    OSAtomicTestAndClear(0, &m_receivedMemoryPressure);

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

    if (hasReceivedMemoryPressure()) {
        ASSERT(m_releaseMemoryBlock);
        LOG(MemoryPressure, "Handle memory pressure at %s", __PRETTY_FUNCTION__);
        m_releaseMemoryBlock();
        if (m_clearPressureOnMemoryRelease)
            clearMemoryPressure();
    }
}

void MemoryPressureHandler::platformReleaseMemory(bool)
{
#if USE(IOSURFACE)
    IOSurfacePool::sharedPool().discardAllSurfaces();
#endif
}

} // namespace WebCore
