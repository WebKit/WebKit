/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DeviceMotionData : public RefCounted<DeviceMotionData> {
public:
    class Acceleration : public RefCounted<DeviceMotionData::Acceleration> {
    public:
        static Ref<Acceleration> create()
        {
            return adoptRef(*new Acceleration);
        }
        static Ref<Acceleration> create(Optional<double> x, Optional<double> y, Optional<double> z)
        {
            return adoptRef(*new Acceleration(x, y, z));
        }

        Optional<double> x() const { return m_x; }
        Optional<double> y() const { return m_y; }
        Optional<double> z() const { return m_z; }

    private:
        Acceleration() = default;
        Acceleration(Optional<double> x, Optional<double> y, Optional<double> z)
            : m_x(x)
            , m_y(y)
            , m_z(z)
        {
        }

        Optional<double> m_x;
        Optional<double> m_y;
        Optional<double> m_z;
    };

    class RotationRate : public RefCounted<DeviceMotionData::RotationRate> {
    public:
        static Ref<RotationRate> create()
        {
            return adoptRef(*new RotationRate);
        }
        static Ref<RotationRate> create(Optional<double> alpha, Optional<double> beta, Optional<double> gamma)
        {
            return adoptRef(*new RotationRate(alpha, beta, gamma));
        }

        Optional<double> alpha() const { return m_alpha; }
        Optional<double> beta() const { return m_beta; }
        Optional<double> gamma() const { return m_gamma; }

    private:
        RotationRate() = default;
        RotationRate(Optional<double> alpha, Optional<double> beta, Optional<double> gamma)
            : m_alpha(alpha)
            , m_beta(beta)
            , m_gamma(gamma)
        {
        }

        Optional<double> m_alpha;
        Optional<double> m_beta;
        Optional<double> m_gamma;
    };

    WEBCORE_EXPORT static Ref<DeviceMotionData> create();
    WEBCORE_EXPORT static Ref<DeviceMotionData> create(RefPtr<Acceleration>&&, RefPtr<Acceleration>&& accelerationIncludingGravity, RefPtr<RotationRate>&&, Optional<double> interval);

    const Acceleration* acceleration() const { return m_acceleration.get(); }
    const Acceleration* accelerationIncludingGravity() const { return m_accelerationIncludingGravity.get(); }
    const RotationRate* rotationRate() const { return m_rotationRate.get(); }
    
    Optional<double> interval() const { return m_interval; }

private:
    DeviceMotionData() = default;
    DeviceMotionData(RefPtr<Acceleration>&&, RefPtr<Acceleration>&& accelerationIncludingGravity, RefPtr<RotationRate>&&, Optional<double> interval);

    RefPtr<Acceleration> m_acceleration;
    RefPtr<Acceleration> m_accelerationIncludingGravity;
    RefPtr<RotationRate> m_rotationRate;
    Optional<double> m_interval;
};

} // namespace WebCore
