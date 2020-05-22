//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramPipelineTest:
//   Various tests related to Program Pipeline.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class ProgramPipelineTest : public ANGLETest
{
  protected:
    ProgramPipelineTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Verify that program pipeline is not supported in version lower than ES31.
TEST_P(ProgramPipelineTest, GenerateProgramPipelineObject)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    GLuint pipeline;
    glGenProgramPipelines(1, &pipeline);
    if (getClientMajorVersion() < 3 || getClientMinorVersion() < 1)
    {
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
    else
    {
        EXPECT_GL_NO_ERROR();

        glDeleteProgramPipelines(1, &pipeline);
        EXPECT_GL_NO_ERROR();
    }
}

class ProgramPipelineTest31 : public ProgramPipelineTest
{
  protected:
    ~ProgramPipelineTest31()
    {
        glDeleteProgram(mVertProg);
        glDeleteProgram(mFragProg);
        glDeleteProgramPipelines(1, &mPipeline);
    }

    void bindProgramPipeline(const GLchar *vertString, const GLchar *fragString);
    void drawQuad(const std::string &positionAttribName,
                  const GLfloat positionAttribZ,
                  const GLfloat positionAttribXYScale);

    GLuint mVertProg;
    GLuint mFragProg;
    GLuint mPipeline;
};

void ProgramPipelineTest31::bindProgramPipeline(const GLchar *vertString, const GLchar *fragString)
{
    mVertProg = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vertString);
    ASSERT_NE(mVertProg, 0u);
    mFragProg = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragString);
    ASSERT_NE(mFragProg, 0u);

    // Generate a program pipeline and attach the programs to their respective stages
    glGenProgramPipelines(1, &mPipeline);
    EXPECT_GL_NO_ERROR();
    glUseProgramStages(mPipeline, GL_VERTEX_SHADER_BIT, mVertProg);
    EXPECT_GL_NO_ERROR();
    glUseProgramStages(mPipeline, GL_FRAGMENT_SHADER_BIT, mFragProg);
    EXPECT_GL_NO_ERROR();
    glBindProgramPipeline(mPipeline);
    EXPECT_GL_NO_ERROR();
}

// Test generate or delete program pipeline.
TEST_P(ProgramPipelineTest31, GenOrDeleteProgramPipelineTest)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    GLuint pipeline;
    glGenProgramPipelines(-1, &pipeline);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glGenProgramPipelines(0, &pipeline);
    EXPECT_GL_NO_ERROR();

    glDeleteProgramPipelines(-1, &pipeline);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glDeleteProgramPipelines(0, &pipeline);
    EXPECT_GL_NO_ERROR();
}

// Test BindProgramPipeline.
TEST_P(ProgramPipelineTest31, BindProgramPipelineTest)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    glBindProgramPipeline(0);
    EXPECT_GL_NO_ERROR();

    glBindProgramPipeline(2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    GLuint pipeline;
    glGenProgramPipelines(1, &pipeline);
    glBindProgramPipeline(pipeline);
    EXPECT_GL_NO_ERROR();

    glDeleteProgramPipelines(1, &pipeline);
    glBindProgramPipeline(pipeline);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test IsProgramPipeline
TEST_P(ProgramPipelineTest31, IsProgramPipelineTest)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    EXPECT_GL_FALSE(glIsProgramPipeline(0));
    EXPECT_GL_NO_ERROR();

    EXPECT_GL_FALSE(glIsProgramPipeline(2));
    EXPECT_GL_NO_ERROR();

    GLuint pipeline;
    glGenProgramPipelines(1, &pipeline);
    glBindProgramPipeline(pipeline);
    EXPECT_GL_TRUE(glIsProgramPipeline(pipeline));
    EXPECT_GL_NO_ERROR();

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &pipeline);
    EXPECT_GL_FALSE(glIsProgramPipeline(pipeline));
    EXPECT_GL_NO_ERROR();
}

// Simulates a call to glCreateShaderProgramv()
GLuint createShaderProgram(GLenum type, const GLchar *shaderString)
{
    GLShader shader(type);
    if (!shader.get())
    {
        return 0;
    }

    glShaderSource(shader, 1, &shaderString, nullptr);
    glCompileShader(shader);

    GLuint program = glCreateProgram();

    if (program)
    {
        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
        if (compiled)
        {
            glAttachShader(program, shader);
            glLinkProgram(program);
            glDetachShader(program, shader);
        }
    }

    EXPECT_GL_NO_ERROR();

    return program;
}

void ProgramPipelineTest31::drawQuad(const std::string &positionAttribName,
                                     const GLfloat positionAttribZ,
                                     const GLfloat positionAttribXYScale)
{
    glUseProgram(0);

    std::array<Vector3, 6> quadVertices = ANGLETestBase::GetQuadVertices();

    for (Vector3 &vertex : quadVertices)
    {
        vertex.x() *= positionAttribXYScale;
        vertex.y() *= positionAttribXYScale;
        vertex.z() = positionAttribZ;
    }

    GLint positionLocation = glGetAttribLocation(mVertProg, positionAttribName.c_str());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, quadVertices.data());
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
}

// Test glUseProgramStages
TEST_P(ProgramPipelineTest31, UseProgramStages)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // Create two separable program objects from a
    // single source string respectively (vertSrc and fragSrc)
    const GLchar *vertString = essl31_shaders::vs::Simple();
    const GLchar *fragString = essl31_shaders::fs::Red();

    mVertProg = createShaderProgram(GL_VERTEX_SHADER, vertString);
    ASSERT_NE(mVertProg, 0u);
    mFragProg = createShaderProgram(GL_FRAGMENT_SHADER, fragString);
    ASSERT_NE(mFragProg, 0u);

    // Generate a program pipeline and attach the programs to their respective stages
    GLuint pipeline;
    glGenProgramPipelines(1, &pipeline);
    EXPECT_GL_NO_ERROR();
    glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, mVertProg);
    EXPECT_GL_NO_ERROR();
    glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, mFragProg);
    EXPECT_GL_NO_ERROR();
    glBindProgramPipeline(pipeline);
    EXPECT_GL_NO_ERROR();

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Test glUseProgramStages
TEST_P(ProgramPipelineTest31, UseCreateShaderProgramv)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // Create two separable program objects from a
    // single source string respectively (vertSrc and fragSrc)
    const GLchar *vertString = essl31_shaders::vs::Simple();
    const GLchar *fragString = essl31_shaders::fs::Red();

    bindProgramPipeline(vertString, fragString);

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Test glUniform
TEST_P(ProgramPipelineTest31, FragmentStageUniformTest)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // Create two separable program objects from a
    // single source string respectively (vertSrc and fragSrc)
    const GLchar *vertString = essl31_shaders::vs::Simple();
    const GLchar *fragString = R"(#version 310 es
precision highp float;
uniform float redColorIn;
uniform float greenColorIn;
out vec4 my_FragColor;
void main()
{
    my_FragColor = vec4(redColorIn, greenColorIn, 0.0, 1.0);
})";

    bindProgramPipeline(vertString, fragString);

    // Set the output color to yellow
    GLint location = glGetUniformLocation(mFragProg, "redColorIn");
    glActiveShaderProgram(mPipeline, mFragProg);
    glUniform1f(location, 1.0);
    location = glGetUniformLocation(mFragProg, "greenColorIn");
    glActiveShaderProgram(mPipeline, mFragProg);
    glUniform1f(location, 1.0);

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set the output color to red
    location = glGetUniformLocation(mFragProg, "redColorIn");
    glActiveShaderProgram(mPipeline, mFragProg);
    glUniform1f(location, 1.0);
    location = glGetUniformLocation(mFragProg, "greenColorIn");
    glActiveShaderProgram(mPipeline, mFragProg);
    glUniform1f(location, 0.0);

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    glDeleteProgram(mVertProg);
    glDeleteProgram(mFragProg);
}

// Test varyings
TEST_P(ProgramPipelineTest31, ProgramPipelineVaryings)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // Create two separable program objects from a
    // single source string respectively (vertSrc and fragSrc)
    const GLchar *vertString = essl31_shaders::vs::Passthrough();
    const GLchar *fragString = R"(#version 310 es
precision highp float;
in vec4 v_position;
out vec4 my_FragColor;
void main()
{
    my_FragColor = round(v_position);
})";

    bindProgramPipeline(vertString, fragString);

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();

    int w = getWindowWidth() - 2;
    int h = getWindowHeight() - 2;

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
    EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);
}

// Creates a program pipeline with a 2D texture and renders with it.
TEST_P(ProgramPipelineTest31, DrawWith2DTexture)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    const GLchar *vertString = R"(#version 310 es
precision highp float;
in vec4 a_position;
out vec2 texCoord;
void main()
{
    gl_Position = a_position;
    texCoord = vec2(a_position.x, a_position.y) * 0.5 + vec2(0.5);
})";

    const GLchar *fragString = R"(#version 310 es
precision highp float;
in vec2 texCoord;
uniform sampler2D tex;
out vec4 my_FragColor;
void main()
{
    my_FragColor = texture(tex, texCoord);
})";

    std::array<GLColor, 4> colors = {
        {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow}};

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    bindProgramPipeline(vertString, fragString);

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();

    int w = getWindowWidth() - 2;
    int h = getWindowHeight() - 2;

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);
}

// Test modifying a shader after it has been detached from a pipeline
TEST_P(ProgramPipelineTest31, DetachAndModifyShader)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // TODO (timvp): Fix this test for Vulkan with PPO
    // http://anglebug.com/3570
    ANGLE_SKIP_TEST_IF(IsVulkan());

    const GLchar *vertString = essl31_shaders::vs::Simple();
    const GLchar *fragString = essl31_shaders::fs::Green();

    GLShader vertShader(GL_VERTEX_SHADER);
    GLShader fragShader(GL_FRAGMENT_SHADER);
    mVertProg = glCreateProgram();
    mFragProg = glCreateProgram();

    // Compile and link a separable vertex shader
    glShaderSource(vertShader, 1, &vertString, nullptr);
    glCompileShader(vertShader);
    glProgramParameteri(mVertProg, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glAttachShader(mVertProg, vertShader);
    glLinkProgram(mVertProg);
    EXPECT_GL_NO_ERROR();

    // Compile and link a separable fragment shader
    glShaderSource(fragShader, 1, &fragString, nullptr);
    glCompileShader(fragShader);
    glProgramParameteri(mFragProg, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glAttachShader(mFragProg, fragShader);
    glLinkProgram(mFragProg);
    EXPECT_GL_NO_ERROR();

    // Generate a program pipeline and attach the programs
    glGenProgramPipelines(1, &mPipeline);
    glUseProgramStages(mPipeline, GL_VERTEX_SHADER_BIT, mVertProg);
    glUseProgramStages(mPipeline, GL_FRAGMENT_SHADER_BIT, mFragProg);
    glBindProgramPipeline(mPipeline);
    EXPECT_GL_NO_ERROR();

    // Draw once to ensure this worked fine
    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Detach the fragment shader and modify it such that it no longer fits with this pipeline
    glDetachShader(mFragProg, fragShader);

    // Add an input to the fragment shader, which will make it incompatible
    const GLchar *fragString2 = R"(#version 310 es
precision highp float;
in vec4 color;
out vec4 my_FragColor;
void main()
{
    my_FragColor = color;
})";
    glShaderSource(fragShader, 1, &fragString2, nullptr);
    glCompileShader(fragShader);

    // Link and draw with the program again, which should be fine since the shader was detached
    glLinkProgram(mFragProg);

    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
}

// Test binding two programs that use a texture as different types
TEST_P(ProgramPipelineTest31, DifferentTextureTypes)
{
    // Only the Vulkan backend supports PPO
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // TODO (timvp): Fix this test for Vulkan with PPO
    // http://anglebug.com/3570
    ANGLE_SKIP_TEST_IF(IsVulkan());

    // Per the OpenGL ES 3.1 spec:
    //
    // It is not allowed to have variables of different sampler types pointing to the same texture
    // image unit within a program object. This situation can only be detected at the next rendering
    // command issued which triggers shader invocations, and an INVALID_OPERATION error will then
    // be generated
    //

    // Create a vertex shader that uses the texture as 2D
    const GLchar *vertString = R"(#version 310 es
precision highp float;
in vec4 a_position;
uniform sampler2D tex2D;
layout(location = 0) out vec4 texColorOut;
layout(location = 1) out vec2 texCoordOut;
void main()
{
    gl_Position = a_position;
    vec2 texCoord = vec2(a_position.x, a_position.y) * 0.5 + vec2(0.5);
    texColorOut = textureLod(tex2D, texCoord, 0.0);
    texCoordOut = texCoord;
})";

    // Create a fragment shader that uses the texture as Cube
    const GLchar *fragString = R"(#version 310 es
precision highp float;
layout(location = 0) in vec4 texColor;
layout(location = 1) in vec2 texCoord;
uniform samplerCube texCube;
out vec4 my_FragColor;
void main()
{
    my_FragColor = texture(texCube, vec3(texCoord.x, texCoord.y, 0.0));
})";

    // Create and populate the 2D texture
    std::array<GLColor, 4> colors = {
        {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow}};
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Create a pipeline that uses the bad combination.  This should fail to link the pipeline.
    bindProgramPipeline(vertString, fragString);
    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Update the fragment shader to correctly use 2D texture
    const GLchar *fragString2 = R"(#version 310 es
precision highp float;
layout(location = 0) in vec4 texColor;
layout(location = 1) in vec2 texCoord;
uniform sampler2D tex2D;
out vec4 my_FragColor;
void main()
{
    my_FragColor = texture(tex2D, texCoord);
})";

    // Bind the pipeline again, which should succeed.
    bindProgramPipeline(vertString, fragString2);
    ProgramPipelineTest31::drawQuad("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(ProgramPipelineTest);
ANGLE_INSTANTIATE_TEST_ES31(ProgramPipelineTest31);

}  // namespace
