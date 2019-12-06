//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GeometryShaderTest.cpp : Tests of the implementation of geometry shader

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class GeometryShaderTest : public ANGLETest
{
  protected:
    static std::string CreateEmptyGeometryShader(const std::string &inputPrimitive,
                                                 const std::string &outputPrimitive,
                                                 int invocations,
                                                 int maxVertices)
    {
        std::ostringstream ostream;
        ostream << "#version 310 es\n"
                   "#extension GL_EXT_geometry_shader : require\n";
        if (!inputPrimitive.empty())
        {
            ostream << "layout (" << inputPrimitive << ") in;\n";
        }
        if (!outputPrimitive.empty())
        {
            ostream << "layout (" << outputPrimitive << ") out;\n";
        }
        if (invocations > 0)
        {
            ostream << "layout (invocations = " << invocations << ") in;\n";
        }
        if (maxVertices >= 0)
        {
            ostream << "layout (max_vertices = " << maxVertices << ") out;\n";
        }
        ostream << "void main()\n"
                   "{\n"
                   "}";
        return ostream.str();
    }
};

class GeometryShaderTestES3 : public ANGLETest
{};

// Verify that Geometry Shader cannot be created in an OpenGL ES 3.0 context.
TEST_P(GeometryShaderTestES3, CreateGeometryShaderInES3)
{
    EXPECT_TRUE(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));
    GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER_EXT);
    EXPECT_EQ(0u, geometryShader);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Verify that Geometry Shader can be created and attached to a program.
TEST_P(GeometryShaderTest, CreateAndAttachGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kGS[] = R"(#version 310 es
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

    GLuint geometryShader = CompileShader(GL_GEOMETRY_SHADER_EXT, kGS);

    EXPECT_NE(0u, geometryShader);

    GLuint programID = glCreateProgram();
    glAttachShader(programID, geometryShader);

    glDetachShader(programID, geometryShader);
    glDeleteShader(geometryShader);
    glDeleteProgram(programID);

    EXPECT_GL_NO_ERROR();
}

// Verify that all the implementation dependent geometry shader related resource limits meet the
// requirement of GL_EXT_geometry_shader SPEC.
TEST_P(GeometryShaderTest, GeometryShaderImplementationDependentLimits)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    const std::map<GLenum, int> limits = {{GL_MAX_FRAMEBUFFER_LAYERS_EXT, 256},
                                          {GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT, 1024},
                                          {GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT, 12},
                                          {GL_MAX_GEOMETRY_INPUT_COMPONENTS_EXT, 64},
                                          {GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_EXT, 64},
                                          {GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT, 256},
                                          {GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT, 1024},
                                          {GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT, 16},
                                          {GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT, 0},
                                          {GL_MAX_GEOMETRY_ATOMIC_COUNTERS_EXT, 0},
                                          {GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT, 0},
                                          {GL_MAX_GEOMETRY_IMAGE_UNIFORMS_EXT, 0},
                                          {GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT, 32}};

    GLint value;
    for (const auto &limit : limits)
    {
        value = 0;
        glGetIntegerv(limit.first, &value);
        EXPECT_GL_NO_ERROR();
        EXPECT_GE(value, limit.second);
    }

    value = 0;
    glGetIntegerv(GL_LAYER_PROVOKING_VERTEX_EXT, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_TRUE(value == GL_FIRST_VERTEX_CONVENTION_EXT || value == GL_LAST_VERTEX_CONVENTION_EXT ||
                value == GL_UNDEFINED_VERTEX_EXT);
}

// Verify that all the combined resource limits meet the requirement of GL_EXT_geometry_shader SPEC.
TEST_P(GeometryShaderTest, CombinedResourceLimits)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    // See http://anglebug.com/2261.
    ANGLE_SKIP_TEST_IF(IsAndroid());

    const std::map<GLenum, int> limits = {{GL_MAX_UNIFORM_BUFFER_BINDINGS, 48},
                                          {GL_MAX_COMBINED_UNIFORM_BLOCKS, 36},
                                          {GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 64}};

    GLint value;
    for (const auto &limit : limits)
    {
        value = 0;
        glGetIntegerv(limit.first, &value);
        EXPECT_GL_NO_ERROR();
        EXPECT_GE(value, limit.second);
    }
}

// Verify that linking a program with an uncompiled geometry shader causes a link failure.
TEST_P(GeometryShaderTest, LinkWithUncompiledGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    GLuint vertexShader   = CompileShader(GL_VERTEX_SHADER, essl31_shaders::vs::Simple());
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, essl31_shaders::fs::Red());
    ASSERT_NE(0u, vertexShader);
    ASSERT_NE(0u, fragmentShader);

    GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER_EXT);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glAttachShader(program, geometryShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(geometryShader);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_EQ(0, linkStatus);

    glDeleteProgram(program);
    ASSERT_GL_NO_ERROR();
}

// Verify that linking a program with geometry shader whose version is different from other shaders
// in this program causes a link error.
TEST_P(GeometryShaderTest, LinkWhenShaderVersionMismatch)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    const std::string &emptyGeometryShader = CreateEmptyGeometryShader("points", "points", 2, 1);

    GLuint program = CompileProgramWithGS(essl3_shaders::vs::Simple(), emptyGeometryShader.c_str(),
                                          essl3_shaders::fs::Red());
    EXPECT_EQ(0u, program);
}

// Verify that linking a program with geometry shader that lacks input primitive,
// output primitive, or declaration on 'max_vertices' causes a link failure.
TEST_P(GeometryShaderTest, LinkValidationOnGeometryShaderLayouts)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    const std::string gsWithoutInputPrimitive  = CreateEmptyGeometryShader("", "points", 2, 1);
    const std::string gsWithoutOutputPrimitive = CreateEmptyGeometryShader("points", "", 2, 1);
    const std::string gsWithoutInvocations = CreateEmptyGeometryShader("points", "points", -1, 1);
    const std::string gsWithoutMaxVertices = CreateEmptyGeometryShader("points", "points", 2, -1);

    // Linking a program with a geometry shader that only lacks 'invocations' should not cause a
    // link failure.
    GLuint program = CompileProgramWithGS(essl31_shaders::vs::Simple(),
                                          gsWithoutInvocations.c_str(), essl31_shaders::fs::Red());
    EXPECT_NE(0u, program);

    glDeleteProgram(program);

    // Linking a program with a geometry shader that lacks input primitive, output primitive or
    // 'max_vertices' causes a link failure.
    program = CompileProgramWithGS(essl31_shaders::vs::Simple(), gsWithoutInputPrimitive.c_str(),
                                   essl31_shaders::fs::Red());
    EXPECT_EQ(0u, program);

    program = CompileProgramWithGS(essl31_shaders::vs::Simple(), gsWithoutOutputPrimitive.c_str(),
                                   essl31_shaders::fs::Red());
    EXPECT_EQ(0u, program);

    program = CompileProgramWithGS(essl31_shaders::vs::Simple(), gsWithoutMaxVertices.c_str(),
                                   essl31_shaders::fs::Red());
    EXPECT_EQ(0u, program);

    ASSERT_GL_NO_ERROR();
}

// Verify that an link error occurs when the vertex shader has an array output and there is a
// geometry shader in the program.
TEST_P(GeometryShaderTest, VertexShaderArrayOutput)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kVS[] = R"(#version 310 es
in vec4 vertex_in;
out vec4 vertex_out[3];
void main()
{
    gl_Position = vertex_in;
    vertex_out[0] = vec4(1.0, 0.0, 0.0, 1.0);
    vertex_out[1] = vec4(0.0, 1.0, 0.0, 1.0);
    vertex_out[2] = vec4(0.0, 0.0, 1.0, 1.0);
})";

    constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (invocations = 3, triangles) in;
layout (points, max_vertices = 3) out;
in vec4 vertex_out[];
out vec4 geometry_color;
void main()
{
    gl_Position = gl_in[0].gl_Position;
    geometry_color = vertex_out[0];
    EmitVertex();
})";

    constexpr char kFS[] = R"(#version 310 es
precision mediump float;
in vec4 geometry_color;
layout (location = 0) out vec4 output_color;
void main()
{
    output_color = geometry_color;
})";

    GLuint program = CompileProgramWithGS(kVS, kGS, kFS);
    EXPECT_EQ(0u, program);

    EXPECT_GL_NO_ERROR();
}

// Verify that an link error occurs when the definition of a unform in fragment shader is different
// from those in a geometry shader.
TEST_P(GeometryShaderTest, UniformMismatchBetweenGeometryAndFragmentShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kVS[] = R"(#version 310 es
uniform highp vec4 uniform_value_vert;
in vec4 vertex_in;
out vec4 vertex_out;
void main()
{
    gl_Position = vertex_in;
    vertex_out = uniform_value_vert;
})";

    constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
uniform vec4 uniform_value;
layout (invocations = 3, triangles) in;
layout (points, max_vertices = 3) out;
in vec4 vertex_out[];
out vec4 geometry_color;
void main()
{
    gl_Position = gl_in[0].gl_Position;
    geometry_color = vertex_out[0] + uniform_value;
    EmitVertex();
})";

    constexpr char kFS[] = R"(#version 310 es
precision highp float;
uniform float uniform_value;
in vec4 geometry_color;
layout (location = 0) out vec4 output_color;
void main()
{
    output_color = vec4(geometry_color.rgb, uniform_value);
})";

    GLuint program = CompileProgramWithGS(kVS, kGS, kFS);
    EXPECT_EQ(0u, program);

    EXPECT_GL_NO_ERROR();
}

// Verify that an link error occurs when the number of uniform blocks in a geometry shader exceeds
// MAX_GEOMETRY_UNIFORM_BLOCKS_EXT.
TEST_P(GeometryShaderTest, TooManyUniformBlocks)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    GLint maxGeometryUniformBlocks = 0;
    glGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT, &maxGeometryUniformBlocks);

    GLint numUniformBlocks = maxGeometryUniformBlocks + 1;
    std::ostringstream stream;
    stream << "#version 310 es\n"
              "#extension GL_EXT_geometry_shader : require\n"
              "uniform ubo\n"
              "{\n"
              "    vec4 value1;\n"
              "} block0["
           << numUniformBlocks
           << "];\n"
              "layout (triangles) in;\n"
              "layout (points, max_vertices = 1) out;\n"
              "void main()\n"
              "{\n"
              "    gl_Position = gl_in[0].gl_Position;\n";

    for (GLint i = 0; i < numUniformBlocks; ++i)
    {
        stream << "    gl_Position += block0[" << i << "].value1;\n";
    }
    stream << "    EmitVertex();\n"
              "}\n";

    GLuint program = CompileProgramWithGS(essl31_shaders::vs::Simple(), stream.str().c_str(),
                                          essl31_shaders::fs::Red());
    EXPECT_EQ(0u, program);

    EXPECT_GL_NO_ERROR();
}

// Verify that an link error occurs when the number of shader storage blocks in a geometry shader
// exceeds MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT.
TEST_P(GeometryShaderTest, TooManyShaderStorageBlocks)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    GLint maxGeometryShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT, &maxGeometryShaderStorageBlocks);

    GLint numSSBOs = maxGeometryShaderStorageBlocks + 1;
    std::ostringstream stream;
    stream << "#version 310 es\n"
              "#extension GL_EXT_geometry_shader : require\n"
              "buffer ssbo\n"
              "{\n"
              "    vec4 value1;\n"
              "} block0["
           << numSSBOs
           << "];\n"
              "layout (triangles) in;\n"
              "layout (points, max_vertices = 1) out;\n"
              "void main()\n"
              "{\n"
              "    gl_Position = gl_in[0].gl_Position;\n";

    for (GLint i = 0; i < numSSBOs; ++i)
    {
        stream << "    gl_Position += block0[" << i << "].value1;\n";
    }
    stream << "    EmitVertex();\n"
              "}\n";

    GLuint program = CompileProgramWithGS(essl31_shaders::vs::Simple(), stream.str().c_str(),
                                          essl31_shaders::fs::Red());
    EXPECT_EQ(0u, program);

    EXPECT_GL_NO_ERROR();
}

// Verify that an link error occurs when the definition of a unform block in the vertex shader is
// different from that in a geometry shader.
TEST_P(GeometryShaderTest, UniformBlockMismatchBetweenVertexAndGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kVS[] = R"(#version 310 es
uniform ubo
{
    vec4 uniform_value_vert;
} block0;
in vec4 vertex_in;
out vec4 vertex_out;
void main()
{
    gl_Position = vertex_in;
    vertex_out = block0.uniform_value_vert;
})";

    constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
uniform ubo
{
    vec4 uniform_value_geom;
} block0;
layout (triangles) in;
layout (points, max_vertices = 1) out;
in vec4 vertex_out[];
void main()
{
    gl_Position = gl_in[0].gl_Position + vertex_out[0];
    gl_Position += block0.uniform_value_geom;
    EmitVertex();
})";

    GLuint program = CompileProgramWithGS(kVS, kGS, essl31_shaders::fs::Red());
    EXPECT_EQ(0u, program);

    EXPECT_GL_NO_ERROR();
}

// Verify that an link error occurs when the definition of a shader storage block in the geometry
// shader is different from that in a fragment shader.
TEST_P(GeometryShaderTest, ShaderStorageBlockMismatchBetweenGeometryAndFragmentShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    GLint maxGeometryShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT, &maxGeometryShaderStorageBlocks);

    // The minimun value of MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT can be 0.
    // [EXT_geometry_shader] Table 20.43gs
    ANGLE_SKIP_TEST_IF(maxGeometryShaderStorageBlocks == 0);

    GLint maxFragmentShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);

    // The minimun value of MAX_FRAGMENT_SHADER_STORAGE_BLOCKS can be 0.
    // [OpenGL ES 3.1] Table 20.44
    ANGLE_SKIP_TEST_IF(maxFragmentShaderStorageBlocks == 0);

    constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
buffer ssbo
{
    vec4 ssbo_value;
} block0;
layout (triangles) in;
layout (points, max_vertices = 1) out;
void main()
{
    gl_Position = gl_in[0].gl_Position + block0.ssbo_value;
    EmitVertex();
})";

    constexpr char kFS[] = R"(#version 310 es
precision highp float;
buffer ssbo
{
    vec3 ssbo_value;
} block0;
layout (location = 0) out vec4 output_color;
void main()
{
    output_color = vec4(block0.ssbo_value, 1);
})";

    GLuint program = CompileProgramWithGS(essl31_shaders::vs::Simple(), kGS, kFS);
    EXPECT_EQ(0u, program);

    EXPECT_GL_NO_ERROR();
}

// Verify GL_REFERENCED_BY_GEOMETRY_SHADER_EXT cannot be used on platforms that don't support
// EXT_geometry_shader, or we will get an INVALID_ENUM error.
TEST_P(GeometryShaderTest, ReferencedByGeometryShaderWithoutExtensionEnabled)
{
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kFS[] = R"(#version 310 es
precision highp float;
uniform vec4 color;
layout(location = 0) out vec4 oColor;
void main()
{
    oColor = color;
})";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);

    const GLuint index = glGetProgramResourceIndex(program, GL_UNIFORM, "color");
    ASSERT_GL_NO_ERROR();
    ASSERT_NE(GL_INVALID_INDEX, index);

    constexpr GLenum kProps[]    = {GL_REFERENCED_BY_GEOMETRY_SHADER_EXT};
    constexpr GLsizei kPropCount = static_cast<GLsizei>(ArraySize(kProps));
    GLint params[ArraySize(kProps)];
    GLsizei length;

    glGetProgramResourceiv(program, GL_UNIFORM, index, kPropCount, kProps, kPropCount, &length,
                           params);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Verify GL_REFERENCED_BY_GEOMETRY_SHADER_EXT can work correctly on platforms that support
// EXT_geometry_shader.
TEST_P(GeometryShaderTest, ReferencedByGeometryShader)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    constexpr char kVS[] = R"(#version 310 es
precision highp float;
layout(location = 0) in highp vec4 position;
void main()
{
    gl_Position = position;
})";

    constexpr char kGS[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (binding = 3) uniform ubo0
{
    vec4 ubo0_location;
} block0;
layout (binding = 4) uniform ubo1
{
    vec4 ubo1_location;
} block1;
uniform vec4 u_color;
layout (triangles) in;
layout (points, max_vertices = 1) out;
out vec4 gs_out;
void main()
{
    gl_Position = gl_in[0].gl_Position;
    gl_Position += block0.ubo0_location + block1.ubo1_location;
    gs_out = u_color;
    EmitVertex();
})";

    constexpr char kFS[] = R"(#version 310 es
precision highp float;
in vec4 gs_out;
layout(location = 0) out vec4 oColor;
void main()
{
    oColor = gs_out;
})";

    ANGLE_GL_PROGRAM_WITH_GS(program, kVS, kGS, kFS);

    constexpr GLenum kProps[]    = {GL_REFERENCED_BY_GEOMETRY_SHADER_EXT};
    constexpr GLsizei kPropCount = static_cast<GLsizei>(ArraySize(kProps));
    std::array<GLint, ArraySize(kProps)> params;
    GLsizei length;

    params.fill(1);
    GLuint index = glGetProgramResourceIndex(program, GL_PROGRAM_INPUT, "position");
    glGetProgramResourceiv(program, GL_PROGRAM_INPUT, index, kPropCount, kProps, kPropCount,
                           &length, params.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0, params[0]);

    params.fill(1);
    index = glGetProgramResourceIndex(program, GL_PROGRAM_OUTPUT, "oColor");
    glGetProgramResourceiv(program, GL_PROGRAM_OUTPUT, index, kPropCount, kProps, kPropCount,
                           &length, params.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0, params[0]);

    index = glGetProgramResourceIndex(program, GL_UNIFORM, "u_color");
    glGetProgramResourceiv(program, GL_UNIFORM, index, kPropCount, kProps, kPropCount, &length,
                           params.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, params[0]);

    params.fill(0);
    index = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, "ubo1");
    glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, index, kPropCount, kProps, kPropCount,
                           &length, params.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, params[0]);

    GLint maxGeometryShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT, &maxGeometryShaderStorageBlocks);
    // The maximum number of shader storage blocks in a geometry shader can be 0.
    // [EXT_geometry_shader] Table 20.43gs
    if (maxGeometryShaderStorageBlocks > 0)
    {
        constexpr char kGSWithSSBO[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout (binding = 2) buffer ssbo
{
    vec4 ssbo_value;
} block0;
layout (triangles) in;
layout (points, max_vertices = 1) out;
out vec4 gs_out;
void main()
{
    gl_Position = gl_in[0].gl_Position + block0.ssbo_value;
    gs_out = block0.ssbo_value;
    EmitVertex();
})";

        ANGLE_GL_PROGRAM_WITH_GS(programWithSSBO, kVS, kGSWithSSBO, kFS);

        params.fill(0);
        index = glGetProgramResourceIndex(programWithSSBO, GL_SHADER_STORAGE_BLOCK, "ssbo");
        glGetProgramResourceiv(programWithSSBO, GL_SHADER_STORAGE_BLOCK, index, kPropCount, kProps,
                               kPropCount, &length, params.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(1, params[0]);

        params.fill(0);
        index = glGetProgramResourceIndex(programWithSSBO, GL_BUFFER_VARIABLE, "ssbo.ssbo_value");
        glGetProgramResourceiv(programWithSSBO, GL_BUFFER_VARIABLE, index, kPropCount, kProps,
                               kPropCount, &length, params.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(1, params[0]);
    }

    GLint maxGeometryAtomicCounterBuffers = 0;
    glGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT, &maxGeometryAtomicCounterBuffers);
    // The maximum number of atomic counter buffers in a geometry shader can be 0.
    // [EXT_geometry_shader] Table 20.43gs
    if (maxGeometryAtomicCounterBuffers > 0)
    {
        constexpr char kGSWithAtomicCounters[] = R"(#version 310 es
#extension GL_EXT_geometry_shader : require
layout(binding = 1, offset = 0) uniform atomic_uint gs_counter;
layout (triangles) in;
layout (points, max_vertices = 1) out;
out vec4 gs_out;
void main()
{
    atomicCounterIncrement(gs_counter);
    gl_Position = gl_in[0].gl_Position;
    gs_out = vec4(1.0, 0.0, 0.0, 1.0);
    EmitVertex();
})";

        ANGLE_GL_PROGRAM_WITH_GS(programWithAtomicCounter, kVS, kGSWithAtomicCounters, kFS);

        params.fill(0);
        index = glGetProgramResourceIndex(programWithAtomicCounter, GL_UNIFORM, "gs_counter");
        EXPECT_GL_NO_ERROR();
        glGetProgramResourceiv(programWithAtomicCounter, GL_ATOMIC_COUNTER_BUFFER, index,
                               kPropCount, kProps, kPropCount, &length, params.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(1, params[0]);
    }
}

// Verify correct errors can be reported when we use illegal parameters on FramebufferTextureEXT.
TEST_P(GeometryShaderTest, NegativeFramebufferTextureEXT)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 32, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // [EXT_geometry_shader] Section 9.2.8, "Attaching Texture Images to a Framebuffer"
    // An INVALID_ENUM error is generated if <target> is not DRAW_FRAMEBUFFER, READ_FRAMEBUFFER, or
    // FRAMEBUFFER.
    glFramebufferTextureEXT(GL_TEXTURE_2D, GL_COLOR_ATTACHMENT0, tex, 0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // An INVALID_ENUM error is generated if <attachment> is not one of the attachments in Table
    // 9.1.
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_TEXTURE_2D, tex, 0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // An INVALID_OPERATION error is generated if zero is bound to <target>.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // An INVALID_VALUE error is generated if <texture> is not the name of a texture object, or if
    // <level> is not a supported texture level for <texture>.
    GLuint tex2;
    glGenTextures(1, &tex2);
    glDeleteTextures(1, &tex2);
    ASSERT_FALSE(glIsTexture(tex2));
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex2, 0);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    GLint max3DSize;
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DSize);
    GLint max3DLevel = static_cast<GLint>(std::log2(max3DSize));
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, max3DLevel + 1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Verify CheckFramebufferStatus can work correctly on layered depth and stencil attachments.
TEST_P(GeometryShaderTest, LayeredFramebufferCompletenessWithDepthAttachment)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    GLint maxFramebufferLayers;
    glGetIntegerv(GL_MAX_FRAMEBUFFER_LAYERS_EXT, &maxFramebufferLayers);

    constexpr GLint kTexLayers = 2;
    ASSERT_LT(kTexLayers, maxFramebufferLayers);

    GLTexture layeredColorTex;
    glBindTexture(GL_TEXTURE_3D, layeredColorTex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 32, 32, kTexLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);

    // [EXT_geometry_shader] section 9.4.1, "Framebuffer Completeness"
    // If any framebuffer attachment is layered, all populated attachments must be layered.
    // {FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT }
    GLTexture layeredDepthStencilTex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, layeredDepthStencilTex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH24_STENCIL8, 32, 32, kTexLayers, 0,
                 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    // 1. Color attachment is layered, while depth attachment is not layered.
    GLFramebuffer fbo1;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, layeredColorTex, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, layeredDepthStencilTex, 0, 0);
    GLenum status1 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT, status1);

    // 2. Color attachment is not layered, while depth attachment is layered.
    GLFramebuffer fbo2;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, layeredColorTex, 0, 0);
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, layeredDepthStencilTex, 0);
    GLenum status2 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT, status2);

    // 3. Color attachment is not layered, while stencil attachment is layered.
    GLFramebuffer fbo3;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo3);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, layeredColorTex, 0, 0);
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, layeredDepthStencilTex, 0);
    GLenum status3 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT, status3);

    // [EXT_geometry_shader] section 9.4.1, "Framebuffer Completeness"
    // If <image> is a three-dimensional texture or a two-dimensional array texture and the
    // attachment is layered, the depth or layer count, respectively, of the texture is less than or
    // equal to the value of MAX_FRAMEBUFFER_LAYERS_EXT.
    GLint maxArrayTextureLayers;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);
    GLint depthTexLayer4 = maxFramebufferLayers + 1;
    ANGLE_SKIP_TEST_IF(maxArrayTextureLayers < depthTexLayer4);

    // Use a depth attachment whose layer count exceeds MAX_FRAMEBUFFER_LAYERS
    GLTexture depthTex4;
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthTex4);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH24_STENCIL8, 32, 32, depthTexLayer4, 0,
                 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    GLFramebuffer fbo4;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo4);
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex4, 0);
    GLenum status4 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, status4);
}

// Verify correct errors can be reported when we use layered cube map attachments on a framebuffer.
TEST_P(GeometryShaderTest, NegativeLayeredFramebufferCompletenessWithCubeMapTextures)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_geometry_shader"));

    GLTexture tex;
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, status);

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glFramebufferTextureEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    ASSERT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, status);
}

ANGLE_INSTANTIATE_TEST_ES3(GeometryShaderTestES3);
ANGLE_INSTANTIATE_TEST_ES31(GeometryShaderTest);
}  // namespace
