/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

// PannerNode is an AudioNode with one input and one output.
// It positions a sound in 3D space, with the exact effect dependent on the panning model.
// It has a position and an orientation in 3D space which is relative to the position and orientation of the context's AudioListener.
// A distance effect will attenuate the gain as the position moves away from the listener.
// A cone effect will attenuate the gain as the orientation moves away from the listener.
// All of these effects follow the OpenAL specification very closely.

class PannerNode final : public AudioNode {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(PannerNode);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PannerNode);
public:
    static ExceptionOr<Ref<PannerNode>> create(BaseAudioContext&, const PannerOptions& = { });

    virtual ~PannerNode();

    // AudioNode
    void process(size_t framesToProcess) override;
    void processOnlyAudioParams(size_t framesToProcess) final;

    // Listener
    AudioListener& listener();

    // Panning model
    PanningModelType panningModelForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_panningModel; }
    void setPanningModelForBindings(PanningModelType);

    // Position
    ExceptionOr<void> setPosition(float x, float y, float z);
    AudioParam& positionX() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_positionX.get(); }
    AudioParam& positionY() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_positionY.get(); }
    AudioParam& positionZ() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_positionZ.get(); }

    // Orientation
    ExceptionOr<void> setOrientation(float x, float y, float z);
    AudioParam& orientationX() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_orientationX.get(); }
    AudioParam& orientationY() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_orientationY.get(); }
    AudioParam& orientationZ() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_orientationZ.get(); }

    // Distance parameters
    DistanceModelType distanceModelForBindings() const;
    void setDistanceModelForBindings(DistanceModelType);

    double refDistanceForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_distanceEffect.refDistance(); }
    ExceptionOr<void> setRefDistanceForBindings(double);

    double maxDistanceForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_distanceEffect.maxDistance(); }
    ExceptionOr<void> setMaxDistanceForBindings(double);

    double rolloffFactorForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_distanceEffect.rolloffFactor(); }
    ExceptionOr<void> setRolloffFactorForBindings(double);

    // Sound cones - angles in degrees
    double coneInnerAngleForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_coneEffect.innerAngle(); }
    void setConeInnerAngleForBindings(double);

    double coneOuterAngleForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_coneEffect.outerAngle(); }
    void setConeOuterAngleForBindings(double);

    double coneOuterGainForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_coneEffect.outerGain(); }
    ExceptionOr<void> setConeOuterGainForBindings(double);
    
    ExceptionOr<void> setChannelCount(unsigned) final;
    ExceptionOr<void> setChannelCountMode(ChannelCountMode) final;

    double tailTime() const final;
    double latencyTime() const final;

private:
    PannerNode(BaseAudioContext&, const PannerOptions&);

    struct AzimuthElevation {
        double azimuth { 0. };
        double elevation { 0. };
    };
    static AzimuthElevation calculateAzimuthElevation(const FloatPoint3D& position, const FloatPoint3D& listenerPosition, const FloatPoint3D& listenerForward, const FloatPoint3D& listenerUp);
    static float calculateDistanceConeGain(const FloatPoint3D& position, const FloatPoint3D& orientation, const FloatPoint3D& listenerPosition, const DistanceEffect&, const ConeEffect&);

    // Returns the combined distance and cone gain attenuation.
    float distanceConeGain() WTF_REQUIRES_LOCK(m_processLock);

    bool requiresTailProcessing() const final;

    void invalidateCachedPropertiesIfNecessary() WTF_REQUIRES_LOCK(m_processLock);

    const AzimuthElevation& azimuthElevation() WTF_REQUIRES_LOCK(m_processLock);
    void processSampleAccurateValues(AudioBus& destination, const AudioBus& source, size_t framesToProcess) WTF_REQUIRES_LOCK(m_processLock);
    bool hasSampleAccurateValues() const WTF_REQUIRES_LOCK(m_processLock);
    bool shouldUseARate() const WTF_REQUIRES_LOCK(m_processLock);

    FloatPoint3D position() const WTF_REQUIRES_LOCK(m_processLock);
    FloatPoint3D orientation() const WTF_REQUIRES_LOCK(m_processLock);

    const Ref<HRTFDatabaseLoader> m_hrtfDatabaseLoader;
    PanningModelType m_panningModel WTF_GUARDED_BY_LOCK(m_processLock);
    std::unique_ptr<Panner> m_panner WTF_GUARDED_BY_LOCK(m_processLock);

    // Gain
    DistanceEffect m_distanceEffect WTF_GUARDED_BY_LOCK(m_processLock);
    ConeEffect m_coneEffect WTF_GUARDED_BY_LOCK(m_processLock);
    
    const Ref<AudioParam> m_positionX WTF_GUARDED_BY_LOCK(m_processLock);
    const Ref<AudioParam> m_positionY WTF_GUARDED_BY_LOCK(m_processLock);
    const Ref<AudioParam> m_positionZ WTF_GUARDED_BY_LOCK(m_processLock);
    
    const Ref<AudioParam> m_orientationX WTF_GUARDED_BY_LOCK(m_processLock);
    const Ref<AudioParam> m_orientationY WTF_GUARDED_BY_LOCK(m_processLock);
    const Ref<AudioParam> m_orientationZ WTF_GUARDED_BY_LOCK(m_processLock);

    mutable std::optional<AzimuthElevation> m_cachedAzimuthElevation WTF_GUARDED_BY_LOCK(m_processLock);
    mutable std::optional<float> m_cachedConeGain WTF_GUARDED_BY_LOCK(m_processLock);
    FloatPoint3D m_lastPosition WTF_GUARDED_BY_LOCK(m_processLock);
    FloatPoint3D m_lastOrientation WTF_GUARDED_BY_LOCK(m_processLock);

    // Synchronize process() with setting of the panning model, source's location
    // information, listener, distance parameters and sound cones.
    mutable Lock m_processLock;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_AUDIONODE(PannerNode, NodeTypePanner);

#endif
