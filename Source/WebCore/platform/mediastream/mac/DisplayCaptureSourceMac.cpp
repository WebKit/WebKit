/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "DisplayCaptureSourceMac.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#include "CGDisplayStreamScreenCaptureSource.h"
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
#include <pal/spi/cf/CoreAudioSPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#include <sys/time.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#include "CoreVideoSoftLink.h"
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

CaptureSourceOrError DisplayCaptureSourceMac::create(const CaptureDevice& device, const MediaConstraints* constraints)
{
    switch (device.type()) {
    case CaptureDevice::DeviceType::Screen:
        return create(CGDisplayStreamScreenCaptureSource::create(device.persistentId()), device, constraints);
    case CaptureDevice::DeviceType::Window:
    case CaptureDevice::DeviceType::Microphone:
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

CaptureSourceOrError DisplayCaptureSourceMac::create(Expected<UniqueRef<Capturer>, String>&& capturer, const CaptureDevice& device, const MediaConstraints* constraints)
{
    if (!capturer.has_value())
        return CaptureSourceOrError { WTFMove(capturer.error()) };

    auto source = adoptRef(*new DisplayCaptureSourceMac(WTFMove(capturer.value()), String { device.label() }));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return WTFMove(result.value().badConstraint);
    }

    return CaptureSourceOrError(WTFMove(source));
}

DisplayCaptureSourceMac::DisplayCaptureSourceMac(UniqueRef<Capturer>&& capturer, String&& name)
    : RealtimeMediaSource(Type::Video, WTFMove(name))
    , m_capturer(WTFMove(capturer))
    , m_timer(RunLoop::current(), this, &DisplayCaptureSourceMac::emitFrame)
{
}

DisplayCaptureSourceMac::~DisplayCaptureSourceMac()
{
}

const RealtimeMediaSourceCapabilities& DisplayCaptureSourceMac::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        auto intrinsicSize = m_capturer->intrinsicSize();
        capabilities.setWidth(CapabilityValueOrRange(1, intrinsicSize.width()));
        capabilities.setHeight(CapabilityValueOrRange(1, intrinsicSize.height()));
        capabilities.setFrameRate(CapabilityValueOrRange(.01, 30.0));

        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& DisplayCaptureSourceMac::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setFrameRate(frameRate());

        auto size = this->size();
        settings.setWidth(size.width());
        settings.setHeight(size.height());

        settings.setDisplaySurface(m_capturer->surfaceType());
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

void DisplayCaptureSourceMac::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.contains(RealtimeMediaSourceSettings::Flag::FrameRate) && m_timer.isActive())
        m_timer.startRepeating(1_s / frameRate());

    m_currentSettings = { };
}

void DisplayCaptureSourceMac::startProducingData()
{
    m_startTime = MonotonicTime::now();
    m_timer.startRepeating(1_s / frameRate());

    if (!m_capturer->start())
        captureFailed();
}

void DisplayCaptureSourceMac::stopProducingData()
{
    m_timer.stop();
    m_elapsedTime += MonotonicTime::now() - m_startTime;
    m_startTime = MonotonicTime::nan();

    m_capturer->stop();
}

Seconds DisplayCaptureSourceMac::elapsedTime()
{
    if (std::isnan(m_startTime))
        return m_elapsedTime;

    return m_elapsedTime + (MonotonicTime::now() - m_startTime);
}

// We keep the aspect ratio of the intrinsic size for the frame size as getDisplayMedia allows max constraints only.
void DisplayCaptureSourceMac::updateFrameSize()
{
    auto intrinsicSize = this->intrinsicSize();

    auto frameSize = size();
    if (!frameSize.height())
        frameSize.setHeight(intrinsicSize.height());
    if (!frameSize.width())
        frameSize.setWidth(intrinsicSize.width());

    auto maxHeight = std::min(frameSize.height(), intrinsicSize.height());
    auto maxWidth = std::min(frameSize.width(), intrinsicSize.width());

    auto heightForMaxWidth = maxWidth * intrinsicSize.height() / intrinsicSize.width();
    auto widthForMaxHeight = maxHeight * intrinsicSize.width() / intrinsicSize.height();

    if (heightForMaxWidth <= maxHeight) {
        setSize({ maxWidth, heightForMaxWidth });
        return;
    }
    if (widthForMaxHeight <= maxWidth) {
        setSize({ widthForMaxHeight, maxHeight });
        return;
    }
    setSize(intrinsicSize);
}

void DisplayCaptureSourceMac::emitFrame()
{
    if (muted())
        return;

    if (!m_imageTransferSession)
        m_imageTransferSession = ImageTransferSessionVT::create(preferedPixelBufferFormat());

    auto sampleTime = MediaTime::createWithDouble((elapsedTime() + 100_ms).seconds());

    auto frame = m_capturer->generateFrame();
    auto imageSize = WTF::switchOn(frame,
        [](RetainPtr<IOSurfaceRef> surface) -> IntSize {
            if (!surface)
                return { };

            return IntSize(IOSurfaceGetWidth(surface.get()), IOSurfaceGetHeight(surface.get()));
        },
        [](RefPtr<NativeImage> image) -> IntSize {
            if (!image)
                return { };

            return image->size();
        }
    );

    if (imageSize.isEmpty())
        return;

    if (intrinsicSize() != imageSize) {
        setIntrinsicSize(imageSize);
        updateFrameSize();
    }

    auto sample = WTF::switchOn(frame,
        [this, sampleTime](RetainPtr<IOSurfaceRef> surface) -> RefPtr<MediaSample> {
            if (!surface)
                return nullptr;

            return m_imageTransferSession->createMediaSample(surface.get(), sampleTime, size());
        },
        [this, sampleTime](RefPtr<NativeImage> image) -> RefPtr<MediaSample> {
            if (!image)
                return nullptr;

            return m_imageTransferSession->createMediaSample(image->platformImage().get(), sampleTime, size());
        }
    );

    if (!sample) {
        ASSERT_NOT_REACHED();
        return;
    }

    videoSampleAvailable(*sample.get());
}

void DisplayCaptureSourceMac::Capturer::setLogger(const Logger& newLogger, const void* newLogIdentifier)
{
    m_logger = &newLogger;
    m_logIdentifier = newLogIdentifier;
}

WTFLogChannel& DisplayCaptureSourceMac::Capturer::logChannel() const
{
    return LogWebRTC;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
