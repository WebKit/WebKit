//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include <memory>
#include <stdint.h>

#include "EGLWindow.h"
#include "OSWindow.h"
#include "test_utils/angle_test_configs.h"

using namespace angle;

class ProgramBinaryTest : public ANGLETest
{
  protected:
    ProgramBinaryTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const std::string vertexShaderSource = SHADER_SOURCE
        (
            attribute vec4 inputAttribute;
            void main()
            {
                gl_Position = inputAttribute;
            }
        );

        const std::string fragmentShaderSource = SHADER_SOURCE
        (
            void main()
            {
                gl_FragColor = vec4(1,0,0,1);
            }
        );

        mProgram = CompileProgram(vertexShaderSource, fragmentShaderSource);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        glGenBuffers(1, &mBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
        glBufferData(GL_ARRAY_BUFFER, 128, NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);
        glDeleteBuffers(1, &mBuffer);

        ANGLETest::TearDown();
    }

    GLint getAvailableProgramBinaryFormatCount() const
    {
        GLint formatCount;
        glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS_OES, &formatCount);
        return formatCount;
    }

    GLuint mProgram;
    GLuint mBuffer;
};

// This tests the assumption that float attribs of different size
// should not internally cause a vertex shader recompile (for conversion).
TEST_P(ProgramBinaryTest, FloatDynamicShaderSize)
{
    if (!extensionEnabled("GL_OES_get_program_binary"))
    {
        std::cout << "Test skipped because GL_OES_get_program_binary is not available."
                  << std::endl;
        return;
    }

    if (getAvailableProgramBinaryFormatCount() == 0)
    {
        std::cout << "Test skipped because no program binary formats are available." << std::endl;
        return;
    }

    glUseProgram(mProgram);
    glBindBuffer(GL_ARRAY_BUFFER, mBuffer);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8, NULL);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_POINTS, 0, 1);

    GLint programLength;
    glGetProgramiv(mProgram, GL_PROGRAM_BINARY_LENGTH_OES, &programLength);

    EXPECT_GL_NO_ERROR();

    for (GLsizei size = 1; size <= 3; size++)
    {
        glVertexAttribPointer(0, size, GL_FLOAT, GL_FALSE, 8, NULL);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_POINTS, 0, 1);

        GLint newProgramLength;
        glGetProgramiv(mProgram, GL_PROGRAM_BINARY_LENGTH_OES, &newProgramLength);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(programLength, newProgramLength);
    }
}

// Tests that switching between signed and unsigned un-normalized data doesn't trigger a bug
// in the D3D11 back-end.
TEST_P(ProgramBinaryTest, DynamicShadersSignatureBug)
{
    glUseProgram(mProgram);
    glBindBuffer(GL_ARRAY_BUFFER, mBuffer);

    GLint attribLocation = glGetAttribLocation(mProgram, "inputAttribute");
    ASSERT_NE(-1, attribLocation);
    glEnableVertexAttribArray(attribLocation);

    glVertexAttribPointer(attribLocation, 2, GL_BYTE, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_POINTS, 0, 1);

    glVertexAttribPointer(attribLocation, 2, GL_UNSIGNED_BYTE, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_POINTS, 0, 1);
}

// This tests the ability to successfully save and load a program binary.
TEST_P(ProgramBinaryTest, SaveAndLoadBinary)
{
    if (!extensionEnabled("GL_OES_get_program_binary"))
    {
        std::cout << "Test skipped because GL_OES_get_program_binary is not available."
                  << std::endl;
        return;
    }

    if (getAvailableProgramBinaryFormatCount() == 0)
    {
        std::cout << "Test skipped because no program binary formats are available." << std::endl;
        return;
    }

    GLint programLength = 0;
    GLint writtenLength = 0;
    GLenum binaryFormat = 0;

    glGetProgramiv(mProgram, GL_PROGRAM_BINARY_LENGTH_OES, &programLength);
    EXPECT_GL_NO_ERROR();

    std::vector<uint8_t> binary(programLength);
    glGetProgramBinaryOES(mProgram, programLength, &writtenLength, &binaryFormat, binary.data());
    EXPECT_GL_NO_ERROR();

    // The lengths reported by glGetProgramiv and glGetProgramBinaryOES should match
    EXPECT_EQ(programLength, writtenLength);

    if (writtenLength)
    {
        GLuint program2 = glCreateProgram();
        glProgramBinaryOES(program2, binaryFormat, binary.data(), writtenLength);

        EXPECT_GL_NO_ERROR();

        GLint linkStatus;
        glGetProgramiv(program2, GL_LINK_STATUS, &linkStatus);
        if (linkStatus == 0)
        {
            GLint infoLogLength;
            glGetProgramiv(program2, GL_INFO_LOG_LENGTH, &infoLogLength);

            if (infoLogLength > 0)
            {
                std::vector<GLchar> infoLog(infoLogLength);
                glGetProgramInfoLog(program2, static_cast<GLsizei>(infoLog.size()), NULL,
                                    &infoLog[0]);
                FAIL() << "program link failed: " << &infoLog[0];
            }
            else
            {
                FAIL() << "program link failed.";
            }
        }
        else
        {
            glUseProgram(program2);
            glBindBuffer(GL_ARRAY_BUFFER, mBuffer);

            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8, NULL);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_POINTS, 0, 1);

            EXPECT_GL_NO_ERROR();
        }

        glDeleteProgram(program2);
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(ProgramBinaryTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL());

class ProgramBinaryTransformFeedbackTest : public ANGLETest
{
  protected:
    ProgramBinaryTransformFeedbackTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const std::string vertexShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            in vec4 inputAttribute;
            out vec4 outputVarying;
            void main()
            {
                outputVarying = inputAttribute;
            }
        );

        const std::string fragmentShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            precision highp float;
            out vec4 outputColor;
            void main()
            {
                outputColor = vec4(1,0,0,1);
            }
        );

        std::vector<std::string> transformFeedbackVaryings;
        transformFeedbackVaryings.push_back("outputVarying");

        mProgram = CompileProgramWithTransformFeedback(
            vertexShaderSource, fragmentShaderSource, transformFeedbackVaryings,
            GL_SEPARATE_ATTRIBS);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);

        ANGLETest::TearDown();
    }

    GLint getAvailableProgramBinaryFormatCount() const
    {
        GLint formatCount;
        glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS_OES, &formatCount);
        return formatCount;
    }

    GLuint mProgram;
};

// This tests the assumption that float attribs of different size
// should not internally cause a vertex shader recompile (for conversion).
TEST_P(ProgramBinaryTransformFeedbackTest, GetTransformFeedbackVarying)
{
    if (!extensionEnabled("GL_OES_get_program_binary"))
    {
        std::cout << "Test skipped because GL_OES_get_program_binary is not available."
                  << std::endl;
        return;
    }

    if (getAvailableProgramBinaryFormatCount() == 0)
    {
        std::cout << "Test skipped because no program binary formats are available." << std::endl;
        return;
    }

    std::vector<uint8_t> binary(0);
    GLint programLength = 0;
    GLint writtenLength = 0;
    GLenum binaryFormat = 0;

    // Save the program binary out
    glGetProgramiv(mProgram, GL_PROGRAM_BINARY_LENGTH_OES, &programLength);
    ASSERT_GL_NO_ERROR();
    binary.resize(programLength);
    glGetProgramBinaryOES(mProgram, programLength, &writtenLength, &binaryFormat, binary.data());
    ASSERT_GL_NO_ERROR();

    glDeleteProgram(mProgram);

    // Load program binary
    mProgram = glCreateProgram();
    glProgramBinaryOES(mProgram, binaryFormat, binary.data(), writtenLength);

    // Ensure the loaded binary is linked
    GLint linkStatus;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linkStatus);
    EXPECT_TRUE(linkStatus != 0);

    // Query information about the transform feedback varying
    char varyingName[64];
    GLsizei varyingSize = 0;
    GLenum varyingType = GL_NONE;

    glGetTransformFeedbackVarying(mProgram, 0, 64, &writtenLength, &varyingSize, &varyingType, varyingName);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(13, writtenLength);
    EXPECT_STREQ("outputVarying", varyingName);
    EXPECT_EQ(1, varyingSize);
    EXPECT_GLENUM_EQ(GL_FLOAT_VEC4, varyingType);

    EXPECT_GL_NO_ERROR();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(ProgramBinaryTransformFeedbackTest,
                       ES3_D3D11(),
                       ES3_OPENGL());

// For the ProgramBinariesAcrossPlatforms tests, we need two sets of params:
// - a set to save the program binary
// - a set to load the program binary
// We combine these into one struct extending PlatformParameters so we can reuse existing ANGLE test macros
struct PlatformsWithLinkResult : PlatformParameters
{
    PlatformsWithLinkResult(PlatformParameters saveParams, PlatformParameters loadParamsIn, bool expectedLinkResultIn)
    {
        majorVersion = saveParams.majorVersion;
        minorVersion = saveParams.minorVersion;
        eglParameters = saveParams.eglParameters;
        loadParams = loadParamsIn;
        expectedLinkResult = expectedLinkResultIn;
    }

    PlatformParameters loadParams;
    bool expectedLinkResult;
};

// Provide a custom gtest parameter name function for PlatformsWithLinkResult
// to avoid returning the same parameter name twice. Such a conflict would happen
// between ES2_D3D11_to_ES2D3D11 and ES2_D3D11_to_ES3D3D11 as they were both
// named ES2_D3D11
std::ostream &operator<<(std::ostream& stream, const PlatformsWithLinkResult &platform)
{
    const PlatformParameters &platform1 = platform;
    const PlatformParameters &platform2 = platform.loadParams;
    stream << platform1 << "_to_" << platform2;
    return stream;
}

class ProgramBinariesAcrossPlatforms : public testing::TestWithParam<PlatformsWithLinkResult>
{
  public:
    void SetUp() override
    {
        mOSWindow = CreateOSWindow();
        bool result = mOSWindow->initialize("ProgramBinariesAcrossRenderersTests", 100, 100);

        if (result == false)
        {
            FAIL() << "Failed to create OS window";
        }
    }

    EGLWindow *createAndInitEGLWindow(angle::PlatformParameters &param)
    {
        EGLWindow *eglWindow =
            new EGLWindow(param.majorVersion, param.minorVersion, param.eglParameters);
        bool result = eglWindow->initializeGL(mOSWindow);
        if (result == false)
        {
            SafeDelete(eglWindow);
            eglWindow = nullptr;
        }

        return eglWindow;
    }

    void destroyEGLWindow(EGLWindow **eglWindow)
    {
        ASSERT(*eglWindow != nullptr);
        (*eglWindow)->destroyGL();
        SafeDelete(*eglWindow);
        *eglWindow = nullptr;
    }

    GLuint createES2ProgramFromSource()
    {
        const std::string testVertexShaderSource = SHADER_SOURCE
        (
            attribute highp vec4 position;

            void main(void)
            {
                gl_Position = position;
            }
        );

        const std::string testFragmentShaderSource = SHADER_SOURCE
        (
            void main(void)
            {
                gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
        );

        return CompileProgram(testVertexShaderSource, testFragmentShaderSource);
    }

    GLuint createES3ProgramFromSource()
    {
        const std::string testVertexShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            precision highp float;
            in highp vec4 position;

            void main(void)
            {
                gl_Position = position;
            }
        );

        const std::string testFragmentShaderSource = SHADER_SOURCE
        (   #version 300 es \n
            precision highp float;
            out vec4 out_FragColor;

            void main(void)
            {
                out_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
        );

        return CompileProgram(testVertexShaderSource, testFragmentShaderSource);
    }

    void drawWithProgram(GLuint program)
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        GLint positionLocation = glGetAttribLocation(program, "position");

        glUseProgram(program);

        const GLfloat vertices[] =
        {
            -1.0f,  1.0f, 0.5f,
            -1.0f, -1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,

            -1.0f,  1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,
             1.0f,  1.0f, 0.5f,
        };

        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(positionLocation);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);

        EXPECT_PIXEL_EQ(mOSWindow->getWidth() / 2, mOSWindow->getHeight() / 2, 255, 0, 0, 255);
    }

    void TearDown() override
    {
        mOSWindow->destroy();
        SafeDelete(mOSWindow);
    }

    OSWindow *mOSWindow;
};

// Tries to create a program binary using one set of platform params, then load it using a different sent of params
TEST_P(ProgramBinariesAcrossPlatforms, CreateAndReloadBinary)
{
    angle::PlatformParameters firstRenderer  = GetParam();
    angle::PlatformParameters secondRenderer = GetParam().loadParams;
    bool expectedLinkResult                  = GetParam().expectedLinkResult;

    if (!(IsPlatformAvailable(firstRenderer)))
    {
        std::cout << "First renderer not supported, skipping test";
        return;
    }

    if (!(IsPlatformAvailable(secondRenderer)))
    {
        std::cout << "Second renderer not supported, skipping test";
        return;
    }

    EGLWindow *eglWindow = nullptr;
    std::vector<uint8_t> binary(0);
    GLuint program = 0;

    GLint programLength = 0;
    GLint writtenLength = 0;
    GLenum binaryFormat = 0;

    // Create a EGL window with the first renderer
    eglWindow = createAndInitEGLWindow(firstRenderer);
    if (eglWindow == nullptr)
    {
        FAIL() << "Failed to create EGL window";
        return;
    }

    // If the test is trying to use both the default GPU and WARP, but the default GPU *IS* WARP,
    // then our expectations for the test results will be invalid.
    if (firstRenderer.eglParameters.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE &&
        secondRenderer.eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE)
    {
        std::string rendererString = std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        std::transform(rendererString.begin(), rendererString.end(), rendererString.begin(), ::tolower);

        auto basicRenderPos = rendererString.find(std::string("microsoft basic render"));
        auto softwareAdapterPos = rendererString.find(std::string("software adapter"));

        if (basicRenderPos != std::string::npos || softwareAdapterPos != std::string::npos)
        {
            // The first renderer is using WARP, even though we didn't explictly request it
            // We should skip this test
            std::cout << "Test skipped on when default GPU is WARP." << std::endl;
            return;
        }
    }

    // Create a program
    if (firstRenderer.majorVersion == 3)
    {
        program = createES3ProgramFromSource();
    }
    else
    {
        program = createES2ProgramFromSource();
    }

    if (program == 0)
    {
        destroyEGLWindow(&eglWindow);
        FAIL() << "Failed to create program from source";
    }

    // Draw using the program to ensure it works as expected
    drawWithProgram(program);
    EXPECT_GL_NO_ERROR();

    // Save the program binary out from this renderer
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH_OES, &programLength);
    EXPECT_GL_NO_ERROR();
    binary.resize(programLength);
    glGetProgramBinaryOES(program, programLength, &writtenLength, &binaryFormat, binary.data());
    EXPECT_GL_NO_ERROR();

    // Destroy the first renderer
    glDeleteProgram(program);
    destroyEGLWindow(&eglWindow);

    // Create an EGL window with the second renderer
    eglWindow = createAndInitEGLWindow(secondRenderer);
    if (eglWindow == nullptr)
    {
        FAIL() << "Failed to create EGL window";
        return;
    }

    program = glCreateProgram();
    glProgramBinaryOES(program, binaryFormat, binary.data(), writtenLength);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_EQ(expectedLinkResult, (linkStatus != 0));

    if (linkStatus != 0)
    {
        // If the link was successful, then we should try to draw using the program to ensure it works as expected
        drawWithProgram(program);
        EXPECT_GL_NO_ERROR();
    }

    // Destroy the second renderer
    glDeleteProgram(program);
    destroyEGLWindow(&eglWindow);
}

ANGLE_INSTANTIATE_TEST(ProgramBinariesAcrossPlatforms,
                       //                     | Save the program   | Load the program      | Expected
                       //                     | using these params | using these params    | link result
                       PlatformsWithLinkResult(ES2_D3D11(),         ES2_D3D11(),            true         ), // Loading + reloading binary should work
                       PlatformsWithLinkResult(ES3_D3D11(),         ES3_D3D11(),            true         ), // Loading + reloading binary should work
                       PlatformsWithLinkResult(ES2_D3D11_FL11_0(),  ES2_D3D11_FL9_3(),      false        ), // Switching feature level shouldn't work
                       PlatformsWithLinkResult(ES2_D3D11(),         ES2_D3D11_WARP(),       false        ), // Switching from hardware to software shouldn't work
                       PlatformsWithLinkResult(ES2_D3D11_FL9_3(),   ES2_D3D11_FL9_3_WARP(), false        ), // Switching from hardware to software shouldn't work for FL9 either
                       PlatformsWithLinkResult(ES2_D3D11(),         ES2_D3D9(),             false        ), // Switching from D3D11 to D3D9 shouldn't work
                       PlatformsWithLinkResult(ES2_D3D9(),          ES2_D3D11(),            false        ), // Switching from D3D9 to D3D11 shouldn't work
                       PlatformsWithLinkResult(ES2_D3D11(),         ES3_D3D11(),            true         )  // Switching to newer client version should work

                       // TODO: ANGLE issue 523
                       // Compiling a program with client version 3, saving the binary, then loading it with client version 2 should not work
                       // PlatformsWithLinkResult(ES3_D3D11(),         ES2_D3D11(),            false       )
                       );
