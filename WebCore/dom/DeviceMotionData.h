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

#ifndef DeviceMotionData_h
#define DeviceMotionData_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DeviceMotionData : public RefCounted<DeviceMotionData> {
public:
    static PassRefPtr<DeviceMotionData> create();
    static PassRefPtr<DeviceMotionData> create(bool canProvideXAcceleration, double xAcceleration,
                                               bool canProvideYAcceleration, double yAcceleration,
                                               bool canProvideZAcceleration, double zAcceleration,
                                               bool canProvideXRotationRate, double xRotationRate,
                                               bool canProvideYRotationRate, double yRotationRate,
                                               bool canProvideZRotationRate, double zRotationRate,
                                               bool canProvideInterval, double interval);

    double xAcceleration() const { return m_xAcceleration; }
    double yAcceleration() const { return m_yAcceleration; }
    double zAcceleration() const { return m_zAcceleration; }
    double xRotationRate() const { return m_xRotationRate; }
    double yRotationRate() const { return m_yRotationRate; }
    double zRotationRate() const { return m_zRotationRate; }
    double interval() const { return m_interval; }

    bool canProvideXAcceleration() const { return m_canProvideXAcceleration; }
    bool canProvideYAcceleration() const { return m_canProvideYAcceleration; }
    bool canProvideZAcceleration() const { return m_canProvideZAcceleration; }
    bool canProvideXRotationRate() const { return m_canProvideXRotationRate; }
    bool canProvideYRotationRate() const { return m_canProvideYRotationRate; }
    bool canProvideZRotationRate() const { return m_canProvideZRotationRate; }
    bool canProvideInterval() const { return m_canProvideInterval; }

private:
    DeviceMotionData();
    DeviceMotionData(bool canProvideXAcceleration, double xAcceleration,
                     bool canProvideYAcceleration, double yAcceleration,
                     bool canProvideZAcceleration, double zAcceleration,
                     bool canProvideXRotationRate, double xRotationRate,
                     bool canProvideYRotationRate, double yRotationRate,
                     bool canProvideZRotationRate, double zRotationRate,
                     bool canProvideInterval, double interval);

    bool m_canProvideXAcceleration;
    bool m_canProvideYAcceleration;
    bool m_canProvideZAcceleration;
    bool m_canProvideXRotationRate;
    bool m_canProvideYRotationRate;
    bool m_canProvideZRotationRate;
    bool m_canProvideInterval;

    double m_xAcceleration;
    double m_yAcceleration;
    double m_zAcceleration;
    double m_xRotationRate;
    double m_yRotationRate;
    double m_zRotationRate;
    double m_interval;
};

} // namespace WebCore

#endif // DeviceMotionData_h
