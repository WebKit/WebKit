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
#include "ChannelCountMode.h"
#include "HRTFDatabaseLoader.h"
#include "HRTFPanner.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PannerNode);

static void fixNANs(double &x)
{
    if (std::isnan(x) || std::isinf(x))
        x = 0.0;
}

PannerNodeBase::PannerNodeBase(BaseAudioContext& context)
    : AudioNode(context)
{
}

ExceptionOr<Ref<PannerNode>> PannerNode::create(BaseAudioContext& context, const PannerOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    auto panner = adoptRef(*new PannerNode(context, options));

    auto result = panner->handleAudioNodeOptions(options, { 2, ChannelCountMode::ClampedMax, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    result = panner->setMaxDistance(options.maxDistance);
    if (result.hasException())
        return result.releaseException();

    result = panner->setRefDistance(options.refDistance);
    if (result.hasException())
        return result.releaseException();

    result = panner->setRolloffFactor(options.rolloffFactor);
    if (result.hasException())
        return result.releaseException();

    result = panner->setConeOuterGain(options.coneOuterGain);
    if (result.hasException())
        return result.releaseException();

    return panner;
}

PannerNode::PannerNode(BaseAudioContext& context, const PannerOptions& options)
    : PannerNodeBase(context)
    , m_panningModel(options.panningModel)
    , m_positionX(AudioParam::create(context, "positionX"_s, options.positionX, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_positionY(AudioParam::create(context, "positionY"_s, options.positionY, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_positionZ(AudioParam::create(context, "positionZ"_s, options.positionZ, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_orientationX(AudioParam::create(context, "orientationX"_s, options.orientationX, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_orientationY(AudioParam::create(context, "orientationY"_s, options.orientationY, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_orientationZ(AudioParam::create(context, "orientationZ"_s, options.orientationZ, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    // Load the HRTF database asynchronously so we don't block the Javascript thread while creating the HRTF database.
    , m_hrtfDatabaseLoader(HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(context.sampleRate()))
{
    setNodeType(NodeTypePanner);

    setDistanceModel(options.distanceModel);
    setConeInnerAngle(options.coneInnerAngle);
    setConeOuterAngle(options.coneOuterAngle);

    addInput(makeUnique<AudioNodeInput>(this));
    addOutput(makeUnique<AudioNodeOutput>(this, 2));

    initialize();
}

PannerNode::~PannerNode()
{
    uninitialize();
}

void PannerNode::pullInputs(size_t framesToProcess)
{
    // We override pullInputs(), so we can detect new AudioSourceNodes which have connected to us when new connections are made.
    // These AudioSourceNodes need to be made aware of our existence in order to handle doppler shift pitch changes.
    if (m_connectionCount != context().connectionCount()) {
        m_connectionCount = context().connectionCount();

        // Recursively go through all nodes connected to us.
        HashSet<AudioNode*> visitedNodes;
        notifyAudioSourcesConnectedToNode(this, visitedNodes);
    }
    
    AudioNode::pullInputs(framesToProcess);
}

void PannerNode::process(size_t framesToProcess)
{
    AudioBus* destination = output(0)->bus();

    if (!isInitialized() || !input(0)->isConnected() || !m_panner.get()) {
        destination->zero();
        return;
    }

    AudioBus* source = input(0)->bus();
    if (!source) {
        destination->zero();
        return;
    }

    // HRTFDatabase should be loaded before proceeding for offline audio context when panningModel() is "HRTF".
    if (panningModel() == PanningModelType::HRTF && !m_hrtfDatabaseLoader->isLoaded()) {
        if (context().isOfflineContext())
            m_hrtfDatabaseLoader->waitForLoaderThreadCompletion();
        else {
            destination->zero();
            return;
        }
    }

    // The audio thread can't block on this lock, so we use std::try_to_lock instead.
    std::unique_lock<Lock> lock(m_pannerMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        // Too bad - The try_lock() failed. We must be in the middle of changing the panner.
        destination->zero();
        return;
    }

    // Apply the panning effect.
    double azimuth;
    double elevation;
    getAzimuthElevation(&azimuth, &elevation);
    m_panner->pan(azimuth, elevation, source, destination, framesToProcess);

    // Get the distance and cone gain.
    double totalGain = distanceConeGain();

    // Snap to desired gain at the beginning.
    if (m_lastGain == -1.0)
        m_lastGain = totalGain;

    // Apply gain in-place with de-zippering.
    destination->copyWithGainFrom(*destination, &m_lastGain, totalGain);
}

void PannerNode::reset()
{
    m_lastGain = -1.0; // force to snap to initial gain
    if (m_panner.get())
        m_panner->reset();
}

void PannerNode::initialize()
{
    if (isInitialized())
        return;

    m_panner = Panner::create(m_panningModel, sampleRate(), m_hrtfDatabaseLoader.get());

    AudioNode::initialize();
}

void PannerNode::uninitialize()
{
    if (!isInitialized())
        return;
        
    m_panner = nullptr;
    AudioNode::uninitialize();
}

AudioListener& PannerNode::listener()
{
    return context().listener();
}

void PannerNode::setPanningModel(PanningModelType model)
{
    if (!m_panner.get() || model != m_panningModel) {
        // This synchronizes with process().
        auto locker = holdLock(m_pannerMutex);

        m_panner = Panner::create(model, sampleRate(), m_hrtfDatabaseLoader.get());
        m_panningModel = model;
    }
}

FloatPoint3D PannerNode::position() const
{
    return FloatPoint3D(m_positionX->value(), m_positionY->value(), m_positionZ->value());
}

void PannerNode::setPosition(float x, float y, float z)
{
    m_positionX->setValue(x);
    m_positionY->setValue(y);
    m_positionZ->setValue(z);
}

FloatPoint3D PannerNode::orientation() const
{
    return FloatPoint3D(m_orientationX->value(), m_orientationY->value(), m_orientationZ->value());
}

void PannerNode::setOrientation(float x, float y, float z)
{
    m_orientationX->setValue(x);
    m_orientationY->setValue(y);
    m_orientationZ->setValue(z);
}

DistanceModelType PannerNode::distanceModel() const
{
    return const_cast<PannerNode*>(this)->m_distanceEffect.model();
}

void PannerNode::setDistanceModel(DistanceModelType model)
{
    m_distanceEffect.setModel(model, true);
}

ExceptionOr<void> PannerNode::setRefDistance(double refDistance)
{
    if (refDistance < 0)
        return Exception { RangeError, "refDistance cannot be set to a negative value"_s };
    
    m_distanceEffect.setRefDistance(refDistance);
    return { };
}

ExceptionOr<void> PannerNode::setMaxDistance(double maxDistance)
{
    if (maxDistance <= 0)
        return Exception { RangeError, "maxDistance cannot be set to a non-positive value"_s };
    
    m_distanceEffect.setMaxDistance(maxDistance);
    return { };
}

ExceptionOr<void> PannerNode::setRolloffFactor(double rolloffFactor)
{
    if (rolloffFactor < 0)
        return Exception { RangeError, "rolloffFactor cannot be set to a negative value"_s };
    
    m_distanceEffect.setRolloffFactor(rolloffFactor);
    return { };
}

ExceptionOr<void> PannerNode::setConeOuterGain(double gain)
{
    if (gain < 0 || gain > 1)
        return Exception { InvalidStateError, "coneOuterGain must be in [0, 1]"_s };
    
    m_coneEffect.setOuterGain(gain);
    return { };
}

ExceptionOr<void> PannerNode::setChannelCount(unsigned channelCount)
{
    if (channelCount > 2)
        return Exception { NotSupportedError, "PannerNode's channelCount cannot be greater than 2"_s };
    
    return AudioNode::setChannelCount(channelCount);
}

ExceptionOr<void> PannerNode::setChannelCountMode(ChannelCountMode mode)
{
    if (mode == ChannelCountMode::Max)
        return Exception { NotSupportedError, "PannerNode's channelCountMode cannot be max"_s };
    
    return AudioNode::setChannelCountMode(mode);
}

void PannerNode::getAzimuthElevation(double* outAzimuth, double* outElevation)
{
    // FIXME: we should cache azimuth and elevation (if possible), so we only re-calculate if a change has been made.

    double azimuth = 0.0;

    // Calculate the source-listener vector
    FloatPoint3D listenerPosition = listener().position();
    FloatPoint3D sourceListener = position() - listenerPosition;

    if (sourceListener.isZero()) {
        // degenerate case if source and listener are at the same point
        *outAzimuth = 0.0;
        *outElevation = 0.0;
        return;
    }

    sourceListener.normalize();

    // Align axes
    FloatPoint3D listenerFront = listener().orientation();
    FloatPoint3D listenerUp = listener().upVector();
    FloatPoint3D listenerRight = listenerFront.cross(listenerUp);
    listenerRight.normalize();

    FloatPoint3D listenerFrontNorm = listenerFront;
    listenerFrontNorm.normalize();

    FloatPoint3D up = listenerRight.cross(listenerFrontNorm);

    float upProjection = sourceListener.dot(up);

    FloatPoint3D projectedSource = sourceListener - upProjection * up;
    projectedSource.normalize();

    azimuth = 180.0 * acos(projectedSource.dot(listenerRight)) / piDouble;
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

    if (outAzimuth)
        *outAzimuth = azimuth;
    if (outElevation)
        *outElevation = elevation;
}

float PannerNode::dopplerRate()
{
    return 1.0f;
}

float PannerNode::distanceConeGain()
{
    FloatPoint3D listenerPosition = listener().position();
    FloatPoint3D sourcePosition = position();

    double listenerDistance = sourcePosition.distanceTo(listenerPosition);
    double distanceGain = m_distanceEffect.gain(listenerDistance);

    // FIXME: could optimize by caching coneGain
    double coneGain = m_coneEffect.gain(sourcePosition, orientation(), listenerPosition);

    return float(distanceGain * coneGain);
}

void PannerNode::notifyAudioSourcesConnectedToNode(AudioNode* node, HashSet<AudioNode*>& visitedNodes)
{
    ASSERT(node);
    if (!node)
        return;
        
    // First check if this node is an AudioBufferSourceNode. If so, let it know about us so that doppler shift pitch can be taken into account.
    if (node->nodeType() == NodeTypeAudioBufferSource) {
        AudioBufferSourceNode* bufferSourceNode = reinterpret_cast<AudioBufferSourceNode*>(node);
        bufferSourceNode->setPannerNode(this);
    } else {    
        // Go through all inputs to this node.
        for (unsigned i = 0; i < node->numberOfInputs(); ++i) {
            AudioNodeInput* input = node->input(i);

            // For each input, go through all of its connections, looking for AudioBufferSourceNodes.
            for (unsigned j = 0; j < input->numberOfRenderingConnections(); ++j) {
                AudioNodeOutput* connectedOutput = input->renderingOutput(j);
                AudioNode* connectedNode = connectedOutput->node();
                if (visitedNodes.contains(connectedNode))
                    continue;

                visitedNodes.add(connectedNode);
                notifyAudioSourcesConnectedToNode(connectedNode, visitedNodes);
            }
        }
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
