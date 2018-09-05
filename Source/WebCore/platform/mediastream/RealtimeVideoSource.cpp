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
#include "CaptureDevice.h"
#include "Logging.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"

namespace WebCore {

RealtimeVideoSource::RealtimeVideoSource(const String& id, const String& name)
    : RealtimeMediaSource(id, Type::Video, name)
{
}

RealtimeVideoSource::~RealtimeVideoSource()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().videoFactory().unsetActiveSource(*this);
#endif
}

void RealtimeVideoSource::prepareToProduceData()
{
    ASSERT(frameRate());

#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().videoFactory().setActiveSource(*this);
#endif

    if (size().isEmpty() && !m_defaultSize.isEmpty())
        setSize(m_defaultSize);
}

void RealtimeVideoSource::setSupportedFrameRates(Vector<double>&& rates)
{
    m_supportedFrameRates = WTFMove(rates);
    std::sort(m_supportedFrameRates.begin(), m_supportedFrameRates.end());
}

void RealtimeVideoSource::addSupportedCapabilities(RealtimeMediaSourceCapabilities& capabilities) const
{
    ASSERT(!m_supportedCaptureSizes.isEmpty());
    ASSERT(!m_supportedFrameRates.isEmpty());

    capabilities.setFrameRate({ m_supportedFrameRates[0], m_supportedFrameRates[m_supportedFrameRates.size() - 1] });

    int minimumWidth = std::numeric_limits<int>::max();
    int maximumWidth = 0;
    int minimumHeight = std::numeric_limits<int>::max();
    int maximumHeight = 0;
    float minimumAspectRatio = std::numeric_limits<float>::max();
    float maximumAspectRatio = 0;
    for (const auto& size : m_supportedCaptureSizes) {
        minimumWidth = std::min(minimumWidth, size.width());
        maximumWidth = std::max(maximumWidth, size.width());

        minimumHeight = std::min(minimumHeight, size.height());
        maximumHeight = std::max(maximumHeight, size.height());

        minimumAspectRatio = std::min(minimumAspectRatio, size.aspectRatio());
        maximumAspectRatio = std::max(maximumAspectRatio, size.aspectRatio());
    }

    capabilities.setWidth({ minimumWidth, maximumWidth });
    capabilities.setHeight({ minimumHeight, maximumHeight });
    capabilities.setAspectRatio({ minimumAspectRatio, maximumAspectRatio });
}

bool RealtimeVideoSource::supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (!height && !width && !frameRate)
        return true;

    if (height || width) {
        if (m_supportedCaptureSizes.isEmpty())
            return false;

        if (bestSupportedCaptureSizeForWidthAndHeight(width, height).isEmpty())
            return false;
    }

    if (!frameRate)
        return true;

    return supportsFrameRate(frameRate.value());
}

IntSize RealtimeVideoSource::bestSupportedCaptureSizeForWidthAndHeight(std::optional<int> width, std::optional<int> height)
{
    if (!width && !height)
        return { };

    for (auto size : m_supportedCaptureSizes) {
        if ((!width || width.value() == size.width()) && (!height || height.value() == size.height()))
            return size;
    }

    return { };
}

void RealtimeVideoSource::applySizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (width || height) {
        IntSize supportedSize = bestSupportedCaptureSizeForWidthAndHeight(WTFMove(width), WTFMove(height));
        ASSERT(!supportedSize.isEmpty());
        if (!supportedSize.isEmpty())
            setSize(supportedSize);
    }

    if (frameRate)
        setFrameRate(frameRate.value());
}

void RealtimeVideoSource::dispatchMediaSampleToObservers(MediaSample& sample)
{
    MediaTime sampleTime = sample.outputPresentationTime();
    if (!sampleTime || !sampleTime.isValid())
        sampleTime = sample.presentationTime();

    auto frameTime = sampleTime.toDouble();
    m_observedFrameTimeStamps.append(frameTime);
    m_observedFrameTimeStamps.removeAllMatching([&](auto time) {
        return time <= frameTime - 2;
    });

    auto interval = m_observedFrameTimeStamps.last() - m_observedFrameTimeStamps.first();
    if (interval > 1)
        m_observedFrameRate = (m_observedFrameTimeStamps.size() / interval);

    videoSampleAvailable(sample);
}

bool RealtimeVideoSource::supportsFrameRate(double frameRate)
{
    double epsilon = 0.001;
    for (auto rate : m_supportedFrameRates) {
        if (frameRate + epsilon >= rate && frameRate - epsilon <= rate)
            return true;
    }
    return false;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
