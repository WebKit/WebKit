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

    virtual void SetUp()
    {
        ANGLETest::SetUp();

        mSimpleVSSource =
            R"(attribute vec4 inputAttribute;
            void main()
            {
                gl_Position = inputAttribute;
            })";
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
            sprintf(buff, "varying %s v%d[%d];\n", GenerateVaryingType(vectorSize).c_str(), id, arraySize);
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
                sprintf(buff, "\t v%d[%d] = %s(1.0);\n", id, i, GenerateVaryingType(vectorSize).c_str());
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

    void GenerateGLSLWithVaryings(GLint floatCount, GLint floatArrayCount, GLint vec2Count, GLint vec2ArrayCount, GLint vec3Count, GLint vec3ArrayCount,
                                  GLint vec4Count, GLint vec4ArrayCount, bool useFragCoord, bool usePointCoord, bool usePointSize,
                                  std::string* fragmentShader, std::string* vertexShader)
    {
        // Generate a string declaring the varyings, to share between the fragment shader and the vertex shader.
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

    void VaryingTestBase(GLint floatCount, GLint floatArrayCount, GLint vec2Count, GLint vec2ArrayCount, GLint vec3Count, GLint vec3ArrayCount,
                         GLint vec4Count, GLint vec4ArrayCount, bool useFragCoord, bool usePointCoord, bool usePointSize, bool expectSuccess)
    {
        std::string fragmentShaderSource;
        std::string vertexShaderSource;

        GenerateGLSLWithVaryings(floatCount, floatArrayCount, vec2Count, vec2ArrayCount, vec3Count, vec3ArrayCount,
                                 vec4Count, vec4ArrayCount, useFragCoord, usePointCoord, usePointSize,
                                 &fragmentShaderSource, &vertexShaderSource);

        GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);

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

        GLuint program = CompileProgram(vertexShader.str(), fragmentShader.str());

        if (expectSuccess)
        {
            EXPECT_NE(0u, program);
        }
        else
        {
            EXPECT_EQ(0u, program);
        }
    }

    std::string mSimpleVSSource;
};

class GLSLTestNoValidation : public GLSLTest
{
  public:
    GLSLTestNoValidation() { setNoErrorEnabled(true); }
};

class GLSLTest_ES3 : public GLSLTest
{
    void SetUp() override
    {
        ANGLETest::SetUp();

        mSimpleVSSource =
            R"(#version 300 es
            in vec4 inputAttribute;
            void main()
            {
                gl_Position = inputAttribute;
            })";
    }
};

class GLSLTest_ES31 : public GLSLTest
{
    void SetUp() override
    {
        ANGLETest::SetUp();

        mSimpleVSSource =
            R"(#version 310 es
            in vec4 inputAttribute;
            void main()
            {
                gl_Position = inputAttribute;
            })";
    }
};

TEST_P(GLSLTest, NamelessScopedStructs)
{
    const std::string fragmentShaderSource =
        R"(precision mediump float;
        void main()
        {
            struct
            {
                float q;
            } b;

            gl_FragColor = vec4(1, 0, 0, 1);
            gl_FragColor.a += b.q;
        })";

    GLuint program = CompileProgram(mSimpleVSSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

TEST_P(GLSLTest, ScopedStructsOrderBug)
{
    // TODO(geofflang): Find out why this doesn't compile on Apple OpenGL drivers
    // (http://anglebug.com/1292)
    // TODO(geofflang): Find out why this doesn't compile on AMD OpenGL drivers
    // (http://anglebug.com/1291)
    if (IsDesktopOpenGL() && (IsOSX() || !IsNVIDIA()))
    {
        std::cout << "Test disabled on this OpenGL configuration." << std::endl;
        return;
    }

    const std::string fragmentShaderSource =
        R"(precision mediump float;

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

    GLuint program = CompileProgram(mSimpleVSSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

TEST_P(GLSLTest, ScopedStructsBug)
{
    const std::string fragmentShaderSource =
        R"(precision mediump float;

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

    GLuint program = CompileProgram(mSimpleVSSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

TEST_P(GLSLTest, DxPositionBug)
{
    const std::string &vertexShaderSource =
        R"(attribute vec4 inputAttribute;
        varying float dx_Position;
        void main()
        {
            gl_Position = vec4(inputAttribute);
            dx_Position = 0.0;
        })";

    const std::string &fragmentShaderSource =
        R"(precision mediump float;

        varying float dx_Position;

        void main()
        {
            gl_FragColor = vec4(dx_Position, 0, 0, 1);
        })";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

TEST_P(GLSLTest, ElseIfRewriting)
{
    const std::string &vertexShaderSource =
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

    const std::string &fragmentShaderSource =
        "precision highp float;\n"
        "varying float v;\n"
        "void main() {\n"
        "  vec4 color = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "  if (v >= 1.0) color = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "  if (v >= 2.0) color = vec4(0.0, 0.0, 1.0, 1.0);\n"
        "  gl_FragColor = color;\n"
        "}\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    ASSERT_NE(0u, program);

    drawQuad(program, "a_position", 0.5f);

    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth()-1, 0, 0, 255, 0, 255);
}

TEST_P(GLSLTest, TwoElseIfRewriting)
{
    const std::string &vertexShaderSource =
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

    const std::string &fragmentShaderSource =
        "precision highp float;\n"
        "varying float v;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(v, 0.0, 0.0, 1.0);\n"
        "}\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

TEST_P(GLSLTest, FrontFacingAndVarying)
{
    EGLPlatformParameters platform = GetParam().eglParameters;

    const std::string vertexShaderSource =
        R"(attribute vec4 a_position;
        varying float v_varying;
        void main()
        {
            v_varying = a_position.x;
            gl_Position = a_position;
        })";

    const std::string fragmentShaderSource =
        R"(precision mediump float;
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

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);

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
    const std::string &simpleVS =
        "attribute vec4 position; void main() { gl_Position = position; }";
    const std::string &simpleFS = "void main() { gl_FragColor = vec4(1, 0, 0, 1); }";

    // Draw with the first program.
    ANGLE_GL_PROGRAM(program1, simpleVS, simpleFS);
    drawQuad(program1, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Clear and release shader compiler.
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    glReleaseShaderCompiler();
    ASSERT_GL_NO_ERROR();

    // Draw with a second program.
    ANGLE_GL_PROGRAM(program2, simpleVS, simpleFS);
    drawQuad(program2, "position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Verify that linking shaders declaring different shading language versions fails.
TEST_P(GLSLTest_ES3, VersionMismatch)
{
    const std::string fragmentShaderSource100 =
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource100 =
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    const std::string fragmentShaderSource300 =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource300 =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource300, fragmentShaderSource100);
    EXPECT_EQ(0u, program);

    program = CompileProgram(vertexShaderSource100, fragmentShaderSource300);
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant only in vertex shader fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantVaryingOut)
{
    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "attribute vec4 a_position;\n"
        "invariant varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant only in vertex shader succeeds in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantVaryingOut)
{
    // TODO: ESSL 3.00 -> GLSL 1.20 translation should add "invariant" in fragment shader
    // for varyings which are invariant in vertex shader (http://anglebug.com/1293)
    if (IsDesktopOpenGL())
    {
        std::cout << "Test disabled on OpenGL." << std::endl;
        return;
    }

    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "invariant out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that declaring varying as invariant only in fragment shader fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantVaryingIn)
{
    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "invariant varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant only in fragment shader fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantVaryingIn)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "invariant in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that declaring varying as invariant in both shaders succeeds in ESSL 1.00.
TEST_P(GLSLTest, InvariantVaryingBoth)
{
    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "invariant varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "attribute vec4 a_position;\n"
        "invariant varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that declaring varying as invariant in both shaders fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantVaryingBoth)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "invariant in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "invariant out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that declaring gl_Position as invariant succeeds in ESSL 1.00.
TEST_P(GLSLTest, InvariantGLPosition)
{
    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "attribute vec4 a_position;\n"
        "invariant gl_Position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that declaring gl_Position as invariant succeeds in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantGLPosition)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "invariant gl_Position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that using invariant(all) in both shaders succeeds in ESSL 1.00.
TEST_P(GLSLTest, InvariantAllBoth)
{
    // TODO: ESSL 1.00 -> GLSL 1.20 translation should add "invariant" in fragment shader
    // for varyings which are invariant in vertex shader individually,
    // and remove invariant(all) from fragment shader (http://anglebug.com/1293)
    if (IsDesktopOpenGL())
    {
        std::cout << "Test disabled on OpenGL." << std::endl;
        return;
    }

    const std::string fragmentShaderSource =
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#pragma STDGL invariant(all)\n"
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnFloat)
{
    const std::string vertexShaderSource =
        "varying float v_varying;\n"
        "float f() { if (v_varying > 0.0) return 1.0; }\n"
        "void main() { gl_Position = vec4(f(), 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnVec2)
{
    const std::string vertexShaderSource =
        "varying float v_varying;\n"
        "vec2 f() { if (v_varying > 0.0) return vec2(1.0, 1.0); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnVec3)
{
    const std::string vertexShaderSource =
        "varying float v_varying;\n"
        "vec3 f() { if (v_varying > 0.0) return vec3(1.0, 1.0, 1.0); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnVec4)
{
    const std::string vertexShaderSource =
        "varying float v_varying;\n"
        "vec4 f() { if (v_varying > 0.0) return vec4(1.0, 1.0, 1.0, 1.0); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnIVec4)
{
    const std::string vertexShaderSource =
        "varying float v_varying;\n"
        "ivec4 f() { if (v_varying > 0.0) return ivec4(1, 1, 1, 1); }\n"
        "void main() { gl_Position = vec4(f().x, 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnMat4)
{
    const std::string vertexShaderSource =
        "varying float v_varying;\n"
        "mat4 f() { if (v_varying > 0.0) return mat4(1.0); }\n"
        "void main() { gl_Position = vec4(f()[0][0], 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest, MissingReturnStruct)
{
    const std::string vertexShaderSource =
        "varying float v_varying;\n"
        "struct s { float a; int b; vec2 c; };\n"
        "s f() { if (v_varying > 0.0) return s(1.0, 1, vec2(1.0, 1.0)); }\n"
        "void main() { gl_Position = vec4(f().a, 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest_ES3, MissingReturnArray)
{
    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in float v_varying;\n"
        "vec2[2] f() { if (v_varying > 0.0) { return vec2[2](vec2(1.0, 1.0), vec2(1.0, 1.0)); } }\n"
        "void main() { gl_Position = vec4(f()[0].x, 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest_ES3, MissingReturnArrayOfStructs)
{
    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in float v_varying;\n"
        "struct s { float a; int b; vec2 c; };\n"
        "s[2] f() { if (v_varying > 0.0) { return s[2](s(1.0, 1, vec2(1.0, 1.0)), s(1.0, 1, "
        "vec2(1.0, 1.0))); } }\n"
        "void main() { gl_Position = vec4(f()[0].a, 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that functions without return statements still compile
TEST_P(GLSLTest_ES3, MissingReturnStructOfArrays)
{
    // TODO(cwallez) remove the suppression once NVIDIA drivers are updated across trybots, drivers
    // since late 2016 should have the fix. Last check on 2017-05-30 revealed that the Windows
    // Server 2008 bots still had the old, failing drivers.
    if (IsNVIDIA() && IsOpenGLES())
    {
        std::cout << "Test skipped on NVIDIA OpenGL ES because it disallows returning "
                     "structure of arrays"
                  << std::endl;
        return;
    }

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in float v_varying;\n"
        "struct s { float a[2]; int b[2]; vec2 c[2]; };\n"
        "s f() { if (v_varying > 0.0) { return s(float[2](1.0, 1.0), int[2](1, 1),"
        "vec2[2](vec2(1.0, 1.0), vec2(1.0, 1.0))); } }\n"
        "void main() { gl_Position = vec4(f().a[0], 0, 0, 1); }\n";

    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(0, 0, 0, 1); }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Verify that using invariant(all) in both shaders fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantAllBoth)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that using invariant(all) only in fragment shader fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantAllIn)
{
    const std::string fragmentShaderSource =
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that using invariant(all) only in fragment shader fails in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantAllIn)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that using invariant(all) only in vertex shader fails in ESSL 1.00.
TEST_P(GLSLTest, InvariantAllOut)
{
    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "varying float v_varying;\n"
        "void main() { gl_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#pragma STDGL invariant(all)\n"
        "attribute vec4 a_position;\n"
        "varying float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_EQ(0u, program);
}

// Verify that using invariant(all) only in vertex shader succeeds in ESSL 3.00.
TEST_P(GLSLTest_ES3, InvariantAllOut)
{
    // TODO: ESSL 3.00 -> GLSL 1.20 translation should add "invariant" in fragment shader
    // for varyings which are invariant in vertex shader,
    // because of invariant(all) being used in vertex shader (http://anglebug.com/1293)
    if (IsDesktopOpenGL())
    {
        std::cout << "Test disabled on OpenGL." << std::endl;
        return;
    }

    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in float v_varying;\n"
        "out vec4 my_FragColor;\n"
        "void main() { my_FragColor = vec4(v_varying, 0, 0, 1.0); }\n";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "#pragma STDGL invariant(all)\n"
        "in vec4 a_position;\n"
        "out float v_varying;\n"
        "void main() { v_varying = a_position.x; gl_Position = a_position; }\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

TEST_P(GLSLTest, MaxVaryingVec4)
{
#if defined(__APPLE__)
    // TODO(geofflang): Find out why this doesn't compile on Apple AND OpenGL drivers
    // (http://anglebug.com/1291)
    if (IsAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test disabled on Apple AMD OpenGL." << std::endl;
        return;
    }
#endif

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings, 0, false, false, false, true);
}

TEST_P(GLSLTest, MaxMinusTwoVaryingVec4PlusTwoSpecialVariables)
{
    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord and gl_PointCoord, two special fragment shader variables.
    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings - 2, 0, true, true, false, true);
}

TEST_P(GLSLTest, MaxMinusTwoVaryingVec4PlusThreeSpecialVariables)
{
    // TODO(geofflang): Figure out why this fails on OpenGL AMD (http://anglebug.com/1291)
    if (IsAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test disabled on OpenGL." << std::endl;
        return;
    }

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Generate shader code that uses gl_FragCoord, gl_PointCoord and gl_PointSize.
    VaryingTestBase(0, 0, 0, 0, 0, 0, maxVaryings - 2, 0, true, true, true, true);
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

// Disabled because of a failure in D3D9
TEST_P(GLSLTest, MaxVaryingVec3AndOneFloat)
{
    if (IsD3D9())
    {
        std::cout << "Test disabled on D3D9." << std::endl;
        return;
    }

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(1, 0, 0, 0, maxVaryings, 0, 0, 0, false, false, false, true);
}

// Disabled because of a failure in D3D9
TEST_P(GLSLTest, MaxVaryingVec3ArrayAndOneFloatArray)
{
    if (IsD3D9())
    {
        std::cout << "Test disabled on D3D9." << std::endl;
        return;
    }

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 1, 0, 0, 0, maxVaryings / 2, 0, 0, false, false, false, true);
}

// Disabled because of a failure in D3D9
TEST_P(GLSLTest, TwiceMaxVaryingVec2)
{
    if (IsD3D9())
    {
        std::cout << "Test disabled on D3D9." << std::endl;
        return;
    }

    if (getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
    {
        // TODO(geofflang): Figure out why this fails on NVIDIA's GLES driver
        std::cout << "Test disabled on OpenGL ES." << std::endl;
        return;
    }

#if defined(__APPLE__)
    // TODO(geofflang): Find out why this doesn't compile on Apple AND OpenGL drivers
    // (http://anglebug.com/1291)
    if (IsAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test disabled on Apple AMD OpenGL." << std::endl;
        return;
    }
#endif

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    VaryingTestBase(0, 0, 2 * maxVaryings, 0, 0, 0, 0, 0, false, false, false, true);
}

// Disabled because of a failure in D3D9
TEST_P(GLSLTest, MaxVaryingVec2Arrays)
{
    if (IsD3DSM3())
    {
        std::cout << "Test disabled on SM3." << std::endl;
        return;
    }

    if (getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
    {
        // TODO(geofflang): Figure out why this fails on NVIDIA's GLES driver
        std::cout << "Test disabled on OpenGL ES." << std::endl;
        return;
    }

#if defined(__APPLE__)
    // TODO(geofflang): Find out why this doesn't compile on Apple AND OpenGL drivers
    // (http://anglebug.com/1291)
    if (IsAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test disabled on Apple AMD OpenGL." << std::endl;
        return;
    }
#endif

    GLint maxVaryings = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryings);

    // Special case: because arrays of mat2 are packed as small grids of two rows by two columns,
    // we should be aware that when we're packing into an odd number of varying registers the
    // last row will be empty and can not fit the final vec2 arrary.
    GLint maxVec2Arrays = (maxVaryings >> 1) << 1;

    VaryingTestBase(0, 0, 0, maxVec2Arrays, 0, 0, 0, 0, false, false, false, true);
}

// Verify shader source with a fixed length that is less than the null-terminated length will compile.
TEST_P(GLSLTest, FixedShaderLength)
{
    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);

    const std::string appendGarbage = "abcasdfasdfasdfasdfasdf";
    const std::string source = "void main() { gl_FragColor = vec4(0, 0, 0, 0); }" + appendGarbage;
    const char *sourceArray[1] = { source.c_str() };
    GLint lengths[1] = { static_cast<GLint>(source.length() - appendGarbage.length()) };
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

    const char *sourceArray[1] = { "void main() { gl_FragColor = vec4(0, 0, 0, 0); }" };
    GLint lengths[1] = { -10 };
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

    const char *sourceArray[] =
    {
        "void main()",
        "{",
        "    gl_FragColor = vec4(0, 0, 0, 0);",
        "}",
    };
    GLint lengths[] =
    {
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

    const char *sourceArray[] =
    {
        "adfasdf",
        "34534",
        "void main() { gl_FragColor = vec4(0, 0, 0, 0); }",
        "",
        "asdfasdfsdsdf",
    };
    GLint lengths[] =
    {
        0,
        0,
        -1,
        0,
        0,
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
    const std::string &fragmentShaderSourceVec =
        "precision mediump float;\n"
        "uniform vec4 uniformVec;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(uniformVec[int()]);\n"
        "}";

    GLuint shader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSourceVec);
    EXPECT_EQ(0u, shader);

    if (shader != 0)
    {
        glDeleteShader(shader);
    }

    const std::string &fragmentShaderSourceMat =
        "precision mediump float;\n"
        "uniform mat4 uniformMat;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(uniformMat[int()]);\n"
        "}";

    shader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSourceMat);
    EXPECT_EQ(0u, shader);

    if (shader != 0)
    {
        glDeleteShader(shader);
    }

    const std::string &fragmentShaderSourceArray =
        "precision mediump float;\n"
        "uniform vec4 uniformArray;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(uniformArray[int()]);\n"
        "}";

    shader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSourceArray);
    EXPECT_EQ(0u, shader);

    if (shader != 0)
    {
        glDeleteShader(shader);
    }
}

// Test that structs defined in uniforms are translated correctly.
TEST_P(GLSLTest, StructSpecifiersUniforms)
{
    const std::string fragmentShaderSource =
        R"(precision mediump float;

        uniform struct S { float field;} s;

        void main()
        {
            gl_FragColor = vec4(1, 0, 0, 1);
            gl_FragColor.a += s.field;
        })";

    GLuint program = CompileProgram(mSimpleVSSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Test that gl_DepthRange is not stored as a uniform location. Since uniforms
// beginning with "gl_" are filtered out by our validation logic, we must
// bypass the validation to test the behaviour of the implementation.
// (note this test is still Impl-independent)
TEST_P(GLSLTestNoValidation, DepthRangeUniforms)
{
    const std::string fragmentShaderSource =
        R"(precision mediump float;

        void main()
        {
            gl_FragColor = vec4(gl_DepthRange.near, gl_DepthRange.far, gl_DepthRange.diff, 1);
        })";

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShaderSource);

    // We need to bypass validation for this call.
    GLint nearIndex = glGetUniformLocation(program.get(), "gl_DepthRange.near");
    EXPECT_EQ(-1, nearIndex);

    // Test drawing does not throw an exception.
    drawQuad(program.get(), "inputAttribute", 0.5f);

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
    std::vector<double> bads;
    for (int eps = -1; eps <= 1; ++eps)
    {
        for (int i = -4; i <= 5; ++i)
        {
            if (i >= -1 && i <= 1)
                continue;
            const double epsilon = 1.0e-8;
            double bad           = static_cast<double>(i) + static_cast<double>(eps) * epsilon;
            bads.push_back(bad);
        }
    }

    for (double bad : bads)
    {
        const std::string &fragmentShaderSource = GenerateSmallPowShader(1.0e-6, bad);

        ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShaderSource);

        drawQuad(program.get(), "inputAttribute", 0.5f);

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
    const std::string fragmentShaderSource =
        R"(precision mediump float;

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

    const char *sourceArray[1] = {fragmentShaderSource.c_str()};
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
    if (IsLinux() && IsIntel())
    {
        std::cout << "Test timed out on Linux Intel. See crbug.com/680631." << std::endl;
        return;
    }

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
    if (GetParam().eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE ||
        GetParam().eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
    {
        std::cout << "Test disabled on OpenGL." << std::endl;
        return;
    }

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
    if (IsLinux() && IsIntel())
    {
        std::cout << "Test timed out on Linux Intel. See crbug.com/680631." << std::endl;
        return;
    }

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
    if (GetParam().eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE ||
        GetParam().eglParameters.renderer == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
    {
        std::cout << "Test disabled on OpenGL." << std::endl;
        return;
    }

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
    if (!extensionEnabled("GL_EXT_shader_texture_lod"))
    {
        std::cout << "Test skipped due to missing GL_EXT_shader_texture_lod." << std::endl;
        return;
    }

    const std::string source =
        "#extension GL_EXT_shader_texture_lod : require\n"
        "uniform sampler2D u_texture;\n"
        "void main() {\n"
        "    gl_FragColor = texture2DGradEXT(u_texture, vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, "
        "0.0));\n"
        "}\n";

    GLuint shader = CompileShader(GL_FRAGMENT_SHADER, source);
    ASSERT_NE(0u, shader);
    glDeleteShader(shader);
}

// Test that two constructors which have vec4 and mat2 parameters get disambiguated (issue in
// HLSL).
TEST_P(GLSLTest_ES3, AmbiguousConstructorCall2x2)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(0.0);\n"
        "}";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "in vec4 a_vec;\n"
        "in mat2 a_mat;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_vec) + vec4(a_mat);\n"
        "}";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Test that two constructors which have mat2x3 and mat3x2 parameters get disambiguated.
// This was suspected to be an issue in HLSL, but HLSL seems to be able to natively choose between
// the function signatures in this case.
TEST_P(GLSLTest_ES3, AmbiguousConstructorCall2x3)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(0.0);\n"
        "}";

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "in mat3x2 a_matA;\n"
        "in mat2x3 a_matB;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_matA) + vec4(a_matB);\n"
        "}";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Test that two functions which have vec4 and mat2 parameters get disambiguated (issue in HLSL).
TEST_P(GLSLTest_ES3, AmbiguousFunctionCall2x2)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(0.0);\n"
        "}";

    const std::string vertexShaderSource =
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

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Test that an user-defined function with a large number of float4 parameters doesn't fail due to
// the function name being too long.
TEST_P(GLSLTest_ES3, LargeNumberOfFloat4Parameters)
{
    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(0.0);\n"
        "}";

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
                          "    return ";
    for (unsigned int i = 0; i < paramCount; ++i)
    {
        vertexShaderStream << "a" << i << " + ";
    }
    vertexShaderStream << "aLast;\n"
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

    GLuint program = CompileProgram(vertexShaderStream.str(), fragmentShaderSource);
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
    if (IsAndroid() && IsAdreno() && IsOpenGLES())
    {
        std::cout << "Test skipped on Adreno OpenGLES on Android." << std::endl;
        return;
    }

    const std::string vertexShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "in vec4 a_vec;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_vec);\n"
        "}";

    const std::string fragmentShaderSource =
        "#version 300 es\n"
        "precision highp float;\n"
        "out vec4 my_FragColor;\n"
        "const highp float f[2] = float[2](0.1, 0.2);\n"
        "const highp float[2] g = float[2](0.3, 0.4), h = float[2](0.5, f[1]);\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(h[1]);\n"
        "}";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Test that index-constant sampler array indexing is supported.
TEST_P(GLSLTest, IndexConstantSamplerArrayIndexing)
{
    if (IsD3D11_FL93()) {
        std::cout << "Test skipped on D3D11 FL 9.3." << std::endl;
        return;
    }

    const std::string vertexShaderSource =
        "attribute vec4 vPosition;\n"
        "void main()\n"
        "{\n"
        "      gl_Position = vPosition;\n"
        "}";

    const std::string fragmentShaderSource =
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

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Test that the #pragma directive is supported and doesn't trigger a compilation failure on the
// native driver. The only pragma that gets passed to the OpenGL driver is "invariant" but we don't
// want to test its behavior, so don't use any varyings.
TEST_P(GLSLTest, PragmaDirective)
{
    const std::string vertexShaderSource =
        "#pragma STDGL invariant(all)\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    const std::string fragmentShaderSource =
        "precision mediump float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    GLuint program = CompileProgram(vertexShaderSource, fragmentShaderSource);
    EXPECT_NE(0u, program);
}

// Sequence operator evaluates operands from left to right (ESSL 3.00 section 5.9).
// The function call that returns the array needs to be evaluated after ++j for the expression to
// return the correct value (true).
TEST_P(GLSLTest_ES3, SequenceOperatorEvaluationOrderArray)
{
    const std::string &fragmentShaderSource =
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

    GLuint program = CompileProgram(mSimpleVSSource, fragmentShaderSource);
    ASSERT_NE(0u, program);

    drawQuad(program, "inputAttribute", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Sequence operator evaluates operands from left to right (ESSL 3.00 section 5.9).
// The short-circuiting expression needs to be evaluated after ++j for the expression to return the
// correct value (true).
TEST_P(GLSLTest_ES3, SequenceOperatorEvaluationOrderShortCircuit)
{
    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor; \n"
        "void main() {\n"
        "    int j = 0; \n"
        "    bool result = ((++j), (j == 1 ? true : (++j == 3)));\n"
        "    my_FragColor = vec4(0.0, ((result && j == 1) ? 1.0 : 0.0), 0.0, 1.0);\n"
        "}\n";

    GLuint program = CompileProgram(mSimpleVSSource, fragmentShaderSource);
    ASSERT_NE(0u, program);

    drawQuad(program, "inputAttribute", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Sequence operator evaluates operands from left to right (ESSL 3.00 section 5.9).
// Indexing the vector needs to be evaluated after func() for the right result.
TEST_P(GLSLTest_ES3, SequenceOperatorEvaluationOrderDynamicVectorIndexingInLValue)
{
    const std::string &fragmentShaderSource =
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

    GLuint program = CompileProgram(mSimpleVSSource, fragmentShaderSource);
    ASSERT_NE(0u, program);

    drawQuad(program, "inputAttribute", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that using gl_PointCoord with GL_TRIANGLES doesn't produce a link error.
// From WebGL test conformance/rendering/point-specific-shader-variables.html
// See http://anglebug.com/1380
TEST_P(GLSLTest, RenderTrisWithPointCoord)
{
    const std::string &vert =
        "attribute vec2 aPosition;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPosition, 0, 1);\n"
        "    gl_PointSize = 1.0;\n"
        "}";
    const std::string &frag =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(gl_PointCoord.xy, 0, 1);\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(prog, vert, frag);
    drawQuad(prog.get(), "aPosition", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Convers a bug with the integer pow statement workaround.
TEST_P(GLSLTest, NestedPowStatements)
{
    const std::string &vert =
        "attribute vec2 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "}";
    const std::string &frag =
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

    ANGLE_GL_PROGRAM(prog, vert, frag);
    drawQuad(prog.get(), "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that -float calculation is correct.
TEST_P(GLSLTest_ES3, UnaryMinusOperatorFloat)
{
    const std::string &vert =
        "#version 300 es\n"
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    const std::string &frag =
        "#version 300 es\n"
        "out highp vec4 o_color;\n"
        "void main() {\n"
        "    highp float f = -1.0;\n"
        "    // atan(tan(0.5), -f) should be 0.5.\n"
        "    highp float v = atan(tan(0.5), -f);\n"
        "    o_color = abs(v - 0.5) < 0.001 ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, vert, frag);
    drawQuad(prog.get(), "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that atan(vec2, vec2) calculation is correct.
TEST_P(GLSLTest_ES3, AtanVec2)
{
    const std::string &vert =
        "#version 300 es\n"
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    const std::string &frag =
        "#version 300 es\n"
        "out highp vec4 o_color;\n"
        "void main() {\n"
        "    highp float f = 1.0;\n"
        "    // atan(tan(0.5), f) should be 0.5.\n"
        "    highp vec2 v = atan(vec2(tan(0.5)), vec2(f));\n"
        "    o_color = (abs(v[0] - 0.5) < 0.001 && abs(v[1] - 0.5) < 0.001) ? vec4(0, 1, 0, 1) : "
        "vec4(1, 0, 0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, vert, frag);
    drawQuad(prog.get(), "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Convers a bug with the unary minus operator on signed integer workaround.
TEST_P(GLSLTest_ES3, UnaryMinusOperatorSignedInt)
{
    const std::string &vert =
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
    const std::string &frag =
        "#version 300 es\n"
        "in mediump vec4 v_color;\n"
        "layout(location=0) out mediump vec4 o_color;\n"
        "void main() {\n"
        "    o_color = v_color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, vert, frag);

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
    const std::string &vert =
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
    const std::string &frag =
        "#version 300 es\n"
        "in mediump vec4 v_color;\n"
        "layout(location=0) out mediump vec4 o_color;\n"
        "void main() {\n"
        "    o_color = v_color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(prog, vert, frag);

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
    const std::string &vert =
        "attribute vec2 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "}";

    // Note that the uniform keep_flop_positive doesn't need to be set - the test expects it to have
    // its default value false.
    const std::string &frag =
        "precision mediump float;\n"
        "uniform bool keep_flop_positive;\n"
        "float flop;\n"
        "void main() {\n"
        "    flop = -1.0,\n"
        "    (flop *= -1.0,\n"
        "    keep_flop_positive ? 0.0 : flop *= -1.0),\n"
        "    gl_FragColor = vec4(0, -flop, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(prog, vert, frag);
    drawQuad(prog.get(), "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that using a sampler2D and samplerExternalOES in the same shader works (anglebug.com/1534)
TEST_P(GLSLTest, ExternalAnd2DSampler)
{
    if (!extensionEnabled("GL_OES_EGL_image_external"))
    {
        std::cout << "Test skipped because GL_OES_EGL_image_external is not available."
                  << std::endl;
        return;
    }

    const std::string fragmentShader =
        R"(
        #extension GL_OES_EGL_image_external : enable
        precision mediump float;
        uniform samplerExternalOES tex0;
        uniform sampler2D tex1;
        void main(void)
        {
            vec2 uv = vec2(0.0, 0.0);
            gl_FragColor = texture2D(tex0, uv) + texture2D(tex1, uv);
        })";

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test that literal infinity can be written out from the shader translator.
// A similar test can't be made for NaNs, since ESSL 3.00.6 requirements for NaNs are very loose.
TEST_P(GLSLTest_ES3, LiteralInfinityOutput)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that literal negative infinity can be written out from the shader translator.
// A similar test can't be made for NaNs, since ESSL 3.00.6 requirements for NaNs are very loose.
TEST_P(GLSLTest_ES3, LiteralNegativeInfinityOutput)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// The following MultipleDeclaration* tests are testing TranslatorHLSL specific simplification
// passes. Because the interaction of multiple passes must be tested, it is difficult to write
// a unittest for them. Instead we add the tests as end2end so will in particular test
// TranslatorHLSL when run on Windows.

// Test that passes splitting multiple declarations and comma operators are correctly ordered.
TEST_P(GLSLTest_ES3, MultipleDeclarationWithCommaOperator)
{
    const std::string &fragmentShader =
        R"(#version 300 es
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test that passes splitting multiple declarations and comma operators and for loops are
// correctly ordered.
TEST_P(GLSLTest_ES3, MultipleDeclarationWithCommaOperatorInForLoop)
{
    const std::string &fragmentShader =
        R"(#version 300 es
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test that splitting multiple declaration in for loops works with no loop condition
TEST_P(GLSLTest_ES3, MultipleDeclarationInForLoopEmptyCondition)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test that splitting multiple declaration in for loops works with no loop expression
TEST_P(GLSLTest_ES3, MultipleDeclarationInForLoopEmptyExpression)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test that dynamic indexing of a matrix inside a dynamic indexing of a vector in an l-value works
// correctly.
TEST_P(GLSLTest_ES3, NestedDynamicIndexingInLValue)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
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

}  // anonymous namespace

// Test that FindLSB and FindMSB return correct values in their corner cases.
TEST_P(GLSLTest_ES31, FindMSBAndFindLSBCornerCases)
{
    // Suspecting AMD driver bug - failure seen on bots running on AMD R5 230.
    if (IsAMD() && IsOpenGL() && IsLinux())
    {
        std::cout << "Test skipped on AMD OpenGL Linux" << std::endl;
        return;
    }

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that writing into a swizzled vector that is dynamically indexed succeeds.
TEST_P(GLSLTest_ES3, WriteIntoDynamicIndexingOfSwizzledVector)
{
    if (IsOpenGL())
    {
        // http://anglebug.com/1924
        std::cout << "Test skipped on all OpenGL configurations because it has incorrect results"
                  << std::endl;
        return;
    }

    // The shader first assigns v.x to v.z (1.0)
    // Then v.y to v.y (2.0)
    // Then v.z to v.x (1.0)
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// This test covers a bug (and associated workaround) with nested sampling operations in the HLSL
// compiler DLL.
TEST_P(GLSLTest_ES3, NestedSamplingOperation)
{
    // This seems to be bugged on some version of Android. Might not affect the newest versions.
    // TODO(jmadill): Lift suppression when Chromium bots are upgraded.
    if (IsAndroid() && IsOpenGLES())
    {
        std::cout << "Test skipped on Android because of bug with Nexus 5X." << std::endl;
        return;
    }

    const std::string &vertexShader =
        "#version 300 es\n"
        "out vec2 texCoord;\n"
        "in vec2 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    texCoord = position * 0.5 + vec2(0.5);\n"
        "}\n";

    const std::string &simpleFragmentShader =
        "#version 300 es\n"
        "in mediump vec2 texCoord;\n"
        "out mediump vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    fragColor = vec4(texCoord, 0, 1);\n"
        "}\n";

    const std::string &nestedFragmentShader =
        "#version 300 es\n"
        "uniform mediump sampler2D samplerA;\n"
        "uniform mediump sampler2D samplerB;\n"
        "in mediump vec2 texCoord;\n"
        "out mediump vec4 fragColor;\n"
        "void main ()\n"
        "{\n"
        "    fragColor = texture(samplerB, texture(samplerA, texCoord).xy);\n"
        "}\n";

    ANGLE_GL_PROGRAM(initProg, vertexShader, simpleFragmentShader);
    ANGLE_GL_PROGRAM(nestedProg, vertexShader, nestedFragmentShader);

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
    const std::string &vertexShader =
        "void main()\n"
        "{\n"
        "    for (int i = 0; i < 10; ++i)\n"
        "        const int b = 0;\n"
        "}\n";

    GLuint shader = CompileShader(GL_VERTEX_SHADER, vertexShader);
    EXPECT_NE(0u, shader);
    glDeleteShader(shader);
}

// Tests that using a constant declaration as a loop init expression doesn't crash. Note that this
// test doesn't work on D3D9 due to looping limitations, so it is only run on ES3.
TEST_P(GLSLTest_ES3, ConstantStatementAsLoopInit)
{
    const std::string &vertexShader =
        "void main()\n"
        "{\n"
        "    for (const int i = 0; i < 0;) {}\n"
        "}\n";

    GLuint shader = CompileShader(GL_VERTEX_SHADER, vertexShader);
    EXPECT_NE(0u, shader);
    glDeleteShader(shader);
}

// Test that uninitialized local variables are initialized to 0.
TEST_P(GLSLTest_ES3, InitUninitializedLocals)
{
    if (IsAndroid() && IsOpenGLES())
    {
        // http://anglebug.com/2046
        std::cout
            << "Test skipped on Android GLES because local variable initialization is disabled."
            << std::endl;
        return;
    }

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that uninitialized structs containing arrays of structs are initialized to 0. This
// specifically tests with two different struct variables declared in the same block.
TEST_P(GLSLTest, InitUninitializedStructContainingArrays)
{
    if (IsAndroid() && IsOpenGLES())
    {
        // http://anglebug.com/2046
        std::cout
            << "Test skipped on Android GLES because local variable initialization is disabled."
            << std::endl;
        return;
    }

    if (IsOSX() && IsOpenGL())
    {
        // http://anglebug.com/2041
        std::cout << "Test skipped on Mac OpenGL because local variable initialization is disabled."
                  << std::endl;
        return;
    }

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
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
    if (IsAndroid() && IsOpenGLES())
    {
        // http://anglebug.com/2046
        std::cout
            << "Test skipped on Android GLES because local variable initialization is disabled."
            << std::endl;
        return;
    }

    if (IsOSX() && IsOpenGL())
    {
        // http://anglebug.com/2041
        std::cout << "Test skipped on Mac OpenGL because local variable initialization is disabled."
                  << std::endl;
        return;
    }

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that uninitialized global variables are initialized to 0.
TEST_P(WebGLGLSLTest, InitUninitializedGlobals)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that an uninitialized nameless struct in the global scope works.
TEST_P(WebGLGLSLTest, UninitializedNamelessStructInGlobalScope)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a loop condition that has an initializer declares a variable.
TEST_P(GLSLTest_ES3, ConditionInitializerDeclaresVariable)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a variable hides a user-defined function with the same name after its initializer.
// GLSL ES 1.00.17 section 4.2.2: "A variable declaration is visible immediately following the
// initializer if present, otherwise immediately following the identifier"
TEST_P(GLSLTest, VariableHidesUserDefinedFunctionAfterInitializer)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that structs with identical members are not ambiguous as function arguments.
TEST_P(GLSLTest, StructsWithSameMembersDisambiguatedByName)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a varying struct that's not statically used in the fragment shader works.
// GLSL ES 3.00.6 section 4.3.10.
TEST_P(GLSLTest_ES3, VaryingStructNotStaticallyUsedInFragmentShader)
{
    const std::string &vertexShader =
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

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
}

// Test that a varying struct that's not declared in the fragment shader links successfully.
// GLSL ES 3.00.6 section 4.3.10.
TEST_P(GLSLTest_ES3, VaryingStructNotDeclaredInFragmentShader)
{
    const std::string &vertexShader =
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

    const std::string &fragmentShader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "    col = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
}

// Test that a varying struct that gets used in the fragment shader works.
TEST_P(GLSLTest_ES3, VaryingStructUsedInFragmentShader)
{
    const std::string &vertexShader =
        "#version 300 es\n"
        "in vec4 inputAttribute;\n"
        "struct S {\n"
        "    vec4 field;\n"
        "};\n"
        "out S varStruct;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = inputAttribute;\n"
        "    varStruct.field = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}\n";

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// This test covers passing an array of structs containing samplers as a function argument.
TEST_P(GLSLTest, ArrayOfStructsWithSamplersAsFunctionArg)
{
    if (IsAndroid() && IsAdreno() && IsOpenGLES())
    {
        // Shader failed to compile on Android. http://anglebug.com/2114
        std::cout << "Test skipped on Adreno OpenGLES on Android." << std::endl;
        return;
    }

    const std::string &vertexShader =
        "attribute vec2 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "}\n";

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

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

    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// This test covers passing a struct containing an array of samplers as a function argument.
TEST_P(GLSLTest, StructWithSamplerArrayAsFunctionArg)
{
    if (IsAndroid() && IsAdreno() && IsOpenGLES())
    {
        // Shader failed to compile on Android. http://anglebug.com/2114
        std::cout << "Test skipped on Adreno OpenGLES on Android." << std::endl;
        return;
    }

    const std::string &vertexShader =
        "attribute vec2 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "}\n";

    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

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

    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
}

// Test that a global variable declared after main() works. This is a regression test for an issue
// in global variable initialization.
TEST_P(WebGLGLSLTest, GlobalVariableDeclaredAfterMain)
{
    const std::string &fragmentShader =
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression having side effects inside a loop condition.
// The spec says that sequence operator operands need to run in sequence.
TEST_P(GLSLTest_ES3, ArrayLengthOnExpressionWithSideEffectsInLoopCondition)
{
    // "a" gets doubled three times in the below program.
    const std::string &fragmentShader =
        R"(#version 300 es
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression having side effects that interact with side
// effects of another operand of the same sequence operator. The spec says that sequence operator
// operands need to run in order from left to right (ESSL 3.00.6 section 5.9).
TEST_P(GLSLTest_ES3, ArrayLengthOnExpressionWithSideEffectsInSequence)
{
    const std::string &fragmentShader =
        R"(#version 300 es
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test calling array length() with a "this" expression that also contains a call of array length().
// Both "this" expressions also have side effects.
TEST_P(GLSLTest_ES3, NestedArrayLengthMethodsWithSideEffects)
{
    const std::string &fragmentShader =
        R"(#version 300 es
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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that statements inside switch() get translated to correct HLSL.
TEST_P(GLSLTest_ES3, DifferentStatementsInsideSwitch)
{
    const std::string &fragmentShader =
        R"(#version 300 es

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
    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test that switch fall-through works correctly.
// This is a regression test for http://anglebug.com/2178
TEST_P(GLSLTest_ES3, SwitchFallThroughCodeDuplication)
{
    const std::string &fragmentShader =
        R"(#version 300 es

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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a switch statement with an empty block inside as a final statement compiles.
TEST_P(GLSLTest_ES3, SwitchFinalCaseHasEmptyBlock)
{
    const std::string &fragmentShader =
        R"(#version 300 es

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
    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test that a switch statement with an empty declaration inside as a final statement compiles.
TEST_P(GLSLTest_ES3, SwitchFinalCaseHasEmptyDeclaration)
{
    const std::string &fragmentShader =
        R"(#version 300 es

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
    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
}

// Test switch/case where break/return statements are within blocks.
TEST_P(GLSLTest_ES3, SwitchBreakOrReturnInsideBlocks)
{
    const std::string &fragmentShader =
        R"(#version 300 es

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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test switch/case where a variable is declared inside one of the cases and is accessed by a
// subsequent case.
TEST_P(GLSLTest_ES3, SwitchWithVariableDeclarationInside)
{
    const std::string &fragmentShader =
        R"(#version 300 es

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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test nested switch/case where a variable is declared inside one of the cases and is accessed by a
// subsequent case.
TEST_P(GLSLTest_ES3, NestedSwitchWithVariableDeclarationInside)
{
    const std::string &fragmentShader =
        R"(#version 300 es

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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that an empty switch/case statement is translated in a way that compiles and executes the
// init-statement.
TEST_P(GLSLTest_ES3, EmptySwitch)
{
    const std::string &fragmentShader =
        R"(#version 300 es

        precision highp float;

        uniform int u_zero;
        out vec4 my_FragColor;

        void main()
        {
            int i = u_zero;
            switch(++i) {}
            my_FragColor = (i == 1) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
        })";

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a constant struct inside an expression is handled correctly.
TEST_P(GLSLTest_ES3, ConstStructInsideExpression)
{
    // Incorrect output color was seen on Android. http://anglebug.com/2226
    ANGLE_SKIP_TEST_IF(IsAndroid() && !IsNVIDIA() && IsOpenGLES());

    const std::string &fragmentShader =
        R"(#version 300 es

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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that a varying struct that's defined as a part of the declaration is handled correctly.
TEST_P(GLSLTest_ES3, VaryingStructWithInlineDefinition)
{
    const std::string &vertexShader =
        R"(#version 300 es
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

    const std::string &fragmentShader =
        R"(#version 300 es

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

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test vector/scalar arithmetic (in this case multiplication and addition). Meant to reproduce a
// bug that appeared in NVIDIA OpenGL drivers and that is worked around by
// VectorizeVectorScalarArithmetic AST transform.
TEST_P(GLSLTest, VectorScalarMultiplyAndAddInLoop)
{
    const std::string &fragmentShader =
        R"(

        precision mediump float;

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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test vector/scalar arithmetic (in this case compound division and addition). Meant to reproduce a
// bug that appeared in NVIDIA OpenGL drivers and that is worked around by
// VectorizeVectorScalarArithmetic AST transform.
TEST_P(GLSLTest, VectorScalarDivideAndAddInLoop)
{
    const std::string &fragmentShader =
        R"(

        precision mediump float;

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

    ANGLE_GL_PROGRAM(program, mSimpleVSSource, fragmentShader);
    drawQuad(program.get(), "inputAttribute", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(GLSLTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(GLSLTest_ES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());

ANGLE_INSTANTIATE_TEST(WebGLGLSLTest, ES2_D3D11(), ES2_OPENGL(), ES2_OPENGLES());

ANGLE_INSTANTIATE_TEST(GLSLTest_ES31, ES31_D3D11(), ES31_OPENGL(), ES31_OPENGLES());
