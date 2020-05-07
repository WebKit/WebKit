//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class GLSLTest : public ANGLETest
{
  protected:
    GLSLTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    std::string GenerateVaryingType(GLint vectorSize)
    {
        char varyingType[10];

        if (vectorSize == 1)
        {
            sprintf(varyingType, "float");
        }
        else
        {
            sprintf(varyingType, "vec%d", vectorSize);
        }

        return std::string(varyingType);
    }

    std::string GenerateVectorVaryingDeclaration(GLint vectorSize, GLint arraySize, GLint id)
    {
        char buff[100];

        if (arraySize == 1)
        {
            sprintf(buff, "varying %s v%d;\n", GenerateVaryingType(vectorSize).c_str(), id);
        }
        else
        {
            sprintf(buff, "varying %s v%d[%d];\n", GenerateVaryingType(vectorSize).c_str(), id,
                    arraySize);
        }

        return std::string(buff);
    }

    std::string GenerateVectorVaryingSettingCode(GLint vectorSize, GLint arraySize, GLint id)
    {
        std::string returnString;
        char buff[100];

        if (arraySize == 1)
        {
            sprintf(buff, "\t v%d = %s(1.0);\n", id, GenerateVaryingType(vectorSize).c_str());
            returnString += buff;
        }
        else
        {
            for (int i = 0; i < arraySize; i++)
            {
                sprintf(buff, "\t v%d[%d] = %s(1.0);\n", id, i,
                        GenerateVaryingType(vectorSize).c_str());
                returnString += buff;
            }
        }

        return returnString;
    }

    std::string GenerateVectorVaryingUseCode(GLint arraySize, GLint id)
    {
        if (arraySize == 1)
        {
            char buff[100];
            sprintf(buff, "v%d + ", id);
            return std::string(buff);
        }
        else
        {
            std::string returnString;
            for (int i = 0; i < arraySize; i++)
            {
                char buff[100];
                sprintf(buff, "v%d[%d] + ", id, i);
                returnString += buff;
            }
            return returnString;
        }
    }

    void GenerateGLSLWithVaryings(GLint floatCount,
                                  GLint floatArrayCount,
                                  GLint vec2Count,
                                  GLint vec2ArrayCount,
                                  GLint vec3Count,
                                  GLint vec3ArrayCount,
                                  GLint vec4Count,
                                  GLint vec4ArrayCount,
                                  bool useFragCoord,
                                  bool usePointCoord,
                                  bool usePointSize,
                                  std::string *fragmentShader,
                                  std::string *vertexShader)
    {
        // Generate a string declaring the varyings, to share between the fragment shader and the
        // vertex shader.
        std::string varyingDeclaration;

        unsigned int varyingCount = 0;

        for (GLint i = 0; i < floatCount; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(1, 1, varyingCount);
            varyingCount += 1;
        }

        for (GLint i = 0; i < floatArrayCount; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(1, 2, varyingCount);
            varyingCount += 1;
        }

        for (GLint i = 0; i < vec2Count; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(2, 1, varyingCount);
            varyingCount += 1;
        }

        for (GLint i = 0; i < vec2ArrayCount; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(2, 2, varyingCount);
            varyingCount += 1;
        }

        for (GLint i = 0; i < vec3Count; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(3, 1, varyingCount);
            varyingCount += 1;
        }

        for (GLint i = 0; i < vec3ArrayCount; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(3, 2, varyingCount);
            varyingCount += 1;
        }

        for (GLint i = 0; i < vec4Count; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(4, 1, varyingCount);
            varyingCount += 1;
        }

        for (GLint i = 0; i < vec4ArrayCount; i++)
        {
            varyingDeclaration += GenerateVectorVaryingDeclaration(4, 2, varyingCount);
            varyingCount += 1;
        }

        // Generate the vertex shader
        vertexShader->clear();
        vertexShader->append(varyingDeclaration);
        vertexShader->append("\nvoid main()\n{\n");

        unsigned int currentVSVarying = 0;

        for (GLint i = 0; i < floatCount; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(1, 1, currentVSVarying));
            currentVSVarying += 1;
        }

        for (GLint i = 0; i < floatArrayCount; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(1, 2, currentVSVarying));
            currentVSVarying += 1;
        }

        for (GLint i = 0; i < vec2Count; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(2, 1, currentVSVarying));
            currentVSVarying += 1;
        }

        for (GLint i = 0; i < vec2ArrayCount; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(2, 2, currentVSVarying));
            currentVSVarying += 1;
        }

        for (GLint i = 0; i < vec3Count; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(3, 1, currentVSVarying));
            currentVSVarying += 1;
        }

        for (GLint i = 0; i < vec3ArrayCount; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(3, 2, currentVSVarying));
            currentVSVarying += 1;
        }

        for (GLint i = 0; i < vec4Count; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(4, 1, currentVSVarying));
            currentVSVarying += 1;
        }

        for (GLint i = 0; i < vec4ArrayCount; i++)
        {
            vertexShader->append(GenerateVectorVaryingSettingCode(4, 2, currentVSVarying));
            currentVSVarying += 1;
        }

        if (usePointSize)
        {
            vertexShader->append("gl_PointSize = 1.0;\n");
        }

        vertexShader->append("}\n");

        // Generate the fragment shader
        fragmentShader->clear();
        fragmentShader->append("precision highp float;\n");
        fragmentShader->append(varyingDeclaration);
        fragmentShader->append("\nvoid main() \n{ \n\tvec4 retColor = vec4(0,0,0,0);\n");

        unsigned int currentFSVarying = 0;

        // Make use of the float varyings
        fragmentShader->append("\tretColor += vec4(");

        for (GLint i = 0; i < floatCount; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(1, currentFSVarying));
            currentFSVarying += 1;
        }

        for (GLint i = 0; i < floatArrayCount; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(2, currentFSVarying));
            currentFSVarying += 1;
        }

        fragmentShader->append("0.0, 0.0, 0.0, 0.0);\n");

        // Make use of the vec2 varyings
        fragmentShader->append("\tretColor += vec4(");

        for (GLint i = 0; i < vec2Count; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(1, currentFSVarying));
            currentFSVarying += 1;
        }

        for (GLint i = 0; i < vec2ArrayCount; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(2, currentFSVarying));
            currentFSVarying += 1;
        }

        fragmentShader->append("vec2(0.0, 0.0), 0.0, 0.0);\n");

        // Make use of the vec3 varyings
        fragmentShader->append("\tretColor += vec4(");

        for (GLint i = 0; i < vec3Count; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(1, currentFSVarying));
            currentFSVarying += 1;
        }

        for (GLint i = 0; i < vec3ArrayCount; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(2, currentFSVarying));
            currentFSVarying += 1;
        }

        fragmentShader->append("vec3(0.0, 0.0, 0.0), 0.0);\n");

        // Make use of the vec4 varyings
        fragmentShader->append("\tretColor += ");

        for (GLint i = 0; i < vec4Count; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(1, currentFSVarying));
            currentFSVarying += 1;
        }

        for (GLint i = 0; i < vec4ArrayCount; i++)
        {
            fragmentShader->append(GenerateVectorVaryingUseCode(2, currentFSVarying));
            currentFSVarying += 1;
        }

        fragmentShader->append("vec4(0.0, 0.0, 0.0, 0.0);\n");

        // Set gl_FragColor, and use special variables if requested
        fragmentShader->append("\tgl_FragColor = retColor");

        if (useFragCoord)
        {
            fragmentShader->append(" + gl_FragCoord");
        }

        if (usePointCoord)
        {
            fragmentShader->append(" + vec4(gl_PointCoord, 0.0, 0.0)");
        }

        fragmentShader->append(";\n}");
    }

    void VaryingTestBase(GLint floatCount,
                         GLint floatArrayCount,
                         GLint vec2Count,
                         GLint vec2ArrayCount,
                         GLint vec3Count,
                         GLint vec3ArrayCount,
                         GLint vec4Count,
                         GLint vec4ArrayCount,
                         bool useFragCoord,
                         bool usePointCoord,
                         bool usePointSize,
                         bool expectSuccess)
    {
        std::string fragmentShaderSource;
        std::string vertexShaderSource;

        GenerateGLSLWithVaryings(floatCount, floatArrayCount, vec2Count, vec2ArrayCount, vec3Count,
                                 vec3ArrayCount, vec4Count, vec4ArrayCount, useFragCoord,
                                 usePointCoord, usePointSize, &fragmentShaderSource,
                                 &vertexShaderSource);

        GLuint program = CompileProgram(vertexShaderSource.c_str(), fragmentShaderSource.c_str());

        if (expectSuccess)
        {
            EXPECT_NE(0u, program);
        }
        else
        {
            EXPECT_EQ(0u, program);
        }
    }

    void CompileGLSLWithUniformsAndSamplers(GLint vertexUniformCount,
                                            GLint fragmentUniformCount,
                                            GLint vertexSamplersCount,
                                            GLint fragmentSamplersCount,
                                            bool expectSuccess)
    {
        std::stringstream vertexShader;
        std::stringstream fragmentShader;

        // Generate the vertex shader
        vertexShader << "precision mediump float;\n";

        for (int i = 0; i < vertexUniformCount; i++)
        {
            vertexShader << "uniform vec4 v" << i << ";\n";
        }

        for (int i = 0; i < vertexSamplersCount; i++)
        {
            vertexShader << "uniform sampler2D s" << i << ";\n";
        }

        vertexShader << "void main()\n{\n";

        for (int i = 0; i < vertexUniformCount; i++)
        {
            vertexShader << "    gl_Position +=  v" << i << ";\n";
        }

        for (int i = 0; i < vertexSamplersCount; i++)
        {
            vertexShader << "    gl_Position +=  texture2D(s" << i << ", vec2(0.0, 0.0));\n";
        }

        if (vertexUniformCount == 0 && vertexSamplersCount == 0)
        {
            vertexShader << "   gl_Position = vec4(0.0);\n";
        }

        vertexShader << "}\n";

        // Generate the fragment shader
        fragmentShader << "precision mediump float;\n";

        for (int i = 0; i < fragmentUniformCount; i++)
        {
            fragmentShader << "uniform vec4 v" << i << ";\n";
        }

        for (int i = 0; i < fragmentSamplersCount; i++)
        {
            fragmentShader << "uniform sampler2D s" << i << ";\n";
        }

        fragmentShader << "void main()\n{\n";

        for (int i = 0; i < fragmentUniformCount; i++)
        {
            fragmentShader << "    gl_FragColor +=  v" << i << ";\n";
        }

        for (int i = 0; i < fragmentSamplersCount; i++)
        {
            fragmentShader << "    gl_FragColor +=  texture2D(s" << i << ", vec2(0.0, 0.0));\n";
        }

        if (fragmentUniformCount == 0 && fragmentSamplersCount == 0)
        {
            fragmentShader << "    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n";
        }

        fragmentShader << "}\n";

        GLuint program = CompileProgram(vertexShader.str().c_str(), fragmentShader.str().c_str());

        if (expectSuccess)
        {
            EXPECT_NE(0u, program);
        }
        else
        {
            EXPECT_EQ(0u, program);
        }
    }

    std::string QueryErrorMessage(GLuint program)
    {
        GLint infoLogLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
        EXPECT_GL_NO_ERROR();

        if (infoLogLength >= 1)
        {
            std::vector<GLchar> infoLog(infoLogLength);
            glGetProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()), nullptr,
                                infoLog.data());
            EXPECT_GL_NO_ERROR();
            return infoLog.data();
        }

        return "";
    }

    void validateComponentsInErrorMessage(const char *vertexShader,
                                          const char *fragmentShader,
                                          const char *expectedErrorType,
                                          const char *expectedVariableFullName)
    {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        const std::string &errorMessage = QueryErrorMessage(program);
        printf("%s\n", errorMessage.c_str());

        EXPECT_NE(std::string::npos, errorMessage.find(expectedErrorType));
        EXPECT_NE(std::string::npos, errorMessage.find(expectedVariableFullName));

        glDeleteProgram(program);
        ASSERT_GL_NO_ERROR();
    }

    void verifyAttachment2DColor(unsigned int index,
                                 GLuint textureName,
                                 GLenum target,
                                 GLint level,
                                 GLColor color)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, color)
            << "index " << index;
    }
};

class GLSLTestNoValidation : public GLSLTest
{
  public:
    GLSLTestNoValidation() { setNoErrorEnabled(true); }
};

class GLSLTest_ES3 : public GLSLTest
{};

class GLSLTest_ES31 : public GLSLTest
{};

std::string BuillBigInitialStackShader(int length)
{
    std::string result;
    result += "void main() { \n";
    for (int i = 0; i < length; i++)
    {
        result += "  if (true) { \n";
    }
    result += "  int temp; \n";
    for (int i = 0; i <= length; i++)
    {
        result += "} \n";
    }
    return result;
}

TEST_P(GLSLTest, NamelessScopedStructs)
{
    constexpr char kFS[] = R"(precision mediump float;
void main()
{
    struct
    {
        float q;
    } b;

    gl_FragColor = vec4(1, 0, 0, 1);
    gl_FragColor.a += b.q;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
}

// Test that array of fragment shader outputs is processed properly and draws
// E.g. was issue with "out vec4 frag_color[4];"
TEST_P(GLSLTest_ES3, FragmentShaderOutputArray)
{
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

    GLuint textures[4];
    glGenTextures(4, textures);

    for (size_t texIndex = 0; texIndex < ArraySize(textures); texIndex++)
    {
        glBindTexture(GL_TEXTURE_2D, textures[texIndex]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
    }

    GLint maxDrawBuffers;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, 4);

    GLuint readFramebuffer;
    glGenFramebuffers(1, &readFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);

    constexpr char kFS[] = R"(#version 300 es
precision highp float;

out vec4 frag_color[4];

void main()
{
    frag_color[0] = vec4(1.0, 0.0, 0.0, 1.0);
    frag_color[1] = vec4(0.0, 1.0, 0.0, 1.0);
    frag_color[2] = vec4(0.0, 0.0, 1.0, 1.0);
    frag_color[3] = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);

    GLenum allBufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                         GL_COLOR_ATTACHMENT3};

    constexpr GLuint kMaxBuffers = 4;

    // Enable all draw buffers.
    for (GLuint texIndex = 0; texIndex < kMaxBuffers; texIndex++)
    {
        glBindTexture(GL_TEXTURE_2D, textures[texIndex]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               textures[texIndex], 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               textures[texIndex], 0);
    }
    glDrawBuffers(kMaxBuffers, allBufs);

    // Draw with simple program.
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    verifyAttachment2DColor(0, textures[0], GL_TEXTURE_2D, 0, GLColor::red);
    verifyAttachment2DColor(1, textures[1], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(2, textures[2], GL_TEXTURE_2D, 0, GLColor::blue);
    verifyAttachment2DColor(3, textures[3], GL_TEXTURE_2D, 0, GLColor::white);
}

// Test that inactive fragment shader outputs don't cause a crash.
TEST_P(GLSLTest_ES3, InactiveFragmentShaderOutput)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;

// Make color0 inactive but specify color1 first.  The Vulkan backend assigns bogus locations when
// compiling and fixes it up in SPIR-V.  If color0's location is not fixed, it will return location
// 1 (aliasing color1).  This will lead to a Vulkan validation warning about attachment 0 not being
// written to, which shouldn't be fatal.
layout(location = 1) out vec4 color1;
layout(location = 0) out vec4 color0;

void main()
{
    color1 = vec4(0.0, 1.0, 0.0, 1.0);
}
)";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);

    constexpr GLint kDrawBufferCount = 2;

    GLint maxDrawBuffers;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, kDrawBufferCount);

    GLTexture textures[kDrawBufferCount];

    for (GLint texIndex = 0; texIndex < kDrawBufferCount; ++texIndex)
    {
        glBindTexture(GL_TEXTURE_2D, textures[texIndex]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
    }

    GLenum allBufs[kDrawBufferCount] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    GLFramebuffer fbo;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

    // Enable all draw buffers.
    for (GLint texIndex = 0; texIndex < kDrawBufferCount; ++texIndex)
    {
        glBindTexture(GL_TEXTURE_2D, textures[texIndex]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               textures[texIndex], 0);
    }
    glDrawBuffers(kDrawBufferCount, allBufs);

    // Draw with simple program.
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
}

TEST_P(GLSLTest, ScopedStructsOrderBug)
{
    // TODO(geofflang): Find out why this doesn't compile on Apple OpenGL drivers
    // (http://anglebug.com/1292)
    // TODO(geofflang): Find out why this doesn't compile on AMD OpenGL drivers
    // (http://anglebug.com/1291)
    ANGLE_SKIP_TEST_IF(IsDesktopOpenGL() && (IsOSX() || !IsNVIDIA()));

    constexpr char kFS[] = R"(precision mediump float;

struct T
{
    float f;
};

void main()
{
    T a;

    struct T
    {
        float q;
    };

    T b;

    gl_FragColor = vec4(1, 0, 0, 1);
    gl_FragColor.a += a.f;
    gl_FragColor.a += b.q;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
}

TEST_P(GLSLTest, ScopedStructsBug)
{
    constexpr char kFS[] = R"(precision mediump float;

struct T_0
{
    float f;
};

void main()
{
    gl_FragColor = vec4(1, 0, 0, 1);

    struct T
    {
        vec2 v;
    };

    T_0 a;
    T b;

    gl_FragColor.a += a.f;
    gl_FragColor.a += b.v.x;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
}

TEST_P(GLSLTest, DxPositionBug)
{
    constexpr char kVS[] = R"(attribute vec4 inputAttribute;
varying float dx_Position;
void main()
{
    gl_Position = vec4(inputAttribute);
    dx_Position = 0.0;
})";

    constexpr char kFS[] = R"(precision mediump float;

varying float dx_Position;

void main()
{
    gl_FragColor = vec4(dx_Position, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Draw an array of points with the first vertex offset at 0 using gl_VertexID
TEST_P(GLSLTest_ES3, GLVertexIDOffsetZeroDrawArray)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    constexpr int kStartIndex  = 0;
    constexpr int kArrayLength = 5;
    constexpr char kVS[]       = R"(#version 300 es
precision highp float;
void main() {
    gl_Position = vec4(float(gl_VertexID)/10.0, 0, 0, 1);
    gl_PointSize = 3.0;
})";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 outColor;
void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    glUseProgram(program);
    glDrawArrays(GL_POINTS, kStartIndex, kArrayLength);

    double pointCenterX = static_cast<double>(getWindowWidth()) / 2.0;
    double pointCenterY = static_cast<double>(getWindowHeight()) / 2.0;
    for (int i = kStartIndex; i < kStartIndex + kArrayLength; i++)
    {
        double pointOffsetX = static_cast<double>(i * getWindowWidth()) / 20.0;
        EXPECT_PIXEL_COLOR_EQ(static_cast<int>(pointCenterX + pointOffsetX),
                              static_cast<int>(pointCenterY), GLColor::red);
    }
}

// Helper function for the GLVertexIDIntegerTextureDrawArrays test
void GLVertexIDIntegerTextureDrawArrays_helper(int first, int count, GLenum err)
{
    glDrawArrays(GL_POINTS, first, count);

    int pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, pixel);
    // If we call this function with err as GL_NO_ERROR, then we expect no error and check the
    // pixels.
    if (err == static_cast<GLenum>(GL_NO_ERROR))
    {
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(pixel[0], first + count - 1);
    }
    else
    {
        // If we call this function with err set, we will allow the error, but check the pixels if
        // the error hasn't occurred.
        GLenum glError = glGetError();
        if (glError == err || glError == static_cast<GLenum>(GL_NO_ERROR))
        {
            EXPECT_EQ(pixel[0], first + count - 1);
        }
    }
}

// Ensure gl_VertexID gets passed to an integer texture properly when drawArrays is called. This
// is based off the WebGL test:
// https://github.com/KhronosGroup/WebGL/blob/master/sdk/tests/conformance2/rendering/vertex-id.html
TEST_P(GLSLTest_ES3, GLVertexIDIntegerTextureDrawArrays)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    // Have to set a large point size because the window size is much larger than the texture
    constexpr char kVS[] = R"(#version 300 es
flat out highp int vVertexID;
void main() {
    vVertexID = gl_VertexID;
    gl_Position = vec4(0,0,0,1);
    gl_PointSize = 1000.0;
})";

    constexpr char kFS[] = R"(#version 300 es
flat in highp int vVertexID;
out highp int oVertexID;
void main() {
    oVertexID = vVertexID;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32I, 1, 1);
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    EXPECT_GL_NO_ERROR();

    // Clear the texture to 42 to ensure the first test case doesn't accidentally pass
    GLint val[4] = {42};
    glClearBufferiv(GL_COLOR, 0, val);
    int pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, pixel);
    EXPECT_EQ(pixel[0], val[0]);

    GLVertexIDIntegerTextureDrawArrays_helper(0, 1, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(1, 1, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(10000, 1, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(100000, 1, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(1000000, 1, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(0, 2, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(1, 2, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(10000, 2, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(100000, 2, GL_NO_ERROR);
    GLVertexIDIntegerTextureDrawArrays_helper(1000000, 2, GL_NO_ERROR);

    int32_t int32Max = 0x7FFFFFFF;
    GLVertexIDIntegerTextureDrawArrays_helper(int32Max - 2, 1, GL_OUT_OF_MEMORY);
    GLVertexIDIntegerTextureDrawArrays_helper(int32Max - 1, 1, GL_OUT_OF_MEMORY);
    GLVertexIDIntegerTextureDrawArrays_helper(int32Max, 1, GL_OUT_OF_MEMORY);
}

// Draw an array of points with the first vertex offset at 5 using gl_VertexID
TEST_P(GLSLTest_ES3, GLVertexIDOffsetFiveDrawArray)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    // Bug in Nexus drivers, offset does not work. (anglebug.com/3264)
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsOpenGLES());

    constexpr int kStartIndex  = 5;
    constexpr int kArrayLength = 5;
    constexpr char kVS[]       = R"(#version 300 es
precision highp float;
void main() {
    gl_Position = vec4(float(gl_VertexID)/10.0, 0, 0, 1);
    gl_PointSize = 3.0;
})";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 outColor;
void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    glUseProgram(program);
    glDrawArrays(GL_POINTS, kStartIndex, kArrayLength);

    double pointCenterX = static_cast<double>(getWindowWidth()) / 2.0;
    double pointCenterY = static_cast<double>(getWindowHeight()) / 2.0;
    for (int i = kStartIndex; i < kStartIndex + kArrayLength; i++)
    {
        double pointOffsetX = static_cast<double>(i * getWindowWidth()) / 20.0;
        EXPECT_PIXEL_COLOR_EQ(static_cast<int>(pointCenterX + pointOffsetX),
                              static_cast<int>(pointCenterY), GLColor::red);
    }
}

TEST_P(GLSLTest, ElseIfRewriting)
{
    constexpr char kVS[] =
        "attribute vec4 a_position;\n"
        "varying float v;\n"
        "void main() {\n"
        "  gl_Position = a_position;\n"
        "  v = 1.0;\n"
        "  if (a_position.x <= 0.5) {\n"
        "    v = 0.0;\n"
        "  } else if (a_position.x >= 0.5) {\n"
        "    v = 2.0;\n"
        "  }\n"
        "}\n";

    constexpr char kFS[] =
        "precision highp float;\n"
        "varying float v;\n"
        "void main() {\n"
        "  vec4 color = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "  if (v >= 1.0) color = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "  if (v >= 2.0) color = vec4(0.0, 0.0, 1.0, 1.0);\n"
        "  gl_FragColor = color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    drawQuad(program, "a_position", 0.5f);

    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() - 1, 0, 0, 255, 0, 255);
}

TEST_P(GLSLTest, TwoElseIfRewriting)
{
    constexpr char kVS[] =
        "attribute vec4 a_position;\n"
        "varying float v;\n"
        "void main() {\n"
        "  gl_Position = a_position;\n"
        "  if (a_position.x == 0.0) {\n"
        "    v = 1.0;\n"
        "  } else if (a_position.x > 0.5) {\n"
        "    v = 0.0;\n"
        "  } else if (a_position.x > 0.75) {\n"
        "    v = 0.5;\n"
        "  }\n"
        "}\n";

    constexpr char kFS[] =
        "precision highp float;\n"
        "varying float v;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(v, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

TEST_P(GLSLTest, FrontFacingAndVarying)
{
    EGLPlatformParameters platform = GetParam().eglParameters;

    constexpr char kVS[] = R"(attribute vec4 a_position;
varying float v_varying;
void main()
{
    v_varying = a_position.x;
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying float v_varying;
void main()
{
    vec4 c;

    if (gl_FrontFacing)
    {
        c = vec4(v_varying, 0, 0, 1.0);
    }
    else
    {
        c = vec4(0, v_varying, 0, 1.0);
    }
    gl_FragColor = c;
})";

    GLuint program = CompileProgram(kVS, kFS);

    // Compilation should fail on D3D11 feature level 9_3, since gl_FrontFacing isn't supported.
    if (platform.renderer == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        if (platform.majorVersion == 9 && platform.minorVersion == 3)
        {
            EXPECT_EQ(0u, program);
            return;
        }
    }

    // Otherwise, compilation should succeed
    EXPECT_NE(0u, program);
}

// Test that we can release the shader compiler and still compile things properly.
TEST_P(GLSLTest, ReleaseCompilerThenCompile)
{
    // Draw with the first program.
    ANGLE_GL_PROGRAM(program1, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(program1, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Clear and release shader compiler.
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    glReleaseShaderCompiler();
    ASSERT_GL_NO_ERROR();

    // Draw with a second program.
    ANGLE_GL_PROGRAM(program2, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(program2, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Verify that linking shaders declaring different shading language versions fails.
TEST_P(GLSLTest_ES3, VersionMismatch)
{
    GLuint program = CompileProgram(essl3_shaders::vs::Simple(), essl1_shaders::fs::Red());
    EXPECT_EQ(0u, program);

    program = CompileProgram(essl1_shaders::vs::Simple(), essl3_shaders::fs::Red());
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant only in vertex shader fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantVaryingOut)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "attribute vec4 a_position;\n"
        "invariant varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant only in vertex shader succeeds in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantVaryingOut)
{
    // TODO: ESSL 3.00 -> GLSL 1.20 translation should add "invariant" in fragment shader
    // for varyings which are invariant in vertex shader (http://anglebug.com/1293)
    ANGLE_SKIP_TEST_IF(IsDesktopOpenGL());

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "invariant out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_NE(0u, program);
}

// Verify that declaring varying as invariant only in fragment shader fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantVaryingIn)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "invariant varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant only in fragment shader fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantVaryingIn)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "invariant in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant in both shaders succeeds in ESSL 1.00.
TEST_P(GLSLTest, InvariantVaryingBoth)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "invariant varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "attribute vec4 a_position;\n"
        "invariant varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_NE(0u, program);
}

// Verify that declaring varying as invariant in both shaders fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantVaryingBoth)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "invariant in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "invariant out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that declaring gl_Position as invariant succeeds in ESSL 1.00.
TEST_P(GLSLTest, InvariantGLPosition)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "attribute vec4 a_position;\n"
        "invariant gl_Position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_NE(0u, program);
}

// Verify that declaring gl_Position as invariant succeeds in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantGLPosition)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "invariant gl_Position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_NE(0u, program);
}

// Verify that using invariant(all) in both shaders fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantAllBoth)
{
    constexpr char kFS[] =
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#pragma STDGL invariant(all)\n"
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnFloat)
{
    constexpr char kVS[] =
        "varying float v_varying;\n"
        "float f() { if (v_varying > 0.0) return 1.0; }\n"
        "void main() { gl_Position = vec4(f(), 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnVec2)
{
    constexpr char kVS[] =
        "varying float v_varying;\n"
        "vec2 f() { if (v_varying > 0.0) return vec2(1.0, 1.0); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnVec3)
{
    constexpr char kVS[] =
        "varying float v_varying;\n"
        "vec3 f() { if (v_varying > 0.0) return vec3(1.0, 1.0, 1.0); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnVec4)
{
    constexpr char kVS[] =
        "varying float v_varying;\n"
        "vec4 f() { if (v_varying > 0.0) return vec4(1.0, 1.0, 1.0, 1.0); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnIVec4)
{
    constexpr char kVS[] =
        "varying float v_varying;\n"
        "ivec4 f() { if (v_varying > 0.0) return ivec4(1, 1, 1, 1); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnMat4)
{
    constexpr char kVS[] =
        "varying float v_varying;\n"
        "mat4 f() { if (v_varying > 0.0) return mat4(1.0); }\n"
        "void main() { gl_Position = vec4(f()[0][0], 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnStruct)
{
    constexpr char kVS[] =
        "varying float v_varying;\n"
        "struct s { float a; int b; vec2 c; };\n"
        "s f() { if (v_varying > 0.0) return s(1.0, 1, vec2(1.0, 1.0)); }\n"
        "void main() { gl_Position = vec4(f().a, 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest_ES3, MissingReturnArray)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in float v_varying;\n"
        "vec2[2] f() { if (v_varying > 0.0) { return vec2[2](vec2(1.0, 1.0), vec2(1.0, 1.0)); } }\n"
        "void main() { gl_Position = vec4(f()[0].x, 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl3_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest_ES3, MissingReturnArrayOfStructs)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in float v_varying;\n"
        "struct s { float a; int b; vec2 c; };\n"
        "s[2] f() { if (v_varying > 0.0) { return s[2](s(1.0, 1, vec2(1.0, 1.0)), s(1.0, 1, "
        "vec2(1.0, 1.0))); } }\n"
        "void main() { gl_Position = vec4(f()[0].a, 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl3_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest_ES3, MissingReturnStructOfArrays)
{
    // TODO(crbug.com/998505): Test failing on Android FYI Release (NVIDIA Shield TV)
    ANGLE_SKIP_TEST_IF(IsNVIDIAShield());

    constexpr char kVS[] =
        "#version 300 es\n"
        "in float v_varying;\n"
        "struct s { float a[2]; int b[2]; vec2 c[2]; };\n"
        "s f() { if (v_varying > 0.0) { return s(float[2](1.0, 1.0), int[2](1, 1),"
        "vec2[2](vec2(1.0, 1.0), vec2(1.0, 1.0))); } }\n"
        "void main() { gl_Position = vec4(f().a[0], 0, 0, 1); }\n";

    GLuint program = CompileProgram(kVS, essl3_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Verify that using invariant(all) in both shaders fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantAllBoth)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that using invariant(all) only in fragment shader succeeds in ESSL 1.00.
TEST_P(GLSLTest, InvariantAllIn)
{
    constexpr char kFS[] =
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_NE(0u, program);
}

// Verify that using invariant(all) only in fragment shader fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantAllIn)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that using invariant(all) only in vertex shader fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantAllOut)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#pragma STDGL invariant(all)\n"
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Verify that using invariant(all) only in vertex shader succeeds in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantAllOut)
{
    // TODO: ESSL 3.00 -> GLSL 1.20 translation should add "invariant" in fragment shader
    // for varyings which are invariant in vertex shader,
    // because of invariant(all) being used in vertex shader (http://anglebug.com/1293)
    ANGLE_SKIP_TEST_IF(IsDesktopOpenGL());

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    constexpr char kVS[] =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_NE(0u, program);
}

TEST_P(GLSLTest, MaxVaryingVec4)
{
    // TODO(geofflang): Find out why this doesn't compile on Apple AMD OpenGL drivers
    // (http://anglebug.com/1291)
    ANGLE_SKIP_TEST_IF(IsOSX() && IsAMD() && IsOpenGL());

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings, 0, false, false, false, true);
}

// Verify we can pack registers with one builtin varying.
TEST_P(GLSLTest, MaxVaryingVec4_OneBuiltin)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord.
    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings - 1, 0, true, false, false, true);
}

// Verify we can pack registers with two builtin varyings.
TEST_P(GLSLTest, MaxVaryingVec4_TwoBuiltins)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord and gl_PointCoord.
    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings - 2, 0, true, true, false, true);
}

// Verify we can pack registers with three builtin varyings.
TEST_P(GLSLTest, MaxVaryingVec4_ThreeBuiltins)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord, gl_PointCoord and gl_PointSize.
    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings - 3, 0, true, true, true, true);
}

// This covers a problematic case in D3D9 - we are limited by the number of available semantics,
// rather than total register use.
TEST_P(GLSLTest, MaxVaryingsSpecialCases)
{
    ANGLE_SKIP_TEST_IF(!IsD3D9());

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(maxVaryings, 0, 0, 0, 0, 0, 0, 0, true, false, false, false);
    VaryingTestBase(maxVaryings - 1, 0, 0, 0, 0, 0, 0, 0, true, true, false, false);
    VaryingTestBase(maxVaryings - 2, 0, 0, 0, 0, 0, 0, 0, true, true, false, true);

    // Special case for gl_PointSize: we get it for free on D3D9.
    VaryingTestBase(maxVaryings - 2, 0, 0, 0, 0, 0, 0, 0, true, true, true, true);
}

// This covers a problematic case in D3D9 - we are limited by the number of available semantics,
// rather than total register use.
TEST_P(GLSLTest, MaxMinusTwoVaryingVec2PlusOneSpecialVariable)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord.
    VaryingTestBase(0, 0, maxVaryings, 0, 0, 0, 0, 0, true, false, false, !IsD3D9());
}

TEST_P(GLSLTest, MaxVaryingVec3)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 0, 0, maxVaryings, 0, 0, 0, false, false, false, true);
}

TEST_P(GLSLTest, MaxVaryingVec3Array)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 0, 0, 0, maxVaryings / 2, 0, 0, false, false, false, true);
}

// Only fails on D3D9 because of packing limitations.
TEST_P(GLSLTest, MaxVaryingVec3AndOneFloat)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(1, 0, 0, 0, maxVaryings, 0, 0, 0, false, false, false, !IsD3D9());
}

// Only fails on D3D9 because of packing limitations.
TEST_P(GLSLTest, MaxVaryingVec3ArrayAndOneFloatArray)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 1, 0, 0, 0, maxVaryings / 2, 0, 0, false, false, false, !IsD3D9());
}

// Only fails on D3D9 because of packing limitations.
TEST_P(GLSLTest, TwiceMaxVaryingVec2)
{
    // TODO(geofflang): Figure out why this fails on NVIDIA's GLES driver
    // (http://anglebug.com/3849)
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsOpenGLES());

    // TODO(geofflang): Find out why this doesn't compile on Apple AMD OpenGL drivers
    // (http://anglebug.com/1291)
    ANGLE_SKIP_TEST_IF(IsOSX() && IsAMD() && IsOpenGL());

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 2 * maxVaryings, 0, 0, 0, 0, 0, false, false, false, !IsD3D9());
}

// Disabled because of a failure in D3D9
TEST_P(GLSLTest, MaxVaryingVec2Arrays)
{
    ANGLE_SKIP_TEST_IF(IsD3DSM3());

    // TODO(geofflang): Figure out why this fails on NVIDIA's GLES driver
    ANGLE_SKIP_TEST_IF(IsOpenGLES());

    // TODO(geofflang): Find out why this doesn't compile on Apple AMD OpenGL drivers
    // (http://anglebug.com/1291)
    ANGLE_SKIP_TEST_IF(IsOSX() && IsAMD() && IsOpenGL());

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Special case: because arrays of mat2 are packed as small grids of two rows by two columns,
    // we should be aware that when we're packing into an odd number of varying registers the
    // last row will be empty and can not fit the final vec2 arrary.
    GLint maxVec2Arrays = (maxVaryings >> 1) << 1;

    VaryingTestBase(0, 0, 0, maxVec2Arrays, 0, 0, 0, 0, false, false, false, true);
}

// Verify shader source with a fixed length that is less than the null-terminated length will
// compile.
TEST_P(GLSLTest, FixedShaderLength)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const std::string appendGarbage = "abcdefghijklmnopqrstuvwxyz";
    const std::string source   = "void main() { gl_FragColor = vec4(0, 0, 0, 0); }" + appendGarbage;
    const char *sourceArray[1] = {source.c_str()};
    GLint lengths[1]           = {static_cast<GLint>(source.length() - appendGarbage.length())};
    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Verify that a negative shader source length is treated as a null-terminated length.
TEST_P(GLSLTest, NegativeShaderLength)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *sourceArray[1] = {essl1_shaders::fs::Red()};
    GLint lengths[1]           = {-10};
    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Check that having an invalid char after the "." doesn't cause an assert.
TEST_P(GLSLTest, InvalidFieldFirstChar)
{
    GLuint shader      = glCreateShader(GL_VERTEX_SHADER);
    const char *source = "void main() {vec4 x; x.}";
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_EQ(0, compileResult);
}

// Verify that a length array with mixed positive and negative values compiles.
TEST_P(GLSLTest, MixedShaderLengths)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *sourceArray[] = {
        "void main()",
        "{",
        "    gl_FragColor = vec4(0, 0, 0, 0);",
        "}",
    };
    GLint lengths[] = {
        -10,
        1,
        static_cast<GLint>(strlen(sourceArray[2])),
        -1,
    };
    ASSERT_EQ(ArraySize(sourceArray), ArraySize(lengths));

    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Verify that zero-length shader source does not affect shader compilation.
TEST_P(GLSLTest, ZeroShaderLength)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *sourceArray[] = {
        "abcdefg", "34534", "void main() { gl_FragColor = vec4(0, 0, 0, 0); }", "", "abcdefghijklm",
    };
    GLint lengths[] = {
        0, 0, -1, 0, 0,
    };
    ASSERT_EQ(ArraySize(sourceArray), ArraySize(lengths));

    glShaderSource(shader, static_cast<GLsizei>(ArraySize(sourceArray)), sourceArray, lengths);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
    EXPECT_NE(compileResult, 0);
}

// Tests that bad index expressions don't crash ANGLE's translator.
// https://code.google.com/p/angleproject/issues/detail?id=857
TEST_P(GLSLTest, BadIndexBug)
{
    constexpr char kFSSourceVec[] =
        "precision mediump float;\n"
        "uniform vec4 uniformVec;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(uniformVec[int()]);\n"
        "}";

    GLuint shader = CompileShader(GL_FRAGMENT_SHADER, kFSSourceVec);
    EXPECT_EQ(0u, shader);

    if (shader != 0)
    {
        glDeleteShader(shader);
    }

    constexpr char kFSSourceMat[] =
        "precision mediump float;\n"
        "uniform mat4 uniformMat;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(uniformMat[int()]);\n"
        "}";

    shader = CompileShader(GL_FRAGMENT_SHADER, kFSSourceMat);
    EXPECT_EQ(0u, shader);

    if (shader != 0)
    {
        glDeleteShader(shader);
    }

    constexpr char kFSSourceArray[] =
        "precision mediump float;\n"
        "uniform vec4 uniformArray;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(uniformArray[int()]);\n"
        "}";

    shader = CompileShader(GL_FRAGMENT_SHADER, kFSSourceArray);
    EXPECT_EQ(0u, shader);

    if (shader != 0)
    {
        glDeleteShader(shader);
    }
}

// Test that structs defined in uniforms are translated correctly.
TEST_P(GLSLTest, StructSpecifiersUniforms)
{
    constexpr char kFS[] = R"(precision mediump float;

uniform struct S { float field; } s;

void main()
{
    gl_FragColor = vec4(1, 0, 0, 1);
    gl_FragColor.a += s.field;
})";

    GLuint program = CompileProgram(essl1_shaders::vs::Simple(), kFS);
    EXPECT_NE(0u, program);
}

// Test that structs declaration followed directly by an initialization is translated correctly.
TEST_P(GLSLTest, StructWithInitializer)
{
    constexpr char kFS[] = R"(precision mediump float;

struct S { float a; } s = S(1.0);

void main()
{
    gl_FragColor = vec4(0, 0, 0, 1);
    gl_FragColor.r += s.a;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    glUseProgram(program);

    // Test drawing, should be red.
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_GL_NO_ERROR();
}

// Test that structs without initializer, followed by a uniform usage works as expected.
TEST_P(GLSLTest, UniformStructWithoutInitializer)
{
    constexpr char kFS[] = R"(precision mediump float;

struct S { float a; };
uniform S u_s;

void main()
{
    gl_FragColor = vec4(u_s.a);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    glUseProgram(program);

    // Test drawing, should be red.
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);
    EXPECT_GL_NO_ERROR();
}

// Test that structs declaration followed directly by an initialization in a uniform.
TEST_P(GLSLTest, StructWithUniformInitializer)
{
    constexpr char kFS[] = R"(precision mediump float;

struct S { float a; } s = S(1.0);
uniform S us;

void main()
{
    gl_FragColor = vec4(0, 0, 0, 1);
    gl_FragColor.r += s.a;
    gl_FragColor.g += us.a;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    glUseProgram(program);

    // Test drawing, should be red.
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_GL_NO_ERROR();
}

// Test that gl_DepthRange is not stored as a uniform location. Since uniforms
// beginning with "gl_" are filtered out by our validation logic, we must
// bypass the validation to test the behaviour of the implementation.
// (note this test is still Impl-independent)
TEST_P(GLSLTestNoValidation, DepthRangeUniforms)
{
    constexpr char kFS[] = R"(precision mediump float;

void main()
{
    gl_FragColor = vec4(gl_DepthRange.near, gl_DepthRange.far, gl_DepthRange.diff, 1);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);

    // We need to bypass validation for this call.
    GLint nearIndex = glGetUniformLocation(program.get(), "gl_DepthRange.near");
    EXPECT_EQ(-1, nearIndex);

    // Test drawing does not throw an exception.
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_GL_NO_ERROR();
}

std::string GenerateSmallPowShader(double base, double exponent)
{
    std::stringstream stream;

    stream.precision(8);

    double result = pow(base, exponent);

    stream << "precision highp float;\n"
           << "float fun(float arg)\n"
           << "{\n"
           << "    return pow(arg, " << std::fixed << exponent << ");\n"
           << "}\n"
           << "\n"
           << "void main()\n"
           << "{\n"
           << "    const float a = " << std::scientific << base << ";\n"
           << "    float b = fun(a);\n"
           << "    if (abs(" << result << " - b) < " << std::abs(result * 0.001) << ")\n"
           << "    {\n"
           << "        gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
           << "    }\n"
           << "    else\n"
           << "    {\n"
           << "        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
           << "    }\n"
           << "}\n";

    return stream.str();
}

// Covers the WebGL test 'glsl/bugs/pow-of-small-constant-in-user-defined-function'
// See http://anglebug.com/851
TEST_P(GLSLTest, PowOfSmallConstant)
{
    // Test with problematic exponents that are close to an integer.
    std::vector<double> testExponents;
    std::array<double, 5> epsilonMultipliers = {-100.0, -1.0, 0.0, 1.0, 100.0};
    for (double epsilonMultiplier : epsilonMultipliers)
    {
        for (int i = -4; i <= 5; ++i)
        {
            if (i >= -1 && i <= 1)
                continue;
            const double epsilon = 1.0e-8;
            double bad           = static_cast<double>(i) + epsilonMultiplier * epsilon;
            testExponents.push_back(bad);
        }
    }

    // Also test with a few exponents that are not close to an integer.
    testExponents.push_back(3.6);
    testExponents.push_back(3.4);

    for (double testExponent : testExponents)
    {
        const std::string &fragmentShaderSource = GenerateSmallPowShader(1.0e-6, testExponent);

        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), fragmentShaderSource.c_str());

        drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        EXPECT_GL_NO_ERROR();
    }
}

// Test that fragment shaders which contain non-constant loop indexers and compiled for FL9_3 and
// below
// fail with a specific error message.
// Additionally test that the same fragment shader compiles successfully with feature levels greater
// than FL9_3.
TEST_P(GLSLTest, LoopIndexingValidation)
{
    constexpr char kFS[] = R"(precision mediump float;

uniform float loopMax;

void main()
{
    gl_FragColor = vec4(1, 0, 0, 1);
    for (float l = 0.0; l < loopMax; l++)
    {
        if (loopMax > 3.0)
        {
            gl_FragColor.a += 0.1;
        }
    }
})";

    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char *sourceArray[1] = {kFS};
    glShaderSource(shader, 1, sourceArray, nullptr);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);

    // If the test is configured to run limited to Feature Level 9_3, then it is
    // assumed that shader compilation will fail with an expected error message containing
    // "Loop index cannot be compared with non-constant expression"
    if ((GetParam() == ES2_D3D11_FL9_3() || GetParam() == ES2_D3D9()))
    {
        if (compileResult != 0)
        {
            FAIL() << "Shader compilation succeeded, expected failure";
        }
        else
        {
            GLint infoLogLength;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

            std::string infoLog;
            infoLog.resize(infoLogLength);
            glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), nullptr, &infoLog[0]);

            if (infoLog.find("Loop index cannot be compared with non-constant expression") ==
                std::string::npos)
            {
                FAIL() << "Shader compilation failed with unexpected error message";
            }
        }
    }
    else
    {
        EXPECT_NE(0, compileResult);
    }

    if (shader != 0)
    {
        glDeleteShader(shader);
    }
}

// Tests that the maximum uniforms count returned from querying GL_MAX_VERTEX_UNIFORM_VECTORS
// can actually be used.
TEST_P(GLSLTest, VerifyMaxVertexUniformVectors)
{
    // crbug.com/680631
    ANGLE_SKIP_TEST_IF(IsOzone() && IsIntel());

    int maxUniforms = 10000;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxUniforms);
    EXPECT_GL_NO_ERROR();
    std::cout << "Validating GL_MAX_VERTEX_UNIFORM_VECTORS = " << maxUniforms << std::endl;

    CompileGLSLWithUniformsAndSamplers(maxUniforms, 0, 0, 0, true);
}

// Tests that the maximum uniforms count returned from querying GL_MAX_VERTEX_UNIFORM_VECTORS
// can actually be used along with the maximum number of texture samplers.
TEST_P(GLSLTest, VerifyMaxVertexUniformVectorsWithSamplers)
{
    ANGLE_SKIP_TEST_IF(IsOpenGL() || IsOpenGLES());

    int maxUniforms = 10000;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxUniforms);
    EXPECT_GL_NO_ERROR();
    std::cout << "Validating GL_MAX_VERTEX_UNIFORM_VECTORS = " << maxUniforms << std::endl;

    int maxTextureImageUnits = 0;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);

    CompileGLSLWithUniformsAndSamplers(maxUniforms, 0, maxTextureImageUnits, 0, true);
}

// Tests that the maximum uniforms count + 1 from querying GL_MAX_VERTEX_UNIFORM_VECTORS
// fails shader compilation.
TEST_P(GLSLTest, VerifyMaxVertexUniformVectorsExceeded)
{
    int maxUniforms = 10000;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxUniforms);
    EXPECT_GL_NO_ERROR();
    std::cout << "Validating GL_MAX_VERTEX_UNIFORM_VECTORS + 1 = " << maxUniforms + 1 << std::endl;

    CompileGLSLWithUniformsAndSamplers(maxUniforms + 1, 0, 0, 0, false);
}

// Tests that the maximum uniforms count returned from querying GL_MAX_FRAGMENT_UNIFORM_VECTORS
// can actually be used.
TEST_P(GLSLTest, VerifyMaxFragmentUniformVectors)
{
    // crbug.com/680631
    ANGLE_SKIP_TEST_IF(IsOzone() && IsIntel());

    int maxUniforms = 10000;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxUniforms);
    EXPECT_GL_NO_ERROR();
    std::cout << "Validating GL_MAX_FRAGMENT_UNIFORM_VECTORS = " << maxUniforms << std::endl;

    CompileGLSLWithUniformsAndSamplers(0, maxUniforms, 0, 0, true);
}

// Tests that the maximum uniforms count returned from querying GL_MAX_FRAGMENT_UNIFORM_VECTORS
// can actually be used along with the maximum number of texture samplers.
TEST_P(GLSLTest, VerifyMaxFragmentUniformVectorsWithSamplers)
{
    ANGLE_SKIP_TEST_IF(IsOpenGL() || IsOpenGLES());

    int maxUniforms = 10000;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxUniforms);
    EXPECT_GL_NO_ERROR();

    int maxTextureImageUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);

    CompileGLSLWithUniformsAndSamplers(0, maxUniforms, 0, maxTextureImageUnits, true);
}

// Tests that the maximum uniforms count + 1 from querying GL_MAX_FRAGMENT_UNIFORM_VECTORS
// fails shader compilation.
TEST_P(GLSLTest, VerifyMaxFragmentUniformVectorsExceeded)
{
    int maxUniforms = 10000;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxUniforms);
    EXPECT_GL_NO_ERROR();
    std::cout << "Validating GL_MAX_FRAGMENT_UNIFORM_VECTORS + 1 = " << maxUniforms + 1
              << std::endl;

    CompileGLSLWithUniformsAndSamplers(0, maxUniforms + 1, 0, 0, false);
}

// Test compiling shaders using the GL_EXT_shader_texture_lod extension
TEST_P(GLSLTest, TextureLOD)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_texture_lod"));

    constexpr char kFS[] =
        "#extension GL_EXT_shader_texture_lod : require\n"
        "uniform sampler2D u_texture;\n"
        "void main() {\n"
        "    gl_FragColor = texture2DGradEXT(u_texture, vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, "
        "0.0));\n"
        "}\n";

    GLuint shader = CompileShader(GL_FRAGMENT_SHADER, kFS);
    ASSERT_NE(0u, shader);
    glDeleteShader(shader);
}

// HLSL generates extra lod0 variants of functions. There was a bug that incorrectly reworte
// function calls to use them in vertex shaders.  http://anglebug.com/3471
TEST_P(GLSLTest, TextureLODRewriteInVertexShader)
{
    constexpr char kVS[] = R"(
  precision highp float;
  uniform int uni;
  uniform sampler2D texture;

  vec4 A();

  vec4 B() {
    vec4 a;
    for(int r=0; r<14; r++){
      if (r < uni) return vec4(0.0);
      a = A();
    }
    return a;
  }

  vec4 A() {
    return texture2D(texture, vec2(0.0, 0.0));
  }

  void main() {
    gl_Position = B();
  })";

    constexpr char kFS[] = R"(
void main() { gl_FragColor = vec4(gl_FragCoord.x / 640.0, gl_FragCoord.y / 480.0, 0, 1); }
)";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Test to verify the a shader can have a sampler unused in a vertex shader
// but used in the fragment shader.
TEST_P(GLSLTest, VerifySamplerInBothVertexAndFragmentShaders)
{
    constexpr char kVS[] = R"(
attribute vec2 position;
varying mediump vec2 texCoord;
uniform sampler2D tex;
void main()
{
    gl_Position = vec4(position, 0, 1);
    texCoord = position * 0.5 + vec2(0.5);
})";

    constexpr char kFS[] = R"(
varying mediump vec2 texCoord;
uniform sampler2D tex;
void main()
{
    gl_FragColor = texture2D(tex, texCoord);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    // Initialize basic red texture.
    const std::vector<GLColor> redColors(4, GLColor::red);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, redColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    drawQuad(program, "position", 0.0f);

    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::red);
}

// Test that two constructors which have vec4 and mat2 parameters get disambiguated (issue in
// HLSL).
TEST_P(GLSLTest_ES3, AmbiguousConstructorCall2x2)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "in vec4 a_vec;\n"
        "in mat2 a_mat;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_vec) + vec4(a_mat);\n"
        "}";

    GLuint program = CompileProgram(kVS, essl3_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Test that two constructors which have mat2x3 and mat3x2 parameters get disambiguated.
// This was suspected to be an issue in HLSL, but HLSL seems to be able to natively choose between
// the function signatures in this case.
TEST_P(GLSLTest_ES3, AmbiguousConstructorCall2x3)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "in mat3x2 a_matA;\n"
        "in mat2x3 a_matB;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_matA) + vec4(a_matB);\n"
        "}";

    GLuint program = CompileProgram(kVS, essl3_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Test that two functions which have vec4 and mat2 parameters get disambiguated (issue in HLSL).
TEST_P(GLSLTest_ES3, AmbiguousFunctionCall2x2)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "in vec4 a_vec;\n"
        "in mat2 a_mat;\n"
        "vec4 foo(vec4 a)\n"
        "{\n"
        "    return a;\n"
        "}\n"
        "vec4 foo(mat2 a)\n"
        "{\n"
        "    return vec4(a[0][0]);\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    gl_Position = foo(a_vec) + foo(a_mat);\n"
        "}";

    GLuint program = CompileProgram(kVS, essl3_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Test that an user-defined function with a large number of float4 parameters doesn't fail due to
// the function name being too long.
TEST_P(GLSLTest_ES3, LargeNumberOfFloat4Parameters)
{
    std::stringstream vertexShaderStream;
    const unsigned int paramCount = 1024u;

    vertexShaderStream << "#version 300 es\n"
                          "precision highp float;\n"
                          "in vec4 a_vec;\n"
                          "vec4 lotsOfVec4Parameters(";
    for (unsigned int i = 0; i < paramCount; ++i)
    {
        vertexShaderStream << "vec4 a" << i << ", ";
    }
    vertexShaderStream << "vec4 aLast)\n"
                          "{\n"
                          "    vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);\n";
    for (unsigned int i = 0; i < paramCount; ++i)
    {
        vertexShaderStream << "    sum += a" << i << ";\n";
    }
    vertexShaderStream << "    sum += aLast;\n"
                          "    return sum;\n "
                          "}\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position = lotsOfVec4Parameters(";
    for (unsigned int i = 0; i < paramCount; ++i)
    {
        vertexShaderStream << "a_vec, ";
    }
    vertexShaderStream << "a_vec);\n"
                          "}";

    GLuint program = CompileProgram(vertexShaderStream.str().c_str(), essl3_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// This test was written specifically to stress DeferGlobalInitializers AST transformation.
// Test a shader where a global constant array is initialized with an expression containing array
// indexing. This initializer is tricky to constant fold, so if it's not constant folded it needs to
// be handled in a way that doesn't generate statements in the global scope in HLSL output.
// Also includes multiple array initializers in one declaration, where only the second one has
// array indexing. This makes sure that the qualifier for the declaration is set correctly if
// transformations are applied to the declaration also in the case of ESSL output.
TEST_P(GLSLTest_ES3, InitGlobalArrayWithArrayIndexing)
{
    // TODO(ynovikov): re-enable once root cause of http://anglebug.com/1428 is fixed
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "const highp float f[2] = float[2](0.1, 0.2);\n"
        "const highp float[2] g = float[2](0.3, 0.4), h = float[2](0.5, f[1]);\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(h[1]);\n"
        "}";

    GLuint program = CompileProgram(essl3_shaders::vs::Simple(), kFS);
    EXPECT_NE(0u, program);
}

// Test that index-constant sampler array indexing is supported.
TEST_P(GLSLTest, IndexConstantSamplerArrayIndexing)
{
    ANGLE_SKIP_TEST_IF(IsD3D11_FL93());

    constexpr char kFS[] =
        "precision mediump float;\n"
        "uniform sampler2D uni[2];\n"
        "\n"
        "float zero(int x)\n"
        "{\n"
        "    return float(x) - float(x);\n"
        "}\n"
        "\n"
        "void main()\n"
        "{\n"
        "    vec4 c = vec4(0,0,0,0);\n"
        "    for (int ii = 1; ii < 3; ++ii) {\n"
        "        if (c.x > 255.0) {\n"
        "            c.x = 255.0 + zero(ii);\n"
        "            break;\n"
        "        }\n"
        // Index the sampler array with a predictable loop index (index-constant) as opposed to
        // a true constant. This is valid in OpenGL ES but isn't in many Desktop OpenGL versions,
        // without an extension.
        "        c += texture2D(uni[ii - 1], vec2(0.5, 0.5));\n"
        "    }\n"
        "    gl_FragColor = c;\n"
        "}";

    GLuint program = CompileProgram(essl1_shaders::vs::Simple(), kFS);
    EXPECT_NE(0u, program);
}

// Test that the #pragma directive is supported and doesn't trigger a compilation failure on the
// native driver. The only pragma that gets passed to the OpenGL driver is "invariant" but we don't
// want to test its behavior, so don't use any varyings.
TEST_P(GLSLTest, PragmaDirective)
{
    constexpr char kVS[] =
        "#pragma STDGL invariant(all)\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    GLuint program = CompileProgram(kVS, essl1_shaders::fs::Red());
    EXPECT_NE(0u, program);
}

// Sequence operator evaluates operands from left to right (ESSL 3.00 section 5.9).
// The function call that returns the array needs to be evaluated after ++j for the expression to
// return the correct value (true).
TEST_P(GLSLTest_ES3, SequenceOperatorEvaluationOrderArray)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor; \n"
        "int[2] func(int param) {\n"
        "    return int[2](param, param);\n"
        "}\n"
        "void main() {\n"
        "    int a[2]; \n"
        "    for (int i = 0; i < 2; ++i) {\n"
        "        a[i] = 1;\n"
        "    }\n"
        "    int j = 0; \n"
        "    bool result = ((++j), (a == func(j)));\n"
        "    my_FragColor = vec4(0.0, (result ? 1.0 : 0.0), 0.0, 1.0);\n"
        "}\n";

    GLuint program = CompileProgram(essl3_shaders::vs::Simple(), kFS);
    ASSERT_NE(0u, program);

    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Sequence operator evaluates operands from left to right (ESSL 3.00 section 5.9).
// The short-circuiting expression needs to be evaluated after ++j for the expression to return the
// correct value (true).
TEST_P(GLSLTest_ES3, SequenceOperatorEvaluationOrderShortCircuit)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor; \n"
        "void main() {\n"
        "    int j = 0; \n"
        "    bool result = ((++j), (j == 1 ? true : (++j == 3)));\n"
        "    my_FragColor = vec4(0.0, ((result && j == 1) ? 1.0 : 0.0), 0.0, 1.0);\n"
        "}\n";

    GLuint program = CompileProgram(essl3_shaders::vs::Simple(), kFS);
    ASSERT_NE(0u, program);

    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Sequence operator evaluates operands from left to right (ESSL 3.00 section 5.9).
// Indexing the vector needs to be evaluated after func() for the right result.
TEST_P(GLSLTest_ES3, SequenceOperatorEvaluationOrderDynamicVectorIndexingInLValue)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform int u_zero;\n"
        "int sideEffectCount = 0;\n"
        "float func() {\n"
        "    ++sideEffectCount;\n"
        "    return -1.0;\n"
        "}\n"
        "void main() {\n"
        "    vec4 v = vec4(0.0, 2.0, 4.0, 6.0); \n"
        "    float f = (func(), (++v[u_zero + sideEffectCount]));\n"
        "    bool green = abs(f - 3.0) < 0.01 && abs(v[1] - 3.0) < 0.01 && sideEffectCount == 1;\n"
        "    my_FragColor = vec4(0.0, (green ? 1.0 : 0.0), 0.0, 1.0);\n"
        "}\n";

    GLuint program = CompileProgram(essl3_shaders::vs::Simple(), kFS);
    ASSERT_NE(0u, program);

    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that using gl_PointCoord with GL_TRIANGLES doesn't produce a link error.
// From WebGL test conformance/rendering/point-specific-shader-variables.html
// See http://anglebug.com/1380
TEST_P(GLSLTest, RenderTrisWithPointCoord)
{
    constexpr char kVS[] =
        "attribute vec2 aPosition;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPosition, 0, 1);\n"
        "    gl_PointSize = 1.0;\n"
        "}";
    constexpr char kFS[] =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(gl_PointCoord.xy, 0, 1);\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(prog, kVS, kFS);
    drawQuad(prog.get(), "aPosition", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Convers a bug with the integer pow statement workaround.
TEST_P(GLSLTest, NestedPowStatements)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "float func(float v)\n"
        "{\n"
        "   float f1 = pow(v, 2.0);\n"
        "   return pow(f1 + v, 2.0);\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    float v = func(2.0);\n"
        "    gl_FragColor = abs(v - 36.0) < 0.001 ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(prog, essl1_shaders::vs::Simple(), kFS);
    drawQuad(prog.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that -float calculation is correct.
TEST_P(GLSLTest_ES3, UnaryMinusOperatorFloat)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "out highp vec4 o_color;\n"
        "void main() {\n"
        "    highp float f = -1.0;\n"
        "    // atan(tan(0.5), -f) should be 0.5.\n"
        "    highp float v = atan(tan(0.5), -f);\n"
        "    o_color = abs(v - 0.5) < 0.001 ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, essl3_shaders::vs::Simple(), kFS);
    drawQuad(prog.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that atan(vec2, vec2) calculation is correct.
TEST_P(GLSLTest_ES3, AtanVec2)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "out highp vec4 o_color;\n"
        "void main() {\n"
        "    highp float f = 1.0;\n"
        "    // atan(tan(0.5), f) should be 0.5.\n"
        "    highp vec2 v = atan(vec2(tan(0.5)), vec2(f));\n"
        "    o_color = (abs(v[0] - 0.5) < 0.001 && abs(v[1] - 0.5) < 0.001) ? vec4(0, 1, 0, 1) : "
        "vec4(1, 0, 0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, essl3_shaders::vs::Simple(), kFS);
    drawQuad(prog.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Convers a bug with the unary minus operator on signed integer workaround.
TEST_P(GLSLTest_ES3, UnaryMinusOperatorSignedInt)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in highp vec4 position;\n"
        "out mediump vec4 v_color;\n"
        "uniform int ui_one;\n"
        "uniform int ui_two;\n"
        "uniform int ui_three;\n"
        "void main() {\n"
        "    int s[3];\n"
        "    s[0] = ui_one;\n"
        "    s[1] = -(-(-ui_two + 1) + 1);\n"  // s[1] = -ui_two
        "    s[2] = ui_three;\n"
        "    int result = 0;\n"
        "    for (int i = 0; i < ui_three; i++) {\n"
        "        result += s[i];\n"
        "    }\n"
        "    v_color = (result == 2) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
        "    gl_Position = position;\n"
        "}\n";
    constexpr char kFS[] =
        "#version 300 es\n"
        "in mediump vec4 v_color;\n"
        "layout(location=0) out mediump vec4 o_color;\n"
        "void main() {\n"
        "    o_color = v_color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, kVS, kFS);

    GLint oneIndex = glGetUniformLocation(prog.get(), "ui_one");
    ASSERT_NE(-1, oneIndex);
    GLint twoIndex = glGetUniformLocation(prog.get(), "ui_two");
    ASSERT_NE(-1, twoIndex);
    GLint threeIndex = glGetUniformLocation(prog.get(), "ui_three");
    ASSERT_NE(-1, threeIndex);
    glUseProgram(prog.get());
    glUniform1i(oneIndex, 1);
    glUniform1i(twoIndex, 2);
    glUniform1i(threeIndex, 3);

    drawQuad(prog.get(), "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Convers a bug with the unary minus operator on unsigned integer workaround.
TEST_P(GLSLTest_ES3, UnaryMinusOperatorUnsignedInt)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in highp vec4 position;\n"
        "out mediump vec4 v_color;\n"
        "uniform uint ui_one;\n"
        "uniform uint ui_two;\n"
        "uniform uint ui_three;\n"
        "void main() {\n"
        "    uint s[3];\n"
        "    s[0] = ui_one;\n"
        "    s[1] = -(-(-ui_two + 1u) + 1u);\n"  // s[1] = -ui_two
        "    s[2] = ui_three;\n"
        "    uint result = 0u;\n"
        "    for (uint i = 0u; i < ui_three; i++) {\n"
        "        result += s[i];\n"
        "    }\n"
        "    v_color = (result == 2u) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
        "    gl_Position = position;\n"
        "}\n";
    constexpr char kFS[] =
        "#version 300 es\n"
        "in mediump vec4 v_color;\n"
        "layout(location=0) out mediump vec4 o_color;\n"
        "void main() {\n"
        "    o_color = v_color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, kVS, kFS);

    GLint oneIndex = glGetUniformLocation(prog.get(), "ui_one");
    ASSERT_NE(-1, oneIndex);
    GLint twoIndex = glGetUniformLocation(prog.get(), "ui_two");
    ASSERT_NE(-1, twoIndex);
    GLint threeIndex = glGetUniformLocation(prog.get(), "ui_three");
    ASSERT_NE(-1, threeIndex);
    glUseProgram(prog.get());
    glUniform1ui(oneIndex, 1u);
    glUniform1ui(twoIndex, 2u);
    glUniform1ui(threeIndex, 3u);

    drawQuad(prog.get(), "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test a nested sequence operator with a ternary operator inside. The ternary operator is
// intended to be such that it gets converted to an if statement on the HLSL backend.
TEST_P(GLSLTest, NestedSequenceOperatorWithTernaryInside)
{
    // Note that the uniform keep_flop_positive doesn't need to be set - the test expects it to have
    // its default value false.
    constexpr char kFS[] =
        "precision mediump float;\n"
        "uniform bool keep_flop_positive;\n"
        "float flop;\n"
        "void main() {\n"
        "    flop = -1.0,\n"
        "    (flop *= -1.0,\n"
        "    keep_flop_positive ? 0.0 : flop *= -1.0),\n"
        "    gl_FragColor = vec4(0, -flop, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(prog, essl1_shaders::vs::Simple(), kFS);
    drawQuad(prog.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that using a sampler2D and samplerExternalOES in the same shader works (anglebug.com/1534)
TEST_P(GLSLTest, ExternalAnd2DSampler)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_EGL_image_external"));

    constexpr char kFS[] = R"(#extension GL_OES_EGL_image_external : enable
precision mediump float;
uniform samplerExternalOES tex0;
uniform sampler2D tex1;
void main(void)
{
    vec2 uv = vec2(0.0, 0.0);
    gl_FragColor = texture2D(tex0, uv) + texture2D(tex1, uv);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
}

// Test that literal infinity can be written out from the shader translator.
// A similar test can't be made for NaNs, since ESSL 3.00.6 requirements for NaNs are very loose.
TEST_P(GLSLTest_ES3, LiteralInfinityOutput)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 out_color;\n"
        "uniform float u;\n"
        "void main()\n"
        "{\n"
        "   float infVar = 1.0e40 - u;\n"
        "   bool correct = isinf(infVar) && infVar > 0.0;\n"
        "   out_color = correct ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that literal negative infinity can be written out from the shader translator.
// A similar test can't be made for NaNs, since ESSL 3.00.6 requirements for NaNs are very loose.
TEST_P(GLSLTest_ES3, LiteralNegativeInfinityOutput)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 out_color;\n"
        "uniform float u;\n"
        "void main()\n"
        "{\n"
        "   float infVar = -1.0e40 + u;\n"
        "   bool correct = isinf(infVar) && infVar < 0.0;\n"
        "   out_color = correct ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// The following MultipleDeclaration* tests are testing TranslatorHLSL specific simplification
// passes. Because the interaction of multiple passes must be tested, it is difficult to write
// a unittest for them. Instead we add the tests as end2end so will in particular test
// TranslatorHLSL when run on Windows.

// Test that passes splitting multiple declarations and comma operators are correctly ordered.
TEST_P(GLSLTest_ES3, MultipleDeclarationWithCommaOperator)
{
    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 color;

uniform float u;
float c = 0.0;
float sideEffect()
{
    c = u;
    return c;
}

void main(void)
{
    float a = 0.0, b = ((gl_FragCoord.x < 0.5 ? a : sideEffect()), a);
    color = vec4(b + c);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
}

// Test that passes splitting multiple declarations and comma operators and for loops are
// correctly ordered.
TEST_P(GLSLTest_ES3, MultipleDeclarationWithCommaOperatorInForLoop)
{
    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 color;

uniform float u;
float c = 0.0;
float sideEffect()
{
    c = u;
    return c;
}

void main(void)
{
    for(float a = 0.0, b = ((gl_FragCoord.x < 0.5 ? a : sideEffect()), a); a < 10.0; a++)
    {
        b += 1.0;
        color = vec4(b);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
}

// Test that splitting multiple declaration in for loops works with no loop condition
TEST_P(GLSLTest_ES3, MultipleDeclarationInForLoopEmptyCondition)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "void main(void)\n"
        "{\n"
        " for(float a = 0.0, b = 1.0;; a++)\n"
        " {\n"
        "  b += 1.0;\n"
        "  if (a > 10.0) {break;}\n"
        "  color = vec4(b);\n"
        " }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
}

// Test that splitting multiple declaration in for loops works with no loop expression
TEST_P(GLSLTest_ES3, MultipleDeclarationInForLoopEmptyExpression)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "void main(void)\n"
        "{\n"
        " for(float a = 0.0, b = 1.0; a < 10.0;)\n"
        " {\n"
        "  b += 1.0;\n"
        "  a += 1.0;\n"
        "  color = vec4(b);\n"
        " }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
}

// Test that dynamic indexing of a matrix inside a dynamic indexing of a vector in an l-value works
// correctly.
TEST_P(GLSLTest_ES3, NestedDynamicIndexingInLValue)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform int u_zero;\n"
        "void main() {\n"
        "    mat2 m = mat2(0.0, 0.0, 0.0, 0.0);\n"
        "    m[u_zero + 1][u_zero + 1] = float(u_zero + 1);\n"
        "    float f = m[1][1];\n"
        "    my_FragColor = vec4(1.0 - f, f, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

class WebGLGLSLTest : public GLSLTest
{
  protected:
    WebGLGLSLTest() { setWebGLCompatibilityEnabled(true); }
};

TEST_P(WebGLGLSLTest, MaxVaryingVec4PlusFragCoord)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord, a special fragment shader variables.
    // This test should fail, since we are really using (maxVaryings + 1) varyings.
    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings, 0, true, false, false, false);
}

TEST_P(WebGLGLSLTest, MaxVaryingVec4PlusPointCoord)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord, a special fragment shader variables.
    // This test should fail, since we are really using (maxVaryings + 1) varyings.
    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings, 0, false, true, false, false);
}

TEST_P(WebGLGLSLTest, MaxPlusOneVaryingVec3)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 0, 0, maxVaryings + 1, 0, 0, 0, false, false, false, false);
}

TEST_P(WebGLGLSLTest, MaxPlusOneVaryingVec3Array)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 0, 0, 0, maxVaryings / 2 + 1, 0, 0, false, false, false, false);
}

TEST_P(WebGLGLSLTest, MaxVaryingVec3AndOneVec2)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 1, 0, maxVaryings, 0, 0, 0, false, false, false, false);
}

TEST_P(WebGLGLSLTest, MaxPlusOneVaryingVec2)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 2 * maxVaryings + 1, 0, 0, 0, 0, 0, false, false, false, false);
}

TEST_P(WebGLGLSLTest, MaxVaryingVec3ArrayAndMaxPlusOneFloatArray)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, maxVaryings / 2 + 1, 0, 0, 0, 0, 0, maxVaryings / 2, false, false, false,
                    false);
}

// Test that FindLSB and FindMSB return correct values in their corner cases.
TEST_P(GLSLTest_ES31, FindMSBAndFindLSBCornerCases)
{
    // Suspecting AMD driver bug - failure seen on bots running on AMD R5 230.
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL() && IsLinux());

    // Failing on N5X Oreo http://anglebug.com/2304
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform int u_zero;\n"
        "void main() {\n"
        "    if (findLSB(u_zero) == -1 && findMSB(u_zero) == -1 && findMSB(u_zero - 1) == -1)\n"
        "    {\n"
        "        my_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        my_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that writing into a swizzled vector that is dynamically indexed succeeds.
TEST_P(GLSLTest_ES3, WriteIntoDynamicIndexingOfSwizzledVector)
{
    // http://anglebug.com/1924
    ANGLE_SKIP_TEST_IF(IsOpenGL());

    // The shader first assigns v.x to v.z (1.0)
    // Then v.y to v.y (2.0)
    // Then v.z to v.x (1.0)
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "    vec3 v = vec3(1.0, 2.0, 3.0);\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        v.zyx[i] = v[i];\n"
        "    }\n"
        "    my_FragColor = distance(v, vec3(1.0, 2.0, 1.0)) < 0.01 ? vec4(0, 1, 0, 1) : vec4(1, "
        "0, 0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that the length() method is correctly translated in Vulkan atomic counter buffer emulation.
TEST_P(GLSLTest_ES31, AtomicCounterArrayLength)
{
    // Crashes on an assertion.  The driver reports no atomic counter buffers when queried from the
    // program, but ANGLE believes there to be one.
    //
    // This is likely due to the fact that ANGLE generates the following code, as a side effect of
    // the code on which .length() is being called:
    //
    //     _uac1[(_uvalue = _utestSideEffectValue)];
    //
    // The driver is optimizing the subscription out, and calling the atomic counter inactive.  This
    // was observed on nvidia, mesa and amd/windows.
    //
    // The fix would be for ANGLE to skip uniforms it believes should exist, but when queried, the
    // driver says don't.
    //
    // http://anglebug.com/3782
    ANGLE_SKIP_TEST_IF(IsOpenGL());

    // Skipping due to a bug on the Qualcomm Vulkan Android driver.
    // http://anglebug.com/3726
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kCS[] = R"(#version 310 es
precision mediump float;
layout(local_size_x=1) in;

layout(binding = 0) uniform atomic_uint ac1[2][3];
uniform uint testSideEffectValue;

layout(binding = 1, std140) buffer Result
{
    uint value;
} result;

void main() {
    bool passed = true;
    if (ac1.length() != 2)
    {
        passed = false;
    }
    uint value = 0u;
    if (ac1[(value = testSideEffectValue)].length() != 3)
    {
        passed = false;
    }
    if (value != testSideEffectValue)
    {
        passed = false;
    }
    result.value = passed ? 255u : 127u;
})";

    constexpr unsigned int kUniformTestValue     = 17;
    constexpr unsigned int kExpectedSuccessValue = 255;
    constexpr unsigned int kAtomicCounterRows    = 2;
    constexpr unsigned int kAtomicCounterCols    = 3;

    GLint maxAtomicCounters = 0;
    glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTERS, &maxAtomicCounters);
    EXPECT_GL_NO_ERROR();

    // Required minimum is 8 by the spec
    EXPECT_GE(maxAtomicCounters, 8);
    ANGLE_SKIP_TEST_IF(static_cast<uint32_t>(maxAtomicCounters) <
                       kAtomicCounterRows * kAtomicCounterCols);

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);
    glUseProgram(program.get());

    constexpr unsigned int kBufferData[kAtomicCounterRows * kAtomicCounterCols] = {};
    GLBuffer atomicCounterBuffer;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(kBufferData), kBufferData, GL_STATIC_DRAW);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer);

    constexpr unsigned int kOutputInitValue = 0;
    GLBuffer shaderStorageBuffer;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, shaderStorageBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(kOutputInitValue), &kOutputInitValue,
                 GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, shaderStorageBuffer);

    GLint uniformLocation = glGetUniformLocation(program.get(), "testSideEffectValue");
    EXPECT_NE(uniformLocation, -1);
    glUniform1i(uniformLocation, kUniformTestValue);

    glDispatchCompute(1, 1, 1);

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    const GLuint *ptr = reinterpret_cast<const GLuint *>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT));
    EXPECT_EQ(*ptr, kExpectedSuccessValue);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

// Test that inactive images don't cause any errors.
TEST_P(GLSLTest_ES31, InactiveImages)
{
    ANGLE_SKIP_TEST_IF(IsD3D11());

    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(rgba32ui) uniform highp readonly uimage2D image1;
layout(rgba32ui) uniform highp readonly uimage2D image2[4];
void main()
{
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);

    glUseProgram(program.get());
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    // Verify that the images are indeed inactive.
    GLuint index = glGetProgramResourceIndex(program, GL_UNIFORM, "image1");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_INVALID_INDEX, index);

    index = glGetProgramResourceIndex(program, GL_UNIFORM, "image2");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

// Test that inactive atomic counters don't cause any errors.
TEST_P(GLSLTest_ES31, InactiveAtomicCounters)
{
    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(binding = 0, offset = 0) uniform atomic_uint ac1;
layout(binding = 0, offset = 4) uniform atomic_uint ac2[5];
void main()
{
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);

    glUseProgram(program.get());
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    // Verify that the atomic counters are indeed inactive.
    GLuint index = glGetProgramResourceIndex(program, GL_UNIFORM, "ac1");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_INVALID_INDEX, index);

    index = glGetProgramResourceIndex(program, GL_UNIFORM, "ac2");
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

// Test that inactive samplers in structs don't cause any errors.
TEST_P(GLSLTest_ES31, InactiveSamplersInStructCS)
{
    // While the sampler is being extracted and declared outside of the struct, it's not removed
    // from the struct definition.  http://anglebug.com/4211
    ANGLE_SKIP_TEST_IF(IsVulkan() || IsMetal());

    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
struct S
{
    vec4 v;
    sampler2D t[10];
};
uniform S s;
void main()
{
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);

    glUseProgram(program.get());
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();
}

// Test that array indices for arrays of arrays of basic types work as expected.
TEST_P(GLSLTest_ES31, ArraysOfArraysBasicType)
{
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform ivec2 test[2][2];\n"
        "void main() {\n"
        "    bool passed = true;\n"
        "    for (int i = 0; i < 2; i++) {\n"
        "        for (int j = 0; j < 2; j++) {\n"
        "            if (test[i][j] != ivec2(i + 1, j + 1)) {\n"
        "                passed = false;\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            std::stringstream uniformName;
            uniformName << "test[" << i << "][" << j << "]";
            GLint uniformLocation = glGetUniformLocation(program.get(), uniformName.str().c_str());
            // All array indices should be used.
            EXPECT_NE(uniformLocation, -1);
            glUniform2i(uniformLocation, i + 1, j + 1);
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that array indices for arrays of arrays of basic types work as expected
// inside blocks.
TEST_P(GLSLTest_ES31, ArraysOfArraysBlockBasicType)
{
    // anglebug.com/3821 - fails on AMD Windows
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsOpenGL());
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "layout(packed) uniform UBO { ivec2 test[2][2]; } ubo_data;\n"
        "void main() {\n"
        "    bool passed = true;\n"
        "    for (int i = 0; i < 2; i++) {\n"
        "        for (int j = 0; j < 2; j++) {\n"
        "            if (ubo_data.test[i][j] != ivec2(i + 1, j + 1)) {\n"
        "                passed = false;\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    // Use interface queries to determine buffer size and offset
    GLuint uboBlockIndex   = glGetProgramResourceIndex(program.get(), GL_UNIFORM_BLOCK, "UBO");
    GLenum uboDataSizeProp = GL_BUFFER_DATA_SIZE;
    GLint uboDataSize;
    glGetProgramResourceiv(program.get(), GL_UNIFORM_BLOCK, uboBlockIndex, 1, &uboDataSizeProp, 1,
                           nullptr, &uboDataSize);
    std::unique_ptr<char[]> uboData(new char[uboDataSize]);
    for (int i = 0; i < 2; i++)
    {
        std::stringstream resourceName;
        resourceName << "UBO.test[" << i << "][0]";
        GLenum resourceProps[] = {GL_ARRAY_STRIDE, GL_OFFSET};
        struct
        {
            GLint stride;
            GLint offset;
        } values;
        GLuint resourceIndex =
            glGetProgramResourceIndex(program.get(), GL_UNIFORM, resourceName.str().c_str());
        ASSERT_NE(resourceIndex, GL_INVALID_INDEX);
        glGetProgramResourceiv(program.get(), GL_UNIFORM, resourceIndex, 2, &resourceProps[0], 2,
                               nullptr, &values.stride);
        for (int j = 0; j < 2; j++)
        {
            GLint(&dataPtr)[2] =
                *reinterpret_cast<GLint(*)[2]>(&uboData[values.offset + j * values.stride]);
            dataPtr[0] = i + 1;
            dataPtr[1] = j + 1;
        }
    }
    GLBuffer ubo;
    glBindBuffer(GL_UNIFORM_BUFFER, ubo.get());
    glBufferData(GL_UNIFORM_BUFFER, uboDataSize, &uboData[0], GL_STATIC_DRAW);
    GLuint ubo_index = glGetUniformBlockIndex(program.get(), "UBO");
    ASSERT_NE(ubo_index, GL_INVALID_INDEX);
    glUniformBlockBinding(program.get(), ubo_index, 5);
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, ubo.get());
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that arrays of arrays of samplers work as expected.
TEST_P(GLSLTest_ES31, ArraysOfArraysSampler)
{
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform mediump isampler2D test[2][2];\n"
        "void main() {\n"
        "    bool passed = true;\n"
        "#define DO_CHECK(i,j) \\\n"
        "    if (texture(test[i][j], vec2(0.0, 0.0)) != ivec4(i + 1, j + 1, 0, 1)) { \\\n"
        "        passed = false; \\\n"
        "    }\n"
        "    DO_CHECK(0, 0)\n"
        "    DO_CHECK(0, 1)\n"
        "    DO_CHECK(1, 0)\n"
        "    DO_CHECK(1, 1)\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[2][2];
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            // First generate the texture
            int textureUnit = i * 2 + j;
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D, textures[i][j]);
            GLint texData[2] = {i + 1, j + 1};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32I, 1, 1, 0, GL_RG_INTEGER, GL_INT, &texData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            // Then send it as a uniform
            std::stringstream uniformName;
            uniformName << "test[" << i << "][" << j << "]";
            GLint uniformLocation = glGetUniformLocation(program.get(), uniformName.str().c_str());
            // All array indices should be used.
            EXPECT_NE(uniformLocation, -1);
            glUniform1i(uniformLocation, textureUnit);
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that structs containing arrays of samplers work as expected.
TEST_P(GLSLTest_ES31, StructArraySampler)
{
    // ASAN error on vulkan backend; ASAN tests only enabled on Mac Swangle
    // (http://crbug.com/1029378)
    ANGLE_SKIP_TEST_IF(IsOSX() && isSwiftshader());

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "struct Data { mediump sampler2D data[2]; };\n"
        "uniform Data test;\n"
        "void main() {\n"
        "    my_FragColor = vec4(texture(test.data[0], vec2(0.0, 0.0)).rg,\n"
        "                        texture(test.data[1], vec2(0.0, 0.0)).rg);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[2];
    GLColor expected = MakeGLColor(32, 64, 96, 255);
    for (int i = 0; i < 2; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        // Each element provides two components.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     expected.data() + 2 * i);
        std::stringstream uniformName;
        uniformName << "test.data[" << i << "]";
        // Then send it as a uniform
        GLint uniformLocation = glGetUniformLocation(program.get(), uniformName.str().c_str());
        // The uniform should be active.
        EXPECT_NE(uniformLocation, -1);
        glUniform1i(uniformLocation, i);
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, expected);
}

// Test that arrays of arrays of samplers inside structs work as expected.
TEST_P(GLSLTest_ES31, StructArrayArraySampler)
{
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "struct Data { mediump isampler2D data[2][2]; };\n"
        "uniform Data test;\n"
        "void main() {\n"
        "    bool passed = true;\n"
        "#define DO_CHECK(i,j) \\\n"
        "    if (texture(test.data[i][j], vec2(0.0, 0.0)) != ivec4(i + 1, j + 1, 0, 1)) { \\\n"
        "        passed = false; \\\n"
        "    }\n"
        "    DO_CHECK(0, 0)\n"
        "    DO_CHECK(0, 1)\n"
        "    DO_CHECK(1, 0)\n"
        "    DO_CHECK(1, 1)\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[2][2];
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            // First generate the texture
            int textureUnit = i * 2 + j;
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D, textures[i][j]);
            GLint texData[2] = {i + 1, j + 1};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32I, 1, 1, 0, GL_RG_INTEGER, GL_INT, &texData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            // Then send it as a uniform
            std::stringstream uniformName;
            uniformName << "test.data[" << i << "][" << j << "]";
            GLint uniformLocation = glGetUniformLocation(program.get(), uniformName.str().c_str());
            // All array indices should be used.
            EXPECT_NE(uniformLocation, -1);
            glUniform1i(uniformLocation, textureUnit);
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that an array of structs with arrays of arrays of samplers works.
TEST_P(GLSLTest_ES31, ArrayStructArrayArraySampler)
{
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    GLint numTextures;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numTextures);
    ANGLE_SKIP_TEST_IF(numTextures < 2 * (2 * 2 + 2 * 2));
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "struct Data { mediump isampler2D data0[2][2]; mediump isampler2D data1[2][2]; };\n"
        "uniform Data test[2];\n"
        "void main() {\n"
        "    bool passed = true;\n"
        "#define DO_CHECK_ikl(i,k,l) \\\n"
        "    if (texture(test[i].data0[k][l], vec2(0.0, 0.0)) != ivec4(i, 0, k, l)+1) { \\\n"
        "        passed = false; \\\n"
        "    } \\\n"
        "    if (texture(test[i].data1[k][l], vec2(0.0, 0.0)) != ivec4(i, 1, k, l)+1) { \\\n"
        "        passed = false; \\\n"
        "    }\n"
        "#define DO_CHECK_ik(i,k) \\\n"
        "    DO_CHECK_ikl(i, k, 0) \\\n"
        "    DO_CHECK_ikl(i, k, 1)\n"
        "#define DO_CHECK_i(i) \\\n"
        "    DO_CHECK_ik(i, 0) \\\n"
        "    DO_CHECK_ik(i, 1)\n"
        "    DO_CHECK_i(0)\n"
        "    DO_CHECK_i(1)\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[2][2][2][2];
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                for (size_t l = 0; l < 2; l++)
                {
                    // First generate the texture
                    int textureUnit = l + 2 * (k + 2 * (j + 2 * i));
                    glActiveTexture(GL_TEXTURE0 + textureUnit);
                    glBindTexture(GL_TEXTURE_2D, textures[i][j][k][l]);
                    GLint texData[4] = {i + 1, j + 1, k + 1, l + 1};
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1, 1, 0, GL_RGBA_INTEGER, GL_INT,
                                 &texData[0]);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    // Then send it as a uniform
                    std::stringstream uniformName;
                    uniformName << "test[" << i << "].data" << j << "[" << k << "][" << l << "]";
                    GLint uniformLocation =
                        glGetUniformLocation(program.get(), uniformName.str().c_str());
                    // All array indices should be used.
                    EXPECT_NE(uniformLocation, -1);
                    glUniform1i(uniformLocation, textureUnit);
                }
            }
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a complex chain of structs and arrays of samplers works as expected.
TEST_P(GLSLTest_ES31, ComplexStructArraySampler)
{
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    GLint numTextures;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numTextures);
    ANGLE_SKIP_TEST_IF(numTextures < 2 * 3 * (2 + 3));
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "struct Data { mediump isampler2D data0[2]; mediump isampler2D data1[3]; };\n"
        "uniform Data test[2][3];\n"
        "const vec2 ZERO = vec2(0.0, 0.0);\n"
        "void main() {\n"
        "    bool passed = true;\n"
        "#define DO_CHECK_INNER0(i,j,l) \\\n"
        "    if (texture(test[i][j].data0[l], ZERO) != ivec4(i, j, 0, l) + 1) { \\\n"
        "        passed = false; \\\n"
        "    }\n"
        "#define DO_CHECK_INNER1(i,j,l) \\\n"
        "    if (texture(test[i][j].data1[l], ZERO) != ivec4(i, j, 1, l) + 1) { \\\n"
        "        passed = false; \\\n"
        "    }\n"
        "#define DO_CHECK(i,j) \\\n"
        "    DO_CHECK_INNER0(i, j, 0) \\\n"
        "    DO_CHECK_INNER0(i, j, 1) \\\n"
        "    DO_CHECK_INNER1(i, j, 0) \\\n"
        "    DO_CHECK_INNER1(i, j, 1) \\\n"
        "    DO_CHECK_INNER1(i, j, 2)\n"
        "    DO_CHECK(0, 0)\n"
        "    DO_CHECK(0, 1)\n"
        "    DO_CHECK(0, 2)\n"
        "    DO_CHECK(1, 0)\n"
        "    DO_CHECK(1, 1)\n"
        "    DO_CHECK(1, 2)\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    struct Data
    {
        GLTexture data1[2];
        GLTexture data2[3];
    };
    Data textures[2][3];
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            GLTexture *arrays[]     = {&textures[i][j].data1[0], &textures[i][j].data2[0]};
            size_t arrayLengths[]   = {2, 3};
            size_t arrayOffsets[]   = {0, 2};
            size_t totalArrayLength = 5;
            for (int k = 0; k < 2; k++)
            {
                GLTexture *array   = arrays[k];
                size_t arrayLength = arrayLengths[k];
                size_t arrayOffset = arrayOffsets[k];
                for (size_t l = 0; l < arrayLength; l++)
                {
                    // First generate the texture
                    int textureUnit = arrayOffset + l + totalArrayLength * (j + 3 * i);
                    glActiveTexture(GL_TEXTURE0 + textureUnit);
                    glBindTexture(GL_TEXTURE_2D, array[l]);
                    GLint texData[4] = {i + 1, j + 1, k + 1, l + 1};
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1, 1, 0, GL_RGBA_INTEGER, GL_INT,
                                 &texData[0]);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    // Then send it as a uniform
                    std::stringstream uniformName;
                    uniformName << "test[" << i << "][" << j << "].data" << k << "[" << l << "]";
                    GLint uniformLocation =
                        glGetUniformLocation(program.get(), uniformName.str().c_str());
                    // All array indices should be used.
                    EXPECT_NE(uniformLocation, -1);
                    glUniform1i(uniformLocation, textureUnit);
                }
            }
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

TEST_P(GLSLTest_ES31, ArraysOfArraysStructDifferentTypesSampler)
{
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    GLint numTextures;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numTextures);
    ANGLE_SKIP_TEST_IF(numTextures < 3 * (2 + 2));
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "struct Data { mediump isampler2D data0[2]; mediump sampler2D data1[2]; };\n"
        "uniform Data test[3];\n"
        "ivec4 f2i(vec4 x) { return ivec4(x * 4.0 + 0.5); }"
        "void main() {\n"
        "    bool passed = true;\n"
        "#define DO_CHECK_ik(i,k) \\\n"
        "    if (texture(test[i].data0[k], vec2(0.0, 0.0)) != ivec4(i, 0, k, 0)+1) { \\\n"
        "        passed = false; \\\n"
        "    } \\\n"
        "    if (f2i(texture(test[i].data1[k], vec2(0.0, 0.0))) != ivec4(i, 1, k, 0)+1) { \\\n"
        "        passed = false; \\\n"
        "    }\n"
        "#define DO_CHECK_i(i) \\\n"
        "    DO_CHECK_ik(i, 0) \\\n"
        "    DO_CHECK_ik(i, 1)\n"
        "    DO_CHECK_i(0)\n"
        "    DO_CHECK_i(1)\n"
        "    DO_CHECK_i(2)\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[3][2][2];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                // First generate the texture
                int textureUnit = k + 2 * (j + 2 * i);
                glActiveTexture(GL_TEXTURE0 + textureUnit);
                glBindTexture(GL_TEXTURE_2D, textures[i][j][k]);
                GLint texData[4]        = {i + 1, j + 1, k + 1, 1};
                GLubyte texDataFloat[4] = {(i + 1) * 64 - 1, (j + 1) * 64 - 1, (k + 1) * 64 - 1,
                                           64};
                if (j == 0)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1, 1, 0, GL_RGBA_INTEGER, GL_INT,
                                 &texData[0]);
                }
                else
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 &texDataFloat[0]);
                }
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                // Then send it as a uniform
                std::stringstream uniformName;
                uniformName << "test[" << i << "].data" << j << "[" << k << "]";
                GLint uniformLocation =
                    glGetUniformLocation(program.get(), uniformName.str().c_str());
                // All array indices should be used.
                EXPECT_NE(uniformLocation, -1);
                glUniform1i(uniformLocation, textureUnit);
            }
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that arrays of arrays of samplers as parameters works as expected.
TEST_P(GLSLTest_ES31, ParameterArraysOfArraysSampler)
{
    // anglebug.com/3832 - no sampler array params on Android
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform mediump isampler2D test[2][3];\n"
        "const vec2 ZERO = vec2(0.0, 0.0);\n"
        "\n"
        "bool check(isampler2D data[2][3]);\n"
        "bool check(isampler2D data[2][3]) {\n"
        "#define DO_CHECK(i,j) \\\n"
        "    if (texture(data[i][j], ZERO) != ivec4(i+1, j+1, 0, 1)) { \\\n"
        "        return false; \\\n"
        "    }\n"
        "    DO_CHECK(0, 0)\n"
        "    DO_CHECK(0, 1)\n"
        "    DO_CHECK(0, 2)\n"
        "    DO_CHECK(1, 0)\n"
        "    DO_CHECK(1, 1)\n"
        "    DO_CHECK(1, 2)\n"
        "    return true;\n"
        "}\n"
        "void main() {\n"
        "    bool passed = check(test);\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[2][3];
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            // First generate the texture
            int textureUnit = i * 3 + j;
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D, textures[i][j]);
            GLint texData[2] = {i + 1, j + 1};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32I, 1, 1, 0, GL_RG_INTEGER, GL_INT, &texData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            // Then send it as a uniform
            std::stringstream uniformName;
            uniformName << "test[" << i << "][" << j << "]";
            GLint uniformLocation = glGetUniformLocation(program.get(), uniformName.str().c_str());
            // All array indices should be used.
            EXPECT_NE(uniformLocation, -1);
            glUniform1i(uniformLocation, textureUnit);
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that structs with arrays of arrays of samplers as parameters works as expected.
TEST_P(GLSLTest_ES31, ParameterStructArrayArraySampler)
{
    // anglebug.com/3832 - no sampler array params on Android
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "struct Data { mediump isampler2D data[2][3]; };\n"
        "uniform Data test;\n"
        "const vec2 ZERO = vec2(0.0, 0.0);\n"
        "\n"
        "bool check(Data data) {\n"
        "#define DO_CHECK(i,j) \\\n"
        "    if (texture(data.data[i][j], ZERO) != ivec4(i+1, j+1, 0, 1)) { \\\n"
        "        return false; \\\n"
        "    }\n"
        "    DO_CHECK(0, 0)\n"
        "    DO_CHECK(0, 1)\n"
        "    DO_CHECK(0, 2)\n"
        "    DO_CHECK(1, 0)\n"
        "    DO_CHECK(1, 1)\n"
        "    DO_CHECK(1, 2)\n"
        "    return true;\n"
        "}\n"
        "void main() {\n"
        "    bool passed = check(test);\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[2][3];
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            // First generate the texture
            int textureUnit = i * 3 + j;
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D, textures[i][j]);
            GLint texData[2] = {i + 1, j + 1};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32I, 1, 1, 0, GL_RG_INTEGER, GL_INT, &texData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            // Then send it as a uniform
            std::stringstream uniformName;
            uniformName << "test.data[" << i << "][" << j << "]";
            GLint uniformLocation = glGetUniformLocation(program.get(), uniformName.str().c_str());
            // All array indices should be used.
            EXPECT_NE(uniformLocation, -1);
            glUniform1i(uniformLocation, textureUnit);
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that arrays of arrays of structs with arrays of arrays of samplers
// as parameters works as expected.
TEST_P(GLSLTest_ES31, ParameterArrayArrayStructArrayArraySampler)
{
    // anglebug.com/3832 - no sampler array params on Android
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());
    GLint numTextures;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numTextures);
    ANGLE_SKIP_TEST_IF(numTextures < 3 * 2 * 2 * 2);
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "struct Data { mediump isampler2D data[2][2]; };\n"
        "uniform Data test[3][2];\n"
        "const vec2 ZERO = vec2(0.0, 0.0);\n"
        "\n"
        "bool check(Data data[3][2]) {\n"
        "#define DO_CHECK_ijkl(i,j,k,l) \\\n"
        "    if (texture(data[i][j].data[k][l], ZERO) != ivec4(i, j, k, l) + 1) { \\\n"
        "        return false; \\\n"
        "    }\n"
        "#define DO_CHECK_ij(i,j) \\\n"
        "    DO_CHECK_ijkl(i, j, 0, 0) \\\n"
        "    DO_CHECK_ijkl(i, j, 0, 1) \\\n"
        "    DO_CHECK_ijkl(i, j, 1, 0) \\\n"
        "    DO_CHECK_ijkl(i, j, 1, 1)\n"
        "    DO_CHECK_ij(0, 0)\n"
        "    DO_CHECK_ij(1, 0)\n"
        "    DO_CHECK_ij(2, 0)\n"
        "    DO_CHECK_ij(0, 1)\n"
        "    DO_CHECK_ij(1, 1)\n"
        "    DO_CHECK_ij(2, 1)\n"
        "    return true;\n"
        "}\n"
        "void main() {\n"
        "    bool passed = check(test);\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures[3][2][2][2];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                for (int l = 0; l < 2; l++)
                {
                    // First generate the texture
                    int textureUnit = l + 2 * (k + 2 * (j + 2 * i));
                    glActiveTexture(GL_TEXTURE0 + textureUnit);
                    glBindTexture(GL_TEXTURE_2D, textures[i][j][k][l]);
                    GLint texData[4] = {i + 1, j + 1, k + 1, l + 1};
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1, 1, 0, GL_RGBA_INTEGER, GL_INT,
                                 &texData[0]);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    // Then send it as a uniform
                    std::stringstream uniformName;
                    uniformName << "test[" << i << "][" << j << "].data[" << k << "][" << l << "]";
                    GLint uniformLocation =
                        glGetUniformLocation(program.get(), uniformName.str().c_str());
                    // All array indices should be used.
                    EXPECT_NE(uniformLocation, -1);
                    glUniform1i(uniformLocation, textureUnit);
                }
            }
        }
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that 3D arrays with sub-arrays passed as parameters works as expected.
TEST_P(GLSLTest_ES31, ParameterArrayArrayArraySampler)
{
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    GLint numTextures;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numTextures);
    ANGLE_SKIP_TEST_IF(numTextures < 2 * 3 * 4 + 4);
    // anglebug.com/3832 - no sampler array params on Android
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());
    // Seems like this is failing on Windows Intel?
    ANGLE_SKIP_TEST_IF(IsWindows() && IsIntel() && IsOpenGL());
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform mediump isampler2D test[2][3][4];\n"
        "uniform mediump isampler2D test2[4];\n"
        "const vec2 ZERO = vec2(0.0, 0.0);\n"
        "\n"
        "bool check1D(isampler2D arr[4], int x, int y) {\n"
        "    if (texture(arr[0], ZERO) != ivec4(x, y, 0, 0)+1) return false;\n"
        "    if (texture(arr[1], ZERO) != ivec4(x, y, 1, 0)+1) return false;\n"
        "    if (texture(arr[2], ZERO) != ivec4(x, y, 2, 0)+1) return false;\n"
        "    if (texture(arr[3], ZERO) != ivec4(x, y, 3, 0)+1) return false;\n"
        "    return true;\n"
        "}\n"
        "bool check2D(isampler2D arr[3][4], int x) {\n"
        "    if (!check1D(arr[0], x, 0)) return false;\n"
        "    if (!check1D(arr[1], x, 1)) return false;\n"
        "    if (!check1D(arr[2], x, 2)) return false;\n"
        "    return true;\n"
        "}\n"
        "bool check3D(isampler2D arr[2][3][4]) {\n"
        "    if (!check2D(arr[0], 0)) return false;\n"
        "    if (!check2D(arr[1], 1)) return false;\n"
        "    return true;\n"
        "}\n"
        "void main() {\n"
        "    bool passed = check3D(test) && check1D(test2, 7, 8);\n"
        "    my_FragColor = passed ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glUseProgram(program.get());
    GLTexture textures1[2][3][4];
    GLTexture textures2[4];
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int k = 0; k < 4; k++)
            {
                // First generate the texture
                int textureUnit = k + 4 * (j + 3 * i);
                glActiveTexture(GL_TEXTURE0 + textureUnit);
                glBindTexture(GL_TEXTURE_2D, textures1[i][j][k]);
                GLint texData[3] = {i + 1, j + 1, k + 1};
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32I, 1, 1, 0, GL_RGB_INTEGER, GL_INT,
                             &texData[0]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                // Then send it as a uniform
                std::stringstream uniformName;
                uniformName << "test[" << i << "][" << j << "][" << k << "]";
                GLint uniformLocation =
                    glGetUniformLocation(program.get(), uniformName.str().c_str());
                // All array indices should be used.
                EXPECT_NE(uniformLocation, -1);
                glUniform1i(uniformLocation, textureUnit);
            }
        }
    }
    for (int k = 0; k < 4; k++)
    {
        // First generate the texture
        int textureUnit = 2 * 3 * 4 + k;
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, textures2[k]);
        GLint texData[3] = {7 + 1, 8 + 1, k + 1};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32I, 1, 1, 0, GL_RGB_INTEGER, GL_INT, &texData[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // Then send it as a uniform
        std::stringstream uniformName;
        uniformName << "test2[" << k << "]";
        GLint uniformLocation = glGetUniformLocation(program.get(), uniformName.str().c_str());
        // All array indices should be used.
        EXPECT_NE(uniformLocation, -1);
        glUniform1i(uniformLocation, textureUnit);
    }
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that names do not collide when translating arrays of arrays of samplers.
TEST_P(GLSLTest_ES31, ArraysOfArraysNameCollisionSampler)
{
    ANGLE_SKIP_TEST_IF(IsVulkan());  // anglebug.com/3604 - rewriter can create name collisions
    GLint numTextures;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numTextures);
    ANGLE_SKIP_TEST_IF(numTextures < 2 * 2 + 3 * 3 + 4 * 4);
    // anglebug.com/3832 - no sampler array params on Android
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());
    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump sampler2D;\n"
        "precision mediump float;\n"
        "uniform sampler2D test_field1_field2[2][2];\n"
        "struct S1 { sampler2D field2[3][3]; }; uniform S1 test_field1;\n"
        "struct S2 { sampler2D field1_field2[4][4]; }; uniform S2 test;\n"
        "vec4 func1(sampler2D param_field1_field2[2][2],\n"
        "           int param_field1_field2_offset,\n"
        "           S1 param_field1,\n"
        "           S2 param) {\n"
        "    return vec4(0.0, 1.0, 0.0, 0.0);\n"
        "}\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "    my_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "    my_FragColor += func1(test_field1_field2, 0, test_field1, test);\n"
        "    vec2 uv = vec2(0.0);\n"
        "    my_FragColor += texture(test_field1_field2[0][0], uv) +\n"
        "                    texture(test_field1.field2[0][0], uv) +\n"
        "                    texture(test.field1_field2[0][0], uv);\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glActiveTexture(GL_TEXTURE0);
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint zero = 0;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &zero);
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that regular arrays are unmodified.
TEST_P(GLSLTest_ES31, BasicTypeArrayAndArrayOfSampler)
{
    // anglebug.com/2703 - QC doesn't support arrays of samplers as parameters,
    // so sampler array of array handling is disabled
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump sampler2D;\n"
        "precision mediump float;\n"
        "uniform sampler2D sampler_array[2][2];\n"
        "uniform int array[3][2];\n"
        "vec4 func1(int param[2],\n"
        "           int param2[3]) {\n"
        "    return vec4(0.0, 1.0, 0.0, 0.0);\n"
        "}\n"
        "out vec4 my_FragColor;\n"
        "void main() {\n"
        "    my_FragColor = texture(sampler_array[0][0], vec2(0.0));\n"
        "    my_FragColor += func1(array[1], int[](1, 2, 3));\n"
        "}\n";
    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);
    glActiveTexture(GL_TEXTURE0);
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint zero = 0;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &zero);
    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// This test covers a bug (and associated workaround) with nested sampling operations in the HLSL
// compiler DLL.
TEST_P(GLSLTest_ES3, NestedSamplingOperation)
{
    // This seems to be bugged on some version of Android. Might not affect the newest versions.
    // TODO(jmadill): Lift suppression when Chromium bots are upgraded.
    // Test skipped on Android because of bug with Nexus 5X.
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    constexpr char kVS[] =
        "#version 300 es\n"
        "out vec2 texCoord;\n"
        "in vec2 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    texCoord = position * 0.5 + vec2(0.5);\n"
        "}\n";

    constexpr char kSimpleFS[] =
        "#version 300 es\n"
        "in mediump vec2 texCoord;\n"
        "out mediump vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    fragColor = vec4(texCoord, 0, 1);\n"
        "}\n";

    constexpr char kNestedFS[] =
        "#version 300 es\n"
        "uniform mediump sampler2D samplerA;\n"
        "uniform mediump sampler2D samplerB;\n"
        "in mediump vec2 texCoord;\n"
        "out mediump vec4 fragColor;\n"
        "void main ()\n"
        "{\n"
        "    fragColor = texture(samplerB, texture(samplerA, texCoord).xy);\n"
        "}\n";

    ANGLE_GL_PROGRAM(initProg, kVS, kSimpleFS);
    ANGLE_GL_PROGRAM(nestedProg, kVS, kNestedFS);

    // Initialize a first texture with default texCoord data.
    GLTexture texA;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texA);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texA, 0);

    drawQuad(initProg, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    // Initialize a second texture with a simple color pattern.
    GLTexture texB;
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texB);

    std::array<GLColor, 4> simpleColors = {
        {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow}};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 simpleColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Draw with the nested program, using the first texture to index the second.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(nestedProg);
    GLint samplerALoc = glGetUniformLocation(nestedProg, "samplerA");
    ASSERT_NE(-1, samplerALoc);
    glUniform1i(samplerALoc, 0);
    GLint samplerBLoc = glGetUniformLocation(nestedProg, "samplerB");
    ASSERT_NE(-1, samplerBLoc);
    glUniform1i(samplerBLoc, 1);

    drawQuad(nestedProg, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    // Compute four texel centers.
    Vector2 windowSize(getWindowWidth(), getWindowHeight());
    Vector2 quarterWindowSize = windowSize / 4;
    Vector2 ul                = quarterWindowSize;
    Vector2 ur(windowSize.x() - quarterWindowSize.x(), quarterWindowSize.y());
    Vector2 ll(quarterWindowSize.x(), windowSize.y() - quarterWindowSize.y());
    Vector2 lr = windowSize - quarterWindowSize;

    EXPECT_PIXEL_COLOR_EQ_VEC2(ul, simpleColors[0]);
    EXPECT_PIXEL_COLOR_EQ_VEC2(ur, simpleColors[1]);
    EXPECT_PIXEL_COLOR_EQ_VEC2(ll, simpleColors[2]);
    EXPECT_PIXEL_COLOR_EQ_VEC2(lr, simpleColors[3]);
}

// Tests that using a constant declaration as the only statement in a for loop without curly braces
// doesn't crash.
TEST_P(GLSLTest, ConstantStatementInForLoop)
{
    constexpr char kVS[] =
        "void main()\n"
        "{\n"
        "    for (int i = 0; i < 10; ++i)\n"
        "        const int b = 0;\n"
        "}\n";

    GLuint shader = CompileShader(GL_VERTEX_SHADER, kVS);
    EXPECT_NE(0u, shader);
    glDeleteShader(shader);
}

// Tests that using a constant declaration as a loop init expression doesn't crash. Note that this
// test doesn't work on D3D9 due to looping limitations, so it is only run on ES3.
TEST_P(GLSLTest_ES3, ConstantStatementAsLoopInit)
{
    constexpr char kVS[] =
        "void main()\n"
        "{\n"
        "    for (const int i = 0; i < 0;) {}\n"
        "}\n";

    GLuint shader = CompileShader(GL_VERTEX_SHADER, kVS);
    EXPECT_NE(0u, shader);
    glDeleteShader(shader);
}

// Test that uninitialized local variables are initialized to 0.
TEST_P(GLSLTest_ES3, InitUninitializedLocals)
{
    // Test skipped on Android GLES because local variable initialization is disabled.
    // http://anglebug.com/2046
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "int result = 0;\n"
        "void main()\n"
        "{\n"
        "    int u;\n"
        "    result += u;\n"
        "    int k = 0;\n"
        "    for (int i[2], j = i[0] + 1; k < 2; ++k)\n"
        "    {\n"
        "        result += j;\n"
        "    }\n"
        "    if (result == 2)\n"
        "    {\n"
        "        my_FragColor = vec4(0, 1, 0, 1);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        my_FragColor = vec4(1, 0, 0, 1);\n"
        "    }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that uninitialized structs containing arrays of structs are initialized to 0. This
// specifically tests with two different struct variables declared in the same block.
TEST_P(GLSLTest, InitUninitializedStructContainingArrays)
{
    // Test skipped on Android GLES because local variable initialization is disabled.
    // http://anglebug.com/2046
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    constexpr char kFS[] =
        "precision mediump float;\n"
        "struct T\n"
        "{\n"
        "    int a[2];\n"
        "};\n"
        "struct S\n"
        "{\n"
        "    T t[2];\n"
        "};\n"
        "void main()\n"
        "{\n"
        "    S s;\n"
        "    S s2;\n"
        "    if (s.t[1].a[1] == 0 && s2.t[1].a[1] == 0)\n"
        "    {\n"
        "        gl_FragColor = vec4(0, 1, 0, 1);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_FragColor = vec4(1, 0, 0, 1);\n"
        "    }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Verify that two shaders with the same uniform name and members but different structure names will
// not link.
TEST_P(GLSLTest, StructureNameMatchingTest)
{
    const char *vsSource =
        "// Structures must have the same name, sequence of type names, and\n"
        "// type definitions, and field names to be considered the same type.\n"
        "// GLSL 1.017 4.2.4\n"
        "precision mediump float;\n"
        "struct info {\n"
        "  vec4 pos;\n"
        "  vec4 color;\n"
        "};\n"
        "\n"
        "uniform info uni;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = uni.pos;\n"
        "}\n";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    ASSERT_NE(0u, vs);
    glDeleteShader(vs);

    const char *fsSource =
        "// Structures must have the same name, sequence of type names, and\n"
        "// type definitions, and field names to be considered the same type.\n"
        "// GLSL 1.017 4.2.4\n"
        "precision mediump float;\n"
        "struct info1 {\n"
        "  vec4 pos;\n"
        "  vec4 color;\n"
        "};\n"
        "\n"
        "uniform info1 uni;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = uni.color;\n"
        "}\n";

    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    ASSERT_NE(0u, fs);
    glDeleteShader(fs);

    GLuint program = CompileProgram(vsSource, fsSource);
    EXPECT_EQ(0u, program);
}

// Test that an uninitialized nameless struct inside a for loop init statement works.
TEST_P(GLSLTest_ES3, UninitializedNamelessStructInForInitStatement)
{
    // Test skipped on Android GLES because local variable initialization is disabled.
    // http://anglebug.com/2046
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(1, 0, 0, 1);\n"
        "    for (struct { float q; } b; b.q < 2.0; b.q++) {\n"
        "        my_FragColor = vec4(0, 1, 0, 1);\n"
        "    }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that uninitialized global variables are initialized to 0.
TEST_P(WebGLGLSLTest, InitUninitializedGlobals)
{
    // http://anglebug.com/2862
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    constexpr char kFS[] =
        "precision mediump float;\n"
        "int result;\n"
        "int i[2], j = i[0] + 1;\n"
        "void main()\n"
        "{\n"
        "    result += j;\n"
        "    if (result == 1)\n"
        "    {\n"
        "        gl_FragColor = vec4(0, 1, 0, 1);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        gl_FragColor = vec4(1, 0, 0, 1);\n"
        "    }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that an uninitialized nameless struct in the global scope works.
TEST_P(WebGLGLSLTest, UninitializedNamelessStructInGlobalScope)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "struct { float q; } b;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1, 0, 0, 1);\n"
        "    if (b.q == 0.0)\n"
        "    {\n"
        "        gl_FragColor = vec4(0, 1, 0, 1);\n"
        "    }\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests nameless struct uniforms.
TEST_P(GLSLTest, EmbeddedStructUniform)
{
    const char kFragmentShader[] = R"(precision mediump float;
uniform struct { float q; } b;
void main()
{
    gl_FragColor = vec4(1, 0, 0, 1);
    if (b.q == 0.5)
    {
        gl_FragColor = vec4(0, 1, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);
    glUseProgram(program);
    GLint uniLoc = glGetUniformLocation(program, "b.q");
    ASSERT_NE(-1, uniLoc);
    glUniform1f(uniLoc, 0.5f);

    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests that rewriting samplers in structs doesn't mess up indexing.
TEST_P(GLSLTest, SamplerInStructMemberIndexing)
{
    const char kVertexShader[] = R"(attribute vec2 position;
varying vec2 texCoord;
void main()
{
    gl_Position = vec4(position, 0, 1);
    texCoord = position * 0.5 + vec2(0.5);
})";

    const char kFragmentShader[] = R"(precision mediump float;
struct S { sampler2D samp; bool b; };
uniform S uni;
varying vec2 texCoord;
void main()
{
    if (uni.b)
    {
        gl_FragColor = texture2D(uni.samp, texCoord);
    }
    else
    {
        gl_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, kVertexShader, kFragmentShader);
    glUseProgram(program);

    GLint bLoc = glGetUniformLocation(program, "uni.b");
    ASSERT_NE(-1, bLoc);
    GLint sampLoc = glGetUniformLocation(program, "uni.samp");
    ASSERT_NE(-1, sampLoc);

    glUniform1i(bLoc, 1);

    std::array<GLColor, 4> kGreenPixels = {
        {GLColor::green, GLColor::green, GLColor::green, GLColor::green}};

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kGreenPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests two nameless struct uniforms.
TEST_P(GLSLTest, TwoEmbeddedStructUniforms)
{
    const char kFragmentShader[] = R"(precision mediump float;
uniform struct { float q; } b, c;
void main()
{
    gl_FragColor = vec4(1, 0, 0, 1);
    if (b.q == 0.5 && c.q == 1.0)
    {
        gl_FragColor = vec4(0, 1, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);
    glUseProgram(program);

    GLint uniLocB = glGetUniformLocation(program, "b.q");
    ASSERT_NE(-1, uniLocB);
    glUniform1f(uniLocB, 0.5f);

    GLint uniLocC = glGetUniformLocation(program, "c.q");
    ASSERT_NE(-1, uniLocC);
    glUniform1f(uniLocC, 1.0f);

    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a loop condition that has an initializer declares a variable.
TEST_P(GLSLTest_ES3, ConditionInitializerDeclaresVariable)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    float i = 0.0;\n"
        "    while (bool foo = (i < 1.5))\n"
        "    {\n"
        "        if (!foo)\n"
        "        {\n"
        "            ++i;\n"
        "        }\n"
        "        if (i > 3.5)\n"
        "        {\n"
        "            break;\n"
        "        }\n"
        "        ++i;\n"
        "    }\n"
        "    my_FragColor = vec4(i * 0.5 - 1.0, i * 0.5, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a variable hides a user-defined function with the same name after its initializer.
// GLSL ES 1.00.17 section 4.2.2: "A variable declaration is visible immediately following the
// initializer if present, otherwise immediately following the identifier"
TEST_P(GLSLTest, VariableHidesUserDefinedFunctionAfterInitializer)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "uniform vec4 u;\n"
        "vec4 foo()\n"
        "{\n"
        "    return u;\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    vec4 foo = foo();\n"
        "    gl_FragColor = foo + vec4(0, 1, 0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that structs with identical members are not ambiguous as function arguments.
TEST_P(GLSLTest, StructsWithSameMembersDisambiguatedByName)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "uniform float u_zero;\n"
        "struct S { float foo; };\n"
        "struct S2 { float foo; };\n"
        "float get(S s) { return s.foo + u_zero; }\n"
        "float get(S2 s2) { return 0.25 + s2.foo + u_zero; }\n"
        "void main()\n"
        "{\n"
        "    S s;\n"
        "    s.foo = 0.5;\n"
        "    S2 s2;\n"
        "    s2.foo = 0.25;\n"
        "    gl_FragColor = vec4(0.0, get(s) + get(s2), 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a varying struct that's not statically used in the fragment shader works.
// GLSL ES 3.00.6 section 4.3.10.
TEST_P(GLSLTest_ES3, VaryingStructNotStaticallyUsedInFragmentShader)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "out S varStruct;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0);\n"
        "    varStruct.field = vec4(0.0, 0.5, 0.0, 0.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "in S varStruct;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Test that a varying struct that's not declared in the fragment shader links successfully.
// GLSL ES 3.00.6 section 4.3.10.
TEST_P(GLSLTest_ES3, VaryingStructNotDeclaredInFragmentShader)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "out S varStruct;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0);\n"
        "    varStruct.field = vec4(0.0, 0.5, 0.0, 0.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Test that a varying struct that's not declared in the vertex shader, and is unused in the
// fragment shader links successfully.
TEST_P(GLSLTest_ES3, VaryingStructNotDeclaredInVertexShader)
{
    // GLSL ES allows the vertex shader to not declare a varying if the fragment shader is not
    // going to use it.  See section 9.1 in
    // https://www.khronos.org/registry/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.pdf or
    // section 4.3.5 in https://www.khronos.org/files/opengles_shading_language.pdf
    //
    // However, nvidia OpenGL ES drivers fail to link this program.
    //
    // http://anglebug.com/3413
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && IsNVIDIA());

    constexpr char kVS[] =
        "#version 300 es\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "in S varStruct;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Test that a varying struct that's not initialized in the vertex shader links successfully.
TEST_P(GLSLTest_ES3, VaryingStructNotInitializedInVertexShader)
{
    // GLSL ES allows the vertex shader to declare but not initialize a varying (with a
    // specification that the varying values are undefined in the fragment stage).  See section 9.1
    // in https://www.khronos.org/registry/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.pdf
    // or section 4.3.5 in https://www.khronos.org/files/opengles_shading_language.pdf
    //
    // However, windows and mac OpenGL drivers fail to link this program.  With a message like:
    //
    // > Input of fragment shader 'varStruct' not written by vertex shader
    //
    // http://anglebug.com/3413
    ANGLE_SKIP_TEST_IF(IsDesktopOpenGL() && (IsOSX() || (IsWindows() && !IsNVIDIA())));

    constexpr char kVS[] =
        "#version 300 es\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "out S varStruct;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "in S varStruct;\n"
        "void main()\n"
        "{\n"
        "    col = varStruct.field;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Test that a varying struct that gets used in the fragment shader works.
TEST_P(GLSLTest_ES3, VaryingStructUsedInFragmentShader)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 inputAttribute;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "out S varStruct;\n"
        "out S varStruct2;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = inputAttribute;\n"
        "    varStruct.field = vec4(0.0, 0.5, 0.0, 1.0);\n"
        "    varStruct2.field = vec4(0.0, 0.5, 0.0, 1.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "in S varStruct;\n"
        "in S varStruct2;\n"
        "void main()\n"
        "{\n"
        "    col = varStruct.field + varStruct2.field;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that multiple multi-field varying structs that get used in the fragment shader work.
TEST_P(GLSLTest_ES3, ComplexVaryingStructsUsedInFragmentShader)
{
    // TODO(syoussefi): fails on android with:
    //
    // > Internal Vulkan error: A return array was too small for the result
    //
    // http://anglebug.com/3220
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAndroid());

    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 inputAttribute;\n"
        "struct S {\n"
        "    vec4 field1;\n"
        "    vec4 field2;\n"
        "};\n"
        "out S varStruct;\n"
        "out S varStruct2;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = inputAttribute;\n"
        "    varStruct.field1 = vec4(0.0, 0.5, 0.0, 1.0);\n"
        "    varStruct.field2 = vec4(0.0, 0.5, 0.0, 1.0);\n"
        "    varStruct2.field1 = vec4(0.0, 0.5, 0.0, 1.0);\n"
        "    varStruct2.field2 = vec4(0.0, 0.5, 0.0, 1.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "struct S {\n"
        "    vec4 field1;\n"
        "    vec4 field2;\n"
        "};\n"
        "in S varStruct;\n"
        "in S varStruct2;\n"
        "void main()\n"
        "{\n"
        "    col = varStruct.field1 + varStruct2.field2;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that an inactive varying array that doesn't get used in the fragment shader works.
TEST_P(GLSLTest_ES3, InactiveVaryingArrayUnusedInFragmentShader)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 inputAttribute;\n"
        "out vec4 varArray[4];\n"
        "void main()\n"
        "{\n"
        "    gl_Position = inputAttribute;\n"
        "    varArray[0] = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "    varArray[1] = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "    varArray[2] = vec4(0.0, 0.0, 1.0, 1.0);\n"
        "    varArray[3] = vec4(1.0, 1.0, 0.0, 1.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Test that an inactive varying struct that doesn't get used in the fragment shader works.
TEST_P(GLSLTest_ES3, InactiveVaryingStructUnusedInFragmentShader)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 inputAttribute;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "out S varStruct;\n"
        "out S varStruct2;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = inputAttribute;\n"
        "    varStruct.field = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "    varStruct2.field = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "in S varStruct;\n"
        "in S varStruct2;\n"
        "void main()\n"
        "{\n"
        "    col = varStruct.field;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that multiple varying matrices that get used in the fragment shader work.
TEST_P(GLSLTest_ES3, VaryingMatrices)
{
    constexpr char kVS[] =
        "#version 300 es\n"
        "in vec4 inputAttribute;\n"
        "out mat2x2 varMat;\n"
        "out mat2x2 varMat2;\n"
        "out mat4x3 varMat3;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = inputAttribute;\n"
        "    varMat[0] = vec2(1, 1);\n"
        "    varMat[1] = vec2(1, 1);\n"
        "    varMat2[0] = vec2(0.5, 0.5);\n"
        "    varMat2[1] = vec2(0.5, 0.5);\n"
        "    varMat3[0] = vec3(0.75, 0.75, 0.75);\n"
        "    varMat3[1] = vec3(0.75, 0.75, 0.75);\n"
        "    varMat3[2] = vec3(0.75, 0.75, 0.75);\n"
        "    varMat3[3] = vec3(0.75, 0.75, 0.75);\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "in mat2x2 varMat;\n"
        "in mat2x2 varMat2;\n"
        "in mat4x3 varMat3;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(varMat[0].x, varMat2[1].y, varMat3[2].z, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(255, 127, 191, 255), 1);
}

// This test covers passing a struct containing a sampler as a function argument.
TEST_P(GLSLTest, StructsWithSamplersAsFunctionArg)
{
    // Shader failed to compile on Nexus devices. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsAdreno() && IsOpenGLES());

    const char kFragmentShader[] = R"(precision mediump float;
struct S { sampler2D samplerMember; };
uniform S uStruct;
uniform vec2 uTexCoord;
vec4 foo(S structVar)
{
    return texture2D(structVar.samplerMember, uTexCoord);
}
void main()
{
    gl_FragColor = foo(uStruct);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStruct.samplerMember");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// This test covers passing a struct containing a sampler as a function argument.
TEST_P(GLSLTest, StructsWithSamplersAsFunctionArgWithPrototype)
{
    // Shader failed to compile on Android. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    const char kFragmentShader[] = R"(precision mediump float;
struct S { sampler2D samplerMember; };
uniform S uStruct;
uniform vec2 uTexCoord;
vec4 foo(S structVar);
vec4 foo(S structVar)
{
    return texture2D(structVar.samplerMember, uTexCoord);
}
void main()
{
    gl_FragColor = foo(uStruct);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStruct.samplerMember");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}
// This test covers passing an array of structs containing samplers as a function argument.
TEST_P(GLSLTest, ArrayOfStructsWithSamplersAsFunctionArg)
{
    // Shader failed to compile on Nexus devices. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsAdreno() && IsOpenGLES());

    constexpr char kFS[] =
        "precision mediump float;\n"
        "struct S\n"
        "{\n"
        "    sampler2D samplerMember; \n"
        "};\n"
        "uniform S uStructs[2];\n"
        "uniform vec2 uTexCoord;\n"
        "\n"
        "vec4 foo(S[2] structs)\n"
        "{\n"
        "    return texture2D(structs[0].samplerMember, uTexCoord);\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = foo(uStructs);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStructs[0].samplerMember");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// This test covers passing a struct containing an array of samplers as a function argument.
TEST_P(GLSLTest, StructWithSamplerArrayAsFunctionArg)
{
    // Shader failed to compile on Nexus devices. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsAdreno() && IsOpenGLES());

    // TODO(jmadill): Fix on Android/vulkan if possible. http://anglebug.com/2703
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kFS[] =
        "precision mediump float;\n"
        "struct S\n"
        "{\n"
        "    sampler2D samplerMembers[2];\n"
        "};\n"
        "uniform S uStruct;\n"
        "uniform vec2 uTexCoord;\n"
        "\n"
        "vec4 foo(S str)\n"
        "{\n"
        "    return texture2D(str.samplerMembers[0], uTexCoord);\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = foo(uStruct);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStruct.samplerMembers[0]");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// This test covers passing nested structs containing a sampler as a function argument.
TEST_P(GLSLTest, NestedStructsWithSamplersAsFunctionArg)
{
    // Shader failed to compile on Nexus devices. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsAdreno() && IsOpenGLES());

    const char kFragmentShader[] = R"(precision mediump float;
struct S { sampler2D samplerMember; };
struct T { S nest; };
uniform T uStruct;
uniform vec2 uTexCoord;
vec4 foo2(S structVar)
{
    return texture2D(structVar.samplerMember, uTexCoord);
}
vec4 foo(T structVar)
{
    return foo2(structVar.nest);
}
void main()
{
    gl_FragColor = foo(uStruct);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStruct.nest.samplerMember");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// This test covers passing a compound structs containing a sampler as a function argument.
TEST_P(GLSLTest, CompoundStructsWithSamplersAsFunctionArg)
{
    // Shader failed to compile on Nexus devices. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsAdreno() && IsOpenGLES());

    const char kFragmentShader[] = R"(precision mediump float;
struct S { sampler2D samplerMember; bool b; };
uniform S uStruct;
uniform vec2 uTexCoord;
vec4 foo(S structVar)
{
    if (structVar.b)
        return texture2D(structVar.samplerMember, uTexCoord);
    else
        return vec4(1, 0, 0, 1);
}
void main()
{
    gl_FragColor = foo(uStruct);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStruct.samplerMember");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);
    GLint bLoc = glGetUniformLocation(program, "uStruct.b");
    ASSERT_NE(-1, bLoc);
    glUniform1i(bLoc, 1);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// This test covers passing nested compound structs containing a sampler as a function argument.
TEST_P(GLSLTest, NestedCompoundStructsWithSamplersAsFunctionArg)
{
    // Shader failed to compile on Nexus devices. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsAdreno() && IsOpenGLES());

    const char kFragmentShader[] = R"(precision mediump float;
struct S { sampler2D samplerMember; bool b; };
struct T { S nest; bool b; };
uniform T uStruct;
uniform vec2 uTexCoord;
vec4 foo2(S structVar)
{
    if (structVar.b)
        return texture2D(structVar.samplerMember, uTexCoord);
    else
        return vec4(1, 0, 0, 1);
}
vec4 foo(T structVar)
{
    if (structVar.b)
        return foo2(structVar.nest);
    else
        return vec4(1, 0, 0, 1);
}
void main()
{
    gl_FragColor = foo(uStruct);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStruct.nest.samplerMember");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);

    GLint bLoc = glGetUniformLocation(program, "uStruct.b");
    ASSERT_NE(-1, bLoc);
    glUniform1i(bLoc, 1);

    GLint nestbLoc = glGetUniformLocation(program, "uStruct.nest.b");
    ASSERT_NE(-1, nestbLoc);
    glUniform1i(nestbLoc, 1);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// Same as the prior test but with reordered struct members.
TEST_P(GLSLTest, MoreNestedCompoundStructsWithSamplersAsFunctionArg)
{
    // Shader failed to compile on Nexus devices. http://anglebug.com/2114
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && IsAdreno() && IsOpenGLES());

    const char kFragmentShader[] = R"(precision mediump float;
struct S { bool b; sampler2D samplerMember; };
struct T { bool b; S nest; };
uniform T uStruct;
uniform vec2 uTexCoord;
vec4 foo2(S structVar)
{
    if (structVar.b)
        return texture2D(structVar.samplerMember, uTexCoord);
    else
        return vec4(1, 0, 0, 1);
}
vec4 foo(T structVar)
{
    if (structVar.b)
        return foo2(structVar.nest);
    else
        return vec4(1, 0, 0, 1);
}
void main()
{
    gl_FragColor = foo(uStruct);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragmentShader);

    // Initialize the texture with green.
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLubyte texData[] = {0u, 255u, 0u, 255u};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw
    glUseProgram(program);
    GLint samplerMemberLoc = glGetUniformLocation(program, "uStruct.nest.samplerMember");
    ASSERT_NE(-1, samplerMemberLoc);
    glUniform1i(samplerMemberLoc, 0);
    GLint texCoordLoc = glGetUniformLocation(program, "uTexCoord");
    ASSERT_NE(-1, texCoordLoc);
    glUniform2f(texCoordLoc, 0.5f, 0.5f);

    GLint bLoc = glGetUniformLocation(program, "uStruct.b");
    ASSERT_NE(-1, bLoc);
    glUniform1i(bLoc, 1);

    GLint nestbLoc = glGetUniformLocation(program, "uStruct.nest.b");
    ASSERT_NE(-1, nestbLoc);
    glUniform1i(nestbLoc, 1);

    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}
// Test that a global variable declared after main() works. This is a regression test for an issue
// in global variable initialization.
TEST_P(WebGLGLSLTest, GlobalVariableDeclaredAfterMain)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "int getFoo();\n"
        "uniform int u_zero;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1, 0, 0, 1);\n"
        "    if (getFoo() == 0)\n"
        "    {\n"
        "        gl_FragColor = vec4(0, 1, 0, 1);\n"
        "    }\n"
        "}\n"
        "int foo;\n"
        "int getFoo()\n"
        "{\n"
        "    foo = u_zero;\n"
        "    return foo;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression having side effects inside a loop condition.
// The spec says that sequence operator operands need to run in sequence.
TEST_P(GLSLTest_ES3, ArrayLengthOnExpressionWithSideEffectsInLoopCondition)
{
    // "a" gets doubled three times in the below program.
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
uniform int u_zero;
int a;
int[2] doubleA()
{
    a *= 2;
    return int[2](a, a);
}
void main()
{
    a = u_zero + 1;
    for (int i = 0; i < doubleA().length(); ++i)
    {}
    if (a == 8)
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
    else
    {
        my_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression having side effects that interact with side
// effects of another operand of the same sequence operator. The spec says that sequence operator
// operands need to run in order from left to right (ESSL 3.00.6 section 5.9).
TEST_P(GLSLTest_ES3, ArrayLengthOnExpressionWithSideEffectsInSequence)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
uniform int u_zero;
int a;
int[3] doubleA()
{
    a *= 2;
    return int[3](a, a, a);
}
void main()
{
    a = u_zero;
    int b = (a++, doubleA().length());
    if (b == 3 && a == 2)
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
    else
    {
        my_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression that also contains a call of array length().
// Both "this" expressions also have side effects.
TEST_P(GLSLTest_ES3, NestedArrayLengthMethodsWithSideEffects)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
uniform int u_zero;
int a;
int[3] multiplyA(int multiplier)
{
    a *= multiplier;
    return int[3](a, a, a);
}
void main()
{
    a = u_zero + 1;
    int b = multiplyA(multiplyA(2).length()).length();
    if (b == 3 && a == 6)
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
    else
    {
        my_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression having side effects inside an if condition.
// This is an issue if the the side effect can be short circuited.
TEST_P(GLSLTest_ES3, ArrayLengthOnShortCircuitedExpressionWithSideEffectsInIfCondition)
{
    // Bug in the shader translator.  http://anglebug.com/3829
    ANGLE_SKIP_TEST_IF(true);

    // "a" shouldn't get modified by this shader.
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
uniform int u_zero;
int a;
int[2] doubleA()
{
    a *= 2;
    return int[2](a, a);
}
void main()
{
    a = u_zero + 1;
    if (u_zero != 0 && doubleA().length() == 2)
    {
        ++a;
    }
    if (a == 1)
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
    else
    {
        my_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression having side effects in a statement where the
// side effect can be short circuited.
TEST_P(GLSLTest_ES3, ArrayLengthOnShortCircuitedExpressionWithSideEffectsInStatement)
{
    // Bug in the shader translator.  http://anglebug.com/3829
    ANGLE_SKIP_TEST_IF(true);

    // "a" shouldn't get modified by this shader.
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
uniform int u_zero;
int a;
int[2] doubleA()
{
    a *= 2;
    return int[2](a, a);
}
void main()
{
    a = u_zero + 1;
    bool test = u_zero != 0 && doubleA().length() == 2;
    if (a == 1)
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
    else
    {
        my_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that statements inside switch() get translated to correct HLSL.
TEST_P(GLSLTest_ES3, DifferentStatementsInsideSwitch)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
uniform int u;
void main()
{
    switch (u)
    {
        case 0:
            ivec2 i;
            i.yx;
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
}

// Test that switch fall-through works correctly.
// This is a regression test for http://anglebug.com/2178
TEST_P(GLSLTest_ES3, SwitchFallThroughCodeDuplication)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
uniform int u_zero;

void main()
{
    int i = 0;
    // switch should fall through both cases.
    switch(u_zero)
    {
        case 0:
            i += 1;
        case 1:
            i += 2;
    }
    if (i == 3)
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
    else
    {
        my_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a switch statement with an empty block inside as a final statement compiles.
TEST_P(GLSLTest_ES3, SwitchFinalCaseHasEmptyBlock)
{
    constexpr char kFS[] = R"(#version 300 es

precision mediump float;
uniform int i;
void main()
{
    switch (i)
    {
        case 0:
            break;
        default:
            {}
    }
})";
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
}

// Test that a switch statement with an empty declaration inside as a final statement compiles.
TEST_P(GLSLTest_ES3, SwitchFinalCaseHasEmptyDeclaration)
{
    constexpr char kFS[] = R"(#version 300 es

precision mediump float;
uniform int i;
void main()
{
    switch (i)
    {
        case 0:
            break;
        default:
            float;
    }
})";
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
}

// Test switch/case where break/return statements are within blocks.
TEST_P(GLSLTest_ES3, SwitchBreakOrReturnInsideBlocks)
{
    constexpr char kFS[] = R"(#version 300 es

precision highp float;

uniform int u_zero;
out vec4 my_FragColor;

bool test(int n)
{
    switch(n) {
        case 0:
        {
            {
                break;
            }
        }
        case 1:
        {
            return true;
        }
        case 2:
        {
            n++;
        }
    }
    return false;
}

void main()
{
    my_FragColor = test(u_zero + 1) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test switch/case where a variable is declared inside one of the cases and is accessed by a
// subsequent case.
TEST_P(GLSLTest_ES3, SwitchWithVariableDeclarationInside)
{
    constexpr char kFS[] = R"(#version 300 es

precision highp float;
out vec4 my_FragColor;

uniform int u_zero;

void main()
{
    my_FragColor = vec4(1, 0, 0, 1);
    switch (u_zero)
    {
        case 0:
            ivec2 i;
            i = ivec2(1, 0);
        default:
            my_FragColor = vec4(0, i[0], 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test nested switch/case where a variable is declared inside one of the cases and is accessed by a
// subsequent case.
TEST_P(GLSLTest_ES3, NestedSwitchWithVariableDeclarationInside)
{
    constexpr char kFS[] = R"(#version 300 es

precision highp float;
out vec4 my_FragColor;

uniform int u_zero;
uniform int u_zero2;

void main()
{
    my_FragColor = vec4(1, 0, 0, 1);
    switch (u_zero)
    {
        case 0:
            ivec2 i;
            i = ivec2(1, 0);
            switch (u_zero2)
            {
                case 0:
                    int j;
                default:
                    j = 1;
                    i *= j;
            }
        default:
            my_FragColor = vec4(0, i[0], 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that an empty switch/case statement is translated in a way that compiles and executes the
// init-statement.
TEST_P(GLSLTest_ES3, EmptySwitch)
{
    constexpr char kFS[] = R"(#version 300 es

precision highp float;

uniform int u_zero;
out vec4 my_FragColor;

void main()
{
    int i = u_zero;
    switch(++i) {}
    my_FragColor = (i == 1) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a constant struct inside an expression is handled correctly.
TEST_P(GLSLTest_ES3, ConstStructInsideExpression)
{
    // Incorrect output color was seen on Android. http://anglebug.com/2226
    ANGLE_SKIP_TEST_IF(IsAndroid() && !IsNVIDIA() && IsOpenGLES());

    constexpr char kFS[] = R"(#version 300 es

precision highp float;
out vec4 my_FragColor;

uniform float u_zero;

struct S
{
    float field;
};

void main()
{
    const S constS = S(1.0);
    S nonConstS = constS;
    nonConstS.field = u_zero;
    bool fail = (constS == nonConstS);
    my_FragColor = vec4(0, 1, 0, 1);
    if (fail)
    {
        my_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a varying struct that's defined as a part of the declaration is handled correctly.
TEST_P(GLSLTest_ES3, VaryingStructWithInlineDefinition)
{
    constexpr char kVS[] = R"(#version 300 es
in vec4 inputAttribute;

flat out struct S
{
    int field;
} v_s;

void main()
{
    v_s.field = 1;
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(#version 300 es

precision highp float;
out vec4 my_FragColor;

flat in struct S
{
    int field;
} v_s;

void main()
{
    bool success = (v_s.field == 1);
    my_FragColor = vec4(1, 0, 0, 1);
    if (success)
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test vector/scalar arithmetic (in this case multiplication and addition). Meant to reproduce a
// bug that appeared in NVIDIA OpenGL drivers and that is worked around by
// VectorizeVectorScalarArithmetic AST transform.
TEST_P(GLSLTest, VectorScalarMultiplyAndAddInLoop)
{
    constexpr char kFS[] = R"(precision mediump float;

void main() {
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 2; i++)
    {
        gl_FragColor += (2.0 * gl_FragCoord.x);
    }
    if (gl_FragColor.g == gl_FragColor.r &&
        gl_FragColor.b == gl_FragColor.r &&
        gl_FragColor.a == gl_FragColor.r)
    {
        gl_FragColor = vec4(0, 1, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test vector/scalar arithmetic (in this case compound division and addition). Meant to reproduce a
// bug that appeared in NVIDIA OpenGL drivers and that is worked around by
// VectorizeVectorScalarArithmetic AST transform.
TEST_P(GLSLTest, VectorScalarDivideAndAddInLoop)
{
    constexpr char kFS[] = R"(precision mediump float;

void main() {
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 2; i++)
    {
        float x = gl_FragCoord.x;
        gl_FragColor = gl_FragColor + (x /= 2.0);
    }
    if (gl_FragColor.g == gl_FragColor.r &&
        gl_FragColor.b == gl_FragColor.r &&
        gl_FragColor.a == gl_FragColor.r)
    {
        gl_FragColor = vec4(0, 1, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a varying with a flat qualifier that is used as an operand of a folded ternary operator
// is handled correctly.
TEST_P(GLSLTest_ES3, FlatVaryingUsedInFoldedTernary)
{
    constexpr char kVS[] = R"(#version 300 es

in vec4 inputAttribute;

flat out int v;

void main()
{
    v = 1;
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(#version 300 es

precision highp float;
out vec4 my_FragColor;

flat in int v;

void main()
{
    my_FragColor = vec4(0, (true ? v : 0), 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Verify that the link error message from last link failure is cleared when the new link is
// finished.
TEST_P(GLSLTest, ClearLinkErrorLog)
{
    constexpr char kVS[] = R"(attribute vec4 vert_in;
varying vec4 vert_out;
void main()
{
    gl_Position = vert_in;
    vert_out = vert_in;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec4 frag_in;
void main()
{
    gl_FragColor = frag_in;
})";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVS);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);

    GLuint program = glCreateProgram();

    // The first time the program link fails because of lack of fragment shader.
    glAttachShader(program, vs);
    glLinkProgram(program);
    GLint linkStatus = GL_TRUE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    ASSERT_FALSE(linkStatus);

    const std::string &lackOfFragmentShader = QueryErrorMessage(program);

    // The second time the program link fails because of the mismatch of the varying types.
    glAttachShader(program, fs);
    glLinkProgram(program);
    linkStatus = GL_TRUE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    ASSERT_FALSE(linkStatus);

    const std::string &varyingTypeMismatch = QueryErrorMessage(program);

    EXPECT_EQ(std::string::npos, varyingTypeMismatch.find(lackOfFragmentShader));

    glDetachShader(program, vs);
    glDetachShader(program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(program);

    ASSERT_GL_NO_ERROR();
}

// Validate error messages when the link mismatch occurs on the type of a non-struct varying.
TEST_P(GLSLTest, ErrorMessageOfVaryingMismatch)
{
    constexpr char kVS[] = R"(attribute vec4 inputAttribute;
varying vec4 vertex_out;
void main()
{
    vertex_out = inputAttribute;
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying float vertex_out;
void main()
{
    gl_FragColor = vec4(vertex_out, 0.0, 0.0, 1.0);
})";

    validateComponentsInErrorMessage(kVS, kFS, "Types", "varying 'vertex_out'");
}

// Validate error messages when the link mismatch occurs on the name of a varying field.
TEST_P(GLSLTest_ES3, ErrorMessageOfVaryingStructFieldNameMismatch)
{
    constexpr char kVS[] = R"(#version 300 es
in vec4 inputAttribute;
struct S {
    float val1;
    vec4 val2;
};
out S vertex_out;
void main()
{
    vertex_out.val2 = inputAttribute;
    vertex_out.val1 = inputAttribute[0];
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
struct S {
    float val1;
    vec4 val3;
};
in S vertex_out;
layout (location = 0) out vec4 frag_out;
void main()
{
    frag_out = vec4(vertex_out.val1, 0.0, 0.0, 1.0);
})";

    validateComponentsInErrorMessage(kVS, kFS, "Field names", "varying 'vertex_out'");
}

// Validate error messages when the link mismatch occurs on the type of a varying field.
TEST_P(GLSLTest_ES3, ErrorMessageOfVaryingStructFieldMismatch)
{
    constexpr char kVS[] = R"(#version 300 es
in vec4 inputAttribute;
struct S {
    float val1;
    vec4 val2;
};
out S vertex_out;
void main()
{
    vertex_out.val2 = inputAttribute;
    vertex_out.val1 = inputAttribute[0];
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
struct S {
    float val1;
    vec2 val2;
};
in S vertex_out;
layout (location = 0) out vec4 frag_out;
void main()
{
    frag_out = vec4(vertex_out.val1, 0.0, 0.0, 1.0);
})";

    validateComponentsInErrorMessage(kVS, kFS, "Types",
                                     "varying 'vertex_out' member 'vertex_out.val2'");
}

// Validate error messages when the link mismatch occurs on the name of a struct member of a uniform
// field.
TEST_P(GLSLTest, ErrorMessageOfLinkUniformStructFieldNameMismatch)
{
    constexpr char kVS[] = R"(
struct T
{
    vec2 t1;
    vec3 t2;
};
struct S {
    T val1;
    vec4 val2;
};
uniform S uni;

attribute vec4 inputAttribute;
varying vec4 vertex_out;
void main()
{
    vertex_out = uni.val2;
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(precision highp float;
struct T
{
    vec2 t1;
    vec3 t3;
};
struct S {
    T val1;
    vec4 val2;
};
uniform S uni;

varying vec4 vertex_out;
void main()
{
    gl_FragColor = vec4(uni.val1.t1[0], 0.0, 0.0, 1.0);
})";

    validateComponentsInErrorMessage(kVS, kFS, "Field names", "uniform 'uni' member 'uni.val1'");
}

// Validate error messages  when the link mismatch occurs on the type of a non-struct uniform block
// field.
TEST_P(GLSLTest_ES3, ErrorMessageOfLinkInterfaceBlockFieldMismatch)
{
    constexpr char kVS[] = R"(#version 300 es
uniform S {
    vec2 val1;
    vec4 val2;
} uni;

in vec4 inputAttribute;
out vec4 vertex_out;
void main()
{
    vertex_out = uni.val2;
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
uniform S {
    vec2 val1;
    vec3 val2;
} uni;

in vec4 vertex_out;
layout (location = 0) out vec4 frag_out;
void main()
{
    frag_out = vec4(uni.val1[0], 0.0, 0.0, 1.0);
})";

    validateComponentsInErrorMessage(kVS, kFS, "Types", "uniform block 'S' member 'S.val2'");
}

// Validate error messages  when the link mismatch occurs on the type of a member of a uniform block
// struct field.
TEST_P(GLSLTest_ES3, ErrorMessageOfLinkInterfaceBlockStructFieldMismatch)
{
    constexpr char kVS[] = R"(#version 300 es
struct T
{
    vec2 t1;
    vec3 t2;
};
uniform S {
    T val1;
    vec4 val2;
} uni;

in vec4 inputAttribute;
out vec4 vertex_out;
void main()
{
    vertex_out = uni.val2;
    gl_Position = inputAttribute;
})";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
struct T
{
    vec2 t1;
    vec4 t2;
};
uniform S {
    T val1;
    vec4 val2;
} uni;

in vec4 vertex_out;
layout (location = 0) out vec4 frag_out;
void main()
{
    frag_out = vec4(uni.val1.t1[0], 0.0, 0.0, 1.0);
})";

    validateComponentsInErrorMessage(kVS, kFS, "Types", "uniform block 'S' member 'S.val1.t2'");
}

// Test a vertex shader that doesn't declare any varyings with a fragment shader that statically
// uses a varying, but in a statement that gets trivially optimized out by the compiler.
TEST_P(GLSLTest_ES3, FragmentShaderStaticallyUsesVaryingMissingFromVertex)
{
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;

void main()
{
    gl_Position = vec4(0, 1, 0, 1);
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in float foo;
out vec4 my_FragColor;

void main()
{
    if (false)
    {
        float unreferenced = foo;
    }
    my_FragColor = vec4(0, 1, 0, 1);
})";

    validateComponentsInErrorMessage(kVS, kFS, "does not match any", "foo");
}

// Test a varying that is statically used but not active in the fragment shader.
TEST_P(GLSLTest_ES3, VaryingStaticallyUsedButNotActiveInFragmentShader)
{
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;
in vec4 iv;
out vec4 v;
void main()
{
    gl_Position = iv;
    v = iv;
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec4 v;
out vec4 color;
void main()
{
    color = true ? vec4(0.0) : v;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Test that linking varyings by location works.
TEST_P(GLSLTest_ES31, LinkVaryingsByLocation)
{
    // http://anglebug.com/4355
    ANGLE_SKIP_TEST_IF(IsVulkan() || IsMetal() || IsD3D11());

    constexpr char kVS[] = R"(#version 310 es
precision highp float;
in vec4 position;
layout(location = 1) out vec4 shaderOutput;
void main() {
    gl_Position = position;
    shaderOutput = vec4(0.0, 1.0, 0.0, 1.0);
})";

    constexpr char kFS[] = R"(#version 310 es
precision highp float;
layout(location = 1) in vec4 shaderInput;
out vec4 outColor;
void main() {
    outColor = shaderInput;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test nesting floor() calls with a large multiplier inside.
TEST_P(GLSLTest_ES3, NestedFloorWithLargeMultiplierInside)
{
    // D3D11 seems to ignore the floor() calls in this particular case, so one of the corners ends
    // up red. http://crbug.com/838885
    ANGLE_SKIP_TEST_IF(IsD3D11());

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
void main()
{
    vec2 coord = gl_FragCoord.xy / 500.0;
    my_FragColor = vec4(1, 0, 0, 1);
    if (coord.y + 0.1 > floor(1e-6 * floor(coord.x*4e5)))
    {
        my_FragColor = vec4(0, 1, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    // Verify that all the corners of the rendered result are green.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, getWindowHeight() - 1, GLColor::green);
}

// Verify that a link error is generated when the sum of the number of active image uniforms and
// active shader storage blocks in a rendering pipeline exceeds
// GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES.
TEST_P(GLSLTest_ES31, ExceedCombinedShaderOutputResourcesInVSAndFS)
{
    // TODO(jiawei.shao@intel.com): enable this test when shader storage buffer is supported on
    // D3D11 back-ends.
    ANGLE_SKIP_TEST_IF(IsD3D11());

    GLint maxVertexShaderStorageBlocks;
    GLint maxVertexImageUniforms;
    GLint maxFragmentShaderStorageBlocks;
    GLint maxFragmentImageUniforms;
    GLint maxCombinedShaderStorageBlocks;
    GLint maxCombinedImageUniforms;
    glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &maxVertexShaderStorageBlocks);
    glGetIntegerv(GL_MAX_VERTEX_IMAGE_UNIFORMS, &maxVertexImageUniforms);
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);
    glGetIntegerv(GL_MAX_FRAGMENT_IMAGE_UNIFORMS, &maxFragmentImageUniforms);
    glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &maxCombinedShaderStorageBlocks);
    glGetIntegerv(GL_MAX_COMBINED_IMAGE_UNIFORMS, &maxCombinedImageUniforms);

    ASSERT_GE(maxCombinedShaderStorageBlocks, maxVertexShaderStorageBlocks);
    ASSERT_GE(maxCombinedShaderStorageBlocks, maxFragmentShaderStorageBlocks);
    ASSERT_GE(maxCombinedImageUniforms, maxVertexImageUniforms);
    ASSERT_GE(maxCombinedImageUniforms, maxFragmentImageUniforms);

    GLint vertexSSBOs   = maxVertexShaderStorageBlocks;
    GLint fragmentSSBOs = maxFragmentShaderStorageBlocks;
    // Limit the sum of ssbos in vertex and fragment shaders to maxCombinedShaderStorageBlocks.
    if (vertexSSBOs + fragmentSSBOs > maxCombinedShaderStorageBlocks)
    {
        fragmentSSBOs = maxCombinedShaderStorageBlocks - vertexSSBOs;
    }

    GLint vertexImages   = maxVertexImageUniforms;
    GLint fragmentImages = maxFragmentImageUniforms;
    // Limit the sum of images in vertex and fragment shaders to maxCombinedImageUniforms.
    if (vertexImages + fragmentImages > maxCombinedImageUniforms)
    {
        vertexImages = maxCombinedImageUniforms - fragmentImages;
    }

    GLint maxDrawBuffers;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);

    GLint maxCombinedShaderOutputResources;
    glGetIntegerv(GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES, &maxCombinedShaderOutputResources);
    ASSERT_GL_NO_ERROR();

    ANGLE_SKIP_TEST_IF(vertexSSBOs + fragmentSSBOs + vertexImages + fragmentImages +
                           maxDrawBuffers <=
                       maxCombinedShaderOutputResources);

    std::ostringstream vertexStream;
    vertexStream << "#version 310 es\n";
    for (int i = 0; i < vertexSSBOs; ++i)
    {
        vertexStream << "layout(shared, binding = " << i << ") buffer blockName" << i
                     << "{\n"
                        "    float data;\n"
                        "} ssbo"
                     << i << ";\n";
    }
    vertexStream << "layout(r32f, binding = 0) uniform highp image2D imageArray[" << vertexImages
                 << "];\n";
    vertexStream << "void main()\n"
                    "{\n"
                    "    float val = 0.1;\n"
                    "    vec4 val2 = vec4(0.0);\n";
    for (int i = 0; i < vertexSSBOs; ++i)
    {
        vertexStream << "    val += ssbo" << i << ".data; \n";
    }
    for (int i = 0; i < vertexImages; ++i)
    {
        vertexStream << "    val2 += imageLoad(imageArray[" << i << "], ivec2(0, 0)); \n";
    }
    vertexStream << "    gl_Position = vec4(val, val2);\n"
                    "}\n";

    std::ostringstream fragmentStream;
    fragmentStream << "#version 310 es\n"
                   << "precision highp float;\n";
    for (int i = 0; i < fragmentSSBOs; ++i)
    {
        fragmentStream << "layout(shared, binding = " << i << ") buffer blockName" << i
                       << "{\n"
                          "    float data;\n"
                          "} ssbo"
                       << i << ";\n";
    }
    fragmentStream << "layout(r32f, binding = 0) uniform highp image2D imageArray["
                   << fragmentImages << "];\n";
    fragmentStream << "layout (location = 0) out vec4 foutput[" << maxDrawBuffers << "];\n";

    fragmentStream << "void main()\n"
                      "{\n"
                      "    float val = 0.1;\n"
                      "    vec4 val2 = vec4(0.0);\n";
    for (int i = 0; i < fragmentSSBOs; ++i)
    {
        fragmentStream << "    val += ssbo" << i << ".data; \n";
    }
    for (int i = 0; i < fragmentImages; ++i)
    {
        fragmentStream << "    val2 += imageLoad(imageArray[" << i << "], ivec2(0, 0)); \n";
    }
    for (int i = 0; i < maxDrawBuffers; ++i)
    {
        fragmentStream << "    foutput[" << i << "] = vec4(val, val2);\n";
    }
    fragmentStream << "}\n";

    GLuint program = CompileProgram(vertexStream.str().c_str(), fragmentStream.str().c_str());
    EXPECT_EQ(0u, program);

    ASSERT_GL_NO_ERROR();
}

// Test that assigning an assignment expression to a swizzled vector field in a user-defined
// function works correctly.
TEST_P(GLSLTest_ES3, AssignAssignmentToSwizzled)
{
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;

uniform float uzero;

vec3 fun(float s, float v)
{
    vec3 r = vec3(0);
    if (s < 1.0) {
        r.x = r.y = r.z = v;
        return r;
    }
    return r;
}

void main()
{
    my_FragColor.a = 1.0;
    my_FragColor.rgb = fun(uzero, 1.0);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
}

// Test a fragment shader that returns inside if (that being the only branch that actually gets
// executed). Regression test for http://anglebug.com/2325
TEST_P(GLSLTest, IfElseIfAndReturn)
{
    constexpr char kVS[] = R"(attribute vec4 a_position;
varying vec2 vPos;
void main()
{
    gl_Position = a_position;
    vPos = a_position.xy;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec2 vPos;
void main()
{
    if (vPos.x < 1.0) // This colors the whole canvas green
    {
        gl_FragColor = vec4(0, 1, 0, 1);
        return;
    }
    else if (vPos.x < 1.1) // This should have no effect
    {
        gl_FragColor = vec4(1, 0, 0, 1);
    }
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "a_position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests that PointCoord behaves the same betweeen a user FBO and the back buffer.
TEST_P(GLSLTest, PointCoordConsistency)
{
    // On Intel Windows OpenGL drivers PointCoord appears to be flipped when drawing to the
    // default framebuffer. http://anglebug.com/2805
    ANGLE_SKIP_TEST_IF(IsIntel() && IsWindows() && IsOpenGL());

    // AMD's OpenGL drivers may have the same issue. http://anglebug.com/1643
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsOpenGL());
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());

    constexpr char kPointCoordVS[] = R"(attribute vec2 position;
uniform vec2 viewportSize;
void main()
{
   gl_Position = vec4(position, 0, 1);
   gl_PointSize = viewportSize.x;
})";

    constexpr char kPointCoordFS[] = R"(void main()
{
    gl_FragColor = vec4(gl_PointCoord.xy, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kPointCoordVS, kPointCoordFS);
    glUseProgram(program);

    GLint uniLoc = glGetUniformLocation(program, "viewportSize");
    ASSERT_NE(-1, uniLoc);
    glUniform2f(uniLoc, static_cast<GLfloat>(getWindowWidth()),
                static_cast<GLfloat>(getWindowHeight()));

    // Draw to backbuffer.
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_POINTS, 0, 1);
    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> backbufferData(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 backbufferData.data());

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Draw to user FBO.
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_POINTS, 0, 1);
    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> userFBOData(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 userFBOData.data());

    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(userFBOData.size(), backbufferData.size());
    EXPECT_EQ(userFBOData, backbufferData);
}

bool SubrectEquals(const std::vector<GLColor> &bigArray,
                   const std::vector<GLColor> &smallArray,
                   int bigSize,
                   int offset,
                   int smallSize)
{
    int badPixels = 0;
    for (int y = 0; y < smallSize; y++)
    {
        for (int x = 0; x < smallSize; x++)
        {
            int bigOffset   = (y + offset) * bigSize + x + offset;
            int smallOffset = y * smallSize + x;
            if (bigArray[bigOffset] != smallArray[smallOffset])
                badPixels++;
        }
    }
    return badPixels == 0;
}

// Tests that FragCoord behaves the same betweeen a user FBO and the back buffer.
TEST_P(GLSLTest, FragCoordConsistency)
{
    constexpr char kFragCoordShader[] = R"(uniform mediump vec2 viewportSize;
void main()
{
    gl_FragColor = vec4(gl_FragCoord.xy / viewportSize, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFragCoordShader);
    glUseProgram(program);

    GLint uniLoc = glGetUniformLocation(program, "viewportSize");
    ASSERT_NE(-1, uniLoc);
    glUniform2f(uniLoc, static_cast<GLfloat>(getWindowWidth()),
                static_cast<GLfloat>(getWindowHeight()));

    // Draw to backbuffer.
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5);
    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> backbufferData(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 backbufferData.data());

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Draw to user FBO.
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5);
    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> userFBOData(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 userFBOData.data());

    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(userFBOData.size(), backbufferData.size());
    EXPECT_EQ(userFBOData, backbufferData)
        << "FragCoord should be the same to default and user FBO";

    // Repeat the same test but with a smaller viewport.
    ASSERT_EQ(getWindowHeight(), getWindowWidth());
    const int kQuarterSize = getWindowWidth() >> 2;
    glViewport(kQuarterSize, kQuarterSize, kQuarterSize * 2, kQuarterSize * 2);

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5);

    std::vector<GLColor> userFBOViewportData(kQuarterSize * kQuarterSize * 4);
    glReadPixels(kQuarterSize, kQuarterSize, kQuarterSize * 2, kQuarterSize * 2, GL_RGBA,
                 GL_UNSIGNED_BYTE, userFBOViewportData.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5);

    std::vector<GLColor> defaultFBOViewportData(kQuarterSize * kQuarterSize * 4);
    glReadPixels(kQuarterSize, kQuarterSize, kQuarterSize * 2, kQuarterSize * 2, GL_RGBA,
                 GL_UNSIGNED_BYTE, defaultFBOViewportData.data());
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(userFBOViewportData, defaultFBOViewportData)
        << "FragCoord should be the same to default and user FBO even with a custom viewport";

    // Check that the subrectangles are the same between the viewport and non-viewport modes.
    EXPECT_TRUE(SubrectEquals(userFBOData, userFBOViewportData, getWindowWidth(), kQuarterSize,
                              kQuarterSize * 2));
    EXPECT_TRUE(SubrectEquals(backbufferData, defaultFBOViewportData, getWindowWidth(),
                              kQuarterSize, kQuarterSize * 2));
}

// Ensure that using defined in a macro works in this simple case. This mirrors a dEQP test.
TEST_P(GLSLTest, DefinedInMacroSucceeds)
{
    constexpr char kVS[] = R"(precision mediump float;
attribute highp vec4 position;
varying vec2 out0;

void main()
{
#define AAA defined(BBB)

#if !AAA
    out0 = vec2(0.0, 1.0);
#else
    out0 = vec2(1.0, 0.0);
#endif
    gl_Position = position;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec2 out0;
void main()
{
    gl_FragColor = vec4(out0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Validate the defined operator is evaluated when the macro is called, not when defined.
TEST_P(GLSLTest, DefinedInMacroWithUndef)
{
    constexpr char kVS[] = R"(precision mediump float;
attribute highp vec4 position;
varying vec2 out0;

void main()
{
#define BBB 1
#define AAA defined(BBB)
#undef BBB

#if AAA
    out0 = vec2(1.0, 0.0);
#else
    out0 = vec2(0.0, 1.0);
#endif
    gl_Position = position;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec2 out0;
void main()
{
    gl_FragColor = vec4(out0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Validate the defined operator is evaluated when the macro is called, not when defined.
TEST_P(GLSLTest, DefinedAfterMacroUsage)
{
    constexpr char kVS[] = R"(precision mediump float;
attribute highp vec4 position;
varying vec2 out0;

void main()
{
#define AAA defined(BBB)
#define BBB 1

#if AAA
    out0 = vec2(0.0, 1.0);
#else
    out0 = vec2(1.0, 0.0);
#endif
    gl_Position = position;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec2 out0;
void main()
{
    gl_FragColor = vec4(out0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test generating "defined" by concatenation when a macro is called. This is not allowed.
TEST_P(GLSLTest, DefinedInMacroConcatenationNotAllowed)
{
    constexpr char kVS[] = R"(precision mediump float;
attribute highp vec4 position;
varying vec2 out0;

void main()
{
#define BBB 1
#define AAA(defi, ned) defi ## ned(BBB)

#if AAA(defi, ned)
    out0 = vec2(0.0, 1.0);
#else
    out0 = vec2(1.0, 0.0);
#endif
    gl_Position = position;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec2 out0;
void main()
{
    gl_FragColor = vec4(out0, 0, 1);
})";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
    glDeleteProgram(program);
}

// Test using defined in a macro parameter name. This is not allowed.
TEST_P(GLSLTest, DefinedAsParameterNameNotAllowed)
{
    constexpr char kVS[] = R"(precision mediump float;
attribute highp vec4 position;
varying vec2 out0;

void main()
{
#define BBB 1
#define AAA(defined) defined(BBB)

#if AAA(defined)
    out0 = vec2(0.0, 1.0);
#else
    out0 = vec2(1.0, 0.0);
#endif
    gl_Position = position;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying vec2 out0;
void main()
{
    gl_FragColor = vec4(out0, 0, 1);
})";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
    glDeleteProgram(program);
}

// Ensure that defined in a macro is no accepted in WebGL.
TEST_P(WebGLGLSLTest, DefinedInMacroFails)
{
    constexpr char kVS[] = R"(precision mediump float;
attribute highp vec4 position;
varying float out0;

void main()
{
#define AAA defined(BBB)

#if !AAA
    out0 = 1.0;
#else
    out0 = 0.0;
#endif
    gl_Position = dEQP_Position;
})";

    constexpr char kFS[] = R"(precision mediump float;
varying float out0;
void main()
{
    gl_FragColor = vec4(out0, 0, 0, 1);
})";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
    glDeleteProgram(program);
}

// Simple test using a define macro in WebGL.
TEST_P(WebGLGLSLTest, DefinedGLESSymbol)
{
    constexpr char kVS[] = R"(void main()
{
    gl_Position = vec4(1, 0, 0, 1);
})";

    constexpr char kFS[] = R"(#if defined(GL_ES)
precision mediump float;
void main()
{
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
#else
foo
#endif
)";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
}

// Tests constant folding of non-square 'matrixCompMult'.
TEST_P(GLSLTest_ES3, NonSquareMatrixCompMult)
{
    constexpr char kFS[] = R"(#version 300 es
precision mediump float;

const mat4x2 matA = mat4x2(2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0);
const mat4x2 matB = mat4x2(1.0/2.0, 1.0/4.0, 1.0/8.0, 1.0/16.0, 1.0/32.0, 1.0/64.0, 1.0/128.0, 1.0/256.0);

out vec4 color;

void main()
{
    mat4x2 result = matrixCompMult(matA, matB);
    vec2 vresult = result * vec4(1.0, 1.0, 1.0, 1.0);
    if (vresult == vec2(4.0, 4.0))
    {
        color = vec4(0.0, 1.0, 0.0, 1.0);
    }
    else
    {
        color = vec4(1.0, 0.0, 0.0, 1.0);
    }
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test initializing an array with the same name of previously declared array
TEST_P(GLSLTest_ES3, InitSameNameArray)
{
    constexpr char kFS[] = R"(#version 300 es
      precision highp float;
      out vec4 my_FragColor;

      void main()
      {
          float arr[2] = float[2](1.0, 1.0);
          {
              float arr[2] = arr;
              my_FragColor = vec4(0.0, arr[0], 0.0, arr[1]);
          }
      })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    drawQuad(program, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests using gl_FragData[0] instead of gl_FragColor.
TEST_P(GLSLTest, FragData)
{
    // Ensures that we don't regress and emit Vulkan layer warnings.
    // TODO(jonahr): http://anglebug.com/3900 - Remove check once warnings are cleaned up
    if (IsVulkan())
    {
        treatPlatformWarningsAsErrors();
    }

    constexpr char kFS[] = R"(void main() { gl_FragData[0] = vec4(1, 0, 0, 1); })";
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Test angle can handle big initial stack size with dynamic stack allocation.
TEST_P(GLSLTest, MemoryExhaustedTest)
{
    ANGLE_SKIP_TEST_IF(IsD3D11_FL93());
    GLuint program =
        CompileProgram(essl1_shaders::vs::Simple(), BuillBigInitialStackShader(36).c_str());
    EXPECT_NE(0u, program);
}

// Test that inactive samplers in structs don't cause any errors.
TEST_P(GLSLTest, InactiveSamplersInStruct)
{
    // While the sampler is being extracted and declared outside of the struct, it's not removed
    // from the struct definition.  http://anglebug.com/4211
    ANGLE_SKIP_TEST_IF(IsVulkan() || IsMetal());

    constexpr char kVS[] = R"(attribute vec4 a_position;
void main() {
  gl_Position = a_position;
})";

    constexpr char kFS[] = R"(precision highp float;
struct S
{
    vec4 v;
    sampler2D t[10];
};
uniform S s;
void main() {
  gl_FragColor = s.v;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    drawQuad(program, "a_position", 0.5f);
}

// Helper functions for MixedRowAndColumnMajorMatrices* tests

// Round up to alignment, assuming it's a power of 2
uint32_t RoundUpPow2(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void CreateOutputBuffer(GLBuffer *buffer, uint32_t binding)
{
    unsigned int outputInitData = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(outputInitData), &outputInitData, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, *buffer);
    EXPECT_GL_NO_ERROR();
}

// Fill provided buffer with matrices based on the given dimensions.  The buffer should be large
// enough to accomodate the data.
uint32_t FillBuffer(const std::pair<uint32_t, uint32_t> matrixDims[],
                    const bool matrixIsColMajor[],
                    size_t matrixCount,
                    float data[],
                    bool isStd430,
                    bool isTransposed)
{
    size_t offset = 0;
    for (size_t m = 0; m < matrixCount; ++m)
    {
        uint32_t cols   = matrixDims[m].first;
        uint32_t rows   = matrixDims[m].second;
        bool isColMajor = matrixIsColMajor[m] != isTransposed;

        uint32_t arraySize              = isColMajor ? cols : rows;
        uint32_t arrayElementComponents = isColMajor ? rows : cols;
        // Note: stride is generally 4 with std140, except for scalar and gvec2 types (which
        // MixedRowAndColumnMajorMatrices* tests don't use).  With std430, small matrices can have
        // a stride of 2 between rows/columns.
        uint32_t stride = isStd430 ? RoundUpPow2(arrayElementComponents, 2) : 4;

        offset = RoundUpPow2(offset, stride);

        for (uint32_t i = 0; i < arraySize; ++i)
        {
            for (uint32_t c = 0; c < arrayElementComponents; ++c)
            {
                uint32_t row = isColMajor ? c : i;
                uint32_t col = isColMajor ? i : c;

                data[offset + i * stride + c] = col * 4 + row;
            }
        }

        offset += arraySize * stride;
    }
    return offset;
}

// Initialize and bind the buffer.
void InitBuffer(GLuint program,
                const char *name,
                GLuint buffer,
                uint32_t bindingIndex,
                float data[],
                uint32_t dataSize,
                bool isUniform)
{
    GLenum bindPoint = isUniform ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;

    glBindBufferBase(bindPoint, bindingIndex, buffer);
    glBufferData(bindPoint, dataSize * sizeof(*data), data, GL_STATIC_DRAW);

    if (isUniform)
    {
        GLint blockIndex = glGetUniformBlockIndex(program, name);
        glUniformBlockBinding(program, blockIndex, bindingIndex);
    }
}

// Verify that buffer data is written by the shader as expected.
bool VerifyBuffer(GLuint buffer, const float data[], uint32_t dataSize)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);

    const float *ptr = reinterpret_cast<const float *>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, dataSize, GL_MAP_READ_BIT));

    bool isCorrect = memcmp(ptr, data, dataSize * sizeof(*data)) == 0;
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    return isCorrect;
}

// Verify that the success output of the shader is as expected.
bool VerifySuccess(GLuint buffer)
{
    uint32_t success = 1;
    return VerifyBuffer(buffer, reinterpret_cast<const float *>(&success), 1);
}

// Test reading from UBOs and SSBOs and writing to SSBOs with mixed row- and colum-major layouts in
// both std140 and std430 layouts.  Tests many combinations of std140 vs std430, struct being used
// as row- or column-major in different UBOs, reading from UBOs and SSBOs and writing to SSBOs,
// nested structs, matrix arrays, inout parameters etc.
//
// Some very specific corner cases that are not covered here are tested in the subsequent tests.
TEST_P(GLSLTest_ES31, MixedRowAndColumnMajorMatrices)
{
    GLint maxComputeShaderStorageBlocks;
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &maxComputeShaderStorageBlocks);

    // The test uses 9 SSBOs.  Skip if not that many are supported.
    ANGLE_SKIP_TEST_IF(maxComputeShaderStorageBlocks < 9);

    // Fails on Nvidia because having |Matrices| qualified as row-major in one UBO makes the other
    // UBO also see it as row-major despite explicit column-major qualifier.
    // http://anglebug.com/3830
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsOpenGL());

    // Fails on mesa because in the first UBO which is qualified as column-major, |Matrices| is
    // read column-major despite explicit row-major qualifier.  http://anglebug.com/3837
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsOpenGL());

    // Fails on windows AMD on GL: http://anglebug.com/3838
    ANGLE_SKIP_TEST_IF(IsWindows() && IsOpenGL() && IsAMD());

    // Fails to compile the shader on Android.  http://anglebug.com/3839
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGL());

    // Fails on assertion in translation to D3D.  http://anglebug.com/3841
    ANGLE_SKIP_TEST_IF(IsD3D11());

    // Fails on SSBO validation on Android/Vulkan.  http://anglebug.com/3840
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    // Fails input verification as well as std140 SSBO validation.  http://anglebug.com/3844
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsVulkan());

    constexpr char kCS[] = R"(#version 310 es
precision highp float;
layout(local_size_x=1) in;

struct Inner
{
    mat3x4 m3c4r;
    mat4x3 m4c3r;
};

struct Matrices
{
    mat2 m2c2r;
    mat2x3 m2c3r[2];
    mat3x2 m3c2r;
    Inner inner;
};

// For simplicity, the layouts are either of:
// - col-major mat4, row-major rest
// - row-major mat4, col-major rest
//
// The former is tagged with c, the latter with r.
layout(std140, column_major) uniform Ubo140c
{
    mat4 m4c4r;
    layout(row_major) Matrices m;
} ubo140cIn;

layout(std140, row_major) uniform Ubo140r
{
    mat4 m4c4r;
    layout(column_major) Matrices m;
} ubo140rIn;

layout(std140, row_major, binding = 0) buffer Ssbo140c
{
    layout(column_major) mat4 m4c4r;
    Matrices m;
} ssbo140cIn;

layout(std140, column_major, binding = 1) buffer Ssbo140r
{
    layout(row_major) mat4 m4c4r;
    Matrices m;
} ssbo140rIn;

layout(std430, column_major, binding = 2) buffer Ssbo430c
{
    mat4 m4c4r;
    layout(row_major) Matrices m;
} ssbo430cIn;

layout(std430, row_major, binding = 3) buffer Ssbo430r
{
    mat4 m4c4r;
    layout(column_major) Matrices m;
} ssbo430rIn;

layout(std140, row_major, binding = 4) buffer Ssbo140cOut
{
    layout(column_major) mat4 m4c4r;
    Matrices m;
} ssbo140cOut;

layout(std140, column_major, binding = 5) buffer Ssbo140rOut
{
    layout(row_major) mat4 m4c4r;
    Matrices m;
} ssbo140rOut;

layout(std430, column_major, binding = 6) buffer Ssbo430cOut
{
    mat4 m4c4r;
    layout(row_major) Matrices m;
} ssbo430cOut;

layout(std430, row_major, binding = 7) buffer Ssbo430rOut
{
    mat4 m4c4r;
    layout(column_major) Matrices m;
} ssbo430rOut;

layout(std140, binding = 8) buffer Result
{
    uint success;
} resultOut;

#define EXPECT(result, expression, value) if ((expression) != value) { result = false; }
#define EXPECTV(result, expression, value) if (any(notEqual(expression, value))) { result = false; }

#define VERIFY_IN(result, mat, cols, rows)                  \
    EXPECT(result, mat[0].x, 0.0);                          \
    EXPECT(result, mat[0][1], 1.0);                         \
    EXPECTV(result, mat[0].xy, vec2(0, 1));                 \
    EXPECTV(result, mat[1].xy, vec2(4, 5));                 \
    for (int c = 0; c < cols; ++c)                          \
    {                                                       \
        for (int r = 0; r < rows; ++r)                      \
        {                                                   \
            EXPECT(result, mat[c][r], float(c * 4 + r));    \
        }                                                   \
    }

#define COPY(matIn, matOut, cols, rows)     \
    matOut = matOut + matIn;                \
    /* random operations for testing */     \
    matOut[0].x += matIn[0].x + matIn[1].x; \
    matOut[0].x -= matIn[1].x;              \
    matOut[0][1] += matIn[0][1];            \
    matOut[1] += matIn[1];                  \
    matOut[1].xy -= matIn[1].xy;            \
    /* undo the above to get back matIn */  \
    matOut[0].x -= matIn[0].x;              \
    matOut[0][1] -= matIn[0][1];            \
    matOut[1] -= matIn[1];                  \
    matOut[1].xy += matIn[1].xy;

bool verifyMatrices(in Matrices m)
{
    bool result = true;
    VERIFY_IN(result, m.m2c2r, 2, 2);
    VERIFY_IN(result, m.m2c3r[0], 2, 3);
    VERIFY_IN(result, m.m2c3r[1], 2, 3);
    VERIFY_IN(result, m.m3c2r, 3, 2);
    VERIFY_IN(result, m.inner.m3c4r, 3, 4);
    VERIFY_IN(result, m.inner.m4c3r, 4, 3);
    return result;
}

mat4 copyMat4(in mat4 m)
{
    return m;
}

void copyMatrices(in Matrices mIn, inout Matrices mOut)
{
    COPY(mIn.m2c2r, mOut.m2c2r, 2, 2);
    COPY(mIn.m2c3r[0], mOut.m2c3r[0], 2, 3);
    COPY(mIn.m2c3r[1], mOut.m2c3r[1], 2, 3);
    COPY(mIn.m3c2r, mOut.m3c2r, 3, 2);
    COPY(mIn.inner.m3c4r, mOut.inner.m3c4r, 3, 4);
    COPY(mIn.inner.m4c3r, mOut.inner.m4c3r, 4, 3);
}

void main()
{
    bool result = true;

    VERIFY_IN(result, ubo140cIn.m4c4r, 4, 4);
    VERIFY_IN(result, ubo140cIn.m.m2c3r[0], 2, 3);
    EXPECT(result, verifyMatrices(ubo140cIn.m), true);

    VERIFY_IN(result, ubo140rIn.m4c4r, 4, 4);
    VERIFY_IN(result, ubo140rIn.m.m2c2r, 2, 2);
    EXPECT(result, verifyMatrices(ubo140rIn.m), true);

    VERIFY_IN(result, ssbo140cIn.m4c4r, 4, 4);
    VERIFY_IN(result, ssbo140cIn.m.m3c2r, 3, 2);
    EXPECT(result, verifyMatrices(ssbo140cIn.m), true);

    VERIFY_IN(result, ssbo140rIn.m4c4r, 4, 4);
    VERIFY_IN(result, ssbo140rIn.m.inner.m4c3r, 4, 3);
    EXPECT(result, verifyMatrices(ssbo140rIn.m), true);

    VERIFY_IN(result, ssbo430cIn.m4c4r, 4, 4);
    VERIFY_IN(result, ssbo430cIn.m.m2c3r[1], 2, 3);
    EXPECT(result, verifyMatrices(ssbo430cIn.m), true);

    VERIFY_IN(result, ssbo430rIn.m4c4r, 4, 4);
    VERIFY_IN(result, ssbo430rIn.m.inner.m3c4r, 3, 4);
    EXPECT(result, verifyMatrices(ssbo430rIn.m), true);

    // Only assign to SSBO from a single invocation.
    if (gl_GlobalInvocationID.x == 0u)
    {
        ssbo140cOut.m4c4r = copyMat4(ssbo140cIn.m4c4r);
        copyMatrices(ssbo430cIn.m, ssbo140cOut.m);
        ssbo140cOut.m.m2c3r[1] = mat2x3(0);
        COPY(ssbo430cIn.m.m2c3r[1], ssbo140cOut.m.m2c3r[1], 2, 3);

        ssbo140rOut.m4c4r = copyMat4(ssbo140rIn.m4c4r);
        copyMatrices(ssbo430rIn.m, ssbo140rOut.m);
        ssbo140rOut.m.inner.m3c4r = mat3x4(0);
        COPY(ssbo430rIn.m.inner.m3c4r, ssbo140rOut.m.inner.m3c4r, 3, 4);

        ssbo430cOut.m4c4r = copyMat4(ssbo430cIn.m4c4r);
        copyMatrices(ssbo140cIn.m, ssbo430cOut.m);
        ssbo430cOut.m.m3c2r = mat3x2(0);
        COPY(ssbo430cIn.m.m3c2r, ssbo430cOut.m.m3c2r, 3, 2);

        ssbo430rOut.m4c4r = copyMat4(ssbo430rIn.m4c4r);
        copyMatrices(ssbo140rIn.m, ssbo430rOut.m);
        ssbo430rOut.m.inner.m4c3r = mat4x3(0);
        COPY(ssbo430rIn.m.inner.m4c3r, ssbo430rOut.m.inner.m4c3r, 4, 3);

        resultOut.success = uint(result);
    }
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);
    EXPECT_GL_NO_ERROR();

    constexpr size_t kMatrixCount                                     = 7;
    constexpr std::pair<uint32_t, uint32_t> kMatrixDims[kMatrixCount] = {
        {4, 4}, {2, 2}, {2, 3}, {2, 3}, {3, 2}, {3, 4}, {4, 3},
    };
    constexpr bool kMatrixIsColMajor[kMatrixCount] = {
        true, false, false, false, false, false, false,
    };

    float dataStd140ColMajor[kMatrixCount * 4 * 4] = {};
    float dataStd140RowMajor[kMatrixCount * 4 * 4] = {};
    float dataStd430ColMajor[kMatrixCount * 4 * 4] = {};
    float dataStd430RowMajor[kMatrixCount * 4 * 4] = {};
    float dataZeros[kMatrixCount * 4 * 4]          = {};

    const uint32_t sizeStd140ColMajor =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, dataStd140ColMajor, false, false);
    const uint32_t sizeStd140RowMajor =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, dataStd140RowMajor, false, true);
    const uint32_t sizeStd430ColMajor =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, dataStd430ColMajor, true, false);
    const uint32_t sizeStd430RowMajor =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, dataStd430RowMajor, true, true);

    GLBuffer uboStd140ColMajor, uboStd140RowMajor;
    GLBuffer ssboStd140ColMajor, ssboStd140RowMajor;
    GLBuffer ssboStd430ColMajor, ssboStd430RowMajor;
    GLBuffer ssboStd140ColMajorOut, ssboStd140RowMajorOut;
    GLBuffer ssboStd430ColMajorOut, ssboStd430RowMajorOut;

    InitBuffer(program, "Ubo140c", uboStd140ColMajor, 0, dataStd140ColMajor, sizeStd140ColMajor,
               true);
    InitBuffer(program, "Ubo140r", uboStd140RowMajor, 1, dataStd140RowMajor, sizeStd140RowMajor,
               true);
    InitBuffer(program, "Ssbo140c", ssboStd140ColMajor, 0, dataStd140ColMajor, sizeStd140ColMajor,
               false);
    InitBuffer(program, "Ssbo140r", ssboStd140RowMajor, 1, dataStd140RowMajor, sizeStd140RowMajor,
               false);
    InitBuffer(program, "Ssbo430c", ssboStd430ColMajor, 2, dataStd430ColMajor, sizeStd430ColMajor,
               false);
    InitBuffer(program, "Ssbo430r", ssboStd430RowMajor, 3, dataStd430RowMajor, sizeStd430RowMajor,
               false);
    InitBuffer(program, "Ssbo140cOut", ssboStd140ColMajorOut, 4, dataZeros, sizeStd140ColMajor,
               false);
    InitBuffer(program, "Ssbo140rOut", ssboStd140RowMajorOut, 5, dataZeros, sizeStd140RowMajor,
               false);
    InitBuffer(program, "Ssbo430cOut", ssboStd430ColMajorOut, 6, dataZeros, sizeStd430ColMajor,
               false);
    InitBuffer(program, "Ssbo430rOut", ssboStd430RowMajorOut, 7, dataZeros, sizeStd430RowMajor,
               false);
    EXPECT_GL_NO_ERROR();

    GLBuffer outputBuffer;
    CreateOutputBuffer(&outputBuffer, 8);

    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();
    EXPECT_TRUE(VerifySuccess(outputBuffer));

    EXPECT_TRUE(VerifyBuffer(ssboStd140ColMajorOut, dataStd140ColMajor, sizeStd140ColMajor));
    EXPECT_TRUE(VerifyBuffer(ssboStd140RowMajorOut, dataStd140RowMajor, sizeStd140RowMajor));
    EXPECT_TRUE(VerifyBuffer(ssboStd430ColMajorOut, dataStd430ColMajor, sizeStd430ColMajor));
    EXPECT_TRUE(VerifyBuffer(ssboStd430RowMajorOut, dataStd430RowMajor, sizeStd430RowMajor));
}

// Test that array UBOs are transformed correctly.
TEST_P(GLSLTest_ES3, MixedRowAndColumnMajorMatrices_ArrayBufferDeclaration)
{
    // Fails to compile the shader on Android: http://anglebug.com/3839
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGL());

    // http://anglebug.com/3837
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsOpenGL());

    // Fails on Mac on Intel and AMD: http://anglebug.com/3842
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL() && (IsIntel() || IsAMD()));

    // Fails on windows AMD on GL: http://anglebug.com/3838
    ANGLE_SKIP_TEST_IF(IsWindows() && IsOpenGL() && IsAMD());

    // Fails on D3D due to mistranslation: http://anglebug.com/3841
    ANGLE_SKIP_TEST_IF(IsD3D11());

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 outColor;

layout(std140, column_major) uniform Ubo
{
    mat4 m1;
    layout(row_major) mat4 m2;
} ubo[3];

#define EXPECT(result, expression, value) if ((expression) != value) { result = false; }

#define VERIFY_IN(result, mat, cols, rows)                  \
    for (int c = 0; c < cols; ++c)                          \
    {                                                       \
        for (int r = 0; r < rows; ++r)                      \
        {                                                   \
            EXPECT(result, mat[c][r], float(c * 4 + r));    \
        }                                                   \
    }

void main()
{
    bool result = true;

    VERIFY_IN(result, ubo[0].m1, 4, 4);
    VERIFY_IN(result, ubo[0].m2, 4, 4);

    VERIFY_IN(result, ubo[1].m1, 4, 4);
    VERIFY_IN(result, ubo[1].m2, 4, 4);

    VERIFY_IN(result, ubo[2].m1, 4, 4);
    VERIFY_IN(result, ubo[2].m2, 4, 4);

    outColor = result ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    EXPECT_GL_NO_ERROR();

    constexpr size_t kMatrixCount                                     = 2;
    constexpr std::pair<uint32_t, uint32_t> kMatrixDims[kMatrixCount] = {
        {4, 4},
        {4, 4},
    };
    constexpr bool kMatrixIsColMajor[kMatrixCount] = {
        true,
        false,
    };

    float data[kMatrixCount * 4 * 4] = {};

    const uint32_t size =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, data, false, false);

    GLBuffer ubos[3];

    InitBuffer(program, "Ubo[0]", ubos[0], 0, data, size, true);
    InitBuffer(program, "Ubo[1]", ubos[1], 0, data, size, true);
    InitBuffer(program, "Ubo[2]", ubos[2], 0, data, size, true);

    EXPECT_GL_NO_ERROR();

    drawQuad(program, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that side effects when transforming read operations are preserved.
TEST_P(GLSLTest_ES3, MixedRowAndColumnMajorMatrices_ReadSideEffect)
{
    // Fails on Mac on Intel and AMD: http://anglebug.com/3842
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL() && (IsIntel() || IsAMD()));

    // Fails on D3D due to mistranslation: http://anglebug.com/3841
    ANGLE_SKIP_TEST_IF(IsD3D11());

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 outColor;

struct S
{
    mat2x3 m2[3];
};

layout(std140, column_major) uniform Ubo
{
    mat4 m1;
    layout(row_major) S s[2];
} ubo;

#define EXPECT(result, expression, value) if ((expression) != value) { result = false; }

#define VERIFY_IN(result, mat, cols, rows)                  \
    for (int c = 0; c < cols; ++c)                          \
    {                                                       \
        for (int r = 0; r < rows; ++r)                      \
        {                                                   \
            EXPECT(result, mat[c][r], float(c * 4 + r));    \
        }                                                   \
    }

bool verify2x3(mat2x3 mat)
{
    bool result = true;

    for (int c = 0; c < 2; ++c)
    {
        for (int r = 0; r < 3; ++r)
        {
            EXPECT(result, mat[c][r], float(c * 4 + r));
        }
    }

    return result;
}

void main()
{
    bool result = true;

    int sideEffect = 0;
    VERIFY_IN(result, ubo.m1, 4, 4);
    EXPECT(result, verify2x3(ubo.s[0].m2[0]), true);
    EXPECT(result, verify2x3(ubo.s[0].m2[sideEffect += 1]), true);
    EXPECT(result, verify2x3(ubo.s[0].m2[sideEffect += 1]), true);

    EXPECT(result, sideEffect, 2);

    EXPECT(result, verify2x3(ubo.s[sideEffect = 1].m2[0]), true);
    EXPECT(result, verify2x3(ubo.s[1].m2[(sideEffect = 4) - 3]), true);
    EXPECT(result, verify2x3(ubo.s[1].m2[sideEffect - 2]), true);

    EXPECT(result, sideEffect, 4);

    outColor = result ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    EXPECT_GL_NO_ERROR();

    constexpr size_t kMatrixCount                                     = 7;
    constexpr std::pair<uint32_t, uint32_t> kMatrixDims[kMatrixCount] = {
        {4, 4}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
    };
    constexpr bool kMatrixIsColMajor[kMatrixCount] = {
        true, false, false, false, false, false, false,
    };

    float data[kMatrixCount * 4 * 4] = {};

    const uint32_t size =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, data, false, false);

    GLBuffer ubo;
    InitBuffer(program, "Ubo", ubo, 0, data, size, true);

    EXPECT_GL_NO_ERROR();

    drawQuad(program, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that side effects respect the order of logical expression operands.
TEST_P(GLSLTest_ES3, MixedRowAndColumnMajorMatrices_ReadSideEffectOrder)
{
    // http://anglebug.com/3837
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsOpenGL());

    // IntermTraverser::insertStatementsInParentBlock that's used to move side effects does not
    // respect the order of evaluation of logical expressions.  http://anglebug.com/3829.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL());

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 outColor;

layout(std140, column_major) uniform Ubo
{
    mat4 m1;
    layout(row_major) mat4 m2[2];
} ubo;

void main()
{
    bool result = true;

    int x = 0;
    if (x == 0 && ubo.m2[x = 1][1][1] == 5.0)
    {
        result = true;
    }
    else
    {
        result = false;
    }

    outColor = result ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    EXPECT_GL_NO_ERROR();

    constexpr size_t kMatrixCount                                     = 3;
    constexpr std::pair<uint32_t, uint32_t> kMatrixDims[kMatrixCount] = {
        {4, 4},
        {4, 4},
        {4, 4},
    };
    constexpr bool kMatrixIsColMajor[kMatrixCount] = {true, false, false};

    float data[kMatrixCount * 4 * 4] = {};

    const uint32_t size =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, data, false, false);

    GLBuffer ubo;
    InitBuffer(program, "Ubo", ubo, 0, data, size, true);

    EXPECT_GL_NO_ERROR();

    drawQuad(program, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that side effects respect short-circuit.
TEST_P(GLSLTest_ES3, MixedRowAndColumnMajorMatrices_ReadSideEffectShortCircuit)
{
    // Fails on Android: http://anglebug.com/3839
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGL());

    // IntermTraverser::insertStatementsInParentBlock that's used to move side effects does not
    // respect the order of evaluation of logical expressions.  http://anglebug.com/3829.
    ANGLE_SKIP_TEST_IF(IsOSX() && IsOpenGL());

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 outColor;

layout(std140, column_major) uniform Ubo
{
    mat4 m1;
    layout(row_major) mat4 m2[2];
} ubo;

void main()
{
    bool result = true;

    int x = 0;
    if (x == 1 && ubo.m2[x = 1][1][1] == 5.0)
    {
        // First x == 1 should prevent the side effect of the second expression (x = 1) from
        // being executed.  If x = 1 is run before the if, the condition of the if would be true,
        // which is a failure.
        result = false;
    }
    if (x == 1)
    {
        result = false;
    }

    outColor = result ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    EXPECT_GL_NO_ERROR();

    constexpr size_t kMatrixCount                                     = 3;
    constexpr std::pair<uint32_t, uint32_t> kMatrixDims[kMatrixCount] = {
        {4, 4},
        {4, 4},
        {4, 4},
    };
    constexpr bool kMatrixIsColMajor[kMatrixCount] = {true, false, false};

    float data[kMatrixCount * 4 * 4] = {};

    const uint32_t size =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, data, false, false);

    GLBuffer ubo;
    InitBuffer(program, "Ubo", ubo, 0, data, size, true);

    EXPECT_GL_NO_ERROR();

    drawQuad(program, essl31_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that dynamic indexing of swizzled l-values should work.
// A simple porting of sdk/tests/conformance2/glsl3/vector-dynamic-indexing-swizzled-lvalue.html
TEST_P(GLSLTest_ES3, DynamicIndexingOfSwizzledLValuesShouldWork)
{
    // The shader first assigns v.x to v.z (1.0)
    // Then v.y to v.y (2.0)
    // Then v.z to v.x (1.0)
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
out vec4 my_FragColor;
void main() {
    vec3 v = vec3(1.0, 2.0, 3.0);
    for (int i = 0; i < 3; i++) {
        v.zyx[i] = v[i];
    }
    my_FragColor = distance(v, vec3(1.0, 2.0, 1.0)) < 0.01 ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    EXPECT_GL_NO_ERROR();
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test shader with all resources (default uniform, UBO, SSBO, image, sampler and atomic counter) to
// make sure they are all linked ok.  The front-end sorts these resources and traverses the list of
// "uniforms" to find the range for each resource.  A bug there was causing some resource ranges to
// be empty in the presence of other resources.
TEST_P(GLSLTest_ES31, MixOfAllResources)
{
    constexpr char kComputeShader[] = R"(#version 310 es
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(binding = 1, std430) buffer Output {
  uint ubo_value;
  uint default_value;
  uint sampler_value;
  uint ac_value;
  uint image_value;
} outbuf;
uniform Input {
  uint input_value;
} inbuf;
uniform uint default_uniform;
uniform sampler2D smplr;
layout(binding=0) uniform atomic_uint ac;
layout(r32ui) uniform highp readonly uimage2D image;

void main(void)
{
  outbuf.ubo_value = inbuf.input_value;
  outbuf.default_value = default_uniform;
  outbuf.sampler_value = uint(texture(smplr, vec2(0.5, 0.5)).x * 255.0);
  outbuf.ac_value = atomicCounterIncrement(ac);
  outbuf.image_value = imageLoad(image, ivec2(0, 0)).x;
}
)";
    ANGLE_GL_COMPUTE_PROGRAM(program, kComputeShader);
    EXPECT_GL_NO_ERROR();

    glUseProgram(program);

    unsigned int inputData = 89u;
    GLBuffer inputBuffer;
    glBindBuffer(GL_UNIFORM_BUFFER, inputBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(inputData), &inputData, GL_STATIC_DRAW);
    GLuint inputBufferIndex = glGetUniformBlockIndex(program.get(), "Input");
    ASSERT_NE(inputBufferIndex, GL_INVALID_INDEX);
    glUniformBlockBinding(program.get(), inputBufferIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, inputBuffer);

    unsigned int outputInitData[5] = {0x12345678u, 0x09ABCDEFu, 0x56789ABCu, 0x0DEF1234u,
                                      0x13579BDFu};
    GLBuffer outputBuffer;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(outputInitData), outputInitData, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputBuffer);
    EXPECT_GL_NO_ERROR();

    unsigned int uniformData = 456u;
    GLint uniformLocation    = glGetUniformLocation(program, "default_uniform");
    ASSERT_NE(uniformLocation, -1);
    glUniform1ui(uniformLocation, uniformData);

    unsigned int acData = 2u;
    GLBuffer atomicCounterBuffer;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(acData), &acData, GL_STATIC_DRAW);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer);
    EXPECT_GL_NO_ERROR();

    unsigned int imageData = 33u;
    GLTexture image;
    glBindTexture(GL_TEXTURE_2D, image);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 1, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &imageData);
    glBindImageTexture(0, image, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    EXPECT_GL_NO_ERROR();

    GLColor textureData(127, 18, 189, 211);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &textureData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    // read back
    const GLuint *ptr = reinterpret_cast<const GLuint *>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(outputInitData), GL_MAP_READ_BIT));
    EXPECT_EQ(ptr[0], inputData);
    EXPECT_EQ(ptr[1], uniformData);
    EXPECT_EQ(ptr[2], textureData.R);
    EXPECT_EQ(ptr[3], acData);
    EXPECT_EQ(ptr[4], imageData);

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

// Test that multiple nested assignments are handled correctly.
TEST_P(GLSLTest_ES31, MixedRowAndColumnMajorMatrices_WriteSideEffect)
{
    // http://anglebug.com/3831
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsOpenGL());

    // Fails on windows AMD on GL: http://anglebug.com/3838
    ANGLE_SKIP_TEST_IF(IsWindows() && IsOpenGL() && IsAMD());

    // Fails on D3D due to mistranslation: http://anglebug.com/3841
    ANGLE_SKIP_TEST_IF(IsD3D11());

    constexpr char kCS[] = R"(#version 310 es
precision highp float;
layout(local_size_x=1) in;

layout(std140, column_major) uniform Ubo
{
    mat4 m1;
    layout(row_major) mat4 m2;
} ubo;

layout(std140, row_major, binding = 0) buffer Ssbo
{
    layout(column_major) mat4 m1;
    mat4 m2;
} ssbo;

layout(std140, binding = 1) buffer Result
{
    uint success;
} resultOut;

void main()
{
    bool result = true;

    // Only assign to SSBO from a single invocation.
    if (gl_GlobalInvocationID.x == 0u)
    {
        if ((ssbo.m2 = ssbo.m1 = ubo.m1) != ubo.m2)
        {
            result = false;
        }

        resultOut.success = uint(result);
    }
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);
    EXPECT_GL_NO_ERROR();

    constexpr size_t kMatrixCount                                     = 2;
    constexpr std::pair<uint32_t, uint32_t> kMatrixDims[kMatrixCount] = {
        {4, 4},
        {4, 4},
    };
    constexpr bool kMatrixIsColMajor[kMatrixCount] = {
        true,
        false,
    };

    float data[kMatrixCount * 4 * 4]  = {};
    float zeros[kMatrixCount * 4 * 4] = {};

    const uint32_t size =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, data, false, false);

    GLBuffer ubo, ssbo;

    InitBuffer(program, "Ubo", ubo, 0, data, size, true);
    InitBuffer(program, "Ssbo", ssbo, 0, zeros, size, false);
    EXPECT_GL_NO_ERROR();

    GLBuffer outputBuffer;
    CreateOutputBuffer(&outputBuffer, 1);

    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();
    EXPECT_TRUE(VerifySuccess(outputBuffer));

    EXPECT_TRUE(VerifyBuffer(ssbo, data, size));
}

// Test that assignments to array of array of matrices are handled correctly.
TEST_P(GLSLTest_ES31, MixedRowAndColumnMajorMatrices_WriteArrayOfArray)
{
    // Fails on windows AMD on GL: http://anglebug.com/3838
    ANGLE_SKIP_TEST_IF(IsWindows() && IsOpenGL() && IsAMD());

    // Fails on D3D due to mistranslation: http://anglebug.com/3841
    ANGLE_SKIP_TEST_IF(IsD3D11());

    // Fails compiling shader on Android/Vulkan.  http://anglebug.com/4290
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kCS[] = R"(#version 310 es
precision highp float;
layout(local_size_x=1) in;

layout(std140, column_major) uniform Ubo
{
    mat4 m1;
    layout(row_major) mat4 m2[2][3];
} ubo;

layout(std140, row_major, binding = 0) buffer Ssbo
{
    layout(column_major) mat4 m1;
    mat4 m2[2][3];
} ssbo;

layout(std140, binding = 1) buffer Result
{
    uint success;
} resultOut;

void main()
{
    bool result = true;

    // Only assign to SSBO from a single invocation.
    if (gl_GlobalInvocationID.x == 0u)
    {
        ssbo.m1 = ubo.m1;
        ssbo.m2 = ubo.m2;

        resultOut.success = uint(result);
    }
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);
    EXPECT_GL_NO_ERROR();

    constexpr size_t kMatrixCount                                     = 7;
    constexpr std::pair<uint32_t, uint32_t> kMatrixDims[kMatrixCount] = {
        {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4},
    };
    constexpr bool kMatrixIsColMajor[kMatrixCount] = {
        true, false, false, false, false, false, false,
    };

    float data[kMatrixCount * 4 * 4]  = {};
    float zeros[kMatrixCount * 4 * 4] = {};

    const uint32_t size =
        FillBuffer(kMatrixDims, kMatrixIsColMajor, kMatrixCount, data, false, false);

    GLBuffer ubo, ssbo;

    InitBuffer(program, "Ubo", ubo, 0, data, size, true);
    InitBuffer(program, "Ssbo", ssbo, 0, zeros, size, false);
    EXPECT_GL_NO_ERROR();

    GLBuffer outputBuffer;
    CreateOutputBuffer(&outputBuffer, 1);

    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();
    EXPECT_TRUE(VerifySuccess(outputBuffer));

    EXPECT_TRUE(VerifyBuffer(ssbo, data, size));
}

// Test that the precise keyword is not reserved before ES3.1.
TEST_P(GLSLTest_ES3, PreciseNotReserved)
{
    // Skip in ES3.1+ as the precise keyword is reserved/core.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() > 3 ||
                       (getClientMajorVersion() == 3 && getClientMinorVersion() >= 1));

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in float precise;
out vec4 my_FragColor;
void main() { my_FragColor = vec4(precise, 0, 0, 1.0); })";

    constexpr char kVS[] = R"(#version 300 es
in vec4 a_position;
out float precise;
void main() { precise = a_position.x; gl_Position = a_position; })";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_NE(0u, program);
}

// Test that the precise keyword is reserved on ES3.0 without GL_EXT_gpu_shader5.
TEST_P(GLSLTest_ES31, PreciseReservedWithoutExtension)
{
    // Skip if EXT_gpu_shader5 is enabled.
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_EXT_gpu_shader5"));
    // Skip in ES3.2+ as the precise keyword is core.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() > 3 ||
                       (getClientMajorVersion() == 3 && getClientMinorVersion() >= 2));

    constexpr char kFS[] = R"(#version 310 es
precision mediump float;
in float v_varying;
out vec4 my_FragColor;
void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); })";

    constexpr char kVS[] = R"(#version 310 es
in vec4 a_position;
precise out float v_varying;
void main() { v_varying = a_position.x; gl_Position = a_position; })";

    // Should fail, as precise is a reserved keyword when the extension is not enabled.
    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

}  // anonymous namespace

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(GLSLTest);

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(GLSLTestNoValidation);

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES3(GLSLTest_ES3);

ANGLE_INSTANTIATE_TEST_ES2(WebGLGLSLTest);

ANGLE_INSTANTIATE_TEST_ES31(GLSLTest_ES31);
