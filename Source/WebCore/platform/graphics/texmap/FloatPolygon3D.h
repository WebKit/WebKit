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

#include "FloatPlane3D.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "TransformationMatrix.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

// FloatPolygon3D represents planar polygon in 3D space

class FloatPolygon3D {
    WTF_MAKE_TZONE_ALLOCATED(FloatPolygon3D);
public:
    FloatPolygon3D() = default;
    FloatPolygon3D(const FloatRect&, const TransformationMatrix&);

    const FloatPoint3D& vertexAt(unsigned index) const { return m_vertices[index]; }
    unsigned numberOfVertices() const { return m_vertices.size(); }

    const FloatPoint3D& normal() const { return m_normal; }

    std::pair<FloatPolygon3D, FloatPolygon3D> split(const FloatPlane3D&) const;

private:
    FloatPolygon3D(Vector<FloatPoint3D>&&, const FloatPoint3D&);

    Vector<FloatPoint3D> m_vertices;
    FloatPoint3D m_normal = { 0, 0, 1 };
};

} // namespace WebCore
