/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioListener.h"

#include "AudioBus.h"
#include "AudioParam.h"

namespace WebCore {

AudioListener::AudioListener(BaseAudioContext& context)
    : m_positionX(AudioParam::create(context, "positionX", 0.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_positionY(AudioParam::create(context, "positionY", 0.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_positionZ(AudioParam::create(context, "positionZ", 0.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_forwardX(AudioParam::create(context, "forwardX", 0.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_forwardY(AudioParam::create(context, "forwardY", 0.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_forwardZ(AudioParam::create(context, "forwardZ", -1.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_upX(AudioParam::create(context, "upX", 0.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_upY(AudioParam::create(context, "upY", 1.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_upZ(AudioParam::create(context, "upZ", 0.0, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_positionXValues(AudioNode::ProcessingSizeInFrames)
    , m_positionYValues(AudioNode::ProcessingSizeInFrames)
    , m_positionZValues(AudioNode::ProcessingSizeInFrames)
    , m_forwardXValues(AudioNode::ProcessingSizeInFrames)
    , m_forwardYValues(AudioNode::ProcessingSizeInFrames)
    , m_forwardZValues(AudioNode::ProcessingSizeInFrames)
    , m_upXValues(AudioNode::ProcessingSizeInFrames)
    , m_upYValues(AudioNode::ProcessingSizeInFrames)
    , m_upZValues(AudioNode::ProcessingSizeInFrames)
{
}

AudioListener::~AudioListener() = default;

bool AudioListener::hasSampleAccurateValues() const
{
    return m_positionX->hasSampleAccurateValues()
        || m_positionY->hasSampleAccurateValues()
        || m_positionZ->hasSampleAccurateValues()
        || m_forwardX->hasSampleAccurateValues()
        || m_forwardY->hasSampleAccurateValues()
        || m_forwardZ->hasSampleAccurateValues()
        || m_upX->hasSampleAccurateValues()
        || m_upY->hasSampleAccurateValues()
        || m_upZ->hasSampleAccurateValues();
}

bool AudioListener::shouldUseARate() const
{
    return m_positionX->automationRate() == AutomationRate::ARate
        || m_positionY->automationRate() == AutomationRate::ARate
        || m_positionZ->automationRate() == AutomationRate::ARate
        || m_forwardX->automationRate() == AutomationRate::ARate
        || m_forwardY->automationRate() == AutomationRate::ARate
        || m_forwardZ->automationRate() == AutomationRate::ARate
        || m_upX->automationRate() == AutomationRate::ARate
        || m_upY->automationRate() == AutomationRate::ARate
        || m_upZ->automationRate() == AutomationRate::ARate;
}

void AudioListener::updateValuesIfNeeded(size_t framesToProcess)
{
    double currentTime = positionX().context().currentTime();
    if (m_lastUpdateTime != currentTime) {
        // Time has changed. Update all of the automation values now.
        m_lastUpdateTime = currentTime;

        ASSERT(framesToProcess <= m_positionXValues.size());
        ASSERT(framesToProcess <= m_positionYValues.size());
        ASSERT(framesToProcess <= m_positionZValues.size());
        ASSERT(framesToProcess <= m_forwardXValues.size());
        ASSERT(framesToProcess <= m_forwardYValues.size());
        ASSERT(framesToProcess <= m_forwardZValues.size());
        ASSERT(framesToProcess <= m_upXValues.size());
        ASSERT(framesToProcess <= m_upYValues.size());
        ASSERT(framesToProcess <= m_upZValues.size());

        positionX().calculateSampleAccurateValues(m_positionXValues.data(), framesToProcess);
        positionY().calculateSampleAccurateValues(m_positionYValues.data(), framesToProcess);
        positionZ().calculateSampleAccurateValues(m_positionZValues.data(), framesToProcess);

        forwardX().calculateSampleAccurateValues(m_forwardXValues.data(), framesToProcess);
        forwardY().calculateSampleAccurateValues(m_forwardYValues.data(), framesToProcess);
        forwardZ().calculateSampleAccurateValues(m_forwardZValues.data(), framesToProcess);

        upX().calculateSampleAccurateValues(m_upXValues.data(), framesToProcess);
        upY().calculateSampleAccurateValues(m_upYValues.data(), framesToProcess);
        upZ().calculateSampleAccurateValues(m_upZValues.data(), framesToProcess);
    }
}

const float* AudioListener::positionXValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_positionXValues.data();
}

const float* AudioListener::positionYValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_positionYValues.data();
}

const float* AudioListener::positionZValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_positionZValues.data();
}

const float* AudioListener::forwardXValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_forwardXValues.data();
}

const float* AudioListener::forwardYValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_forwardYValues.data();
}

const float* AudioListener::forwardZValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_forwardZValues.data();
}

const float* AudioListener::upXValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_upXValues.data();
}

const float* AudioListener::upYValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_upYValues.data();
}

const float* AudioListener::upZValues(size_t framesToProcess)
{
    updateValuesIfNeeded(framesToProcess);
    return m_upZValues.data();
}

ExceptionOr<void> AudioListener::setPosition(float x, float y, float z)
{
    ASSERT(isMainThread());

    double now = m_positionX->context().currentTime();

    auto result = m_positionX->setValueAtTime(x, now);
    if (result.hasException())
        return result.releaseException();
    result = m_positionY->setValueAtTime(y, now);
    if (result.hasException())
        return result.releaseException();
    result = m_positionZ->setValueAtTime(z, now);
    if (result.hasException())
        return result.releaseException();

    return { };
}

FloatPoint3D AudioListener::position() const
{
    return FloatPoint3D { m_positionX->value(), m_positionY->value(), m_positionZ->value() };
}

ExceptionOr<void> AudioListener::setOrientation(float x, float y, float z, float upX, float upY, float upZ)
{
    ASSERT(isMainThread());

    double now = m_forwardX->context().currentTime();

    auto result = m_forwardX->setValueAtTime(x, now);
    if (result.hasException())
        return result.releaseException();
    result = m_forwardY->setValueAtTime(y, now);
    if (result.hasException())
        return result.releaseException();
    result = m_forwardZ->setValueAtTime(z, now);
    if (result.hasException())
        return result.releaseException();
    result = m_upX->setValueAtTime(upX, now);
    if (result.hasException())
        return result.releaseException();
    result = m_upY->setValueAtTime(upY, now);
    if (result.hasException())
        return result.releaseException();
    result = m_upZ->setValueAtTime(upZ, now);
    if (result.hasException())
        return result.releaseException();

    return { };
}

FloatPoint3D AudioListener::orientation() const
{
    return FloatPoint3D { m_forwardX->value(), m_forwardY->value(), m_forwardZ->value() };
}

FloatPoint3D AudioListener::upVector() const
{
    return FloatPoint3D { m_upX->value(), m_upY->value(), m_upZ->value() };
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
