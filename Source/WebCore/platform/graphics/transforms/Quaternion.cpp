/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Quaternion.h"

#include <cmath>

namespace WebCore {

// Perform a spherical linear interpolation between the two
// passed quaternions with 0 <= t <= 1.

Quaternion Quaternion::slerp(const Quaternion& other, double t)
{
    const double kEpsilon = 1e-5;
    Quaternion copy = *this;

    double cosHalfAngle = copy.x * other.x + copy.y * other.y + copy.z * other.z + copy.w * other.w;

    if (cosHalfAngle < 0.0) {
        copy.x = -copy.x;
        copy.y = -copy.y;
        copy.z = -copy.z;
        copy.w = -copy.w;
        cosHalfAngle = -cosHalfAngle;
    }

    if (cosHalfAngle > 1)
        cosHalfAngle = 1;

    double sinHalfAngle = std::sqrt(1.0 - cosHalfAngle * cosHalfAngle);
    if (sinHalfAngle < kEpsilon) {
        // Quaternions share common axis and angle.
        return *this;
    }

    double halfAngle = std::acos(cosHalfAngle);
    double scale = std::sin((1 - t) * halfAngle) / sinHalfAngle;
    double invscale = std::sin(t * halfAngle) / sinHalfAngle;

    return { copy.x * scale + other.x * invscale, copy.y * scale + other.y * invscale, copy.z * scale + other.z * invscale, copy.w * scale + other.w * invscale };
}

// Compute quaternion multiplication
Quaternion Quaternion::accumulate(const Quaternion& other)
{
    return { w * other.x + x * other.w + y * other.z - z * other.y,
        w * other.y - x * other.z + y * other.w + z * other.x,
        w * other.z + x * other.y - y * other.x + z * other.w,
        w * other.w - x * other.x - y * other.y - z * other.z };
}

Quaternion Quaternion::interpolate(const Quaternion& other, double progress, CompositeOperation compositeOperation)
{
    if (compositeOperation == CompositeOperation::Accumulate)
        return accumulate(other);
    return slerp(other, progress);
}

} // namespace WebCore
