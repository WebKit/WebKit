//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// geometry_utils:
//   Helper library for generating certain sets of geometry.
//

#ifndef UTIL_GEOMETRY_UTILS_H
#define UTIL_GEOMETRY_UTILS_H

#include <cstddef>
#include <vector>

#include <export.h>
#include <GLES2/gl2.h>

#include "Vector.h"

struct ANGLE_EXPORT SphereGeometry
{
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<GLushort> indices;
};

ANGLE_EXPORT void CreateSphereGeometry(size_t sliceCount, float radius, SphereGeometry *result);

struct ANGLE_EXPORT CubeGeometry
{
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<GLushort> indices;
};

ANGLE_EXPORT void GenerateCubeGeometry(float radius, CubeGeometry *result);

#endif  // UTIL_GEOMETRY_UTILS_H
