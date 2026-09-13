/*
 * Copyright (C) 2026 Savoir-faire Linux, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEQtUnderlayBlitter.h"

#include <QOpenGLContext>

static GLuint compileShader(QOpenGLFunctions* gl, GLenum type, const char* source)
{
    auto shader = gl->glCreateShader(type);
    gl->glShaderSource(shader, 1, &source, nullptr);
    gl->glCompileShader(shader);

    GLint status = GL_FALSE;
    gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE)
        return shader;
    gl->glDeleteShader(shader);
    return 0;
}

static GLuint createProgram(QOpenGLFunctions* gl)
{
    static constexpr auto vertexShaderSource = R"(
        attribute vec2 position;
        attribute vec2 texCoord;
        varying vec2 vTexCoord;

        void main()
        {
            vTexCoord = texCoord;
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    static constexpr auto fragmentShaderSource = R"(
        precision mediump float;
        varying vec2 vTexCoord;
        uniform sampler2D u_texture;

        void main()
        {
            gl_FragColor = texture2D(u_texture, vTexCoord);
        }
    )";

    auto vertexShader = compileShader(gl, GL_VERTEX_SHADER, vertexShaderSource);
    if (!vertexShader)
        return 0;

    auto fragmentShader = compileShader(gl, GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!fragmentShader) {
        gl->glDeleteShader(vertexShader);
        return 0;
    }

    auto program = gl->glCreateProgram();
    gl->glAttachShader(program, vertexShader);
    gl->glAttachShader(program, fragmentShader);
    gl->glLinkProgram(program);
    gl->glDeleteShader(vertexShader);
    gl->glDeleteShader(fragmentShader);

    GLint status = GL_FALSE;
    gl->glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_TRUE)
        return program;
    gl->glDeleteProgram(program);
    return 0;
}

WPEQtUnderlayBlitter::~WPEQtUnderlayBlitter()
{
    invalidate();
}

bool WPEQtUnderlayBlitter::initialize()
{
    if (m_program)
        return true;

    auto* context = QOpenGLContext::currentContext();
    if (!context)
        return false;

    m_gl = context->functions();
    m_program = createProgram(m_gl);
    if (!m_program)
        return false;

    m_imageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(epoxy_eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!m_imageTargetTexture2DOES) {
        qWarning("WPEQtUnderlayBlitter::initialize: failed to resolve glEGLImageTargetTexture2DOES");
        invalidate();
        return false;
    }

    m_positionLocation = m_gl->glGetAttribLocation(m_program, "position");
    m_texCoordLocation = m_gl->glGetAttribLocation(m_program, "texCoord");
    m_textureLocation = m_gl->glGetUniformLocation(m_program, "u_texture");
    if (m_positionLocation < 0 || m_texCoordLocation < 0 || m_textureLocation < 0) {
        qWarning("WPEQtUnderlayBlitter::initialize: failed to resolve shader locations (position=%d, texCoord=%d, texture=%d)", m_positionLocation, m_texCoordLocation, m_textureLocation);
        invalidate();
        return false;
    }

    static constexpr GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 1.0f, 0.0f,
    };

    m_gl->glGenBuffers(1, &m_vertexBuffer);
    m_gl->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    m_gl->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    m_gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_gl->glGenTextures(1, &m_texture);
    m_gl->glBindTexture(GL_TEXTURE_2D, m_texture);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    m_gl->glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool WPEQtUnderlayBlitter::importEGLImage(EGLImage image)
{
    if (!image || !initialize())
        return false;

    if (image == m_importedImage)
        return true;

    m_gl->glBindTexture(GL_TEXTURE_2D, m_texture);
    m_imageTargetTexture2DOES(GL_TEXTURE_2D, image);
    m_gl->glBindTexture(GL_TEXTURE_2D, 0);
    m_importedImage = image;
    return true;
}

bool WPEQtUnderlayBlitter::draw(int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    if (viewportWidth <= 0 || viewportHeight <= 0)
        return false;

    if (!initialize() || !m_texture)
        return false;
    // Qt Quick owns the surrounding GL state, so save and restore everything this
    // function changes.
    GLint prevProgram = 0;
    m_gl->glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    GLint prevActiveTexture = GL_TEXTURE0;
    m_gl->glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);

    GLint prevTexture2D = 0;
    m_gl->glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture2D);

    GLint prevArrayBuffer = 0;
    m_gl->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);

    GLint prevViewport[4] = { 0, 0, 0, 0 };
    m_gl->glGetIntegerv(GL_VIEWPORT, prevViewport);

    GLboolean wasBlendEnabled = m_gl->glIsEnabled(GL_BLEND);
    m_gl->glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    // The WPE frame is copied opaquely into the target; blending is restored below.
    m_gl->glDisable(GL_BLEND);
    m_gl->glUseProgram(m_program);
    m_gl->glActiveTexture(GL_TEXTURE0);
    m_gl->glBindTexture(GL_TEXTURE_2D, m_texture);
    m_gl->glUniform1i(m_textureLocation, 0);
    m_gl->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    m_gl->glEnableVertexAttribArray(m_positionLocation);
    m_gl->glEnableVertexAttribArray(m_texCoordLocation);
    m_gl->glVertexAttribPointer(m_positionLocation, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    m_gl->glVertexAttribPointer(m_texCoordLocation, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
    m_gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_gl->glDisableVertexAttribArray(m_texCoordLocation);
    m_gl->glDisableVertexAttribArray(m_positionLocation);
    m_gl->glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
    m_gl->glBindTexture(GL_TEXTURE_2D, prevTexture2D);
    m_gl->glActiveTexture(prevActiveTexture);
    m_gl->glUseProgram(prevProgram);
    m_gl->glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    if (wasBlendEnabled)
        m_gl->glEnable(GL_BLEND);
    return true;
}

void WPEQtUnderlayBlitter::invalidate()
{
    auto* context = QOpenGLContext::currentContext();
    if (auto* gl = context ? context->functions() : nullptr) {
        if (m_texture)
            gl->glDeleteTextures(1, &m_texture);
        if (m_vertexBuffer)
            gl->glDeleteBuffers(1, &m_vertexBuffer);
        if (m_program)
            gl->glDeleteProgram(m_program);
    }

    m_texture = 0;
    m_vertexBuffer = 0;
    m_importedImage = EGL_NO_IMAGE_KHR;
    m_program = 0;
    m_positionLocation = -1;
    m_texCoordLocation = -1;
    m_textureLocation = -1;
    m_imageTargetTexture2DOES = nullptr;
    m_gl = nullptr;
}
