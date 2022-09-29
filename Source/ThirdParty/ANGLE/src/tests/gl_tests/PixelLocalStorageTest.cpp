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

// For building the <cleardata> parameter of glBeginPixelLocalStorageANGLE.
struct ClearValue
{
    ClearValue() = default;
    template <typename T>
    ClearValue(typename std::enable_if<sizeof(T) == 4, T>::type r, T g, T b, T a)
    {
        memcpy(mData, &r, 4);
        memcpy(mData + 1, &g, 4);
        memcpy(mData + 2, &b, 4);
        memcpy(mData + 3, &a, 4);
    }
    operator const void *() const { return mData; }
    uint32_t mData[4]{};
};
static ClearValue ClearF(float r, float g, float b, float a)
{
    return ClearValue(r, g, b, a);
}
static ClearValue ClearI(int32_t r, int32_t g, int32_t b, int32_t a)
{
    return ClearValue(r, g, b, a);
}
static ClearValue ClearUI(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return ClearValue(r, g, b, a);
}
static Array<ClearValue> ClearArray(const std::initializer_list<ClearValue> &list)
{
    return Array<ClearValue>(list);
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

    void useProgram(std::string fsMain)
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

        mProgram.makeRaster(
            R"(#version 310 es
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

            std::string(R"(#version 310 es
            #extension GL_ANGLE_shader_pixel_local_storage : require
            precision highp float;
            in vec4 color;
            in vec4 aux1;
            in vec4 aux2;)")
                .append(fsMain)
                .c_str());

        ASSERT_TRUE(mProgram.valid());

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

    void drawBoxes(std::vector<Box> boxes, UseBarriers useBarriers = UseBarriers::IfNotCoherent)
    {
        if (useBarriers == UseBarriers::IfNotCoherent &&
            !IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage_coherent"))
        {
            for (const auto &box : boxes)
            {
                glVertexAttribPointer(mLTRBLocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                      box.rect.data());
                glVertexAttribPointer(mRGBALocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                      box.color.data());
                glVertexAttribPointer(mAux1Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                      box.aux1.data());
                glVertexAttribPointer(mAux2Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                      box.aux2.data());
                glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1);
                glPixelLocalStorageBarrierANGLE();
            }
        }
        else
        {
            glVertexAttribPointer(mLTRBLocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  boxes[0].rect.data());
            glVertexAttribPointer(mRGBALocation, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  boxes[0].color.data());
            glVertexAttribPointer(mAux1Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  boxes[0].aux1.data());
            glVertexAttribPointer(mAux2Location, 4, GL_FLOAT, GL_FALSE, sizeof(Box),
                                  boxes[0].aux2.data());
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, boxes.size());
        }
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
                R"(#version 310 es
                precision highp float;
                out vec2 texcoord;
                void main()
                {
                    texcoord.x = (gl_VertexID & 1) == 0 ? 0.0 : 1.0;
                    texcoord.y = (gl_VertexID & 2) == 0 ? 0.0 : 1.0;
                    gl_Position = vec4(texcoord * 2.0 - 1.0, 0, 1);
                })";

            constexpr char kFS[] =
                R"(#version 310 es
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

    GLint MAX_PIXEL_LOCAL_STORAGE_PLANES                           = 0;
    GLint MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE    = 0;
    GLint MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES = 0;
    GLint MAX_COLOR_ATTACHMENTS                                    = 0;
    GLint MAX_DRAW_BUFFERS                                         = 0;

    GLProgram mProgram;

    GLint mWidthUniform  = -1;
    GLint mHeightUniform = -1;

    GLint mLTRBLocation = -1;
    GLint mRGBALocation = -1;
    GLint mAux1Location = -1;
    GLint mAux2Location = -1;

    GLuint mScratchFBO = 0;
    GLProgram mRenderTextureProgram;
};

// Verify conformant implementation-dependent PLS limits.
TEST_P(PixelLocalStorageTest, ImplementationDependentLimits)
{
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

    useProgram(R"(
        layout(binding=0, rgba8) lowp uniform pixelLocalANGLE plane1;
        layout(rgba8i, binding=1) lowp uniform ipixelLocalANGLE plane2;
        layout(binding=2, rgba8ui) lowp uniform upixelLocalANGLE plane3;
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

    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}), nullptr);

    // Accumulate R, G, B, A in 4 separate passes.
    // Store out-of-range values to ensure they are properly clamped upon storage.
    drawBoxes({{FULLSCREEN, {2, -1, -2, -3}, {-500, 0, 0, 0}, {1, 0, 0, 0}},
               {FULLSCREEN, {0, 1, 0, 100}, {0, -129, 0, 0}, {0, 50, 0, 0}},
               {FULLSCREEN, {0, 0, 1, 0}, {0, 0, -70, 0}, {0, 0, 100, 0}},
               {FULLSCREEN, {0, 0, 0, -1}, {128, 0, 0, 500}, {0, 0, 0, 300}}});

    glEndPixelLocalStorageANGLE();

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
    useProgram(R"(
        layout(r32f, binding=0) highp uniform pixelLocalANGLE plane1;
        layout(binding=1, r32ui) highp uniform upixelLocalANGLE plane2;
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

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}), nullptr);

    // Accumulate R in 4 separate passes.
    drawBoxes({{FULLSCREEN, {-1.5, 0, 0, 0}, {0x000000ff, 0, 0, 0}},
               {FULLSCREEN, {-10.25, 0, 0, 0}, {0x0000ff00, 0, 0, 0}},
               {FULLSCREEN, {-100, 0, 0, 0}, {0x00ff0000, 0, 0, 0}},
               {FULLSCREEN, {.25, 0, 0, 0}, {0xff000000, 0, 0, 22}}});

    glEndPixelLocalStorageANGLE();

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

// Check proper functioning of the <cleardata> parameter of BeginPixelLocalStorageANGLE.
TEST_P(PixelLocalStorageTest, ClearData)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    // Scissor, viewport, and clear color should not affect GL_CLEAR_ANGLE loads.
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 1, 1);
    glViewport(0, 0, 1, 1);
    glClearColor(.1f, .2f, .3f, .4f);

    PLSTestTexture tex8f(GL_RGBA8);
    PLSTestTexture tex8i(GL_RGBA8I);
    PLSTestTexture tex8ui(GL_RGBA8UI);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex8f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex8i, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex8ui, 0, 0);
    auto clearLoads = GLenumArray({GL_CLEAR_ANGLE, GL_CLEAR_ANGLE, GL_CLEAR_ANGLE});

    // Test custom RGBA8 clear values.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBeginPixelLocalStorageANGLE(
        3, clearLoads, ClearArray({ClearF(1, 0, 0, 0), ClearI(-1, 2, -3, 4), ClearUI(5, 6, 7, 8)}));
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(255, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(-1, 2, -3, 4));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(5, 6, 7, 8));

    // Rotate and test again.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex8f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex8i, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex8ui, 0, 0);
    // If any component of the clear value is larger than can be represented in plane's
    // internalformat, it is clamped.
    glBeginPixelLocalStorageANGLE(3, clearLoads,
                                  ClearArray({ClearUI(254, 255, 256, 257), ClearF(-1, 0, 1, 2),
                                              ClearI(-129, -128, 127, 128)}));
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(254, 255, 255, 255));
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 255, 255));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(-128, -128, 127, 127));

    // GL_ZERO shouldn't be affected by previous clear colors.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}), nullptr);
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(tex8f);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8i);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(tex8ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0, 0, 0, 0));

    // Test custom R32 clear values.
    PLSTestTexture tex32f(GL_R32F);
    PLSTestTexture tex32ui(GL_R32UI);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex32f, 0, 0);
    glFramebufferTexturePixelLocalStorageANGLE(2, tex32ui, 0, 0);
    glBeginPixelLocalStorageANGLE(
        3, GLenumArray({GL_CLEAR_ANGLE, GL_DISABLE_ANGLE, GL_CLEAR_ANGLE}),
        ClearArray({ClearF(-100.5, 1, 1, 0), ClearValue(), ClearUI(0xbaadbeef, 1, 1, 0)}));
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(tex32f);
    EXPECT_PIXEL_RECT32F_EQ(0, 0, W, H, GLColor32F(-100.5, 0, 0, 1));
    attachTextureToScratchFBO(tex32ui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0xbaadbeef, 0, 0, 1));
}

// Check proper support of GL_ZERO, GL_KEEP, GL_CLEAR_ANGLE, and GL_DISABLE_ANGLE loadOps. Also
// verify that it works do draw with GL_MAX_LOCAL_STORAGE_PLANES_ANGLE planes.
TEST_P(PixelLocalStorageTest, LoadOps)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    std::stringstream fs;
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "layout(binding=" << i << ", rgba8) highp uniform pixelLocalANGLE pls" << i << ";\n";
    }
    fs << "void main() {\n";
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "pixelLocalStoreANGLE(pls" << i << ", color + pixelLocalLoadANGLE(pls" << i
           << "));\n";
    }
    fs << "}";
    useProgram(fs.str().c_str());

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
    std::vector<ClearValue> cleardata(MAX_PIXEL_LOCAL_STORAGE_PLANES);
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        glFramebufferTexturePixelLocalStorageANGLE(i, texs[i], 0, 0);
        loadOps[i]   = (i & 1) ? GL_CLEAR_ANGLE : GL_KEEP;
        cleardata[i] = ClearF(0, 0, 0, 1);
    }
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // Draw transparent green into all pls attachments.
    glBeginPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, loadOps.data(), cleardata.data());
    drawBoxes({{{FULLSCREEN}, {0, 1, 0, 0}}});
    glEndPixelLocalStorageANGLE();

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
    glBeginPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, loadOps.data(), cleardata.data());
    glEndPixelLocalStorageANGLE();

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
        glViewport(0, 0, W, H);
        glDrawBuffers(0, nullptr);
    }
};
constexpr static int FRAG_REJECT_TEST_WIDTH  = 64;
constexpr static int FRAG_REJECT_TEST_HEIGHT = 64;
constexpr static PixelLocalStorageTest::Box FRAG_REJECT_TEST_BOX(
    FULLSCREEN,
    {0, 1, 0, 0},                                              // draw color
    {0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT});  // reject pixels outside aux1
#define FRAG_REJECT_TEST_CLEAR ClearF(0, 0, 0, 1)

// Check that the stencil test prevents stores to PLS.
TEST_P(PixelLocalStorageTest, FragmentReject_stencil)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(tex);

    useProgram(R"(
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;
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
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    // glStencilFunc(GL_NEVER, ...) should not update pls.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), FRAG_REJECT_TEST_CLEAR);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NEVER, 1, ~0u);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    drawBoxes({{{0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT}}});
    glEndPixelLocalStorageANGLE();
    glDisable(GL_STENCIL_TEST);
    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::black);

    // Stencil should be preserved after PLS, and only pixels that pass the stencil test should
    // update PLS next.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glEnable(GL_STENCIL_TEST);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), FRAG_REJECT_TEST_CLEAR);
    glStencilFunc(GL_NOTEQUAL, 0, ~0u);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    drawBoxes({FRAG_REJECT_TEST_BOX});
    glDisable(GL_STENCIL_TEST);
    glEndPixelLocalStorageANGLE();
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

    useProgram(R"(
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;
    void main()
    {
        pixelLocalStoreANGLE(pls, pixelLocalLoadANGLE(pls) + color);
    })");
    GLuint depth;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), FRAG_REJECT_TEST_CLEAR);
    glClearDepthf(0.f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT);
    glClearDepthf(1.f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glDepthFunc(GL_LESS);
    drawBoxes({FRAG_REJECT_TEST_BOX});
    glEndPixelLocalStorageANGLE();
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

    useProgram(R"(
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;
    void main()
    {
        vec4 dst = pixelLocalLoadANGLE(pls);
        pixelLocalStoreANGLE(pls, color + dst);
    })");
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), FRAG_REJECT_TEST_CLEAR);
    glViewport(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT);
    drawBoxes({FRAG_REJECT_TEST_BOX});
    glEndPixelLocalStorageANGLE();
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

    useProgram(R"(
    layout(binding=0, r32f) highp uniform pixelLocalANGLE framebuffer;
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
    ClearValue clearColor = ClearF(1, 0, 0, 0);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // First make sure it works properly with a barrier.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), clearColor);
    drawBoxes(boxesA_100, UseBarriers::No);
    glPixelLocalStorageBarrierANGLE();
    drawBoxes(boxesB_7, UseBarriers::No);
    glEndPixelLocalStorageANGLE();

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
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), clearColor);
    drawBoxes(boxesA_100, UseBarriers::No);
    // OOPS! We forgot to insert a barrier!
    drawBoxes(boxesB_7, UseBarriers::No);
    glEndPixelLocalStorageANGLE();

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

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}), nullptr);

    // Draw into memoryless storage.
    useProgram(R"(
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE memoryless;
    void main()
    {
        pixelLocalStoreANGLE(memoryless, color + pixelLocalLoadANGLE(memoryless));
    })");

    drawBoxes({{{0, 20, W, H}, {1, 0, 0, 0}},
               {{0, 40, W, H}, {0, 1, 0, 0}},
               {{0, 60, W, H}, {0, 0, 1, 0}}});

    // Transfer to a texture.
    useProgram(R"(
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE framebuffer;
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE memoryless;
    void main()
    {
        pixelLocalStoreANGLE(framebuffer, vec4(1) - pixelLocalLoadANGLE(memoryless));
    })");

    drawBoxes({{FULLSCREEN}});

    glEndPixelLocalStorageANGLE();

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
            fs << "layout(binding=" << i << ", rgba8ui) highp uniform upixelLocalANGLE pls" << i
               << ";\n";
        }
        for (int i = 0; i < numDrawBuffers; ++i)
        {
            fs << "layout(location=" << i << ") out uvec4 out" << i << ";\n";
        }
        fs << "void main() {\n";
        for (int i = 0; i < numPLSPlanes; ++i)
        {
            fs << "pixelLocalStoreANGLE(pls" << i << ", uvec4(color) - uvec4(" << i << "));\n";
        }
        for (int i = 0; i < numDrawBuffers; ++i)
        {
            fs << "out" << i << " = uvec4(aux1) + uvec4(" << i << ");\n";
        }
        fs << "}";
        useProgram(fs.str().c_str());

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
                                      std::vector<GLenum>(numPLSPlanes, GL_ZERO).data(), nullptr);
        drawBoxes({{FULLSCREEN, {255, 254, 253, 252}, {0, 1, 2, 3}}});
        glEndPixelLocalStorageANGLE();

        for (int i = 0; i < numPLSPlanes; ++i)
        {
            attachTextureToScratchFBO(localTexs[i]);
            EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H,
                                     GLColor32UI(255u - i, 254u - i, 253u - i, 252u - i));
        }
        for (int i = 0; i < numDrawBuffers; ++i)
        {
            attachTextureToScratchFBO(renderTexs[i]);
            EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0u + i, 1u + i, 2u + i, 3u + i));
        }

        ASSERT_GL_NO_ERROR();
    }
}

// Check that the pls is preserved when a shader does not call pixelLocalStoreANGLE(). (Whether
// that's because a conditional branch failed or because the shader didn't write to it at all.) It's
// conceivable that an implementation may need to be careful to preserve the pls contents in this
// scenario.
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

    // Leave unit 0 with the default clear value of zero.
    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_CLEAR_ANGLE, GL_CLEAR_ANGLE}),
                                  ClearArray({ClearValue(), ClearF(0, 1, 0, 0)}));

    // Pass 1: draw to memoryless conditionally.
    useProgram(R"(
    layout(binding=0, r32f) highp uniform pixelLocalANGLE memoryless;
    void main()
    {
        // Omit braces on the 'if' to ensure proper insertion of memoryBarriers in the translator.
        if (gl_FragCoord.x < 64.0)
            pixelLocalStoreANGLE(memoryless, vec4(1, -.1, .2, -.3));  // Only stores r.
    })");
    drawBoxes({{FULLSCREEN}});

    // Pass 2: draw to tex conditionally.
    useProgram(R"(
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE tex;
    void main()
    {
        // Omit braces on the 'if' to ensure proper insertion of memoryBarriers in the translator.
        if (gl_FragCoord.y < 64.0)
            pixelLocalStoreANGLE(tex, vec4(0, 1, 1, 0));
    })");
    drawBoxes({{FULLSCREEN}});

    // Pass 3: combine memoryless and tex.
    useProgram(R"(
    layout(binding=0, r32f) highp uniform pixelLocalANGLE memoryless;
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE tex;
    void main()
    {
        pixelLocalStoreANGLE(tex, pixelLocalLoadANGLE(tex) + pixelLocalLoadANGLE(memoryless));
    })");
    drawBoxes({{FULLSCREEN}});

    glEndPixelLocalStorageANGLE();

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, 64, 64, GLColor(255, 255, 255, 255));
    EXPECT_PIXEL_RECT_EQ(64, 0, W - 64, 64, GLColor(0, 255, 255, 255));
    EXPECT_PIXEL_RECT_EQ(0, 64, 64, H - 64, GLColor(255, 255, 0, 255));
    EXPECT_PIXEL_RECT_EQ(64, 64, W - 64, H - 64, GLColor(0, 255, 0, 255));

    ASSERT_GL_NO_ERROR();

    // Now treat "tex" as entirely readonly for an entire local storage render pass.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture rttex(GL_RGBA8);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rttex, 0);
    glDrawBuffers(1, GLenumArray({GL_COLOR_ATTACHMENT0}));
    glClear(GL_COLOR_BUFFER_BIT);

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_DISABLE_ANGLE, GL_KEEP}), nullptr);

    useProgram(R"(
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE tex;
    out vec4 fragcolor;
    void main()
    {
        fragcolor = 1.0 - pixelLocalLoadANGLE(tex);
    })");
    drawBoxes({{FULLSCREEN}});

    glEndPixelLocalStorageANGLE();

    // Ensure "tex" was properly read in the shader.
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
    useProgram(R"(
    layout(binding=0, rgba8ui) highp uniform upixelLocalANGLE fibonacci;
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

    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
    drawBoxes({{FULLSCREEN}});
    glEndPixelLocalStorageANGLE();

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(233, 144, 89, 55));  // fib(13, 12, 11, 10)

    ASSERT_GL_NO_ERROR();

    // Now verify that r32f and r32ui still reload as (r, 0, 0, 1), even after an in-shader store.
    useProgram(R"(
    layout(binding=0, r32f) highp uniform pixelLocalANGLE pls32f;
    layout(binding=1, r32ui) highp uniform upixelLocalANGLE pls32ui;
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

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}), nullptr);
    drawBoxes({{FULLSCREEN}});
    glEndPixelLocalStorageANGLE();

    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(255, 0, 0, 255));
    ASSERT_GL_NO_ERROR();
}

// Check that PLS handles can be passed as function arguments.
TEST_P(PixelLocalStorageTest, FunctionArguments)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    useProgram(R"(
    layout(binding=0, rgba8) lowp uniform pixelLocalANGLE dst;
    layout(binding=1, rgba8) mediump uniform pixelLocalANGLE src1;
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
    layout(binding=2, r32f) highp uniform pixelLocalANGLE src2;
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

    glBeginPixelLocalStorageANGLE(
        3, GLenumArray({GL_ZERO, GL_CLEAR_ANGLE, GL_CLEAR_ANGLE}),
        ClearArray({ClearF(1, 1, 1, 1) /*ignored*/, ClearF(0, 1, 1, 0), ClearF(1, 0, 0, 1)}));
    drawBoxes({{FULLSCREEN}});
    glEndPixelLocalStorageANGLE();

    attachTextureToScratchFBO(dst);
    EXPECT_PIXEL_RECT_EQ(0, 0, 25, H, GLColor(0, 255, 255, 0));
    EXPECT_PIXEL_RECT_EQ(25, 0, W - 25, H, GLColor(255, 0, 0, 255));

    ASSERT_GL_NO_ERROR();
}

// Check that early_fragment_tests are not triggered when PLS uniforms are not declared.
TEST_P(PixelLocalStorageTest, EarlyFragmentTests)
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

// Check that if the "_coherent" extension is advertised, PLS operations are ordered and coherent.
TEST_P(PixelLocalStorageTest, Coherency)
{
    // We could run this test with barriers and non-coherent, but it takes an extremely long time.
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage_coherent"));

    useProgram(R"(
    layout(binding=0, rgba8ui) lowp uniform upixelLocalANGLE framebuffer;
    layout(binding=1, rgba8) lowp uniform pixelLocalANGLE tmp;
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

    // Prepare a ton of random sized boxes in various draws.
    std::vector<Box> boxesList[5];
    srand(17);
    uint32_t boxID = 1;
    for (auto &boxes : boxesList)
    {
        for (int i = 0; i < H * W * 11; ++i)
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

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}), nullptr);
    for (const std::vector<Box> &boxes : boxesList)
    {
        drawBoxes(boxes);
    }
    glEndPixelLocalStorageANGLE();

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

    useProgram(R"(
        layout(binding=0, rgba8) lowp uniform pixelLocalANGLE pls;
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

        glUniform1f(mWidthUniform, levelWidth);
        glUniform1f(mHeightUniform, levelHeight);
        glViewport(0, 0, levelWidth, levelHeight);
        glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, levelWidth, levelHeight, GL_RGBA,
                        GL_UNSIGNED_BYTE, redData.data());

        glFramebufferTexturePixelLocalStorageANGLE(0, tex, level, 0);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}), nullptr);
        drawBoxes({{{0, 0, (float)levelWidth - 3, (float)levelHeight}, {0, 0, 1, 0}},
                   {{0, 0, (float)levelWidth - 2, (float)levelHeight}, {0, 1, 0, 0}},
                   {{0, 0, (float)levelWidth - 1, (float)levelHeight}, {1, 0, 0, 0}}});
        glEndPixelLocalStorageANGLE();
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

// Check that application-facing state is not perturbed by pixel local storage.
TEST_P(PixelLocalStorageTest, StateRestoration)
{
    // Setup state.
    PLSTestTexture plsTex(GL_RGBA8UI, 32, 33);
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glViewport(1, 1, 5, 5);
    glScissor(2, 2, 4, 4);
    glEnable(GL_SCISSOR_TEST);

    std::vector<GLenum> drawBuffers(MAX_DRAW_BUFFERS);
    for (int i = 0; i < MAX_DRAW_BUFFERS; ++i)
    {
        drawBuffers[i] = i % 3 == 0 ? GL_NONE : GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(MAX_DRAW_BUFFERS, drawBuffers.data());

    GLenum accesses[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};
    GLenum formats[]  = {GL_RGBA8, GL_R32UI, GL_R32I, GL_R32F};
    std::vector<GLTexture> images;
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        GLuint tex = images.emplace_back();
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 3, GL_RGBA8, 8, 8, 5);
        GLboolean layered = i % 2;
        glBindImageTexture(i, images.back(), i % 3, layered, layered == GL_FALSE ? i % 5 : 0,
                           accesses[i % 3], formats[i % 4]);
    }

    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 17);
    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 1);

    PLSTestTexture boundTex(GL_RGBA8, 1, 1);
    glBindTexture(GL_TEXTURE_2D, boundTex);

    // Run pixel local storage.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8);
    glFramebufferTexturePixelLocalStorageANGLE(1, plsTex, 0, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(2, GL_RGBA8);
    glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_RGBA8);
    glBeginPixelLocalStorageANGLE(4, GLenumArray({GL_ZERO, GL_KEEP, GL_CLEAR_ANGLE, GL_DONT_CARE}),
                                  ClearArray({ClearValue(), ClearValue(), ClearF(.1, .2, .3, .4)}));
    glPixelLocalStorageBarrierANGLE();
    glEndPixelLocalStorageANGLE();

    // Check state.
    GLint textureBinding2D;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding2D);
    EXPECT_EQ(static_cast<GLuint>(textureBinding2D), boundTex);

    GLint defaultWidth, defaultHeight;
    glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, &defaultWidth);
    glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, &defaultHeight);
    EXPECT_EQ(defaultWidth, 17);
    EXPECT_EQ(defaultHeight, 1);

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
        EXPECT_EQ(static_cast<GLuint>(access), accesses[i % 3]);
        EXPECT_EQ(static_cast<GLuint>(format), formats[i % 4]);
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

// Check that PLS gets properly cleaned up when its framebuffer and textures are never deleted.
TEST_P(PixelLocalStorageTest, LeakFramebufferAndTexture)
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

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(PixelLocalStorageTest);
ANGLE_INSTANTIATE_TEST(PixelLocalStorageTest,
                       // D3D coherent.
                       ES31_D3D11().enable(Feature::EmulatePixelLocalStorage),
                       // D3D noncoherent.
                       ES31_D3D11()
                           .enable(Feature::DisableRasterizerOrderViews)
                           .enable(Feature::EmulatePixelLocalStorage),
                       // OpenGL coherent.
                       ES31_OPENGL().enable(Feature::EmulatePixelLocalStorage),
                       // OpenGL noncoherent.
                       ES31_OPENGL()
                           .enable(Feature::EmulatePixelLocalStorage)
                           .disable(Feature::SupportsFragmentShaderInterlockNV)
                           .disable(Feature::SupportsFragmentShaderOrderingINTEL)
                           .disable(Feature::SupportsFragmentShaderInterlockARB),
                       // OpenGL ES noncoherent.
                       ES31_OPENGLES().enable(Feature::EmulatePixelLocalStorage),
                       // Vulkan coherent.
                       ES31_VULKAN().enable(Feature::EmulatePixelLocalStorage),
                       // Vulkan noncoherent.
                       ES31_VULKAN()
                           .disable(Feature::SupportsFragmentShaderPixelInterlock)
                           .enable(Feature::EmulatePixelLocalStorage),
                       // Vulkan coherent, GLSL instead of SPIR-V: The coherent version of the
                       // extension relies on ARB_fragment_shader_interlock. Ensure it works in
                       // Vulkan GLSL.
                       ES31_VULKAN()
                           .enable(Feature::AsyncCommandQueue)
                           .enable(Feature::EmulatePixelLocalStorage)
                           .enable(Feature::GenerateSPIRVThroughGlslang),
                       // Swiftshader.
                       ES31_VULKAN_SWIFTSHADER()
                           .enable(Feature::AsyncCommandQueue)
                           .enable(Feature::EmulatePixelLocalStorage));

class PixelLocalStorageValidationTest : public ANGLETest<>
{
  protected:
    void testSetUp() override
    {
        // INVALID_OPERATION is generated if DITHER is enabled.
        glDisable(GL_DITHER);

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

        ANGLETest::testSetUp();
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

#define ASSERT_GL_BOOLEAN(pname, expected)     \
    {                                          \
        GLboolean value;                       \
        glGetBooleanv(pname, &value);          \
        ASSERT_EQ(value, GLboolean(expected)); \
    }

#define EXPECT_GL_BOOLEAN(pname, expected)     \
    {                                          \
        GLboolean value;                       \
        glGetBooleanv(pname, &value);          \
        EXPECT_EQ(value, GLboolean(expected)); \
    }

#define EXPECT_GL_INTEGER_INDEXED(target, index, expected) \
    {                                                      \
        GLint value;                                       \
        glGetIntegeri_v(target, index, &value);            \
        EXPECT_EQ(value, GLint(expected));                 \
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
    // It's valid to query GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE even when fbo 0 is bound.
    EXPECT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    EXPECT_GL_NO_ERROR();

    // Table 6.Y: Pixel Local Storage State
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    for (int i = 0; i < MAX_PIXEL_LOCAL_STORAGE_PLANES; ++i)
    {
        EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, i, GL_NONE);
        EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, i, 0);
        EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, i, 0);
        EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, i, 0);
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
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_R32F);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0, GL_NONE);

    // If <internalformat> is NONE, the pixel local storage plane at index <plane> is deinitialized
    // and any internal storage is released.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_NONE);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0, GL_NONE);

    // Set back to GL_RGBA8I.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8I);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_RGBA8I);

    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_R32UI);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_RGBA8I);

    // INVALID_VALUE is generated if <plane> < 0 or <plane> >= MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glFramebufferMemorylessPixelLocalStorageANGLE(-1, GL_R32UI);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");
    glFramebufferMemorylessPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_R32UI);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    glFramebufferMemorylessPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, GL_R32UI);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, MAX_PIXEL_LOCAL_STORAGE_PLANES - 1,
                              GL_R32UI);

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

    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_RGBA8I);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0, GL_NONE);

    ASSERT_GL_NO_ERROR();
}

// Check that glFramebufferTexturePixelLocalStorageANGLE validates as specified.
TEST_P(PixelLocalStorageValidationTest, FramebufferTexturePixelLocalStorageANGLE)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Initially, pixel local storage planes are in a deinitialized state and are unusable.
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 1, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 1, 0);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8UI, 10, 10);
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 1, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 1, GL_RGBA8UI);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 1, tex);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1, 1);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 1, 0);

    // If <backingtexture> is 0, <level> and <layer> are ignored and the pixel local storage plane
    // <plane> is deinitialized.
    glFramebufferTexturePixelLocalStorageANGLE(1, 0, 1, 2);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 1, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 1, 0);

    // Set back to GL_RGBA8I.
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 1, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 1, GL_RGBA8UI);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 1, tex);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1, 1);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 1, 0);

    // When a texture object is deleted, any pixel local storage plane to which it was bound is
    // automatically converted to a memoryless plane of matching internalformat.
    tex.reset();
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 1, GL_RGBA8UI);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 1, 0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8UI, 10, 10);

    // Same as above, but with orphaning.
    glFramebufferTexturePixelLocalStorageANGLE(1, tex, 1, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 1, GL_RGBA8UI);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 1, tex);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1, 1);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 1, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    tex.reset();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 1, GL_RGBA8UI);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 1, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 1, 0);
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

    // INVALID_VALUE is generated if <plane> < 0 or <plane> >= MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glFramebufferTexturePixelLocalStorageANGLE(-1, tex, 1, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane cannot be less than 0.");
    glFramebufferTexturePixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, tex, 1, 0);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Plane must be less than GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    glFramebufferTexturePixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, tex, 2, 0);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, MAX_PIXEL_LOCAL_STORAGE_PLANES - 1,
                              tex);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE,
                              MAX_PIXEL_LOCAL_STORAGE_PLANES - 1, 2);

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
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_NONE);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0, 0);

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
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 2, GL_RGBA8);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 2, texCube);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 2, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 2, 5);
    texCube.reset();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 2, GL_RGBA8);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 2, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 2, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 2, 0);

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
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 2, GL_RGBA8I);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 2, tex2DArray);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 2, 1);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 2, 6);
    tex2DArray.reset();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 2, GL_RGBA8I);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 2, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 2, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 2, 0);

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
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_RGBA8I);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0, tex3D);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0, 2);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0, 255);
    tex3D.reset();
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_FORMAT_ANGLE, 0, GL_RGBA8I);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, 0, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, 0, 0);
    EXPECT_GL_INTEGER_INDEXED(GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE, 0, 0);

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

// Check that glBeginPixelLocalStorageANGLE validates non-PLS context state as specified.
TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_context_state)
{
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture pls0(GL_RGBA8, 100, 100);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_TRUE);

    // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE is TRUE.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Operation not permitted while pixel local storage is active.");
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_TRUE);
    glEndPixelLocalStorageANGLE();
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

    // INVALID_OPERATION is generated if the value of SAMPLE_BUFFERS is 1 (i.e., if rendering to a
    // multisampled framebuffer).
    {
        GLRenderbuffer msaa;
        glBindRenderbuffer(GL_RENDERBUFFER, msaa);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA8, 100, 100);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaa);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with a multisampled framebuffer.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }

    // INVALID_OPERATION is generated if DITHER is enabled.
    {
        ScopedEnable scopedEnable(GL_DITHER);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_DITHER enabled.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }

    // INVALID_OPERATION is generated if RASTERIZER_DISCARD is enabled.
    {
        ScopedEnable scopedEnable(GL_RASTERIZER_DISCARD);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_RASTERIZER_DISCARD enabled.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }

    // INVALID_OPERATION is generated if SAMPLE_ALPHA_TO_COVERAGE is enabled.
    {
        ScopedEnable scopedEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_SAMPLE_ALPHA_TO_COVERAGE enabled.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }

    // INVALID_OPERATION is generated if SAMPLE_COVERAGE is enabled.
    {
        ScopedEnable scopedEnable(GL_SAMPLE_COVERAGE);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to begin pixel local storage with GL_SAMPLE_COVERAGE enabled.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }
}

// Check that glBeginPixelLocalStorageANGLE validates the draw framebuffer's state as specified.
TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_framebuffer_state)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture pls0(GL_RGBA8, 100, 100);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);

    // INVALID_VALUE is generated if <planes> < 1 or <planes> >
    // MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    glBeginPixelLocalStorageANGLE(0, nullptr, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Planes must be greater than 0.");
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

    glBeginPixelLocalStorageANGLE(
        MAX_PIXEL_LOCAL_STORAGE_PLANES + 1,
        std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES + 1, GL_DISABLE_ANGLE).data(), nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Planes must be less than or equal to GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

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
            glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_DISABLE_ANGLE}), nullptr);
            EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
            EXPECT_GL_SINGLE_ERROR_MSG(
                "Framebuffer cannot have images attached to color attachment points on or after "
                "COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE.");
            ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
        }
    }

    // INVALID_FRAMEBUFFER_OPERATION is generated if the draw framebuffer has an image attached to
    // any color attachment point on or after:
    //
    //   COLOR_ATTACHMENT0 +
    //   MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE -
    //   <planes>
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
                std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_DISABLE_ANGLE).data(),
                nullptr);
            EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
            EXPECT_GL_SINGLE_ERROR_MSG(
                "Framebuffer cannot have images attached to color attachment points on or after "
                "COLOR_ATTACHMENT0 + "
                "MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE - <planes>.");
            ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
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
    std::vector<std::array<uint32_t, 4>> cleardata(MAX_PIXEL_LOCAL_STORAGE_PLANES);

    // INVALID_VALUE is generated if <loadops> is NULL.
    glBeginPixelLocalStorageANGLE(1, nullptr, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("loadops cannot null.");

    // INVALID_ENUM is generated if <loadops>[0..<planes>-1] is not one of the Load Operations
    // enumerated in Table X.1.
    {
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}), nullptr);

        EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
        EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Load Operation.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

        std::vector<GLenum> loadops(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_DISABLE_ANGLE);
        loadops.back() = GL_SCISSOR_BOX;
        glBeginPixelLocalStorageANGLE(MAX_PIXEL_LOCAL_STORAGE_PLANES, loadops.data(), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_ENUM);
        EXPECT_GL_SINGLE_ERROR_MSG("Invalid pixel local storage Load Operation.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }

    // INVALID_OPERATION is generated if <loadops>[0..<planes>-1] is not DISABLE_ANGLE, and the
    // pixel local storage plane at that same index is is in a deinitialized state.
    {
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), cleardata.data());
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE();
        EXPECT_GL_NO_ERROR();

        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_CLEAR_ANGLE, GL_DONT_CARE}),
                                      cleardata.data());
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to enable a pixel local storage plane that is in a deinitialized state.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

        // If <backingtexture> is 0, <level> and <layer> are ignored and the pixel local storage
        // plane <plane> is deinitialized.
        glFramebufferTexturePixelLocalStorageANGLE(0, 0, -1, 999);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), cleardata.data());
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to enable a pixel local storage plane that is in a deinitialized state.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

        glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE();
        EXPECT_GL_NO_ERROR();

        // If <internalformat> is NONE, the pixel local storage plane at index <plane> is
        // deinitialized and any internal storage is released.
        glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_NONE);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_CLEAR_ANGLE}), cleardata.data());
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Attempted to enable a pixel local storage plane that is in a deinitialized state.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }

    // INVALID_VALUE is generated if <loadops>[0..<planes>-1] is CLEAR_ANGLE and <cleardata> is
    // NULL.
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);
    glFramebufferMemorylessPixelLocalStorageANGLE(1, GL_RGBA8);
    glFramebufferMemorylessPixelLocalStorageANGLE(2, GL_RGBA8);
    glFramebufferMemorylessPixelLocalStorageANGLE(3, GL_RGBA8);
    glBeginPixelLocalStorageANGLE(4, GLenumArray({GL_KEEP, GL_ZERO, GL_DONT_CARE, GL_CLEAR_ANGLE}),
                                  nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "cleardata cannot null if Load Operation GL_CLEAR_ANGLE is specified.");
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

    // INVALID_OPERATION is generated if <loadops>[0..<planes>-1] is KEEP and the pixel local
    // storage plane at that same index is memoryless.
    glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_KEEP}), nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Load Operation GL_KEEP is invalid for memoryless planes.");
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
}

// Check that glBeginPixelLocalStorageANGLE validates the pixel local storage planes as specified.
TEST_P(PixelLocalStorageValidationTest, BeginPixelLocalStorageANGLE_pls_planes)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    PLSTestTexture pls0(GL_RGBA8, 100, 100);
    glFramebufferTexturePixelLocalStorageANGLE(0, pls0, 0, 0);
    std::vector<std::array<uint32_t, 4>> cleardata(MAX_PIXEL_LOCAL_STORAGE_PLANES);

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
        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_DISABLE_ANGLE, GL_KEEP}),
                                      nullptr);
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE();
        EXPECT_GL_NO_ERROR();

        // Enabling the mismatched size plane errors.
        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_KEEP, GL_KEEP}), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG("Mismatched pixel local storage backing texture sizes.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

        // Converting the mismatched size plane to memoryless also works.
        pls1.reset();
        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_KEEP, GL_ZERO, GL_KEEP}), nullptr);
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE();
        EXPECT_GL_NO_ERROR();

        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
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
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_CLEAR_ANGLE}), cleardata.data());
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Pixel local storage backing texture dimensions not equal to the rendering area.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

        // rt1 is taller than the PLS extents.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt1, 0);
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_CLEAR_ANGLE}), cleardata.data());
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Pixel local storage backing texture dimensions not equal to the rendering area.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

        // The intersection of rt0 and rt1 is equal to the PLS extents.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, rt0, 0);
        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_KEEP, GL_CLEAR_ANGLE}), cleardata.data());
        EXPECT_GL_NO_ERROR();
        glEndPixelLocalStorageANGLE();

        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    }

    // INVALID_OPERATION is generated if the draw framebuffer has no attachments and no enabled,
    // texture-backed pixel local storage planes.
    {
        glBeginPixelLocalStorageANGLE(
            MAX_PIXEL_LOCAL_STORAGE_PLANES,
            std::vector<GLenum>(MAX_PIXEL_LOCAL_STORAGE_PLANES, GL_DISABLE_ANGLE).data(), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Draw framebuffer has no attachments and no enabled, texture-backed pixel local "
            "storage "
            "planes.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

        glFramebufferMemorylessPixelLocalStorageANGLE(0, GL_RGBA8);
        glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_DONT_CARE}), nullptr);
        EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
        EXPECT_GL_SINGLE_ERROR_MSG(
            "Draw framebuffer has no attachments and no enabled, texture-backed pixel local "
            "storage "
            "planes.");
        ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
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
    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is bound
    // to DRAW_FRAMEBUFFER.
    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG(
        "Default framebuffer object name 0 does not support pixel local storage.");

    // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE is FALSE.
    GLFramebuffer fbo;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

    glEndPixelLocalStorageANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glBeginPixelLocalStorageANGLE(0, nullptr, nullptr);
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_VALUE);
    EXPECT_GL_SINGLE_ERROR_MSG("Planes must be greater than 0.");
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glEndPixelLocalStorageANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    PLSTestTexture tex(GL_RGBA8);
    glFramebufferTexturePixelLocalStorageANGLE(0, tex, 0, 0);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}), nullptr);
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_TRUE);

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_NO_ERROR();

    glEndPixelLocalStorageANGLE();
    EXPECT_GL_NO_ERROR();

    ASSERT_GL_BOOLEAN(GL_PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE, GL_FALSE);
    EXPECT_GL_NO_ERROR();

    glEndPixelLocalStorageANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

    glPixelLocalStorageBarrierANGLE();
    EXPECT_GL_SINGLE_ERROR(GL_INVALID_OPERATION);
    EXPECT_GL_SINGLE_ERROR_MSG("Pixel local storage is not active.");

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
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_robustness"));
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_EXT_create_context_robustness"));

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
                       WithRobustness(ES31_NULL()).enable(Feature::EmulatePixelLocalStorage));

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
                       ES31_NULL().enable(Feature::EmulatePixelLocalStorage));

class PixelLocalStorageTestPreES31 : public ANGLETest<>
{};

// Check that GL_ANGLE_shader_pixel_local_storage is not advertised before ES 3.1.
//
// TODO(anglebug.com/7279): we can relax the min supported version once the implementation details
// are inside ANGLE.
TEST_P(PixelLocalStorageTestPreES31, UnsupportedClientVersion)
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

ANGLE_INSTANTIATE_TEST(PixelLocalStorageTestPreES31,
                       ES1_NULL().enable(Feature::EmulatePixelLocalStorage),
                       ES2_NULL().enable(Feature::EmulatePixelLocalStorage),
                       ES3_NULL().enable(Feature::EmulatePixelLocalStorage));
