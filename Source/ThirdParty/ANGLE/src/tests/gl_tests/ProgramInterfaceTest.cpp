//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramInterfaceTest: Tests of program interfaces.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class ProgramInterfaceTestES31 : public ANGLETest
{
  protected:
    ProgramInterfaceTestES31()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Tests glGetProgramResourceIndex.
TEST_P(ProgramInterfaceTestES31, GetResourceIndex)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "in highp vec4 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position;\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform vec4 color;\n"
        "out vec4 oColor;\n"
        "void main()\n"
        "{\n"
        "    oColor = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLuint index = glGetProgramResourceIndex(program, GL_PROGRAM_INPUT, "position");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    index = glGetProgramResourceIndex(program, GL_PROGRAM_INPUT, "missing");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_INVALID_INDEX, index);

    index = glGetProgramResourceIndex(program, GL_PROGRAM_OUTPUT, "oColor");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    index = glGetProgramResourceIndex(program, GL_PROGRAM_OUTPUT, "missing");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_INVALID_INDEX, index);

    index = glGetProgramResourceIndex(program, GL_ATOMIC_COUNTER_BUFFER, "missing");
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Tests glGetProgramResourceName.
TEST_P(ProgramInterfaceTestES31, GetResourceName)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "in highp vec4 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position;\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform vec4 color;\n"
        "out vec4 oColor[4];\n"
        "void main()\n"
        "{\n"
        "    oColor[0] = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLuint index = glGetProgramResourceIndex(program, GL_PROGRAM_INPUT, "position");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    GLchar name[64];
    GLsizei length;
    glGetProgramResourceName(program, GL_PROGRAM_INPUT, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(8, length);
    EXPECT_EQ("position", std::string(name));

    glGetProgramResourceName(program, GL_PROGRAM_INPUT, index, 4, &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(3, length);
    EXPECT_EQ("pos", std::string(name));

    glGetProgramResourceName(program, GL_PROGRAM_INPUT, index, -1, &length, name);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetProgramResourceName(program, GL_PROGRAM_INPUT, GL_INVALID_INDEX, sizeof(name), &length,
                             name);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    index = glGetProgramResourceIndex(program, GL_PROGRAM_OUTPUT, "oColor");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    glGetProgramResourceName(program, GL_PROGRAM_OUTPUT, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(9, length);
    EXPECT_EQ("oColor[0]", std::string(name));

    glGetProgramResourceName(program, GL_PROGRAM_OUTPUT, index, 8, &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(7, length);
    EXPECT_EQ("oColor[", std::string(name));
}

// Tests glGetProgramResourceLocation.
TEST_P(ProgramInterfaceTestES31, GetResourceLocation)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(location = 3) in highp vec4 position;\n"
        "in highp vec4 noLocationSpecified;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position;\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform vec4 color;\n"
        "layout(location = 2) out vec4 oColor[4];\n"
        "void main()\n"
        "{\n"
        "    oColor[0] = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLenum invalidInterfaces[] = {GL_UNIFORM_BLOCK, GL_TRANSFORM_FEEDBACK_VARYING,
                                  GL_BUFFER_VARIABLE, GL_SHADER_STORAGE_BLOCK,
                                  GL_ATOMIC_COUNTER_BUFFER};
    GLint location;
    for (auto &invalidInterface : invalidInterfaces)
    {
        location = glGetProgramResourceLocation(program, invalidInterface, "any");
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
        EXPECT_EQ(-1, location);
    }

    location = glGetProgramResourceLocation(program, GL_PROGRAM_INPUT, "position");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(3, location);

    location = glGetProgramResourceLocation(program, GL_PROGRAM_INPUT, "noLocationSpecified");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(-1, location);

    location = glGetProgramResourceLocation(program, GL_PROGRAM_INPUT, "missing");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(-1, location);

    location = glGetProgramResourceLocation(program, GL_PROGRAM_OUTPUT, "oColor");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(2, location);
    location = glGetProgramResourceLocation(program, GL_PROGRAM_OUTPUT, "oColor[0]");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(2, location);
    location = glGetProgramResourceLocation(program, GL_PROGRAM_OUTPUT, "oColor[3]");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(5, location);
}

// Tests glGetProgramResource.
TEST_P(ProgramInterfaceTestES31, GetResource)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(location = 3) in highp vec4 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position;\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform vec4 color;\n"
        "layout(location = 2) out vec4 oColor[4];\n"
        "void main()\n"
        "{\n"
        "    oColor[0] = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLuint index = glGetProgramResourceIndex(program, GL_PROGRAM_INPUT, "position");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    GLenum props[]    = {GL_TYPE,
                      GL_ARRAY_SIZE,
                      GL_LOCATION,
                      GL_NAME_LENGTH,
                      GL_REFERENCED_BY_VERTEX_SHADER,
                      GL_REFERENCED_BY_FRAGMENT_SHADER,
                      GL_REFERENCED_BY_COMPUTE_SHADER};
    GLsizei propCount = static_cast<GLsizei>(ArraySize(props));
    GLint params[ArraySize(props)];
    GLsizei length;

    glGetProgramResourceiv(program, GL_PROGRAM_INPUT, index, propCount, props, propCount, &length,
                           params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount, length);
    EXPECT_EQ(GL_FLOAT_VEC4, params[0]);  // type
    EXPECT_EQ(1, params[1]);              // array_size
    EXPECT_EQ(3, params[2]);              // location
    EXPECT_EQ(9, params[3]);              // name_length
    EXPECT_EQ(1, params[4]);              // referenced_by_vertex_shader
    EXPECT_EQ(0, params[5]);              // referenced_by_fragment_shader
    EXPECT_EQ(0, params[6]);              // referenced_by_compute_shader

    index = glGetProgramResourceIndex(program, GL_PROGRAM_OUTPUT, "oColor[0]");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(index, GL_INVALID_INDEX);
    // bufSize is smaller than propCount.
    glGetProgramResourceiv(program, GL_PROGRAM_OUTPUT, index, propCount, props, propCount - 1,
                           &length, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount - 1, length);
    EXPECT_EQ(GL_FLOAT_VEC4, params[0]);  // type
    EXPECT_EQ(4, params[1]);              // array_size
    EXPECT_EQ(2, params[2]);              // location
    EXPECT_EQ(10, params[3]);             // name_length
    EXPECT_EQ(0, params[4]);              // referenced_by_vertex_shader
    EXPECT_EQ(1, params[5]);              // referenced_by_fragment_shader

    GLenum invalidOutputProp = GL_OFFSET;
    glGetProgramResourceiv(program, GL_PROGRAM_OUTPUT, index, 1, &invalidOutputProp, 1, &length,
                           params);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Tests glGetProgramInterfaceiv.
TEST_P(ProgramInterfaceTestES31, GetProgramInterface)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "in highp vec4 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = position;\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform vec4 color;\n"
        "out vec4 oColor;\n"
        "uniform ub {\n"
        "    vec4 mem0;\n"
        "    vec4 mem1;\n"
        "} instance;\n"
        "void main()\n"
        "{\n"
        "    oColor = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLint num;
    glGetProgramInterfaceiv(program, GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, num);

    glGetProgramInterfaceiv(program, GL_PROGRAM_INPUT, GL_MAX_NAME_LENGTH, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(9, num);

    glGetProgramInterfaceiv(program, GL_PROGRAM_INPUT, GL_MAX_NUM_ACTIVE_VARIABLES, &num);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glGetProgramInterfaceiv(program, GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, num);

    glGetProgramInterfaceiv(program, GL_PROGRAM_OUTPUT, GL_MAX_NAME_LENGTH, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(7, num);

    glGetProgramInterfaceiv(program, GL_PROGRAM_OUTPUT, GL_MAX_NUM_ACTIVE_VARIABLES, &num);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, num);

    glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_MAX_NAME_LENGTH, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(3, num);

    glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_MAX_NUM_ACTIVE_VARIABLES, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(2, num);  // mem0, mem1

    glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(3, num);

    glGetProgramInterfaceiv(program, GL_UNIFORM, GL_MAX_NAME_LENGTH, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(8, num);  // "ub.mem0"

    glGetProgramInterfaceiv(program, GL_UNIFORM, GL_MAX_NUM_ACTIVE_VARIABLES, &num);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Tests the resource property query for uniform can be done correctly.
TEST_P(ProgramInterfaceTestES31, GetUniformProperties)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform layout(location=12) vec4 color;\n"
        "layout(binding = 2, offset = 4) uniform atomic_uint foo;\n"
        "void main()\n"
        "{\n"
        "    atomicCounterIncrement(foo);\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform vec4 color;\n"
        "out vec4 oColor;\n"
        "void main()\n"
        "{\n"
        "    oColor = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLuint index = glGetProgramResourceIndex(program, GL_UNIFORM, "color");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    GLchar name[64];
    GLsizei length;
    glGetProgramResourceName(program, GL_UNIFORM, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(5, length);
    EXPECT_EQ("color", std::string(name));

    GLint location = glGetProgramResourceLocation(program, GL_UNIFORM, "color");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(12, location);

    GLenum props[]    = {GL_TYPE,
                      GL_ARRAY_SIZE,
                      GL_LOCATION,
                      GL_NAME_LENGTH,
                      GL_REFERENCED_BY_VERTEX_SHADER,
                      GL_REFERENCED_BY_FRAGMENT_SHADER,
                      GL_REFERENCED_BY_COMPUTE_SHADER,
                      GL_ARRAY_STRIDE,
                      GL_BLOCK_INDEX,
                      GL_IS_ROW_MAJOR,
                      GL_MATRIX_STRIDE,
                      GL_OFFSET,
                      GL_ATOMIC_COUNTER_BUFFER_INDEX};
    GLsizei propCount = static_cast<GLsizei>(ArraySize(props));
    GLint params[ArraySize(props)];
    glGetProgramResourceiv(program, GL_UNIFORM, index, propCount, props, propCount, &length,
                           params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount, length);
    EXPECT_EQ(GL_FLOAT_VEC4, params[0]);  // type
    EXPECT_EQ(1, params[1]);              // array_size
    EXPECT_EQ(12, params[2]);             // location
    EXPECT_EQ(6, params[3]);              // name_length
    EXPECT_EQ(0, params[4]);              // referenced_by_vertex_shader
    EXPECT_EQ(1, params[5]);              // referenced_by_fragment_shader
    EXPECT_EQ(0, params[6]);              // referenced_by_compute_shader
    EXPECT_EQ(-1, params[7]);             // array_stride
    EXPECT_EQ(-1, params[8]);             // block_index
    EXPECT_EQ(0, params[9]);              // is_row_major
    EXPECT_EQ(-1, params[10]);            // matrix_stride
    EXPECT_EQ(-1, params[11]);            // offset
    EXPECT_EQ(-1, params[12]);            // atomic_counter_buffer_index

    index = glGetProgramResourceIndex(program, GL_UNIFORM, "foo");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    glGetProgramResourceName(program, GL_UNIFORM, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(3, length);
    EXPECT_EQ("foo", std::string(name));

    location = glGetProgramResourceLocation(program, GL_UNIFORM, "foo");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(-1, location);

    glGetProgramResourceiv(program, GL_UNIFORM, index, propCount, props, propCount, &length,
                           params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount, length);
    EXPECT_EQ(GL_UNSIGNED_INT_ATOMIC_COUNTER, params[0]);  // type
    EXPECT_EQ(1, params[1]);                               // array_size
    EXPECT_EQ(-1, params[2]);                              // location
    EXPECT_EQ(4, params[3]);                               // name_length
    EXPECT_EQ(1, params[4]);                               // referenced_by_vertex_shader
    EXPECT_EQ(0, params[5]);                               // referenced_by_fragment_shader
    EXPECT_EQ(0, params[6]);                               // referenced_by_compute_shader
    EXPECT_EQ(0, params[7]);                               // array_stride
    EXPECT_EQ(-1, params[8]);                              // block_index
    EXPECT_EQ(0, params[9]);                               // is_row_major
    EXPECT_EQ(0, params[10]);                              // matrix_stride
    EXPECT_EQ(4, params[11]);                              // offset
    EXPECT_NE(-1, params[12]);                             // atomic_counter_buffer_index
}

// Tests the resource property query for uniform block can be done correctly.
TEST_P(ProgramInterfaceTestES31, GetUniformBlockProperties)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "in vec2 position;\n"
        "out vec2 v;\n"
        "layout(binding = 2) uniform blockName {\n"
        "  float f1;\n"
        "  float f2;\n"
        "} instanceName;\n"
        "void main() {\n"
        "  v = vec2(instanceName.f1, instanceName.f2);\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "in vec2 v;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(v, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLuint index = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, "blockName");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    GLchar name[64];
    GLsizei length;
    glGetProgramResourceName(program, GL_UNIFORM_BLOCK, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(9, length);
    EXPECT_EQ("blockName", std::string(name));

    GLenum props[]         = {GL_BUFFER_BINDING,
                      GL_BUFFER_DATA_SIZE,
                      GL_NAME_LENGTH,
                      GL_NUM_ACTIVE_VARIABLES,
                      GL_ACTIVE_VARIABLES,
                      GL_REFERENCED_BY_VERTEX_SHADER,
                      GL_REFERENCED_BY_FRAGMENT_SHADER,
                      GL_REFERENCED_BY_COMPUTE_SHADER};
    GLsizei propCount      = static_cast<GLsizei>(ArraySize(props));
    constexpr int kBufSize = 256;
    GLint params[kBufSize];
    GLint magic = 0xBEEF;

    // Tests bufSize is respected even some prop returns more than one value.
    params[propCount] = magic;
    glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, index, propCount, props, propCount, &length,
                           params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount, length);
    EXPECT_EQ(2, params[0]);   // buffer_binding
    EXPECT_NE(0, params[1]);   // buffer_data_size
    EXPECT_EQ(10, params[2]);  // name_length
    EXPECT_EQ(2, params[3]);   // num_active_variables
    EXPECT_LE(0, params[4]);   // index of 'f1' or 'f2'
    EXPECT_LE(0, params[5]);   // index of 'f1' or 'f2'
    EXPECT_EQ(1, params[6]);   // referenced_by_vertex_shader
    EXPECT_EQ(0, params[7]);   // referenced_by_fragment_shader
    EXPECT_EQ(magic, params[8]);

    glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, index, propCount, props, kBufSize, &length,
                           params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount + 1, length);
    EXPECT_EQ(0, params[8]);  // referenced_by_compute_shader

    // bufSize is reached in middle of outputting values for GL_ACTIVE_VARIABLES.
    GLenum actvieVariablesProperty = GL_ACTIVE_VARIABLES;
    params[1]                      = magic;
    glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, index, 1, &actvieVariablesProperty, 1,
                           &length, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, length);
    EXPECT_LE(0, params[0]);  // index of 'f1' or 'f2'
    EXPECT_EQ(magic, params[1]);
}

// Tests atomic counter buffer qeury works correctly.
TEST_P(ProgramInterfaceTestES31, QueryAtomicCounteBuffer)
{
    const std::string &vertShader =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 2, offset = 0) uniform atomic_uint vcounter;\n"
        "in highp vec4 a_position;\n"
        "void main()\n"
        "{\n"
        "    atomicCounterIncrement(vcounter);\n"
        "    gl_Position = a_position;\n"
        "}\n";

    const std::string &fragShader =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 2, offset = 4) uniform atomic_uint fcounter;\n"
        "out highp vec4 my_color;\n"
        "void main()\n"
        "{\n"
        "    atomicCounterDecrement(fcounter);\n"
        "    my_color = vec4(0.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vertShader, fragShader);
    GLint num;
    glGetProgramInterfaceiv(program, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, num);

    glGetProgramInterfaceiv(program, GL_ATOMIC_COUNTER_BUFFER, GL_MAX_NUM_ACTIVE_VARIABLES, &num);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(2, num);

    GLenum props[]    = {GL_BUFFER_BINDING, GL_NUM_ACTIVE_VARIABLES, GL_REFERENCED_BY_VERTEX_SHADER,
                      GL_REFERENCED_BY_FRAGMENT_SHADER, GL_REFERENCED_BY_COMPUTE_SHADER};
    GLsizei propCount = static_cast<GLsizei>(ArraySize(props));
    GLint params[ArraySize(props)];
    GLsizei length = 0;
    glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, 0, propCount, props, propCount,
                           &length, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount, length);
    EXPECT_EQ(2, params[0]);  // buffer_binding
    EXPECT_EQ(2, params[1]);  // num_active_variables
    EXPECT_EQ(1, params[2]);  // referenced_by_vertex_shader
    EXPECT_EQ(1, params[3]);  // referenced_by_fragment_shader
    EXPECT_EQ(0, params[4]);  // referenced_by_compute_shader
}

// Tests the resource property query for buffer variable can be done correctly.
TEST_P(ProgramInterfaceTestES31, GetBufferVariableProperties)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "struct S {\n"
        "    vec3 a;\n"
        "    ivec2 b[4];\n"
        "};\n"
        "layout(std140) buffer blockName0 {\n"
        "    S s0;\n"
        "    vec2 v0;\n"
        "    S s1[2];\n"
        "    uint u0;\n"
        "};\n"
        "layout(binding = 1) buffer blockName1 {\n"
        "    uint u1[2];\n"
        "    float f1;\n"
        "} instanceName1[2];\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(instanceName1[0].f1, s1[0].a);\n"
        "}\n";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 1) buffer blockName1 {\n"
        "    uint u1[2];\n"
        "    float f1;\n"
        "} instanceName1[2];\n"
        "out vec4 oColor;\n"
        "void main()\n"
        "{\n"
        "    oColor = vec4(instanceName1[0].f1, 0, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLuint index = glGetProgramResourceIndex(program, GL_BUFFER_VARIABLE, "blockName1.f1");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    GLchar name[64];
    GLsizei length;
    glGetProgramResourceName(program, GL_BUFFER_VARIABLE, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(13, length);
    EXPECT_EQ("blockName1.f1", std::string(name));

    GLenum props[]         = {GL_ARRAY_SIZE,
                      GL_ARRAY_STRIDE,
                      GL_BLOCK_INDEX,
                      GL_IS_ROW_MAJOR,
                      GL_MATRIX_STRIDE,
                      GL_NAME_LENGTH,
                      GL_OFFSET,
                      GL_REFERENCED_BY_VERTEX_SHADER,
                      GL_REFERENCED_BY_FRAGMENT_SHADER,
                      GL_REFERENCED_BY_COMPUTE_SHADER,
                      GL_TOP_LEVEL_ARRAY_SIZE,
                      GL_TOP_LEVEL_ARRAY_STRIDE,
                      GL_TYPE};
    GLsizei propCount      = static_cast<GLsizei>(ArraySize(props));
    constexpr int kBufSize = 256;
    GLint params[kBufSize];

    glGetProgramResourceiv(program, GL_BUFFER_VARIABLE, index, propCount, props, kBufSize, &length,
                           params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount, length);
    EXPECT_EQ(1, params[0]);   // array_size
    EXPECT_LE(0, params[1]);   // array_stride
    EXPECT_LE(0, params[2]);   // block_index
    EXPECT_EQ(0, params[3]);   // is_row_major
    EXPECT_EQ(0, params[4]);   // matrix_stride
    EXPECT_EQ(14, params[5]);  // name_length
    EXPECT_LE(0, params[6]);   // offset

    // TODO(jiajia.qin@intel.com): Enable them once the block member staticUse are implemented.
    // EXPECT_EQ(1, params[7]);  // referenced_by_vertex_shader
    // EXPECT_EQ(1, params[8]);  // referenced_by_fragment_shader
    // EXPECT_EQ(0, params[9]);  // referenced_by_compute_shader

    EXPECT_EQ(1, params[10]);  // top_level_array_size
    EXPECT_LE(0, params[11]);  // top_level_array_stride

    EXPECT_EQ(GL_FLOAT, params[12]);  // type

    index = glGetProgramResourceIndex(program, GL_BUFFER_VARIABLE, "s1[0].a");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    glGetProgramResourceName(program, GL_BUFFER_VARIABLE, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(7, length);
    EXPECT_EQ("s1[0].a", std::string(name));

    glGetProgramResourceiv(program, GL_BUFFER_VARIABLE, index, propCount, props, kBufSize, &length,
                           params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(propCount, length);
    EXPECT_EQ(1, params[0]);  // array_size
    EXPECT_LE(0, params[1]);  // array_stride
    EXPECT_LE(0, params[2]);  // block_index
    EXPECT_EQ(0, params[3]);  // is_row_major
    EXPECT_EQ(0, params[4]);  // matrix_stride
    EXPECT_EQ(8, params[5]);  // name_length
    EXPECT_LE(0, params[6]);  // offset

    // TODO(jiajia.qin@intel.com): Enable them once the block member staticUse are implemented.
    // EXPECT_EQ(1, params[7]);  // referenced_by_vertex_shader
    // EXPECT_EQ(0, params[8]);  // referenced_by_fragment_shader
    // EXPECT_EQ(0, params[9]);  // referenced_by_compute_shader

    EXPECT_EQ(2, params[10]);   // top_level_array_size
    EXPECT_EQ(80, params[11]);  // top_level_array_stride

    EXPECT_EQ(GL_FLOAT_VEC3, params[12]);  // type
}

// Tests the resource property query for shader storage block can be done correctly.
TEST_P(ProgramInterfaceTestES31, GetShaderStorageBlockProperties)
{
    const std::string &vertexShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "struct S {\n"
        "    vec3 a;\n"
        "    ivec2 b[4];\n"
        "};\n"
        "layout(std140) buffer blockName0 {\n"
        "    S s0;\n"
        "    vec2 v0;\n"
        "    S s1[2];\n"
        "    uint u0;\n"
        "};\n"
        "layout(binding = 1) buffer blockName1 {\n"
        "    uint u1[2];\n"
        "    float f1;\n"
        "} instanceName1[2];\n"
        "layout(binding = 2) buffer blockName2 {\n"
        "    uint u2;\n"
        "    float f2;\n"
        "};\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(instanceName1[0].f1, s1[0].a);\n"
        "}\n";

    const std::string &fragmentShaderSource =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform vec4 color;\n"
        "out vec4 oColor;\n"
        "void main()\n"
        "{\n"
        "    oColor = color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShaderSource, fragmentShaderSource);

    GLuint index = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "blockName0");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    GLchar name[64];
    GLsizei length;
    glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(10, length);
    EXPECT_EQ("blockName0", std::string(name));

    GLenum props[]         = {GL_ACTIVE_VARIABLES,
                      GL_BUFFER_BINDING,
                      GL_NUM_ACTIVE_VARIABLES,
                      GL_BUFFER_DATA_SIZE,
                      GL_NAME_LENGTH,
                      GL_REFERENCED_BY_VERTEX_SHADER,
                      GL_REFERENCED_BY_FRAGMENT_SHADER,
                      GL_REFERENCED_BY_COMPUTE_SHADER};
    GLsizei propCount      = static_cast<GLsizei>(ArraySize(props));
    constexpr int kBufSize = 256;
    GLint params[kBufSize];

    glGetProgramResourceiv(program, GL_SHADER_STORAGE_BLOCK, index, propCount, props, kBufSize,
                           &length, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(13, length);
    EXPECT_LE(0, params[0]);   // active_variables s0.a
    EXPECT_LE(0, params[1]);   // active_variables s0.b
    EXPECT_LE(0, params[2]);   // active_variables v0
    EXPECT_LE(0, params[3]);   // active_variables s1[0].a
    EXPECT_LE(0, params[4]);   // active_variables s1[0].b
    EXPECT_LE(0, params[5]);   // active_variables u0
    EXPECT_EQ(0, params[6]);   // buffer_binding
    EXPECT_EQ(6, params[7]);   // num_active_variables
    EXPECT_LE(0, params[8]);   // buffer_data_size
    EXPECT_EQ(11, params[9]);  // name_length

    EXPECT_EQ(1, params[10]);  // referenced_by_vertex_shader
    EXPECT_EQ(0, params[11]);  // referenced_by_fragment_shader
    EXPECT_EQ(0, params[12]);  // referenced_by_compute_shader

    index = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "blockName1");
    EXPECT_GL_NO_ERROR();
    EXPECT_NE(GL_INVALID_INDEX, index);

    glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, index, sizeof(name), &length, name);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(13, length);
    EXPECT_EQ("blockName1[0]", std::string(name));
}

ANGLE_INSTANTIATE_TEST(ProgramInterfaceTestES31, ES31_OPENGL(), ES31_OPENGLES());

}  // anonymous namespace
