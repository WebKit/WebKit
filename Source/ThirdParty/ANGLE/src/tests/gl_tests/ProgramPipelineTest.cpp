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
{};

// Test generate or delete program pipeline.
TEST_P(ProgramPipelineTest31, GenOrDeleteProgramPipelineTest)
{
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

ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(ProgramPipelineTest);
ANGLE_INSTANTIATE_TEST_ES31(ProgramPipelineTest31);

}  // namespace
