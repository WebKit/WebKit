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

#pragma once

#if ENABLE(WEB_AUDIO)

#include "AudioContext.h"
#include "AudioListener.h"
#include "AudioNode.h"
#include "AudioParam.h"
#include "Cone.h"
#include "Distance.h"
#include "FloatPoint3D.h"
#include "Panner.h"
#include "PannerOptions.h"
#include <memory>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

namespace WebCore {

class HRTFDatabaseLoader;
class BaseAudioContext;

class PannerNodeBase : public AudioNode {
public:
    virtual ~PannerNodeBase() = default;

    virtual float dopplerRate() = 0;

protected:
    PannerNodeBase(BaseAudioContext&);
};

// PannerNode is an AudioNode with one input and one output.
// It positions a sound in 3D space, with the exact effect dependent on the panning model.
// It has a position and an orientation in 3D space which is relative to the position and orientation of the context's AudioListener.
// A distance effect will attenuate the gain as the position moves away from the listener.
// A cone effect will attenuate the gain as the orientation moves away from the listener.
// All of these effects follow the OpenAL specification very closely.

class PannerNode final : public PannerNodeBase {
    WTF_MAKE_ISO_ALLOCATED(PannerNode);
public:
    static ExceptionOr<Ref<PannerNode>> create(BaseAudioContext&, const PannerOptions& = { });

    virtual ~PannerNode();

    // AudioNode
    void process(size_t framesToProcess) override;
    void pullInputs(size_t framesToProcess) override;
    void reset() override;
    void initialize() override;
    void uninitialize() override;

    // Listener
    AudioListener& listener();

    // Panning model
    PanningModelType panningModel() const { return m_panningModel; }
    void setPanningModel(PanningModelType);

    // Position
    FloatPoint3D position() const;
    ExceptionOr<void> setPosition(float x, float y, float z);
    AudioParam& positionX() { return m_positionX.get(); }
    AudioParam& positionY() { return m_positionY.get(); }
    AudioParam& positionZ() { return m_positionZ.get(); }

    // Orientation
    FloatPoint3D orientation() const;
    ExceptionOr<void> setOrientation(float x, float y, float z);
    AudioParam& orientationX() { return m_orientationX.get(); }
    AudioParam& orientationY() { return m_orientationY.get(); }
    AudioParam& orientationZ() { return m_orientationZ.get(); }

    // Distance parameters
    DistanceModelType distanceModel() const;
    void setDistanceModel(DistanceModelType);

    double refDistance() const { return m_distanceEffect.refDistance(); }
    ExceptionOr<void> setRefDistance(double);

    double maxDistance() const { return m_distanceEffect.maxDistance(); }
    ExceptionOr<void> setMaxDistance(double);

    double rolloffFactor() const { return m_distanceEffect.rolloffFactor(); }
    ExceptionOr<void> setRolloffFactor(double);

    // Sound cones - angles in degrees
    double coneInnerAngle() const { return m_coneEffect.innerAngle(); }
    void setConeInnerAngle(double);

    double coneOuterAngle() const { return m_coneEffect.outerAngle(); }
    void setConeOuterAngle(double);

    double coneOuterGain() const { return m_coneEffect.outerGain(); }
    ExceptionOr<void> setConeOuterGain(double);
    
    ExceptionOr<void> setChannelCount(unsigned) final;
    ExceptionOr<void> setChannelCountMode(ChannelCountMode) final;

    void azimuthElevation(double* outAzimuth, double* outElevation);
    float dopplerRate() final;

    double tailTime() const override { return m_panner ? m_panner->tailTime() : 0; }
    double latencyTime() const override { return m_panner ? m_panner->latencyTime() : 0; }

private:
    PannerNode(BaseAudioContext&, const PannerOptions&);

    void calculateAzimuthElevation(double* outAzimuth, double* outElevation, const FloatPoint3D& position, const FloatPoint3D& listenerPosition, const FloatPoint3D& listenerForward, const FloatPoint3D& listenerUp);
    float calculateDistanceConeGain(const FloatPoint3D& position, const FloatPoint3D& orientation, const FloatPoint3D& listenerPosition);

    // Returns the combined distance and cone gain attenuation.
    float distanceConeGain();

    // Notifies any AudioBufferSourceNodes connected to us either directly or indirectly about our existence.
    // This is in order to handle the pitch change necessary for the doppler shift.
    void notifyAudioSourcesConnectedToNode(AudioNode*, HashSet<AudioNode*>& visitedNodes);

    void processSampleAccurateValues(AudioBus* destination, const AudioBus* source, size_t framesToProcess);
    bool hasSampleAccurateValues() const;
    bool shouldUseARate() const;

    std::unique_ptr<Panner> m_panner;
    PanningModelType m_panningModel;

    // Gain
    DistanceEffect m_distanceEffect;
    ConeEffect m_coneEffect;
    float m_lastGain { -1.0 };
    
    Ref<AudioParam> m_positionX;
    Ref<AudioParam> m_positionY;
    Ref<AudioParam> m_positionZ;
    
    Ref<AudioParam> m_orientationX;
    Ref<AudioParam> m_orientationY;
    Ref<AudioParam> m_orientationZ;

    // HRTF Database loader
    RefPtr<HRTFDatabaseLoader> m_hrtfDatabaseLoader;

    unsigned m_connectionCount { 0 };

    // Synchronize process() with setting of the panning model, source's location
    // information, listener, distance parameters and sound cones.
    mutable Lock m_pannerMutex;
};

} // namespace WebCore

#endif
