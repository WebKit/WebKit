//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GeometryShaderTest.cpp : Tests of the implementation of geometry shader

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class GeometryShaderTest : public ANGLETest
{
};

class GeometryShaderTestES3 : public ANGLETest
{
};

// Verify that Geometry Shader cannot be created in an OpenGL ES 3.0 context.
TEST_P(GeometryShaderTestES3, CreateGeometryShaderInES3)
{
    EXPECT_TRUE(!extensionEnabled("GL_EXT_geometry_shader"));
    GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER_EXT);
    EXPECT_EQ(0u, geometryShader);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Verify that Geometry Shader can be created and attached to a program.
TEST_P(GeometryShaderTest, CreateAndAttachGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!extensionEnabled("GL_EXT_geometry_shader"));

    const std::string &geometryShaderSource =
        R"(#version 310 es
        #extension GL_EXT_geometry_shader : require
        layout (invocations = 3, triangles) in;
        layout (triangle_strip, max_vertices = 3) out;
        in vec4 texcoord[];
        out vec4 o_texcoord;
        void main()
        {
            int n;
            for (n = 0; n < gl_in.length(); n++)
            {
                gl_Position = gl_in[n].gl_Position;
                gl_Layer   = gl_InvocationID;
                o_texcoord = texcoord[n];
                EmitVertex();
            }
            EndPrimitive();
        })";

    GLuint geometryShader = CompileShader(GL_GEOMETRY_SHADER_EXT, geometryShaderSource);

    EXPECT_NE(0u, geometryShader);

    GLuint programID = glCreateProgram();
    glAttachShader(programID, geometryShader);

    glDetachShader(programID, geometryShader);
    glDeleteShader(geometryShader);
    glDeleteProgram(programID);

    EXPECT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(GeometryShaderTestES3, ES3_OPENGL(), ES3_OPENGLES(), ES3_D3D11());
ANGLE_INSTANTIATE_TEST(GeometryShaderTest, ES31_OPENGL(), ES31_OPENGLES(), ES31_D3D11());
}
