/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FrameMemoryMonitor.h"

#include "Document.h"
#include "DocumentInlines.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameLoaderClient.h"
#include "Quirks.h"

namespace WebCore {

Ref<FrameMemoryMonitor> FrameMemoryMonitor::create(const LocalFrame& frame)
{
    return adoptRef(*new FrameMemoryMonitor(frame));
}

FrameMemoryMonitor::FrameMemoryMonitor(const LocalFrame& frame)
    : m_frame(frame)
{
}

void FrameMemoryMonitor::setUsage(size_t bytes)
{
    if (m_usageHasExceeded)
        return;

    // FIXME: Add support for memory usage deltas
    m_currentMemoryUsage = bytes;

    checkMemoryPressureAndUnloadFrameIfNeeded();
}

void FrameMemoryMonitor::checkMemoryPressureAndUnloadFrameIfNeeded()
{
    if (m_usageHasExceeded)
        return;

    if (m_currentMemoryUsage.hasOverflowed() || m_currentMemoryUsage > m_maxMemoryAllowedPerFrame) {
        m_exceedCount++;

        if (m_exceedCount >= m_memoryPressureConsecutiveLimit) {
            m_usageHasExceeded = true;
            unloadFrameAndShowMemoryMonitorError();
        }
    } else
        m_exceedCount = 0;
}

void FrameMemoryMonitor::unloadFrameAndShowMemoryMonitorError()
{
    if (RefPtr frame = m_frame.get()) {
        if (RefPtr document = m_frame->document()) {
            if (document->quirks().shouldUnloadHeavyFrame()) {
                // FIXME: Prevent any navigations of an unloaded frame
                frame->showMemoryMonitorError();
            }
        }
    }
}

void FrameMemoryMonitor::lowerAllMemoryLimitsForTesting()
{
    m_maxMemoryAllowedPerFrame = 1;
    m_memoryPressureConsecutiveLimit = 0;
}

} // namespace WebCore
