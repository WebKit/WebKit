//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <regex>
#include <sstream>
#include <string>
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

constexpr static int W = 128, H = 128;
constexpr static std::array<float, 4> FULLSCREEN = {0, 0, W, H};

// For building the <loadops> parameter of glBeginPixelLocalStorageANGLE.
template <typename T>
struct Array
{
    Array(const std::initializer_list<T> &list) : mVec(list) {}
    operator const T *() const { return mVec.data(); }
    std::vector<T> mVec;
};

template <typename T>
static Array<T> MakeArray(const std::initializer_list<T> &list)
{
    return Array<T>(list);
}

static Array<GLenum> GLenumArray(const std::initializer_list<GLenum> &list)
{
    return Array<GLenum>(list);
}

static Array<GLfloat> ClearF(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    return MakeArray({r, g, b, a});
}

static Array<GLint> ClearI(GLint r, GLint g, GLint b, GLint a)
{
    return MakeArray({r, g, b, a});
}

static Array<GLuint> ClearUI(GLuint r, GLuint g, GLuint b, GLuint a)
{
    return MakeArray({r, g, b, a});
}

class PLSTestTexture
{
  public:
    PLSTestTexture(GLenum internalformat) { reset(internalformat); }
    PLSTestTexture(GLenum internalformat, int w, int h) { reset(internalformat, w, h); }
    PLSTestTexture(PLSTestTexture &&that) : mID(std::exchange(that.mID, 0)) {}
    void reset(GLenum internalformat) { reset(internalformat, W, H); }
    void reset(GLenum internalformat, int w, int h)
    {
        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, w, h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (mID)
        {
            glDeleteTextures(1, &mID);
        }
        mID = id;
    }
    ~PLSTestTexture()
    {
        if (mID)
        {
            glDeleteTextures(1, &mID);
        }
    }
    operator GLuint() const { return mID; }

  private:
    PLSTestTexture &operator=(const PLSTestTexture &) = delete;
    PLSTestTexture(const PLSTestTexture &)            = delete;
    GLuint mID                                        = 0;
};

struct Box
{
    using float4 = std::array<float, 4>;
    constexpr Box(float4 rect) : rect(rect), color{}, aux1{}, aux2{} {}
    constexpr Box(float4 rect, float4 incolor) : rect(rect), color(incolor), aux1{}, aux2{} {}
    constexpr Box(float4 rect, float4 incolor, float4 inaux1)
        : rect(rect), color(incolor), aux1(inaux1), aux2{}
    {}
    constexpr Box(float4 rect, float4 incolor, float4 inaux1, float4 inaux2)
        : rect(rect), color(incolor), aux1(inaux1), aux2(inaux2)
    {}
    float4 rect;
    float4 color;
    float4 aux1;
    float4 aux2;
};

enum class UseBarriers : bool
{
    No = false,
    IfNotCoherent
};

class PLSProgram
{
  public:
    enum class VertexArray
    {
        Default,
        VAO
    };

    PLSProgram(VertexArray vertexArray = VertexArray::VAO)
    {
        if (vertexArray == VertexArray::VAO)
        {
            glGenVertexArrays(1, &mVertexArray);
            glGenBuffers(1, &mVertexBuffer);
        }
    }

    ~PLSProgram()
    {
        glDeleteVertexArrays(1, &mVertexArray);
        glDeleteBuffers(1, &mVertexBuffer);
    }

    int widthUniform() const { return mWidthUniform; }
    int heightUniform() const { return mHeightUniform; }

    void compile(std::string fsMain) { compile("", fsMain); }

    void compile(std::string extensions, std::string fsMain)
    {
        glBindVertexArray(mVertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);

        if (mVertexArray == 0)
        {
            if (mLTRBLocation >= 0)
            {
                glDisableVertexAttribArray(mLTRBLocation);
            }
            if (mRGBALocation >= 0)
            {
                glDisableVertexAttribArray(mRGBALocation);
            }
            if (mAux1Location >= 0)
            {
                glDisableVertexAttribArray(mAux1Location);
            }
            if (mAux2Location >= 0)
            {
                glDisableVertexAttribArray(mAux2Location);
            }
        }

        mProgram.makeRaster(
            R"(#version 300 es
            precision highp float;

            uniform float W, H;
            in vec4 rect;
            in vec4 incolor;
            in vec4 inaux1;
            in vec4 inaux2;
            out vec4 color;
            out vec4 aux1;
            out vec4 aux2;

            void main()
            {
                color = incolor;
                aux1 = inaux1;
                aux2 = inaux2;
                gl_Position.x = ((gl_VertexID & 1) == 0 ? rect.x : rect.z) * 2.0/W - 1.0;
                gl_Position.y = ((gl_VertexID & 2) == 0 ? rect.y : rect.w) * 2.0/H - 1.0;
                gl_Position.zw = vec2(0, 1);
            })",

            std::string(R"(#version 300 es
            #extension GL_ANGLE_shader_pixel_local_storage : require
            )")
                .append(extensions)
                .append(R"(
            precision highp float;
            in vec4 color;
            in vec4 aux1;
            in vec4 aux2;)")
                .append(fsMain)
                .c_str());

        EXPECT_TRUE(mProgram.valid());

        glUseProgram(mProgram);

        mWidthUniform = glGetUniformLocation(mProgram, "W");
        glUniform1f(mWidthUniform, W);

        mHeightUniform = glGetUniformLocation(mProgram, "H");
        glUniform1f(mHeightUniform, H);

        mLTRBLocation = glGetAttribLocation(mProgram, "rect");
        glEnableVertexAttribArray(mLTRBLocation);
        glVertexAttribDivisor(mLTRBLocation, 1);

        mRGBALocation = glGetAttribLocation(mProgram, "incolor");
        glEnableVertexAttribArray(mRGBALocation);
        glVertexAttribDivisor(mRGBALocation, 1);

        mAux1Location = glGetAttribLocation(mProgram, "inaux1");
        glEnableVertexAttribArray(mAux1Location);
        glVertexAttribDivisor(mAux1Location, 1);

        mAux2Location = glGetAttribLocation(mProgram, "inaux2");
        glEnableVertexAttribArray(mAux2Location);
        glVertexAttribDivisor(mAux2Location, 1);
    }

    void bind()
    {
        glUseProgram(mProgram);
        glBindVertexArray(mVertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    }

    void drawBoxes(std::vector<Box> boxes, UseBarriers useBarriers = UseBarriers::IfNotCoherent)
    {
        uintptr_t base;
        if (mVertexBuffer == 0)
        {
            base = reinterpret_cast<uintptr_t>(boxes.data());
        }
        else
        {
            glBufferData(GL_ARRAY_BUFFER, boxes.size() * sizeof(Box), boxes.data(), GL_STATIC_DRAW);
            base = 0;
        }
        if (useBarriers == UseBarriers::IfNotCoherent &&
            !IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage_coherent"))
        {
            for (size_t i = 0; i < boxes.size(); ++i)
            {
                glVertexAttribPointer(
                    mLTRBLocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                    reinterpret_cast<void *>(base + i * sizeof(Box) + offsetof(Box, rect)));
                glVertexAttribPointer(
                    mRGBALocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                    reinterpret_cast<void *>(base + i * sizeof(Box) + offsetof(Box, color)));
                glVertexAttribPointer(
                    mAux1Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                    reinterpret_cast<void *>(base + i * sizeof(Box) + offsetof(Box, aux1)));
                glVertexAttribPointer(
                    mAux2Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                    reinterpret_cast<void *>(base + i * sizeof(Box) + offsetof(Box, aux2)));
                glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1);
                glPixelLocalStorageBarrierANGLE();
            }
        }
        else
        {
            glVertexAttribPointer(mLTRBLocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  reinterpret_cast<void *>(base + offsetof(Box, rect)));
            glVertexAttribPointer(mRGBALocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  reinterpret_cast<void *>(base + offsetof(Box, color)));
            glVertexAttribPointer(mAux1Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  reinterpret_cast<void *>(base + offsetof(Box, aux1)));
            glVertexAttribPointer(mAux2Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  reinterpret_cast<void *>(base + offsetof(Box, aux2)));
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, boxes.size());
        }
    }

  private:
    GLProgram mProgram;
    GLuint mVertexArray  = 0;
    GLuint mVertexBuffer = 0;

    GLint mWidthUniform  = -1;
    GLint mHeightUniform = -1;

    GLint mLTRBLocation = -1;
    GLint mRGBALocation = -1;
    GLint mAux1Location = -1;
    GLint mAux2Location = -1;
};

class ShaderInfoLog
{
  public:
    bool compileFragmentShader(const char *source)
    {
        return compileShader(source, GL_FRAGMENT_SHADER);
    }

    bool compileShader(const char *source, GLenum shaderType)
    {
        mInfoLog.clear();

        GLuint shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint compileResult;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);

        if (compileResult == 0)
        {
            GLint infoLogLength;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
            // Info log length includes the null terminator; std::string::reserve does not.
            mInfoLog.resize(std::max(infoLogLength - 1, 0));
            glGetShaderInfoLog(shader, infoLogLength, nullptr, mInfoLog.data());
        }

        glDeleteShader(shader);
        return compileResult != 0;
    }

    bool has(const char *subStr) const { return strstr(mInfoLog.c_str(), subStr); }

  private:
    std::string mInfoLog;
};

class PixelLocalStorageTest : public ANGLETest<>
{
  public:
    PixelLocalStorageTest()
    {
        setWindowWidth(W);
        setWindowHeight(H);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    ~PixelLocalStorageTest()
    {
        if (mScratchFBO)
        {
            glDeleteFramebuffers(1, &mScratchFBO);
        }
    }

    void testSetUp() override
    {
        if (IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"))
        {
            glGetIntegerv(GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE, &MAX_PIXEL_LOCAL_STORAGE_PLANES);
            glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE,
                          &MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE);
            glGetIntegerv(GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE,
                          &MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES);
            glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);
            glGetIntegerv(GL_MAX_DRAW_BUFFERS, &MAX_DRAW_BUFFERS);
        }

        // INVALID_OPERATION is generated if DITHER is enabled.
        glDisable(GL_DITHER);

        ANGLETest::testSetUp();
    }

    int isContextVersionAtLeast(int major, int minor)
    {
        return getClientMajorVersion() > major ||
               (getClientMajorVersion() == major && getClientMinorVersion() >= minor);
    }

    void attachTextureToScratchFBO(GLuint tex, GLint level = 0)
    {
        if (!mScratchFBO)
        {
            glGenFramebuffers(1, &mScratchFBO);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, mScratchFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, level);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    }

    // Access texture contents by rendering them into FBO 0, rather than just grabbing them with
    // glReadPixels.
    void renderTextureToDefaultFramebuffer(GLuint tex)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // Reset the framebuffer contents to some value that might help debugging.
        glClearColor(.1f, .4f, .6f, .9f);
        glClear(GL_COLOR_BUFFER_BIT);

        GLint linked = 0;
        glGetProgramiv(mRenderTextureProgram, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            constexpr char kVS[] =
                R"(#version 300 es
                precision highp float;
                out vec2 texcoord;
                void main()
                {
                    texcoord.x = (gl_VertexID & 1) == 0 ? 0.0 : 1.0;
                    texcoord.y = (gl_VertexID & 2) == 0 ? 0.0 : 1.0;
                    gl_Position = vec4(texcoord * 2.0 - 1.0, 0, 1);
                })";

            constexpr char kFS[] =
                R"(#version 300 es
                precision highp float;
                uniform highp sampler2D tex;  // FIXME! layout(binding=0) causes an ANGLE crash!
                in vec2 texcoord;
                out vec4 fragcolor;
                void main()
                {
                    fragcolor = texture(tex, texcoord);
                })";

            mRenderTextureProgram.makeRaster(kVS, kFS);
            ASSERT_TRUE(mRenderTextureProgram.valid());
            glUseProgram(mRenderTextureProgram);
            glUniform1i(glGetUniformLocation(mRenderTextureProgram, "tex"), 0);
        }

        glUseProgram(mRenderTextureProgram);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // Implemented as a class members so we can run the test on ES3 and ES31 both.
    void doStateRestorationTest();
    void doDrawStateTest();

    GLint MAX_PIXEL_LOCAL_STORAGE_PLANES                           = 0;
    GLint MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE    = 0;
    GLint MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES = 0;
    GLint MAX_COLOR_ATTACHMENTS                                    = 0;
    GLint MAX_DRAW_BUFFERS                                         = 0;

    PLSProgram mProgram{PLSProgram::VertexArray::Default};

    GLuint mScratchFBO = 0;
    GLProgram mRenderTextureProgram;
};

// Verify conformant implementation-dependent PLS limits.
TEST_P(PixelLocalStorageTest, ImplementationDependentLimits)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    // Table 6.X: Impementation Dependent Pixel Local Storage Limits.
    EXPECT_TRUE(MAX_PIXEL_LOCAL_STORAGE_PLANES >= 4);
    EXPECT_TRUE(MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE >= 0);
    EXPECT_TRUE(MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES >= 4);

    // Logical deductions based on 6.X.
    EXPECT_TRUE(MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES >=
                MAX_PIXEL_LOCAL_STORAGE_PLANES);
    EXPECT_TRUE(MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES >=
                MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE);
    EXPECT_TRUE(MAX_COLOR_ATTACHMENTS + MAX_PIXEL_LOCAL_STORAGE_PLANES >=
                MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES);
    EXPECT_TRUE(MAX_DRAW_BUFFERS + MAX_PIXEL_LOCAL_STORAGE_PLANES >=
                MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES);
}

// Verify that rgba8, rgba8i, and rgba8ui pixel local storage behaves as specified.
TEST_P(PixelLocalStorageTest, RGBA8)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    mProgram.compile(R"(
        layout(binding=0, rgba8) uniform lowp pixelLocalANGLE plane1;
        layout(rgba8i, binding=1) uniform lowp ipixelLocalANGLE plane2;
        layout(binding=2, rgba8ui) uniform lowp upixelLocalANGLE plane3;
        void main()
        {
            pixelLocalStoreANGLE(plane1, color + pixelLocalLoadANGLE(plane1));
            pixelLocalStoreANGLE(plane2, ivec4(aux1) + pixelLocalLoadANGLE(plane2));
            pixelLocalStoreANGLE(plane3, uvec4(aux2) + pixelLocalLoadANGLE(plane3));
        })");

    PLSTestTexture tex1(GL_RGBA8);
    PLSTestTexture tex2(GL_RGBA8I);
    PLSTestTexture tex3(GL_RGBA8UI);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex1, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex2, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex3, 0, 0);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}));

    // Accumulate R, G, B, A in 4 separate passes.
    // Store out-of-range values to ensure they are properly clamped upon storage.
    mProgram.drawBoxes({{FULLSCREEN, {2, -1, -2, -3}, {-500, 0, 0, 0}, {1, 0, 0, 0}},
                        {FULLSCREEN, {0, 1, 0, 100}, {0, -129, 0, 0}, {0, 50, 0, 0}},
                        {FULLSCREEN, {0, 0, 1, 0}, {0, 0, -70, 0}, {0, 0, 100, 0}},
                        {FULLSCREEN, {0, 0, 0, -1}, {128, 0, 0, 500}, {0, 0, 0, 300}}});

    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_KEEP, GL_KEEP}));

    attachTextureToScratchFBO(tex1);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(255, 255, 255, 0));

    attachTextureToScratchFBO(tex2);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(0, -128, -70, 127));

    attachTextureToScratchFBO(tex3);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(1, 50, 100, 255));

    ASSERT_GL_NO_ERROR();
}

// Verify that r32f and r32ui pixel local storage behaves as specified.
TEST_P(PixelLocalStorageTest, R32)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    mProgram.compile(R"(
        layout(r32f, binding=0) uniform highp pixelLocalANGLE plane1;
        layout(binding=1, r32ui) uniform highp upixelLocalANGLE plane2;
        void main()
        {
            pixelLocalStoreANGLE(plane1, color + pixelLocalLoadANGLE(plane1));
            pixelLocalStoreANGLE(plane2, uvec4(aux1) + pixelLocalLoadANGLE(plane2));
        })");

    PLSTestTexture tex1(GL_R32F);
    PLSTestTexture tex2(GL_R32UI);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex1, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex2, 0, 0);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));

    // Accumulate R in 4 separate passes.
    mProgram.drawBoxes({{FULLSCREEN, {-1.5, 0, 0, 0}, {0x000000ff, 0, 0, 0}},
                        {FULLSCREEN, {-10.25, 0, 0, 0}, {0x0000ff00, 0, 0, 0}},
                        {FULLSCREEN, {-100, 0, 0, 0}, {0x00ff0000, 0, 0, 0}},
                        {FULLSCREEN, {.25, 0, 0, 0}, {0xff000000, 0, 0, 22}}});

    glEndPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_KEEP}));

    // These values should be exact matches.
    //
    // GL_R32F is spec'd as a 32-bit IEEE float, and GL_R32UI is a 32-bit unsigned integer.
    // There is some affordance for fp32 fused operations, but "a + b" is required to be
    // correctly rounded.
    //
    // From the GLSL ES 3.0 spec:
    //
    //   "Highp unsigned integers have exactly 32 bits of precision. Highp signed integers use
    //    32 bits, including a sign bit, in two's complement form."
    //
    //   "Highp floating-point variables within a shader are encoded according to the IEEE 754
    //    specification for single-precision floating-point values (logically, not necessarily
    //    physically)."
    //
    //   "Operation: a + b, a - b, a * b
    //    Precision: Correctly rounded."
    attachTextureToScratchFBO(tex1);
    EXPECT_PIXEL_RECT32F_EQ(0, 0, W, H, GLColor32F(-111.5, 0, 0, 1));

    attachTextureToScratchFBO(tex2);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0xffffffff, 0, 0, 1));

    ASSERT_GL_NO_ERROR();
}

#define EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(plane, rgba)                 \
    {                                                                     \
        std::array<GLfloat, 4> expected rgba;                             \
        std::array<GLfloat, 4> value;                                     \
        glGetFramebufferPixelLocalStorageParameterfvANGLE(                \
            plane, GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE, value.data()); \
        EXPECT_EQ(value, expected);                                       \
    }

#define EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(plane, rgba)                 \
    {                                                                   \
        std::array<GLint, 4> expected rgba;                             \
        std::array<GLint, 4> value;                                     \
        glGetFramebufferPixelLocalStorageParameterivANGLE(              \
            plane, GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE, value.data()); \
        EXPECT_EQ(value, expected);                                     \
    }

#define EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(plane, rgba)                  \
    {                                                                             \
        std::array<GLuint, 4> expected rgba;                                      \
        std::array<GLint, 4> valuei;                                              \
        glGetFramebufferPixelLocalStorageParameterivANGLE(                        \
            plane, GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE, valuei.data()); \
        std::array<GLuint, 4> value;                                              \
        memcpy(value.data(), valuei.data(), sizeof(value));                       \
        EXPECT_EQ(value, expected);                                               \
    }

// Check proper functioning of the clear value state.
TEST_P(PixelLocalStorageTest, ClearState)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex8f(GL_RGBA8, 1, 1);
    PLSTestTexture tex8i(GL_RGBA8I, 1, 1);
    PLSTestTexture tex8ui(GL_RGBA8UI, 1, 1);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex8f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex8i, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex8ui, 0, 0);
    auto clearLoads = GLenumArray({GL_CLEAR_ANGLE, GL_CLEAR_ANGLE, GL_CLEAR_ANGLE});
    auto keepStores = GLenumArray({GL_KEEP, GL_KEEP, GL_KEEP});

    // Clear values are initially zero.
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(0, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(1, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(2, ({0, 0, 0, 0}));
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(0, 0, 0, 0));

    // Test custom RGBA8 clear values.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalClearValuefvANGLE(0, ClearF(100.5, 0, 0, 0));
    glFramebufferPixelLocalClearValueivANGLE(1, ClearI(-1, 2, -3, 4));
    glFramebufferPixelLocalClearValueuivANGLE(2, ClearUI(5, 6, 7, 8));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(0, ({100.5, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(1, ({-1, 2, -3, 4}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(2, ({5, 6, 7, 8}));
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(255, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(-1, 2, -3, 4));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(5, 6, 7, 8));

    // Rotate and test again.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex8ui, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex8f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex8i, 0, 0);
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    // Since each clear value type is separate state, these should all be zero again.
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(0, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(1, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(2, ({0, 0, 0, 0}));
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(0, 0, 0, 0));
    // If any component of the clear value is larger than can be represented in plane's
    // internalformat, it is clamped.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalClearValueuivANGLE(0, ClearUI(254, 255, 256, 257));
    glFramebufferPixelLocalClearValuefvANGLE(1, ClearF(-1, 0, 1, 2));
    glFramebufferPixelLocalClearValueivANGLE(2, ClearI(-129, -128, 127, 128));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(0, ({254, 255, 256, 257}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(1, ({-1, 0, 1, 2}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(2, ({-129, -128, 127, 128}));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(254, 255, 255, 255));
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(0, 0, 255, 255));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(-128, -128, 127, 127));

    // Final rotation.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex8i, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex8ui, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex8f, 0, 0);
    // Since each clear value type is separate state, these should all be zero yet again.
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(0, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(1, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(2, ({0, 0, 0, 0}));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(0, 0, 0, 0));
    // If any component of the clear value is larger than can be represented in plane's
    // internalformat, it is clamped.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalClearValueivANGLE(0, ClearI(999, 999, -999, -999));
    glFramebufferPixelLocalClearValueuivANGLE(1, ClearUI(0, 0, 999, 999));
    glFramebufferPixelLocalClearValuefvANGLE(2, ClearF(999, 999, -999, -999));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(0, ({999, 999, -999, -999}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(1, ({0, 0, 999, 999}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(2, ({999, 999, -999, -999}));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(127, 127, -128, -128));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(0, 0, 255, 255));
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(255, 255, 0, 0));

    // GL_ZERO shouldn't be affected by previous clear colors.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}));
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(0, 0, 0, 0));

    // Cycle back to the original configuration and ensure that clear state hasn't changed.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex8f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex8i, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex8ui, 0, 0);
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(0, ({100.5f, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(1, ({-1, 2, -3, 4}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(2, ({5, 6, 7, 8}));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(255, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(-1, 2, -3, 4));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(5, 6, 7, 8));

    // Clear state is specific to the draw framebuffer; clear values on one framebuffer do not
    // affect clear values on another.
    GLFramebuffer fbo2;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo2);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex8f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex8i, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex8ui, 0, 0);
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(0, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_INT(1, ({0, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(2, ({0, 0, 0, 0}));
    glEndPixelLocalStorageANGLE(3, keepStores);
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, 1, 1, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, 1, 1, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(0, 0, 0, 0));

    // Test custom R32 clear values.
    PLSTestTexture tex32f(GL_R32F);
    PLSTestTexture tex32ui(GL_R32UI);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex32f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex32ui, 0, 0);
    glFramebufferPixelLocalClearValueuivANGLE(2, ClearUI(0xbaadbeef, 1, 1, 0));
    glBeginPixelLocalStorageANGLE(3,
                                  GLenumArray({GL_CLEAR_ANGLE, GL_DISABLE_ANGLE, GL_CLEAR_ANGLE}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_FLOAT(0, ({100.5f, 0, 0, 0}));
    EXPECT_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT(2, ({0xbaadbeef, 1, 1, 0}));
    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_DONT_CARE, GL_KEEP}));
    attachTextureToScratchFBO(tex32f);
    EXPECT_PIXEL_RECT32F_EQ(0, 0, 1, 1, GLColor32F(100.5f, 0, 0, 1));
    attachTextureToScratchFBO(tex32ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, 1, 1, GLColor32UI(0xbaadbeef, 0, 0, 1));
}

// Check proper support of GL_ZERO, GL_KEEP, GL_CLEAR_ANGLE, and GL_DISABLE_ANGLE loadOps. Also
// verify that it works do draw with GL_MAX_LOCAL_STORAGE_PLANES_ANGLE planes.
TEST_P(PixelLocalStorageTest, LoadOps)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    std::stringstream fs;
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "layout(binding=" << i << ", rgba8) uniform highp pixelLocalANGLE pls" << i << ";\n";
    }
    fs << "void main() {\n";
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "pixelLocalStoreANGLE(pls" << i << ", color + pixelLocalLoadANGLE(pls" << i
           << "));\n";
    }
    fs << "}";
    mProgram.compile(fs.str().c_str());

    // Create pls textures and clear them to red.
    glClearColor(1, 0, 0, 1);
    std::vector<PLSTestTexture> texs;
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        texs.emplace_back(GL_RGBA8);
        attachTextureToScratchFBO(texs[i]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Turn on scissor to try and confuse the local storage clear step.
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 20, H);

    // Set up pls color planes with a clear color of black. Odd units load with GL_CLEAR_ANGLE
    // (cleared to black) and even load with GL_KEEP (preserved red).
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    std::vector<GLenum> loadOps(MAX_PIXEL_LOCAL_STORAGE_PLANES);
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        glFramebufferTexturePixelLocalStorageANGLE(i, texs[i], 0, 0);
        glFramebufferPixelLocalClearValuefvANGLE(i, ClearF(0, 0, 0, 1));
        loadOps[i] = (i & 1) ? GL_CLEAR_ANGLE : GL_KEEP;
    }
    std::vector<GLenum> storeOps(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_KEEP);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // Draw transparent green into all pls attachments.
    glBeginPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, loadOps.data());
    mProgram.drawBoxes({{{FULLSCREEN}, {0, 1, 0, 0}}});
    glEndPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, storeOps.data());

    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texs[i], 0);
        // Check that the draw buffers didn't get perturbed by local storage -- GL_COLOR_ATTACHMENT0
        // is currently off, so glClear has no effect. This also verifies that local storage planes
        // didn't get left attached to the framebuffer somewhere with draw buffers on.
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_RECT_EQ(0, 0, 20, H,
                             loadOps[i] == GL_CLEAR_ANGLE ? GLColor(0, 255, 0, 255)
                                                          : /*GL_KEEP*/ GLColor(255, 255, 0, 255));
        // Check that the scissor didn't get perturbed by local storage.
        EXPECT_PIXEL_RECT_EQ(20, 0, W - 20, H,
                             loadOps[i] == GL_CLEAR_ANGLE ? GLColor(0, 0, 0, 255)
                                                          : /*GL_KEEP*/ GLColor(255, 0, 0, 255));
    }

    // Detach the last read pls texture from the framebuffer.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    // Now test GL_DISABLE_ANGLE and GL_ZERO.
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        loadOps[i] = loadOps[i] == GL_CLEAR_ANGLE ? GL_ZERO : GL_DISABLE_ANGLE;
    }

    // Execute a pls pass without a draw.
    glBeginPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, loadOps.data());
    glEndPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, storeOps.data());

    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texs[i], 0);
        if (loadOps[i] == GL_ZERO)
        {
            EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 0, 0));
        }
        else
        {
            EXPECT_PIXEL_RECT_EQ(0, 0, 20, H, GLColor(255, 255, 0, 255));
            EXPECT_PIXEL_RECT_EQ(20, 0, W - 20, H, GLColor(255, 0, 0, 255));
        }
    }

    // Now turn GL_COLOR_ATTACHMENT0 back on and check that the clear color and scissor didn't get
    // perturbed by local storage.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texs[1], 0);
    glDrawBuffers(1, GLenumArray({GL_COLOR_ATTACHMENT0}));
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_RECT_EQ(0, 0, 20, H, GLColor(255, 0, 0, 255));
    EXPECT_PIXEL_RECT_EQ(20, 0, W - 20, H, GLColor(0, 0, 0, 0));

    ASSERT_GL_NO_ERROR();
}

// This next series of tests checks that GL utilities for rejecting fragments prevent stores to PLS:
//
//   * stencil test
//   * depth test
//   * viewport
//
// Some utilities are not legal in ANGLE_shader_pixel_local_storage:
//
//   * gl_SampleMask is disallowed by the spec
//   * discard, after potential calls to pixelLocalLoadANGLE/Store, is disallowed by the spec
//   * pixelLocalLoadANGLE/Store after a return from main is disallowed by the spec
//
// To run the tests, bind a FragmentRejectTestFBO and draw {FRAG_REJECT_TEST_BOX}:
//
//   * {0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT} should be green
//   * Fragments outside should have been rejected, leaving the pixels black
//
struct FragmentRejectTestFBO : GLFramebuffer
{
    FragmentRejectTestFBO(GLuint tex)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, *this);
        glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
        glFramebufferPixelLocalClearValuefvANGLE(0, MakeArray<float>({0, 0, 0, 1}));
        glViewport(0, 0, W, H);
        glDrawBuffers(0, nullptr);
    }
};
constexpr static int FRAG_REJECT_TEST_WIDTH  = 64;
constexpr static int FRAG_REJECT_TEST_HEIGHT = 64;
constexpr static Box FRAG_REJECT_TEST_BOX(FULLSCREEN,
                                          {0, 1, 0, 0},  // draw color
                                          {0, 0, FRAG_REJECT_TEST_WIDTH,
                                           FRAG_REJECT_TEST_HEIGHT});  // reject pixels outside aux1

// Check that the stencil test prevents stores to PLS.
TEST_P(PixelLocalStorageTest, FragmentReject_stencil)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(tex);

    mProgram.compile(R"(
    layout(binding=0, rgba8) uniform highp pixelLocalANGLE pls;
    void main()
    {
        pixelLocalStoreANGLE(pls, color + pixelLocalLoadANGLE(pls));
    })");
    GLuint depthStencil;
    glGenRenderbuffers(1, &depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    // glStencilFunc(GL_NEVER, ...) should not update pls.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NEVER, 1, ~0u);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    GLint zero = 0;
    glClearBufferiv(GL_STENCIL, 0, &zero);
    mProgram.drawBoxes({{{0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT}}});
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
    glDisable(GL_STENCIL_TEST);
    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::black);

    // Stencil should be preserved after PLS, and only pixels that pass the stencil test should
    // update PLS next.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glEnable(GL_STENCIL_TEST);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
    glStencilFunc(GL_NOTEQUAL, 0, ~0u);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    mProgram.drawBoxes({FRAG_REJECT_TEST_BOX});
    glDisable(GL_STENCIL_TEST);
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
    renderTextureToDefaultFramebuffer(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(FRAG_REJECT_TEST_WIDTH, 0, W - FRAG_REJECT_TEST_WIDTH,
                         FRAG_REJECT_TEST_HEIGHT, GLColor::black);
    EXPECT_PIXEL_RECT_EQ(0, FRAG_REJECT_TEST_HEIGHT, W, H - FRAG_REJECT_TEST_HEIGHT,
                         GLColor::black);

    ASSERT_GL_NO_ERROR();
}

// Check that the depth test prevents stores to PLS.
TEST_P(PixelLocalStorageTest, FragmentReject_depth)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(tex);

    mProgram.compile(R"(
    layout(binding=0, rgba8) uniform highp pixelLocalANGLE pls;
    void main()
    {
        pixelLocalStoreANGLE(pls, pixelLocalLoadANGLE(pls) + color);
    })");
    GLuint depth;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
    GLfloat zero = 0;
    glClearBufferfv(GL_DEPTH, 0, &zero);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT);
    GLfloat one = 1;
    glClearBufferfv(GL_DEPTH, 0, &one);
    glDisable(GL_SCISSOR_TEST);
    glDepthFunc(GL_LESS);
    mProgram.drawBoxes({FRAG_REJECT_TEST_BOX});
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
    glDisable(GL_DEPTH_TEST);

    renderTextureToDefaultFramebuffer(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(FRAG_REJECT_TEST_WIDTH, 0, W - FRAG_REJECT_TEST_WIDTH,
                         FRAG_REJECT_TEST_HEIGHT, GLColor::black);
    EXPECT_PIXEL_RECT_EQ(0, FRAG_REJECT_TEST_HEIGHT, W, H - FRAG_REJECT_TEST_HEIGHT,
                         GLColor::black);
    ASSERT_GL_NO_ERROR();
}

// Check that restricting the viewport also restricts stores to PLS.
TEST_P(PixelLocalStorageTest, FragmentReject_viewport)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(tex);

    mProgram.compile(R"(
    layout(binding=0, rgba8) uniform highp pixelLocalANGLE pls;
    void main()
    {
        vec4 dst = pixelLocalLoadANGLE(pls);
        pixelLocalStoreANGLE(pls, color + dst);
    })");
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
    glViewport(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT);
    mProgram.drawBoxes({FRAG_REJECT_TEST_BOX});
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
    glViewport(0, 0, W, H);

    renderTextureToDefaultFramebuffer(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(FRAG_REJECT_TEST_WIDTH, 0, W - FRAG_REJECT_TEST_WIDTH,
                         FRAG_REJECT_TEST_HEIGHT, GLColor::black);
    EXPECT_PIXEL_RECT_EQ(0, FRAG_REJECT_TEST_HEIGHT, W, H - FRAG_REJECT_TEST_HEIGHT,
                         GLColor::black);
    ASSERT_GL_NO_ERROR();
}

// Check that results are only nondeterministic within predictable constraints, and that no data is
// random or leaked from other contexts when we forget to insert a barrier.
TEST_P(PixelLocalStorageTest, ForgetBarrier)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    mProgram.compile(R"(
    layout(binding=0, r32f) uniform highp pixelLocalANGLE framebuffer;
    void main()
    {
        vec4 dst = pixelLocalLoadANGLE(framebuffer);
        pixelLocalStoreANGLE(framebuffer, color + dst * 2.0);
    })");

    // Draw r=100, one pixel at a time, in random order.
    constexpr static int NUM_PIXELS = H * W;
    std::vector<Box> boxesA_100;
    int pixelIdx = 0;
    for (int i = 0; i < NUM_PIXELS; ++i)
    {
        int iy  = pixelIdx / W;
        float y = iy;
        int ix  = pixelIdx % W;
        float x = ix;
        pixelIdx =
            (pixelIdx + 69484171) % NUM_PIXELS;  // Prime numbers guarantee we hit every pixel once.
        boxesA_100.push_back(Box{{x, y, x + 1, y + 1}, {100, 0, 0, 0}});
    }

    // Draw r=7, one pixel at a time, in random order.
    std::vector<Box> boxesB_7;
    for (int i = 0; i < NUM_PIXELS; ++i)
    {
        int iy  = pixelIdx / W;
        float y = iy;
        int ix  = pixelIdx % W;
        float x = ix;
        pixelIdx =
            (pixelIdx + 97422697) % NUM_PIXELS;  // Prime numbers guarantee we hit every pixel once.
        boxesB_7.push_back(Box{{x, y, x + 1, y + 1}, {7, 0, 0, 0}});
    }

    PLSTestTexture tex(GL_R32F);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    glFramebufferPixelLocalClearValuefvANGLE(0, ClearF(1, 0, 0, 0));
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // First make sure it works properly with a barrier.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
    mProgram.drawBoxes(boxesA_100, UseBarriers::No);
    glPixelLocalStorageBarrierANGLE();
    mProgram.drawBoxes(boxesB_7, UseBarriers::No);
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT32F_EQ(0, 0, W, H, GLColor32F(211, 0, 0, 1));

    ASSERT_GL_NO_ERROR();

    // Vulkan generates rightful "SYNC-HAZARD-READ_AFTER_WRITE" validation errors when we omit the
    // barrier.
    ANGLE_SKIP_TEST_IF(IsVulkan() &&
                       !IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage_coherent"));

    // Now forget to insert the barrier and ensure our nondeterminism still falls within predictable
    // constraints.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
    mProgram.drawBoxes(boxesA_100, UseBarriers::No);
    // OOPS! We forgot to insert a barrier!
    mProgram.drawBoxes(boxesB_7, UseBarriers::No);
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));

    float pixels[H * W * 4];
    attachTextureToScratchFBO(tex);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, pixels);
    for (int r = 0; r < NUM_PIXELS * 4; r += 4)
    {
        // When two fragments, A and B, touch a pixel, there are 6 possible orderings of operations:
        //
        //   * Read A, Write A, Read B, Write B
        //   * Read B, Write B, Read A, Write A
        //   * Read A, Read B, Write A, Write B
        //   * Read A, Read B, Write B, Write A
        //   * Read B, Read A, Write B, Write A
        //   * Read B, Read A, Write A, Write B
        //
        // Which (assumimg the read and/or write operations themselves are atomic), is equivalent to
        // 1 of 4 potential effects:
        bool isAcceptableValue = pixels[r] == 211 ||  // A, then B  (  7 + (100 + 1 * 2) * 2 == 211)
                                 pixels[r] == 118 ||  // B, then A  (100 + (  7 + 1 * 2) * 2 == 118)
                                 pixels[r] == 102 ||  // A only     (100 +             1 * 2 == 102)
                                 pixels[r] == 9;
        if (!isAcceptableValue)
        {
            printf(__FILE__ "(%i): UNACCEPTABLE value at pixel location [%i, %i]\n", __LINE__,
                   (r / 4) % W, (r / 4) / W);
            printf("              Got: %f\n", pixels[r]);
            printf("  Expected one of: { 211, 118, 102, 9 }\n");
        }
        ASSERT_TRUE(isAcceptableValue);
    }

    ASSERT_GL_NO_ERROR();
}

// Check loading and storing from memoryless local storage planes.
TEST_P(PixelLocalStorageTest, MemorylessStorage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    // Bind the texture, but don't call glTexStorage until after creating the memoryless plane.
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    // Create a memoryless plane.
    glFramebufferMemorylessPixelLocalStorageANGLE(1, GL_RGBA8);
    // Define the persistent texture now, after attaching the memoryless pixel local storage. This
    // verifies that the GL_TEXTURE_2D binding doesn't get perturbed by local storage.
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, W, H);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    PLSProgram drawMemorylessProgram;
    drawMemorylessProgram.compile(R"(
    layout(binding=0, rgba8) uniform highp pixelLocalANGLE framebuffer;
    layout(binding=1, rgba8) uniform highp pixelLocalANGLE memoryless;
    void main()
    {
        pixelLocalStoreANGLE(memoryless, color + pixelLocalLoadANGLE(memoryless));
    })");

    PLSProgram transferToTextureProgram;
    transferToTextureProgram.compile(R"(
    layout(binding=0, rgba8) uniform highp pixelLocalANGLE framebuffer;
    layout(binding=1, rgba8) uniform highp pixelLocalANGLE memoryless;
    void main()
    {
        pixelLocalStoreANGLE(framebuffer, vec4(1) - pixelLocalLoadANGLE(memoryless));
    })");

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));

    // Draw into memoryless storage.
    drawMemorylessProgram.bind();
    drawMemorylessProgram.drawBoxes({{{0, 20, W, H}, {1, 0, 0, 0}},
                                     {{0, 40, W, H}, {0, 1, 0, 0}},
                                     {{0, 60, W, H}, {0, 0, 1, 0}}});

    ASSERT_GL_NO_ERROR();

    // Transfer to a texture.
    transferToTextureProgram.bind();
    transferToTextureProgram.drawBoxes({{FULLSCREEN}});

    glEndPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_DONT_CARE}));

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 60, W, H - 60, GLColor(0, 0, 0, 255));
    EXPECT_PIXEL_RECT_EQ(0, 40, W, 20, GLColor(0, 0, 255, 255));
    EXPECT_PIXEL_RECT_EQ(0, 20, W, 20, GLColor(0, 255, 255, 255));
    EXPECT_PIXEL_RECT_EQ(0, 0, W, 20, GLColor(255, 255, 255, 255));

    // Ensure the GL_TEXTURE_2D binding still hasn't been perturbed by local storage.
    GLint textureBinding2D;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding2D);
    ASSERT_EQ((GLuint)textureBinding2D, tex);

    ASSERT_GL_NO_ERROR();
}

// Check that it works to render with the maximum supported data payload:
//
//   GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE
//
TEST_P(PixelLocalStorageTest, MaxCombinedDrawBuffersAndPLSPlanes)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    for (int numDrawBuffers : {0, 1, MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES - 1})
    {
        numDrawBuffers =
            std::min(numDrawBuffers, MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE);
        int numPLSPlanes =
            MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES - numDrawBuffers;
        numPLSPlanes = std::min(numPLSPlanes, MAX_PIXEL_LOCAL_STORAGE_PLANES);

        std::stringstream fs;
        for (int i = 0; i < numPLSPlanes; ++i)
        {
            fs << "layout(binding=" << i << ", rgba8ui) uniform highp upixelLocalANGLE pls" << i
               << ";\n";
        }
        for (int i = 0; i < numDrawBuffers; ++i)
        {
            if (numDrawBuffers > 1)
            {
                fs << "layout(location=" << i << ") ";
            }
            fs << "out uvec4 out" << i << ";\n";
        }
        fs << "void main() {\n";
        int c = 0;
        for (; c < std::min(numPLSPlanes, numDrawBuffers); ++c)
        {
            // Nest an expression involving a fragment output, which will have an unspecified
            // location when numDrawBuffers is 1, in pixelLocalStoreANGLE().
            fs << "pixelLocalStoreANGLE(pls" << c << ", uvec4(color) - (out" << c
               << " = uvec4(aux1) + uvec4(" << c << ")));\n";
        }
        for (int i = c; i < numPLSPlanes; ++i)
        {
            fs << "pixelLocalStoreANGLE(pls" << i << ", uvec4(color) - (uvec4(aux1) + uvec4(" << i
               << ")));\n";
        }
        for (int i = c; i < numDrawBuffers; ++i)
        {
            fs << "out" << i << " = uvec4(aux1) + uvec4(" << i << ");\n";
        }
        fs << "}";
        mProgram.compile(fs.str().c_str());

        glViewport(0, 0, W, H);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        std::vector<PLSTestTexture> localTexs;
        localTexs.reserve(numPLSPlanes);
        for (int i = 0; i < numPLSPlanes; ++i)
        {
            localTexs.emplace_back(GL_RGBA8UI);
            glFramebufferTexturePixelLocalStorageANGLE(i, localTexs[i], 0, 0);
        }
        std::vector<PLSTestTexture> renderTexs;
        renderTexs.reserve(numDrawBuffers);
        std::vector<GLenum> drawBuffers(numDrawBuffers);
        for (int i = 0; i < numDrawBuffers; ++i)
        {
            renderTexs.emplace_back(GL_RGBA32UI);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
                                   renderTexs[i], 0);
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }
        glDrawBuffers(drawBuffers.size(), drawBuffers.data());

        glBeginPixelLocalStorageANGLE(numPLSPlanes,
                                      std::vector<GLenum>(numPLSPlanes, GL_ZERO).data());
        mProgram.drawBoxes({{FULLSCREEN, {255, 254, 253, 252}, {0, 1, 2, 3}}});
        glEndPixelLocalStorageANGLE(numPLSPlanes,
                                    std::vector<GLenum>(numPLSPlanes, GL_KEEP).data());

        for (int i = 0; i < numPLSPlanes; ++i)
        {
            attachTextureToScratchFBO(localTexs[i]);
            EXPECT_PIXEL_RECT32UI_EQ(
                0, 0, W, H,
                GLColor32UI(255u - i - 0u, 254u - i - 1u, 253u - i - 2u, 252u - i - 3u));
        }
        for (int i = 0; i < numDrawBuffers; ++i)
        {
            attachTextureToScratchFBO(renderTexs[i]);
            EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0u + i, 1u + i, 2u + i, 3u + i));
        }

        ASSERT_GL_NO_ERROR();
    }
}

// Check that pls is preserved when a shader does not call pixelLocalStoreANGLE(). (Whether that's
// because a conditional branch failed or because the shader didn't write to it at all.)
//
//   * The framebuffer fetch implementation needs to make sure every active plane's output variable
//     gets written during every invocation, or else its value will become undefined.
//
//   * The native pixel local storage implementation needs to declare a variable for every active
//     plane, even if it is unused in a particular shader invocation.
//
// Also check that a pixelLocalLoadANGLE() of an r32f texture returns (r, 0, 0, 1).
TEST_P(PixelLocalStorageTest, LoadOnly)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex(GL_RGBA8);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_R32F);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 0, 0);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // Pass 1: draw to memoryless conditionally.
    PLSProgram pass1;
    pass1.compile(R"(
    layout(binding=0, r32f) uniform highp pixelLocalANGLE memoryless;
    layout(binding=1, rgba8) uniform highp pixelLocalANGLE tex;
    void main()
    {
        // Omit braces on the 'if' to ensure proper insertion of memoryBarriers in the translator.
        if (gl_FragCoord.x < 64.0)
            pixelLocalStoreANGLE(memoryless, vec4(1, -.1, .2, -.3));  // Only stores r.
    })");

    // Pass 2: draw to tex conditionally.
    // Don't touch memoryless -- make sure it gets preserved!
    PLSProgram pass2;
    pass2.compile(R"(
    layout(binding=0, r32f) uniform highp pixelLocalANGLE memoryless;
    layout(binding=1, rgba8) uniform highp pixelLocalANGLE tex;
    void main()
    {
        // Omit braces on the 'if' to ensure proper insertion of memoryBarriers in the translator.
        if (gl_FragCoord.y < 64.0)
            pixelLocalStoreANGLE(tex, vec4(0, 1, 1, 0));
    })");

    // Pass 3: combine memoryless and tex.
    PLSProgram pass3;
    pass3.compile(R"(
    layout(binding=0, r32f) uniform highp pixelLocalANGLE memoryless;
    layout(binding=1, rgba8) uniform highp pixelLocalANGLE tex;
    void main()
    {
        pixelLocalStoreANGLE(tex, pixelLocalLoadANGLE(tex) + pixelLocalLoadANGLE(memoryless));
    })");

    // Leave unit 0 with the default clear value of zero.
    glFramebufferPixelLocalClearValuefvANGLE(0, ClearF(0, 0, 0, 0));
    glFramebufferPixelLocalClearValuefvANGLE(1, ClearF(0, 1, 0, 0));
    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_CLEAR_ANGLE, GL_CLEAR_ANGLE}));

    pass1.bind();
    pass1.drawBoxes({{FULLSCREEN}});

    pass2.bind();
    pass2.drawBoxes({{FULLSCREEN}});

    pass3.bind();
    pass3.drawBoxes({{FULLSCREEN}});

    glEndPixelLocalStorageANGLE(2, GLenumArray({GL_DONT_CARE, GL_KEEP}));

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, 64, 64, GLColor(255, 255, 255, 255));
    EXPECT_PIXEL_RECT_EQ(64, 0, W - 64, 64, GLColor(0, 255, 255, 255));
    EXPECT_PIXEL_RECT_EQ(0, 64, 64, H - 64, GLColor(255, 255, 0, 255));
    EXPECT_PIXEL_RECT_EQ(64, 64, W - 64, H - 64, GLColor(0, 255, 0, 255));

    ASSERT_GL_NO_ERROR();

    // Now treat "tex" as entirely readonly for an entire local storage render pass.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture tex2(GL_RGBA8);

    if (MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE > 0)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);
        glDrawBuffers(1, GLenumArray({GL_COLOR_ATTACHMENT0}));
        glClear(GL_COLOR_BUFFER_BIT);

        mProgram.compile(R"(
        layout(binding=1, rgba8) uniform highp pixelLocalANGLE tex;
        out vec4 fragcolor;
        void main()
        {
            fragcolor = 1.0 - pixelLocalLoadANGLE(tex);
        })");

        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_DISABLE_ANGLE, GL_KEEP}));
    }
    else
    {
        glFramebufferTexturePixelLocalStorageANGLE(2, tex2, 0, 0);

        mProgram.compile(R"(
        layout(binding=1, rgba8) uniform highp pixelLocalANGLE tex;
        layout(binding=2, rgba8) uniform highp pixelLocalANGLE fragcolor;
        void main()
        {
            pixelLocalStoreANGLE(fragcolor, 1.0 - pixelLocalLoadANGLE(tex));
        })");

        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_DISABLE_ANGLE, GL_KEEP, GL_ZERO}));
    }

    mProgram.drawBoxes({{FULLSCREEN}});

    int n;
    glGetIntegerv(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, &n);
    glEndPixelLocalStorageANGLE(n, GLenumArray({GL_DONT_CARE, GL_KEEP, GL_KEEP}));

    // Ensure "tex" was properly read in the shader.
    if (MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE == 0)
    {
        attachTextureToScratchFBO(tex2);
    }
    EXPECT_PIXEL_RECT_EQ(0, 0, 64, 64, GLColor(0, 0, 0, 0));
    EXPECT_PIXEL_RECT_EQ(64, 0, W - 64, 64, GLColor(255, 0, 0, 0));
    EXPECT_PIXEL_RECT_EQ(0, 64, 64, H - 64, GLColor(0, 0, 255, 0));
    EXPECT_PIXEL_RECT_EQ(64, 64, W - 64, H - 64, GLColor(255, 0, 255, 0));

    // Ensure "tex" was preserved after the shader.
    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, 64, 64, GLColor(255, 255, 255, 255));
    EXPECT_PIXEL_RECT_EQ(64, 0, W - 64, 64, GLColor(0, 255, 255, 255));
    EXPECT_PIXEL_RECT_EQ(0, 64, 64, H - 64, GLColor(255, 255, 0, 255));
    EXPECT_PIXEL_RECT_EQ(64, 64, W - 64, H - 64, GLColor(0, 255, 0, 255));

    ASSERT_GL_NO_ERROR();
}

// Check that stores and loads in a single shader invocation are coherent.
TEST_P(PixelLocalStorageTest, LoadAfterStore)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    // Run a fibonacci loop that stores and loads the same PLS multiple times.
    mProgram.compile(R"(
    layout(binding=0, rgba8ui) uniform highp upixelLocalANGLE fibonacci;
    void main()
    {
        pixelLocalStoreANGLE(fibonacci, uvec4(1, 0, 0, 0));  // fib(1, 0, 0, 0)
        for (int i = 0; i < 3; ++i)
        {
            uvec4 fib0 = pixelLocalLoadANGLE(fibonacci);
            uvec4 fib1;
            fib1.w = fib0.x + fib0.y;
            fib1.z = fib1.w + fib0.x;
            fib1.y = fib1.z + fib1.w;
            fib1.x = fib1.y + fib1.z;  // fib(i*4 + (5, 4, 3, 2))
            pixelLocalStoreANGLE(fibonacci, fib1);
        }
        // fib is at indices (13, 12, 11, 10)
    })");

    PLSTestTexture tex(GL_RGBA8UI);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
    mProgram.drawBoxes({{FULLSCREEN}});
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(233, 144, 89, 55));  // fib(13, 12, 11, 10)

    ASSERT_GL_NO_ERROR();

    // Now verify that r32f and r32ui still reload as (r, 0, 0, 1), even after an in-shader store.
    if (MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE > 0)
    {
        mProgram.compile(R"(
        layout(binding=0, r32f) uniform highp pixelLocalANGLE pls32f;
        layout(binding=1, r32ui) uniform highp upixelLocalANGLE pls32ui;

        out vec4 fragcolor;
        void main()
        {
            pixelLocalStoreANGLE(pls32f, vec4(1, .5, .5, .5));
            pixelLocalStoreANGLE(pls32ui, uvec4(1, 1, 1, 0));
            if ((int(floor(gl_FragCoord.x)) & 1) == 0)
                fragcolor = pixelLocalLoadANGLE(pls32f);
            else
                fragcolor = vec4(pixelLocalLoadANGLE(pls32ui));
        })");

        tex.reset(GL_RGBA8);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_R32F);
        glFramebufferMemorylessPixelLocalStorageANGLE(1, GL_R32UI);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glDrawBuffers(1, GLenumArray({GL_COLOR_ATTACHMENT0}));

        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));
    }
    else
    {
        mProgram.compile(R"(
        layout(binding=0, r32f) uniform highp pixelLocalANGLE pls32f;
        layout(binding=1, r32ui) uniform highp upixelLocalANGLE pls32ui;
        layout(binding=2, rgba8) uniform highp pixelLocalANGLE fragcolor;
        void main()
        {
            pixelLocalStoreANGLE(pls32f, vec4(1, .5, .5, .5));
            pixelLocalStoreANGLE(pls32ui, uvec4(1, 1, 1, 0));
            if ((int(floor(gl_FragCoord.x)) & 1) == 0)
                pixelLocalStoreANGLE(fragcolor, pixelLocalLoadANGLE(pls32f));
            else
                pixelLocalStoreANGLE(fragcolor, vec4(pixelLocalLoadANGLE(pls32ui)));
        })");

        tex.reset(GL_RGBA8);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_R32F);
        glFramebufferMemorylessPixelLocalStorageANGLE(1, GL_R32UI);
        glFramebufferTexturePixelLocalStorageANGLE(2, tex, 0, 0);

        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_DONT_CARE}));
    }

    mProgram.drawBoxes({{FULLSCREEN}});

    int n;
    glGetIntegerv(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, &n);
    glEndPixelLocalStorageANGLE(n, GLenumArray({GL_DONT_CARE, GL_DONT_CARE, GL_KEEP}));

    if (MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE == 0)
    {
        attachTextureToScratchFBO(tex);
    }
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(255, 0, 0, 255));
    ASSERT_GL_NO_ERROR();
}

// Check that PLS handles can be passed as function arguments.
TEST_P(PixelLocalStorageTest, FunctionArguments)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    mProgram.compile(R"(
    layout(binding=0, rgba8) uniform lowp pixelLocalANGLE dst;
    layout(binding=1, rgba8) uniform mediump pixelLocalANGLE src1;
    void store2(lowp pixelLocalANGLE d);
    void store(highp pixelLocalANGLE d, lowp pixelLocalANGLE s)
    {
        pixelLocalStoreANGLE(d, pixelLocalLoadANGLE(s));
    }
    void main()
    {
        if (gl_FragCoord.x < 25.0)
            store(dst, src1);
        else
            store2(dst);
    }
    // Ensure inlining still works on a uniform declared after main().
    layout(binding=2, r32f) uniform highp pixelLocalANGLE src2;
    void store2(lowp pixelLocalANGLE d)
    {
        pixelLocalStoreANGLE(d, pixelLocalLoadANGLE(src2));
    })");

    PLSTestTexture dst(GL_RGBA8);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, dst, 0, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(1, GL_RGBA8);

    // [ANGLE_shader_pixel_local_storage] Section 4.4.2.X "Configuring Pixel Local Storage
    // on a Framebuffer": When a texture object is deleted, any pixel local storage plane to
    // which it was bound is automatically converted to a memoryless plane of matching
    // internalformat.
    {
        PLSTestTexture tempTex(GL_R32F, 1, 1);
        glFramebufferTexturePixelLocalStorageANGLE(2, tempTex, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // ~PLSTestTexture deletes the texture and converts plane #2 to memoryless.
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glFramebufferPixelLocalClearValuefvANGLE(0, ClearF(1, 1, 1, 1));  // ignored
    glFramebufferPixelLocalClearValuefvANGLE(1, ClearF(0, 1, 1, 0));
    glFramebufferPixelLocalClearValuefvANGLE(2, ClearF(1, 0, 0, 1));
    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_CLEAR_ANGLE, GL_CLEAR_ANGLE}));
    mProgram.drawBoxes({{FULLSCREEN}});
    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_DONT_CARE, GL_DONT_CARE}));

    attachTextureToScratchFBO(dst);
    EXPECT_PIXEL_RECT_EQ(0, 0, 25, H, GLColor(0, 255, 255, 0));
    EXPECT_PIXEL_RECT_EQ(25, 0, W - 25, H, GLColor(255, 0, 0, 255));

    ASSERT_GL_NO_ERROR();
}

// Check that if the "_coherent" extension is advertised, PLS operations are ordered and coherent.
TEST_P(PixelLocalStorageTest, Coherency)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    mProgram.compile(R"(
    layout(binding=0, rgba8ui) uniform lowp upixelLocalANGLE framebuffer;
    layout(binding=1, rgba8) uniform lowp pixelLocalANGLE tmp;
    // The application shouldn't be able to override internal synchronization functions used by
    // the compiler.
    //
    // If the compiler accidentally calls any of these functions, stomp out the framebuffer to make
    // the test fail.
    void endInvocationInterlockNV() { pixelLocalStoreANGLE(framebuffer, uvec4(0)); }
    void beginFragmentShaderOrderingINTEL() { pixelLocalStoreANGLE(framebuffer, uvec4(0)); }
    void beginInvocationInterlockARB() { pixelLocalStoreANGLE(framebuffer, uvec4(0)); }

    // Give these functions a side effect so they don't get pruned, then call them from main().
    void beginInvocationInterlockNV() { pixelLocalStoreANGLE(tmp, vec4(0)); }
    void endInvocationInterlockARB() { pixelLocalStoreANGLE(tmp, vec4(0)); }

    void main()
    {
        highp uvec4 d = pixelLocalLoadANGLE(framebuffer) >> 1;
        pixelLocalStoreANGLE(framebuffer, uvec4(color) + d);
        beginInvocationInterlockNV();
        endInvocationInterlockARB();
    })");

    PLSTestTexture tex(GL_RGBA8UI);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(1, GL_RGBA8);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    std::vector<uint8_t> expected(H * W * 4);
    memset(expected.data(), 0, H * W * 4);

    // This test times out on Swiftshader and noncoherent backends if we draw anywhere near the
    // same number of boxes as we do on coherent, hardware backends.
    int boxesPerList = !IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage_coherent") ||
                               strstr((const char *)glGetString(GL_RENDERER), "SwiftShader")
                           ? 200
                           : H * W * 3;

    // Prepare a ton of random sized boxes in various draws.
    std::vector<Box> boxesList[5];
    srand(17);
    uint32_t boxID = 1;
    for (auto &boxes : boxesList)
    {
        for (int i = 0; i < boxesPerList; ++i)
        {
            // Define a box.
            int w     = rand() % 10 + 1;
            int h     = rand() % 10 + 1;
            float x   = rand() % (W - w);
            float y   = rand() % (H - h);
            uint8_t r = boxID & 0x7f;
            uint8_t g = (boxID >> 7) & 0x7f;
            uint8_t b = (boxID >> 14) & 0x7f;
            uint8_t a = (boxID >> 21) & 0x7f;
            ++boxID;
            // Update expectations.
            for (int yy = y; yy < y + h; ++yy)
            {
                for (int xx = x; xx < x + w; ++xx)
                {
                    int p           = (yy * W + xx) * 4;
                    expected[p]     = r + (expected[p] >> 1);
                    expected[p + 1] = g + (expected[p + 1] >> 1);
                    expected[p + 2] = b + (expected[p + 2] >> 1);
                    expected[p + 3] = a + (expected[p + 3] >> 1);
                }
            }
            // Set up the gpu draw.
            float x0 = x;
            float x1 = x + w;
            float y0 = y;
            float y1 = y + h;
            // Allow boxes to have negative widths and heights. This adds randomness by making the
            // diagonals go in different directions.
            if (rand() & 1)
                std::swap(x0, x1);
            if (rand() & 1)
                std::swap(y0, y1);
            boxes.push_back({{x0, y0, x1, y1}, {(float)r, (float)g, (float)b, (float)a}});
        }
    }

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));
    for (const std::vector<Box> &boxes : boxesList)
    {
        mProgram.drawBoxes(boxes);
    }
    glEndPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_DONT_CARE}));

    attachTextureToScratchFBO(tex);
    std::vector<uint8_t> actual(H * W * 4);
    glReadPixels(0, 0, W, H, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, actual.data());
    EXPECT_EQ(expected, actual);

    ASSERT_GL_NO_ERROR();
}

// Check that binding mipmap levels to PLS is supported.
TEST_P(PixelLocalStorageTest, MipMapLevels)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    mProgram.compile(R"(
        layout(binding=0, rgba8) uniform lowp pixelLocalANGLE pls;
        void main()
        {
            pixelLocalStoreANGLE(pls, color + pixelLocalLoadANGLE(pls));
        })");

    constexpr int LEVELS = 3;
    int levelWidth = 179, levelHeight = 313;
    std::vector<GLColor> redData(levelHeight * levelWidth, GLColor::black);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, LEVELS, GL_RGBA8, levelWidth, levelHeight);

    GLFramebuffer fbo;
    for (int level = 0; level < LEVELS; ++level)
    {
        if (IsVulkan())
        {
            // anglebug.com/7647 -- a workaround is to create and bind a new texture.
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexStorage2D(GL_TEXTURE_2D, LEVELS, GL_RGBA8, 179, 313);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glUniform1f(mProgram.widthUniform(), levelWidth);
        glUniform1f(mProgram.heightUniform(), levelHeight);
        glViewport(0, 0, levelWidth, levelHeight);
        glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, levelWidth, levelHeight, GL_RGBA,
                        GL_UNSIGNED_BYTE, redData.data());

        glFramebufferTexturePixelLocalStorageANGLE(0, tex, level, 0);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
        mProgram.drawBoxes({{{0, 0, (float)levelWidth - 3, (float)levelHeight}, {0, 0, 1, 0}},
                            {{0, 0, (float)levelWidth - 2, (float)levelHeight}, {0, 1, 0, 0}},
                            {{0, 0, (float)levelWidth - 1, (float)levelHeight}, {1, 0, 0, 0}}});
        glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
        attachTextureToScratchFBO(tex, level);
        EXPECT_PIXEL_RECT_EQ(0, 0, levelWidth - 3, levelHeight, GLColor::white);
        EXPECT_PIXEL_RECT_EQ(levelWidth - 3, 0, 1, levelHeight, GLColor::yellow);
        EXPECT_PIXEL_RECT_EQ(levelWidth - 2, 0, 1, levelHeight, GLColor::red);
        EXPECT_PIXEL_RECT_EQ(levelWidth - 1, 0, 1, levelHeight, GLColor::black);

        levelWidth >>= 1;
        levelHeight >>= 1;
        ASSERT_GL_NO_ERROR();
    }

    // Delete fbo.
    // Don't delete tex -- exercise pixel local storage in a way that it has to clean itself up when
    // the context is torn down. (It has internal assertions that validate it is torn down
    // correctly.)
}

void PixelLocalStorageTest::doStateRestorationTest()
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    // Setup state.
    PLSTestTexture plsTex(GL_RGBA8UI, 32, 33);
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glViewport(1, 1, 5, 5);
    glScissor(2, 2, 4, 4);
    glEnable(GL_SCISSOR_TEST);

    bool hasDrawBuffersIndexedOES = IsGLExtensionEnabled("GL_OES_draw_buffers_indexed");
    if (hasDrawBuffersIndexedOES)
    {
        for (int i = 0; i < MAX_DRAW_BUFFERS; ++i)
        {
            if (i % 2 == 1)
            {
                glEnableiOES(GL_BLEND, i);
            }
            glColorMaskiOES(i, i % 3 == 0, i % 3 == 1, i % 3 == 2, i % 2 == 0);
        }
    }
    else
    {
        glEnable(GL_BLEND);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    }

    std::vector<GLenum> drawBuffers(MAX_DRAW_BUFFERS);
    for (int i = 0; i < MAX_DRAW_BUFFERS; ++i)
    {
        drawBuffers[i] = i % 3 == 0 ? GL_NONE : GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(MAX_DRAW_BUFFERS, drawBuffers.data());

    GLenum imageAccesses[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};
    GLenum imageFormats[]  = {GL_RGBA8, GL_R32UI, GL_R32I, GL_R32F};
    std::vector<GLTexture> images;
    if (isContextVersionAtLeast(3, 1))
    {
        for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
        {
            GLuint tex = images.emplace_back();
            glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 3, GL_RGBA8, 8, 8, 5);
            GLboolean layered = i % 2;
            glBindImageTexture(i, images.back(), i % 3, layered, layered == GL_FALSE ? i % 5 : 0,
                               imageAccesses[i % 3], imageFormats[i % 4]);
        }

        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 17);
        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 1);
    }

    PLSTestTexture boundTex(GL_RGBA8, 1, 1);
    glBindTexture(GL_TEXTURE_2D, boundTex);

    // Run pixel local storage.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8);
    glFramebufferTexturePixelLocalStorageANGLE(1, plsTex, 0, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(2, GL_RGBA8);
    glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_RGBA8);
    glFramebufferPixelLocalClearValuefvANGLE(2, ClearF(.1, .2, .3, .4));
    glBeginPixelLocalStorageANGLE(4, GLenumArray({GL_ZERO, GL_KEEP, GL_CLEAR_ANGLE, GL_DONT_CARE}));
    glPixelLocalStorageBarrierANGLE();
    glEndPixelLocalStorageANGLE(4,
                                GLenumArray({GL_DONT_CARE, GL_KEEP, GL_DONT_CARE, GL_DONT_CARE}));

    // Check state.
    GLint textureBinding2D;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding2D);
    EXPECT_EQ(static_cast<GLuint>(textureBinding2D), boundTex);

    if (isContextVersionAtLeast(3, 1))
    {
        for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
        {
            GLint name, level, layer, access, format;
            GLboolean layered;
            glGetIntegeri_v(GL_IMAGE_BINDING_NAME, i, &name);
            glGetIntegeri_v(GL_IMAGE_BINDING_LEVEL, i, &level);
            glGetBooleani_v(GL_IMAGE_BINDING_LAYERED, i, &layered);
            glGetIntegeri_v(GL_IMAGE_BINDING_LAYER, i, &layer);
            glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, i, &access);
            glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, i, &format);
            EXPECT_EQ(static_cast<GLuint>(name), images[i]);
            EXPECT_EQ(level, i % 3);
            EXPECT_EQ(layered, i % 2);
            EXPECT_EQ(layer, layered == GL_FALSE ? i % 5 : 0);
            EXPECT_EQ(static_cast<GLuint>(access), imageAccesses[i % 3]);
            EXPECT_EQ(static_cast<GLuint>(format), imageFormats[i % 4]);
        }

        GLint defaultWidth, defaultHeight;
        glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                    &defaultWidth);
        glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                    &defaultHeight);
        EXPECT_EQ(defaultWidth, 17);
        EXPECT_EQ(defaultHeight, 1);
    }

    for (int i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
    {
        GLint attachmentType;
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                              &attachmentType);
        EXPECT_EQ(attachmentType, GL_NONE);
    }

    for (int i = 0; i < MAX_DRAW_BUFFERS; ++i)
    {
        GLint drawBuffer;
        glGetIntegerv(GL_DRAW_BUFFER0 + i, &drawBuffer);
        if (i % 3 == 0)
        {
            EXPECT_EQ(drawBuffer, GL_NONE);
        }
        else
        {
            EXPECT_EQ(drawBuffer, GL_COLOR_ATTACHMENT0 + i);
        }
    }

    if (hasDrawBuffersIndexedOES)
    {
        for (int i = 0; i < MAX_DRAW_BUFFERS; ++i)
        {
            EXPECT_EQ(glIsEnablediOES(GL_BLEND, i), i % 2 == 1);
            // glGetBooleani_v is not in ES3.
            using ColorMaskInt = std::array<GLint, 4>;
            ColorMaskInt mask;
            glGetIntegeri_v(GL_COLOR_WRITEMASK, i, mask.data());
            EXPECT_EQ(mask, (ColorMaskInt{i % 3 == 0, i % 3 == 1, i % 3 == 2, i % 2 == 0}));
        }
    }
    else
    {
        EXPECT_TRUE(glIsEnabled(GL_BLEND));
        using ColorMask = std::array<GLboolean, 4>;
        ColorMask mask;
        glGetBooleanv(GL_COLOR_WRITEMASK, mask.data());
        EXPECT_EQ(mask, (ColorMask{GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE}));
    }

    EXPECT_TRUE(glIsEnabled(GL_SCISSOR_TEST));

    std::array<GLint, 4> scissorBox;
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox.data());
    EXPECT_EQ(scissorBox, (std::array<GLint, 4>{2, 2, 4, 4}));

    std::array<GLint, 4> viewport;
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    EXPECT_EQ(viewport, (std::array<GLint, 4>{1, 1, 5, 5}));

    GLint drawFramebuffer;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
    EXPECT_EQ(static_cast<GLuint>(drawFramebuffer), fbo);

    ASSERT_GL_NO_ERROR();
}

// Check that application-facing ES3 state is not perturbed by pixel local storage.
TEST_P(PixelLocalStorageTest, StateRestoration)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    doStateRestorationTest();
}

void PixelLocalStorageTest::doDrawStateTest()
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_ONE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT_AND_BACK);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_NEVER);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1e9f, 1e9f);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 1, 1);

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NEVER, 0, 0);

    if (isContextVersionAtLeast(3, 1))
    {
        glEnable(GL_SAMPLE_MASK);
        glSampleMaski(0, 0);
    }

    glViewport(0, 0, 1, 1);

    glClearColor(.1f, .2f, .3f, .4f);

    // Issue a draw to ensure GL state gets synced.
    PLSTestTexture tex(GL_RGBA8);
    renderTextureToDefaultFramebuffer(tex);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint depthStencil;
    glGenRenderbuffers(1, &depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);

    mProgram.compile(R"(
        layout(binding=0, rgba8) uniform lowp pixelLocalANGLE pls;
        void main()
        {
            pixelLocalStoreANGLE(pls, vec4(1, 0, 0, 1));
        })");

    // Clear to green. This should work regardless of draw state.
    glFramebufferPixelLocalClearValuefvANGLE(0, ClearF(0, 1, 0, 1));
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));

    // Draw a red box. This should not go through because the blocking draw state should have been
    // restored after the clear.
    mProgram.drawBoxes({{FULLSCREEN}});

    // Store PLS to the texture. This should work again even though the blocking draw state was
    // synced for the previous draw.
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 255, 0, 255));

    ASSERT_GL_NO_ERROR();
}

// Check that draw state does not affect PLS loads and stores, particularly for
// EXT_shader_pixel_local_storage, where they are implemented as fullscreen draws.
TEST_P(PixelLocalStorageTest, DrawStateReset)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    doDrawStateTest();
}

// Check that blend and color mask state do not affect pixel local storage, and that PLS does not
// affect blend or color mask on the application's draw buffers.
TEST_P(PixelLocalStorageTest, BlendAndColorMask)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex1(GL_RGBA8);
    PLSTestTexture tex2(GL_RGBA8);
    PLSTestTexture tex3(GL_RGBA8);
    PLSTestTexture tex4(GL_RGBA8);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, W, H);

    if (!IsGLExtensionEnabled("GL_OES_draw_buffers_indexed") ||
        MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE < 2)
    {
        // Blend should not affect pixel local storage.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ONE);

        // Color mask should not affect pixel local storage.
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        mProgram.compile(R"(
        layout(binding=0, rgba8) uniform lowp pixelLocalANGLE pls1;
        layout(binding=1, rgba8) uniform lowp pixelLocalANGLE pls2;
        layout(binding=2, rgba8) uniform lowp pixelLocalANGLE pls3;
        layout(binding=3, rgba8) uniform lowp pixelLocalANGLE pls4;
        void main()
        {
            pixelLocalStoreANGLE(pls1, vec4(1, 0, 0, 1));
            pixelLocalStoreANGLE(pls2, vec4(0, 1, 0, 1));
            pixelLocalStoreANGLE(pls3, vec4(0, 0, 1, 1));
            pixelLocalStoreANGLE(pls4, vec4(0, 0, 0, 1));
        })");

        glFramebufferTexturePixelLocalStorageANGLE(0, tex1, 0, 0);
        glFramebufferTexturePixelLocalStorageANGLE(1, tex2, 0, 0);
        glFramebufferTexturePixelLocalStorageANGLE(2, tex3, 0, 0);
        glFramebufferTexturePixelLocalStorageANGLE(3, tex4, 0, 0);

        glBeginPixelLocalStorageANGLE(4, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO}));
    }
    else
    {
        mProgram.compile(R"(
        layout(binding=0, rgba8) uniform lowp pixelLocalANGLE pls1;
        layout(binding=1, rgba8) uniform lowp pixelLocalANGLE pls2;
        layout(location=0) out lowp vec4 out1;
        layout(location=1) out lowp vec4 out2;
        void main()
        {
            out1 = vec4(0, 1, 1, 0);
            out2 = vec4(1, 1, 0, 0);
            pixelLocalStoreANGLE(pls1, vec4(0, 0, 1, 1));
            pixelLocalStoreANGLE(pls2, vec4(0, 0, 0, 1));
        })");

        std::vector<GLColor> whiteData(H * W, GLColor::white);
        glBindTexture(GL_TEXTURE_2D, tex1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, whiteData.data());

        glEnablei(GL_BLEND, 0);
        glBlendEquationi(0, GL_FUNC_REVERSE_SUBTRACT);
        glBlendFunci(0, GL_ONE, GL_ONE);

        std::vector<GLColor> blackData(H * W, GLColor::black);
        glBindTexture(GL_TEXTURE_2D, tex2);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, blackData.data());

        glColorMaski(1, GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);

        for (int i = 2; i < MAX_DRAW_BUFFERS; ++i)
        {
            // Blend should not affect pixel local storage.
            glEnablei(GL_BLEND, i);
            glBlendFunci(i, GL_ZERO, GL_ONE);

            // Color mask should not affect pixel local storage.
            glColorMaski(i, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex2, 0);
        glFramebufferTexturePixelLocalStorageANGLE(0, tex3, 0, 0);
        glFramebufferTexturePixelLocalStorageANGLE(1, tex4, 0, 0);
        glDrawBuffers(2, GLenumArray({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}));

        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));
    }

    mProgram.drawBoxes({{FULLSCREEN}});

    int n;
    glGetIntegerv(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, &n);
    glEndPixelLocalStorageANGLE(n, GLenumArray({GL_KEEP, GL_KEEP, GL_KEEP, GL_KEEP}));

    attachTextureToScratchFBO(tex1);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::red);

    attachTextureToScratchFBO(tex2);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::green);

    attachTextureToScratchFBO(tex3);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::blue);

    attachTextureToScratchFBO(tex4);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::black);

    ASSERT_GL_NO_ERROR();
}

// Check that PLS and EXT_shader_framebuffer_fetch can be used together.
TEST_P(PixelLocalStorageTest, ParallelFramebufferFetch)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    ANGLE_SKIP_TEST_IF(MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE == 0);

    mProgram.compile("#extension GL_EXT_shader_framebuffer_fetch : require", R"(
    layout(binding=0, rgba8) uniform mediump pixelLocalANGLE pls;
    inout highp vec4 fbfetch;
    void main()
    {
        // Swap pls and fbfetch.
        vec4 tmp = pixelLocalLoadANGLE(pls);
        pixelLocalStoreANGLE(pls, fbfetch);
        fbfetch = tmp;
    })");

    PLSTestTexture pls(GL_RGBA8), fbfetch(GL_RGBA8);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbfetch, 0);

    glFramebufferPixelLocalClearValuefvANGLE(0, ClearF(0, 1, 0, 1));
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));

    GLfloat red[] = {1, 0, 0, 1};
    glClearBufferfv(GL_COLOR, 0, red);

    // Swap pls and fbfetch.
    mProgram.drawBoxes({{FULLSCREEN}});

    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));

    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::green);

    attachTextureToScratchFBO(pls);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::red);
}

// Check that PLS gets properly cleaned up when its framebuffer and textures are never deleted.
TEST_P(PixelLocalStorageTest, LeakFramebufferAndTexture)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint tex0;
    glGenTextures(1, &tex0);
    glBindTexture(GL_TEXTURE_2D, tex0);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 10, 10);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex0, 0, 0);

    PLSTestTexture tex1(GL_R32F);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex1, 0, 0);

    glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_RGBA8I);

    // Delete tex1.
    // Don't delete tex0.
    // Don't delete fbo.

    // The PixelLocalStorage frontend implementation has internal assertions that verify all its GL
    // context objects are properly disposed of.
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(PixelLocalStorageTest);
#define PLATFORM(API, BACKEND) API##_##BACKEND()
#define PLS_INSTANTIATE_RENDERING_TEST_AND(TEST, API, ...)                                \
    ANGLE_INSTANTIATE_TEST(                                                               \
        TEST,                                                                             \
        PLATFORM(API, D3D11) /* D3D coherent. */                                          \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, D3D11) /* D3D noncoherent. */                                       \
            .enable(Feature::DisableRasterizerOrderViews)                                 \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, OPENGL) /* OpenGL coherent. */                                      \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, OPENGL) /* OpenGL noncoherent. */                                   \
            .enable(Feature::EmulatePixelLocalStorage)                                    \
            .disable(Feature::SupportsFragmentShaderInterlockNV)                          \
            .disable(Feature::SupportsFragmentShaderOrderingINTEL)                        \
            .disable(Feature::SupportsFragmentShaderInterlockARB),                        \
        PLATFORM(API, OPENGLES) /* OpenGL ES coherent */                                  \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, OPENGLES) /* OpenGL ES noncoherent                                  \
                                   (EXT_shader_framebuffer_fetch_non_coherent). */        \
            .enable(Feature::EmulatePixelLocalStorage)                                    \
            .disable(Feature::SupportsShaderFramebufferFetchEXT)                          \
            .disable(Feature::SupportsShaderPixelLocalStorageEXT),                        \
        PLATFORM(API, OPENGLES) /* OpenGL ES noncoherent (shader images). */              \
            .enable(Feature::EmulatePixelLocalStorage)                                    \
            .disable(Feature::SupportsShaderFramebufferFetchEXT)                          \
            .disable(Feature::SupportsShaderFramebufferFetchNonCoherentEXT)               \
            .disable(Feature::SupportsShaderPixelLocalStorageEXT),                        \
        PLATFORM(API, VULKAN) /* Vulkan coherent. */                                      \
            .enable(Feature::AsyncCommandQueue)                                           \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, VULKAN) /* Vulkan noncoherent. */                                   \
            .disable(Feature::SupportsShaderFramebufferFetch)                             \
            .disable(Feature::SupportsFragmentShaderPixelInterlock)                       \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, VULKAN_SWIFTSHADER) /* Swiftshader coherent (framebuffer fetch). */ \
            .enable(Feature::AsyncCommandQueue)                                           \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, VULKAN_SWIFTSHADER) /* Swiftshader noncoherent. */                  \
            .disable(Feature::SupportsShaderFramebufferFetch)                             \
            .disable(Feature::SupportsFragmentShaderPixelInterlock)                       \
            .enable(Feature::AsyncCommandQueue)                                           \
            .enable(Feature::EmulatePixelLocalStorage),                                   \
        PLATFORM(API, VULKAN_SWIFTSHADER) /* Test PLS not having access to                \
                                             glEnablei/glDisablei/glColorMaski. */        \
            .enable(Feature::EmulatePixelLocalStorage)                                    \
            .enable(Feature::DisableDrawBuffersIndexed),                                  \
        __VA_ARGS__)

#define PLS_INSTANTIATE_RENDERING_TEST(TEST, API) PLS_INSTANTIATE_RENDERING_TEST_AND(TEST, API)

PLS_INSTANTIATE_RENDERING_TEST_AND(PixelLocalStorageTest,
                                   ES3,
                                   ES3_METAL().enable(Feature::EmulatePixelLocalStorage));

class PixelLocalStorageTestES31 : public PixelLocalStorageTest
{};

// Check that early_fragment_tests are not triggered when PLS uniforms are not declared.
TEST_P(PixelLocalStorageTestES31, EarlyFragmentTests)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex(GL_RGBA8);
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLuint stencil;
    glGenRenderbuffers(1, &stencil);
    glBindRenderbuffer(GL_RENDERBUFFER, stencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

    // Emits a fullscreen quad.
    constexpr char kFullscreenVS[] = R"(#version 310 es
    precision highp float;
    void main()
    {
        gl_Position.x = (gl_VertexID & 1) == 0 ? -1.0 : 1.0;
        gl_Position.y = (gl_VertexID & 2) == 0 ? -1.0 : 1.0;
        gl_Position.zw = vec2(0, 1);
    })";

    // Renders green to the framebuffer.
    constexpr char kDrawRed[] = R"(#version 310 es
    out mediump vec4 fragColor;
    void main()
    {
        fragColor = vec4(1, 0, 0, 1);
    })";

    ANGLE_GL_PROGRAM(drawGreen, kFullscreenVS, kDrawRed);

    // Render to stencil without PLS uniforms and with a discard. Since we discard, and since the
    // shader shouldn't enable early_fragment_tests, stencil should not be affected.
    constexpr char kNonPLSDiscard[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : enable
    void f(highp ipixelLocalANGLE pls)
    {
        // Function arguments don't trigger PLS restrictions.
        pixelLocalStoreANGLE(pls, ivec4(8));
    }
    void main()
    {
        discard;
    })";
    ANGLE_GL_PROGRAM(lateDiscard, kFullscreenVS, kNonPLSDiscard);
    glUseProgram(lateDiscard);
    glStencilFunc(GL_ALWAYS, 1, ~0u);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Clear the framebuffer to green.
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render red to the framebuffer with a stencil test. This should have no effect because the
    // stencil buffer should be all zeros.
    glUseProgram(drawGreen);
    glStencilFunc(GL_NOTEQUAL, 0, ~0u);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::green);

    // Now double check that this test would have failed if the shader had enabled
    // early_fragment_tests. Render to stencil *with* early_fragment_tests and a discard. Stencil
    // should be affected this time even though we discard.
    ANGLE_GL_PROGRAM(earlyDiscard, kFullscreenVS,
                     (std::string(kNonPLSDiscard) + "layout(early_fragment_tests) in;").c_str());
    glUseProgram(earlyDiscard);
    glStencilFunc(GL_ALWAYS, 1, ~0u);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Clear the framebuffer to green.
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render red to the framebuffer again. This time the stencil test should pass because the
    // stencil buffer should be all ones.
    glUseProgram(drawGreen);
    glStencilFunc(GL_NOTEQUAL, 0, ~0u);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::red);

    ASSERT_GL_NO_ERROR();
}

// Check that application-facing ES31 state is not perturbed by pixel local storage.
TEST_P(PixelLocalStorageTestES31, StateRestoration)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    doStateRestorationTest();
}

// Check that draw state does not affect PLS loads and stores, particularly for
// EXT_shader_pixel_local_storage, where they are implemented as fullscreen draws.
TEST_P(PixelLocalStorageTestES31, DrawStateReset)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    doDrawStateTest();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(PixelLocalStorageTestES31);
PLS_INSTANTIATE_RENDERING_TEST(PixelLocalStorageTestES31, ES31);

class PixelLocalStorageValidationTest : public ANGLETest<>
{
  protected:
    void testSetUp() override
    {
        ASSERT(IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));
        glGetIntegerv(GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE, &MAX_PIXEL_LOCAL_STORAGE_PLANES);
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE,
                      &MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE);
        glGetIntegerv(GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE,
                      &MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES);
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &MAX_DRAW_BUFFERS);

        mHasDebugKHR = EnsureGLExtensionEnabled("GL_KHR_debug");
        if (mHasDebugKHR)
        {
            glDebugMessageControlKHR(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR,
                                     GL_DEBUG_SEVERITY_HIGH, 0, NULL, GL_TRUE);
            glDebugMessageCallbackKHR(&ErrorMessageCallback, this);
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        }

        // INVALID_OPERATION is generated if DITHER is enabled.
        glDisable(GL_DITHER);

        ANGLETest::testSetUp();
    }

    int isContextVersionAtLeast(int major, int minor)
    {
        return getClientMajorVersion() > major ||
               (getClientMajorVersion() == major && getClientMinorVersion() >= minor);
    }

    static void GL_APIENTRY ErrorMessageCallback(GLenum source,
                                                 GLenum type,
                                                 GLuint id,
                                                 GLenum severity,
                                                 GLsizei length,
                                                 const GLchar *message,
                                                 const void *userParam)
    {
        auto test = const_cast<PixelLocalStorageValidationTest *>(
            static_cast<const PixelLocalStorageValidationTest *>(userParam));
        test->mErrorMessages.emplace_back(message);
    }

    GLint MAX_PIXEL_LOCAL_STORAGE_PLANES                           = 0;
    GLint MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE    = 0;
    GLint MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES = 0;
    GLint MAX_COLOR_ATTACHMENTS                                    = 0;
    GLint MAX_DRAW_BUFFERS                                         = 0;

    bool mHasDebugKHR;
    std::vector<std::string> mErrorMessages;
};

class ScopedEnable
{
  public:
    ScopedEnable(GLenum feature) : mFeature(feature) { glEnable(mFeature); }
    ~ScopedEnable() { glDisable(mFeature); }

  private:
    GLenum mFeature;
};

#define ASSERT_GL_INTEGER(pname, expected) \
    {                                      \
        GLint value;                       \
        glGetIntegerv(pname, &value);      \
        ASSERT_EQ(value, GLint(expected)); \
    }

#define EXPECT_GL_INTEGER(pname, expected)     \
    {                                          \
        GLint value;                           \
        glGetIntegerv(pname, &value);          \
        EXPECT_EQ(value, GLboolean(expected)); \
    }

#define EXPECT_PLS_INTEGER(plane, pname, expected)                               \
    {                                                                            \
        GLint value;                                                             \
        glGetFramebufferPixelLocalStorageParameterivANGLE(plane, pname, &value); \
        EXPECT_EQ(value, GLint(expected));                                       \
    }

#define EXPECT_GL_SINGLE_ERROR(err)                \
    EXPECT_GL_ERROR(err);                          \
    while (GLenum nextError = glGetError())        \
    {                                              \
        EXPECT_EQ(nextError, GLenum(GL_NO_ERROR)); \
    }

#define EXPECT_GL_SINGLE_ERROR_MSG(msg)                         \
    if (mHasDebugKHR)                                           \
    {                                                           \
        EXPECT_EQ(mErrorMessages.size(), size_t(1));            \
        if (mErrorMessages.size() == 1)                         \
        {                                                       \
            EXPECT_EQ(std::string(msg), mErrorMessages.back()); \
        }                                                       \
        mErrorMessages.clear();                                 \
    }

// Check that PLS state has the correct initial values.
TEST_P(PixelLocalStorageValidationTest, InitialValues)
{
    // It's valid to query GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE even when fbo 0 is bound.
    EXPECT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    EXPECT_GL_NO_ERROR();

    // Table 6.Y: Pixel Local Storage State
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        EXPECT_PLS_INTEGER(i, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_NONE);
        EXPECT_PLS_INTEGER(i, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
        EXPECT_PLS_INTEGER(i, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
        EXPECT_PLS_INTEGER(i, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);
    }
    EXPECT_GL_NO_ERROR();
}

// Check that glFramebufferMemorylessPixelLocalStorageANGLE validates as specified.
TEST_P(PixelLocalStorageValidationTest, FramebufferMemorylessPixelLocalStorageANGLE)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_R32F);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_R32F);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, GL_NONE);

    // If <internalformat> is NONE, the pixel local storage plane at index <plane> is deinitialized
    // and any internal storage is released.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_NONE);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, GL_NONE);

    // Set back to GL_RGBA8I.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8I);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8I);

    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_R32UI);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8I);

    // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE is nonzero.
    {
        PLSTestTexture tmp(GL_RGBA8);
        glFramebufferTexturePixelLocalStorageANGLE(1, tmp, 0, 0);
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_DONT_CARE, GL_DONT_CARE}));
        glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_R32UI);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG("Operation not permitted while pixel local storage is active.");
        // The store operation is ignored if its plane is memoryless or disabled.
        glEndPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_KEEP}));
        glFramebufferTexturePixelLocalStorageANGLE(1, 0, 0, 0);
    }

    // INVALID_VALUE is generated if <plane> < 0 or <plane> >= MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glFramebufferMemorylessPixelLocalStorageANGLE(-1, GL_R32UI);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");
    glFramebufferMemorylessPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_R32UI);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    glFramebufferMemorylessPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, GL_R32UI);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_R32UI);

    // INVALID_ENUM is generated if <internalformat> is not one of the acceptable values in Table
    // X.2, or NONE.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA16F);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage internal format.");
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA32UI);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage internal format.");
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8_SNORM);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage internal format.");

    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8I);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, GL_NONE);

    ASSERT_GL_NO_ERROR();
}

// Check that glFramebufferTexturePixelLocalStorageANGLE validates as specified.
TEST_P(PixelLocalStorageValidationTest, FramebufferTexturePixelLocalStorageANGLE)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Initially, pixel local storage planes are in a deinitialized state and are unusable.
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8UI, 10, 10);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 1, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8UI);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, tex);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    // If <backingtexture> is 0, <level> and <layer> are ignored and the pixel local storage plane
    // <plane> is deinitialized.
    glFramebufferTexturePixelLocalStorageANGLE(1, 0, 1, 2);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    // Set back to GL_RGBA8I.
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 1, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8UI);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, tex);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    // When a texture object is deleted, any pixel local storage plane to which it was bound is
    // automatically converted to a memoryless plane of matching internalformat.
    tex.reset();
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8UI);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8UI, 10, 10);

    // Same as above, but with orphaning.
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 1, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8UI);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, tex);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    tex.reset();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8UI);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8UI, 10, 10);

    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 1, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE is nonzero.
    {
        PLSTestTexture tmp(GL_RGBA8);
        glFramebufferTexturePixelLocalStorageANGLE(1, tex, 0, 0);
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_DISABLE_ANGLE, GL_DONT_CARE}));
        glFramebufferTexturePixelLocalStorageANGLE(0, tmp, 0, 0);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG("Operation not permitted while pixel local storage is active.");
        // The store operation is ignored if its plane is memoryless or disabled.
        glEndPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_DONT_CARE}));
        glFramebufferTexturePixelLocalStorageANGLE(1, 0, 0, 0);
    }

    // INVALID_VALUE is generated if <plane> < 0 or <plane> >= MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glFramebufferTexturePixelLocalStorageANGLE(-1, tex, 1, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");
    glFramebufferTexturePixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, tex, 1, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    glFramebufferTexturePixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, tex, 2, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, tex);
    EXPECT_PLS_INTEGER(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 2);

    // INVALID_OPERATION is generated if <backingtexture> is not the name of an existing immutable
    // texture object, or zero.
    GLTexture badTex;
    glFramebufferTexturePixelLocalStorageANGLE(2, badTex, 0, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Not a valid texture object name.");
    glBindTexture(GL_TEXTURE_2D, badTex);
    glFramebufferTexturePixelLocalStorageANGLE(2, badTex, 0, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Texture is not immutable.");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 10, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexturePixelLocalStorageANGLE(2, badTex, 0, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Texture is not immutable.");

    // INVALID_ENUM is generated if <backingtexture> is nonzero and not of type GL_TEXTURE_2D,
    // GL_TEXTURE_CUBE_MAP, GL_TEXTURE_2D_ARRAY, or GL_TEXTURE_3D.
    GLTexture tex2DMultisample;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex2DMultisample);
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 10, 10, 1);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexturePixelLocalStorageANGLE(0, tex2DMultisample, 0, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage texture type.");
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_NONE);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    // INVALID_VALUE is generated if <level> < 0.
    tex.reset();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_R32UI, 10, 10);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex, -1, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Level is negative.");

    // GL_INVALID_VALUE is generated if <backingtexture> is nonzero and <level> >= the immutable
    // number of mipmap levels in <backingtexture>.
    glFramebufferTexturePixelLocalStorageANGLE(2, tex, 3, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Level is larger than texture level count.");

    // INVALID_VALUE is generated if <layer> < 0.
    glFramebufferTexturePixelLocalStorageANGLE(2, tex, 1, -1);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Negative layer.");

    // GL_INVALID_VALUE is generated if <backingtexture> is nonzero and <layer> >= the immutable
    // number of texture layers in <backingtexture>.
    glFramebufferTexturePixelLocalStorageANGLE(2, tex, 1, 1);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Layer is larger than texture depth.");

    GLTexture texCube;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texCube);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, 10, 10);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexturePixelLocalStorageANGLE(2, texCube, 0, 6);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Layer is larger than texture depth.");
    glFramebufferTexturePixelLocalStorageANGLE(2, texCube, 1, 5);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Level is larger than texture level count.");
    glFramebufferTexturePixelLocalStorageANGLE(2, texCube, 0, 5);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, texCube);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 5);
    texCube.reset();
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    GLTexture tex2DArray;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex2DArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 2, GL_RGBA8I, 10, 10, 7);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexturePixelLocalStorageANGLE(2, tex2DArray, 1, 7);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Layer is larger than texture depth.");
    glFramebufferTexturePixelLocalStorageANGLE(2, tex2DArray, 2, 6);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Level is larger than texture level count.");
    glFramebufferTexturePixelLocalStorageANGLE(2, tex2DArray, 1, 6);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8I);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, tex2DArray);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 6);
    tex2DArray.reset();
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8I);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(2, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    GLTexture tex3D;
    glBindTexture(GL_TEXTURE_3D, tex3D);
    glTexStorage3D(GL_TEXTURE_3D, 3, GL_RGBA8I, 10, 10, 256);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexturePixelLocalStorageANGLE(0, tex3D, 2, 256);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Layer is larger than texture depth.");
    glFramebufferTexturePixelLocalStorageANGLE(0, tex3D, 3, 255);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Level is larger than texture level count.");
    glFramebufferTexturePixelLocalStorageANGLE(0, tex3D, 2, 255);
    EXPECT_GL_NO_ERROR();
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8I);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, tex3D);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 2);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 255);
    tex3D.reset();
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_FORMAT_ANGLE, GL_RGBA8I);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0);
    EXPECT_PLS_INTEGER(0, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0);

    // INVALID_ENUM is generated if <backingtexture> is nonzero and its internalformat is not
    // one of the acceptable values in Table X.2.
    tex.reset();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RG32F, 10, 10);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexturePixelLocalStorageANGLE(2, tex, 1, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage internal format.");

    ASSERT_GL_NO_ERROR();
}

// Check that FramebufferPixelLocalClearValue{f,i,ui}vANGLE validate as specified.
TEST_P(PixelLocalStorageValidationTest, glFramebufferPixelLocalClearValuesANGLE)
{
    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glFramebufferPixelLocalClearValuefvANGLE(0, ClearF(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");

    glFramebufferPixelLocalClearValueivANGLE(1, ClearI(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");

    glFramebufferPixelLocalClearValueuivANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1,
                                              ClearUI(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");

    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // INVALID_VALUE is generated if <plane> < 0 or <plane> >= MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glFramebufferPixelLocalClearValuefvANGLE(-1, ClearF(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");

    glFramebufferPixelLocalClearValueivANGLE(-1, ClearI(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");

    glFramebufferPixelLocalClearValueuivANGLE(-1, ClearUI(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");

    glFramebufferPixelLocalClearValuefvANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, ClearF(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");

    glFramebufferPixelLocalClearValueivANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, ClearI(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");

    glFramebufferPixelLocalClearValueuivANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, ClearUI(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");

    ASSERT_GL_NO_ERROR();

    // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE is TRUE.
    PLSTestTexture tex(GL_R32UI);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
    ASSERT_GL_NO_ERROR();

    glFramebufferPixelLocalClearValuefvANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1,
                                             ClearF(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Operation not permitted while pixel local storage is active.");

    glFramebufferPixelLocalClearValuefvANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES - 2,
                                             ClearF(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Operation not permitted while pixel local storage is active.");

    glFramebufferPixelLocalClearValueuivANGLE(0, ClearUI(0, 0, 0, 0));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Operation not permitted while pixel local storage is active.");

    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 1);
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    ASSERT_GL_NO_ERROR();
}

// Check that glBeginPixelLocalStorageANGLE validates non-PLS context state as specified.
TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_context_state)
{
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture pls0(GL_RGBA8, 100, 100);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 1);

    // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE is nonzero.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Operation not permitted while pixel local storage is active.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 1);
    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_DONT_CARE}));
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    // INVALID_OPERATION is generated if the value of SAMPLE_BUFFERS is 1 (i.e., if rendering to a
    // multisampled framebuffer).
    {
        GLRenderbuffer msaa;
        glBindRenderbuffer(GL_RENDERBUFFER, msaa);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA8, 100, 100);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaa);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with a multisampled framebuffer.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if DITHER is enabled.
    {
        ScopedEnable scopedEnable(GL_DITHER);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_DITHER enabled.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if RASTERIZER_DISCARD is enabled.
    {
        ScopedEnable scopedEnable(GL_RASTERIZER_DISCARD);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_RASTERIZER_DISCARD enabled.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if SAMPLE_ALPHA_TO_COVERAGE is enabled.
    {
        ScopedEnable scopedEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_SAMPLE_ALPHA_TO_COVERAGE enabled.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if SAMPLE_COVERAGE is enabled.
    {
        ScopedEnable scopedEnable(GL_SAMPLE_COVERAGE);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_SAMPLE_COVERAGE enabled.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }
}

// Check that glBeginPixelLocalStorageANGLE validates the draw framebuffer's state as specified.
TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_framebuffer_state)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture pls0(GL_RGBA8, 100, 100);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);

    // INVALID_VALUE is generated if <n> < 1 or <n> > MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glBeginPixelLocalStorageANGLE(0, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Planes must be greater than 0.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    glBeginPixelLocalStorageANGLE(
        MAX_PIXEL_LOCAL_STORAGE_PLANES + 1,
        std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES + 1, GL_DISABLE_ANGLE).data());
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Planes must be less than or equal to GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    // INVALID_FRAMEBUFFER_OPERATION is generated if the draw framebuffer has an image attached to
    // any color attachment point on or after:
    //
    //   COLOR_ATTACHMENT0 +
    //   MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE
    //
    if (MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE < MAX_COLOR_ATTACHMENTS)
    {
        for (int reservedAttachment :
             {MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE, MAX_COLOR_ATTACHMENTS - 1})
        {
            PLSTestTexture tmp(GL_RGBA8, 100, 100);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + reservedAttachment,
                                   GL_TEXTURE_2D, tmp, 0);
            glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_DISABLE_ANGLE}));
            EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
            EXPECT_GL_SINGLE_ERROR_MSG(
                "Framebuffer cannot have images attached to color attachment points on or after "
                "COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE.");
            ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
        }
    }

    // INVALID_FRAMEBUFFER_OPERATION is generated if the draw framebuffer has an image attached to
    // any color attachment point on or after:
    //
    //   COLOR_ATTACHMENT0 + MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE - <n>
    //
    int maxColorAttachmentsWithMaxPLSPlanes =
        MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES - MAX_PIXEL_LOCAL_STORAGE_PLANES;
    if (maxColorAttachmentsWithMaxPLSPlanes < MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE)
    {
        for (int reservedAttachment : {maxColorAttachmentsWithMaxPLSPlanes,
                                       MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE - 1})
        {
            PLSTestTexture tmp(GL_RGBA8, 100, 100);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + reservedAttachment,
                                   GL_TEXTURE_2D, tmp, 0);
            glBeginPixelLocalStorageANGLE(
                MAX_PIXEL_LOCAL_STORAGE_PLANES,
                std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_DISABLE_ANGLE).data());
            EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
            EXPECT_GL_SINGLE_ERROR_MSG(
                "Framebuffer cannot have images attached to color attachment points on or after "
                "COLOR_ATTACHMENT0 + "
                "MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE - <n>.");
            ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
        }
    }
}

// Check that glBeginPixelLocalStorageANGLE validates its loadops as specified.
TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_loadops)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture pls0(GL_RGBA8, 100, 100);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);

    // INVALID_VALUE is generated if <loadops> is NULL.
    glBeginPixelLocalStorageANGLE(1, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("loadops cannot null.");

    // INVALID_ENUM is generated if <loadops>[0..<n>-1] is not one of the Load Operations enumerated
    // in Table X.1.
    {
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));

        EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
        EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Load Operation: 0x1E01.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

        std::vector<GLenum> loadops(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_DISABLE_ANGLE);
        loadops.back() = GL_SCISSOR_BOX;
        glBeginPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, loadops.data());
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
        EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Load Operation: 0x0C10.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if <loadops>[0..<n>-1] is not DISABLE_ANGLE, and the pixel
    // local storage plane at that same index is is in a deinitialized state.
    {
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE(1, GLenumArray({GL_DONT_CARE}));
        EXPECT_GL_NO_ERROR();

        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_CLEAR_ANGLE, GL_DONT_CARE}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to enable a pixel local storage plane that is in a deinitialized state.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

        // If <backingtexture> is 0, <level> and <layer> are ignored and the pixel local storage
        // plane <plane> is deinitialized.
        glFramebufferTexturePixelLocalStorageANGLE(0, 0, -1, 999);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to enable a pixel local storage plane that is in a deinitialized state.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

        glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE(1, GLenumArray({GL_DONT_CARE}));
        EXPECT_GL_NO_ERROR();

        // If <internalformat> is NONE, the pixel local storage plane at index <plane> is
        // deinitialized and any internal storage is released.
        glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_NONE);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to enable a pixel local storage plane that is in a deinitialized state.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if <loadops>[0..<n>-1] is KEEP and the pixel local storage
    // plane at that same index is memoryless.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Load Operation GL_KEEP is invalid for memoryless planes.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
}

// Check that glBeginPixelLocalStorageANGLE validates the pixel local storage planes as specified.
TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_pls_planes)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture pls0(GL_RGBA8, 100, 100);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);

    // INVALID_OPERATION is generated if all enabled, texture-backed pixel local storage planes do
    // not have the same width and height.
    {
        GLTexture pls1;
        glBindTexture(GL_TEXTURE_2D, pls1);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 100, 101);
        PLSTestTexture pls2(GL_RGBA8, 100, 100);

        glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);
        glFramebufferTexturePixelLocalStorageANGLE(1, pls1, 0, 0);
        glFramebufferTexturePixelLocalStorageANGLE(2, pls2, 0, 0);

        // Disabling the mismatched size plane is fine.
        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_DISABLE_ANGLE, GL_KEEP}));
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE(3, GLenumArray({GL_DONT_CARE, GL_KEEP, GL_DONT_CARE}));
        EXPECT_GL_NO_ERROR();

        // Enabling the mismatched size plane errors.
        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_KEEP, GL_KEEP}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG("Mismatched pixel local storage backing texture sizes.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

        // Converting the mismatched size plane to memoryless also works.
        pls1.reset();
        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_ZERO, GL_KEEP}));
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_KEEP, GL_KEEP}));
        EXPECT_GL_NO_ERROR();

        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if the draw framebuffer has other attachments, and its
    // enabled, texture-backed pixel local storage planes do not have identical dimensions with the
    // rendering area.
    if (MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE > 0)
    {
        PLSTestTexture rt0(GL_RGBA8, 200, 100);
        PLSTestTexture rt1(GL_RGBA8, 100, 200);

        // rt0 is wider than the PLS extents.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt0, 0);
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_CLEAR_ANGLE}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Pixel local storage backing texture dimensions not equal to the rendering area.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

        // rt1 is taller than the PLS extents.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt1, 0);
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_CLEAR_ANGLE}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Pixel local storage backing texture dimensions not equal to the rendering area.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

        // The intersection of rt0 and rt1 is equal to the PLS extents.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, rt0, 0);
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_CLEAR_ANGLE}));
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_DONT_CARE}));

        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }

    // INVALID_OPERATION is generated if the draw framebuffer has no attachments and no enabled,
    // texture-backed pixel local storage planes.
    {
        glBeginPixelLocalStorageANGLE(
            MAX_PIXEL_LOCAL_STORAGE_PLANES,
            std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_DISABLE_ANGLE).data());
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Draw framebuffer has no attachments and no enabled, texture-backed pixel local "
            "storage planes.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

        glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_DONT_CARE}));
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Draw framebuffer has no attachments and no enabled, texture-backed pixel local "
            "storage planes.");
        ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    }
}

// TODO(anglebug.com/7279): Block feedback loops
// Check glBeginPixelLocalStorageANGLE validates feedback loops as specified.
// TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_feedback_loops)
// {
//     // INVALID_OPERATION is generated if a single texture image is bound to more than one pixel
//     // local storage plane.
//
//     // INVALID_OPERATION is generated if a single texture image is simultaneously bound to a
//     // pixel local storage plane and attached to the draw framebuffer.
//
//     ASSERT_GL_NO_ERROR();
// }

// Check that glEndPixelLocalStorageANGLE and glPixelLocalStorageBarrierANGLE validate as specified.
TEST_P(PixelLocalStorageValidationTest, EndAndBarrierANGLE)
{
    // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE is zero.
    glEndPixelLocalStorageANGLE(0, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    glEndPixelLocalStorageANGLE(1, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glBeginPixelLocalStorageANGLE(0, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Planes must be greater than 0.");
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glEndPixelLocalStorageANGLE(2, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    PLSTestTexture tex(GL_RGBA8);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    for (int i = 1; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        glFramebufferMemorylessPixelLocalStorageANGLE(i, GL_RGBA8);
    }
    glBeginPixelLocalStorageANGLE(
        MAX_PIXEL_LOCAL_STORAGE_PLANES,
        std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_ZERO).data());
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, MAX_PIXEL_LOCAL_STORAGE_PLANES);

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_NO_ERROR();

    glEndPixelLocalStorageANGLE(
        MAX_PIXEL_LOCAL_STORAGE_PLANES,
        std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_DONT_CARE).data());
    EXPECT_GL_NO_ERROR();

    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, 0);
    EXPECT_GL_NO_ERROR();

    glEndPixelLocalStorageANGLE(0, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    ASSERT_GL_NO_ERROR();

    // INVALID_VALUE is generated if <n> != PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE.
    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_DISABLE_ANGLE}));
    ASSERT_GL_NO_ERROR();

    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("<n> != ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE");

    glEndPixelLocalStorageANGLE(1, GLenumArray({GL_DONT_CARE}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("<n> != ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE");

    glEndPixelLocalStorageANGLE(2, GLenumArray({GL_DONT_CARE, GL_DONT_CARE}));
    ASSERT_GL_NO_ERROR();

    // INVALID_ENUM is generated if <storeops>[0..PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE-1] is not
    // one of the Store Operations enumerated in Table X.2.
    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_DISABLE_ANGLE, GL_CLEAR_ANGLE}));
    ASSERT_GL_NO_ERROR();

    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_CLEAR_ANGLE, GL_DONT_CARE, GL_KEEP}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Store Operation: 0x96E4.");

    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_DONT_CARE, GL_DISABLE_ANGLE, GL_KEEP}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Store Operation: 0x96E5.");

    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_DONT_CARE, GL_KEEP, GL_ZERO}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Store Operation: 0x0000.");

    glEndPixelLocalStorageANGLE(3, GLenumArray({GL_DONT_CARE, GL_SCISSOR_BOX, GL_KEEP}));
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Store Operation: 0x0C10.");

    ASSERT_GL_NO_ERROR();
}

// Check that glEndPixelLocalStorageANGLE and glPixelLocalStorageBarrierANGLE validate as specified.
TEST_P(PixelLocalStorageValidationTest, GetFramebufferPixelLocalStorageParametersANGLE)
{
    GLfloat floats[4];
    GLint ints[4];

    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glGetFramebufferPixelLocalStorageParameterfvANGLE(0, GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE,
                                                      floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");
    glGetFramebufferPixelLocalStorageParameterivANGLE(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, ints);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");

    GLFramebuffer fbo;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

    // INVALID_VALUE is generated if <plane> < 0 or <plane> >= MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glGetFramebufferPixelLocalStorageParameterfvANGLE(-1, GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE,
                                                      floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");

    glGetFramebufferPixelLocalStorageParameterivANGLE(-1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, ints);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");

    glGetFramebufferPixelLocalStorageParameterfvANGLE(
        MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE, floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");

    glGetFramebufferPixelLocalStorageParameterivANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES,
                                                      GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, ints);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");

    // INVALID_ENUM is generated if the command issued is not the associated "Get Command" for
    // <pname> in Table 6.Y.
    glGetFramebufferPixelLocalStorageParameterivANGLE(0, GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE,
                                                      ints);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x96EA is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterivANGLE(0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                      ints);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x8CD0 is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterfvANGLE(1, GL_PIXEL_LOCAL_FORMAT_ANGLE, floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x96E6 is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterfvANGLE(1, GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x96E7 is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterfvANGLE(1, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE,
                                                      floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x96E8 is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterfvANGLE(1, GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE,
                                                      floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x96E9 is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterfvANGLE(1, GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE,
                                                      floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x96EB is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterfvANGLE(
        1, GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE, floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x96EC is currently not supported.");
    glGetFramebufferPixelLocalStorageParameterfvANGLE(
        1, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, floats);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
    EXPECT_GL_SINGLE_ERROR_MSG("Enum 0x8CD3 is currently not supported.");
}

#define EXPECT_BANNED(cmd, msg)                   \
    cmd;                                          \
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION); \
    EXPECT_GL_SINGLE_ERROR_MSG(msg)

#define EXPECT_BANNED_DEFAULT_MSG(cmd) \
    EXPECT_BANNED(cmd, "Operation not permitted while pixel local storage is active.")

static std::vector<char> FormatBannedCapMsg(GLenum cap)
{
    constexpr char format[] =
        "Cap 0x%04X cannot be enabled or disabled while pixel local storage is active.";
    std::vector<char> msg(std::snprintf(nullptr, 0, format, cap) + 1);
    std::snprintf(msg.data(), msg.size(), format, cap);
    return msg;
}

#define EXPECT_BANNED_CAP(cap)                           \
    {                                                    \
        std::vector<char> msg = FormatBannedCapMsg(cap); \
        EXPECT_BANNED(glEnable(cap), msg.data());        \
        EXPECT_BANNED(glDisable(cap), msg.data());       \
        glIsEnabled(cap);                                \
        EXPECT_GL_NO_ERROR();                            \
    }

// Check command-specific errors that go into effect when PLS is active, as well as commands
// specifically called out by EXT_shader_pixel_local_storage for flushing tiled memory.
TEST_P(PixelLocalStorageValidationTest, BannedCommands)
{
    PLSTestTexture tex(GL_RGBA8);
    GLFramebuffer fbo;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    int numActivePlanes = MAX_PIXEL_LOCAL_STORAGE_PLANES - 1;
    for (int i = 1; i < numActivePlanes; ++i)
    {
        glFramebufferMemorylessPixelLocalStorageANGLE(i, GL_RGBA8);
    }
    glBeginPixelLocalStorageANGLE(numActivePlanes,
                                  std::vector<GLenum>(numActivePlanes, GL_ZERO).data());
    ASSERT_GL_INTEGER(GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE, numActivePlanes);
    ASSERT_GL_NO_ERROR();

    // Check commands called out by EXT_shader_pixel_local_storage for flushing tiled memory.
    EXPECT_BANNED_DEFAULT_MSG(glFlush());
    EXPECT_BANNED_DEFAULT_MSG(glFinish());
    EXPECT_BANNED_DEFAULT_MSG(glClientWaitSync(nullptr, 0, 0));
    EXPECT_BANNED_DEFAULT_MSG(
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    EXPECT_BANNED_DEFAULT_MSG(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, 0));
    EXPECT_BANNED_DEFAULT_MSG(glBlitFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    EXPECT_BANNED_DEFAULT_MSG(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    EXPECT_BANNED_DEFAULT_MSG(glDeleteFramebuffers(1, nullptr));
    EXPECT_BANNED_DEFAULT_MSG(glDrawBuffers(0, nullptr));

    // INVALID_OPERATION is generated by Enable(), Disable() if <cap> is not one of: CULL_FACE,
    // DEPTH_TEST, POLYGON_OFFSET_FILL, PRIMITIVE_RESTART_FIXED_INDEX, SCISSOR_TEST,
    // STENCIL_TEST
    EXPECT_BANNED_CAP(GL_SAMPLE_ALPHA_TO_COVERAGE);
    EXPECT_BANNED_CAP(GL_SAMPLE_COVERAGE);
    EXPECT_BANNED_CAP(GL_BLEND);
    EXPECT_BANNED_CAP(GL_DITHER);
    EXPECT_BANNED_CAP(GL_RASTERIZER_DISCARD);
    if (isContextVersionAtLeast(3, 1))
    {
        EXPECT_BANNED_CAP(GL_SAMPLE_MASK);
    }
    if (IsGLExtensionEnabled("GL_EXT_multisample_compatibility"))
    {
        EXPECT_BANNED_CAP(GL_MULTISAMPLE_EXT);
        EXPECT_BANNED_CAP(GL_SAMPLE_ALPHA_TO_ONE_EXT);
    }
    if (IsGLExtensionEnabled("GL_KHR_debug"))
    {
        EXPECT_BANNED_CAP(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        EXPECT_BANNED_CAP(GL_DEBUG_OUTPUT);
    }
    if (IsGLExtensionEnabled("GL_OES_sample_shading"))
    {
        EXPECT_BANNED_CAP(GL_SAMPLE_SHADING);
    }
    if (IsGLExtensionEnabled("GL_QCOM_shading_rate"))
    {
        EXPECT_BANNED_CAP(GL_SHADING_RATE_PRESERVE_ASPECT_RATIO_QCOM);
    }
    if (IsGLExtensionEnabled("GL_ANGLE_logic_op"))
    {
        EXPECT_BANNED_CAP(GL_COLOR_LOGIC_OP);
    }

    // The validation tests are expected to run in a configuration where the draw buffers are
    // restricted by PLS (e.g., not shader images).
    int bannedColorAttachment = MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE;
    ASSERT(bannedColorAttachment < MAX_DRAW_BUFFERS);

    int bannedCombinedAttachment =
        MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES - numActivePlanes;

    // INVALID_OPERATION is generated by ClearBufferfv(), ClearBufferiv(), ClearBufferuiv() if
    // <buffer> is GL_COLOR and:
    //
    //   <buffer> < MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE
    //   <buffer> < (MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE -
    //               ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE)
    //
    GLfloat clearf[4]{};
    GLint cleari[4]{};
    GLuint clearui[4]{};
    {
        const char *msg =
            "Argument <drawbuffer> must be less than "
            "MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE when pixel local "
            "storage "
            "is active.";
        EXPECT_BANNED(glClearBufferfv(GL_COLOR, bannedColorAttachment, clearf), msg);
        EXPECT_BANNED(glClearBufferiv(GL_COLOR, bannedColorAttachment, cleari), msg);
        EXPECT_BANNED(glClearBufferuiv(GL_COLOR, bannedColorAttachment, clearui), msg);
        EXPECT_BANNED(glClearBufferfv(GL_COLOR, MAX_DRAW_BUFFERS - 1, clearf), msg);
        EXPECT_BANNED(glClearBufferiv(GL_COLOR, MAX_DRAW_BUFFERS - 1, cleari), msg);
        EXPECT_BANNED(glClearBufferuiv(GL_COLOR, MAX_DRAW_BUFFERS - 1, clearui), msg);
    }
    if (bannedCombinedAttachment < bannedColorAttachment)
    {
        const char *msg =
            "Argument <drawbuffer> must be less than "
            "(MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE - "
            "ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE) when pixel local storage is active.";
        EXPECT_BANNED(glClearBufferfv(GL_COLOR, bannedCombinedAttachment, clearf), msg);
        EXPECT_BANNED(glClearBufferiv(GL_COLOR, bannedCombinedAttachment, cleari), msg);
        EXPECT_BANNED(glClearBufferuiv(GL_COLOR, bannedCombinedAttachment, clearui), msg);
    }
    glClearBufferfv(GL_DEPTH, 0, clearf);
    EXPECT_GL_NO_ERROR();
    glClearBufferiv(GL_STENCIL, 0, cleari);
    EXPECT_GL_NO_ERROR();

    if (IsGLExtensionEnabled("GL_OES_draw_buffers_indexed"))
    {
        // INVALID_OPERATION is generated by Enablei*(), Disablei*() if:
        //
        //   <index> < MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE
        //   <index> < (MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE -
        //              ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE)
        //
        {
            const char *msg =
                "Argument <index> must be less than "
                "MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE when pixel local "
                "storage is active.";
            EXPECT_BANNED(glEnableiOES(GL_BLEND, bannedColorAttachment), msg);
            EXPECT_BANNED(glDisableiOES(GL_BLEND, bannedColorAttachment), msg);
            EXPECT_BANNED(glEnableiOES(GL_BLEND, MAX_DRAW_BUFFERS - 1), msg);
            EXPECT_BANNED(glDisableiOES(GL_BLEND, MAX_DRAW_BUFFERS - 1), msg);
        }
        if (bannedCombinedAttachment < bannedColorAttachment)
        {
            const char *msg =
                "Argument <index> must be less than "
                "(MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE - "
                "ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE) when pixel local storage is active.";
            EXPECT_BANNED(glEnableiOES(GL_BLEND, bannedCombinedAttachment), msg);
            EXPECT_BANNED(glDisableiOES(GL_BLEND, bannedCombinedAttachment), msg);
        }

        // INVALID_OPERATION is generated by BlendEquationi*(), BlendEquationSeparatei*(),
        // BlendFunci*(), BlendFuncSeparatei*(), ColorMaski*() if:
        //
        //   <buf> < MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE
        //   <buf> < (MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE -
        //            ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE)
        //
        {
            const char *msg =
                "Argument <buf> must be less than "
                "MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE when pixel local "
                "storage is active.";
            EXPECT_BANNED(glBlendEquationiOES(bannedColorAttachment, GL_FUNC_ADD), msg);
            EXPECT_BANNED(glBlendEquationSeparateiOES(bannedColorAttachment, GL_FUNC_SUBTRACT,
                                                      GL_FUNC_REVERSE_SUBTRACT),
                          msg);
            EXPECT_BANNED(glBlendFunciOES(bannedColorAttachment, GL_ZERO, GL_ZERO), msg);
            EXPECT_BANNED(
                glBlendFuncSeparateiOES(bannedColorAttachment, GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO),
                msg);
            EXPECT_BANNED(
                glColorMaskiOES(bannedColorAttachment, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE),
                msg);
            EXPECT_BANNED(glBlendEquationiOES(MAX_DRAW_BUFFERS - 1, GL_FUNC_ADD), msg);
            EXPECT_BANNED(glBlendEquationSeparateiOES(MAX_DRAW_BUFFERS - 1, GL_FUNC_SUBTRACT,
                                                      GL_FUNC_REVERSE_SUBTRACT),
                          msg);
            EXPECT_BANNED(glBlendFunciOES(MAX_DRAW_BUFFERS - 1, GL_ZERO, GL_ZERO), msg);
            EXPECT_BANNED(
                glBlendFuncSeparateiOES(MAX_DRAW_BUFFERS - 1, GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO),
                msg);
            EXPECT_BANNED(
                glColorMaskiOES(MAX_DRAW_BUFFERS - 1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE), msg);
        }
        if (bannedCombinedAttachment < MAX_DRAW_BUFFERS - 1)
        {
            const char *msg =
                "Argument <buf> must be less than "
                "(MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE - "
                "ACTIVE_PIXEL_LOCAL_STORAGE_PLANES_ANGLE) when pixel local storage is active.";
            EXPECT_BANNED(glBlendEquationiOES(bannedCombinedAttachment, GL_FUNC_ADD), msg);
            EXPECT_BANNED(glBlendEquationSeparateiOES(bannedCombinedAttachment, GL_FUNC_SUBTRACT,
                                                      GL_FUNC_REVERSE_SUBTRACT),
                          msg);
            EXPECT_BANNED(glBlendFunciOES(bannedCombinedAttachment, GL_ZERO, GL_ZERO), msg);
            EXPECT_BANNED(glBlendFuncSeparateiOES(bannedCombinedAttachment, GL_ZERO, GL_ZERO,
                                                  GL_ZERO, GL_ZERO),
                          msg);
            EXPECT_BANNED(
                glColorMaskiOES(bannedCombinedAttachment, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE),
                msg);
        }
    }

    // INVALID_OPERATION is generated if a draw is issued with a fragment shader that accesses a
    // texture bound to pixel local storage.
    //
    // TODO(anglebug.com/7279).

    // INVALID_OPERATION is generated if a draw is issued with a fragment shader that has a
    // pixel local uniform bound to an inactive pixel local storage plane.
    //
    // TODO(anglebug.com/7279).

    // INVALID_OPERATION is generated if a draw is issued with a fragment shader that does _not_
    // have a pixel local uniform bound to an _active_ pixel local storage plane (i.e., the
    // fragment shader must declare uniforms bound to every single active pixel local storage
    // plane).
    //
    // This is because many backend implementations need to account for every active pixel local
    // storage plane, even if the application code does not access it during a particular shader
    // invocation.
    //
    // TODO(anglebug.com/7279).

    // INVALID_OPERATION is generated if a draw is issued with a fragment shader that has a
    // pixel local storage uniform whose format layout qualifier does not identically match the
    // internalformat of its associated pixel local storage plane on the current draw
    // framebuffer, as enumerated in Table X.3.
    //
    // TODO(anglebug.com/7279).

    ASSERT_GL_NO_ERROR();
}

// Check that PLS gets properly cleaned up when its framebuffer and textures are never deleted.
TEST_P(PixelLocalStorageValidationTest, LeakFramebufferAndTexture)
{
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint tex0;
    glGenTextures(1, &tex0);
    glBindTexture(GL_TEXTURE_2D, tex0);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 10, 10);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex0, 0, 0);

    PLSTestTexture tex1(GL_R32F);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex1, 0, 0);

    glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_RGBA8I);

    // Delete tex1.
    // Don't delete tex0.
    // Don't delete fbo.

    // The PixelLocalStorage frontend implementation has internal assertions that verify all its GL
    // context objects are properly disposed of.
}

// Check that PLS gets properly cleaned up when the context is lost.
TEST_P(PixelLocalStorageValidationTest, LoseContext)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_CHROMIUM_lose_context"));

    GLuint fbo0;
    glGenFramebuffers(1, &fbo0);

    GLFramebuffer fbo1;

    GLuint tex0;
    glGenTextures(1, &tex0);
    glBindTexture(GL_TEXTURE_2D, tex0);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 10, 10);

    PLSTestTexture tex1(GL_R32F);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo0);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex0, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex1, 0, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_RGBA8I);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex0, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex1, 0, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_RGBA8I);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glLoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET, GL_INNOCENT_CONTEXT_RESET);

    // Delete tex1.
    // Don't delete tex0.
    // Delete fbo1.
    // Don't delete fbo0.

    // The PixelLocalStorage frontend implementation has internal assertions that verify all its GL
    // context objects are properly disposed of.
}

ANGLE_INSTANTIATE_TEST(PixelLocalStorageValidationTest,
                       WithRobustness(ES31_NULL()).enable(Feature::EmulatePixelLocalStorage),
                       WithRobustness(ES31_NULL())
                           .enable(Feature::EmulatePixelLocalStorage)
                           .enable(Feature::DisableDrawBuffersIndexed));

class PixelLocalStorageCompilerTest : public ANGLETest<>
{
  protected:
    void testSetUp() override
    {
        ASSERT(IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

        // INVALID_OPERATION is generated if DITHER is enabled.
        glDisable(GL_DITHER);

        ANGLETest::testSetUp();
    }
    ShaderInfoLog log;
};

// Check that PLS #extension support is properly implemented.
TEST_P(PixelLocalStorageCompilerTest, Extension)
{
    // GL_ANGLE_shader_pixel_local_storage_coherent isn't a shader extension. Shaders must always
    // use GL_ANGLE_shader_pixel_local_storage, regardless of coherency.
    constexpr char kNonexistentPLSCoherentExtension[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage_coherent : require
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kNonexistentPLSCoherentExtension));
    EXPECT_TRUE(log.has(
        "ERROR: 0:2: 'GL_ANGLE_shader_pixel_local_storage_coherent' : extension is not supported"));

    // PLS type names cannot be used as variable names when the extension is enabled.
    constexpr char kPLSEnabledTypesAsNames[] = R"(#version 310 es
    #extension all : warn
    void main()
    {
        int pixelLocalANGLE = 0;
        int ipixelLocalANGLE = 0;
        int upixelLocalANGLE = 0;
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSEnabledTypesAsNames));
    EXPECT_TRUE(log.has("ERROR: 0:5: 'pixelLocalANGLE' : syntax error"));

    // PLS type names are fair game when the extension is disabled.
    constexpr char kPLSDisabledTypesAsNames[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : disable
    void main()
    {
        int pixelLocalANGLE = 0;
        int ipixelLocalANGLE = 0;
        int upixelLocalANGLE = 0;
    })";
    EXPECT_TRUE(log.compileFragmentShader(kPLSDisabledTypesAsNames));

    // PLS is not allowed in a vertex shader.
    constexpr char kPLSInVertexShader[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : enable
    layout(binding=0, rgba8) lowp uniform pixelLocalANGLE pls;
    void main()
    {
        pixelLocalStoreANGLE(pls, vec4(0));
    })";
    EXPECT_FALSE(log.compileShader(kPLSInVertexShader, GL_VERTEX_SHADER));
    EXPECT_TRUE(
        log.has("ERROR: 0:3: 'pixelLocalANGLE' : undefined use of pixel local storage outside a "
                "fragment shader"));

    // Internal synchronization functions used by the compiler shouldn't be visible in ESSL.
    EXPECT_FALSE(log.compileFragmentShader(R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void main()
    {
        beginInvocationInterlockNV();
        endInvocationInterlockNV();
    })"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:5: 'beginInvocationInterlockNV' : no matching overloaded function found"));
    EXPECT_TRUE(
        log.has("ERROR: 0:6: 'endInvocationInterlockNV' : no matching overloaded function found"));

    EXPECT_FALSE(log.compileFragmentShader(R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void main()
    {
        beginFragmentShaderOrderingINTEL();
    })"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:5: 'beginFragmentShaderOrderingINTEL' : no matching overloaded function found"));

    EXPECT_FALSE(log.compileFragmentShader(R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void main()
    {
        beginInvocationInterlockARB();
        endInvocationInterlockARB();
    })"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:5: 'beginInvocationInterlockARB' : no matching overloaded function found"));
    EXPECT_TRUE(
        log.has("ERROR: 0:6: 'endInvocationInterlockARB' : no matching overloaded function found"));

    ASSERT_GL_NO_ERROR();
}

// Check proper validation of PLS handle declarations.
TEST_P(PixelLocalStorageCompilerTest, Declarations)
{
    // PLS handles must be uniform.
    constexpr char kPLSTypesMustBeUniform[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : enable
    layout(binding=0, rgba8) highp pixelLocalANGLE pls1;
    void main()
    {
        highp ipixelLocalANGLE pls2;
        highp upixelLocalANGLE pls3;
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSTypesMustBeUniform));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'pixelLocalANGLE' : pixelLocalANGLEs must be uniform"));
    EXPECT_TRUE(log.has("ERROR: 0:6: 'ipixelLocalANGLE' : ipixelLocalANGLEs must be uniform"));
    EXPECT_TRUE(log.has("ERROR: 0:7: 'upixelLocalANGLE' : upixelLocalANGLEs must be uniform"));

    // Memory qualifiers are not allowed on PLS handles.
    constexpr char kPLSMemoryQualifiers[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0, rgba8) uniform lowp volatile coherent restrict pixelLocalANGLE pls1;
    layout(binding=1, rgba8i) uniform mediump readonly ipixelLocalANGLE pls2;
    void f(uniform highp writeonly upixelLocalANGLE pls);
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSMemoryQualifiers));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'coherent' : "));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'restrict' : "));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'volatile' : "));
    EXPECT_TRUE(log.has("ERROR: 0:4: 'readonly' : "));
    EXPECT_TRUE(log.has("ERROR: 0:5: 'writeonly' : "));

    // PLS handles must specify precision.
    constexpr char kPLSNoPrecision[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : enable
    layout(binding=0, rgba8) uniform pixelLocalANGLE pls1;
    layout(binding=1, rgba8i) uniform ipixelLocalANGLE pls2;
    void f(upixelLocalANGLE pls3)
    {
    }
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSNoPrecision));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'pixelLocalANGLE' : No precision specified"));
    EXPECT_TRUE(log.has("ERROR: 0:4: 'ipixelLocalANGLE' : No precision specified"));
    EXPECT_TRUE(log.has("ERROR: 0:5: 'upixelLocalANGLE' : No precision specified"));

    // PLS handles cannot cannot be aggregated in arrays.
    constexpr char kPLSArrays[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0, rgba8) uniform lowp pixelLocalANGLE pls1[1];
    layout(binding=1, rgba8i) uniform mediump ipixelLocalANGLE pls2[2];
    layout(binding=2, rgba8ui) uniform highp upixelLocalANGLE pls3[3];
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSArrays));
    EXPECT_TRUE(log.has(
        "ERROR: 0:3: 'array' : pixel local storage handles cannot be aggregated in arrays"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:4: 'array' : pixel local storage handles cannot be aggregated in arrays"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:5: 'array' : pixel local storage handles cannot be aggregated in arrays"));

    // If PLS handles could be used before their declaration, then we would need to update the PLS
    // rewriters to make two passes.
    constexpr char kPLSUseBeforeDeclaration[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void f()
    {
        pixelLocalStoreANGLE(pls, vec4(0));
        pixelLocalStoreANGLE(pls2, ivec4(0));
    }
    layout(binding=0, rgba8) uniform lowp pixelLocalANGLE pls;
    void main()
    {
        pixelLocalStoreANGLE(pls, vec4(0));
        pixelLocalStoreANGLE(pls2, ivec4(0));
    }
    layout(binding=1, rgba8i) uniform lowp ipixelLocalANGLE pls2;)";
    EXPECT_FALSE(log.compileFragmentShader(kPLSUseBeforeDeclaration));
    EXPECT_TRUE(log.has("ERROR: 0:5: 'pls' : undeclared identifier"));
    EXPECT_TRUE(log.has("ERROR: 0:6: 'pls2' : undeclared identifier"));
    EXPECT_TRUE(log.has("ERROR: 0:12: 'pls2' : undeclared identifier"));

    // PLS unimorms must be declared at global scope; they cannot be declared in structs or
    // interface blocks.
    constexpr char kPLSInStruct[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    struct Foo
    {
        lowp pixelLocalANGLE pls;
    };
    uniform Foo foo;
    uniform PLSBlock
    {
        lowp pixelLocalANGLE blockpls;
    };
    void main()
    {
        pixelLocalStoreANGLE(foo.pls, pixelLocalLoadANGLE(blockpls));
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSInStruct));
    EXPECT_TRUE(log.has("ERROR: 0:5: 'pixelLocalANGLE' : disallowed type in struct"));
    EXPECT_TRUE(
        log.has("ERROR: 0:10: 'pixelLocalANGLE' : unsupported type - pixelLocalANGLE types are not "
                "allowed in interface blocks"));

    ASSERT_GL_NO_ERROR();
}

// Check proper validation of PLS layout qualifiers.
TEST_P(PixelLocalStorageCompilerTest, LayoutQualifiers)
{
    // PLS handles must use a supported format and binding.
    constexpr char kPLSUnsupportedFormatsAndBindings[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0, rgba32f) highp uniform pixelLocalANGLE pls0;
    layout(binding=1, rgba16f) highp uniform pixelLocalANGLE pls1;
    layout(binding=2, rgba8_snorm) highp uniform pixelLocalANGLE pls2;
    layout(binding=3, rgba32ui) highp uniform upixelLocalANGLE pls3;
    layout(binding=4, rgba16ui) highp uniform upixelLocalANGLE pls4;
    layout(binding=5, rgba32i) highp uniform ipixelLocalANGLE pls5;
    layout(binding=6, rgba16i) highp uniform ipixelLocalANGLE pls6;
    layout(binding=7, r32i) highp uniform ipixelLocalANGLE pls7;
    layout(binding=999999999, rgba) highp uniform ipixelLocalANGLE pls8;
    highp uniform pixelLocalANGLE pls9;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSUnsupportedFormatsAndBindings));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'rgba32f' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:4: 'rgba16f' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:5: 'rgba8_snorm' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:6: 'rgba32ui' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:7: 'rgba16ui' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:8: 'rgba32i' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:9: 'rgba16i' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:10: 'r32i' : illegal pixel local storage format"));
    EXPECT_TRUE(log.has("ERROR: 0:11: 'rgba' : invalid layout qualifier"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:11: 'layout qualifier' : pixel local storage requires a format specifier"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:12: 'layout qualifier' : pixel local storage requires a format specifier"));
    // TODO(anglebug.com/7279): "PLS binding greater than gl_MaxPixelLocalStoragePlanesANGLE".
    EXPECT_TRUE(
        log.has("ERROR: 0:12: 'layout qualifier' : pixel local storage requires a binding index"));

    // PLS handles must use the correct type for the given format.
    constexpr char kPLSInvalidTypeForFormat[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0) highp uniform pixelLocalANGLE pls0;
    layout(binding=1) highp uniform upixelLocalANGLE pls1;
    layout(binding=2) highp uniform ipixelLocalANGLE pls2;
    layout(binding=3, rgba8) highp uniform ipixelLocalANGLE pls3;
    layout(binding=4, rgba8) highp uniform upixelLocalANGLE pls4;
    layout(binding=5, rgba8ui) highp uniform pixelLocalANGLE pls5;
    layout(binding=6, rgba8ui) highp uniform ipixelLocalANGLE pls6;
    layout(binding=7, rgba8i) highp uniform upixelLocalANGLE pls7;
    layout(binding=8, rgba8i) highp uniform pixelLocalANGLE pls8;
    layout(binding=9, r32f) highp uniform ipixelLocalANGLE pls9;
    layout(binding=10, r32f) highp uniform upixelLocalANGLE pls10;
    layout(binding=11, r32ui) highp uniform pixelLocalANGLE pls11;
    layout(binding=12, r32ui) highp uniform ipixelLocalANGLE pls12;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSInvalidTypeForFormat));
    EXPECT_TRUE(log.has(
        "ERROR: 0:3: 'layout qualifier' : pixel local storage requires a format specifier"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:4: 'layout qualifier' : pixel local storage requires a format specifier"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:5: 'layout qualifier' : pixel local storage requires a format specifier"));
    EXPECT_TRUE(
        log.has("ERROR: 0:6: 'rgba8' : pixel local storage format requires pixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:7: 'rgba8' : pixel local storage format requires pixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:8: 'rgba8ui' : pixel local storage format requires upixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:9: 'rgba8ui' : pixel local storage format requires upixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:10: 'rgba8i' : pixel local storage format requires ipixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:11: 'rgba8i' : pixel local storage format requires ipixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:12: 'r32f' : pixel local storage format requires pixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:13: 'r32f' : pixel local storage format requires pixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:14: 'r32ui' : pixel local storage format requires upixelLocalANGLE"));
    EXPECT_TRUE(
        log.has("ERROR: 0:15: 'r32ui' : pixel local storage format requires upixelLocalANGLE"));

    // PLS handles cannot have duplicate binding indices.
    constexpr char kPLSDuplicateBindings[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0, rgba) uniform highp pixelLocalANGLE pls0;
    layout(rgba8i, binding=1) uniform highp ipixelLocalANGLE pls1;
    layout(binding=2, rgba8ui) uniform highp upixelLocalANGLE pls2;
    layout(binding=1, rgba) uniform highp ipixelLocalANGLE pls3;
    layout(rgba8i, binding=0) uniform mediump ipixelLocalANGLE pls4;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSDuplicateBindings));
    EXPECT_TRUE(log.has("ERROR: 0:6: '1' : duplicate pixel local storage binding index"));
    EXPECT_TRUE(log.has("ERROR: 0:7: '0' : duplicate pixel local storage binding index"));

    // PLS handles cannot have duplicate binding indices.
    constexpr char kPLSIllegalLayoutQualifiers[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(foo) highp uniform pixelLocalANGLE pls1;
    layout(binding=0, location=0, rgba8ui) highp uniform upixelLocalANGLE pls2;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSIllegalLayoutQualifiers));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'foo' : invalid layout qualifier"));
    EXPECT_TRUE(
        log.has("ERROR: 0:4: 'location' : location must only be specified for a single input or "
                "output variable"));

    // Check that binding is not allowed in ES3, other than pixel local storage. ES3 doesn't have
    // blocks, and only has one opaque type: samplers. So we just need to make sure binding isn't
    // allowed on samplers.
    constexpr char kBindingOnSampler[] = R"(#version 300 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0) uniform mediump sampler2D sampler;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kBindingOnSampler));
    EXPECT_TRUE(
        log.has("ERROR: 0:3: 'binding' : invalid layout qualifier: only valid when used with pixel "
                "local storage"));

    // Binding qualifiers generate different error messages depending on ES3 and ES31.
    constexpr char kBindingOnOutput[] = R"(#version 310 es
    layout(binding=0) out mediump vec4 color;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kBindingOnOutput));
    EXPECT_TRUE(
        log.has("ERROR: 0:2: 'binding' : invalid layout qualifier: only valid when used with "
                "opaque types or blocks"));

    // Check that internalformats are not allowed in ES3 except for PLS.
    constexpr char kFormatOnSamplerES3[] = R"(#version 300 es
    layout(rgba8) uniform mediump sampler2D sampler1;
    layout(rgba8_snorm) uniform mediump sampler2D sampler2;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kFormatOnSamplerES3));
    EXPECT_TRUE(
        log.has("ERROR: 0:2: 'rgba8' : invalid layout qualifier: not supported before GLSL ES "
                "3.10, except pixel local storage"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:3: 'rgba8_snorm' : invalid layout qualifier: not supported before GLSL ES 3.10"));

    // Format qualifiers generate different error messages depending on whether they can be used
    // with PLS.
    constexpr char kFormatOnSamplerES31[] = R"(#version 310 es
    layout(rgba8) uniform mediump sampler2D sampler1;
    layout(rgba8_snorm) uniform mediump sampler2D sampler2;
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kFormatOnSamplerES31));
    EXPECT_TRUE(
        log.has("ERROR: 0:2: 'rgba8' : invalid layout qualifier: only valid when used with images "
                "or pixel local storage"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:3: 'rgba8_snorm' : invalid layout qualifier: only valid when used with images"));

    ASSERT_GL_NO_ERROR();
}

// Check proper validation of the discard statement when pixel local storage is(n't) declared.
TEST_P(PixelLocalStorageCompilerTest, Discard)
{
    // Discard is not allowed when pixel local storage has been declared. When polyfilled with
    // shader images, pixel local storage requires early_fragment_tests, which causes discard to
    // interact differently with the depth and stencil tests.
    //
    // To ensure identical behavior across all backends (some of which may not have access to
    // early_fragment_tests), we disallow discard if pixel local storage has been declared.
    constexpr char kDiscardWithPLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;
    void a()
    {
        discard;
    }
    void b();
    void main()
    {
        if (gl_FragDepth == 3.14)
            discard;
        discard;
    }
    void b()
    {
        discard;
    })";
    EXPECT_FALSE(log.compileFragmentShader(kDiscardWithPLS));
    EXPECT_TRUE(
        log.has("ERROR: 0:6: 'discard' : illegal discard when pixel local storage is declared"));
    EXPECT_TRUE(
        log.has("ERROR: 0:12: 'discard' : illegal discard when pixel local storage is declared"));
    EXPECT_TRUE(
        log.has("ERROR: 0:13: 'discard' : illegal discard when pixel local storage is declared"));
    EXPECT_TRUE(
        log.has("ERROR: 0:17: 'discard' : illegal discard when pixel local storage is declared"));

    // Discard is OK when pixel local storage has _not_ been declared.
    constexpr char kDiscardNoPLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void f(lowp pixelLocalANGLE pls);  // Function arguments don't trigger PLS restrictions.
    void a()
    {
        discard;
    }
    void b();
    void main()
    {
        if (gl_FragDepth == 3.14)
            discard;
        discard;
    }
    void b()
    {
        discard;
    })";
    EXPECT_TRUE(log.compileFragmentShader(kDiscardNoPLS));

    // Ensure discard is caught even if it happens before PLS is declared.
    constexpr char kDiscardBeforePLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void a()
    {
        discard;
    }
    void main()
    {
    }
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;)";
    EXPECT_FALSE(log.compileFragmentShader(kDiscardBeforePLS));
    EXPECT_TRUE(
        log.has("ERROR: 0:5: 'discard' : illegal discard when pixel local storage is declared"));

    ASSERT_GL_NO_ERROR();
}

// Check proper validation of the return statement when pixel local storage is(n't) declared.
TEST_P(PixelLocalStorageCompilerTest, Return)
{
    // Returning from main isn't allowed when pixel local storage has been declared.
    // (ARB_fragment_shader_interlock isn't allowed after return from main.)
    constexpr char kReturnFromMainWithPLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;
    void main()
    {
        if (gl_FragDepth == 3.14)
            return;
        return;
    })";
    EXPECT_FALSE(log.compileFragmentShader(kReturnFromMainWithPLS));
    EXPECT_TRUE(log.has(
        "ERROR: 0:7: 'return' : illegal return from main when pixel local storage is declared"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:8: 'return' : illegal return from main when pixel local storage is declared"));

    // Returning from main is OK when pixel local storage has _not_ been declared.
    constexpr char kReturnFromMainNoPLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void main()
    {
        if (gl_FragDepth == 3.14)
            return;
        return;
    })";
    EXPECT_TRUE(log.compileFragmentShader(kReturnFromMainNoPLS));

    // Returning from subroutines is OK when pixel local storage has been declared.
    constexpr char kReturnFromSubroutinesWithPLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(rgba8ui, binding=0) highp uniform upixelLocalANGLE pls;
    void a()
    {
        return;
    }
    void b();
    void main()
    {
        a();
        b();
    }
    void b()
    {
        return;
    })";
    EXPECT_TRUE(log.compileFragmentShader(kReturnFromSubroutinesWithPLS));

    // Ensure return from main is caught even if it happens before PLS is declared.
    constexpr char kDiscardBeforePLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void main()
    {
        return;
    }
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;)";
    EXPECT_FALSE(log.compileFragmentShader(kDiscardBeforePLS));
    EXPECT_TRUE(log.has(
        "ERROR: 0:5: 'return' : illegal return from main when pixel local storage is declared"));

    ASSERT_GL_NO_ERROR();
}

// Check that gl_FragDepth(EXT) and gl_SampleMask are not assignable when PLS is declared.
TEST_P(PixelLocalStorageCompilerTest, FragmentTestVariables)
{
    // gl_FragDepth is not assignable when pixel local storage has been declared. When polyfilled
    // with shader images, pixel local storage requires early_fragment_tests, which causes
    // assignments to gl_FragDepth(EXT) and gl_SampleMask to be ignored.
    //
    // To ensure identical behavior across all backends, we disallow assignment to these values if
    // pixel local storage has been declared.
    constexpr char kAssignFragDepthWithPLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void set(out mediump float x, mediump float val)
    {
        x = val;
    }
    void set2(inout mediump float x, mediump float val)
    {
        x = val;
    }
    void main()
    {
        gl_FragDepth = 0.0;
        gl_FragDepth -= 1.0;
        set(gl_FragDepth, 0.0);
        set2(gl_FragDepth, 0.1);
    }
    layout(binding=0, rgba8i) lowp uniform ipixelLocalANGLE pls;)";
    EXPECT_FALSE(log.compileFragmentShader(kAssignFragDepthWithPLS));
    EXPECT_TRUE(log.has(
        "ERROR: 0:13: 'gl_FragDepth' : value not assignable when pixel local storage is declared"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:14: 'gl_FragDepth' : value not assignable when pixel local storage is declared"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:15: 'gl_FragDepth' : value not assignable when pixel local storage is declared"));
    EXPECT_TRUE(log.has(
        "ERROR: 0:16: 'gl_FragDepth' : value not assignable when pixel local storage is declared"));

    // Assigning gl_FragDepth is OK if we don't declare any PLS.
    constexpr char kAssignFragDepthNoPLS[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void f(highp ipixelLocalANGLE pls)
    {
        // Function arguments don't trigger PLS restrictions.
        pixelLocalStoreANGLE(pls, ivec4(8));
    }
    void set(out mediump float x, mediump float val)
    {
        x = val;
    }
    void main()
    {
        gl_FragDepth = 0.0;
        gl_FragDepth /= 2.0;
        set(gl_FragDepth, 0.0);
    })";
    EXPECT_TRUE(log.compileFragmentShader(kAssignFragDepthNoPLS));

    // Reading gl_FragDepth is OK.
    constexpr char kReadFragDepth[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(r32f, binding=0) highp uniform pixelLocalANGLE pls;
    highp vec4 get(in mediump float x)
    {
        return vec4(x);
    }
    void set(inout mediump float x, mediump float val)
    {
        x = val;
    }
    void main()
    {
        pixelLocalStoreANGLE(pls, get(gl_FragDepth));
        // Check when gl_FragDepth is involved in an l-value expression, but not assigned to.
        highp float x[2];
        x[int(gl_FragDepth)] = 1.0;
        set(x[1 - int(gl_FragDepth)], 2.0);
    })";
    EXPECT_TRUE(log.compileFragmentShader(kReadFragDepth));

    if (IsGLExtensionEnabled("GL_OES_sample_variables"))
    {
        // gl_SampleMask is not assignable when pixel local storage has been declared. The shader
        // image polyfill requires early_fragment_tests, which causes gl_SampleMask to be ignored.
        //
        // To ensure identical behavior across all implementations (some of which may not have
        // access to early_fragment_tests), we disallow assignment to these values if pixel local
        // storage has been declared.
        constexpr char kAssignSampleMaskWithPLS[] = R"(#version 310 es
        #extension GL_ANGLE_shader_pixel_local_storage : require
        #extension GL_OES_sample_variables : require
        void set(out highp int x, highp int val)
        {
            x = val;
        }
        void set2(inout highp int x, highp int val)
        {
            x = val;
        }
        void main()
        {
            gl_SampleMask[0] = 0;
            gl_SampleMask[0] ^= 1;
            set(gl_SampleMask[0], 9);
            set2(gl_SampleMask[0], 10);
        }
        layout(binding=0, rgba8i) highp uniform ipixelLocalANGLE pls;)";
        EXPECT_FALSE(log.compileFragmentShader(kAssignSampleMaskWithPLS));
        EXPECT_TRUE(
            log.has("ERROR: 0:14: 'gl_SampleMask' : value not assignable when pixel local storage "
                    "is declared"));
        EXPECT_TRUE(
            log.has("ERROR: 0:15: 'gl_SampleMask' : value not assignable when pixel local storage "
                    "is declared"));
        EXPECT_TRUE(
            log.has("ERROR: 0:16: 'gl_SampleMask' : value not assignable when pixel local storage "
                    "is declared"));
        EXPECT_TRUE(
            log.has("ERROR: 0:17: 'gl_SampleMask' : value not assignable when pixel local storage "
                    "is declared"));

        // Assigning gl_SampleMask is OK if we don't declare any PLS.
        constexpr char kAssignSampleMaskNoPLS[] = R"(#version 310 es
        #extension GL_ANGLE_shader_pixel_local_storage : require
        #extension GL_OES_sample_variables : require
        void set(out highp int x, highp int val)
        {
            x = val;
        }
        void main()
        {
            gl_SampleMask[0] = 0;
            gl_SampleMask[0] ^= 1;
            set(gl_SampleMask[0], 9);
        })";
        EXPECT_TRUE(log.compileFragmentShader(kAssignSampleMaskNoPLS));

        // Reading gl_SampleMask is OK enough (even though it's technically output only).
        constexpr char kReadSampleMask[] = R"(#version 310 es
        #extension GL_ANGLE_shader_pixel_local_storage : require
        #extension GL_OES_sample_variables : require
        layout(binding=0, rgba8i) highp uniform ipixelLocalANGLE pls;
        highp int get(in highp int x)
        {
            return x;
        }
        void set(out highp int x, highp int val)
        {
            x = val;
        }
        void main()
        {
            pixelLocalStoreANGLE(pls, ivec4(get(gl_SampleMask[0]), gl_SampleMaskIn[0], 0, 1));
            // Check when gl_SampleMask is involved in an l-value expression, but not assigned to.
            highp int x[2];
            x[gl_SampleMask[0]] = 1;
            set(x[gl_SampleMask[0]], 2);
        })";
        EXPECT_TRUE(log.compileFragmentShader(kReadSampleMask));
    }

    ASSERT_GL_NO_ERROR();
}

// Check proper validation of PLS function arguments.
TEST_P(PixelLocalStorageCompilerTest, FunctionArguments)
{
    // Ensure PLS handles can't be the result of complex expressions.
    constexpr char kPLSHandleComplexExpression[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    layout(rgba8, binding=0) mediump uniform pixelLocalANGLE pls0;
    layout(rgba8, binding=1) mediump uniform pixelLocalANGLE pls1;
    void clear(mediump pixelLocalANGLE pls)
    {
        pixelLocalStoreANGLE(pls, vec4(0));
    }
    void main()
    {
        highp float x = gl_FragDepth;
        clear(((x += 50.0) < 100.0) ? pls0 : pls1);
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSHandleComplexExpression));
    EXPECT_TRUE(log.has("ERROR: 0:12: '?:' : ternary operator is not allowed for opaque types"));

    // As function arguments, PLS handles cannot have layout qualifiers.
    constexpr char kPLSFnArgWithLayoutQualifiers[] = R"(#version 310 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void f(layout(rgba8, binding=1) mediump pixelLocalANGLE pls)
    {
    }
    void g(layout(rgba8) lowp pixelLocalANGLE pls);
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kPLSFnArgWithLayoutQualifiers));
    EXPECT_TRUE(log.has("ERROR: 0:3: 'layout' : only allowed at global scope"));
    EXPECT_TRUE(log.has("ERROR: 0:6: 'layout' : only allowed at global scope"));

    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(PixelLocalStorageCompilerTest,
                       ES31_NULL().enable(Feature::EmulatePixelLocalStorage),
                       ES31_NULL()
                           .enable(Feature::EmulatePixelLocalStorage)
                           .enable(Feature::DisableDrawBuffersIndexed));

class PixelLocalStorageTestPreES3 : public ANGLETest<>
{};

// Check that GL_ANGLE_shader_pixel_local_storage is not advertised before ES 3.1.
//
// TODO(anglebug.com/7279): we can relax the min supported version once the implementation details
// are inside ANGLE.
TEST_P(PixelLocalStorageTestPreES3, UnsupportedClientVersion)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage_coherent"));

    ShaderInfoLog log;

    constexpr char kRequireUnsupportedPLS[] = R"(#version 300 es
    #extension GL_ANGLE_shader_pixel_local_storage : require
    void main()
    {
    })";
    EXPECT_FALSE(log.compileFragmentShader(kRequireUnsupportedPLS));
    EXPECT_TRUE(
        log.has("ERROR: 0:2: 'GL_ANGLE_shader_pixel_local_storage' : extension is not supported"));

    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(PixelLocalStorageTestPreES3,
                       ES1_NULL().enable(Feature::EmulatePixelLocalStorage),
                       ES2_NULL().enable(Feature::EmulatePixelLocalStorage));
