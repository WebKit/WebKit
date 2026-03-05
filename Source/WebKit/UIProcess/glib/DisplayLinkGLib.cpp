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
#include "DisplayLink.h"
#include "Logging.h"
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

using namespace WebCore;

void DisplayLink::platformInitialize()
{
    // FIXME: We can get here with displayID == 0 (webkit.org/b/212120), in which case DisplayVBlankMonitor defaults to the main screen.
    m_vblankMonitor = DisplayVBlankMonitor::create(m_displayID);
    m_vblankMonitor->setHandler([this] {
        m_fpsThrottleCallCounter++;
        if (m_fpsThrottleCallCounter >= m_fpsThrottleRatio) {
            notifyObserversDisplayDidRefresh();
            m_fpsThrottleCallCounter = 0;
        }
    });

    unsigned refreshRate = m_vblankMonitor->refreshRate();
    m_displayNominalFramesPerSecond = refreshRate;
    if (const char* envString = getenv("WEBKIT_DISPLAY_REFRESH_THROTTLE_FPS")) {
        if (auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString))) {
            // The throttle FPS must be non zero and a factor of the real display refresh rate
            if (!*newValue)
                WTFLogAlways("WEBKIT_DISPLAY_REFRESH_THROTTLE_FPS=%s rejected: cannot be zero", envString); // NOLINT
            else if (refreshRate % *newValue)
                WTFLogAlways("WEBKIT_DISPLAY_REFRESH_THROTTLE_FPS=%s rejected: not a factor of refresh rate %ufps (remainder %u)", envString, refreshRate, refreshRate % *newValue); // NOLINT
            else
                m_displayNominalFramesPerSecond = *newValue;
        } else
            WTFLogAlways("WEBKIT_DISPLAY_REFRESH_THROTTLE_FPS=%s rejected: not a positive integer", envString); // NOLINT
    }

    m_fpsThrottleRatio = refreshRate / m_displayNominalFramesPerSecond;
    if (m_fpsThrottleRatio != 1)
        RELEASE_LOG(DisplayLink, "[UI] DisplayLink is throttled down from %ufps to %ufps", refreshRate, m_displayNominalFramesPerSecond);
}

void DisplayLink::platformFinalize()
{
    ASSERT(m_vblankMonitor);
    m_vblankMonitor->invalidate();
}

bool DisplayLink::platformIsRunning() const
{
    return m_vblankMonitor->isActive();
}

void DisplayLink::platformStart()
{
    m_vblankMonitor->start();
}

void DisplayLink::platformStop()
{
    m_vblankMonitor->stop();
}

} // namespace WebKit

