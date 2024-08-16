/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PannerNode.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBufferSourceNode.h"
#include "AudioBus.h"
#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "AudioUtilities.h"
#include "ChannelCountMode.h"
#include "HRTFDatabaseLoader.h"
#include "HRTFPanner.h"
#include "ScriptExecutionContext.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PannerNode);

static void fixNANs(double &x)
{
    if (std::isnan(x) || std::isinf(x))
        x = 0.0;
}

ExceptionOr<Ref<PannerNode>> PannerNode::create(BaseAudioContext& context, const PannerOptions& options)
{
    auto panner = adoptRef(*new PannerNode(context, options));

    auto result = panner->handleAudioNodeOptions(options, { 2, ChannelCountMode::ClampedMax, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    result = panner->setMaxDistanceForBindings(options.maxDistance);
    if (result.hasException())
        return result.releaseException();

    result = panner->setRefDistanceForBindings(options.refDistance);
    if (result.hasException())
        return result.releaseException();

    result = panner->setRolloffFactorForBindings(options.rolloffFactor);
    if (result.hasException())
        return result.releaseException();

    result = panner->setConeOuterGainForBindings(options.coneOuterGain);
    if (result.hasException())
        return result.releaseException();

    return panner;
}

PannerNode::PannerNode(BaseAudioContext& context, const PannerOptions& options)
    : AudioNode(context, NodeTypePanner)
    // Load the HRTF database asynchronously so we don't block the Javascript thread while creating the HRTF database.
    , m_hrtfDatabaseLoader(HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(context.sampleRate()))
    , m_panningModel(options.panningModel)
    , m_panner(Panner::create(m_panningModel, sampleRate(), m_hrtfDatabaseLoader.ptr()))
    , m_positionX(AudioParam::create(context, "positionX"_s, options.positionX, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_positionY(AudioParam::create(context, "positionY"_s, options.positionY, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_positionZ(AudioParam::create(context, "positionZ"_s, options.positionZ, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_orientationX(AudioParam::create(context, "orientationX"_s, options.orientationX, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_orientationY(AudioParam::create(context, "orientationY"_s, options.orientationY, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_orientationZ(AudioParam::create(context, "orientationZ"_s, options.orientationZ, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
{
    setDistanceModelForBindings(options.distanceModel);
    setConeInnerAngleForBindings(options.coneInnerAngle);
    setConeOuterAngleForBindings(options.coneOuterAngle);

    addInput();
    addOutput(2);

    initialize();
}

PannerNode::~PannerNode()
{
    uninitialize();
}

void PannerNode::process(size_t framesToProcess)
{
    AudioBus* destination = output(0)->bus();

    if (!isInitialized() || !input(0)->isConnected()) {
        destination->zero();
        return;
    }

    AudioBus* source = input(0)->bus();
    if (!source) {
        destination->zero();
        return;
    }

    // The audio thread can't block on this lock, so we use tryLock() instead.
    if (!m_processLock.tryLock()) {
        // Too bad - tryLock() failed. We must be in the middle of changing the panner.
        destination->zero();
        return;
    }
    Locker locker { AdoptLock, m_processLock };

    if (!m_panner) {
        destination->zero();
        return;
    }

    // HRTFDatabase should be loaded before proceeding for offline audio context when m_panningModel is "HRTF".
    if (m_panningModel == PanningModelType::HRTF && !m_hrtfDatabaseLoader->isLoaded()) {
        if (context().isOfflineContext())
            m_hrtfDatabaseLoader->waitForLoaderThreadCompletion();
        else {
            destination->zero();
            return;
        }
    }

    invalidateCachedPropertiesIfNecessary();

    if ((hasSampleAccurateValues() || listener().hasSampleAccurateValues()) && (shouldUseARate() || listener().shouldUseARate())) {
        processSampleAccurateValues(destination, source, framesToProcess);
        return;
    }

    // Apply the panning effect.
    auto [azimuth, elevation] = azimuthElevation();
    m_panner->pan(azimuth, elevation, source, destination, framesToProcess);

    // Get the distance and cone gain.
    double totalGain = distanceConeGain();

    // Apply gain in-place.
    destination->copyWithGainFrom(*destination, totalGain);
}

void PannerNode::processOnlyAudioParams(size_t framesToProcess)
{
    ASSERT(context().isAudioThread());
    if (!m_processLock.tryLock())
        return;

    Locker locker { AdoptLock, m_processLock };
    float values[AudioUtilities::renderQuantumSize];
    ASSERT(framesToProcess <= AudioUtilities::renderQuantumSize);

    m_positionX->calculateSampleAccurateValues(values, framesToProcess);
    m_positionY->calculateSampleAccurateValues(values, framesToProcess);
    m_positionZ->calculateSampleAccurateValues(values, framesToProcess);

    m_orientationX->calculateSampleAccurateValues(values, framesToProcess);
    m_orientationY->calculateSampleAccurateValues(values, framesToProcess);
    m_orientationZ->calculateSampleAccurateValues(values, framesToProcess);

    listener().updateValuesIfNeeded(framesToProcess);
}

void PannerNode::processSampleAccurateValues(AudioBus* destination, const AudioBus* source, size_t framesToProcess)
{
    // Get the sample accurate values from all of the AudioParams, including the
    // values from the AudioListener.
    float pannerX[AudioUtilities::renderQuantumSize];
    float pannerY[AudioUtilities::renderQuantumSize];
    float pannerZ[AudioUtilities::renderQuantumSize];

    float orientationX[AudioUtilities::renderQuantumSize];
    float orientationY[AudioUtilities::renderQuantumSize];
    float orientationZ[AudioUtilities::renderQuantumSize];

    m_positionX->calculateSampleAccurateValues(pannerX, framesToProcess);
    m_positionY->calculateSampleAccurateValues(pannerY, framesToProcess);
    m_positionZ->calculateSampleAccurateValues(pannerZ, framesToProcess);
    m_orientationX->calculateSampleAccurateValues(orientationX, framesToProcess);
    m_orientationY->calculateSampleAccurateValues(orientationY, framesToProcess);
    m_orientationZ->calculateSampleAccurateValues(orientationZ, framesToProcess);

    // Get the automation values from the listener.
    const float* listenerX = listener().positionXValues(AudioUtilities::renderQuantumSize);
    const float* listenerY = listener().positionYValues(AudioUtilities::renderQuantumSize);
    const float* listenerZ = listener().positionZValues(AudioUtilities::renderQuantumSize);

    const float* forwardX = listener().forwardXValues(AudioUtilities::renderQuantumSize);
    const float* forwardY = listener().forwardYValues(AudioUtilities::renderQuantumSize);
    const float* forwardZ = listener().forwardZValues(AudioUtilities::renderQuantumSize);

    const float* upX = listener().upXValues(AudioUtilities::renderQuantumSize);
    const float* upY = listener().upYValues(AudioUtilities::renderQuantumSize);
    const float* upZ = listener().upZValues(AudioUtilities::renderQuantumSize);

    // Compute the azimuth, elevation, and total gains for each position.
    double azimuth[AudioUtilities::renderQuantumSize];
    double elevation[AudioUtilities::renderQuantumSize];
    float totalGain[AudioUtilities::renderQuantumSize];

    for (size_t k = 0; k < framesToProcess; ++k) {
        FloatPoint3D pannerPosition(pannerX[k], pannerY[k], pannerZ[k]);
        FloatPoint3D orientation(orientationX[k], orientationY[k], orientationZ[k]);
        FloatPoint3D listenerPosition(listenerX[k], listenerY[k], listenerZ[k]);
        FloatPoint3D listenerFront(forwardX[k], forwardY[k], forwardZ[k]);
        FloatPoint3D listenerUp(upX[k], upY[k], upZ[k]);

        auto [calculatedAzimuth, calculatedElevation] = calculateAzimuthElevation(pannerPosition, listenerPosition, listenerFront, listenerUp);
        azimuth[k] = calculatedAzimuth;
        elevation[k] = calculatedElevation;

        // Get distance and cone gain
        totalGain[k] = calculateDistanceConeGain(pannerPosition, orientation, listenerPosition, m_distanceEffect, m_coneEffect);
    }

    m_panner->panWithSampleAccurateValues(azimuth, elevation, source, destination, framesToProcess);
    destination->copyWithSampleAccurateGainValuesFrom(*destination, totalGain, framesToProcess);
}

bool PannerNode::hasSampleAccurateValues() const
{
    return m_positionX->hasSampleAccurateValues()
        || m_positionY->hasSampleAccurateValues()
        || m_positionZ->hasSampleAccurateValues()
        || m_orientationX->hasSampleAccurateValues()
        || m_orientationY->hasSampleAccurateValues()
        || m_orientationZ->hasSampleAccurateValues();
}

bool PannerNode::shouldUseARate() const
{
    return m_positionX->automationRate() == AutomationRate::ARate
        || m_positionY->automationRate() == AutomationRate::ARate
        || m_positionZ->automationRate() == AutomationRate::ARate
        || m_orientationX->automationRate() == AutomationRate::ARate
        || m_orientationY->automationRate() == AutomationRate::ARate
        || m_orientationZ->automationRate() == AutomationRate::ARate;
}

AudioListener& PannerNode::listener()
{
    return context().listener();
}

void PannerNode::setPanningModelForBindings(PanningModelType model)
{
    ASSERT(isMainThread());

    // This synchronizes with process().
    Locker locker { m_processLock };
    if (!m_panner || model != m_panningModel) {
        m_panner = Panner::create(model, sampleRate(), m_hrtfDatabaseLoader.ptr());
        m_panningModel = model;
    }
}

FloatPoint3D PannerNode::position() const
{
    return FloatPoint3D(m_positionX->value(), m_positionY->value(), m_positionZ->value());
}

ExceptionOr<void> PannerNode::setPosition(float x, float y, float z)
{
    ASSERT(isMainThread());

    // This synchronizes with process().
    Locker locker { m_processLock };

    auto now = context().currentTime();

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

FloatPoint3D PannerNode::orientation() const
{
    return FloatPoint3D(m_orientationX->value(), m_orientationY->value(), m_orientationZ->value());
}

ExceptionOr<void> PannerNode::setOrientation(float x, float y, float z)
{
    ASSERT(isMainThread());

    // This synchronizes with process().
    Locker locker { m_processLock };

    auto now = context().currentTime();

    auto result = m_orientationX->setValueAtTime(x, now);
    if (result.hasException())
        return result.releaseException();
    result = m_orientationY->setValueAtTime(y, now);
    if (result.hasException())
        return result.releaseException();
    result = m_orientationZ->setValueAtTime(z, now);
    if (result.hasException())
        return result.releaseException();

    return { };
}

DistanceModelType PannerNode::distanceModelForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(isMainThread());
    return m_distanceEffect.model();
}

void PannerNode::setDistanceModelForBindings(DistanceModelType model)
{
    ASSERT(isMainThread());

    // This synchronizes with process().
    Locker locker { m_processLock };

    if (m_distanceEffect.model() == model)
        return;

    m_distanceEffect.setModel(model, true);
    m_cachedConeGain = std::nullopt;
}

ExceptionOr<void> PannerNode::setRefDistanceForBindings(double refDistance)
{
    ASSERT(isMainThread());

    if (refDistance < 0)
        return Exception { ExceptionCode::RangeError, "refDistance cannot be set to a negative value"_s };
    
    // This synchronizes with process().
    Locker locker { m_processLock };

    if (m_distanceEffect.refDistance() == refDistance)
        return { };

    m_distanceEffect.setRefDistance(refDistance);
    m_cachedConeGain = std::nullopt;
    return { };
}

ExceptionOr<void> PannerNode::setMaxDistanceForBindings(double maxDistance)
{
    ASSERT(isMainThread());

    if (maxDistance <= 0)
        return Exception { ExceptionCode::RangeError, "maxDistance cannot be set to a non-positive value"_s };
    
    // This synchronizes with process().
    Locker locker { m_processLock };

    if (m_distanceEffect.maxDistance() == maxDistance)
        return { };

    m_distanceEffect.setMaxDistance(maxDistance);
    m_cachedConeGain = std::nullopt;
    return { };
}

ExceptionOr<void> PannerNode::setRolloffFactorForBindings(double rolloffFactor)
{
    ASSERT(isMainThread());

    if (rolloffFactor < 0)
        return Exception { ExceptionCode::RangeError, "rolloffFactor cannot be set to a negative value"_s };
    
    // This synchronizes with process().
    Locker locker { m_processLock };

    if (m_distanceEffect.rolloffFactor() == rolloffFactor)
        return { };

    m_distanceEffect.setRolloffFactor(rolloffFactor);
    m_cachedConeGain = std::nullopt;
    return { };
}

ExceptionOr<void> PannerNode::setConeOuterGainForBindings(double gain)
{
    ASSERT(isMainThread());

    if (gain < 0 || gain > 1)
        return Exception { ExceptionCode::InvalidStateError, "coneOuterGain must be in [0, 1]"_s };
    
    // This synchronizes with process().
    Locker locker { m_processLock };

    if (m_coneEffect.outerGain() == gain)
        return { };

    m_coneEffect.setOuterGain(gain);
    m_cachedConeGain = std::nullopt;
    return { };
}

void PannerNode::setConeOuterAngleForBindings(double angle)
{
    ASSERT(isMainThread());

    // This synchronizes with process().
    Locker locker { m_processLock };

    if (m_coneEffect.outerAngle() == angle)
        return;

    m_coneEffect.setOuterAngle(angle);
    m_cachedConeGain = std::nullopt;
}

void PannerNode::setConeInnerAngleForBindings(double angle)
{
    ASSERT(isMainThread());

    // This synchronizes with process().
    Locker locker { m_processLock };

    if (m_coneEffect.innerAngle() == angle)
        return;

    m_coneEffect.setInnerAngle(angle);
    m_cachedConeGain = std::nullopt;
}

ExceptionOr<void> PannerNode::setChannelCount(unsigned channelCount)
{
    ASSERT(isMainThread());

    if (channelCount > 2)
        return Exception { ExceptionCode::NotSupportedError, "PannerNode's channelCount cannot be greater than 2"_s };
    
    return AudioNode::setChannelCount(channelCount);
}

ExceptionOr<void> PannerNode::setChannelCountMode(ChannelCountMode mode)
{
    ASSERT(isMainThread());

    if (mode == ChannelCountMode::Max)
        return Exception { ExceptionCode::NotSupportedError, "PannerNode's channelCountMode cannot be max"_s };
    
    return AudioNode::setChannelCountMode(mode);
}

auto PannerNode::calculateAzimuthElevation(const FloatPoint3D& position, const FloatPoint3D& listenerPosition, const FloatPoint3D& listenerFront, const FloatPoint3D& listenerUp) -> AzimuthElevation
{
    // Calculate the source-listener vector
    FloatPoint3D sourceListener = position - listenerPosition;

    if (sourceListener.isZero()) {
        // degenerate case if source and listener are at the same point
        return { };
    }

    sourceListener.normalize();

    // Align axes
    FloatPoint3D listenerRight = listenerFront.cross(listenerUp);
    listenerRight.normalize();

    FloatPoint3D listenerFrontNorm = listenerFront;
    listenerFrontNorm.normalize();

    FloatPoint3D up = listenerRight.cross(listenerFrontNorm);

    float upProjection = sourceListener.dot(up);

    FloatPoint3D projectedSource = sourceListener - upProjection * up;
    projectedSource.normalize();

    double azimuth = rad2deg(std::acos(std::clamp(projectedSource.dot(listenerRight), -1.0f, 1.0f)));
    fixNANs(azimuth); // avoid illegal values

    // Source  in front or behind the listener
    double frontBack = projectedSource.dot(listenerFrontNorm);
    if (frontBack < 0.0)
        azimuth = 360.0 - azimuth;

    // Make azimuth relative to "front" and not "right" listener vector
    if ((azimuth >= 0.0) && (azimuth <= 270.0))
        azimuth = 90.0 - azimuth;
    else
        azimuth = 450.0 - azimuth;

    // Elevation
    double elevation = 90.0 - 180.0 * acos(sourceListener.dot(up)) / piDouble;
    fixNANs(elevation); // avoid illegal values

    if (elevation > 90.0)
        elevation = 180.0 - elevation;
    else if (elevation < -90.0)
        elevation = -180.0 - elevation;

    return { azimuth, elevation };
}

auto PannerNode::azimuthElevation() -> const AzimuthElevation&
{
    ASSERT(context().isAudioThread());
    auto& listener = this->listener();
    if (!m_cachedAzimuthElevation)
        m_cachedAzimuthElevation = calculateAzimuthElevation(position(), listener.position(), listener.orientation(), listener.upVector());
    return *m_cachedAzimuthElevation;
}

bool PannerNode::requiresTailProcessing() const
{
    if (!m_processLock.tryLock())
        return true;
    Locker locker { AdoptLock, m_processLock };
    // If there's no internal panner method set up yet, assume we require tail
    // processing in case the HRTF panner is set later, which does require tail
    // processing.
    return !m_panner || m_panner->requiresTailProcessing();
}

float PannerNode::calculateDistanceConeGain(const FloatPoint3D& sourcePosition, const FloatPoint3D& orientation, const FloatPoint3D& listenerPosition, const DistanceEffect& distanceEffect, const ConeEffect& coneEffect)
{
    double listenerDistance = sourcePosition.distanceTo(listenerPosition);
    double distanceGain = distanceEffect.gain(listenerDistance);
    double coneGain = coneEffect.gain(sourcePosition, orientation, listenerPosition);

    return float(distanceGain * coneGain);
}

float PannerNode::distanceConeGain()
{
    ASSERT(context().isAudioThread());
    if (!m_cachedConeGain)
        m_cachedConeGain = calculateDistanceConeGain(position(), orientation(), listener().position(), m_distanceEffect, m_coneEffect);
    return *m_cachedConeGain;
}

double PannerNode::tailTime() const
{
    if (!m_processLock.tryLock())
        return std::numeric_limits<double>::infinity();
    Locker locker { AdoptLock, m_processLock };
    return m_panner ? m_panner->tailTime() : 0;
}

double PannerNode::latencyTime() const
{
    if (!m_processLock.tryLock())
        return std::numeric_limits<double>::infinity();
    Locker locker { AdoptLock, m_processLock };
    return m_panner ? m_panner->latencyTime() : 0;
}

void PannerNode::invalidateCachedPropertiesIfNecessary()
{
    auto lastPosition = std::exchange(m_lastPosition, position());
    bool hasPositionChanged = m_lastPosition != lastPosition;
    auto lastOrientation = std::exchange(m_lastOrientation, position());
    bool hasOrientationChanged = m_lastOrientation != lastOrientation;
    auto& listener = this->listener();

    if (hasPositionChanged || listener.isPositionDirty() || listener.isOrientationDirty() || listener.isUpVectorDirty())
        m_cachedAzimuthElevation = std::nullopt;

    if (hasPositionChanged || hasOrientationChanged || listener.isPositionDirty())
        m_cachedConeGain = std::nullopt;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
