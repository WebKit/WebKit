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

void RealtimeVideoSource::setSupportedPresets(RealtimeVideoSource::VideoPresets&& presets)
{
    m_presets = WTFMove(presets);
    std::sort(m_presets.begin(), m_presets.end(),
        [&] (const auto& a, const auto& b) -> bool {
            return (a.size.width() * a.size.height()) < (b.size.width() * b.size.height());
    });
    
    for (auto& preset : m_presets) {
        std::sort(preset.frameRateRanges.begin(), preset.frameRateRanges.end(),
            [&] (const auto& a, const auto& b) -> bool {
                return a.minimum < b.minimum;
        });
    }
}

template <typename ValueType>
static void updateMinMax(ValueType& min, ValueType& max, ValueType value)
{
    min = std::min<ValueType>(min, value);
    max = std::max<ValueType>(max, value);
}

void RealtimeVideoSource::addSupportedCapabilities(RealtimeMediaSourceCapabilities& capabilities) const
{
    ASSERT(!m_presets.isEmpty());

    int minimumWidth = std::numeric_limits<int>::max();
    int maximumWidth = 0;
    int minimumHeight = std::numeric_limits<int>::max();
    int maximumHeight = 0;
    double minimumAspectRatio = std::numeric_limits<double>::max();
    double maximumAspectRatio = 0;
    double minimumFrameRate = std::numeric_limits<double>::max();
    double maximumFrameRate = 0;
    for (const auto& preset : m_presets) {
        const auto& size = preset.size;
        updateMinMax(minimumWidth, maximumWidth, size.width());
        updateMinMax(minimumHeight, maximumHeight, size.height());

        updateMinMax(minimumAspectRatio, maximumAspectRatio, static_cast<double>(size.width()) / size.height());

        for (const auto& rate : preset.frameRateRanges) {
            updateMinMax(minimumFrameRate, maximumFrameRate, rate.minimum);
            updateMinMax(minimumFrameRate, maximumFrameRate, rate.maximum);
        }
    }
    capabilities.setWidth({ minimumWidth, maximumWidth });
    capabilities.setHeight({ minimumHeight, maximumHeight });
    capabilities.setAspectRatio({ minimumAspectRatio, maximumAspectRatio });
    capabilities.setFrameRate({ minimumFrameRate, maximumFrameRate });
}

bool RealtimeVideoSource::supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (!width && !height && !frameRate)
        return true;

    return !!bestSupportedSizeAndFrameRate(width, height, frameRate);
}

std::optional<RealtimeVideoSource::CaptureSizeAndFrameRate> RealtimeVideoSource::bestSupportedSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (!width && !height && !frameRate)
        return { };

    CaptureSizeAndFrameRate match;
    for (const auto& preset : m_presets) {
        const auto& size = preset.size;
        if ((width && width.value() != size.width()) || (height && height.value() != size.height()))
            continue;

        match.size = size;
        if (!frameRate) {
            match.frameRate = preset.frameRateRanges[preset.frameRateRanges.size() - 1].maximum;
            return match;
        }

        const double epsilon = 0.001;
        for (const auto& rate : preset.frameRateRanges) {
            if (frameRate.value() + epsilon >= rate.minimum && frameRate.value() - epsilon <= rate.maximum) {
                match.frameRate = frameRate.value();
                return match;
            }
        }

        break;
    }

    return { };
}

void RealtimeVideoSource::setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    std::optional<RealtimeVideoSource::CaptureSizeAndFrameRate> match;

    // If only the framerate is changing, first see if it is supported with the current width and height.
    auto size = this->size();
    if (!width && !height && !size.isEmpty())
        match = bestSupportedSizeAndFrameRate(size.width(), size.height(), frameRate);

    if (!match)
        match = bestSupportedSizeAndFrameRate(width, height, frameRate);

    ASSERT(match);
    if (!match)
        return;

    setSize(match->size);
    setFrameRate(match->frameRate);
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

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
