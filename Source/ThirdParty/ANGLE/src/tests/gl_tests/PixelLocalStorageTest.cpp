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

////////////////////////////////////////////////////////////////////////////////////////////////////
// GL_ANGLE_shader_pixel_local_storage prototype.
//
// NOTE: the hope is for this to eventually move into ANGLE.

#define GL_DISABLE_ANGLE 0xbaadbeef

constexpr static int MAX_LOCAL_STORAGE_PLANES                = 3;
constexpr static int MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE = 1;

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
        if (loadOps[i] != GL_DISABLE_ANGLE)
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
        GLenum imageFormat = internalformat;
        // We pack all PLS formats into r32f, r32i, or r32ui image2Ds on ES and D3D.
        if (IsOpenGLES() || IsD3D())
        {
            switch (internalformat)
            {
                case GL_RGBA8:
                case GL_RGBA8UI:
                    imageFormat = GL_R32UI;
                    break;
                case GL_RGBA8I:
                    imageFormat = GL_R32I;
                    break;
            }
        }
        // Bind local storage textures to their corresponding image unit. Use GL_READ_WRITE since
        // this binding will be referenced by two image2Ds -- one readeonly and one writeonly.
        glBindImageTexture(i, tex, 0, GL_FALSE, 0, GL_READ_WRITE, imageFormat);
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

    GLProgram mProgram;
    GLint mLTRBLocation = -1;
    GLint mRGBALocation = -1;
    GLint mAux1Location = -1;
    GLint mAux2Location = -1;

    GLuint mScratchFBO = 0;
    GLProgram mRenderTextureProgram;
};

// Verify that rgba8, rgba8i, and rgba8ui pixel local storage behaves as specified.
TEST_P(PixelLocalStorageTest, RGBA8)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;

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
    glFramebufferPixelLocalStorageANGLE(0, tex1, 0, 0, W, H, GL_RGBA8);
    glFramebufferPixelLocalStorageANGLE(1, tex2, 0, 0, W, H, GL_RGBA8I);
    glFramebufferPixelLocalStorageANGLE(2, tex3, 0, 0, W, H, GL_RGBA8UI);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_ZERO, GL_ZERO}));

    // Accumulate R, G, B, A in 4 separate passes.
    // Store out-of-range values to ensure they are properly clamped upon storage.
    drawBoxes(pls, {{FULLSCREEN, {2, -1, -2, -3}, {-500, 0, 0, 0}, {1, 0, 0, 0}},
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
    PixelLocalStoragePrototype pls;

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

// Check proper functioning of glFramebufferPixelLocalClearValue{fi ui}vANGLE.
TEST_P(PixelLocalStorageTest, ClearValue)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

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

// Check proper support of GL_ZERO, GL_KEEP, GL_REPLACE, and GL_DISABLE_ANGLE loadOps. Also verify
// that it works do draw with GL_MAX_LOCAL_STORAGE_PLANES_ANGLE planes.
TEST_P(PixelLocalStorageTest, LoadOps)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;

    std::stringstream fs;
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "layout(binding=" << i << ", rgba8) highp uniform pixelLocalANGLE pls" << i << ";\n";
    }
    fs << "void main() {\n";
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "pixelLocalStoreANGLE(pls" << i << ", color + pixelLocalLoadANGLE(pls" << i
           << "));\n";
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

    // Set up pls color planes with a clear color of black. Odd units load with GL_REPLACE
    // (cleared to black) and even load with GL_KEEP (preserved red).
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

    // Now test GL_DISABLE_ANGLE and GL_ZERO.
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        loadOps[i] = loadOps[i] == GL_REPLACE ? GL_ZERO : GL_DISABLE_ANGLE;
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

// Check that the stencil test prevents stores to PLS.
TEST_P(PixelLocalStorageTest, FragmentReject_stencil)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

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
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

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
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;
    PLSTestTexture tex(GL_RGBA8);
    FragmentRejectTestFBO fbo(pls, tex);

    useProgram(R"(
    layout(binding=0, rgba8) highp uniform pixelLocalANGLE pls;
    void main()
    {
        vec4 dst = pixelLocalLoadANGLE(pls);
        pixelLocalStoreANGLE(pls, color + dst);
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
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;

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
    glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_R32F);
    glFramebufferPixelLocalClearValuefvANGLE(0, MakeArray<float>({1, 0, 0, 0}));
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // First make sure it works properly with a barrier.
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    drawBoxes(pls, boxesA_100, UseBarriers::No);
    glPixelLocalStorageBarrierANGLE();
    drawBoxes(pls, boxesB_7, UseBarriers::No);
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
    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_REPLACE}));
    drawBoxes(pls, boxesA_100, UseBarriers::No);
    // OOPS! We forgot to insert a barrier!
    drawBoxes(pls, boxesB_7, UseBarriers::No);
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
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE memoryless;
    void main()
    {
        pixelLocalStoreANGLE(memoryless, color + pixelLocalLoadANGLE(memoryless));
    })");

    drawBoxes(pls, {{{0, 20, W, H}, {1, 0, 0, 0}},
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

// Check that it works to render with the maximum supported data payload:
//
//   GL_MAX_LOCAL_STORAGE_PLANES_ANGLE
//   GL_MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE_ANGLE
//
TEST_P(PixelLocalStorageTest, MaxCapacity)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;

    std::stringstream fs;
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "layout(binding=" << i << ", rgba8ui) highp uniform upixelLocalANGLE pls" << i
           << ";\n";
    }
    for (int i = 0; i < MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE; ++i)
    {
        fs << "out uvec4 out" << i << ";\n";
    }
    fs << "void main() {\n";
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        fs << "pixelLocalStoreANGLE(pls" << i << ", uvec4(color) - uvec4(" << i << "));\n";
    }
    for (int i = 0; i < MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE; ++i)
    {
        fs << "out" << i << " = uvec4(aux1) + uvec4(" << i << ");\n";
    }
    fs << "}";
    useProgram(fs.str().c_str());

    glViewport(0, 0, W, H);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    std::vector<PLSTestTexture> localTexs;
    localTexs.reserve(MAX_LOCAL_STORAGE_PLANES);
    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        localTexs.emplace_back(GL_RGBA8UI);
        glFramebufferPixelLocalStorageANGLE(i, localTexs[i], 0, 0, W, H, GL_RGBA8UI);
    }
    std::vector<PLSTestTexture> renderTexs;
    renderTexs.reserve(MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE);
    std::vector<GLenum> drawBuffers(MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE);
    for (int i = 0; i < MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE; ++i)
    {
        renderTexs.emplace_back(GL_RGBA32UI);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
                               renderTexs[i], 0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(drawBuffers.size(), drawBuffers.data());

    glBeginPixelLocalStorageANGLE(MAX_LOCAL_STORAGE_PLANES,
                                  std::vector<GLenum>(MAX_LOCAL_STORAGE_PLANES, GL_ZERO).data());
    drawBoxes(pls, {{FULLSCREEN, {255, 254, 253, 252}, {0, 1, 2, 3}}});
    glEndPixelLocalStorageANGLE();

    for (int i = 0; i < MAX_LOCAL_STORAGE_PLANES; ++i)
    {
        attachTextureToScratchFBO(localTexs[i]);
        EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(255u - i, 254u - i, 253u - i, 252u - i));
    }
    for (int i = 0; i < MAX_FRAGMENT_OUTPUTS_WITH_LOCAL_STORAGE; ++i)
    {
        attachTextureToScratchFBO(renderTexs[i]);
        EXPECT_PIXEL_RECT32UI_EQ(0, 0, W, H, GLColor32UI(0u + i, 1u + i, 2u + i, 3u + i));
    }

    ASSERT_GL_NO_ERROR();
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

    PixelLocalStoragePrototype pls;

    PLSTestTexture tex(GL_RGBA8);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferPixelLocalStorageANGLE(0, 0 /*Memoryless*/, 0, 0, W, H, GL_R32F);
    glFramebufferPixelLocalStorageANGLE(1, tex, 0, 0, W, H, GL_RGBA8);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    // Leave unit 0 with the default clear value of zero.
    glFramebufferPixelLocalClearValuefvANGLE(1, MakeArray<float>({0, 1, 0, 0}));
    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_REPLACE, GL_REPLACE}));

    // Pass 1: draw to memoryless conditionally.
    useProgram(R"(
    layout(binding=0, r32f) highp uniform pixelLocalANGLE memoryless;
    void main()
    {
        // Omit braces on the 'if' to ensure proper insertion of memoryBarriers in the translator.
        if (gl_FragCoord.x < 64.0)
            pixelLocalStoreANGLE(memoryless, vec4(1, -.1, .2, -.3));  // Only stores r.
    })");
    drawBoxes(pls, {{FULLSCREEN}});

    // Pass 2: draw to tex conditionally.
    useProgram(R"(
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE tex;
    void main()
    {
        // Omit braces on the 'if' to ensure proper insertion of memoryBarriers in the translator.
        if (gl_FragCoord.y < 64.0)
            pixelLocalStoreANGLE(tex, vec4(0, 1, 1, 0));
    })");
    drawBoxes(pls, {{FULLSCREEN}});

    // Pass 3: combine memoryless and tex.
    useProgram(R"(
    layout(binding=0, r32f) highp uniform pixelLocalANGLE memoryless;
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE tex;
    void main()
    {
        pixelLocalStoreANGLE(tex, pixelLocalLoadANGLE(tex) + pixelLocalLoadANGLE(memoryless));
    })");
    drawBoxes(pls, {{FULLSCREEN}});

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

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_DISABLE_ANGLE, GL_KEEP}));

    useProgram(R"(
    layout(binding=1, rgba8) highp uniform pixelLocalANGLE tex;
    out vec4 fragcolor;
    void main()
    {
        fragcolor = 1.0 - pixelLocalLoadANGLE(tex);
    })");
    drawBoxes(pls, {{FULLSCREEN}});

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

    PixelLocalStoragePrototype pls;

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
    glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_RGBA8UI);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glBeginPixelLocalStorageANGLE(1, GLenumArray({GL_ZERO}));
    drawBoxes(pls, {{FULLSCREEN}});
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
    glFramebufferPixelLocalStorageANGLE(0, 0 /*memoryless*/, 0, 0, W, H, GL_R32F);
    glFramebufferPixelLocalStorageANGLE(1, 0 /*memoryless*/, 0, 0, W, H, GL_R32UI);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glDrawBuffers(1, GLenumArray({GL_COLOR_ATTACHMENT0}));

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));
    drawBoxes(pls, {{FULLSCREEN}});
    glEndPixelLocalStorageANGLE();

    EXPECT_PIXEL_RECT_EQ(0, 0, W, H, GLColor(255, 0, 0, 255));
    ASSERT_GL_NO_ERROR();
}

// Check that PLS handles can be passed as function arguments.
TEST_P(PixelLocalStorageTest, FunctionArguments)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));

    PixelLocalStoragePrototype pls;

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
    glFramebufferPixelLocalStorageANGLE(0, dst, 0, 0, W, H, GL_RGBA8);
    glFramebufferPixelLocalStorageANGLE(1, 0 /*memoryless*/, 0, 0, W, H, GL_RGBA8);
    glFramebufferPixelLocalStorageANGLE(2, 0 /*memoryless*/, 0, 0, W, H, GL_R32F);
    glViewport(0, 0, W, H);
    glDrawBuffers(0, nullptr);

    glFramebufferPixelLocalClearValuefvANGLE(1, MakeArray<float>({0, 1, 1, 0}));
    glFramebufferPixelLocalClearValuefvANGLE(2, MakeArray<float>({1, 0, 0, 1}));
    glBeginPixelLocalStorageANGLE(3, GLenumArray({GL_ZERO, GL_REPLACE, GL_REPLACE}));
    drawBoxes(pls, {{FULLSCREEN}});
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

    PixelLocalStoragePrototype pls;

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

    PixelLocalStoragePrototype pls;

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
    glFramebufferPixelLocalStorageANGLE(0, tex, 0, 0, W, H, GL_RGBA8UI);
    glFramebufferPixelLocalStorageANGLE(1, 0 /* Memoryless */, 0, 0, W, H, GL_RGBA8);
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

    glBeginPixelLocalStorageANGLE(2, GLenumArray({GL_ZERO, GL_ZERO}));
    for (const std::vector<Box> &boxes : boxesList)
    {
        drawBoxes(pls, boxes);
    }
    glEndPixelLocalStorageANGLE();

    attachTextureToScratchFBO(tex);
    std::vector<uint8_t> actual(H * W * 4);
    glReadPixels(0, 0, W, H, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, actual.data());
    EXPECT_EQ(expected, actual);

    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(
    PixelLocalStorageTest,
    ES31_D3D11().enable(Feature::EmulatePixelLocalStorage),
    ES31_D3D11()
        .enable(Feature::DisableRasterizerOrderViews)
        .enable(Feature::EmulatePixelLocalStorage),
    ES31_OPENGL().enable(Feature::EmulatePixelLocalStorage),
    ES31_OPENGL()
        .disable(Feature::SupportsFragmentShaderInterlockNV)
        .disable(Feature::SupportsFragmentShaderOrderingINTEL)
        .disable(Feature::SupportsFragmentShaderInterlockARB)
        .enable(Feature::EmulatePixelLocalStorage),
    ES31_OPENGLES().enable(Feature::EmulatePixelLocalStorage),
    ES31_VULKAN().enable(Feature::EmulatePixelLocalStorage),
    ES31_VULKAN()
        .disable(Feature::SupportsFragmentShaderPixelInterlock)
        .enable(Feature::EmulatePixelLocalStorage),
    ES31_VULKAN_SWIFTSHADER().enable(Feature::EmulatePixelLocalStorage),
    ES31_VULKAN().enable(Feature::EmulatePixelLocalStorage).enable(Feature::AsyncCommandQueue),
    ES31_VULKAN_SWIFTSHADER()
        .enable(Feature::EmulatePixelLocalStorage)
        .enable(Feature::AsyncCommandQueue),
    // The coherent version of the extension relies on ARB_fragment_shader_interlock. Ensure it
    // works on Vulkan GLSL.
    ES31_VULKAN()
        .enable(Feature::EmulatePixelLocalStorage)
        .enable(Feature::GenerateSPIRVThroughGlslang));

class PixelLocalStorageCompilerTest : public ANGLETest<>
{
  protected:
    void testSetUp() override
    {
        ASSERT(IsGLExtensionEnabled("GL_ANGLE_shader_pixel_local_storage"));
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
