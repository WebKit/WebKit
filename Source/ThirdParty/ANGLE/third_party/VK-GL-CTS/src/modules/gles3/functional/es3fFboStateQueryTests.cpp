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
 * \brief Fbo state query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFboStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "deMath.h"

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

void checkAttachmentComponentSizeAtLeast(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLenum target,
                                         GLenum attachment, int r, int g, int b, int a, int d, int s)
{
    using tcu::TestLog;

    const int referenceSizes[] = {r, g, b, a, d, s};
    const GLenum paramNames[]  = {GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,   GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                  GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,  GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
                                  GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE};

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(referenceSizes) == DE_LENGTH_OF_ARRAY(paramNames));

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(referenceSizes); ++ndx)
    {
        if (referenceSizes[ndx] == -1)
            continue;

        StateQueryMemoryWriteGuard<GLint> state;
        gl.glGetFramebufferAttachmentParameteriv(target, attachment, paramNames[ndx], &state);

        if (!state.verifyValidity(testCtx))
        {
            gl.glGetError(); // just log the error
            continue;
        }

        if (state < referenceSizes[ndx])
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: Expected greater or equal to " << referenceSizes[ndx]
                             << "; got " << state << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
        }
    }
}

void checkAttachmentComponentSizeExactly(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLenum target,
                                         GLenum attachment, int r, int g, int b, int a, int d, int s)
{
    using tcu::TestLog;

    const int referenceSizes[] = {r, g, b, a, d, s};
    const GLenum paramNames[]  = {GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,   GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                  GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,  GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
                                  GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE};

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(referenceSizes) == DE_LENGTH_OF_ARRAY(paramNames));

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(referenceSizes); ++ndx)
    {
        if (referenceSizes[ndx] == -1)
            continue;

        StateQueryMemoryWriteGuard<GLint> state;
        gl.glGetFramebufferAttachmentParameteriv(target, attachment, paramNames[ndx], &state);

        if (!state.verifyValidity(testCtx))
        {
            gl.glGetError(); // just log the error
            continue;
        }

        if (state != referenceSizes[ndx])
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << referenceSizes[ndx] << "; got " << state
                             << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
        }
    }
}

void checkIntEquals(tcu::TestContext &testCtx, GLint got, GLint expected)
{
    using tcu::TestLog;

    if (got != expected)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << expected << "; got " << got
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
    }
}

void checkIntEqualsAny(tcu::TestContext &testCtx, GLint got, GLint expected0, GLint expected1)
{
    using tcu::TestLog;

    if (got != expected0 && got != expected1)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << expected0 << " or " << expected1 << "; got "
                         << got << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
    }
}

void checkAttachmentParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLenum target, GLenum attachment,
                          GLenum pname, GLenum reference)
{
    StateQueryMemoryWriteGuard<GLint> state;
    gl.glGetFramebufferAttachmentParameteriv(target, attachment, pname, &state);

    if (state.verifyValidity(testCtx))
        checkIntEquals(testCtx, state, reference);
}

void checkColorAttachmentParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLenum target, GLenum pname,
                               GLenum reference)
{
    checkAttachmentParam(testCtx, gl, target, GL_COLOR_ATTACHMENT0, pname, reference);
}

class DefaultFramebufferCase : public ApiCase
{
public:
    DefaultFramebufferCase(Context &context, const char *name, const char *description, GLenum framebufferTarget)
        : ApiCase(context, name, description)
        , m_framebufferTarget(framebufferTarget)
    {
    }

    void test(void)
    {
        const bool hasColorBuffer = m_context.getRenderTarget().getPixelFormat().redBits > 0 ||
                                    m_context.getRenderTarget().getPixelFormat().greenBits > 0 ||
                                    m_context.getRenderTarget().getPixelFormat().blueBits > 0 ||
                                    m_context.getRenderTarget().getPixelFormat().alphaBits > 0;

        bool isGlCore45        = glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 5));
        GLenum colorAttachment = isGlCore45 ? GL_FRONT : GL_BACK;
        if (isGlCore45)
        {
            // Make sure GL_FRONT is available. If not, use GL_BACK instead.
            GLint objectType = GL_NONE;
            glGetFramebufferAttachmentParameteriv(m_framebufferTarget, colorAttachment,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objectType);
            if (objectType == GL_NONE)
            {
                colorAttachment = GL_BACK;
            }
        }

        const GLenum attachments[]    = {colorAttachment, GL_DEPTH, GL_STENCIL};
        const bool attachmentExists[] = {hasColorBuffer, m_context.getRenderTarget().getDepthBits() > 0,
                                         m_context.getRenderTarget().getStencilBits() > 0};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(attachments); ++ndx)
        {
            StateQueryMemoryWriteGuard<GLint> state;
            glGetFramebufferAttachmentParameteriv(m_framebufferTarget, attachments[ndx],
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &state);
            expectError(GL_NO_ERROR);

            if (state.verifyValidity(m_testCtx))
            {
                if (attachmentExists[ndx])
                {
                    checkIntEquals(m_testCtx, state, GL_FRAMEBUFFER_DEFAULT);
                }
                else
                {
                    // \note [jarkko] FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE "identifes the type of object which contains the attached image". However, it
                    // is unclear if an object of type FRAMEBUFFER_DEFAULT can contain a null image (or a 0-bits-per-pixel image). Accept both
                    // FRAMEBUFFER_DEFAULT and NONE as valid results in these cases.
                    checkIntEqualsAny(m_testCtx, state, GL_FRAMEBUFFER_DEFAULT, GL_NONE);
                }
            }
        }
    }

private:
    GLenum m_framebufferTarget;
};

class AttachmentObjectCase : public ApiCase
{
public:
    AttachmentObjectCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // initial
        {
            checkAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_NONE);
            checkAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, 0);
            expectError(GL_NO_ERROR);
        }

        // texture
        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            expectError(GL_NO_ERROR);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                      GL_TEXTURE);
            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                      textureID);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        // rb
        {
            GLuint renderbufferID = 0;
            glGenRenderbuffers(1, &renderbufferID);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, 128, 128);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                      GL_RENDERBUFFER);
            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                      renderbufferID);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &renderbufferID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentTextureLevelCase : public ApiCase
{
public:
    AttachmentTextureLevelCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        for (int mipmapLevel = 0; mipmapLevel < 7; ++mipmapLevel)
        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexStorage2D(GL_TEXTURE_2D, 7, GL_RGB8, 128, 128);
            expectError(GL_NO_ERROR);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, mipmapLevel);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
                                      mipmapLevel);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentTextureCubeMapFaceCase : public ApiCase
{
public:
    AttachmentTextureCubeMapFaceCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
            expectError(GL_NO_ERROR);

            glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGB8, 128, 128);
            expectError(GL_NO_ERROR);

            const GLenum faces[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(faces); ++ndx)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, faces[ndx], textureID, 0);
                checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, faces[ndx]);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentTextureLayerCase : public ApiCase
{
public:
    AttachmentTextureLayerCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // tex3d
        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_3D, textureID);
            glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 16, 16, 16);

            for (int layer = 0; layer < 16; ++layer)
            {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureID, 0, layer);
                checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER,
                                          layer);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }
        // tex2d array
        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D_ARRAY, textureID);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 16, 16, 16);

            for (int layer = 0; layer < 16; ++layer)
            {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureID, 0, layer);
                checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER,
                                          layer);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentTextureColorCodingCase : public ApiCase
{
public:
    AttachmentTextureColorCodingCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // rgb8 color
        {
            GLuint renderbufferID = 0;
            glGenRenderbuffers(1, &renderbufferID);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, 128, 128);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
                                      GL_LINEAR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &renderbufferID);
        }

        // srgb8_alpha8 color
        {
            GLuint renderbufferID = 0;
            glGenRenderbuffers(1, &renderbufferID);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, 128, 128);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
                                      GL_SRGB);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &renderbufferID);
        }

        // depth
        {
            GLuint renderbufferID = 0;
            glGenRenderbuffers(1, &renderbufferID);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 128, 128);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            checkAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                 GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, GL_LINEAR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &renderbufferID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentTextureComponentTypeCase : public ApiCase
{
public:
    AttachmentTextureComponentTypeCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // color-renderable required texture formats
        const struct RequiredColorFormat
        {
            GLenum internalFormat;
            GLenum componentType;
        } requiredColorformats[] = {{GL_R8, GL_UNSIGNED_NORMALIZED},
                                    {GL_RG8, GL_UNSIGNED_NORMALIZED},
                                    {GL_RGB8, GL_UNSIGNED_NORMALIZED},
                                    {GL_RGB565, GL_UNSIGNED_NORMALIZED},
                                    {GL_RGBA4, GL_UNSIGNED_NORMALIZED},
                                    {GL_RGB5_A1, GL_UNSIGNED_NORMALIZED},
                                    {GL_RGBA8, GL_UNSIGNED_NORMALIZED},
                                    {GL_RGB10_A2, GL_UNSIGNED_NORMALIZED},
                                    {GL_RGB10_A2UI, GL_UNSIGNED_INT},
                                    {GL_SRGB8_ALPHA8, GL_UNSIGNED_NORMALIZED},
                                    {GL_R8I, GL_INT},
                                    {GL_R8UI, GL_UNSIGNED_INT},
                                    {GL_R16I, GL_INT},
                                    {GL_R16UI, GL_UNSIGNED_INT},
                                    {GL_R32I, GL_INT},
                                    {GL_R32UI, GL_UNSIGNED_INT},
                                    {GL_RG8I, GL_INT},
                                    {GL_RG8UI, GL_UNSIGNED_INT},
                                    {GL_RG16I, GL_INT},
                                    {GL_RG16UI, GL_UNSIGNED_INT},
                                    {GL_RG32I, GL_INT},
                                    {GL_RG32UI, GL_UNSIGNED_INT},
                                    {GL_RGBA8I, GL_INT},
                                    {GL_RGBA8UI, GL_UNSIGNED_INT},
                                    {GL_RGBA16I, GL_INT},
                                    {GL_RGBA16UI, GL_UNSIGNED_INT},
                                    {GL_RGBA32I, GL_INT},
                                    {GL_RGBA32UI, GL_UNSIGNED_INT}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(requiredColorformats); ++ndx)
        {
            const GLenum colorFormat = requiredColorformats[ndx].internalFormat;

            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexStorage2D(GL_TEXTURE_2D, 1, colorFormat, 128, 128);
            expectError(GL_NO_ERROR);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,
                                      requiredColorformats[ndx].componentType);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentSizeInitialCase : public ApiCase
{
public:
    AttachmentSizeInitialCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const GLenum colorAttachment =
            (glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 5)) ? GL_FRONT :
                                                                                                      GL_BACK);
        // check default
        if (attachmentExists(colorAttachment))
        {
            checkAttachmentComponentSizeAtLeast(m_testCtx, *this, GL_FRAMEBUFFER, colorAttachment,
                                                m_context.getRenderTarget().getPixelFormat().redBits,
                                                m_context.getRenderTarget().getPixelFormat().greenBits,
                                                m_context.getRenderTarget().getPixelFormat().blueBits,
                                                m_context.getRenderTarget().getPixelFormat().alphaBits, -1, -1);
            expectError(GL_NO_ERROR);
        }

        if (attachmentExists(GL_DEPTH))
        {
            checkAttachmentComponentSizeAtLeast(m_testCtx, *this, GL_FRAMEBUFFER, GL_DEPTH, -1, -1, -1, -1,
                                                m_context.getRenderTarget().getDepthBits(), -1);
            expectError(GL_NO_ERROR);
        }

        if (attachmentExists(GL_STENCIL))
        {
            checkAttachmentComponentSizeAtLeast(m_testCtx, *this, GL_FRAMEBUFFER, GL_STENCIL, -1, -1, -1, -1, -1,
                                                m_context.getRenderTarget().getStencilBits());
            expectError(GL_NO_ERROR);
        }
    }

    bool attachmentExists(GLenum attachment)
    {
        StateQueryMemoryWriteGuard<GLint> state;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                              &state);
        expectError(GL_NO_ERROR);

        return !state.isUndefined() && state != GL_NONE;
    }
};

class AttachmentSizeCase : public ApiCase
{
public:
    AttachmentSizeCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    virtual void testColorAttachment(GLenum internalFormat, GLenum attachment, GLint bitsR, GLint bitsG, GLint bitsB,
                                     GLint bitsA) = 0;

    virtual void testDepthAttachment(GLenum internalFormat, GLenum attachment, GLint bitsD, GLint bitsS) = 0;

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // check some color targets

        const struct ColorAttachment
        {
            GLenum internalFormat;
            int bitsR, bitsG, bitsB, bitsA;
        } colorAttachments[] = {{GL_RGBA8, 8, 8, 8, 8},   {GL_RGB565, 5, 6, 5, 0}, {GL_RGBA4, 4, 4, 4, 4},
                                {GL_RGB5_A1, 5, 5, 5, 1}, {GL_RGBA8I, 8, 8, 8, 8}, {GL_RG32UI, 32, 32, 0, 0}};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(colorAttachments); ++ndx)
            testColorAttachment(colorAttachments[ndx].internalFormat, GL_COLOR_ATTACHMENT0, colorAttachments[ndx].bitsR,
                                colorAttachments[ndx].bitsG, colorAttachments[ndx].bitsB, colorAttachments[ndx].bitsA);

        // check some depth targets

        const struct DepthAttachment
        {
            GLenum internalFormat;
            GLenum attachment;
            int dbits;
            int sbits;
        } depthAttachments[] = {
            {GL_DEPTH_COMPONENT16, GL_DEPTH_ATTACHMENT, 16, 0},
            {GL_DEPTH_COMPONENT24, GL_DEPTH_ATTACHMENT, 24, 0},
            {GL_DEPTH_COMPONENT32F, GL_DEPTH_ATTACHMENT, 32, 0},
            {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, 24, 8},
            {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, 32, 8},
        };
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthAttachments); ++ndx)
            testDepthAttachment(depthAttachments[ndx].internalFormat, depthAttachments[ndx].attachment,
                                depthAttachments[ndx].dbits, depthAttachments[ndx].sbits);

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentSizeRboCase : public AttachmentSizeCase
{
public:
    AttachmentSizeRboCase(Context &context, const char *name, const char *description)
        : AttachmentSizeCase(context, name, description)
    {
    }

    void testColorAttachment(GLenum internalFormat, GLenum attachment, GLint bitsR, GLint bitsG, GLint bitsB,
                             GLint bitsA)
    {
        GLuint renderbufferID = 0;
        glGenRenderbuffers(1, &renderbufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, 128, 128);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderbufferID);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeAtLeast(m_testCtx, *this, GL_FRAMEBUFFER, attachment, bitsR, bitsG, bitsB, bitsA,
                                            -1, -1);
        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, attachment, -1, -1, -1, -1, 0, 0);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, 0);
        glDeleteRenderbuffers(1, &renderbufferID);
    }

    void testDepthAttachment(GLenum internalFormat, GLenum attachment, GLint bitsD, GLint bitsS)
    {
        GLuint renderbufferID = 0;
        glGenRenderbuffers(1, &renderbufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, 128, 128);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderbufferID);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeAtLeast(m_testCtx, *this, GL_FRAMEBUFFER, attachment, -1, -1, -1, -1, bitsD, bitsS);
        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, attachment, 0, 0, 0, 0, -1, -1);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, 0);
        glDeleteRenderbuffers(1, &renderbufferID);
    }
};

class AttachmentSizeTextureCase : public AttachmentSizeCase
{
public:
    AttachmentSizeTextureCase(Context &context, const char *name, const char *description)
        : AttachmentSizeCase(context, name, description)
    {
    }

    void testColorAttachment(GLenum internalFormat, GLenum attachment, GLint bitsR, GLint bitsG, GLint bitsB,
                             GLint bitsA)
    {
        GLuint textureID = 0;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, 128, 128);
        expectError(GL_NO_ERROR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, textureID, 0);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeAtLeast(m_testCtx, *this, GL_FRAMEBUFFER, attachment, bitsR, bitsG, bitsB, bitsA,
                                            -1, -1);
        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, attachment, -1, -1, -1, -1, 0, 0);

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
        glDeleteTextures(1, &textureID);
    }

    void testDepthAttachment(GLenum internalFormat, GLenum attachment, GLint bitsD, GLint bitsS)
    {
        // don't test stencil formats with textures
        if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
            return;

        GLuint textureID = 0;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, 128, 128);
        expectError(GL_NO_ERROR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, textureID, 0);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeAtLeast(m_testCtx, *this, GL_FRAMEBUFFER, attachment, -1, -1, -1, -1, bitsD, bitsS);
        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, attachment, 0, 0, 0, 0, -1, -1);

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
        glDeleteTextures(1, &textureID);
    }
};

class UnspecifiedAttachmentTextureColorCodingCase : public ApiCase
{
public:
    UnspecifiedAttachmentTextureColorCodingCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // color
        {
            GLuint renderbufferID = 0;
            glGenRenderbuffers(1, &renderbufferID);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
                                      GL_LINEAR);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &renderbufferID);
        }

        // depth
        {
            GLuint renderbufferID = 0;
            glGenRenderbuffers(1, &renderbufferID);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            checkAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                 GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, GL_LINEAR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &renderbufferID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class UnspecifiedAttachmentTextureComponentTypeCase : public ApiCase
{
public:
    UnspecifiedAttachmentTextureComponentTypeCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            expectError(GL_NO_ERROR);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,
                                      GL_NONE);
            expectError(GL_NO_ERROR);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class UnspecifiedAttachmentSizeCase : public ApiCase
{
public:
    UnspecifiedAttachmentSizeCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    virtual void testColorAttachment(void) = 0;

    virtual void testDepthAttachment(void) = 0;

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // check color target
        testColorAttachment();

        // check depth target
        testDepthAttachment();

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class UnspecifiedAttachmentSizeRboCase : public UnspecifiedAttachmentSizeCase
{
public:
    UnspecifiedAttachmentSizeRboCase(Context &context, const char *name, const char *description)
        : UnspecifiedAttachmentSizeCase(context, name, description)
    {
    }

    void testColorAttachment(void)
    {
        GLuint renderbufferID = 0;
        glGenRenderbuffers(1, &renderbufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbufferID);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
        glDeleteRenderbuffers(1, &renderbufferID);
    }

    void testDepthAttachment(void)
    {
        GLuint renderbufferID = 0;
        glGenRenderbuffers(1, &renderbufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbufferID);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        glDeleteRenderbuffers(1, &renderbufferID);
    }
};

class UnspecifiedAttachmentSizeTextureCase : public UnspecifiedAttachmentSizeCase
{
public:
    UnspecifiedAttachmentSizeTextureCase(Context &context, const char *name, const char *description)
        : UnspecifiedAttachmentSizeCase(context, name, description)
    {
    }

    void testColorAttachment(void)
    {
        GLuint textureID = 0;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        expectError(GL_NO_ERROR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glDeleteTextures(1, &textureID);
    }

    void testDepthAttachment(void)
    {
        GLuint textureID = 0;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        expectError(GL_NO_ERROR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textureID, 0);
        expectError(GL_NO_ERROR);

        checkAttachmentComponentSizeExactly(m_testCtx, *this, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        glDeleteTextures(1, &textureID);
    }
};

} // namespace

FboStateQueryTests::FboStateQueryTests(Context &context) : TestCaseGroup(context, "fbo", "Fbo State Query tests")
{
}

void FboStateQueryTests::init(void)
{
    const struct FramebufferTarget
    {
        const char *name;
        GLenum target;
    } fboTargets[] = {{"draw_framebuffer_", GL_DRAW_FRAMEBUFFER}, {"read_framebuffer_", GL_READ_FRAMEBUFFER}};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(fboTargets); ++ndx)
        addChild(new DefaultFramebufferCase(m_context,
                                            (std::string(fboTargets[ndx].name) + "default_framebuffer").c_str(),
                                            "default framebuffer", fboTargets[ndx].target));

    addChild(new AttachmentObjectCase(m_context, "framebuffer_attachment_object",
                                      "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE and FRAMEBUFFER_ATTACHMENT_OBJECT_NAME"));
    addChild(new AttachmentTextureLevelCase(m_context, "framebuffer_attachment_texture_level",
                                            "FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL"));
    addChild(new AttachmentTextureCubeMapFaceCase(m_context, "framebuffer_attachment_texture_cube_map_face",
                                                  "FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE"));
    addChild(new AttachmentTextureLayerCase(m_context, "framebuffer_attachment_texture_layer",
                                            "FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER"));
    addChild(new AttachmentTextureColorCodingCase(m_context, "framebuffer_attachment_color_encoding",
                                                  "FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING"));
    addChild(new AttachmentTextureComponentTypeCase(m_context, "framebuffer_attachment_component_type",
                                                    "FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"));
    addChild(new AttachmentSizeInitialCase(m_context, "framebuffer_attachment_x_size_initial",
                                           "FRAMEBUFFER_ATTACHMENT_x_SIZE"));
    addChild(
        new AttachmentSizeRboCase(m_context, "framebuffer_attachment_x_size_rbo", "FRAMEBUFFER_ATTACHMENT_x_SIZE"));
    addChild(new AttachmentSizeTextureCase(m_context, "framebuffer_attachment_x_size_texture",
                                           "FRAMEBUFFER_ATTACHMENT_x_SIZE"));

    addChild(new UnspecifiedAttachmentTextureColorCodingCase(
        m_context, "framebuffer_unspecified_attachment_color_encoding", "FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING"));
    addChild(new UnspecifiedAttachmentTextureComponentTypeCase(
        m_context, "framebuffer_unspecified_attachment_component_type", "FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"));
    addChild(new UnspecifiedAttachmentSizeRboCase(m_context, "framebuffer_unspecified_attachment_x_size_rbo",
                                                  "FRAMEBUFFER_ATTACHMENT_x_SIZE"));
    addChild(new UnspecifiedAttachmentSizeTextureCase(m_context, "framebuffer_unspecified_attachment_x_size_texture",
                                                      "FRAMEBUFFER_ATTACHMENT_x_SIZE"));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
