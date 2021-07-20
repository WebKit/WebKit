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

#include "FloatRect.h"
#include "GLContext.h"
#include "ImageOrientation.h"
#include "TextureMapperShaderProgram.h"

namespace WebCore {

VideoTextureCopierGStreamer::VideoTextureCopierGStreamer(ColorConversion colorConversion)
{
    GLContext* previousContext = GLContext::current();
    ASSERT(previousContext);
    PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->makeContextCurrent();

    glGenFramebuffers(1, &m_framebuffer);
    glGenTextures(1, &m_resultTexture);

#if !USE(OPENGL_ES)
    // For OpenGL > 3.2 we need to have a VAO.
    if (GLContext::current()->version() >= 320)
        glGenVertexArrays(1, &m_vao);
#endif

    static const GLfloat vertices[] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, vertices, GL_STATIC_DRAW);

    updateColorConversionMatrix(colorConversion);
    updateTextureSpaceMatrix();

    previousContext->makeContextCurrent();
}

VideoTextureCopierGStreamer::~VideoTextureCopierGStreamer()
{
    GLContext* previousContext = GLContext::current();
    PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->makeContextCurrent();

    glDeleteFramebuffers(1, &m_framebuffer);
    glDeleteBuffers(1, &m_vbo);
    glDeleteTextures(1, &m_resultTexture);
#if !USE(OPENGL_ES)
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
#endif
    m_shaderProgram = nullptr;

    if (previousContext)
        previousContext->makeContextCurrent();
}

void VideoTextureCopierGStreamer::updateColorConversionMatrix(ColorConversion colorConversion)
{
    switch (colorConversion) {
    case ColorConversion::ConvertBGRAToRGBA:
        m_colorConversionMatrix.setMatrix(0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
        break;
    case ColorConversion::ConvertARGBToRGBA:
        m_colorConversionMatrix.setMatrix(0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0);
        break;
    case ColorConversion::NoConvert:
        m_colorConversionMatrix.makeIdentity();
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void VideoTextureCopierGStreamer::updateTextureSpaceMatrix()
{
    m_textureSpaceMatrix.makeIdentity();

    switch (m_orientation) {
    case ImageOrientation::OriginTopLeft:
        break;
    case ImageOrientation::OriginRightTop:
        m_textureSpaceMatrix.rotate(-90);
        m_textureSpaceMatrix.translate(-1, 0);
        break;
    case ImageOrientation::OriginBottomRight:
        m_textureSpaceMatrix.rotate(180);
        m_textureSpaceMatrix.translate(-1, -1);
        break;
    case ImageOrientation::OriginLeftBottom:
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

bool VideoTextureCopierGStreamer::copyVideoTextureToPlatformTexture(TextureMapperPlatformLayerBuffer& inputTexture, IntSize& frameSize, GLuint outputTexture, GLenum outputTarget, GLint level, GLenum internalFormat, GLenum format, GLenum type, bool flipY, ImageOrientation sourceOrientation, bool premultiplyAlpha)
{
    if (!m_framebuffer || !m_vbo || frameSize.isEmpty())
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

    // Determine what shader program to use and create it if necessary.
    using Buffer = TextureMapperPlatformLayerBuffer;
    TextureMapperShaderProgram::Options options;
    WTF::switchOn(inputTexture.textureVariant(),
        [&](const Buffer::RGBTexture&) { options = TextureMapperShaderProgram::TextureRGB | (premultiplyAlpha ? TextureMapperShaderProgram::Premultiply : 0); },
        [&](const Buffer::YUVTexture& texture) {
            switch (texture.numberOfPlanes) {
            case 1:
                ASSERT(texture.yuvPlane[0] == texture.yuvPlane[1] && texture.yuvPlane[1] == texture.yuvPlane[2]);
                ASSERT(texture.yuvPlaneOffset[0] == 2 && texture.yuvPlaneOffset[1] == 1 && !texture.yuvPlaneOffset[2]);
                options = TextureMapperShaderProgram::TexturePackedYUV;
                break;
            case 2:
                ASSERT(!texture.yuvPlaneOffset[0]);
                options = texture.yuvPlaneOffset[1] ? TextureMapperShaderProgram::TextureNV21 : TextureMapperShaderProgram::TextureNV12;
                break;
            case 3:
                ASSERT(!texture.yuvPlaneOffset[0] && !texture.yuvPlaneOffset[1] && !texture.yuvPlaneOffset[2]);
                options = TextureMapperShaderProgram::TextureYUV;
                break;
            case 4:
                ASSERT(!texture.yuvPlaneOffset[0] && !texture.yuvPlaneOffset[1] && !texture.yuvPlaneOffset[2]);
                options = TextureMapperShaderProgram::TextureYUVA;
                break;
            }
        },
        [&](const Buffer::ExternalOESTexture&) {
            options = TextureMapperShaderProgram::TextureExternalOES;
        }
    );

    if (options != m_shaderOptions) {
        m_shaderProgram = TextureMapperShaderProgram::create(options);
        m_shaderOptions = options;
    }
    if (!m_shaderProgram) {
        previousContext->makeContextCurrent();
        return false;
    }

    // Save previous bound framebuffer, texture and viewport.
    GLint boundFramebuffer = 0;
    GLint boundTexture = 0;
    GLint previousViewport[4] = { 0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFramebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    // Use our own output texture if we are not given one.
    if (!outputTexture)
        outputTexture = m_resultTexture;

    // Set proper parameters to the output texture and allocate uninitialized memory for it.
    glBindTexture(outputTarget, outputTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(outputTarget, level, internalFormat, m_size.width(), m_size.height(), 0, format, type, nullptr);

    // Bind framebuffer to paint and attach the destination texture to it.
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);

    // Set the viewport.
    glViewport(0, 0, m_size.width(), m_size.height());

    // Set program parameters.
    glUseProgram(m_shaderProgram->programID());

    WTF::switchOn(inputTexture.textureVariant(),
        [&](const Buffer::RGBTexture& texture) {
            glUniform1i(m_shaderProgram->samplerLocation(), 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture.id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        },
        [&](const Buffer::YUVTexture& texture) {
            switch (texture.numberOfPlanes) {
            case 1:
                glUniform1i(m_shaderProgram->samplerLocation(), texture.yuvPlane[0]);
                break;
            case 2:
                glUniform1i(m_shaderProgram->samplerYLocation(), texture.yuvPlane[0]);
                glUniform1i(m_shaderProgram->samplerULocation(), texture.yuvPlane[1]);
                break;
            case 3:
                glUniform1i(m_shaderProgram->samplerYLocation(), texture.yuvPlane[0]);
                glUniform1i(m_shaderProgram->samplerULocation(), texture.yuvPlane[1]);
                glUniform1i(m_shaderProgram->samplerVLocation(), texture.yuvPlane[2]);
                break;
            case 4:
                glUniform1i(m_shaderProgram->samplerYLocation(), texture.yuvPlane[0]);
                glUniform1i(m_shaderProgram->samplerULocation(), texture.yuvPlane[1]);
                glUniform1i(m_shaderProgram->samplerVLocation(), texture.yuvPlane[2]);
                glUniform1i(m_shaderProgram->samplerALocation(), texture.yuvPlane[3]);
                break;
            }
            glUniformMatrix3fv(m_shaderProgram->yuvToRgbLocation(), 1, GL_FALSE, static_cast<const GLfloat *>(&texture.yuvToRgbMatrix[0]));

            for (int i = texture.numberOfPlanes - 1; i >= 0; --i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, texture.planes[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        },
        [&](const Buffer::ExternalOESTexture& texture) {
            glUniform1i(m_shaderProgram->externalOESTextureLocation(), 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture.id);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        });

    m_shaderProgram->setMatrix(m_shaderProgram->modelViewMatrixLocation(), m_modelViewMatrix);
    m_shaderProgram->setMatrix(m_shaderProgram->projectionMatrixLocation(), m_projectionMatrix);
    m_shaderProgram->setMatrix(m_shaderProgram->textureSpaceMatrixLocation(), m_textureSpaceMatrix);
    m_shaderProgram->setMatrix(m_shaderProgram->textureColorSpaceMatrixLocation(), m_colorConversionMatrix);

    // Perform the copy.
#if !USE(OPENGL_ES)
    if (GLContext::current()->version() >= 320 && m_vao)
        glBindVertexArray(m_vao);
#endif
    glEnableVertexAttribArray(m_shaderProgram->vertexLocation());
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(m_shaderProgram->vertexLocation(), 2, GL_FLOAT, false, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(m_shaderProgram->vertexLocation());
    glUseProgram(0);

    // Restore previous bindings and viewport.
    glBindFramebuffer(GL_FRAMEBUFFER, boundFramebuffer);
    glBindTexture(outputTarget, boundTexture);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    bool ok = (glGetError() == GL_NO_ERROR);

    // Restore previous context.
    previousContext->makeContextCurrent();
    return ok;
}

} // namespace WebCore

#endif // USE(GSTREAMER_GL)
