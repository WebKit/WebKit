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

    return gl::NoError();
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

    return gl::NoError();
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
      mSourceTextureLocation(-1),
      mScaleLocation(-1),
      mOffsetLocation(-1),
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
    ANGLE_TRY(initializeResources());

    // Blit the framebuffer to the first scratch texture
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());

    nativegl::CopyTexImageImageFormat copyTexImageFormat = nativegl::GetCopyTexImageImageFormat(
        mFunctions, mWorkarounds, source->getImplementationColorReadFormat(),
        source->getImplementationColorReadType());
    const gl::InternalFormat &internalFormatInfo =
        gl::GetInternalFormatInfo(copyTexImageFormat.internalFormat);

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

    setScratchTextureParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    setScratchTextureParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    mStateManager->activeTexture(0);
    mStateManager->bindTexture(GL_TEXTURE_2D, mScratchTextures[0]);

    mStateManager->useProgram(mBlitProgram);
    mFunctions->uniform1i(mSourceTextureLocation, 0);
    mFunctions->uniform2f(mScaleLocation, 1.0, 1.0);
    mFunctions->uniform2f(mOffsetLocation, 0.0, 0.0);

    mStateManager->bindVertexArray(mVAO, 0);

    mFunctions->drawArrays(GL_TRIANGLES, 0, 3);

    // Copy the swizzled texture to the destination texture
    mStateManager->bindTexture(textureType, texture);
    mFunctions->copyTexSubImage2D(target, static_cast<GLint>(level), destOffset.x, destOffset.y, 0,
                                  0, sourceArea.width, sourceArea.height);

    // Finally orphan the scratch textures so they can be GCed by the driver.
    orphanScratchTextures();

    return gl::NoError();
}

gl::Error BlitGL::blitColorBufferWithShader(const gl::Framebuffer *source,
                                            const gl::Framebuffer *dest,
                                            const gl::Rectangle &sourceAreaIn,
                                            const gl::Rectangle &destAreaIn,
                                            GLenum filter)
{
    ANGLE_TRY(initializeResources());

    // Normalize the destination area to have positive width and height because we will use
    // glViewport to set it, which doesn't allow negative width or height.
    gl::Rectangle sourceArea = sourceAreaIn;
    gl::Rectangle destArea   = destAreaIn;
    if (destArea.width < 0)
    {
        destArea.x += destArea.width;
        destArea.width = -destArea.width;
        sourceArea.x += sourceArea.width;
        sourceArea.width = -sourceArea.width;
    }
    if (destArea.height < 0)
    {
        destArea.y += destArea.height;
        destArea.height = -destArea.height;
        sourceArea.y += sourceArea.height;
        sourceArea.height = -sourceArea.height;
    }

    const gl::FramebufferAttachment *readAttachment = source->getReadColorbuffer();
    ASSERT(readAttachment->getSamples() <= 1);

    // Compute the part of the source that will be sampled.
    gl::Rectangle inBoundsSource;
    {
        gl::Extents sourceSize = readAttachment->getSize();
        gl::Rectangle sourceBounds(0, 0, sourceSize.width, sourceSize.height);
        gl::ClipRectangle(sourceArea, sourceBounds, &inBoundsSource);

        // Note that inBoundsSource will have lost the orientation information.
        ASSERT(inBoundsSource.width >= 0 && inBoundsSource.height >= 0);

        // Early out when the sampled part is empty as the blit will be a noop,
        // and it prevents a division by zero in later computations.
        if (inBoundsSource.width == 0 || inBoundsSource.height == 0)
        {
            return gl::NoError();
        }
    }

    // The blit will be emulated by getting the source of the blit in a texture and sampling it
    // with CLAMP_TO_EDGE. The quad used to draw can trivially compute texture coordinates going
    // from (0, 0) to (1, 1). These texture coordinates will need to be transformed to make two
    // regions match:
    //  - The region of the texture representing the source framebuffer region that will be sampled
    //  - The region of the drawn quad that corresponds to non-clamped blit, this is the same as the
    //    region of the source rectangle that is inside the source attachment.
    //
    //  These two regions, T (texture) and D (dest) are defined by their offset in texcoord space
    //  in (0, 1)^2 and their size in texcoord space in (-1, 1)^2. The size can be negative to
    //  represent the orientation of the blit.
    //
    //  Then if P is the quad texcoord, Q the texcoord inside T, and R the texture texcoord:
    //    - Q = (P - D.offset) / D.size
    //    - Q = (R - T.offset) / T.size
    //  Hence R = (P - D.offset) / D.size * T.size - T.offset
    //          = P * (T.size / D.size) + (T.offset - D.offset * T.size / D.size)

    GLuint textureId;
    gl::Vector2 TOffset;
    gl::Vector2 TSize;

    // TODO(cwallez) once texture dirty bits are landed, reuse attached texture instead of using
    // CopyTexImage2D
    {
        textureId = mScratchTextures[0];
        TOffset   = gl::Vector2(0.0, 0.0);
        TSize     = gl::Vector2(1.0, 1.0);
        if (sourceArea.width < 0)
        {
            TOffset.x = 1.0;
            TSize.x   = -1.0;
        }
        if (sourceArea.height < 0)
        {
            TOffset.y = 1.0;
            TSize.y   = -1.0;
        }

        GLenum format                 = readAttachment->getFormat().info->internalFormat;
        const FramebufferGL *sourceGL = GetImplAs<FramebufferGL>(source);
        mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceGL->getFramebufferID());
        mStateManager->bindTexture(GL_TEXTURE_2D, textureId);

        mFunctions->copyTexImage2D(GL_TEXTURE_2D, 0, format, inBoundsSource.x, inBoundsSource.y,
                                   inBoundsSource.width, inBoundsSource.height, 0);
    }

    // Compute normalized sampled draw quad region
    // It is the same as the region of the source rectangle that is in bounds.
    gl::Vector2 DOffset;
    gl::Vector2 DSize;
    {
        ASSERT(sourceArea.width != 0 && sourceArea.height != 0);
        gl::Rectangle orientedInBounds = inBoundsSource;
        if (sourceArea.width < 0)
        {
            orientedInBounds.x += orientedInBounds.width;
            orientedInBounds.width = -orientedInBounds.width;
        }
        if (sourceArea.height < 0)
        {
            orientedInBounds.y += orientedInBounds.height;
            orientedInBounds.height = -orientedInBounds.height;
        }

        DOffset =
            gl::Vector2(static_cast<float>(orientedInBounds.x - sourceArea.x) / sourceArea.width,
                        static_cast<float>(orientedInBounds.y - sourceArea.y) / sourceArea.height);
        DSize = gl::Vector2(static_cast<float>(orientedInBounds.width) / sourceArea.width,
                            static_cast<float>(orientedInBounds.height) / sourceArea.height);
    }

    ASSERT(DSize.x != 0.0 && DSize.y != 0.0);
    gl::Vector2 texCoordScale  = gl::Vector2(TSize.x / DSize.x, TSize.y / DSize.y);
    gl::Vector2 texCoordOffset = gl::Vector2(TOffset.x - DOffset.x * texCoordScale.x,
                                             TOffset.y - DOffset.y * texCoordScale.y);

    // Reset all the state except scissor and viewport
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

    // Use the viewport to draw exactly to the destination rectangle
    mStateManager->setViewport(destArea);

    // Set uniforms
    setScratchTextureParameter(GL_TEXTURE_MIN_FILTER, filter);
    setScratchTextureParameter(GL_TEXTURE_MAG_FILTER, filter);
    setScratchTextureParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    setScratchTextureParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mStateManager->activeTexture(0);
    mStateManager->bindTexture(GL_TEXTURE_2D, mScratchTextures[0]);

    mStateManager->useProgram(mBlitProgram);
    mFunctions->uniform1i(mSourceTextureLocation, 0);
    mFunctions->uniform2f(mScaleLocation, texCoordScale.x, texCoordScale.y);
    mFunctions->uniform2f(mOffsetLocation, texCoordOffset.x, texCoordOffset.y);

    const FramebufferGL *destGL = GetImplAs<FramebufferGL>(dest);
    mStateManager->bindFramebuffer(GL_DRAW_FRAMEBUFFER, destGL->getFramebufferID());

    mStateManager->bindVertexArray(mVAO, 0);
    mFunctions->drawArrays(GL_TRIANGLES, 0, 3);

    return gl::NoError();
}

gl::Error BlitGL::initializeResources()
{
    if (mBlitProgram == 0)
    {
        mBlitProgram = mFunctions->createProgram();

        // Compile the fragment shader
        // It uses a single, large triangle, to avoid arithmetic precision issues where fragments
        // with the same Y coordinate don't get exactly the same interpolated texcoord Y.
        const char *vsSource =
            "#version 150\n"
            "out vec2 v_texcoord;\n"
            "uniform vec2 u_scale;\n"
            "uniform vec2 u_offset;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    const vec2 quad_positions[3] = vec2[3]\n"
            "    (\n"
            "        vec2(-0.5f, 0.0f),\n"
            "        vec2( 1.5f, 0.0f),\n"
            "        vec2( 0.5f, 2.0f)\n"
            "    );\n"
            "\n"
            "    gl_Position = vec4((quad_positions[gl_VertexID] * 2.0) - 1.0, 0.0, 1.0);\n"
            "    v_texcoord = quad_positions[gl_VertexID] * u_scale + u_offset;\n"
            "}\n";

        GLuint vs = mFunctions->createShader(GL_VERTEX_SHADER);
        mFunctions->shaderSource(vs, 1, &vsSource, nullptr);
        mFunctions->compileShader(vs);
        ANGLE_TRY(CheckCompileStatus(mFunctions, vs));

        mFunctions->attachShader(mBlitProgram, vs);
        mFunctions->deleteShader(vs);

        // Compile the vertex shader
        // It discards if the texcoord is outside (0, 1)^2 so the blitframebuffer workaround
        // doesn't write when the point sampled is outside of the source framebuffer.
        const char *fsSource =
            "#version 150\n"
            "uniform sampler2D u_source_texture;\n"
            "in vec2 v_texcoord;\n"
            "out vec4 output_color;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    if (clamp(v_texcoord, vec2(0.0), vec2(1.0)) != v_texcoord)\n"
            "    {\n"
            "        discard;\n"
            "    }\n"
            "    output_color = texture(u_source_texture, v_texcoord);\n"
            "}\n";

        GLuint fs = mFunctions->createShader(GL_FRAGMENT_SHADER);
        mFunctions->shaderSource(fs, 1, &fsSource, nullptr);
        mFunctions->compileShader(fs);
        ANGLE_TRY(CheckCompileStatus(mFunctions, fs));

        mFunctions->attachShader(mBlitProgram, fs);
        mFunctions->deleteShader(fs);

        mFunctions->linkProgram(mBlitProgram);
        ANGLE_TRY(CheckLinkStatus(mFunctions, mBlitProgram));

        mSourceTextureLocation = mFunctions->getUniformLocation(mBlitProgram, "u_source_texture");
        mScaleLocation         = mFunctions->getUniformLocation(mBlitProgram, "u_scale");
        mOffsetLocation        = mFunctions->getUniformLocation(mBlitProgram, "u_offset");
        mStateManager->useProgram(mBlitProgram);
    }

    for (size_t i = 0; i < ArraySize(mScratchTextures); i++)
    {
        if (mScratchTextures[i] == 0)
        {
            mFunctions->genTextures(1, &mScratchTextures[i]);
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

    return gl::NoError();
}

void BlitGL::orphanScratchTextures()
{
    for (auto texture : mScratchTextures)
    {
        mStateManager->bindTexture(GL_TEXTURE_2D, texture);
        mFunctions->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               nullptr);
    }
}

void BlitGL::setScratchTextureParameter(GLenum param, GLenum value)
{
    for (auto texture : mScratchTextures)
    {
        mStateManager->bindTexture(GL_TEXTURE_2D, texture);
        mFunctions->texParameteri(GL_TEXTURE_2D, param, value);
        mFunctions->texParameteri(GL_TEXTURE_2D, param, value);
    }
}

}  // namespace rx
