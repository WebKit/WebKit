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

namespace WebCore {

RealtimeVideoSource::RealtimeVideoSource(Ref<RealtimeVideoCaptureSource>&& source)
    : RealtimeVideoCaptureSource(String { source->name() }, String { source->persistentID() }, String { source->deviceIDHashSalt() })
    , m_source(WTFMove(source))
{
    m_source->addObserver(*this);
    m_currentSettings = m_source->settings();
}

RealtimeVideoSource::~RealtimeVideoSource()
{
    m_source->removeObserver(*this);
}

void RealtimeVideoSource::startProducingData()
{
    m_source->start();
}

void RealtimeVideoSource::stopProducingData()
{
    m_source->stop();
}

bool RealtimeVideoSource::supportsSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double> frameRate)
{
    return m_source->supportsSizeAndFrameRate(width, height, frameRate);
}

void RealtimeVideoSource::setSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double> frameRate)
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

void RealtimeVideoSource::videoSampleAvailable(MediaSample& sample)
{
    if (!isProducingData())
        return;

    if (auto mediaSample = adaptVideoSample(sample))
        RealtimeMediaSource::videoSampleAvailable(*mediaSample);
}

Ref<RealtimeMediaSource> RealtimeVideoSource::clone()
{
    auto source = create(m_source.copyRef());
    source->m_currentSettings = m_currentSettings;

    return source;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
