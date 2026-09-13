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

#pragma once

#include <epoxy/egl.h> // NOLINT(build/include_order) -- epoxy must precede Qt OpenGL headers.
#include <QOpenGLFunctions>

class WPEQtUnderlayBlitter final {
public:
    WPEQtUnderlayBlitter() = default;
    ~WPEQtUnderlayBlitter();

    Q_DISABLE_COPY(WPEQtUnderlayBlitter);

    // initialize() and importEGLImage() run during scene-graph synchronization;
    // draw() runs during rendering. All methods require a current GL context.
    bool initialize();
    void invalidate();
    bool importEGLImage(EGLImage);
    bool draw(int viewportX, int viewportY, int viewportWidth, int viewportHeight);
    bool isInitialized() const { return m_program; }

private:
    QOpenGLFunctions* m_gl { nullptr };
    GLuint m_program { 0 };
    GLuint m_texture { 0 };
    GLuint m_vertexBuffer { 0 };
    EGLImage m_importedImage { EGL_NO_IMAGE_KHR };
    GLint m_positionLocation { -1 };
    GLint m_texCoordLocation { -1 };
    GLint m_textureLocation { -1 };
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_imageTargetTexture2DOES { nullptr };
};
