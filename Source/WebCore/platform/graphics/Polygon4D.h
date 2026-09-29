/*
 * Copyright (C) 2026 Igalia S.L.
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

#include <span>
#include <utility>
#include <wtf/Vector.h>

namespace WebCore {

class FloatRect;
class TransformationMatrix;

struct Point4D {
    double x { 0.0 };
    double y { 0.0 };
    double z { 0.0 };
    double w { 0.0 };
};

inline Point4D operator+(const Point4D& s, const Point4D& t)
{
    return { s.x + t.x, s.y + t.y, s.z + t.z, s.w + t.w };
}

inline Point4D operator-(const Point4D& s, const Point4D& t)
{
    return { s.x - t.x, s.y - t.y, s.z - t.z, s.w - t.w };
}

inline Point4D operator*(double s, const Point4D& t)
{
    return { s * t.x, s * t.y, s * t.z, s * t.w };
}

inline Point4D operator/(const Point4D& s, double t)
{
    return { s.x / t, s.y / t, s.z / t, s.w / t };
}

inline double signedDistanceToPlane(const Point4D& plane, const Point4D& point)
{
    return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w * point.w;
}

// A convex polygon in homogeneous coordinates, clipped to the half space in front of the camera.
// See https://en.wikipedia.org/wiki/Homogeneous_coordinates.
class Polygon4D {
public:
    using Vertices = Vector<Point4D, 5>;

    // Classifying and splitting treat smaller distances as zero.
    static constexpr double planeTolerance = 0.05;

    Polygon4D(const FloatRect&, const TransformationMatrix&);

    static Vertices clipToFrontOfCamera(const FloatRect&, const TransformationMatrix&);
    static Vertices clipToPlane(std::span<const Point4D>, const Point4D& plane);

    unsigned numberOfVertices() const { return m_vertices.size(); }
    const Point4D& vertexAt(unsigned index) const LIFETIME_BOUND { return m_vertices[index]; }
    const Point4D& plane() const LIFETIME_BOUND { return m_plane; }

    std::pair<Polygon4D, Polygon4D> split(const Point4D& plane) const;

private:
    Polygon4D(Vertices&&, const Point4D& plane);

    Vertices m_vertices;
    Point4D m_plane;
};

} // namespace WebCore
