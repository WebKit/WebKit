/*
 * Copyright (C) 2014 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS_THREADED) && USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include <WebCore/DisplayRefreshMonitor.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class ThreadedCompositor;

class ThreadedDisplayRefreshMonitor : public WebCore::DisplayRefreshMonitor {
public:
    static Ref<ThreadedDisplayRefreshMonitor> create(WebCore::PlatformDisplayID displayID, ThreadedCompositor& compositor)
    {
        return adoptRef(*new ThreadedDisplayRefreshMonitor(displayID, compositor));
    }
    virtual ~ThreadedDisplayRefreshMonitor() = default;

    bool requestRefreshCallback() override;

    bool requiresDisplayRefreshCallback();
    void dispatchDisplayRefreshCallback();
    void invalidate();

private:
    ThreadedDisplayRefreshMonitor(WebCore::PlatformDisplayID, ThreadedCompositor&);

    void displayRefreshCallback();
    RunLoop::Timer<ThreadedDisplayRefreshMonitor> m_displayRefreshTimer;
    ThreadedCompositor* m_compositor;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED) && USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
