//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BlitGL.cpp: Implements the BlitGL class, a helper for blitting textures

#include "libANGLE/renderer/gl/BlitGL.h"

#include "libANGLE/formatutils.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/renderer/gl/formatutilsgl.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"

namespace
{

gl::Error CheckCompileStatus(const rx::FunctionsGL *functions, GLuint shader)
{
    GLint compileStatus = GL_FALSE;
    functions->getShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    ASSERT(compileStatus == GL_TRUE);
    if (compileStatus == GL_FALSE)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to compile internal blit shader.");
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error CheckLinkStatus(const rx::FunctionsGL *functions, GLuint program)
{
    GLint linkStatus = GL_FALSE;
    functions->getProgramiv(program, GL_LINK_STATUS, &linkStatus);
    ASSERT(linkStatus == GL_TRUE);
    if (linkStatus == GL_FALSE)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to link internal blit program.");
    }

    return gl::Error(GL_NO_ERROR);
}

} // anonymous namespace

namespace rx
{

BlitGL::BlitGL(const FunctionsGL *functions,
               const WorkaroundsGL &workarounds,
               StateManagerGL *stateManager)
    : mFunctions(functions),
      mWorkarounds(workarounds),
      mStateManager(stateManager),
      mBlitProgram(0),
      mScratchFBO(0),
      mVAO(0)
{
    for (size_t i = 0; i < ArraySize(mScratchTextures); i++)
    {
        mScratchTextures[i] = 0;
    }

    ASSERT(mFunctions);
    ASSERT(mStateManager);
}

BlitGL::~BlitGL()
{
    if (mBlitProgram != 0)
    {
        mStateManager->deleteProgram(mBlitProgram);
        mBlitProgram = 0;
    }

    for (size_t i = 0; i < ArraySize(mScratchTextures); i++)
    {
        if (mScratchTextures[i] != 0)
        {
            mStateManager->deleteTexture(mScratchTextures[i]);
            mScratchTextures[i] = 0;
        }
    }

    if (mScratchFBO != 0)
    {
        mStateManager->deleteFramebuffer(mScratchFBO);
        mScratchFBO = 0;
    }

    if (mVAO != 0)
    {
        mStateManager->deleteVertexArray(mVAO);
        mVAO = 0;
    }
}

gl::Error BlitGL::copyImageToLUMAWorkaroundTexture(GLuint texture,
                                                   GLenum textureType,
                                                   GLenum target,
                                                   GLenum lumaFormat,
                                                   size_t level,
                                                   const gl::Rectangle &sourceArea,
                                                   GLenum internalFormat,
                                                   const gl::Framebuffer *source)
{
    mStateManager->bindTexture(textureType, texture);

    // Allocate the texture memory
    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat);
    mFunctions->texImage2D(target, static_cast<GLint>(level), internalFormat, sourceArea.width,
                           sourceArea.height, 0, internalFormatInfo.format,
                           source->getImplementationColorReadType(), nullptr);

    return copySubImageToLUMAWorkaroundTexture(texture, textureType, target, lumaFormat, level,
                                               gl::Offset(0, 0, 0), sourceArea, source);
}

gl::Error BlitGL::copySubImageToLUMAWorkaroundTexture(GLuint texture,
                                                      GLenum textureType,
                                                      GLenum target,
                                                      GLenum lumaFormat,
                                                      size_t level,
                                                      const gl::Offset &destOffset,
                                                      const gl::Rectangle &sourceArea,
                                                      const gl::Framebuffer *source)
{
    gl::Error error = initializeResources();
    if (error.isError())
    {
        return error;
    }

    // Blit the framebuffer to the first scratch texture
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());

    nativegl::CopyTexImageImageFormat copyTexImageFormat = nativegl::GetCopyTexImageImageFormat(
        mFunctions, mWorkarounds, source->getImplementationColorReadFormat(),
        source->getImplementationColorReadType());
    const gl::InternalFormat &internalFormatInfo =
        gl::GetInternalFormatInfo(copyTexImageFormat.internalFormat);

    mStateManager->activeTexture(0);
    mStateManager->bindTexture(GL_TEXTURE_2D, mScratchTextures[0]);
    mFunctions->copyTexImage2D(GL_TEXTURE_2D, 0, copyTexImageFormat.internalFormat, sourceArea.x,
                               sourceArea.y, sourceArea.width, sourceArea.height, 0);

    // Set the swizzle of the scratch texture so that the channels sample into the correct emulated
    // LUMA channels.
    GLint swizzle[4] = {
        (lumaFormat == GL_ALPHA) ? GL_ALPHA : GL_RED,
        (lumaFormat == GL_LUMINANCE_ALPHA) ? GL_ALPHA : GL_ZERO, GL_ZERO, GL_ZERO,
    };
    mFunctions->texParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    // Make a temporary framebuffer using the second scratch texture to render the swizzled result
    // to.
    mStateManager->bindTexture(GL_TEXTURE_2D, mScratchTextures[1]);
    mFunctions->texImage2D(GL_TEXTURE_2D, 0, copyTexImageFormat.internalFormat, sourceArea.width,
                           sourceArea.height, 0, internalFormatInfo.format,
                           source->getImplementationColorReadType(), nullptr);

    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mScratchFBO);
    mFunctions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                     mScratchTextures[1], 0);

    // Render to the destination texture, sampling from the scratch texture
    mStateManager->useProgram(mBlitProgram);
    mStateManager->setViewport(gl::Rectangle(0, 0, sourceArea.width, sourceArea.height));
    mStateManager->setScissorTestEnabled(false);
    mStateManager->setDepthRange(0.0f, 1.0f);
    mStateManager->setBlendEnabled(false);
    mStateManager->setColorMask(true, true, true, true);
    mStateManager->setSampleAlphaToCoverageEnabled(false);
    mStateManager->setSampleCoverageEnabled(false);
    mStateManager->setDepthTestEnabled(false);
    mStateManager->setStencilTestEnabled(false);
    mStateManager->setCullFaceEnabled(false);
    mStateManager->setPolygonOffsetFillEnabled(false);
    mStateManager->setRasterizerDiscardEnabled(false);
    mStateManager->bindTexture(GL_TEXTURE_2D, mScratchTextures[0]);

    mStateManager->bindVertexArray(mVAO, 0);

    mFunctions->drawArrays(GL_TRIANGLES, 0, 6);

    // Finally, copy the swizzled texture to the destination texture
    mStateManager->bindTexture(textureType, texture);
    mFunctions->copyTexSubImage2D(target, static_cast<GLint>(level), destOffset.x, destOffset.y, 0,
                                  0, sourceArea.width, sourceArea.height);

    return gl::Error(GL_NO_ERROR);
}

gl::Error BlitGL::initializeResources()
{
    if (mBlitProgram == 0)
    {
        mBlitProgram = mFunctions->createProgram();

        // Compile the fragment shader
        const char *vsSource =
            "#version 150\n"
            "out vec2 v_texcoord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    const vec2 quad_positions[6] = vec2[6]\n"
            "    (\n"
            "        vec2(0.0f, 0.0f),\n"
            "        vec2(0.0f, 1.0f),\n"
            "        vec2(1.0f, 0.0f),\n"
            "\n"
            "        vec2(0.0f, 1.0f),\n"
            "        vec2(1.0f, 0.0f),\n"
            "        vec2(1.0f, 1.0f)\n"
            "    );\n"
            "\n"
            "    gl_Position = vec4((quad_positions[gl_VertexID] * 2.0) - 1.0, 0.0, 1.0);\n"
            "    v_texcoord = quad_positions[gl_VertexID];\n"
            "}\n";

        GLuint vs = mFunctions->createShader(GL_VERTEX_SHADER);
        mFunctions->shaderSource(vs, 1, &vsSource, nullptr);
        mFunctions->compileShader(vs);
        gl::Error error = CheckCompileStatus(mFunctions, vs);

        mFunctions->attachShader(mBlitProgram, vs);
        mFunctions->deleteShader(vs);

        if (error.isError())
        {
            return error;
        }

        // Compile the vertex shader
        const char *fsSource =
            "#version 150\n"
            "uniform sampler2D u_source_texture;\n"
            "in vec2 v_texcoord;\n"
            "out vec4 output_color;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    output_color = texture(u_source_texture, v_texcoord);\n"
            "}\n";

        GLuint fs = mFunctions->createShader(GL_FRAGMENT_SHADER);
        mFunctions->shaderSource(fs, 1, &fsSource, nullptr);
        mFunctions->compileShader(fs);
        error = CheckCompileStatus(mFunctions, fs);

        mFunctions->attachShader(mBlitProgram, fs);
        mFunctions->deleteShader(fs);

        if (error.isError())
        {
            return error;
        }

        mFunctions->linkProgram(mBlitProgram);
        error = CheckLinkStatus(mFunctions, mBlitProgram);
        if (error.isError())
        {
            return error;
        }

        GLuint textureUniform = mFunctions->getUniformLocation(mBlitProgram, "u_source_texture");
        mStateManager->useProgram(mBlitProgram);
        mFunctions->uniform1i(textureUniform, 0);
    }

    for (size_t i = 0; i < ArraySize(mScratchTextures); i++)
    {
        if (mScratchTextures[i] == 0)
        {
            mFunctions->genTextures(1, &mScratchTextures[i]);
            mStateManager->bindTexture(GL_TEXTURE_2D, mScratchTextures[i]);

            // Use nearest, non-mipmapped sampling with the scratch texture
            mFunctions->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            mFunctions->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }

    if (mScratchFBO == 0)
    {
        mFunctions->genFramebuffers(1, &mScratchFBO);
    }

    if (mVAO == 0)
    {
        mFunctions->genVertexArrays(1, &mVAO);
    }

    return gl::Error(GL_NO_ERROR);
}
}
