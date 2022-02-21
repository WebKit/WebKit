/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "RealtimeVideoSource.h"

#if ENABLE(MEDIA_STREAM)

#if PLATFORM(COCOA)
#include "ImageTransferSessionVT.h"
#include "MediaSampleAVFObjC.h"
#endif

namespace WebCore {

RealtimeVideoSource::RealtimeVideoSource(Ref<RealtimeVideoCaptureSource>&& source, bool shouldUseIOSurface)
    : RealtimeMediaSource(Type::Video, String { source->name() }, String { source->persistentID() }, String { source->deviceIDHashSalt() })
    , m_source(WTFMove(source))
#if PLATFORM(COCOA)
    , m_shouldUseIOSurface(shouldUseIOSurface)
#endif
{
    UNUSED_PARAM(shouldUseIOSurface);
    m_source->addObserver(*this);
    m_currentSettings = m_source->settings();
    setSize(m_source->size());
    setFrameRate(m_source->frameRate());
}

RealtimeVideoSource::~RealtimeVideoSource()
{
    m_source->removeVideoSampleObserver(*this);
    m_source->removeObserver(*this);
}

void RealtimeVideoSource::whenReady(CompletionHandler<void(String)>&& callback)
{
    m_source->whenReady([this, protectedThis = Ref { *this }, callback = WTFMove(callback)](auto message) mutable {
        setName(String { m_source->name() });
        m_currentSettings = m_source->settings();
        setSize(m_source->size());
        setFrameRate(m_source->frameRate());
        callback(WTFMove(message));
    });
}

void RealtimeVideoSource::startProducingData()
{
    m_source->start();
    m_source->addVideoSampleObserver(*this);
}

void RealtimeVideoSource::stopProducingData()
{
    m_source->removeVideoSampleObserver(*this);
    m_source->stop();
}

bool RealtimeVideoSource::supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    return m_source->supportsSizeAndFrameRate(width, height, frameRate);
}

void RealtimeVideoSource::setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (!width && !height) {
        width = size().width();
        height = size().height();
    }

    m_source->clientUpdatedSizeAndFrameRate(width, height, frameRate);
    auto sourceSize = m_source->size();
    ASSERT(sourceSize.height());
    ASSERT(sourceSize.width());

    if (!width)
        width = sourceSize.width() * height.value() / sourceSize.height();
    m_currentSettings.setWidth(*width);

    if (!height)
        height = sourceSize.height() * width.value() / sourceSize.width();
    m_currentSettings.setHeight(*height);

    if (frameRate)
        m_currentSettings.setFrameRate(static_cast<float>(*frameRate));

    RealtimeMediaSource::setSizeAndFrameRate(width, height, frameRate);
}

void RealtimeVideoSource::sourceMutedChanged()
{
    notifyMutedChange(m_source->muted());
}

void RealtimeVideoSource::sourceSettingsChanged()
{
    auto rotation = m_source->sampleRotation();
    auto size = this->size();
    if (size.isEmpty())
        size = m_source->size();

    if (rotation == MediaSample::VideoRotation::Left || rotation == MediaSample::VideoRotation::Right)
        size = size.transposedSize();

    m_currentSettings.setWidth(size.width());
    m_currentSettings.setHeight(size.height());

    forEachObserver([&](auto& observer) {
        observer.sourceSettingsChanged();
    });
}

bool RealtimeVideoSource::preventSourceFromStopping()
{
    if (!isProducingData())
        return false;

    bool hasObserverPreventingStopping = false;
    forEachObserver([&](auto& observer) {
        if (observer.preventSourceFromStopping())
            hasObserverPreventingStopping = true;
    });
    return hasObserverPreventingStopping;
}

void RealtimeVideoSource::requestToEnd(RealtimeMediaSource::Observer&)
{
    m_source->requestToEnd(*this);
}

void RealtimeVideoSource::stopBeingObserved()
{
    m_source->requestToEnd(*this);
}

void RealtimeVideoSource::sourceStopped()
{
    if (m_source->captureDidFail()) {
        captureFailed();
        return;
    }
    stop();
    forEachObserver([](auto& observer) {
        observer.sourceStopped();
    });
}

#if PLATFORM(COCOA)
RefPtr<MediaSample> RealtimeVideoSource::adaptVideoSample(MediaSample& sample)
{
    if (sample.platformSample().type != PlatformSample::CMSampleBufferType) {
        // FIXME: Support more efficiently downsampling of remote video frames by downsampling in GPUProcess.
        auto newSample = MediaSampleAVFObjC::createImageSample(sample.pixelBuffer(), sample.videoRotation(), sample.videoMirrored(), sample.presentationTime(), { });
        if (!newSample)
            return nullptr;
        return adaptVideoSample(*newSample);
    }
    ASSERT(sample.platformSample().type == PlatformSample::CMSampleBufferType);
    if (!m_imageTransferSession || m_imageTransferSession->pixelFormat() != sample.videoPixelFormat())
        m_imageTransferSession = ImageTransferSessionVT::create(sample.videoPixelFormat(), m_shouldUseIOSurface);

    ASSERT(m_imageTransferSession);
    if (!m_imageTransferSession)
        return nullptr;

    auto mediaSample = m_imageTransferSession->convertMediaSample(sample, size());
    ASSERT(mediaSample);

    return mediaSample;
}
#endif

void RealtimeVideoSource::videoSampleAvailable(MediaSample& sample, VideoSampleMetadata metadata)
{
    if (m_frameDecimation > 1 && ++m_frameDecimationCounter % m_frameDecimation)
        return;

    auto frameRate = this->frameRate();
    m_frameDecimation = frameRate ? static_cast<size_t>(m_source->observedFrameRate() / frameRate) : 1;
    if (!m_frameDecimation)
        m_frameDecimation = 1;

#if PLATFORM(COCOA)
    auto size = this->size();
    if (!size.isEmpty() && size != expandedIntSize(sample.presentationSize())) {
        if (auto mediaSample = adaptVideoSample(sample)) {
            RealtimeMediaSource::videoSampleAvailable(*mediaSample, metadata);
            return;
        }
    }
#endif

    RealtimeMediaSource::videoSampleAvailable(sample, metadata);
}

Ref<RealtimeMediaSource> RealtimeVideoSource::clone()
{
    auto source = create(m_source.copyRef());
    source->m_currentSettings = m_currentSettings;
    source->setSize(size());
#if !RELEASE_LOG_DISABLED
    source->setLogger(logger(), childLogIdentifier(logIdentifier(), ++m_cloneCounter));
#endif
    return source;
}

#if !RELEASE_LOG_DISABLED
void RealtimeVideoSource::setLogger(const Logger& logger, const void* identifier)
{
    RealtimeMediaSource::setLogger(logger, identifier);
    m_source->setLogger(logger, identifier);
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
