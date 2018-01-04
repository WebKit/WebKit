/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DisplayCaptureSourceCocoa.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"
#include "Timer.h"
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <sys/time.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
using namespace PAL;

DisplayCaptureSourceCocoa::DisplayCaptureSourceCocoa(const String& name)
    : RealtimeMediaSource("", Type::Video, name)
    , m_timer(RunLoop::current(), this, &DisplayCaptureSourceCocoa::emitFrame)
{
}

DisplayCaptureSourceCocoa::~DisplayCaptureSourceCocoa()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().videoFactory().unsetActiveSource(*this);
#endif
}

const RealtimeMediaSourceCapabilities& DisplayCaptureSourceCocoa::capabilities() const
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        // FIXME: what should these be?
        capabilities.setWidth(CapabilityValueOrRange(72, 2880));
        capabilities.setHeight(CapabilityValueOrRange(45, 1800));
        capabilities.setFrameRate(CapabilityValueOrRange(.01, 60.0));

        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& DisplayCaptureSourceCocoa::settings() const
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setFrameRate(frameRate());
        auto size = this->size();
        if (size.width() && size.height()) {
            settings.setWidth(size.width());
            settings.setHeight(size.height());
        }

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsFrameRate(true);
        supportedConstraints.setSupportsWidth(true);
        supportedConstraints.setSupportsHeight(true);
        supportedConstraints.setSupportsAspectRatio(true);
        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }
    return m_currentSettings.value();
}

void DisplayCaptureSourceCocoa::settingsDidChange()
{
    m_currentSettings = std::nullopt;
    RealtimeMediaSource::settingsDidChange();
}

void DisplayCaptureSourceCocoa::startProducingData()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().videoFactory().setActiveSource(*this);
#endif

    m_startTime = monotonicallyIncreasingTime();
    m_timer.startRepeating(1_ms * lround(1000 / frameRate()));
}

void DisplayCaptureSourceCocoa::stopProducingData()
{
    m_timer.stop();
    m_elapsedTime += monotonicallyIncreasingTime() - m_startTime;
    m_startTime = NAN;
}

double DisplayCaptureSourceCocoa::elapsedTime()
{
    if (std::isnan(m_startTime))
        return m_elapsedTime;

    return m_elapsedTime + (monotonicallyIncreasingTime() - m_startTime);
}

bool DisplayCaptureSourceCocoa::applyFrameRate(double rate)
{
    if (m_timer.isActive())
        m_timer.startRepeating(1_ms * lround(1000 / rate));

    return true;
}

void DisplayCaptureSourceCocoa::emitFrame()
{
    if (muted())
        return;

    generateFrame();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
