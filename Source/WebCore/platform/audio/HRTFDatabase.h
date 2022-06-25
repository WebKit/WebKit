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

#ifndef HRTFDatabase_h
#define HRTFDatabase_h

#include "HRTFElevation.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class HRTFKernel;

class HRTFDatabase final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(HRTFDatabase);
public:
    explicit HRTFDatabase(float sampleRate);

    // getKernelsFromAzimuthElevation() returns a left and right ear kernel, and an interpolated left and right frame delay for the given azimuth and elevation.
    // azimuthBlend must be in the range 0 -> 1.
    // Valid values for azimuthIndex are 0 -> HRTFElevation::NumberOfTotalAzimuths - 1 (corresponding to angles of 0 -> 360).
    // Valid values for elevationAngle are MinElevation -> MaxElevation.
    void getKernelsFromAzimuthElevation(double azimuthBlend, unsigned azimuthIndex, double elevationAngle, HRTFKernel* &kernelL, HRTFKernel* &kernelR, double& frameDelayL, double& frameDelayR);

    // Returns the number of different azimuth angles.
    static unsigned numberOfAzimuths() { return HRTFElevation::NumberOfTotalAzimuths; }

    float sampleRate() const { return m_sampleRate; }

    // Number of elevations loaded from resource.
    static const unsigned NumberOfRawElevations { 10 };

private:
    // Minimum and maximum elevation angles (inclusive) for a HRTFDatabase.
    static constexpr int MinElevation { -45 };
    static constexpr int MaxElevation { 90 };
    static constexpr unsigned RawElevationAngleSpacing { 15 };

    // Interpolates by this factor to get the total number of elevations from every elevation loaded from resource.
    static constexpr unsigned InterpolationFactor { 1 };
    
    // Total number of elevations after interpolation.
    static constexpr unsigned NumberOfTotalElevations { NumberOfRawElevations * InterpolationFactor };

    // Returns the index for the correct HRTFElevation given the elevation angle.
    static unsigned indexFromElevationAngle(double);

    Vector<std::unique_ptr<HRTFElevation>> m_elevations { NumberOfTotalElevations };
    float m_sampleRate;
};

} // namespace WebCore

#endif // HRTFDatabase_h
