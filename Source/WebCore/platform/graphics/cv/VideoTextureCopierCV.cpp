/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VideoTextureCopierCV.h"

#include "Logging.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(IOS)
#include <OpenGLES/ES3/glext.h>
#endif

#include "CoreVideoSoftLink.h"

namespace WebCore {

VideoTextureCopierCV::VideoTextureCopierCV(GraphicsContext3D& context)
    : m_context(context)
    , m_framebuffer(context.createFramebuffer())
{
}

VideoTextureCopierCV::~VideoTextureCopierCV()
{
    if (m_vertexBuffer)
        m_context->deleteProgram(m_vertexBuffer);
    if (m_program)
        m_context->deleteProgram(m_program);
    m_context->deleteFramebuffer(m_framebuffer);
}

#if !LOG_DISABLED
using StringMap = std::map<uint32_t, const char*, std::less<uint32_t>, FastAllocator<std::pair<const uint32_t, const char*>>>;
#define STRINGIFY_PAIR(e) e, #e
static StringMap& enumToStringMap()
{
    static NeverDestroyed<StringMap> map;
    if (map.get().empty()) {
        StringMap stringMap;
        map.get().emplace(STRINGIFY_PAIR(GL_RGB));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE));
        map.get().emplace(STRINGIFY_PAIR(GL_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_R8));
        map.get().emplace(STRINGIFY_PAIR(GL_R16F));
        map.get().emplace(STRINGIFY_PAIR(GL_R32F));
        map.get().emplace(STRINGIFY_PAIR(GL_R8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R8I));
        map.get().emplace(STRINGIFY_PAIR(GL_R16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R16I));
        map.get().emplace(STRINGIFY_PAIR(GL_R32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8));
        map.get().emplace(STRINGIFY_PAIR(GL_SRGB8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8));
        map.get().emplace(STRINGIFY_PAIR(GL_SRGB8_ALPHA8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA4));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB10_A2));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT16));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT24));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT32F));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH24_STENCIL8));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH32F_STENCIL8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE));
        map.get().emplace(STRINGIFY_PAIR(GL_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_RED));
        map.get().emplace(STRINGIFY_PAIR(GL_RG_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_STENCIL));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_BYTE));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_5_6_5));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_4_4_4_4));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_5_5_5_1));
        map.get().emplace(STRINGIFY_PAIR(GL_BYTE));
        map.get().emplace(STRINGIFY_PAIR(GL_HALF_FLOAT));
        map.get().emplace(STRINGIFY_PAIR(GL_FLOAT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT));
        map.get().emplace(STRINGIFY_PAIR(GL_SHORT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT));
        map.get().emplace(STRINGIFY_PAIR(GL_INT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_2_10_10_10_REV));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_24_8));
        map.get().emplace(STRINGIFY_PAIR(GL_FLOAT_32_UNSIGNED_INT_24_8_REV));

#if PLATFORM(IOS)
        map.get().emplace(STRINGIFY_PAIR(GL_RED_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB565));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_R11F_G11F_B10F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB9_E5));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB10_A2UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB5_A1));
        map.get().emplace(STRINGIFY_PAIR(GL_RG));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_10F_11F_11F_REV));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_5_9_9_9_REV));
#endif
    }
    return map.get();
}

#endif

bool VideoTextureCopierCV::initializeContextObjects()
{
    StringBuilder vertexShaderSource;
    vertexShaderSource.appendLiteral("attribute vec4 a_position;\n");
    vertexShaderSource.appendLiteral("uniform int u_flipY;\n");
    vertexShaderSource.appendLiteral("varying vec2 v_texturePosition;\n");
    vertexShaderSource.appendLiteral("void main() {\n");
    vertexShaderSource.appendLiteral("    v_texturePosition = vec2((a_position.x + 1.0) / 2.0, (a_position.y + 1.0) / 2.0);\n");
    vertexShaderSource.appendLiteral("    if (u_flipY == 1) {\n");
    vertexShaderSource.appendLiteral("        v_texturePosition.y = 1.0 - v_texturePosition.y;\n");
    vertexShaderSource.appendLiteral("    }\n");
    vertexShaderSource.appendLiteral("    gl_Position = a_position;\n");
    vertexShaderSource.appendLiteral("}\n");

    Platform3DObject vertexShader = m_context->createShader(GraphicsContext3D::VERTEX_SHADER);
    m_context->shaderSource(vertexShader, vertexShaderSource.toString());
    m_context->compileShaderDirect(vertexShader);

    GC3Dint value = 0;
    m_context->getShaderiv(vertexShader, GraphicsContext3D::COMPILE_STATUS, &value);
    if (!value) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Vertex shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        return false;
    }

    StringBuilder fragmentShaderSource;

#if PLATFORM(IOS)
    fragmentShaderSource.appendLiteral("precision mediump float;\n");
    fragmentShaderSource.appendLiteral("uniform sampler2D u_texture;\n");
#else
    fragmentShaderSource.appendLiteral("uniform sampler2DRect u_texture;\n");
#endif
    fragmentShaderSource.appendLiteral("varying vec2 v_texturePosition;\n");
    fragmentShaderSource.appendLiteral("uniform int u_premultiply;\n");
    fragmentShaderSource.appendLiteral("uniform vec2 u_textureDimensions;\n");
    fragmentShaderSource.appendLiteral("void main() {\n");
    fragmentShaderSource.appendLiteral("    vec2 texPos = vec2(v_texturePosition.x * u_textureDimensions.x, v_texturePosition.y * u_textureDimensions.y);\n");
#if PLATFORM(IOS)
    fragmentShaderSource.appendLiteral("    vec4 color = texture2D(u_texture, texPos);\n");
#else
    fragmentShaderSource.appendLiteral("    vec4 color = texture2DRect(u_texture, texPos);\n");
#endif
    fragmentShaderSource.appendLiteral("    if (u_premultiply == 1) {\n");
    fragmentShaderSource.appendLiteral("        gl_FragColor = vec4(color.r * color.a, color.g * color.a, color.b * color.a, color.a);\n");
    fragmentShaderSource.appendLiteral("    } else {\n");
    fragmentShaderSource.appendLiteral("        gl_FragColor = color;\n");
    fragmentShaderSource.appendLiteral("    }\n");
    fragmentShaderSource.appendLiteral("}\n");

    Platform3DObject fragmentShader = m_context->createShader(GraphicsContext3D::FRAGMENT_SHADER);
    m_context->shaderSource(fragmentShader, fragmentShaderSource.toString());
    m_context->compileShaderDirect(fragmentShader);

    m_context->getShaderiv(fragmentShader, GraphicsContext3D::COMPILE_STATUS, &value);
    if (!value) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Fragment shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        return false;
    }

    m_program = m_context->createProgram();
    m_context->attachShader(m_program, vertexShader);
    m_context->attachShader(m_program, fragmentShader);
    m_context->linkProgram(m_program);

    m_context->getProgramiv(m_program, GraphicsContext3D::LINK_STATUS, &value);
    if (!value) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Program failed to link.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        m_context->deleteProgram(m_program);
        m_program = 0;
        return false;
    }

    m_textureUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_texture"));
    m_textureDimensionsUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_textureDimensions"));
    m_flipYUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_flipY"));
    m_premultiplyUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_premultiply"));
    m_positionAttributeLocation = m_context->getAttribLocationDirect(m_program, ASCIILiteral("a_position"));

    m_context->detachShader(m_program, vertexShader);
    m_context->detachShader(m_program, fragmentShader);
    m_context->deleteShader(vertexShader);
    m_context->deleteShader(fragmentShader);

    LOG(WebGL, "Uniform and Attribute locations: u_texture = %d, u_textureDimensions = %d, u_flipY = %d, u_premultiply = %d, a_position = %d", m_textureUniformLocation, m_textureDimensionsUniformLocation, m_flipYUniformLocation, m_premultiplyUniformLocation, m_positionAttributeLocation);
    m_context->enableVertexAttribArray(m_positionAttributeLocation);

    m_vertexBuffer = m_context->createBuffer();
    float vertices[12] = { -1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1 };

    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vertexBuffer);
    m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(float) * 12, vertices, GraphicsContext3D::STATIC_DRAW);

    return true;
}

bool VideoTextureCopierCV::copyVideoTextureToPlatformTexture(TextureType inputVideoTexture, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    if (!inputVideoTexture)
        return false;

    GC3DStateSaver stateSaver(m_context.get());

    if (!m_program) {
        if (!initializeContextObjects()) {
            LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Unable to initialize OpenGL context objects.", this);
            return false;
        }
    }

    stateSaver.saveVertexAttribState(m_positionAttributeLocation);

    GLfloat lowerLeft[2] = { 0, 0 };
    GLfloat lowerRight[2] = { 0, 0 };
    GLfloat upperRight[2] = { 0, 0 };
    GLfloat upperLeft[2] = { 0, 0 };
#if PLATFORM(IOS)
    Platform3DObject videoTextureName = CVOpenGLESTextureGetName(inputVideoTexture);
    GC3Denum videoTextureTarget = CVOpenGLESTextureGetTarget(inputVideoTexture);
    CVOpenGLESTextureGetCleanTexCoords(inputVideoTexture, lowerLeft, lowerRight, upperRight, upperLeft);
#else
    Platform3DObject videoTextureName = CVOpenGLTextureGetName(inputVideoTexture);
    GC3Denum videoTextureTarget = CVOpenGLTextureGetTarget(inputVideoTexture);
    CVOpenGLTextureGetCleanTexCoords(inputVideoTexture, lowerLeft, lowerRight, upperRight, upperLeft);
#endif

    LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - internalFormat: %s, format: %s, type: %s flipY: %s, premultiplyAlpha: %s", this, enumToStringMap()[internalFormat], enumToStringMap()[format], enumToStringMap()[type], flipY ? "true" : "false", premultiplyAlpha ? "true" : "false");

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_framebuffer);
    
    // Allocate memory for the output texture.
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, outputTexture);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texImage2DDirect(GraphicsContext3D::TEXTURE_2D, level, internalFormat, width, height, 0, format, type, nullptr);

    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, outputTexture, level);
    GC3Denum status = m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER);
    if (status != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Unable to create framebuffer for outputTexture.", this);
        return false;
    }

    m_context->useProgram(m_program);
    m_context->viewport(0, 0, width, height);

    // Bind and set up the texture for the video source.
    m_context->activeTexture(GraphicsContext3D::TEXTURE0);
    m_context->bindTexture(videoTextureTarget, videoTextureName);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);

    // Configure the drawing parameters.
    m_context->uniform1i(m_textureUniformLocation, 0);
#if PLATFORM(IOS)
    m_context->uniform2f(m_textureDimensionsUniformLocation, 1, 1);
#else
    m_context->uniform2f(m_textureDimensionsUniformLocation, width, height);
#endif

    if (lowerLeft[1] < upperRight[1])
        flipY = !flipY;

    m_context->uniform1i(m_flipYUniformLocation, flipY);
    m_context->uniform1i(m_premultiplyUniformLocation, premultiplyAlpha);

    // Do the actual drawing.
    m_context->enableVertexAttribArray(m_positionAttributeLocation);
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vertexBuffer);
    m_context->vertexAttribPointer(m_positionAttributeLocation, 2, GraphicsContext3D::FLOAT, false, 0, 0);
    m_context->drawArrays(GraphicsContext3D::TRIANGLES, 0, 6);

    // Clean-up.
    m_context->bindTexture(videoTextureTarget, 0);
    m_context->bindTexture(outputTarget, outputTexture);

    return true;
}

VideoTextureCopierCV::GC3DStateSaver::GC3DStateSaver(GraphicsContext3D& context)
    : m_context(context)
{
    m_context.getIntegerv(GraphicsContext3D::TEXTURE_BINDING_2D, &m_texture);
    m_context.getIntegerv(GraphicsContext3D::FRAMEBUFFER_BINDING, &m_framebuffer);
    m_context.getIntegerv(GraphicsContext3D::CURRENT_PROGRAM, &m_program);
    m_context.getIntegerv(GraphicsContext3D::ARRAY_BUFFER_BINDING, &m_arrayBuffer);
    m_context.getIntegerv(GraphicsContext3D::VIEWPORT, m_viewport);

}

VideoTextureCopierCV::GC3DStateSaver::~GC3DStateSaver()
{
    if (m_vertexAttribEnabled)
        m_context.enableVertexAttribArray(m_vertexAttribIndex);
    else
        m_context.disableVertexAttribArray(m_vertexAttribIndex);

    m_context.bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_arrayBuffer);
    m_context.vertexAttribPointer(m_vertexAttribIndex, m_vertexAttribSize, m_vertexAttribType, m_vertexAttribNormalized, m_vertexAttribStride, m_vertexAttribPointer);

    m_context.bindTexture(GraphicsContext3D::TEXTURE_BINDING_2D, m_texture);
    m_context.bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_framebuffer);
    m_context.useProgram(m_program);
    m_context.viewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
}

void VideoTextureCopierCV::GC3DStateSaver::saveVertexAttribState(GC3Duint index)
{
    m_vertexAttribIndex = index;
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_ENABLED, &m_vertexAttribEnabled);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_SIZE, &m_vertexAttribSize);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_TYPE, &m_vertexAttribType);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_NORMALIZED, &m_vertexAttribNormalized);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_STRIDE, &m_vertexAttribStride);
    m_vertexAttribPointer = m_context.getVertexAttribOffset(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_POINTER);
}

}
