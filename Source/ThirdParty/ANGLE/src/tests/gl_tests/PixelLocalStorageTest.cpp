//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2022 Rive
//

#include <regex>
#include <sstream>
#include <string>
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

////////////////////////////////////////////////////////////////////////////////////////////////////
// GL_ANGLE_shader_pixel_local_storage prototype.
//
// NOTE: the hope is for this to eventually move into ANGLE.

#define GL_DISABLED_ANGLE 0xbaadbeef

constexpr static int MAX_LOCAL_STORAGE_PLANES                = 3;
constexpr static int MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE = 1;

// ES 3.1 unfortunately requires most image formats to be either readonly or writeonly. To work
// around this limitation, we bind the same image unit to both a readonly and a writeonly image2D.
// We mark the images as volatile since they are aliases of the same memory.
//
// The ANGLE GLSL compiler doesn't appear to support macro concatenation (e.g., NAME ## _R). For
// now, the client code is responsible to know there are two image2D variables, append "_R" for
// pixelLocalLoadImpl, and append "_W" for pixelLocalStoreImpl.
//
// NOTE: PixelLocalStorageTest::useProgram appends "_R"/"_W" for you automatically if you use
// PIXEL_LOCAL_DECL / pixelLocalLoad / pixelLocalStore.
constexpr static const char kLocalStorageGLSLDefines[] = R"(
#define PIXEL_LOCAL_DECL_IMPL(NAME_R, NAME_W, BINDING, FORMAT)                       \
    layout(BINDING, FORMAT) coherent volatile readonly highp uniform image2D NAME_R; \
    layout(BINDING, FORMAT) coherent volatile writeonly highp uniform image2D NAME_W
#define PIXEL_LOCAL_DECL_I_IMPL(NAME_R, NAME_W, BINDING, FORMAT)                      \
    layout(BINDING, FORMAT) coherent volatile readonly highp uniform iimage2D NAME_R; \
    layout(BINDING, FORMAT) coherent volatile writeonly highp uniform iimage2D NAME_W
#define PIXEL_LOCAL_DECL_UI_IMPL(NAME_R, NAME_W, BINDING, FORMAT)                     \
    layout(BINDING, FORMAT) coherent volatile readonly highp uniform uimage2D NAME_R; \
    layout(BINDING, FORMAT) coherent volatile writeonly highp uniform uimage2D NAME_W
#define PIXEL_I_COORD \
    ivec2(floor(gl_FragCoord.xy))
#define pixelLocalLoadImpl(NAME_R) \
    imageLoad(NAME_R, PIXEL_I_COORD)
vec4 barrierAfter(vec4 expressionResult)
{
    memoryBarrier();
    return expressionResult;
}
ivec4 barrierAfter(ivec4 expressionResult)
{
    memoryBarrier();
    return expressionResult;
}
uvec4 barrierAfter(uvec4 expressionResult)
{
    memoryBarrier();
    return expressionResult;
}
#define pixelLocalStoreImpl(NAME_W, VALUE_EXPRESSION)                      \
    {                                                                      \
        imageStore(NAME_W, PIXEL_I_COORD, barrierAfter(VALUE_EXPRESSION)); \
        memoryBarrier();                                                   \
    }
// Don't execute pixelLocalStore when depth/stencil fails.
layout(early_fragment_tests) in;
)";

class PixelLocalStoragePrototype
{
  public:
    void framebufferPixelLocalStorage(GLuint unit,
                                      GLuint backingtexture,
                                      GLint level,
                                      GLint layer,
                                      GLint width,
                                      GLint height,
                                      GLenum internalformat);
    void framebufferPixelLocalClearValuefv(GLuint unit, const float *value);
    void framebufferPixelLocalClearValueiv(GLuint unit, const GLint *value);
    void framebufferPixelLocalClearValueuiv(GLuint unit, const GLuint *value);
    void beginPixelLocalStorage(GLsizei n, const GLenum *loadOps);
    void pixelLocalStorageBarrier();
    void endPixelLocalStorage();

  private:
    class PLSPlane
    {
      public:
        PLSPlane() = default;

        void reset(GLuint tex, GLsizei width, GLsizei height, GLuint internalformat)
        {
            if (mMemoryless && mTex)
            {
                glDeleteTextures(1, &mTex);
            }
            mMemoryless = !tex;
            if (mMemoryless)
            {
                GLint textureBinding2D;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding2D);
                glGenTextures(1, &mTex);
                glBindTexture(GL_TEXTURE_2D, mTex);
                glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width, height);
                glBindTexture(GL_TEXTURE_2D, textureBinding2D);
            }
            else
            {
                mTex = tex;
            }
            mWidth          = width;
            mHeight         = height;
            mInternalformat = internalformat;
        }

        ~PLSPlane()
        {
            if (mMemoryless && mTex)
            {
                glDeleteTextures(1, &mTex);
            }
        }

        GLuint tex() const { return mTex; }
        GLsizei width() const { return mWidth; }
        GLsizei height() const { return mHeight; }
        GLenum internalformat() const { return mInternalformat; }

        const float *clearValuef() const { return mClearValuef; }
        const int32_t *clearValuei() const { return mClearValuei; }
        const uint32_t *clearValueui() const { return mClearValueui; }

        void setClearValuef(const float val[4]) { memcpy(mClearValuef, val, sizeof(mClearValuef)); }
        void setClearValuei(const int32_t val[4])
        {
            memcpy(mClearValuei, val, sizeof(mClearValuei));
        }
        void setClearValueui(const uint32_t val[4])
        {
            memcpy(mClearValueui, val, sizeof(mClearValueui));
        }

      private:
        PLSPlane &operator=(const PLSPlane &) = delete;
        PLSPlane(const PLSPlane &)            = delete;

        bool mMemoryless;
        GLuint mTex = 0;
        GLsizei mWidth;
        GLsizei mHeight;
        GLenum mInternalformat;

        float mClearValuef[4]{};
        int32_t mClearValuei[4]{};
        uint32_t mClearValueui[4]{};
    };

    std::array<PLSPlane, MAX_LOCAL_STORAGE_PLANES> &boundLocalStoragePlanes()
    {
        GLint drawFBO;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
        ASSERT(drawFBO != 0);  // GL_INVALID_OPERATION!
        return mPLSPlanes[drawFBO];
    }

    std::map<GLuint, std::array<PLSPlane, MAX_LOCAL_STORAGE_PLANES>> mPLSPlanes;
    bool mLocalStorageEnabled = false;
    std::vector<int> mEnabledLocalStoragePlanes;
    GLint mFramebufferPreviousDefaultWidth  = 0;
    GLint mFramebufferPreviousDefaultHeight = 0;
};

// Bootstrap the draft extension assuming an in-scope PixelLocalStoragePrototype object named "pls".
#define glFramebufferPixelLocalStorageANGLE pls.framebufferPixelLocalStorage
#define glFramebufferPixelLocalClearValuefvANGLE pls.framebufferPixelLocalClearValuefv
#define glFramebufferPixelLocalClearValueivANGLE pls.framebufferPixelLocalClearValueiv
#define glFramebufferPixelLocalClearValueuivANGLE pls.framebufferPixelLocalClearValueuiv
#define glBeginPixelLocalStorageANGLE pls.beginPixelLocalStorage
#define glPixelLocalStorageBarrierANGLE pls.pixelLocalStorageBarrier
#define glEndPixelLocalStorageANGLE pls.endPixelLocalStorage

void PixelLocalStoragePrototype::framebufferPixelLocalStorage(GLuint unit,
                                                              GLuint backingtexture,
                                                              GLint level,
                                                              GLint layer,
                                                              GLsizei width,
                                                              GLsizei height,
                                                              GLenum internalformat)
{
    ASSERT(0 <= unit && unit < MAX_LOCAL_STORAGE_PLANES);  // GL_INVALID_VALUE!
    ASSERT(level == 0);                                    // NOT IMPLEMENTED!
    ASSERT(layer == 0);                                    // NOT IMPLEMENTED!
    ASSERT(width > 0 && height > 0);                       // NOT IMPLEMENTED!
    boundLocalStoragePlanes()[unit].reset(backingtexture, width, height, internalformat);
}

void PixelLocalStoragePrototype::framebufferPixelLocalClearValuefv(GLuint unit,
                                                                   const GLfloat *value)
{
    ASSERT(0 <= unit && unit < MAX_LOCAL_STORAGE_PLANES);  // GL_INVALID_VALUE!
    boundLocalStoragePlanes()[unit].setClearValuef(value);
}

void PixelLocalStoragePrototype::framebufferPixelLocalClearValueiv(GLuint unit, const GLint *value)
{
    ASSERT(0 <= unit && unit < MAX_LOCAL_STORAGE_PLANES);  // GL_INVALID_VALUE!
    boundLocalStoragePlanes()[unit].setClearValuei(value);
}

void PixelLocalStoragePrototype::framebufferPixelLocalClearValueuiv(GLuint unit,
                                                                    const GLuint *value)
{
    ASSERT(0 <= unit && unit < MAX_LOCAL_STORAGE_PLANES);  // GL_INVALID_VALUE!
    boundLocalStoragePlanes()[unit].setClearValueui(value);
}

class AutoRestoreDrawBuffers
{
  public:
    AutoRestoreDrawBuffers()
    {
        GLint MAX_COLOR_ATTACHMENTS;
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);

        GLint MAX_DRAW_BUFFERS;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &MAX_DRAW_BUFFERS);

        for (int i = 0; i < MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE; ++i)
        {
            GLint drawBuffer;
            glGetIntegerv(GL_DRAW_BUFFER0 + i, &drawBuffer);
            // glDrawBuffers must not reference an attachment at or beyond
            // MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE.
            if (GL_COLOR_ATTACHMENT0 <= drawBuffer &&
                drawBuffer < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS)
            {
                // GL_INVALID_OPERATION!
                ASSERT(drawBuffer < GL_COLOR_ATTACHMENT0 + MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE);
            }
            mDrawBuffers[i] = drawBuffer;
        }
        // glDrawBuffers must be GL_NONE at or beyond MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE.
        for (int i = MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE; i < MAX_DRAW_BUFFERS; ++i)
        {
            GLint drawBuffer;
            glGetIntegerv(GL_DRAW_BUFFER0 + i, &drawBuffer);
            ASSERT(drawBuffer == GL_NONE);  // GL_INVALID_OPERATION!
        }
    }

    ~AutoRestoreDrawBuffers()
    {
        glDrawBuffers(MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE, mDrawBuffers);
    }

  private:
    GLenum mDrawBuffers[MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE];
};

class AutoRestoreClearColor
{
  public:
    AutoRestoreClearColor() { glGetFloatv(GL_COLOR_CLEAR_VALUE, mClearColor); }

    ~AutoRestoreClearColor()
    {
        glClearColor(mClearColor[0], mClearColor[1], mClearColor[2], mClearColor[3]);
    }

  private:
    float mClearColor[4];
};

class AutoDisableScissor
{
  public:
    AutoDisableScissor()
    {
        glGetIntegerv(GL_SCISSOR_TEST, &mScissorTestEnabled);
        if (mScissorTestEnabled)
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    ~AutoDisableScissor()
    {
        if (mScissorTestEnabled)
        {
            glEnable(GL_SCISSOR_TEST);
        }
    }

  private:
    GLint mScissorTestEnabled;
};

void PixelLocalStoragePrototype::beginPixelLocalStorage(GLsizei n, const GLenum *loadOps)
{
    ASSERT(1 <= n && n <= MAX_LOCAL_STORAGE_PLANES);  // GL_INVALID_VALUE!
    ASSERT(!mLocalStorageEnabled);                    // GL_INVALID_OPERATION!
    ASSERT(mEnabledLocalStoragePlanes.empty());

    mLocalStorageEnabled = true;

    const auto &planes = boundLocalStoragePlanes();

    // A framebuffer must have no attachments at or beyond MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE.
    GLint MAX_COLOR_ATTACHMENTS;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);
    for (int i = MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE; i < MAX_COLOR_ATTACHMENTS; ++i)
    {
        GLint type;
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        ASSERT(type == GL_NONE);  // GL_INVALID_OPERATION!
    }

    int framebufferWidth  = 0;
    int framebufferHeight = 0;
    bool needsClear       = false;
    GLenum attachmentsToClear[MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + MAX_LOCAL_STORAGE_PLANES];
    memset(attachmentsToClear, 0, sizeof(attachmentsToClear));

    for (int i = 0; i < n; ++i)
    {
        GLuint tex            = 0;
        GLenum internalformat = GL_RGBA8;
        if (loadOps[i] != GL_DISABLED_ANGLE)
        {
            ASSERT(planes[i].tex());  // GL_INVALID_FRAMEBUFFER_OPERATION!
            tex            = planes[i].tex();
            internalformat = planes[i].internalformat();

            // GL_INVALID_FRAMEBUFFER_OPERATION!
            ASSERT(!framebufferWidth || framebufferWidth == planes[i].width());
            ASSERT(!framebufferHeight || framebufferHeight == planes[i].height());
            framebufferWidth  = planes[i].width();
            framebufferHeight = planes[i].height();

            mEnabledLocalStoragePlanes.push_back(i);
        }
        if (loadOps[i] == GL_ZERO || loadOps[i] == GL_REPLACE)
        {
            // Attach all textures that need clearing to the framebuffer.
            GLenum attachmentPoint =
                GL_COLOR_ATTACHMENT0 + MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + i;
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint, GL_TEXTURE_2D, tex, 0);
            // If the GL is bound to a draw framebuffer object, the ith buffer listed in bufs must
            // be GL_COLOR_ATTACHMENTi or GL_NONE.
            needsClear                                                      = true;
            attachmentsToClear[MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + i] = attachmentPoint;
        }
        // Bind local storage textures to their corresponding image unit. Use GL_READ_WRITE since
        // this binding will be referenced by two image2Ds -- one readeonly and one writeonly.
        glBindImageTexture(i, tex, 0, GL_FALSE, 0, GL_READ_WRITE, internalformat);
    }
    if (needsClear)
    {
        AutoRestoreDrawBuffers autoRestoreDrawBuffers;
        AutoRestoreClearColor autoRestoreClearColor;
        AutoDisableScissor autoDisableScissor;
        glDrawBuffers(MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + n, attachmentsToClear);
        for (int i = 0; i < n; ++i)
        {
            if (loadOps[i] != GL_ZERO && loadOps[i] != GL_REPLACE)
            {
                continue;
            }
            constexpr static char zero[4][4]{};
            switch (planes[i].internalformat())
            {
                case GL_RGBA8:
                case GL_R32F:
                case GL_RGBA16F:
                case GL_RGBA32F:
                    glClearBufferfv(GL_COLOR, MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + i,
                                    loadOps[i] == GL_REPLACE
                                        ? planes[i].clearValuef()
                                        : reinterpret_cast<const float *>(zero));
                    break;
                case GL_RGBA8I:
                case GL_RGBA16I:
                    glClearBufferiv(GL_COLOR, MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + i,
                                    loadOps[i] == GL_REPLACE
                                        ? planes[i].clearValuei()
                                        : reinterpret_cast<const int32_t *>(zero));
                    break;
                case GL_RGBA8UI:
                case GL_R32UI:
                case GL_RGBA16UI:
                case GL_RGBA32UI:
                    glClearBufferuiv(GL_COLOR, MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + i,
                                     loadOps[i] == GL_REPLACE
                                         ? planes[i].clearValueui()
                                         : reinterpret_cast<const uint32_t *>(zero));
                    break;
                default:
                    // Internal error. Invalid internalformats should not have made it this far.
                    ASSERT(false);
            }
        }
        // Detach the textures that needed clearing.
        for (int i = 0; i < n; ++i)
        {
            if (loadOps[i] == GL_ZERO || loadOps[i] == GL_REPLACE)
            {
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0 + MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE + i,
                    GL_TEXTURE_2D, 0, 0);
            }
        }
    }

    glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                &mFramebufferPreviousDefaultWidth);
    glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                &mFramebufferPreviousDefaultHeight);
    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, framebufferWidth);
    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, framebufferHeight);

    // Do *ALL* barriers since we don't know what the client did with memory before this point.
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void PixelLocalStoragePrototype::pixelLocalStorageBarrier()
{
    // In an ideal world we would only need GL_SHADER_IMAGE_ACCESS_BARRIER_BIT, but some drivers
    // need a bit more persuasion to get this right.
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void PixelLocalStoragePrototype::endPixelLocalStorage()
{
    ASSERT(mLocalStorageEnabled);  // GL_INVALID_OPERATION!

    // Do *ALL* barriers since we don't know what the client will do with memory after this point.
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // Restore framebuffer default dimensions.
    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                            mFramebufferPreviousDefaultWidth);
    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                            mFramebufferPreviousDefaultHeight);

    // Unbind local storage image textures.
    for (int i : mEnabledLocalStoragePlanes)
    {
        glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    }
    mEnabledLocalStoragePlanes.clear();

    mLocalStorageEnabled = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr static int W = 128, H = 128;
constexpr static std::array<float, 4> FULLSCREEN = {0, 0, W, H};

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

    bool supportsPixelLocalStorage()
    {
        ASSERT(getClientMajorVersion() == 3);
        ASSERT(getClientMinorVersion() == 1);

        if (isD3D11Renderer())
        {
            // We can't implement pixel local storage via shader images on top of D3D11:
            //
            //   * D3D UAVs don't support aliasing: https://anglebug.com/3032
            //   * But ES 3.1 doesn't allow most image2D formats to be readwrite
            //   * And we can't use texelFetch because ps_5_0 does not support thread
            //     synchronization operations in shaders (aka memoryBarrier()).
            //
            // We will need to do a custom local storage implementation in D3D11 that uses
            // RWTexture2D<> or, more ideally, the coherent RasterizerOrderedTexture2D<>.
            return false;
        }

        return true;
    }

    bool supportsPixelLocalStorageCoherent()
    {
        return false;  // ES 3.1 shader images can't be coherent.
    }

    // anglebug.com/7398: imageLoad() eventually starts failing. A workaround is to delete and
    // recreate the texture every once in a while. Hopefully this goes away once we start using
    // proper readwrite desktop GL shader images and INTEL_fragment_shader_ordering.
    bool hasImageLoadBug() { return IsWindows() && IsIntel() && IsOpenGL(); }

    void useProgram(std::string fsMain)
    {
        // Replace: PIXEL_LOCAL_DECL(name, ...) -> PIXEL_LOCAL_DECL_IMPL(name_R, name_W, ...)
        static std::regex kDeclRegex("(PIXEL_LOCAL_DECL[_UI]*)\\s*\\(\\s*([a-zA-Z_][a-zA-Z0-9_]*)");
        fsMain = std::regex_replace(fsMain, kDeclRegex, "$1_IMPL($2_R, $2_W");

        // Replace: pixelLocalLoad(name) -> pixelLocalLoadImpl(name_R)
        static std::regex kLoadRegex("pixelLocalLoad\\s*\\(\\s*([a-zA-Z_][a-zA-Z0-9_]*)");
        fsMain = std::regex_replace(fsMain, kLoadRegex, "pixelLocalLoadImpl($1_R");

        // Replace: pixelLocalStore(name, ...) -> pixelLocalStoreImpl(name_W, ...)
        static std::regex kStoreRegex("pixelLocalStore\\s*\\(\\s*([a-zA-Z_][a-zA-Z0-9_]*)");
        fsMain = std::regex_replace(fsMain, kStoreRegex, "pixelLocalStoreImpl($1_W");

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
            precision highp float;
            in vec4 color;
            in vec4 aux1;
            in vec4 aux2;)")
                .append(kLocalStorageGLSLDefines)
                .append(fsMain)
                .c_str());

        ASSERT_TRUE(mProgram.valid());

        glUseProgram(mProgram);

        glUniform1f(glGetUniformLocation(mProgram, "W"), W);
        glUniform1f(glGetUniformLocation(mProgram, "H"), H);

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

    void drawBoxes(PixelLocalStoragePrototype &pls,
                   std::vector<Box> boxes,
                   UseBarriers useBarriers = UseBarriers::IfNotCoherent)
    {
        if (!supportsPixelLocalStorageCoherent() && useBarriers == UseBarriers::IfNotCoherent)
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

    void attachTextureToScratchFBO(GLuint tex)
    {
        if (!mScratchFBO)
        {
            glGenFramebuffers(1, &mScratchFBO);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, mScratchFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
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
            constexpr static const char *kVS =
                R"(#version 310 es
                precision highp float;
                out vec2 texcoord;
                void main()
                {
                    texcoord.x = (gl_VertexID & 1) == 0 ? 0.0 : 1.0;
                    texcoord.y = (gl_VertexID & 2) == 0 ? 0.0 : 1.0;
                    gl_Position = vec4(texcoord * 2.0 - 1.0, 0, 1);
                })";

            constexpr static const char *kFS =
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

    GLProgram mProgram;
    GLint mLTRBLocation = -1;
    GLint mRGBALocation = -1;
    GLint mAux1Location = -1;
    GLint mAux2Location = -1;

    GLuint mScratchFBO = 0;
    GLProgram mRenderTextureProgram;
};

// Verify that values from separate draw calls persist in pixel local storage, for all supported
// formats. Also verify that clear-to-zero works on every supported format.
TEST_P(PixelLocalStorageTest, AllFormats)
{
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    {
        PixelLocalStoragePrototype pls;

        useProgram(R"(
        PIXEL_LOCAL_DECL(plane1, binding=0, rgba8);
        PIXEL_LOCAL_DECL_I(plane2, binding=1, rgba8i);
        PIXEL_LOCAL_DECL_UI(plane3, binding=2, rgba8ui);
        void main()
        {
            pixelLocalStore(plane1, color + pixelLocalLoad(plane1));
            pixelLocalStore(plane2, ivec4(aux1) + pixelLocalLoad(plane2));
            pixelLocalStore(plane3, uvec4(aux2) + pixelLocalLoad(plane3));
        })");

        PLSTestTexture tex1(GL_RGBA8);
        PLSTestTexture tex2(GL_RGBA8I);
        PLSTestTexture tex3(GL_RGBA8UI);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferPixelLocalStorageANGLE(0, tex1, 0, 0, W, H, GL_RGBA8);
        glFramebufferPixelLocalStorageANGLE(1, tex2, 0, 0, W, H, GL_RGBA8I);
        glFramebufferPixelLocalStorageANGLE(2, tex3, 0, 0, W, H, GL_RGBA8UI);
        glViewport(0, 0, W, H);
        glDrawBuffers(0, nullptr);

        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}));

        // Accumulate R, G, B, A in 4 separate passes.
        drawBoxes(pls, {{FULLSCREEN, {1, 0, 0, 0}, {-5, 0, 0, 0}, {1, 0, 0, 0}},
                        {FULLSCREEN, {0, 1, 0, 0}, {0, -100, 0, 0}, {0, 50, 0, 0}},
                        {FULLSCREEN, {0, 0, 1, 0}, {0, 0, -70, 0}, {0, 0, 100, 0}},
                        {FULLSCREEN, {0, 0, 0, 0}, {0, 0, 0, 22}, {0, 0, 0, 255}}});

        glEndPixelLocalStorageANGLE();

        attachTextureToScratchFBO(tex1);
        EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(255, 255, 255, 0));

        attachTextureToScratchFBO(tex2);
        EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(-5, -100, -70, 22));

        attachTextureToScratchFBO(tex3);
        EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(1, 50, 100, 255));

        ASSERT_GL_NO_ERROR();
    }

    {
        PixelLocalStoragePrototype pls;

        useProgram(R"(
        PIXEL_LOCAL_DECL(plane1, binding=0, r32f);
        PIXEL_LOCAL_DECL_UI(plane2, binding=1, r32ui);
        void main()
        {
            pixelLocalStore(plane1, color + pixelLocalLoad(plane1));
            pixelLocalStore(plane2, uvec4(aux1) + pixelLocalLoad(plane2));
        })");

        PLSTestTexture tex1(GL_R32F);
        PLSTestTexture tex2(GL_R32UI);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferPixelLocalStorageANGLE(0, tex1, 0, 0, W, H, GL_R32F);
        glFramebufferPixelLocalStorageANGLE(1, tex2, 0, 0, W, H, GL_R32UI);
        glViewport(0, 0, W, H);
        glDrawBuffers(0, nullptr);

        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));

        // Accumulate R in 4 separate passes.
        drawBoxes(pls, {{FULLSCREEN, {-1.5, 0, 0, 0}, {5, 0, 0, 0}},
                        {FULLSCREEN, {-10.25, 0, 0, 0}, {60, 0, 0, 0}},
                        {FULLSCREEN, {-100, 0, 0, 0}, {700, 0, 0, 0}},
                        {FULLSCREEN, {.25, 0, 0, 0}, {8000, 0, 0, 22}}});

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
        EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(8765, 0, 0, 1));

        ASSERT_GL_NO_ERROR();
    }

    {
        PixelLocalStoragePrototype pls;

        useProgram(R"(
        PIXEL_LOCAL_DECL(plane1, binding=0, rgba16f);
        PIXEL_LOCAL_DECL_I(plane2, binding=1, rgba16i);
        PIXEL_LOCAL_DECL_UI(plane3, binding=2, rgba16ui);
        void main()
        {
            pixelLocalStore(plane1, color + pixelLocalLoad(plane1));
            pixelLocalStore(plane2, ivec4(aux1) + pixelLocalLoad(plane2));
            pixelLocalStore(plane3, uvec4(aux2) + pixelLocalLoad(plane3));
        })");

        PLSTestTexture tex1(GL_RGBA16F);
        PLSTestTexture tex2(GL_RGBA16I);
        PLSTestTexture tex3(GL_RGBA16UI);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferPixelLocalStorageANGLE(0, tex1, 0, 0, W, H, GL_RGBA16F);
        glFramebufferPixelLocalStorageANGLE(1, tex2, 0, 0, W, H, GL_RGBA16I);
        glFramebufferPixelLocalStorageANGLE(2, tex3, 0, 0, W, H, GL_RGBA16UI);
        glViewport(0, 0, W, H);
        glDrawBuffers(0, nullptr);

        glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}));

        // Accumulate R, G, B, A in 4 separate passes.
        drawBoxes(pls, {{FULLSCREEN, {-100.5, 0, 0, 0}, {-500, 0, 0, 0}, {1, 0, 0, 0}},
                        {FULLSCREEN, {0, 1024, 0, 0}, {0, -10000, 0, 0}, {0, 500, 0, 0}},
                        {FULLSCREEN, {0, 0, -4096, 0}, {0, 0, -7000, 0}, {0, 0, 10000, 0}},
                        {FULLSCREEN, {0, 0, 0, 16384}, {0, 0, 0, 2200}, {0, 0, 0, 65535}}});

        glEndPixelLocalStorageANGLE();

        attachTextureToScratchFBO(tex1);
        EXPECT_PIXEL_RECT32F_EQ(0, 0, W, H, GLColor32F(-100.5, 1024, -4096, 16384));

        attachTextureToScratchFBO(tex2);
        EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(-500, -10000, -7000, 2200));

        attachTextureToScratchFBO(tex3);
        EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(1, 500, 10000, 65535));

        ASSERT_GL_NO_ERROR();
    }

    {
        PixelLocalStoragePrototype pls;

        useProgram(R"(
        PIXEL_LOCAL_DECL(plane1, binding=0, rgba32f);
        PIXEL_LOCAL_DECL_UI(plane2, binding=1, rgba32ui);
        void main()
        {
            pixelLocalStore(plane1, color + pixelLocalLoad(plane1));
            pixelLocalStore(plane2, uvec4(aux1) + pixelLocalLoad(plane2));
        })");

        PLSTestTexture tex1(GL_RGBA32F);
        PLSTestTexture tex2(GL_RGBA32UI);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferPixelLocalStorageANGLE(0, tex1, 0, 0, W, H, GL_RGBA32F);
        glFramebufferPixelLocalStorageANGLE(1, tex2, 0, 0, W, H, GL_RGBA32UI);
        glViewport(0, 0, W, H);
        glDrawBuffers(0, nullptr);

        glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));

        // Accumulate R, G, B, A in 4 separate passes.
        drawBoxes(pls, {{FULLSCREEN, {-100.5, 0, 0, 0}, {1, 0, 0, 0}},
                        {FULLSCREEN, {0, 1024, 0, 0}, {0, 500, 0, 0}},
                        {FULLSCREEN, {0, 0, -4096, 0}, {0, 0, 10000, 0}},
                        {FULLSCREEN, {0, 0, 0, 16384}, {0, 0, 0, 65535}}});

        glEndPixelLocalStorageANGLE();

        attachTextureToScratchFBO(tex1);
        EXPECT_PIXEL_RECT32F_EQ(0, 0, W, H, GLColor32F(-100.5, 1024, -4096, 16384));

        attachTextureToScratchFBO(tex2);
        EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(1, 500, 10000, 65535));

        ASSERT_GL_NO_ERROR();
    }
}

// Check proper functioning of glFramebufferPixelLocalClearValue{fi ui}vANGLE.
TEST_P(PixelLocalStorageTest, ClearValue)
{
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;

    // Scissor and clear color should not affect clear loads.
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 1, 1);
    glClearColor(.1f, .2f, .3f, .4f);

    PLSTestTexture texf(GL_RGBA8);
    PLSTestTexture texi(GL_RGBA16I);
    PLSTestTexture texui(GL_RGBA16UI);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalStorageANGLE(0, texf, 0, 0, W, H, GL_RGBA8);
    glFramebufferPixelLocalStorageANGLE(1, texi, 0, 0, W, H, GL_RGBA16I);
    glFramebufferPixelLocalStorageANGLE(2, texui, 0, 0, W, H, GL_RGBA16UI);
    auto clearLoads = GLenumArray({GL_REPLACE, GL_REPLACE, GL_REPLACE});

    // Clear values are initially zero.
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(texf);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(texi);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(texui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0, 0, 0, 0));

    // Test custom clear values.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalClearValuefvANGLE(0, MakeArray<float>({1, 0, 0, 0}));
    glFramebufferPixelLocalClearValueivANGLE(1, MakeArray<int32_t>({1, 2, 3, 4}));
    glFramebufferPixelLocalClearValueuivANGLE(2, MakeArray<uint32_t>({5, 6, 7, 8}));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(texf);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(255, 0, 0, 0));
    attachTextureToScratchFBO(texi);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(1, 2, 3, 4));
    attachTextureToScratchFBO(texui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(5, 6, 7, 8));

    // Different clear value types are separate state values.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalStorageANGLE(1, texf, 0, 0, W, H, GL_RGBA8);
    glFramebufferPixelLocalStorageANGLE(2, texi, 0, 0, W, H, GL_RGBA16I);
    glFramebufferPixelLocalStorageANGLE(0, texui, 0, 0, W, H, GL_RGBA16UI);
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(texf);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(texi);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(texui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0, 0, 0, 0));

    // Set new custom clear values.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalClearValuefvANGLE(1, MakeArray<float>({0, 1, 0, 0}));
    glFramebufferPixelLocalClearValueivANGLE(2, MakeArray<int32_t>({100, 200, 300, 400}));
    glFramebufferPixelLocalClearValueuivANGLE(0, MakeArray<uint32_t>({500, 600, 700, 800}));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(texf);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 255, 0, 0));
    attachTextureToScratchFBO(texi);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(100, 200, 300, 400));
    attachTextureToScratchFBO(texui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(500, 600, 700, 800));

    // Different clear value types are separate state values (final rotation).
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalStorageANGLE(2, texf, 0, 0, W, H, GL_RGBA8);
    glFramebufferPixelLocalStorageANGLE(0, texi, 0, 0, W, H, GL_RGBA16I);
    glFramebufferPixelLocalStorageANGLE(1, texui, 0, 0, W, H, GL_RGBA16UI);
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(texf);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(texi);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(texui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0, 0, 0, 0));

    // Set new custom clear values (final rotation).
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalClearValuefvANGLE(2, MakeArray<float>({0, 0, 1, 0}));
    glFramebufferPixelLocalClearValueivANGLE(0, MakeArray<int32_t>({1000, 2000, 3000, 4000}));
    glFramebufferPixelLocalClearValueuivANGLE(1, MakeArray<uint32_t>({5000, 6000, 7000, 8000}));
    glBeginPixelLocalStorageANGLE(3, clearLoads);
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(texf);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 255, 0));
    attachTextureToScratchFBO(texi);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(1000, 2000, 3000, 4000));
    attachTextureToScratchFBO(texui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(5000, 6000, 7000, 8000));

    // GL_ZERO shouldn't be affected by the clear color state.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}));
    glEndPixelLocalStorageANGLE();
    attachTextureToScratchFBO(texf);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(0, 0, 0, 0));
    attachTextureToScratchFBO(texi);
    EXPECT_PIXEL_RECT32I_EQ(0, 0, W, H, GLColor32I(0, 0, 0, 0));
    attachTextureToScratchFBO(texui);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0, 0, 0, 0));
}

// Check proper support of GL_ZERO, GL_KEEP, GL_REPLACE, and GL_DISABLED_ANGLE loadOps. Also verify
// that it works do draw with GL_MAX_LOCAL_STORAGE_PLANES_ANGLE planes.
TEST_P(PixelLocalStorageTest, LoadOps)
{
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;

    std::stringstream fs;
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "PIXEL_LOCAL_DECL(pls" << i << ", binding=" << i << ", rgba8);\n";
    }
    fs << "void main() {\n";
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "pixelLocalStore(pls" << i << ", color + pixelLocalLoad(pls" << i << "));\n";
    }
    fs << "}";
    useProgram(fs.str().c_str());

    // Create pls textures and clear them to red.
    glClearColor(1, 0, 0, 1);
    std::vector<PLSTestTexture> texs;
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        texs.emplace_back(GL_RGBA8);
        attachTextureToScratchFBO(texs[i]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Turn on scissor to try and confuse the local storage clear step.
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 20, H);

    // Set up pls color planes with a clear color of black. Odd units load with GL_REPLACE (cleared
    // to black) and even load with GL_KEEP (preserved red).
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    std::vector<GLenum> loadOps(MAX_LOCAL_STORAGE_PLANES);
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        glFramebufferPixelLocalClearValuefvANGLE(i, MakeArray<float>({0, 0, 0, 1}));
        glFramebufferPixelLocalStorageANGLE(i, texs[i], 0, 0, W, H, GL_RGBA8);
        loadOps[i] = (i & 1) ? GL_REPLACE : GL_KEEP;
    }
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // Draw transparent green into all pls attachments.
    glBeginPixelLocalStorageANGLE(MAX_LOCAL_STORAGE_PLANES, loadOps.data());
    drawBoxes(pls, {{{FULLSCREEN}, {0, 1, 0, 0}}});
    glEndPixelLocalStorageANGLE();

    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texs[i], 0);
        // Check that the draw buffers didn't get perturbed by local storage -- GL_COLOR_ATTACHMENT0
        // is currently off, so glClear has no effect. This also verifies that local storage planes
        // didn't get left attached to the framebuffer somewhere with draw buffers on.
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_RECT_EQ(0, 0, 20, H,
                             loadOps[i] == GL_REPLACE ? GLColor(0, 255, 0, 255)
                                                      : /*GL_KEEP*/ GLColor(255, 255, 0, 255));
        // Check that the scissor didn't get perturbed by local storage.
        EXPECT_PIXEL_RECT_EQ(
            20, 0, W - 20, H,
            loadOps[i] == GL_REPLACE ? GLColor(0, 0, 0, 255) : /*GL_KEEP*/ GLColor(255, 0, 0, 255));
    }

    // Detach the last read pls texture from the framebuffer.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    // Now test GL_DISABLED_ANGLE and GL_ZERO.
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        loadOps[i] = loadOps[i] == GL_REPLACE ? GL_ZERO : GL_DISABLED_ANGLE;
    }

    // Execute a pls pass without a draw.
    glBeginPixelLocalStorageANGLE(MAX_LOCAL_STORAGE_PLANES, loadOps.data());
    glEndPixelLocalStorageANGLE();

    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
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
    glDrawBuffers(1, std::array<GLenum, 1>{GL_COLOR_ATTACHMENT0}.data());
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_RECT_EQ(0, 0, 20, H, GLColor(255, 0, 0, 255));
    EXPECT_PIXEL_RECT_EQ(20, 0, W - 20, H, GLColor(0, 0, 0, 0));

    ASSERT_GL_NO_ERROR();
}

// This next series of tests checks that GL utilities for rejecting fragments prevent stores to PLS:
//
//   * discard
//   * return (from main)
//   * stencil test
//   * depth test
//   * viewport
//
// Some utilities are not legal in ANGLE_shader_pixel_local_storage:
//
//   * gl_SampleMask is disallowed by the spec
//   * discard, after potential calls to pixelLocalLoad/Store, is disallowed by the spec
//   * pixelLocalLoad/Store after a return from main is disallowed by the spec
//
// To run the tests, bind a FragmentRejectTestFBO and draw {FRAG_REJECT_TEST_BOX}:
//
//   * {0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT} should be green
//   * Fragments outside should have been rejected, leaving the pixels black
//
struct FragmentRejectTestFBO : GLFramebuffer
{
    FragmentRejectTestFBO(PixelLocalStoragePrototype &pls, GLuint tex)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, *this);
        glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_RGBA8);
        glFramebufferPixelLocalClearValuefvANGLE(0, MakeArray<float>({0, 0, 0, 1}));
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

// Check that discard prevents stores to pls.
// (discard after pixelLocalLoad/Store is illegal because it would have different behavior with
// shader images vs framebuffer fetch.)
TEST_P(PixelLocalStorageTest, FragmentReject_discard)
{
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

    useProgram(R"(
    PIXEL_LOCAL_DECL(pls, binding=0, rgba8);
    void main()
    {
        vec4 dst = pixelLocalLoad(pls);
        if (any(lessThan(gl_FragCoord.xy, aux1.xy)) || any(greaterThan(gl_FragCoord.xy, aux1.zw)))
        {
            discard;
        }
        pixelLocalStore(pls, color + dst);
    })");
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    drawBoxes(pls, {FRAG_REJECT_TEST_BOX});
    glEndPixelLocalStorageANGLE();

    renderTextureToDefaultFramebuffer(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(FRAG_REJECT_TEST_WIDTH, 0, W - FRAG_REJECT_TEST_WIDTH,
                         FRAG_REJECT_TEST_HEIGHT, GLColor::black);
    EXPECT_PIXEL_RECT_EQ(0, FRAG_REJECT_TEST_HEIGHT, W, H - FRAG_REJECT_TEST_HEIGHT,
                         GLColor::black);
    ASSERT_GL_NO_ERROR();
}

// Check that return from main prevents stores to PLS.
// (pixelLocalLoad/Store after a return from main is illegal because
// GL_ARB_fragment_shader_interlock isn't allowed after a return from main.)
TEST_P(PixelLocalStorageTest, FragmentReject_return)
{
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

    useProgram(R"(
    PIXEL_LOCAL_DECL(pls, binding=0, rgba8);
    void main()
    {
        if (any(lessThan(gl_FragCoord.xy, aux1.xy)) || any(greaterThan(gl_FragCoord.xy, aux1.zw)))
        {
            return;
        }
        pixelLocalStore(pls, color + pixelLocalLoad(pls));
    })");
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    drawBoxes(pls, {FRAG_REJECT_TEST_BOX});
    glEndPixelLocalStorageANGLE();

    renderTextureToDefaultFramebuffer(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(FRAG_REJECT_TEST_WIDTH, 0, W - FRAG_REJECT_TEST_WIDTH,
                         FRAG_REJECT_TEST_HEIGHT, GLColor::black);
    EXPECT_PIXEL_RECT_EQ(0, FRAG_REJECT_TEST_HEIGHT, W, H - FRAG_REJECT_TEST_HEIGHT,
                         GLColor::black);
    ASSERT_GL_NO_ERROR();
}

// Check that the stencil test prevents stores to PLS.
TEST_P(PixelLocalStorageTest, FragmentReject_stencil)
{
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

    useProgram(R"(
    PIXEL_LOCAL_DECL(pls, binding=0, rgba8);
    void main()
    {
        pixelLocalStore(pls, color + pixelLocalLoad(pls));
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
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NEVER, 1, ~0u);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    drawBoxes(pls, {{{0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT}}});
    glEndPixelLocalStorageANGLE();
    glDisable(GL_STENCIL_TEST);
    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor::black);

    // Stencil should be preserved after PLS, and only pixels that pass the stencil test should
    // update PLS next.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (hasImageLoadBug())  // anglebug.com/7398
    {
        tex.reset(GL_RGBA8);
        glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_RGBA8);
    }
    glEnable(GL_STENCIL_TEST);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    glStencilFunc(GL_NOTEQUAL, 0, ~0u);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    drawBoxes(pls, {FRAG_REJECT_TEST_BOX});
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
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

    useProgram(R"(
    PIXEL_LOCAL_DECL(pls, binding=0, rgba8);
    void main()
    {
        pixelLocalStore(pls, pixelLocalLoad(pls) + color);
    })");
    GLuint depth;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    glClearDepthf(0.f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT);
    glClearDepthf(1.f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glDepthFunc(GL_LESS);
    drawBoxes(pls, {FRAG_REJECT_TEST_BOX});
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
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

    useProgram(R"(
    PIXEL_LOCAL_DECL(pls, binding=0, rgba8);
    void main()
    {
        vec4 dst = pixelLocalLoad(pls);
        pixelLocalStore(pls, color + dst);
    })");
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    glViewport(0, 0, FRAG_REJECT_TEST_WIDTH, FRAG_REJECT_TEST_HEIGHT);
    drawBoxes(pls, {FRAG_REJECT_TEST_BOX});
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
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;

    useProgram(R"(
    PIXEL_LOCAL_DECL_UI(framebuffer, binding=0, r32ui);
    void main()
    {
        uvec4 dst = pixelLocalLoad(framebuffer);
        pixelLocalStore(framebuffer, uvec4(color) + dst * 2u);
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

    PLSTestTexture tex(GL_R32UI);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_R32UI);
    glFramebufferPixelLocalClearValueuivANGLE(0, MakeArray<uint32_t>({1, 0, 0, 0}));
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // First make sure it works properly with a barrier.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    drawBoxes(pls, boxesA_100, UseBarriers::No);
    glPixelLocalStorageBarrierANGLE();
    drawBoxes(pls, boxesB_7, UseBarriers::No);
    glEndPixelLocalStorageANGLE();

    attachTextureToScratchFBO(tex);
    EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(211, 0, 0, 1));

    ASSERT_GL_NO_ERROR();

    // Vulkan generates rightful "SYNC-HAZARD-READ_AFTER_WRITE" validation errors when we omit the
    // barrier.
    ANGLE_SKIP_TEST_IF(IsVulkan());

    // Now forget to insert the barrier and ensure our nondeterminism still falls within predictable
    // constraints.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (hasImageLoadBug())  // anglebug.com/7398
    {
        tex.reset(GL_R32UI);
        glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_R32UI);
    }
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    drawBoxes(pls, boxesA_100, UseBarriers::No);
    // OOPS! We forgot to insert a barrier!
    drawBoxes(pls, boxesB_7, UseBarriers::No);
    glEndPixelLocalStorageANGLE();

    uint32_t pixels[H * W * 4];
    attachTextureToScratchFBO(tex);
    glReadPixels(0, 0, W, H, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixels);
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
            printf("              Got: %u\n", pixels[r]);
            printf("  Expected one of: { 211, 118, 102, 9 }\n");
        }
        ASSERT_TRUE(isAcceptableValue);
    }

    ASSERT_GL_NO_ERROR();
}

// Check loading and storing from memoryless local storage planes.
TEST_P(PixelLocalStorageTest, MemorylessStorage)
{
    ANGLE_SKIP_TEST_IF(!supportsPixelLocalStorage());

    PixelLocalStoragePrototype pls;

    // Bind the texture, but don't call glTexStorage until after creating the memoryless plane.
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    // Create a memoryless plane.
    glFramebufferPixelLocalStorageANGLE(1, 0 /*memoryless*/, 0, 0, W, H, GL_RGBA8);
    // Define the persistent texture now, after attaching the memoryless pixel local storage. This
    // verifies that the GL_TEXTURE_2D binding doesn't get perturbed by local storage.
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, W, H);
    glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_RGBA8);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));

    // Draw into memoryless storage.
    useProgram(R"(
    PIXEL_LOCAL_DECL(memoryless, binding=1, rgba8);
    void main()
    {
        pixelLocalStore(memoryless, color + pixelLocalLoad(memoryless));
    })");

    drawBoxes(pls, {{{0, 20, W, H}, {1, 0, 0, 0}},
                    {{0, 40, W, H}, {0, 1, 0, 0}},
                    {{0, 60, W, H}, {0, 0, 1, 0}}});

    // Transfer to a texture.
    useProgram(R"(
    PIXEL_LOCAL_DECL(framebuffer, binding=0, rgba8);
    PIXEL_LOCAL_DECL(memoryless, binding=1, rgba8);
    void main()
    {
        pixelLocalStore(framebuffer, vec4(1) - pixelLocalLoad(memoryless));
    })");

    drawBoxes(pls, {{FULLSCREEN}});

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

ANGLE_INSTANTIATE_TEST_ES31(PixelLocalStorageTest);
