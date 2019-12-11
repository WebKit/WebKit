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

#pragma once

#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

#if PLATFORM(MAC)
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <OpenGL/CGLTypes.h>
#endif

namespace WebCore {

const unsigned MaxContexts = 16;

class GraphicsContext3D;
class HostWindow;

using PlatformDisplayID = uint32_t;

#if HAVE(APPLE_GRAPHICS_CONTROL)
WEBCORE_EXPORT bool hasLowAndHighPowerGPUs();
#endif

class GraphicsContext3DManager {
    friend NeverDestroyed<GraphicsContext3DManager>;
public:
    static GraphicsContext3DManager& sharedManager();
    
    void addContext(GraphicsContext3D*, HostWindow*);
    void removeContext(GraphicsContext3D*);

    HostWindow* hostWindowForContext(GraphicsContext3D*) const;
    
    void addContextRequiringHighPerformance(GraphicsContext3D*);
    void removeContextRequiringHighPerformance(GraphicsContext3D*);
    
    void recycleContextIfNecessary();
    bool hasTooManyContexts() const { return m_contexts.size() >= MaxContexts; }
    
    void updateAllContexts();

#if PLATFORM(MAC)
    void screenDidChange(PlatformDisplayID, const HostWindow*);
    WEBCORE_EXPORT static void displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags, void*);
#endif
    
private:
    GraphicsContext3DManager()
        : m_disableHighPerformanceGPUTimer(*this, &GraphicsContext3DManager::disableHighPerformanceGPUTimerFired)
    {
    }

    void updateHighPerformanceState();
    void disableHighPerformanceGPUTimerFired();

    Vector<GraphicsContext3D*> m_contexts;
    HashMap<GraphicsContext3D*, HostWindow*> m_contextWindowMap;
    HashSet<GraphicsContext3D*> m_contextsRequiringHighPerformance;
    
    Timer m_disableHighPerformanceGPUTimer;
    bool m_requestingHighPerformance { false };
};

}
