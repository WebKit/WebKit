/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "MediaSampleAVFObjC.h"
#include "PixelBufferConformerCV.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"
#include "RealtimeVideoUtilities.h"
#include "RemoteVideoSample.h"
#include "Timer.h"
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#include <sys/time.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

DisplayCaptureSourceCocoa::DisplayCaptureSourceCocoa(String&& name)
    : RealtimeMediaSource(Type::Video, WTFMove(name))
    , m_timer(RunLoop::current(), this, &DisplayCaptureSourceCocoa::emitFrame)
{
}

DisplayCaptureSourceCocoa::~DisplayCaptureSourceCocoa()
{
#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().videoCaptureFactory().unsetActiveSource(*this);
#endif
}

const RealtimeMediaSourceCapabilities& DisplayCaptureSourceCocoa::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        // FIXME: what should these be?
        capabilities.setWidth(CapabilityValueOrRange(72, 2880));
        capabilities.setHeight(CapabilityValueOrRange(45, 1800));
        capabilities.setFrameRate(CapabilityValueOrRange(.01, 30.0));

        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& DisplayCaptureSourceCocoa::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setFrameRate(frameRate());
        auto size = frameSize();
        if (!size.isEmpty()) {
            settings.setWidth(size.width());
            settings.setHeight(size.height());
        }
        settings.setDisplaySurface(surfaceType());
        settings.setLogicalSurface(false);

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsFrameRate(true);
        supportedConstraints.setSupportsWidth(true);
        supportedConstraints.setSupportsHeight(true);
        supportedConstraints.setSupportsDisplaySurface(true);
        supportedConstraints.setSupportsLogicalSurface(true);

        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }
    return m_currentSettings.value();
}

void DisplayCaptureSourceCocoa::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.contains(RealtimeMediaSourceSettings::Flag::FrameRate) && m_timer.isActive())
        m_timer.startRepeating(1_s / frameRate());

    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height }))
        m_bufferAttributes = nullptr;

    m_currentSettings = { };
}

void DisplayCaptureSourceCocoa::startProducingData()
{
#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().videoCaptureFactory().setActiveSource(*this);
#endif

    m_startTime = MonotonicTime::now();
    m_timer.startRepeating(1_s / frameRate());
}

void DisplayCaptureSourceCocoa::stopProducingData()
{
    m_timer.stop();
    m_elapsedTime += MonotonicTime::now() - m_startTime;
    m_startTime = MonotonicTime::nan();
}

Seconds DisplayCaptureSourceCocoa::elapsedTime()
{
    if (std::isnan(m_startTime))
        return m_elapsedTime;

    return m_elapsedTime + (MonotonicTime::now() - m_startTime);
}

IntSize DisplayCaptureSourceCocoa::frameSize() const
{
    IntSize frameSize = size();
    if (frameSize.isEmpty())
        frameSize = m_intrinsicSize;

    return frameSize;
}

void DisplayCaptureSourceCocoa::setIntrinsicSize(const IntSize& size)
{
    if (m_intrinsicSize == size)
        return;

    m_intrinsicSize = size;
    notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height });

}

void DisplayCaptureSourceCocoa::emitFrame()
{
#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    if (muted())
        return;

    if (!m_imageTransferSession)
        m_imageTransferSession = ImageTransferSessionVT::create(preferedPixelBufferFormat());

    auto sampleTime = MediaTime::createWithDouble((elapsedTime() + 100_ms).seconds());

    auto frame = generateFrame();
    auto imageSize = WTF::switchOn(frame,
        [](RetainPtr<IOSurfaceRef> surface) -> IntSize {
            if (!surface)
                return { };

            return IntSize(IOSurfaceGetWidth(surface.get()), IOSurfaceGetHeight(surface.get()));
        },
        [](RetainPtr<CGImageRef> image) -> IntSize {
            if (!image)
                return { };

            return IntSize(CGImageGetWidth(image.get()), CGImageGetHeight(image.get()));
        }
    );

    ASSERT(!imageSize.isEmpty());
    if (imageSize.isEmpty())
        return;

    if (m_intrinsicSize != imageSize)
        setIntrinsicSize(imageSize);

    auto mediaSampleSize = isRemote() ? imageSize : frameSize();

    RefPtr<MediaSample> sample = WTF::switchOn(frame,
        [this, sampleTime, mediaSampleSize](RetainPtr<IOSurfaceRef> surface) -> RefPtr<MediaSample> {
            if (!surface)
                return nullptr;

            return m_imageTransferSession->createMediaSample(surface.get(), sampleTime, mediaSampleSize);
        },
        [this, sampleTime, mediaSampleSize](RetainPtr<CGImageRef> image) -> RefPtr<MediaSample> {
            if (!image)
                return nullptr;

            return m_imageTransferSession->createMediaSample(image.get(), sampleTime, mediaSampleSize);
        }
    );

    if (!sample) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (isRemote()) {
        auto remoteSample = RemoteVideoSample::create(WTFMove(*sample));
        if (remoteSample)
            remoteVideoSampleAvailable(WTFMove(*remoteSample));

        return;
    }

    videoSampleAvailable(*sample.get());
#endif
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
