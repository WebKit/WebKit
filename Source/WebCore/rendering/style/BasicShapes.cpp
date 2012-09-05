/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include "BasicShapes.h"
#include "FloatRect.h"
#include "LengthFunctions.h"
#include "Path.h"

namespace WebCore {

void BasicShapeRectangle::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    path.addRoundedRect(FloatRect(floatValueForLength(m_x, boundingBox.width()) + boundingBox.x(),
                                  floatValueForLength(m_y, boundingBox.height()) + boundingBox.y(),
                                  floatValueForLength(m_width, boundingBox.width()),
                                  floatValueForLength(m_height, boundingBox.height())),
                        FloatSize(m_cornerRadiusX.isUndefined() ? 0 : floatValueForLength(m_cornerRadiusX, boundingBox.width()),
                                  m_cornerRadiusY.isUndefined() ? 0 : floatValueForLength(m_cornerRadiusY, boundingBox.height())));
}

void BasicShapeCircle::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    float diagonal = sqrtf((boundingBox.width() * boundingBox.width() + boundingBox.height() * boundingBox.height()) / 2);
    float centerX = floatValueForLength(m_centerX, boundingBox.width());
    float centerY = floatValueForLength(m_centerY, boundingBox.height());
    float radius = floatValueForLength(m_radius, diagonal);
    path.addEllipse(FloatRect(centerX - radius + boundingBox.x(),
                              centerY - radius + boundingBox.y(),
                              radius * 2,
                              radius * 2));
}

void BasicShapeEllipse::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    float centerX = floatValueForLength(m_centerX, boundingBox.width());
    float centerY = floatValueForLength(m_centerY, boundingBox.height());
    float radiusX = floatValueForLength(m_radiusX, boundingBox.width());
    float radiusY = floatValueForLength(m_radiusY, boundingBox.height());
    path.addEllipse(FloatRect(centerX - radiusX + boundingBox.x(),
                              centerY - radiusY + boundingBox.y(),
                              radiusX * 2,
                              radiusY * 2));
}

void BasicShapePolygon::path(Path& path, const FloatRect& boundingBox)
{
    ASSERT(path.isEmpty());
    ASSERT(!(m_values.size() % 2));
    size_t length = m_values.size();
    
    if (!length)
        return;

    path.moveTo(FloatPoint(floatValueForLength(m_values.at(0), boundingBox.width()) + boundingBox.x(),
                           floatValueForLength(m_values.at(1), boundingBox.width()) + boundingBox.y()));
    for (size_t i = 2; i < length; i = i + 2) {
        path.addLineTo(FloatPoint(floatValueForLength(m_values.at(i), boundingBox.width()) + boundingBox.x(),
                                  floatValueForLength(m_values.at(i + 1), boundingBox.width()) + boundingBox.y()));
    }
    path.closeSubpath();
}
}
