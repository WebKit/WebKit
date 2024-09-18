/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "PlatformWebViewClientLibWPE.h"

#include <WPEToolingBackends/HeadlessViewBackend.h>
#include <wtf/RunLoop.h>

namespace WTR {

PlatformWebViewClientLibWPE::PlatformWebViewClientLibWPE(WKPageConfigurationRef configuration)
    : m_backend(std::make_unique<WPEToolingBackends::HeadlessViewBackend>(800, 600))
{
#if USE(WPE_BACKEND_PLAYSTATION)
    m_view = WKViewCreateWPE(m_backend->backend(), configuration);
#else
    m_view = WKViewCreateDeprecated(m_backend->backend(), configuration);
#endif
}

PlatformWebViewClientLibWPE::~PlatformWebViewClientLibWPE()
{
}

void PlatformWebViewClientLibWPE::addToWindow()
{
    m_backend->addActivityState(wpe_view_activity_state_in_window);
}

void PlatformWebViewClientLibWPE::removeFromWindow()
{
    m_backend->removeActivityState(wpe_view_activity_state_in_window);
}

PlatformImage PlatformWebViewClientLibWPE::snapshot()
{
    {
        struct TimeoutTimer {
            TimeoutTimer()
                : timer(RunLoop::main(), this, &TimeoutTimer::fired)
            {
                timer.startOneShot(1_s / 60);
            }

            void fired() { RunLoop::main().stop(); }
            RunLoop::Timer timer;
        } timeoutTimer;

        RunLoop::main().run();
    }

    return m_backend->snapshot();
}

} // namespace WTR
