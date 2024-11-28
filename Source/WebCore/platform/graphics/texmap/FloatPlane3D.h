/*
 * Copyright (C) 2024 Jani Hautakangas <jani@kodegood.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatPoint3D.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FloatPlane3D {
    WTF_MAKE_TZONE_ALLOCATED(FloatPlane3D);
public:
    FloatPlane3D(const FloatPoint3D&, const FloatPoint3D&);

    const FloatPoint3D& normal() const { return m_normal; }

    // Getter for the distance from the origin (plane constant d)
    float distanceConstant() const { return m_distanceConstant; }

    // Signed distance. The sign of the return value is positive
    // if the point is on the front side of the plane, negative if the
    // point is on the back side, and zero if the point is on the plane.
    float distanceToPoint(const FloatPoint3D& point) const
    {
        return m_normal.dot(point) - m_distanceConstant;
    }

private:
    FloatPoint3D m_normal;
    float m_distanceConstant;
};

} // namespace WebCore
