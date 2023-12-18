/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "CGDisplayStreamScreenCaptureSource.h"
#include "ImageTransferSessionVT.h"
#include "Logging.h"
#include "MediaDeviceHashSalts.h"
#include "MediaSampleAVFObjC.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"
#include "RealtimeVideoUtilities.h"
#include "Timer.h"
#include "VideoFrame.h"
#include <IOSurface/IOSurfaceRef.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <sys/time.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>

#if HAVE(REPLAYKIT)
#include "ReplayKitCaptureSource.h"
#endif

#if HAVE(SCREEN_CAPTURE_KIT)
#include "ScreenCaptureKitCaptureSource.h"
#endif

#include "CoreVideoSoftLink.h"
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

CaptureSourceOrError DisplayCaptureSourceCocoa::create(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
    switch (device.type()) {
    case CaptureDevice::DeviceType::Screen:
#if HAVE(REPLAYKIT)
        return create(ReplayKitCaptureSource::create(device.persistentId()), device, WTFMove(hashSalts), constraints, pageIdentifier);
#else
#if HAVE(SCREEN_CAPTURE_KIT)
        if (ScreenCaptureKitCaptureSource::isAvailable())
            return create(ScreenCaptureKitCaptureSource::create(device, constraints), device, WTFMove(hashSalts), constraints, pageIdentifier);
#endif
        return create(CGDisplayStreamScreenCaptureSource::create(device.persistentId()), device, WTFMove(hashSalts), constraints, pageIdentifier);
#endif
    case CaptureDevice::DeviceType::Window:
#if HAVE(SCREEN_CAPTURE_KIT)
        if (ScreenCaptureKitCaptureSource::isAvailable())
            return create(ScreenCaptureKitCaptureSource::create(device, constraints), device, WTFMove(hashSalts), constraints, pageIdentifier);
#endif
        break;
    case CaptureDevice::DeviceType::SystemAudio:
    case CaptureDevice::DeviceType::Microphone:
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

CaptureSourceOrError DisplayCaptureSourceCocoa::create(Expected<UniqueRef<Capturer>, CaptureSourceError>&& capturer, const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
    if (!capturer.has_value())
        return CaptureSourceOrError { WTFMove(capturer.error()) };

    auto source = adoptRef(*new DisplayCaptureSourceCocoa(WTFMove(capturer.value()), device, WTFMove(hashSalts), pageIdentifier));
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return CaptureSourceOrError({ WTFMove(result->badConstraint), MediaAccessDenialReason::InvalidConstraint });
    }

    return CaptureSourceOrError(WTFMove(source));
}

DisplayCaptureSourceCocoa::DisplayCaptureSourceCocoa(UniqueRef<Capturer>&& capturer, const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(device, WTFMove(hashSalts), pageIdentifier)
    , m_capturer(WTFMove(capturer))
    , m_timer(RunLoop::current(), this, &DisplayCaptureSourceCocoa::emitFrame)
    , m_userActivity("App nap disabled for screen capture"_s)
{
    m_capturer->setObserver(*this);
}

DisplayCaptureSourceCocoa::~DisplayCaptureSourceCocoa()
{
}

const RealtimeMediaSourceCapabilities& DisplayCaptureSourceCocoa::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        auto intrinsicSize = m_capturer->intrinsicSize();
        capabilities.setWidth(CapabilityRange(1, intrinsicSize.width()));
        capabilities.setHeight(CapabilityRange(1, intrinsicSize.height()));
        capabilities.setFrameRate(CapabilityRange(.01, 30.0));

        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& DisplayCaptureSourceCocoa::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setFrameRate(frameRate());

        auto size = this->size();
        settings.setWidth(size.width());
        settings.setHeight(size.height());
        settings.setDeviceId(hashedId());
        settings.setLabel(name());

        settings.setDisplaySurface(m_capturer->surfaceType());
        settings.setLogicalSurface(false);

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsFrameRate(true);
        supportedConstraints.setSupportsWidth(true);
        supportedConstraints.setSupportsHeight(true);
        supportedConstraints.setSupportsDeviceId(true);
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

    m_currentSettings = { };
}

void DisplayCaptureSourceCocoa::startProducingData()
{
    m_startTime = MonotonicTime::now();
    m_timer.startRepeating(1_s / frameRate());
    m_userActivity.start();

    commitConfiguration();
    if (!m_capturer->start())
        captureFailed();
}

void DisplayCaptureSourceCocoa::stopProducingData()
{
    m_timer.stop();
    m_userActivity.stop();
    m_elapsedTime += MonotonicTime::now() - m_startTime;
    m_startTime = MonotonicTime::nan();

    m_capturer->stop();
}

Seconds DisplayCaptureSourceCocoa::elapsedTime()
{
    if (m_startTime.isNaN())
        return m_elapsedTime;

    return m_elapsedTime + (MonotonicTime::now() - m_startTime);
}

IntSize DisplayCaptureSourceCocoa::computeResizedVideoFrameSize(IntSize desiredSize, IntSize intrinsicSize)
{
    // We keep the aspect ratio of the intrinsic size for the frame size as getDisplayMedia allows max constraints only.
    if (!intrinsicSize.width() || !intrinsicSize.height())
        return desiredSize;

    if (!desiredSize.height())
        desiredSize.setHeight(intrinsicSize.height());
    if (!desiredSize.width())
        desiredSize.setWidth(intrinsicSize.width());

    auto maxHeight = std::min(desiredSize.height(), intrinsicSize.height());
    auto maxWidth = std::min(desiredSize.width(), intrinsicSize.width());

    auto heightForMaxWidth = maxWidth * intrinsicSize.height() / intrinsicSize.width();
    auto widthForMaxHeight = maxHeight * intrinsicSize.width() / intrinsicSize.height();

    if (heightForMaxWidth <= maxHeight)
        return { maxWidth, heightForMaxWidth };

    if (widthForMaxHeight <= maxWidth)
        return { widthForMaxHeight, maxHeight };

    return intrinsicSize;
}

void DisplayCaptureSourceCocoa::setSizeFrameRateAndZoom(std::optional<int>, std::optional<int>, std::optional<double> frameRate, std::optional<double>)
{
    // We do not need to handle width, height or zoom here since we capture at full size and let each video frame observer resize as needed.
    // FIXME: We should set frameRate according all video frame observers.
    if (frameRate && *frameRate > this->frameRate())
        setFrameRate(*frameRate);
}

void DisplayCaptureSourceCocoa::emitFrame()
{
    if (muted())
        return;

    if (!m_imageTransferSession)
        m_imageTransferSession = ImageTransferSessionVT::create(preferedPixelBufferFormat());

    auto elapsedTime = this->elapsedTime();
    auto sampleTime = MediaTime::createWithDouble((elapsedTime + 100_ms).seconds());

    auto frame = m_capturer->generateFrame();
    auto imageSize = WTF::switchOn(frame,
        [](RetainPtr<IOSurfaceRef>& surface) -> IntSize {
            if (!surface)
                return { };

            return IntSize(IOSurfaceGetWidth(surface.get()), IOSurfaceGetHeight(surface.get()));
        },
        [](RefPtr<NativeImage>& image) -> IntSize {
            if (!image)
                return { };

            return image->size();
        },
        [](RetainPtr<CMSampleBufferRef>& sample) -> IntSize {
            if (!sample)
                return { };

            CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(sample.get());
            if (PAL::CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
                return IntSize();

            return IntSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
        }
    );

    if (imageSize.isEmpty())
        return;

    if (intrinsicSize() != imageSize || size() != imageSize) {
        setIntrinsicSize(imageSize);
        setSize(imageSize);
    }

    auto videoFrame = WTF::switchOn(frame,
        [this, sampleTime](RetainPtr<IOSurfaceRef>& surface) -> RefPtr<VideoFrame> {
            if (!surface)
                return nullptr;

            return m_imageTransferSession->createVideoFrame(surface.get(), sampleTime, size());
        },
        [this, sampleTime](RefPtr<NativeImage>& image) -> RefPtr<VideoFrame> {
            if (!image)
                return nullptr;

            return m_imageTransferSession->createVideoFrame(image->platformImage().get(), sampleTime, size());
        },
        [this, sampleTime](RetainPtr<CMSampleBufferRef>& sample) -> RefPtr<VideoFrame> {
            if (!sample)
                return nullptr;

            return m_imageTransferSession->createVideoFrame(sample.get(), sampleTime, size());
        }
    );

    if (!videoFrame) {
        ASSERT_NOT_REACHED();
        return;
    }

    VideoFrameTimeMetadata metadata;
    metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
    videoFrameAvailable(*videoFrame.get(), metadata);
}

double DisplayCaptureSourceCocoa::observedFrameRate() const
{
    return frameRate();
}

void DisplayCaptureSourceCocoa::capturerConfigurationChanged()
{
    m_currentSettings = { };
    m_capabilities = { };
    auto capturerIntrinsicSize = m_capturer->intrinsicSize();
    if (this->intrinsicSize() != capturerIntrinsicSize)
        setIntrinsicSize(capturerIntrinsicSize);
    forEachObserver([](auto& observer) {
        observer.sourceConfigurationChanged();
    });
}

void DisplayCaptureSourceCocoa::setLogger(const Logger& logger, const void* identifier)
{
    RealtimeMediaSource::setLogger(logger, identifier);
    m_capturer->setLogger(logger, identifier);
}

void DisplayCaptureSourceCocoa::Capturer::setLogger(const Logger& newLogger, const void* newLogIdentifier)
{
    m_logger = &newLogger;
    m_logIdentifier = newLogIdentifier;
}

WTFLogChannel& DisplayCaptureSourceCocoa::Capturer::logChannel() const
{
    return LogWebRTC;
}

void DisplayCaptureSourceCocoa::Capturer::setObserver(CapturerObserver& observer)
{
    m_observer = WeakPtr { observer };
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
