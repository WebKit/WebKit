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

#if ENABLE(GRAPHICS_CONTEXT_3D)
#include "GraphicsContext3DManager.h"

#include "GraphicsContext3D.h"
#include "Logging.h"

#if HAVE(APPLE_GRAPHICS_CONTROL)
#include <sys/sysctl.h>
#endif

#if PLATFORM(MAC)
#include "SwitchingGPUClient.h"
#include <OpenGL/OpenGL.h>
#endif

namespace WebCore {

#if HAVE(APPLE_GRAPHICS_CONTROL)

enum {
    kAGCOpen,
    kAGCClose
};

static io_connect_t attachToAppleGraphicsControl()
{
    mach_port_t masterPort = MACH_PORT_NULL;
    
    if (IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS)
        return MACH_PORT_NULL;
    
    CFDictionaryRef classToMatch = IOServiceMatching("AppleGraphicsControl");
    if (!classToMatch)
        return MACH_PORT_NULL;
    
    kern_return_t kernResult;
    io_iterator_t iterator;
    if ((kernResult = IOServiceGetMatchingServices(masterPort, classToMatch, &iterator)) != KERN_SUCCESS)
        return MACH_PORT_NULL;
    
    io_service_t serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
        return MACH_PORT_NULL;
    
    io_connect_t dataPort;
    IOObjectRetain(serviceObject);
    kernResult = IOServiceOpen(serviceObject, mach_task_self(), 0, &dataPort);
    IOObjectRelease(serviceObject);
    
    return (kernResult == KERN_SUCCESS) ? dataPort : MACH_PORT_NULL;
}

static bool hasMuxCapability()
{
    io_connect_t dataPort = attachToAppleGraphicsControl();
    
    if (dataPort == MACH_PORT_NULL)
        return false;
    
    bool result;
    if (IOConnectCallScalarMethod(dataPort, kAGCOpen, nullptr, 0, nullptr, nullptr) == KERN_SUCCESS) {
        IOConnectCallScalarMethod(dataPort, kAGCClose, nullptr, 0, nullptr, nullptr);
        result = true;
    } else
        result = false;
    
    IOServiceClose(dataPort);
    
    if (result) {
        // This is detecting Mac hardware with an Intel g575 GPU, which
        // we don't want to make available to muxing.
        // Based on information from Apple's OpenGL team, such devices
        // have four or fewer processors.
        // <rdar://problem/30060378>
        int names[2] = { CTL_HW, HW_NCPU };
        int cpuCount;
        size_t cpuCountLength = sizeof(cpuCount);
        sysctl(names, 2, &cpuCount, &cpuCountLength, nullptr, 0);
        result = cpuCount > 4;
    }
    
    return result;
}

bool hasLowAndHighPowerGPUs()
{
    static bool canMux = hasMuxCapability();
    return canMux;
}
#endif // HAVE(APPLE_GRAPHICS_CONTROL)

GraphicsContext3DManager& GraphicsContext3DManager::sharedManager()
{
    static NeverDestroyed<GraphicsContext3DManager> s_manager;
    return s_manager;
}

#if PLATFORM(MAC)
void GraphicsContext3DManager::displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags flags, void*)
{
    LOG(WebGL, "GraphicsContext3DManager::displayWasReconfigured");
    if (flags & kCGDisplaySetModeFlag)
        GraphicsContext3DManager::sharedManager().updateAllContexts();
}
#endif

void GraphicsContext3DManager::updateAllContexts()
{
#if PLATFORM(MAC)
    for (const auto& context : m_contexts) {
        context->updateCGLContext();
        context->dispatchContextChangedNotification();
    }
#endif
}

#if PLATFORM(MAC)
void GraphicsContext3DManager::screenDidChange(PlatformDisplayID displayID, const HostWindow* window)
{
    for (const auto& contextAndWindow : m_contextWindowMap) {
        if (contextAndWindow.value == window) {
            contextAndWindow.key->screenDidChange(displayID);
            LOG(WebGL, "Changing context (%p) to display (%d).", contextAndWindow.key, displayID);
        }
    }
}
#endif

void GraphicsContext3DManager::addContext(GraphicsContext3D* context, HostWindow* window)
{
    ASSERT(context);
    if (!context)
        return;
    
#if PLATFORM(MAC) && !ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    if (!m_contexts.size())
        CGDisplayRegisterReconfigurationCallback(displayWasReconfigured, nullptr);
#endif

    ASSERT(!m_contexts.contains(context));
    m_contexts.append(context);
    m_contextWindowMap.set(context, window);
}

void GraphicsContext3DManager::removeContext(GraphicsContext3D* context)
{
    ASSERT(m_contexts.contains(context));
    m_contexts.removeFirst(context);
    m_contextWindowMap.remove(context);
    removeContextRequiringHighPerformance(context);
    
#if PLATFORM(MAC) && !ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    if (!m_contexts.size())
        CGDisplayRemoveReconfigurationCallback(displayWasReconfigured, nullptr);
#endif
}

HostWindow* GraphicsContext3DManager::hostWindowForContext(GraphicsContext3D* context) const
{
    ASSERT(m_contextWindowMap.contains(context));
    return m_contextWindowMap.get(context);
}

void GraphicsContext3DManager::addContextRequiringHighPerformance(GraphicsContext3D* context)
{
    ASSERT(context);
    if (!context)
        return;
    
    ASSERT(m_contexts.contains(context));
    ASSERT(!m_contextsRequiringHighPerformance.contains(context));
    
    LOG(WebGL, "This context (%p) requires the high-performance GPU.", context);
    m_contextsRequiringHighPerformance.add(context);
    
    updateHighPerformanceState();
}

void GraphicsContext3DManager::removeContextRequiringHighPerformance(GraphicsContext3D* context)
{
    if (!m_contextsRequiringHighPerformance.contains(context))
        return;
    
    LOG(WebGL, "This context (%p) no longer requires the high-performance GPU.", context);
    m_contextsRequiringHighPerformance.remove(context);
    
    updateHighPerformanceState();
}

void GraphicsContext3DManager::updateHighPerformanceState()
{
#if PLATFORM(MAC)
    if (!hasLowAndHighPowerGPUs())
        return;
    
    if (m_contextsRequiringHighPerformance.size()) {
        
        if (m_disableHighPerformanceGPUTimer.isActive()) {
            LOG(WebGL, "Cancel pending timer for turning off high-performance GPU.");
            m_disableHighPerformanceGPUTimer.stop();
        }

        if (!m_requestingHighPerformance) {
            LOG(WebGL, "Request the high-performance GPU.");
            m_requestingHighPerformance = true;
#if PLATFORM(MAC)
            SwitchingGPUClient::singleton().requestHighPerformanceGPU();
#endif
        }

    } else {
        // Don't immediately turn off the high-performance GPU. The user might be
        // swapping back and forth between tabs or windows, and we don't want to cause
        // churn if we can avoid it.
        if (!m_disableHighPerformanceGPUTimer.isActive()) {
            LOG(WebGL, "Set a timer to release the high-performance GPU.");
            // FIXME: Expose this value as a Setting, which would require this class
            // to reference a frame, page or document.
            static const Seconds timeToKeepHighPerformanceGPUAlive { 10_s };
            m_disableHighPerformanceGPUTimer.startOneShot(timeToKeepHighPerformanceGPUAlive);
        }
    }
#endif
}

void GraphicsContext3DManager::disableHighPerformanceGPUTimerFired()
{
    if (m_contextsRequiringHighPerformance.size())
        return;

    m_requestingHighPerformance = false;
#if PLATFORM(MAC)
    SwitchingGPUClient::singleton().releaseHighPerformanceGPU();
#endif
}

void GraphicsContext3DManager::recycleContextIfNecessary()
{
    if (hasTooManyContexts()) {
        LOG(WebGL, "GraphicsContext3DManager recycled context (%p).", m_contexts[0]);
        m_contexts[0]->recycleContext();
    }
}

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
