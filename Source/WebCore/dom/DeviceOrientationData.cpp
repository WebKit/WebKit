/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "DeviceOrientationData.h"

namespace WebCore {

#if PLATFORM(IOS_FAMILY)

// FIXME: We should reconcile the iOS and OpenSource differences.
Ref<DeviceOrientationData> DeviceOrientationData::create(std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<double> compassHeading, std::optional<double> compassAccuracy)
{
    return adoptRef(*new DeviceOrientationData(alpha, beta, gamma, compassHeading, compassAccuracy));
}

DeviceOrientationData::DeviceOrientationData(std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<double> compassHeading, std::optional<double> compassAccuracy)
    : m_alpha(alpha)
    , m_beta(beta)
    , m_gamma(gamma)
    , m_compassHeading(compassHeading)
    , m_compassAccuracy(compassAccuracy)
{
}

#else

Ref<DeviceOrientationData> DeviceOrientationData::create(std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<bool> absolute)
{
    return adoptRef(*new DeviceOrientationData(alpha, beta, gamma, absolute));
}

DeviceOrientationData::DeviceOrientationData(std::optional<double> alpha, std::optional<double> beta, std::optional<double> gamma, std::optional<bool> absolute)
    : m_alpha(alpha)
    , m_beta(beta)
    , m_gamma(gamma)
    , m_absolute(absolute)
{
}

#endif

} // namespace WebCore
