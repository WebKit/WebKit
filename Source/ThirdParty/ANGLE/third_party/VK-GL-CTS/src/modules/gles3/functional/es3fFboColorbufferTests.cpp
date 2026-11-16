/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief FBO colorbuffer tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFboColorbufferTests.hpp"
#include "es3fFboTestCase.hpp"
#include "es3fFboTestUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluContextInfo.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "sglrContextUtil.hpp"
#include "deRandom.hpp"
#include "deString.h"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::TestLog;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace FboTestUtil;

const tcu::RGBA MIN_THRESHOLD(12, 12, 12, 12);

template <int Size>
static tcu::Vector<float, Size> randomVector(de::Random &rnd,
                                             const tcu::Vector<float, Size> &minVal = tcu::Vector<float, Size>(0.0f),
                                             const tcu::Vector<float, Size> &maxVal = tcu::Vector<float, Size>(1.0f))
{
    tcu::Vector<float, Size> res;
    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = rnd.getFloat(minVal[ndx], maxVal[ndx]);
    return res;
}

static tcu::Vec4 generateRandomColor(de::Random &random)
{
    tcu::Vec4 retVal;

    for (int i = 0; i < 3; ++i)
        retVal[i] = random.getFloat();
    retVal[3] = 1.0f;

    return retVal;
}

class FboColorbufferCase : public FboTestCase
{
public:
    FboColorbufferCase(Context &context, const char *name, const char *desc, const uint32_t format)
        : FboTestCase(context, name, desc)
        , m_format(format)
    {
    }

    bool compare(const tcu::Surface &reference, const tcu::Surface &result)
    {
        const tcu::RGBA threshold(tcu::max(getFormatThreshold(m_format), MIN_THRESHOLD));

        m_testCtx.getLog() << TestLog::Message << "Comparing images, threshold: " << threshold << TestLog::EndMessage;

        return tcu::bilinearCompare(m_testCtx.getLog(), "Result", "Image comparison result", reference.getAccess(),
                                    result.getAccess(), threshold, tcu::COMPARE_LOG_RESULT);
    }

protected:
    const uint32_t m_format;
};

class FboColorClearCase : public FboColorbufferCase
{
public:
    FboColorClearCase(Context &context, const char *name, const char *desc, uint32_t format, int width, int height)
        : FboColorbufferCase(context, name, desc, format)
        , m_width(width)
        , m_height(height)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat fboFormat      = glu::mapGLInternalFormat(m_format);
        tcu::TextureChannelClass fmtClass = tcu::getTextureChannelClass(fboFormat.type);
        tcu::TextureFormatInfo fmtInfo    = tcu::getTextureFormatInfo(fboFormat);
        de::Random rnd(17);
        const int numClears = 16;
        uint32_t fbo        = 0;
        uint32_t rbo        = 0;

        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(1, &rbo);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, m_format, m_width, m_height);
        checkError();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glViewport(0, 0, m_width, m_height);

        // Initialize to transparent black.
        switch (fmtClass)
        {
        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            glClearBufferfv(GL_COLOR, 0, Vec4(0.0f).getPtr());
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            glClearBufferuiv(GL_COLOR, 0, UVec4(0).getPtr());
            break;

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            glClearBufferiv(GL_COLOR, 0, IVec4(0).getPtr());
            break;

        default:
            DE_ASSERT(false);
        }

        // Do random scissored clears.
        glEnable(GL_SCISSOR_TEST);
        for (int ndx = 0; ndx < numClears; ndx++)
        {
            int x      = rnd.getInt(0, m_width - 1);
            int y      = rnd.getInt(0, m_height - 1);
            int w      = rnd.getInt(1, m_width - x);
            int h      = rnd.getInt(1, m_height - y);
            Vec4 color = randomVector<4>(rnd, fmtInfo.valueMin, fmtInfo.valueMax);

            glScissor(x, y, w, h);

            switch (fmtClass)
            {
            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
                glClearBufferfv(GL_COLOR, 0, color.getPtr());
                break;

            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
                glClearBufferuiv(GL_COLOR, 0, color.cast<uint32_t>().getPtr());
                break;

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
                glClearBufferiv(GL_COLOR, 0, color.cast<int>().getPtr());
                break;

            default:
                DE_ASSERT(false);
            }
        }

        // Read results from renderbuffer.
        readPixels(dst, 0, 0, m_width, m_height, fboFormat, fmtInfo.lookupScale, fmtInfo.lookupBias);
        checkError();
    }

private:
    const int m_width;
    const int m_height;
};

class FboColorMultiTex2DCase : public FboColorbufferCase
{
public:
    FboColorMultiTex2DCase(Context &context, const char *name, const char *description, uint32_t tex0Fmt,
                           const IVec2 &tex0Size, uint32_t tex1Fmt, const IVec2 &tex1Size)
        : FboColorbufferCase(context, name, description, tex0Fmt)
        , m_tex0Fmt(tex0Fmt)
        , m_tex1Fmt(tex1Fmt)
        , m_tex0Size(tex0Size)
        , m_tex1Size(tex1Size)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_tex0Fmt);
        checkFormatSupport(m_tex1Fmt);
    }

    void render(tcu::Surface &dst)
    {
        tcu::TextureFormat texFmt0      = glu::mapGLInternalFormat(m_tex0Fmt);
        tcu::TextureFormat texFmt1      = glu::mapGLInternalFormat(m_tex1Fmt);
        tcu::TextureFormatInfo fmtInfo0 = tcu::getTextureFormatInfo(texFmt0);
        tcu::TextureFormatInfo fmtInfo1 = tcu::getTextureFormatInfo(texFmt1);

        Texture2DShader texToFbo0Shader(DataTypes() << glu::TYPE_SAMPLER_2D, getFragmentOutputType(texFmt0),
                                        fmtInfo0.valueMax - fmtInfo0.valueMin, fmtInfo0.valueMin);
        Texture2DShader texToFbo1Shader(DataTypes() << glu::TYPE_SAMPLER_2D, getFragmentOutputType(texFmt1),
                                        fmtInfo1.valueMax - fmtInfo1.valueMin, fmtInfo1.valueMin);
        Texture2DShader multiTexShader(DataTypes() << glu::getSampler2DType(texFmt0) << glu::getSampler2DType(texFmt1),
                                       glu::TYPE_FLOAT_VEC4);

        uint32_t texToFbo0ShaderID = getCurrentContext()->createProgram(&texToFbo0Shader);
        uint32_t texToFbo1ShaderID = getCurrentContext()->createProgram(&texToFbo1Shader);
        uint32_t multiTexShaderID  = getCurrentContext()->createProgram(&multiTexShader);

        // Setup shaders
        multiTexShader.setTexScaleBias(0, fmtInfo0.lookupScale * 0.5f, fmtInfo0.lookupBias * 0.5f);
        multiTexShader.setTexScaleBias(1, fmtInfo1.lookupScale * 0.5f, fmtInfo1.lookupBias * 0.5f);
        texToFbo0Shader.setUniforms(*getCurrentContext(), texToFbo0ShaderID);
        texToFbo1Shader.setUniforms(*getCurrentContext(), texToFbo1ShaderID);
        multiTexShader.setUniforms(*getCurrentContext(), multiTexShaderID);

        // Framebuffers.
        uint32_t fbo0 = 0;
        uint32_t fbo1 = 0;
        uint32_t tex0 = 0;
        uint32_t tex1 = 0;

        for (int ndx = 0; ndx < 2; ndx++)
        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(ndx ? texFmt1 : texFmt0);
            uint32_t format                 = ndx ? m_tex1Fmt : m_tex0Fmt;
            bool isFilterable               = glu::isGLInternalColorFormatFilterable(format);
            const IVec2 &size               = ndx ? m_tex1Size : m_tex0Size;
            uint32_t &fbo                   = ndx ? fbo1 : fbo0;
            uint32_t &tex                   = ndx ? tex1 : tex0;

            glGenFramebuffers(1, &fbo);
            glGenTextures(1, &tex);

            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isFilterable ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, isFilterable ? GL_LINEAR : GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0, transferFmt.format, transferFmt.dataType,
                         nullptr);

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);
        }

        // Render textures to both framebuffers.
        for (int ndx = 0; ndx < 2; ndx++)
        {
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = 128;
            const int texH          = 128;
            uint32_t tmpTex         = 0;
            uint32_t fbo            = ndx ? fbo1 : fbo0;
            const IVec2 &viewport   = ndx ? m_tex1Size : m_tex0Size;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            if (ndx == 0)
                tcu::fillWithComponentGradients(data.getAccess(), Vec4(0.0f), Vec4(1.0f));
            else
                tcu::fillWithGrid(data.getAccess(), 8, Vec4(0.2f, 0.7f, 0.1f, 1.0f), Vec4(0.7f, 0.1f, 0.5f, 0.8f));

            glGenTextures(1, &tmpTex);
            glBindTexture(GL_TEXTURE_2D, tmpTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, viewport.x(), viewport.y());
            sglr::drawQuad(*getCurrentContext(), ndx ? texToFbo1ShaderID : texToFbo0ShaderID, Vec3(-1.0f, -1.0f, 0.0f),
                           Vec3(1.0f, 1.0f, 0.0f));
        }

        // Render to framebuffer.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, getWidth(), getHeight());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, tex1);
        sglr::drawQuad(*getCurrentContext(), multiTexShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    uint32_t m_tex0Fmt;
    uint32_t m_tex1Fmt;
    IVec2 m_tex0Size;
    IVec2 m_tex1Size;
};

class FboColorTexCubeCase : public FboColorbufferCase
{
public:
    FboColorTexCubeCase(Context &context, const char *name, const char *description, uint32_t texFmt,
                        const IVec2 &texSize)
        : FboColorbufferCase(context, name, description, texFmt)
        , m_texSize(texSize)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        static const uint32_t cubeGLFaces[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                                               GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                               GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

        static const tcu::CubeFace cubeTexFaces[] = {tcu::CUBEFACE_POSITIVE_X, tcu::CUBEFACE_POSITIVE_Y,
                                                     tcu::CUBEFACE_POSITIVE_Z, tcu::CUBEFACE_NEGATIVE_X,
                                                     tcu::CUBEFACE_NEGATIVE_Y, tcu::CUBEFACE_NEGATIVE_Z};

        de::Random rnd(deStringHash(getName()) ^ 0x9eef603d);
        tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_format);
        tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);

        Texture2DShader texToFboShader(DataTypes() << glu::TYPE_SAMPLER_2D, getFragmentOutputType(texFmt),
                                       fmtInfo.valueMax - fmtInfo.valueMin, fmtInfo.valueMin);
        TextureCubeShader cubeTexShader(glu::getSamplerCubeType(texFmt), glu::TYPE_FLOAT_VEC4);

        uint32_t texToFboShaderID = getCurrentContext()->createProgram(&texToFboShader);
        uint32_t cubeTexShaderID  = getCurrentContext()->createProgram(&cubeTexShader);

        // Setup shaders
        texToFboShader.setUniforms(*getCurrentContext(), texToFboShaderID);
        cubeTexShader.setTexScaleBias(fmtInfo.lookupScale, fmtInfo.lookupBias);

        // Framebuffers.
        std::vector<uint32_t> fbos;
        uint32_t tex;

        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(texFmt);
            bool isFilterable               = glu::isGLInternalColorFormatFilterable(m_format);
            const IVec2 &size               = m_texSize;

            glGenTextures(1, &tex);

            glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, isFilterable ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, isFilterable ? GL_LINEAR : GL_NEAREST);

            // Generate an image and FBO for each cube face
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cubeGLFaces); ndx++)
                glTexImage2D(cubeGLFaces[ndx], 0, m_format, size.x(), size.y(), 0, transferFmt.format,
                             transferFmt.dataType, nullptr);
            checkError();

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cubeGLFaces); ndx++)
            {
                uint32_t layerFbo;

                glGenFramebuffers(1, &layerFbo);
                glBindFramebuffer(GL_FRAMEBUFFER, layerFbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cubeGLFaces[ndx], tex, 0);
                checkError();
                checkFramebufferStatus(GL_FRAMEBUFFER);

                fbos.push_back(layerFbo);
            }
        }

        // Render test images to random cube faces
        std::vector<int> order;

        for (size_t n = 0; n < fbos.size(); n++)
            order.push_back((int)n);
        rnd.shuffle(order.begin(), order.end());

        DE_ASSERT(order.size() >= 4);
        for (int ndx = 0; ndx < 4; ndx++)
        {
            const int face          = order[ndx];
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = 128;
            const int texH          = 128;
            uint32_t tmpTex         = 0;
            const uint32_t fbo      = fbos[face];
            const IVec2 &viewport   = m_texSize;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            tcu::fillWithGrid(data.getAccess(), 8, generateRandomColor(rnd), Vec4(0.0f));

            glGenTextures(1, &tmpTex);
            glBindTexture(GL_TEXTURE_2D, tmpTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, viewport.x(), viewport.y());
            sglr::drawQuad(*getCurrentContext(), texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            checkError();

            // Render to framebuffer
            {
                const Vec3 p0 = Vec3(float(ndx % 2) - 1.0f, float(ndx / 2) - 1.0f, 0.0f);
                const Vec3 p1 = p0 + Vec3(1.0f, 1.0f, 0.0f);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, getWidth(), getHeight());

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

                cubeTexShader.setFace(cubeTexFaces[face]);
                cubeTexShader.setUniforms(*getCurrentContext(), cubeTexShaderID);

                sglr::drawQuad(*getCurrentContext(), cubeTexShaderID, p0, p1);
                checkError();
            }
        }

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    IVec2 m_texSize;
};

class FboColorTex2DArrayCase : public FboColorbufferCase
{
public:
    FboColorTex2DArrayCase(Context &context, const char *name, const char *description, uint32_t texFmt,
                           const IVec3 &texSize)
        : FboColorbufferCase(context, name, description, texFmt)
        , m_texSize(texSize)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        de::Random rnd(deStringHash(getName()) ^ 0xed607a89);
        tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_format);
        tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);

        Texture2DShader texToFboShader(DataTypes() << glu::TYPE_SAMPLER_2D, getFragmentOutputType(texFmt),
                                       fmtInfo.valueMax - fmtInfo.valueMin, fmtInfo.valueMin);
        Texture2DArrayShader arrayTexShader(glu::getSampler2DArrayType(texFmt), glu::TYPE_FLOAT_VEC4);

        uint32_t texToFboShaderID = getCurrentContext()->createProgram(&texToFboShader);
        uint32_t arrayTexShaderID = getCurrentContext()->createProgram(&arrayTexShader);

        // Setup textures
        texToFboShader.setUniforms(*getCurrentContext(), texToFboShaderID);
        arrayTexShader.setTexScaleBias(fmtInfo.lookupScale, fmtInfo.lookupBias);

        // Framebuffers.
        std::vector<uint32_t> fbos;
        uint32_t tex;

        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(texFmt);
            bool isFilterable               = glu::isGLInternalColorFormatFilterable(m_format);
            const IVec3 &size               = m_texSize;

            glGenTextures(1, &tex);

            glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, isFilterable ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, isFilterable ? GL_LINEAR : GL_NEAREST);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, m_format, size.x(), size.y(), size.z(), 0, transferFmt.format,
                         transferFmt.dataType, nullptr);

            // Generate an FBO for each layer
            for (int ndx = 0; ndx < m_texSize.z(); ndx++)
            {
                uint32_t layerFbo;

                glGenFramebuffers(1, &layerFbo);
                glBindFramebuffer(GL_FRAMEBUFFER, layerFbo);
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, ndx);
                checkError();
                checkFramebufferStatus(GL_FRAMEBUFFER);

                fbos.push_back(layerFbo);
            }
        }

        // Render test images to random texture layers
        std::vector<int> order;

        for (size_t n = 0; n < fbos.size(); n++)
            order.push_back((int)n);
        rnd.shuffle(order.begin(), order.end());

        for (size_t ndx = 0; ndx < order.size(); ndx++)
        {
            const int layer         = order[ndx];
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = 128;
            const int texH          = 128;
            uint32_t tmpTex         = 0;
            const uint32_t fbo      = fbos[layer];
            const IVec3 &viewport   = m_texSize;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            tcu::fillWithGrid(data.getAccess(), 8, generateRandomColor(rnd), Vec4(0.0f));

            glGenTextures(1, &tmpTex);
            glBindTexture(GL_TEXTURE_2D, tmpTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, viewport.x(), viewport.y());
            sglr::drawQuad(*getCurrentContext(), texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            checkError();

            // Render to framebuffer
            {
                const Vec3 p0 = Vec3(float(ndx % 2) - 1.0f, float(ndx / 2) - 1.0f, 0.0f);
                const Vec3 p1 = p0 + Vec3(1.0f, 1.0f, 0.0f);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, getWidth(), getHeight());

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D_ARRAY, tex);

                arrayTexShader.setLayer(layer);
                arrayTexShader.setUniforms(*getCurrentContext(), arrayTexShaderID);

                sglr::drawQuad(*getCurrentContext(), arrayTexShaderID, p0, p1);
                checkError();
            }
        }

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    IVec3 m_texSize;
};

class FboColorTex3DCase : public FboColorbufferCase
{
public:
    FboColorTex3DCase(Context &context, const char *name, const char *description, uint32_t texFmt,
                      const IVec3 &texSize)
        : FboColorbufferCase(context, name, description, texFmt)
        , m_texSize(texSize)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        de::Random rnd(deStringHash(getName()) ^ 0x74d947b2);
        tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_format);
        tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);

        Texture2DShader texToFboShader(DataTypes() << glu::TYPE_SAMPLER_2D, getFragmentOutputType(texFmt),
                                       fmtInfo.valueMax - fmtInfo.valueMin, fmtInfo.valueMin);
        Texture3DShader tdTexShader(glu::getSampler3DType(texFmt), glu::TYPE_FLOAT_VEC4);

        uint32_t texToFboShaderID = getCurrentContext()->createProgram(&texToFboShader);
        uint32_t tdTexShaderID    = getCurrentContext()->createProgram(&tdTexShader);

        // Setup shaders
        texToFboShader.setUniforms(*getCurrentContext(), texToFboShaderID);
        tdTexShader.setTexScaleBias(fmtInfo.lookupScale, fmtInfo.lookupBias);

        // Framebuffers.
        std::vector<uint32_t> fbos;
        uint32_t tex;

        {
            glu::TransferFormat transferFmt = glu::getTransferFormat(texFmt);
            const IVec3 &size               = m_texSize;

            glGenTextures(1, &tex);

            glBindTexture(GL_TEXTURE_3D, tex);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage3D(GL_TEXTURE_3D, 0, m_format, size.x(), size.y(), size.z(), 0, transferFmt.format,
                         transferFmt.dataType, nullptr);

            // Generate an FBO for each layer
            for (int ndx = 0; ndx < m_texSize.z(); ndx++)
            {
                uint32_t layerFbo;

                glGenFramebuffers(1, &layerFbo);
                glBindFramebuffer(GL_FRAMEBUFFER, layerFbo);
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, ndx);
                checkError();
                checkFramebufferStatus(GL_FRAMEBUFFER);

                fbos.push_back(layerFbo);
            }
        }

        // Render test images to random texture layers
        std::vector<int> order;

        for (size_t n = 0; n < fbos.size(); n++)
            order.push_back((int)n);
        rnd.shuffle(order.begin(), order.end());

        for (size_t ndx = 0; ndx < order.size(); ndx++)
        {
            const int layer         = order[ndx];
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = 128;
            const int texH          = 128;
            uint32_t tmpTex         = 0;
            const uint32_t fbo      = fbos[layer];
            const IVec3 &viewport   = m_texSize;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            tcu::fillWithGrid(data.getAccess(), 8, generateRandomColor(rnd), Vec4(0.0f));

            glGenTextures(1, &tmpTex);
            glBindTexture(GL_TEXTURE_2D, tmpTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, viewport.x(), viewport.y());
            sglr::drawQuad(*getCurrentContext(), texToFboShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            checkError();

            // Render to framebuffer
            {
                const Vec3 p0 = Vec3(float(ndx % 2) - 1.0f, float(ndx / 2) - 1.0f, 0.0f);
                const Vec3 p1 = p0 + Vec3(1.0f, 1.0f, 0.0f);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, getWidth(), getHeight());

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_3D, tex);

                tdTexShader.setDepth(float(layer) / float(m_texSize.z() - 1));
                tdTexShader.setUniforms(*getCurrentContext(), tdTexShaderID);

                sglr::drawQuad(*getCurrentContext(), tdTexShaderID, p0, p1);
                checkError();
            }
        }

        readPixels(dst, 0, 0, getWidth(), getHeight());
    }

private:
    IVec3 m_texSize;
};

class FboBlendCase : public FboColorbufferCase
{
public:
    FboBlendCase(Context &context, const char *name, const char *desc, uint32_t format, IVec2 size, uint32_t funcRGB,
                 uint32_t funcAlpha, uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha)
        : FboColorbufferCase(context, name, desc, format)
        , m_size(size)
        , m_funcRGB(funcRGB)
        , m_funcAlpha(funcAlpha)
        , m_srcRGB(srcRGB)
        , m_dstRGB(dstRGB)
        , m_srcAlpha(srcAlpha)
        , m_dstAlpha(dstAlpha)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        // \note Assumes floating-point or fixed-point format.
        tcu::TextureFormat fboFmt = glu::mapGLInternalFormat(m_format);
        Texture2DShader texShader(DataTypes() << glu::TYPE_SAMPLER_2D, glu::TYPE_FLOAT_VEC4);
        GradientShader gradShader(glu::TYPE_FLOAT_VEC4);
        uint32_t texShaderID  = getCurrentContext()->createProgram(&texShader);
        uint32_t gradShaderID = getCurrentContext()->createProgram(&gradShader);
        uint32_t fbo          = 0;
        uint32_t rbo          = 0;

        // Setup shaders
        texShader.setUniforms(*getCurrentContext(), texShaderID);
        gradShader.setGradient(*getCurrentContext(), gradShaderID, tcu::Vec4(0.0f), tcu::Vec4(1.0f));

        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(1, &rbo);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, m_format, m_size.x(), m_size.y());
        checkError();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
        checkError();
        checkFramebufferStatus(GL_FRAMEBUFFER);

        glViewport(0, 0, m_size.x(), m_size.y());

        // Fill framebuffer with grid pattern.
        {
            const uint32_t format   = GL_RGBA;
            const uint32_t dataType = GL_UNSIGNED_BYTE;
            const int texW          = 128;
            const int texH          = 128;
            uint32_t gridTex        = 0;
            tcu::TextureLevel data(glu::mapGLTransferFormat(format, dataType), texW, texH, 1);

            tcu::fillWithGrid(data.getAccess(), 8, Vec4(0.2f, 0.7f, 0.1f, 1.0f), Vec4(0.7f, 0.1f, 0.5f, 0.8f));

            glGenTextures(1, &gridTex);
            glBindTexture(GL_TEXTURE_2D, gridTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, format, texW, texH, 0, format, dataType, data.getAccess().getDataPtr());

            sglr::drawQuad(*getCurrentContext(), texShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
        }

        // Setup blend.
        glEnable(GL_BLEND);
        glBlendEquationSeparate(m_funcRGB, m_funcAlpha);
        glBlendFuncSeparate(m_srcRGB, m_dstRGB, m_srcAlpha, m_dstAlpha);

        // Render gradient with blend.
        sglr::drawQuad(*getCurrentContext(), gradShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));

        readPixels(dst, 0, 0, m_size.x(), m_size.y(), fboFmt, Vec4(1.0f), Vec4(0.0f));
    }

private:
    IVec2 m_size;
    uint32_t m_funcRGB;
    uint32_t m_funcAlpha;
    uint32_t m_srcRGB;
    uint32_t m_dstRGB;
    uint32_t m_srcAlpha;
    uint32_t m_dstAlpha;
};

class FboRepeatedClearSampleTex2DCase : public FboColorbufferCase
{
public:
    FboRepeatedClearSampleTex2DCase(Context &context, const char *name, const char *desc, uint32_t format)
        : FboColorbufferCase(context, name, desc, format)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        const tcu::TextureFormat fboFormat   = glu::mapGLInternalFormat(m_format);
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(fboFormat);
        const int numRowsCols                = 4;
        const int cellSize                   = 16;
        const int fboSizes[]                 = {cellSize, cellSize * numRowsCols};

        Texture2DShader fboBlitShader(DataTypes() << glu::getSampler2DType(fboFormat), getFragmentOutputType(fboFormat),
                                      Vec4(1.0f), Vec4(0.0f));
        const uint32_t fboBlitShaderID = getCurrentContext()->createProgram(&fboBlitShader);

        de::Random rnd(18169662);
        uint32_t fbos[]     = {0, 0};
        uint32_t textures[] = {0, 0};

        glGenFramebuffers(2, &fbos[0]);
        glGenTextures(2, &textures[0]);

        for (int fboNdx = 0; fboNdx < DE_LENGTH_OF_ARRAY(fbos); fboNdx++)
        {
            glBindTexture(GL_TEXTURE_2D, textures[fboNdx]);
            glTexStorage2D(GL_TEXTURE_2D, 1, m_format, fboSizes[fboNdx], fboSizes[fboNdx]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            checkError();

            glBindFramebuffer(GL_FRAMEBUFFER, fbos[fboNdx]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[fboNdx], 0);
            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);
        }

        // larger fbo bound -- clear to transparent black
        clearColorBuffer(fboFormat, Vec4(0.0f));

        fboBlitShader.setUniforms(*getCurrentContext(), fboBlitShaderID);
        glBindTexture(GL_TEXTURE_2D, textures[0]);

        for (int cellY = 0; cellY < numRowsCols; cellY++)
            for (int cellX = 0; cellX < numRowsCols; cellX++)
            {
                const Vec4 color = randomVector<4>(rnd, fmtInfo.valueMin, fmtInfo.valueMax);

                glBindFramebuffer(GL_FRAMEBUFFER, fbos[0]);
                clearColorBuffer(fboFormat, color);

                glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]);
                glViewport(cellX * cellSize, cellY * cellSize, cellSize, cellSize);
                sglr::drawQuad(*getCurrentContext(), fboBlitShaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
            }

        readPixels(dst, 0, 0, fboSizes[1], fboSizes[1], fboFormat, fmtInfo.lookupScale, fmtInfo.lookupBias);
        checkError();
    }
};

class FboRepeatedClearBlitTex2DCase : public FboColorbufferCase
{
public:
    FboRepeatedClearBlitTex2DCase(Context &context, const char *name, const char *desc, uint32_t format)
        : FboColorbufferCase(context, name, desc, format)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        const tcu::TextureFormat fboFormat   = glu::mapGLInternalFormat(m_format);
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(fboFormat);
        const int numRowsCols                = 4;
        const int cellSize                   = 16;
        const int fboSizes[]                 = {cellSize, cellSize * numRowsCols};

        de::Random rnd(18169662);
        uint32_t fbos[]     = {0, 0};
        uint32_t textures[] = {0, 0};

        glGenFramebuffers(2, &fbos[0]);
        glGenTextures(2, &textures[0]);

        for (int fboNdx = 0; fboNdx < DE_LENGTH_OF_ARRAY(fbos); fboNdx++)
        {
            glBindTexture(GL_TEXTURE_2D, textures[fboNdx]);
            glTexStorage2D(GL_TEXTURE_2D, 1, m_format, fboSizes[fboNdx], fboSizes[fboNdx]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            checkError();

            glBindFramebuffer(GL_FRAMEBUFFER, fbos[fboNdx]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[fboNdx], 0);
            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);
        }

        // larger fbo bound -- clear to transparent black
        clearColorBuffer(fboFormat, Vec4(0.0f));

        for (int cellY = 0; cellY < numRowsCols; cellY++)
            for (int cellX = 0; cellX < numRowsCols; cellX++)
            {
                const Vec4 color = randomVector<4>(rnd, fmtInfo.valueMin, fmtInfo.valueMax);

                glBindFramebuffer(GL_FRAMEBUFFER, fbos[0]);
                clearColorBuffer(fboFormat, color);

                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);
                glBlitFramebuffer(0, 0, cellSize, cellSize, cellX * cellSize, cellY * cellSize, (cellX + 1) * cellSize,
                                  (cellY + 1) * cellSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

        glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]);
        readPixels(dst, 0, 0, fboSizes[1], fboSizes[1], fboFormat, fmtInfo.lookupScale, fmtInfo.lookupBias);
        checkError();
    }
};

class FboRepeatedClearBlitRboCase : public FboColorbufferCase
{
public:
    FboRepeatedClearBlitRboCase(Context &context, const char *name, const char *desc, uint32_t format)
        : FboColorbufferCase(context, name, desc, format)
    {
    }

protected:
    void preCheck(void)
    {
        checkFormatSupport(m_format);
    }

    void render(tcu::Surface &dst)
    {
        const tcu::TextureFormat fboFormat   = glu::mapGLInternalFormat(m_format);
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(fboFormat);
        const int numRowsCols                = 4;
        const int cellSize                   = 16;
        const int fboSizes[]                 = {cellSize, cellSize * numRowsCols};

        de::Random rnd(18169662);
        uint32_t fbos[] = {0, 0};
        uint32_t rbos[] = {0, 0};

        glGenFramebuffers(2, &fbos[0]);
        glGenRenderbuffers(2, &rbos[0]);

        for (int fboNdx = 0; fboNdx < DE_LENGTH_OF_ARRAY(fbos); fboNdx++)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, rbos[fboNdx]);
            glRenderbufferStorage(GL_RENDERBUFFER, m_format, fboSizes[fboNdx], fboSizes[fboNdx]);
            checkError();

            glBindFramebuffer(GL_FRAMEBUFFER, fbos[fboNdx]);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbos[fboNdx]);
            checkError();
            checkFramebufferStatus(GL_FRAMEBUFFER);
        }

        // larger fbo bound -- clear to transparent black
        clearColorBuffer(fboFormat, Vec4(0.0f));

        for (int cellY = 0; cellY < numRowsCols; cellY++)
            for (int cellX = 0; cellX < numRowsCols; cellX++)
            {
                const Vec4 color = randomVector<4>(rnd, fmtInfo.valueMin, fmtInfo.valueMax);

                glBindFramebuffer(GL_FRAMEBUFFER, fbos[0]);
                clearColorBuffer(fboFormat, color);

                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);
                glBlitFramebuffer(0, 0, cellSize, cellSize, cellX * cellSize, cellY * cellSize, (cellX + 1) * cellSize,
                                  (cellY + 1) * cellSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

        glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]);
        readPixels(dst, 0, 0, fboSizes[1], fboSizes[1], fboFormat, fmtInfo.lookupScale, fmtInfo.lookupBias);
        checkError();
    }
};

FboColorTests::FboColorTests(Context &context) : TestCaseGroup(context, "color", "Colorbuffer tests")
{
}

FboColorTests::~FboColorTests(void)
{
}

void FboColorTests::init(void)
{
    static const uint32_t colorFormats[] = {
        // RGBA formats
        GL_RGBA32I, GL_RGBA32UI, GL_RGBA16I, GL_RGBA16UI, GL_RGBA8, GL_RGBA8I, GL_RGBA8UI, GL_SRGB8_ALPHA8, GL_RGB10_A2,
        GL_RGB10_A2UI, GL_RGBA4, GL_RGB5_A1,

        // RGB formats
        GL_RGB8, GL_RGB565,

        // RG formats
        GL_RG32I, GL_RG32UI, GL_RG16I, GL_RG16UI, GL_RG8, GL_RG8I, GL_RG8UI,

        // R formats
        GL_R32I, GL_R32UI, GL_R16I, GL_R16UI, GL_R8, GL_R8I, GL_R8UI,

        // GL_EXT_color_buffer_float
        GL_RGBA32F, GL_RGBA16F, GL_R11F_G11F_B10F, GL_RG32F, GL_RG16F, GL_R32F, GL_R16F,

        // GL_EXT_color_buffer_half_float
        GL_RGB16F};

    // .clear
    {
        tcu::TestCaseGroup *clearGroup = new tcu::TestCaseGroup(m_testCtx, "clear", "Color clears");
        addChild(clearGroup);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(colorFormats); ndx++)
            clearGroup->addChild(
                new FboColorClearCase(m_context, getFormatName(colorFormats[ndx]), "", colorFormats[ndx], 129, 117));
    }

    // .tex2d
    {
        tcu::TestCaseGroup *tex2DGroup = new tcu::TestCaseGroup(m_testCtx, "tex2d", "Texture 2D tests");
        addChild(tex2DGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
            tex2DGroup->addChild(new FboColorMultiTex2DCase(m_context, getFormatName(colorFormats[fmtNdx]), "",
                                                            colorFormats[fmtNdx], IVec2(129, 117), colorFormats[fmtNdx],
                                                            IVec2(99, 128)));
    }

    // .texcube
    {
        tcu::TestCaseGroup *texCubeGroup = new tcu::TestCaseGroup(m_testCtx, "texcube", "Texture cube map tests");
        addChild(texCubeGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
            texCubeGroup->addChild(new FboColorTexCubeCase(m_context, getFormatName(colorFormats[fmtNdx]), "",
                                                           colorFormats[fmtNdx], IVec2(128, 128)));
    }

    // .tex2darray
    {
        tcu::TestCaseGroup *tex2DArrayGroup = new tcu::TestCaseGroup(m_testCtx, "tex2darray", "Texture 2D array tests");
        addChild(tex2DArrayGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
            tex2DArrayGroup->addChild(new FboColorTex2DArrayCase(m_context, getFormatName(colorFormats[fmtNdx]), "",
                                                                 colorFormats[fmtNdx], IVec3(128, 128, 5)));
    }

    // .tex3d
    {
        tcu::TestCaseGroup *tex3DGroup = new tcu::TestCaseGroup(m_testCtx, "tex3d", "Texture 3D tests");
        addChild(tex3DGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
            tex3DGroup->addChild(new FboColorTex3DCase(m_context, getFormatName(colorFormats[fmtNdx]), "",
                                                       colorFormats[fmtNdx], IVec3(128, 128, 5)));
    }

    // .blend
    {
        tcu::TestCaseGroup *blendGroup = new tcu::TestCaseGroup(m_testCtx, "blend", "Blending tests");
        addChild(blendGroup);

        for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
        {
            uint32_t format                   = colorFormats[fmtNdx];
            tcu::TextureFormat texFmt         = glu::mapGLInternalFormat(format);
            tcu::TextureChannelClass fmtClass = tcu::getTextureChannelClass(texFmt.type);
            string fmtName                    = getFormatName(format);

            if (texFmt.type == tcu::TextureFormat::FLOAT || fmtClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                fmtClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
                continue; // Blending is not supported.

            blendGroup->addChild(new FboBlendCase(m_context, (fmtName + "_src_over").c_str(), "", format,
                                                  IVec2(127, 111), GL_FUNC_ADD, GL_FUNC_ADD, GL_SRC_ALPHA,
                                                  GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));
        }
    }

    // .repeated_clear
    {
        tcu::TestCaseGroup *const repeatedClearGroup =
            new tcu::TestCaseGroup(m_testCtx, "repeated_clear", "Repeated clears and blits");
        addChild(repeatedClearGroup);

        // .sample.tex2d
        {
            tcu::TestCaseGroup *const sampleGroup = new tcu::TestCaseGroup(m_testCtx, "sample", "Read by sampling");
            repeatedClearGroup->addChild(sampleGroup);

            tcu::TestCaseGroup *const tex2DGroup = new tcu::TestCaseGroup(m_testCtx, "tex2d", "2D Texture");
            sampleGroup->addChild(tex2DGroup);

            for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
                tex2DGroup->addChild(new FboRepeatedClearSampleTex2DCase(m_context, getFormatName(colorFormats[fmtNdx]),
                                                                         "", colorFormats[fmtNdx]));
        }

        // .blit
        {
            tcu::TestCaseGroup *const blitGroup = new tcu::TestCaseGroup(m_testCtx, "blit", "Blitted");
            repeatedClearGroup->addChild(blitGroup);

            // .tex2d
            {
                tcu::TestCaseGroup *const tex2DGroup = new tcu::TestCaseGroup(m_testCtx, "tex2d", "2D Texture");
                blitGroup->addChild(tex2DGroup);

                for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
                    tex2DGroup->addChild(new FboRepeatedClearBlitTex2DCase(
                        m_context, getFormatName(colorFormats[fmtNdx]), "", colorFormats[fmtNdx]));
            }

            // .rbo
            {
                tcu::TestCaseGroup *const rboGroup = new tcu::TestCaseGroup(m_testCtx, "rbo", "Renderbuffer");
                blitGroup->addChild(rboGroup);

                for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(colorFormats); fmtNdx++)
                    rboGroup->addChild(new FboRepeatedClearBlitRboCase(m_context, getFormatName(colorFormats[fmtNdx]),
                                                                       "", colorFormats[fmtNdx]));
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
