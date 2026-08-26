//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gtest/gtest.h"

#include "libANGLE/renderer/gl/renderergl_utils.h"

#include <array>

namespace rx
{
namespace
{

// Test parsing of PowerVR driver version strings.
TEST(RendererGLUtilsTest, GetPowerVRDriverVersion)
{
    std::array<int, 2> version = {0, 0};

    // Valid PowerVR driver version strings
    EXPECT_TRUE(GetPowerVRDriverVersion("Imagination Technologies", "PowerVR D-Series DXT-48-1536",
                                        "OpenGL ES 3.2 build 25.3@6908880", &version));
    EXPECT_EQ(version[0], 25);
    EXPECT_EQ(version[1], 3);

    EXPECT_TRUE(GetPowerVRDriverVersion("Imagination Technologies", "PowerVR D-Series DXT-48-1536",
                                        "OpenGL ES 3.2 build 26.1@7000000", &version));
    EXPECT_EQ(version[0], 26);
    EXPECT_EQ(version[1], 1);

    // Negative tests
    EXPECT_FALSE(GetPowerVRDriverVersion("", "", "", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("", "PowerVR D-Series DXT-48-1536",
                                         "OpenGL ES 3.2 build 25.3@6908880", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("Imagination Technologies", "",
                                         "OpenGL ES 3.2 build 25.3@6908880", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("Imagination Technologies", "Some Other Renderer",
                                         "OpenGL ES 3.2 build 25.3@6908880", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("Imagination Technologies", "PowerVR D-Series DXT-48-1536",
                                         "", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("Intel", "Mesa DRI Intel(R) HD Graphics 4000 (IVB GT2)",
                                         "OpenGL ES 3.2 build 25.3@6908880", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("Imagination Technologies", "PowerVR D-Series DXT-48-1536",
                                         "without build version", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("Imagination Technologies", "PowerVR D-Series DXT-48-1536",
                                         "build invalid@version", &version));
    EXPECT_FALSE(GetPowerVRDriverVersion("Imagination Technologies", "PowerVR D-Series DXT-48-1536",
                                         "build 26@7000000", &version));
}

}  // namespace
}  // namespace rx
