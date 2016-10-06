/*
 Copyright (C) 2016 Igalia S.L.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */


#include "config.h"
#include "VideoTextureCopierGStreamer.h"

#if USE(GSTREAMER_GL)

#include "GLContext.h"
#include "ImageOrientation.h"
#include "TextureMapperShaderProgram.h"

namespace WebCore {

VideoTextureCopierGStreamer::VideoTextureCopierGStreamer()
{
    GLContext* previousContext = GLContext::current();
    ASSERT(previousContext);
    PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->makeContextCurrent();

    m_context3D = GraphicsContext3D::createForCurrentGLContext();

    m_shaderProgram = TextureMapperShaderProgram::create(*m_context3D, TextureMapperShaderProgram::Texture);

    m_framebuffer = m_context3D->createFramebuffer();

    static const GLfloat vertices[] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    m_vbo = m_context3D->createBuffer();
    m_context3D->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vbo);
    m_context3D->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(GC3Dfloat) * 8, vertices, GraphicsContext3D::STATIC_DRAW);

    updateTextureSpaceMatrix();

    previousContext->makeContextCurrent();
}

VideoTextureCopierGStreamer::~VideoTextureCopierGStreamer()
{
    GLContext* previousContext = GLContext::current();
    ASSERT(previousContext);
    PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->makeContextCurrent();

    m_context3D->deleteFramebuffer(m_framebuffer);
    m_context3D->deleteBuffer(m_vbo);
    m_shaderProgram = nullptr;
    m_context3D = nullptr;

    previousContext->makeContextCurrent();
}

void VideoTextureCopierGStreamer::updateTextureSpaceMatrix()
{
    m_textureSpaceMatrix.makeIdentity();

    switch (m_orientation) {
    case OriginRightTop:
        m_textureSpaceMatrix.rotate(-90);
        m_textureSpaceMatrix.translate(-1, 0);
        break;
    case OriginBottomRight:
        m_textureSpaceMatrix.rotate(180);
        m_textureSpaceMatrix.translate(-1, -1);
        break;
    case OriginLeftBottom:
        m_textureSpaceMatrix.rotate(-270);
        m_textureSpaceMatrix.translate(0, -1);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (!m_flipY) {
        m_textureSpaceMatrix.flipY();
        m_textureSpaceMatrix.translate(0, -1);
    }
}

void VideoTextureCopierGStreamer::updateTransformationMatrix()
{
    FloatRect targetRect = FloatRect(FloatPoint(), m_size);
    TransformationMatrix identityMatrix;
    m_modelViewMatrix = TransformationMatrix(identityMatrix).multiply(TransformationMatrix::rectToRect(FloatRect(0, 0, 1, 1), targetRect));

    // Taken from TextureMapperGL.
    const float nearValue = 9999999;
    const float farValue = -99999;

    m_projectionMatrix = TransformationMatrix(2.0 / float(m_size.width()), 0, 0, 0,
        0, (-2.0) / float(m_size.height()), 0, 0,
        0, 0, -2.f / (farValue - nearValue), 0,
        -1, 1, -(farValue + nearValue) / (farValue - nearValue), 1);
}

bool VideoTextureCopierGStreamer::copyVideoTextureToPlatformTexture(Platform3DObject inputTexture, IntSize& frameSize, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool flipY, ImageOrientation& sourceOrientation)
{
    if (!m_shaderProgram || !m_framebuffer || !m_vbo || frameSize.isEmpty())
        return false;

    if (m_size != frameSize) {
        m_size = frameSize;
        updateTransformationMatrix();
    }

    if (m_flipY != flipY || m_orientation != sourceOrientation) {
        m_flipY = flipY;
        m_orientation = sourceOrientation;
        updateTextureSpaceMatrix();
    }

    // Save previous context and activate the sharing one.
    GLContext* previousContext = GLContext::current();
    ASSERT(previousContext);
    PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->makeContextCurrent();

    // Save previous bound framebuffer, texture and viewport.
    GC3Dint boundFramebuffer = 0;
    GC3Dint boundTexture = 0;
    GC3Dint previousViewport[4] = { 0, 0, 0, 0};
    m_context3D->getIntegerv(GraphicsContext3D::FRAMEBUFFER_BINDING, &boundFramebuffer);
    m_context3D->getIntegerv(GraphicsContext3D::TEXTURE_BINDING_2D, &boundTexture);
    m_context3D->getIntegerv(GraphicsContext3D::VIEWPORT, previousViewport);

    // Set proper parameters to the output texture and allocate uninitialized memory for it.
    m_context3D->bindTexture(outputTarget, outputTexture);
    m_context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context3D->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context3D->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context3D->texImage2DDirect(outputTarget, level, internalFormat, m_size.width(), m_size.height(), 0, format, type, nullptr);

    // Bind framebuffer to paint and attach the destination texture to it.
    m_context3D->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_framebuffer);
    m_context3D->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);

    // Set proper wrap parameter to the source texture.
    m_context3D->bindTexture(GL_TEXTURE_2D, inputTexture);
    m_context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context3D->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context3D->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);

    // Set the viewport.
    m_context3D->viewport(0, 0, m_size.width(), m_size.height());

    // Set program parameters.
    m_context3D->useProgram(m_shaderProgram->programID());
    m_context3D->uniform1i(m_shaderProgram->samplerLocation(), 0);
    m_shaderProgram->setMatrix(m_shaderProgram->modelViewMatrixLocation(), m_modelViewMatrix);
    m_shaderProgram->setMatrix(m_shaderProgram->projectionMatrixLocation(), m_projectionMatrix);
    m_shaderProgram->setMatrix(m_shaderProgram->textureSpaceMatrixLocation(), m_textureSpaceMatrix);

    // Perform the copy.
    m_context3D->enableVertexAttribArray(m_shaderProgram->vertexLocation());
    m_context3D->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vbo);
    m_context3D->vertexAttribPointer(m_shaderProgram->vertexLocation(), 2, GraphicsContext3D::FLOAT, false, 0, 0);
    m_context3D->drawArrays(GraphicsContext3D::TRIANGLE_FAN, 0, 4);
    m_context3D->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, 0);
    m_context3D->disableVertexAttribArray(m_shaderProgram->vertexLocation());
    m_context3D->useProgram(0);

    // Restore previous bindings and viewport.
    m_context3D->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, boundFramebuffer);
    m_context3D->bindTexture(outputTarget, boundTexture);
    m_context3D->viewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    bool ok = (m_context3D->getError() == GraphicsContext3D::NO_ERROR);

    // Restore previous context.
    previousContext->makeContextCurrent();
    return ok;
}

} // namespace WebCore

#endif // USE(GSTREAMER_GL)
