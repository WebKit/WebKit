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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef Vector3_h
#define Vector3_h

#include <math.h>

namespace WebCore {

class Vector3 {
public:
    Vector3()
        : m_x(0.0)
        , m_y(0.0)
        , m_z(0.0)
    {
    }

    Vector3(double x, double y, double z)
        : m_x(x)
        , m_y(y)
        , m_z(z)
    {
    }

    Vector3(const float p[3])
        : m_x(p[0])
        , m_y(p[1])
        , m_z(p[2])
    {
    }

    Vector3(const double p[3])
        : m_x(p[0])
        , m_y(p[1])
        , m_z(p[2])
    {
    }

    double abs() const
    {
        return sqrt(m_x * m_x + m_y * m_y + m_z * m_z);
    }

    bool isZero() const
    {
        return !m_x && !m_y && !m_z;
    }

    void normalize()
    {
        double absValue = abs();
        if (!absValue)
            return;

        double k = 1.0 / absValue;
        m_x *= k;
        m_y *= k;
        m_z *= k;
    }

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

private:
    double m_x;
    double m_y;
    double m_z;
};

inline Vector3 operator+(const Vector3& v1, const Vector3& v2)
{
    return Vector3(v1.x() + v2.x(), v1.y() + v2.y(), v1.z() + v2.z());
}

inline Vector3 operator-(const Vector3& v1, const Vector3& v2)
{
    return Vector3(v1.x() - v2.x(), v1.y() - v2.y(), v1.z() - v2.z());
}

inline Vector3 operator*(double k, const Vector3& v)
{
    return Vector3(k * v.x(), k * v.y(), k * v.z());
}

inline Vector3 operator*(const Vector3& v, double k)
{
    return Vector3(k * v.x(), k * v.y(), k * v.z());
}

inline double dot(const Vector3& v1, const Vector3& v2)
{
    return v1.x() * v2.x() + v1.y() * v2.y() + v1.z() * v2.z();
}

inline Vector3 cross(const Vector3& v1, const Vector3& v2)
{
    double x3 = v1.y() * v2.z() - v1.z() * v2.y();
    double y3 = v1.z() * v2.x() - v1.x() * v2.z();
    double z3 = v1.x() * v2.y() - v1.y() * v2.x();
    return Vector3(x3, y3, z3);
}

inline double distance(const Vector3& v1, const Vector3& v2)
{
    return (v1 - v2).abs();
}

} // WebCore

#endif // Vector3_h
