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

#include "config.h"
#include "DeviceMotionData.h"

namespace WebCore {

PassRefPtr<DeviceMotionData> DeviceMotionData::create()
{
    return adoptRef(new DeviceMotionData);
}

PassRefPtr<DeviceMotionData> DeviceMotionData::create(bool canProvideXAcceleration, double xAcceleration,
                                                      bool canProvideYAcceleration, double yAcceleration,
                                                      bool canProvideZAcceleration, double zAcceleration,
                                                      bool canProvideXRotationRate, double xRotationRate,
                                                      bool canProvideYRotationRate, double yRotationRate,
                                                      bool canProvideZRotationRate, double zRotationRate,
                                                      bool canProvideInterval, double interval)
{
    return adoptRef(new DeviceMotionData(canProvideXAcceleration, xAcceleration,
                                         canProvideYAcceleration, yAcceleration,
                                         canProvideZAcceleration, zAcceleration,
                                         canProvideXRotationRate, xRotationRate,
                                         canProvideYRotationRate, yRotationRate,
                                         canProvideZRotationRate, zRotationRate,
                                         canProvideInterval, interval));
}

DeviceMotionData::DeviceMotionData()
    : m_canProvideXAcceleration(false)
    , m_canProvideYAcceleration(false)
    , m_canProvideZAcceleration(false)
    , m_canProvideXRotationRate(false)
    , m_canProvideYRotationRate(false)
    , m_canProvideZRotationRate(false)
    , m_canProvideInterval(false)
{
}

DeviceMotionData::DeviceMotionData(bool canProvideXAcceleration, double xAcceleration,
                                   bool canProvideYAcceleration, double yAcceleration,
                                   bool canProvideZAcceleration, double zAcceleration,
                                   bool canProvideXRotationRate, double xRotationRate,
                                   bool canProvideYRotationRate, double yRotationRate,
                                   bool canProvideZRotationRate, double zRotationRate,
                                   bool canProvideInterval, double interval)
    : m_canProvideXAcceleration(canProvideXAcceleration)
    , m_canProvideYAcceleration(canProvideYAcceleration)
    , m_canProvideZAcceleration(canProvideZAcceleration)
    , m_canProvideXRotationRate(canProvideXRotationRate)
    , m_canProvideYRotationRate(canProvideYRotationRate)
    , m_canProvideZRotationRate(canProvideZRotationRate)
    , m_canProvideInterval(canProvideInterval)
    , m_xAcceleration(xAcceleration)
    , m_yAcceleration(yAcceleration)
    , m_zAcceleration(zAcceleration)
    , m_xRotationRate(xRotationRate)
    , m_yRotationRate(yRotationRate)
    , m_zRotationRate(zRotationRate)
    , m_interval(interval)
{
}

} // namespace WebCore
