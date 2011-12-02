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


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCLayerQuad.h"

namespace WebCore {

CCLayerQuad::Edge::Edge(const FloatPoint& p, const FloatPoint& q)
{
    ASSERT(p != q);

    FloatPoint tangent(p.y() - q.y(), q.x() - p.x());
    float cross2 = p.x() * q.y() - q.x() * p.y();

    set(tangent.x(), tangent.y(), cross2);
    scale(1.0f / tangent.length());
}

CCLayerQuad::CCLayerQuad(const FloatQuad& quad)
{
    // Create edges.
    m_left = Edge(quad.p4(), quad.p1());
    m_right = Edge(quad.p2(), quad.p3());
    m_top = Edge(quad.p1(), quad.p2());
    m_bottom = Edge(quad.p3(), quad.p4());

    float sign = quad.isCounterclockwise() ? -1 : 1;
    m_left.scale(sign);
    m_right.scale(sign);
    m_top.scale(sign);
    m_bottom.scale(sign);
}

FloatQuad CCLayerQuad::floatQuad() const
{
    return FloatQuad(m_left.intersect(m_top),
                     m_top.intersect(m_right),
                     m_right.intersect(m_bottom),
                     m_bottom.intersect(m_left));
}

void CCLayerQuad::toFloatArray(float flattened[12]) const
{
    flattened[0] = m_left.x();
    flattened[1] = m_left.y();
    flattened[2] = m_left.z();
    flattened[3] = m_top.x();
    flattened[4] = m_top.y();
    flattened[5] = m_top.z();
    flattened[6] = m_right.x();
    flattened[7] = m_right.y();
    flattened[8] = m_right.z();
    flattened[9] = m_bottom.x();
    flattened[10] = m_bottom.y();
    flattened[11] = m_bottom.z();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
