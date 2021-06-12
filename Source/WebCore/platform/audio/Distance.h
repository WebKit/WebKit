/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef Distance_h
#define Distance_h

namespace WebCore {

// Distance models are defined according to the OpenAL specification:
// http://connect.creativelabs.com/openal/Documentation/OpenAL%201.1%20Specification.htm.

enum class DistanceModelType {
    Linear,
    Inverse,
    Exponential
};

class DistanceEffect final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DistanceEffect();

    // Returns scalar gain for the given distance the current distance model is used
    double gain(double distance);

    DistanceModelType model() const { return m_model; }

    void setModel(DistanceModelType model, bool clamped)
    {
        m_model = model;
        m_isClamped = clamped;
    }

    // Distance params
    void setRefDistance(double refDistance) { m_refDistance = refDistance; }
    void setMaxDistance(double maxDistance) { m_maxDistance = maxDistance; }
    void setRolloffFactor(double rolloffFactor) { m_rolloffFactor = rolloffFactor; }

    double refDistance() const { return m_refDistance; }
    double maxDistance() const { return m_maxDistance; }
    double rolloffFactor() const { return m_rolloffFactor; }

protected:
    double linearGain(double distance);
    double inverseGain(double distance);
    double exponentialGain(double distance);

    DistanceModelType m_model { DistanceModelType::Inverse };
    bool m_isClamped { true };
    double m_refDistance { 1 };
    double m_maxDistance { 10000 };
    double m_rolloffFactor { 1 };
};

} // namespace WebCore

#endif // Distance_h
