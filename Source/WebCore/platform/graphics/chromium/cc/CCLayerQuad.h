/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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


#ifndef CCLayerQuad_h
#define CCLayerQuad_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatPoint3D.h"
#include "FloatQuad.h"

static const float kAntiAliasingInflateDistance = 0.5f;

namespace WebCore {

class CCLayerQuad {
public:
    class Edge {
    public:
        Edge()
            : m_x(0)
            , m_y(0)
            , m_z(0)
        {
        }
        Edge(const FloatPoint&, const FloatPoint&);

        float x() const { return m_x; }
        float y() const { return m_y; }
        float z() const { return m_z; }

        void setX(float x) { m_x = x; }
        void setY(float y) { m_y = y; }
        void setZ(float z) { m_z = z; }
        void set(float x, float y, float z)
        {
            m_x = x;
            m_y = y;
            m_z = z;
        }

        void moveX(float dx) { m_x += dx; }
        void moveY(float dy) { m_y += dy; }
        void moveZ(float dz) { m_z += dz; }
        void move(float dx, float dy, float dz)
        {
            m_x += dx;
            m_y += dy;
            m_z += dz;
        }

        void scaleX(float sx) { m_x *= sx; }
        void scaleY(float sy) { m_y *= sy; }
        void scaleZ(float sz) { m_z *= sz; }
        void scale(float sx, float sy, float sz)
        {
            m_x *= sx;
            m_y *= sy;
            m_z *= sz;
        }
        void scale(float s) { scale(s, s, s); }

        FloatPoint intersect(const Edge& e) const
        {
            return FloatPoint(
                (y() * e.z() - e.y() * z()) / (x() * e.y() - e.x() * y()),
                (x() * e.z() - e.x() * z()) / (e.x() * y() - x() * e.y()));
        }

    private:
        float m_x;
        float m_y;
        float m_z;
    };

    CCLayerQuad(const Edge& left, const Edge& top, const Edge& right, const Edge& bottom)
        : m_left(left)
        , m_top(top)
        , m_right(right)
        , m_bottom(bottom)
    {
    }
    CCLayerQuad(const FloatQuad&);

    Edge left() const { return m_left; }
    Edge top() const { return m_top; }
    Edge right() const { return m_right; }
    Edge bottom() const { return m_bottom; }

    void inflateX(float dx) { m_left.moveZ(dx); m_right.moveZ(dx); }
    void inflateY(float dy) { m_top.moveZ(dy); m_bottom.moveZ(dy); }
    void inflate(float d) { inflateX(d); inflateY(d); }
    void inflateAntiAliasingDistance() { inflate(kAntiAliasingInflateDistance); }

    FloatQuad floatQuad() const;

    void toFloatArray(float[12]) const;

private:
    Edge m_left;
    Edge m_top;
    Edge m_right;
    Edge m_bottom;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
