/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "HighPerformanceGPUManager.h"

#if PLATFORM(MAC)

#include "Logging.h"
#include <WebCore/GraphicsContextGLOpenGLManager.h>
#include <WebCore/OpenGLSoftLinkCocoa.h>

namespace WebKit {

static bool isiOSAppOnMac()
{
#if PLATFORM(MACCATALYST) && CPU(ARM64)
    static bool isiOSAppOnMac = false;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        isiOSAppOnMac = [[NSProcessInfo processInfo] isiOSAppOnMac];
    });
    return isiOSAppOnMac;
#else
    return false;
#endif
}

// FIXME: This class is using OpenGL to control the muxing of GPUs. Ultimately
// we want to use Metal, but currently there isn't a way to "release" a
// discrete MTLDevice, such that the process muxes back to an integrated GPU.

HighPerformanceGPUManager& HighPerformanceGPUManager::singleton()
{
    static NeverDestroyed<HighPerformanceGPUManager> sharedManager;
    return sharedManager;
}

void HighPerformanceGPUManager::addProcessRequiringHighPerformance(WebProcessProxy* process)
{
    if (isiOSAppOnMac())
        return;

    if (!WebCore::hasLowAndHighPowerGPUs())
        return;

    if (m_processesRequiringHighPerformance.add(process)) {
        LOG(WebGL, "HighPerformanceGPUManager::addProcessRequiringHighPerformance() - adding process %p", process);
        updateState();
        return;
    }

    LOG(WebGL, "HighPerformanceGPUManager::addProcessRequiringHighPerformance() - process %p was already requesting high performance", process);
}

void HighPerformanceGPUManager::removeProcessRequiringHighPerformance(WebProcessProxy* process)
{
    if (isiOSAppOnMac())
        return;

    if (!WebCore::hasLowAndHighPowerGPUs())
        return;

    if (m_processesRequiringHighPerformance.remove(process)) {
        LOG(WebGL, "HighPerformanceGPUManager::removeProcessRequiringHighPerformance() - removing process %p", process);
        updateState();
        return;
    }

    LOG(WebGL, "HighPerformanceGPUManager::removeProcessRequiringHighPerformance() - process %p was not requesting high performance", process);
}

void HighPerformanceGPUManager::updateState()
{
    if (isiOSAppOnMac())
        return;

    if (m_processesRequiringHighPerformance.size()) {
        if (!m_pixelFormatObj) {
            LOG(WebGL, "HighPerformanceGPUManager - turning on high-performance GPU.");

            CGLPixelFormatAttribute attributes[] = { kCGLPFAAccelerated, kCGLPFAColorSize, static_cast<CGLPixelFormatAttribute>(32), static_cast<CGLPixelFormatAttribute>(0) };
            GLint numPixelFormats = 0;
            CGLChoosePixelFormat(attributes, &m_pixelFormatObj, &numPixelFormats);

            LOG(WebGL, "HighPerformanceGPUManager - CGLPixelFormatObj is %p", m_pixelFormatObj);
        }
    } else if (m_pixelFormatObj) {
        LOG(WebGL, "HighPerformanceGPUManager - turning off high-performance GPU.");
        CGLReleasePixelFormat(m_pixelFormatObj);
        m_pixelFormatObj = nullptr;
    }
}

}
#endif

