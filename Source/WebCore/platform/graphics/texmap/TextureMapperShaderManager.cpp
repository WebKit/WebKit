/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

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
#include "TextureMapperShaderManager.h"

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)

#include "TextureMapperGL.h"

namespace WebCore {

#ifndef TEXMAP_OPENGL_ES_2
#define OES2_PRECISION_DEFINITIONS \
    "#define lowp\n#define highp\n"
#define OES2_FRAGMENT_SHADER_DEFAULT_PRECISION
#else
#define OES2_PRECISION_DEFINITIONS
#define OES2_FRAGMENT_SHADER_DEFAULT_PRECISION \
    "precision mediump float; \n"
#endif

#define VERTEX_SHADER(src...) OES2_PRECISION_DEFINITIONS#src
#define FRAGMENT_SHADER(src...) OES2_PRECISION_DEFINITIONS\
                                OES2_FRAGMENT_SHADER_DEFAULT_PRECISION\
                                #src

static const char* fragmentShaderSourceOpacityAndMask =
    FRAGMENT_SHADER(
        uniform sampler2D SourceTexture, MaskTexture;
        uniform lowp float Opacity;
        varying highp vec2 OutTexCoordSource, OutTexCoordMask;
        void main(void)
        {
            lowp vec4 color = texture2D(SourceTexture, OutTexCoordSource);
            lowp vec4 maskColor = texture2D(MaskTexture, OutTexCoordMask);
            lowp float fragmentAlpha = Opacity * maskColor.a;
            gl_FragColor = vec4(color.rgb * fragmentAlpha, color.a * fragmentAlpha);
        }
    );

static const char* vertexShaderSourceOpacityAndMask =
    VERTEX_SHADER(
        uniform mat4 InMatrix, InSourceMatrix;
        attribute vec4 InVertex;
        varying highp vec2 OutTexCoordSource, OutTexCoordMask;
        void main(void)
        {
            OutTexCoordSource = vec2(InSourceMatrix * InVertex);
            OutTexCoordMask = vec2(InVertex);
            gl_Position = InMatrix * InVertex;
        }
    );

static const char* fragmentShaderSourceSimple =
    FRAGMENT_SHADER(
        uniform sampler2D SourceTexture;
        uniform lowp float Opacity;
        varying highp vec2 OutTexCoordSource;
        void main(void)
        {
            lowp vec4 color = texture2D(SourceTexture, OutTexCoordSource);
            gl_FragColor = vec4(color.rgb * Opacity, color.a * Opacity);
        }
    );

static const char* vertexShaderSourceSimple =
    VERTEX_SHADER(
        uniform mat4 InMatrix, InSourceMatrix;
        attribute vec4 InVertex;
        varying highp vec2 OutTexCoordSource;
        void main(void)
        {
            OutTexCoordSource = vec2(InSourceMatrix * InVertex);
            gl_Position = InMatrix * InVertex;
        }
    );

void TextureMapperShaderProgram::initializeProgram()
{
    const char* vertexShaderSourceProgram = vertexShaderSource();
    const char* fragmentShaderSourceProgram = fragmentShaderSource();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSourceProgram, 0);
    glShaderSource(fragmentShader, 1, &fragmentShaderSourceProgram, 0);
    GLuint programID = glCreateProgram();
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    m_vertexAttrib = glGetAttribLocation(programID, "InVertex");
    m_id = programID;
    m_vertexShader = vertexShader;
    m_fragmentShader = fragmentShader;
}

void TextureMapperShaderProgram::getUniformLocation(GLint &variable, const char* name)
{
    variable = glGetUniformLocation(m_id, name);
    ASSERT(variable >= 0);
}

TextureMapperShaderProgram::~TextureMapperShaderProgram()
{
    GLuint programID = m_id;
    if (!programID)
        return;

    glDetachShader(programID, m_vertexShader);
    glDeleteShader(m_vertexShader);
    glDetachShader(programID, m_fragmentShader);
    glDeleteShader(m_fragmentShader);
    glDeleteProgram(programID);
}

PassRefPtr<TextureMapperShaderProgramSimple> TextureMapperShaderProgramSimple::create()
{
    return adoptRef(new TextureMapperShaderProgramSimple());
}

TextureMapperShaderProgramSimple::TextureMapperShaderProgramSimple()
{
    initializeProgram();
    getUniformLocation(m_sourceMatrixVariable, "InSourceMatrix");
    getUniformLocation(m_matrixVariable, "InMatrix");
    getUniformLocation(m_sourceTextureVariable, "SourceTexture");
    getUniformLocation(m_opacityVariable, "Opacity");
}

const char* TextureMapperShaderProgramSimple::vertexShaderSource() const
{
    return vertexShaderSourceSimple;
}

const char* TextureMapperShaderProgramSimple::fragmentShaderSource() const
{
    return fragmentShaderSourceSimple;
}

void TextureMapperShaderProgramSimple::prepare(float opacity, const BitmapTexture* maskTexture)
{
    glUniform1f(m_opacityVariable, opacity);
}

PassRefPtr<TextureMapperShaderProgramOpacityAndMask> TextureMapperShaderProgramOpacityAndMask::create()
{
    return adoptRef(new TextureMapperShaderProgramOpacityAndMask());
}

TextureMapperShaderProgramOpacityAndMask::TextureMapperShaderProgramOpacityAndMask()
{
    initializeProgram();
    getUniformLocation(m_matrixVariable, "InMatrix");
    getUniformLocation(m_sourceMatrixVariable, "InSourceMatrix");
    getUniformLocation(m_sourceTextureVariable, "SourceTexture");
    getUniformLocation(m_maskTextureVariable, "MaskTexture");
    getUniformLocation(m_opacityVariable, "Opacity");
}

const char* TextureMapperShaderProgramOpacityAndMask::vertexShaderSource() const
{
    return vertexShaderSourceOpacityAndMask;
}

const char* TextureMapperShaderProgramOpacityAndMask::fragmentShaderSource() const
{
    return fragmentShaderSourceOpacityAndMask;
}

void TextureMapperShaderProgramOpacityAndMask::prepare(float opacity, const BitmapTexture* maskTexture)
{
    glUniform1f(m_opacityVariable, opacity);
    if (!maskTexture || !maskTexture->isValid())
        return;

    const BitmapTextureGL* maskTextureGL = static_cast<const BitmapTextureGL*>(maskTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, maskTextureGL->id());
    glUniform1i(m_maskTextureVariable, 1);
    glActiveTexture(GL_TEXTURE0);
}

TextureMapperShaderManager::TextureMapperShaderManager()
{
    ASSERT(initializeOpenGLShims());
}

TextureMapperShaderManager::~TextureMapperShaderManager()
{
    m_textureMapperShaderProgramMap.clear();
}

};

#endif
