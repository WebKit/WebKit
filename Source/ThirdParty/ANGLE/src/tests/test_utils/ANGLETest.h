//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLETest:
//   Implementation of common ANGLE testing fixture.
//

#ifndef ANGLE_TESTS_ANGLE_TEST_H_
#define ANGLE_TESTS_ANGLE_TEST_H_

#include <gtest/gtest.h>
#include <algorithm>
#include <array>

#include "angle_gl.h"
#include "angle_test_configs.h"
#include "common/angleutils.h"
#include "common/vector_utils.h"
#include "platform/Platform.h"
#include "shader_utils.h"
#include "system_utils.h"

#define ASSERT_GL_TRUE(a) ASSERT_EQ(static_cast<GLboolean>(GL_TRUE), (a))
#define ASSERT_GL_FALSE(a) ASSERT_EQ(static_cast<GLboolean>(GL_FALSE), (a))
#define EXPECT_GL_TRUE(a) EXPECT_EQ(static_cast<GLboolean>(GL_TRUE), (a))
#define EXPECT_GL_FALSE(a) EXPECT_EQ(static_cast<GLboolean>(GL_FALSE), (a))

#define EXPECT_GL_ERROR(err) EXPECT_EQ(static_cast<GLenum>(err), glGetError())
#define EXPECT_GL_NO_ERROR() EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError())

#define ASSERT_GL_ERROR(err) ASSERT_EQ(static_cast<GLenum>(err), glGetError())
#define ASSERT_GL_NO_ERROR() ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError())

#define EXPECT_EGL_ERROR(err) EXPECT_EQ((err), eglGetError())
#define EXPECT_EGL_SUCCESS() EXPECT_EGL_ERROR(EGL_SUCCESS)

// EGLBoolean is |unsigned int| but EGL_TRUE is 0, not 0u.
#define ASSERT_EGL_TRUE(a) ASSERT_EQ(static_cast<EGLBoolean>(EGL_TRUE), (a))
#define ASSERT_EGL_FALSE(a) ASSERT_EQ(static_cast<EGLBoolean>(EGL_FALSE), (a))
#define EXPECT_EGL_TRUE(a) EXPECT_EQ(static_cast<EGLBoolean>(EGL_TRUE), (a))
#define EXPECT_EGL_FALSE(a) EXPECT_EQ(static_cast<EGLBoolean>(EGL_FALSE), (a))

#define ASSERT_EGL_ERROR(err) ASSERT_EQ((err), eglGetError())
#define ASSERT_EGL_SUCCESS() ASSERT_EGL_ERROR(EGL_SUCCESS)

#define ASSERT_GLENUM_EQ(expected, actual) ASSERT_EQ(static_cast<GLenum>(expected), static_cast<GLenum>(actual))
#define EXPECT_GLENUM_EQ(expected, actual) EXPECT_EQ(static_cast<GLenum>(expected), static_cast<GLenum>(actual))
#define ASSERT_GLENUM_NE(expected, actual) \
    ASSERT_NE(static_cast<GLenum>(expected), static_cast<GLenum>(actual))
#define EXPECT_GLENUM_NE(expected, actual) \
    EXPECT_NE(static_cast<GLenum>(expected), static_cast<GLenum>(actual))

namespace angle
{
struct GLColorRGB
{
    GLColorRGB();
    GLColorRGB(GLubyte r, GLubyte g, GLubyte b);
    GLColorRGB(const angle::Vector3 &floatColor);

    GLubyte R, G, B;

    static const GLColorRGB black;
    static const GLColorRGB blue;
    static const GLColorRGB green;
    static const GLColorRGB red;
    static const GLColorRGB yellow;
};

struct GLColor
{
    GLColor();
    GLColor(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
    GLColor(const angle::Vector4 &floatColor);
    GLColor(GLuint colorValue);

    angle::Vector4 toNormalizedVector() const;

    GLubyte &operator[](size_t index) { return (&R)[index]; }

    const GLubyte &operator[](size_t index) const { return (&R)[index]; }

    testing::AssertionResult ExpectNear(const GLColor &expected, const GLColor &err) const;

    GLubyte R, G, B, A;

    static const GLColor black;
    static const GLColor blue;
    static const GLColor cyan;
    static const GLColor green;
    static const GLColor red;
    static const GLColor transparentBlack;
    static const GLColor white;
    static const GLColor yellow;
    static const GLColor magenta;
};

struct GLColor32F
{
    constexpr GLColor32F() : GLColor32F(0.0f, 0.0f, 0.0f, 0.0f) {}
    constexpr GLColor32F(GLfloat r, GLfloat g, GLfloat b, GLfloat a) : R(r), G(g), B(b), A(a) {}

    GLfloat R, G, B, A;
};

static constexpr GLColor32F kFloatRed   = {1.0f, 0.0f, 0.0f, 1.0f};
static constexpr GLColor32F kFloatGreen = {0.0f, 1.0f, 0.0f, 1.0f};
static constexpr GLColor32F kFloatBlue  = {0.0f, 0.0f, 1.0f, 1.0f};

struct WorkaroundsD3D;

// Useful to cast any type to GLubyte.
template <typename TR, typename TG, typename TB, typename TA>
GLColor MakeGLColor(TR r, TG g, TB b, TA a)
{
    return GLColor(static_cast<GLubyte>(r), static_cast<GLubyte>(g), static_cast<GLubyte>(b),
                   static_cast<GLubyte>(a));
}

bool operator==(const GLColor &a, const GLColor &b);
bool operator!=(const GLColor &a, const GLColor &b);
std::ostream &operator<<(std::ostream &ostream, const GLColor &color);
GLColor ReadColor(GLint x, GLint y);

// Useful to cast any type to GLfloat.
template <typename TR, typename TG, typename TB, typename TA>
GLColor32F MakeGLColor32F(TR r, TG g, TB b, TA a)
{
    return GLColor32F(static_cast<GLfloat>(r), static_cast<GLfloat>(g), static_cast<GLfloat>(b),
                      static_cast<GLfloat>(a));
}

bool operator==(const GLColor32F &a, const GLColor32F &b);
std::ostream &operator<<(std::ostream &ostream, const GLColor32F &color);
GLColor32F ReadColor32F(GLint x, GLint y);

}  // namespace angle

#define EXPECT_PIXEL_EQ(x, y, r, g, b, a) \
    EXPECT_EQ(angle::MakeGLColor(r, g, b, a), angle::ReadColor(x, y))

#define EXPECT_PIXEL_NE(x, y, r, g, b, a) \
    EXPECT_NE(angle::MakeGLColor(r, g, b, a), angle::ReadColor(x, y))

#define EXPECT_PIXEL_32F_EQ(x, y, r, g, b, a) \
    EXPECT_EQ(angle::MakeGLColor32F(r, g, b, a), angle::ReadColor32F(x, y))

#define EXPECT_PIXEL_ALPHA_EQ(x, y, a) EXPECT_EQ(a, angle::ReadColor(x, y).A)

#define EXPECT_PIXEL_ALPHA32F_EQ(x, y, a) EXPECT_EQ(a, angle::ReadColor32F(x, y).A)

#define EXPECT_PIXEL_COLOR_EQ(x, y, angleColor) EXPECT_EQ(angleColor, angle::ReadColor(x, y))
#define EXPECT_PIXEL_COLOR_EQ_VEC2(vec2, angleColor) \
    EXPECT_EQ(angleColor,                            \
              angle::ReadColor(static_cast<GLint>(vec2.x()), static_cast<GLint>(vec2.y())))

#define EXPECT_PIXEL_COLOR32F_EQ(x, y, angleColor) EXPECT_EQ(angleColor, angle::ReadColor32F(x, y))

#define EXPECT_PIXEL_RECT_EQ(x, y, width, height, color)                                           \
    \
{                                                                                           \
        std::vector<GLColor> actualColors(width *height);                                          \
        glReadPixels((x), (y), (width), (height), GL_RGBA, GL_UNSIGNED_BYTE, actualColors.data()); \
        std::vector<GLColor> expectedColors(width *height, color);                                 \
        EXPECT_EQ(expectedColors, actualColors);                                                   \
    \
}

#define EXPECT_PIXEL_NEAR(x, y, r, g, b, a, abs_error) \
{ \
    GLubyte pixel[4]; \
    glReadPixels((x), (y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel); \
    EXPECT_GL_NO_ERROR(); \
    EXPECT_NEAR((r), pixel[0], abs_error); \
    EXPECT_NEAR((g), pixel[1], abs_error); \
    EXPECT_NEAR((b), pixel[2], abs_error); \
    EXPECT_NEAR((a), pixel[3], abs_error); \
}

#define EXPECT_PIXEL32F_NEAR(x, y, r, g, b, a, abs_error)       \
                                                                \
    {                                                           \
        GLfloat pixel[4];                                       \
        glReadPixels((x), (y), 1, 1, GL_RGBA, GL_FLOAT, pixel); \
        EXPECT_GL_NO_ERROR();                                   \
        EXPECT_NEAR((r), pixel[0], abs_error);                  \
        EXPECT_NEAR((g), pixel[1], abs_error);                  \
        EXPECT_NEAR((b), pixel[2], abs_error);                  \
        EXPECT_NEAR((a), pixel[3], abs_error);                  \
    }

// TODO(jmadill): Figure out how we can use GLColor's nice printing with EXPECT_NEAR.
#define EXPECT_PIXEL_COLOR_NEAR(x, y, angleColor, abs_error) \
    EXPECT_PIXEL_NEAR(x, y, angleColor.R, angleColor.G, angleColor.B, angleColor.A, abs_error)

#define EXPECT_PIXEL_COLOR32F_NEAR(x, y, angleColor, abs_error) \
    EXPECT_PIXEL32F_NEAR(x, y, angleColor.R, angleColor.G, angleColor.B, angleColor.A, abs_error)

#define EXPECT_COLOR_NEAR(expected, actual, abs_error) \
    \
{                                               \
        EXPECT_NEAR(expected.R, actual.R, abs_error);  \
        EXPECT_NEAR(expected.G, actual.G, abs_error);  \
        EXPECT_NEAR(expected.B, actual.B, abs_error);  \
        EXPECT_NEAR(expected.A, actual.A, abs_error);  \
    \
}
#define EXPECT_PIXEL32F_NEAR(x, y, r, g, b, a, abs_error)       \
                                                                \
    {                                                           \
        GLfloat pixel[4];                                       \
        glReadPixels((x), (y), 1, 1, GL_RGBA, GL_FLOAT, pixel); \
        EXPECT_GL_NO_ERROR();                                   \
        EXPECT_NEAR((r), pixel[0], abs_error);                  \
        EXPECT_NEAR((g), pixel[1], abs_error);                  \
        EXPECT_NEAR((b), pixel[2], abs_error);                  \
        EXPECT_NEAR((a), pixel[3], abs_error);                  \
    }

#define EXPECT_PIXEL_COLOR32F_NEAR(x, y, angleColor, abs_error) \
    EXPECT_PIXEL32F_NEAR(x, y, angleColor.R, angleColor.G, angleColor.B, angleColor.A, abs_error)

class EGLWindow;
class OSWindow;
class ANGLETestBase;

struct TestPlatformContext final : private angle::NonCopyable
{
    bool ignoreMessages    = false;
    ANGLETestBase *currentTest = nullptr;
};

class ANGLETestBase
{
  protected:
    ANGLETestBase(const angle::PlatformParameters &params);
    virtual ~ANGLETestBase();

  public:
    static bool InitTestWindow();
    static bool DestroyTestWindow();
    static void SetWindowVisible(bool isVisible);
    static bool eglDisplayExtensionEnabled(EGLDisplay display, const std::string &extName);

    virtual void overrideWorkaroundsD3D(angle::WorkaroundsD3D *workaroundsD3D) {}

  protected:
    void ANGLETestSetUp();
    void ANGLETestTearDown();

    virtual void swapBuffers();

    void setupQuadVertexBuffer(GLfloat positionAttribZ, GLfloat positionAttribXYScale);
    void setupIndexedQuadVertexBuffer(GLfloat positionAttribZ, GLfloat positionAttribXYScale);
    void setupIndexedQuadIndexBuffer();

    void drawQuad(GLuint program, const std::string &positionAttribName, GLfloat positionAttribZ);
    void drawQuad(GLuint program,
                  const std::string &positionAttribName,
                  GLfloat positionAttribZ,
                  GLfloat positionAttribXYScale);
    void drawQuad(GLuint program,
                  const std::string &positionAttribName,
                  GLfloat positionAttribZ,
                  GLfloat positionAttribXYScale,
                  bool useVertexBuffer);
    void drawQuadInstanced(GLuint program,
                           const std::string &positionAttribName,
                           GLfloat positionAttribZ,
                           GLfloat positionAttribXYScale,
                           bool useVertexBuffer,
                           GLuint numInstances);

    static std::array<angle::Vector3, 6> GetQuadVertices();
    static std::array<GLushort, 6> GetQuadIndices();
    void drawIndexedQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ);
    void drawIndexedQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ,
                         GLfloat positionAttribXYScale);
    void drawIndexedQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ,
                         GLfloat positionAttribXYScale,
                         bool useBufferObject);

    void drawIndexedQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ,
                         GLfloat positionAttribXYScale,
                         bool useBufferObject,
                         bool restrictedRange);

    void draw2DTexturedQuad(GLfloat positionAttribZ,
                            GLfloat positionAttribXYScale,
                            bool useVertexBuffer);

    static GLuint compileShader(GLenum type, const std::string &source);
    static bool extensionEnabled(const std::string &extName);
    static bool extensionRequestable(const std::string &extName);
    static bool eglClientExtensionEnabled(const std::string &extName);
    static bool eglDeviceExtensionEnabled(EGLDeviceEXT device, const std::string &extName);

    void setWindowWidth(int width);
    void setWindowHeight(int height);
    void setConfigRedBits(int bits);
    void setConfigGreenBits(int bits);
    void setConfigBlueBits(int bits);
    void setConfigAlphaBits(int bits);
    void setConfigDepthBits(int bits);
    void setConfigStencilBits(int bits);
    void setConfigComponentType(EGLenum componentType);
    void setMultisampleEnabled(bool enabled);
    void setSamples(EGLint samples);
    void setDebugEnabled(bool enabled);
    void setNoErrorEnabled(bool enabled);
    void setWebGLCompatibilityEnabled(bool webglCompatibility);
    void setRobustAccess(bool enabled);
    void setBindGeneratesResource(bool bindGeneratesResource);
    void setDebugLayersEnabled(bool enabled);
    void setClientArraysEnabled(bool enabled);
    void setRobustResourceInit(bool enabled);
    void setContextProgramCacheEnabled(bool enabled);

    // Some EGL extension tests would like to defer the Context init until the test body.
    void setDeferContextInit(bool enabled);

    int getClientMajorVersion() const;
    int getClientMinorVersion() const;

    EGLWindow *getEGLWindow() const;
    int getWindowWidth() const;
    int getWindowHeight() const;
    bool isMultisampleEnabled() const;

    EGLint getPlatformRenderer() const;

    void ignoreD3D11SDKLayersWarnings();

    static OSWindow *GetOSWindow() { return mOSWindow; }

    GLuint get2DTexturedQuadProgram();

    angle::PlatformMethods mPlatformMethods;

    class ScopedIgnorePlatformMessages : angle::NonCopyable
    {
      public:
        ScopedIgnorePlatformMessages(ANGLETestBase *test);
        ~ScopedIgnorePlatformMessages();

      private:
        ANGLETestBase *mTest;
    };

  private:
    bool destroyEGLContext();

    void checkD3D11SDKLayersMessages();

    void drawQuad(GLuint program,
                  const std::string &positionAttribName,
                  GLfloat positionAttribZ,
                  GLfloat positionAttribXYScale,
                  bool useVertexBuffer,
                  bool useInstancedDrawCalls,
                  GLuint numInstances);

    EGLWindow *mEGLWindow;
    int mWidth;
    int mHeight;

    bool mIgnoreD3D11SDKLayersWarnings;

    // Used for indexed quad rendering
    GLuint mQuadVertexBuffer;
    GLuint mQuadIndexBuffer;

    // Used for texture rendering.
    GLuint m2DTexturedQuadProgram;

    TestPlatformContext mPlatformContext;

    bool mDeferContextInit;

    static OSWindow *mOSWindow;

    // Workaround for NVIDIA not being able to share a window with OpenGL and Vulkan.
    static Optional<EGLint> mLastRendererType;

    // For loading and freeing platform
    static std::unique_ptr<angle::Library> mGLESLibrary;
};

class ANGLETest : public ANGLETestBase, public ::testing::TestWithParam<angle::PlatformParameters>
{
  protected:
    ANGLETest();

  public:
    void SetUp() override;
    void TearDown() override;
};

class ANGLETestEnvironment : public testing::Environment
{
  public:
    void SetUp() override;
    void TearDown() override;
};

// Driver vendors
bool IsIntel();
bool IsAdreno();
bool IsAMD();
bool IsNVIDIA();

// Renderer back-ends
// Note: FL9_3 is explicitly *not* considered D3D11.
bool IsD3D11();
bool IsD3D11_FL93();
// Is a D3D9-class renderer.
bool IsD3D9();
// Is D3D9 or SM9_3 renderer.
bool IsD3DSM3();
bool IsDesktopOpenGL();
bool IsOpenGLES();
bool IsOpenGL();
bool IsOzone();
bool IsNULL();

// Operating systems
bool IsAndroid();
bool IsLinux();
bool IsOSX();
bool IsWindows();
bool IsVulkan();

// Debug/Release
bool IsDebug();
bool IsRelease();

// Note: git cl format messes up this formatting.
#define ANGLE_SKIP_TEST_IF(COND)                              \
    \
if(COND)                                                      \
    \
{                                                      \
        std::cout << "Test skipped: " #COND "." << std::endl; \
        return;                                               \
    \
} ANGLE_EMPTY_STATEMENT

#endif  // ANGLE_TESTS_ANGLE_TEST_H_
