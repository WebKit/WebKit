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

static const char* vertexShaderSourceSolidColor =
    VERTEX_SHADER(
        uniform mat4 InMatrix;
        attribute vec4 InVertex;
        void main(void)
        {
            gl_Position = InMatrix * InVertex;
        }
    );


static const char* fragmentShaderSourceSolidColor =
    VERTEX_SHADER(
        uniform vec4 Color;
        void main(void)
        {
            gl_FragColor = Color;
        }
    );

PassRefPtr<TextureMapperShaderProgramSolidColor> TextureMapperShaderManager::solidColorProgram()
{
    return static_pointer_cast<TextureMapperShaderProgramSolidColor>(getShaderProgram(SolidColor));
}

PassRefPtr<TextureMapperShaderProgram> TextureMapperShaderManager::getShaderProgram(ShaderType shaderType)
{
    RefPtr<TextureMapperShaderProgram> program;
    if (shaderType == Invalid)
        return program;

    TextureMapperShaderProgramMap::iterator it = m_textureMapperShaderProgramMap.find(shaderType);
    if (it != m_textureMapperShaderProgramMap.end())
        return it->second;

    switch (shaderType) {
    case Simple:
        program = TextureMapperShaderProgramSimple::create();
        break;
    case OpacityAndMask:
        program = TextureMapperShaderProgramOpacityAndMask::create();
        break;
    case SolidColor:
        program = TextureMapperShaderProgramSolidColor::create();
        break;
    case Invalid:
        ASSERT_NOT_REACHED();
    }
    m_textureMapperShaderProgramMap.add(shaderType, program);
    return program;
}

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

PassRefPtr<TextureMapperShaderProgramSolidColor> TextureMapperShaderProgramSolidColor::create()
{
    return adoptRef(new TextureMapperShaderProgramSolidColor());
}

TextureMapperShaderProgramSolidColor::TextureMapperShaderProgramSolidColor()
{
    initializeProgram();
    getUniformLocation(m_matrixVariable, "InMatrix");
    getUniformLocation(m_colorVariable, "Color");
}

const char* TextureMapperShaderProgramSolidColor::vertexShaderSource() const
{
    return vertexShaderSourceSolidColor;
}

const char* TextureMapperShaderProgramSolidColor::fragmentShaderSource() const
{
    return fragmentShaderSourceSolidColor;
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
}

#if ENABLE(CSS_FILTERS)
StandardFilterProgram::~StandardFilterProgram()
{
    glDetachShader(m_id, m_vertexShader);
    glDeleteShader(m_vertexShader);
    glDetachShader(m_id, m_fragmentShader);
    glDeleteShader(m_fragmentShader);
    glDeleteProgram(m_id);
}

StandardFilterProgram::StandardFilterProgram(FilterOperation::OperationType type)
    : m_id(0)
{
    const char* vertexShaderSource =
            VERTEX_SHADER(
                attribute vec4 a_vertex;
                attribute vec4 a_texCoord;
                varying highp vec2 v_texCoord;
                void main(void)
                {
                    v_texCoord = vec2(a_texCoord);
                    gl_Position = a_vertex;
                }
            );

#define STANDARD_FILTER(x...) \
        OES2_PRECISION_DEFINITIONS\
        OES2_FRAGMENT_SHADER_DEFAULT_PRECISION\
        "varying highp vec2 v_texCoord;\n"\
        "uniform highp float u_amount;\n"\
        "uniform sampler2D u_texture;\n"\
        #x\
        "void main(void)\n { gl_FragColor = shade(texture2D(u_texture, v_texCoord)); }"

    const char* fragmentShaderSource = 0;
    switch (type) {
    case FilterOperation::GRAYSCALE:
        fragmentShaderSource = STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                lowp float amount = 1.0 - u_amount;
                return vec4((0.2126 + 0.7874 * amount) * color.r + (0.7152 - 0.7152 * amount) * color.g + (0.0722 - 0.0722 * amount) * color.b,
                            (0.2126 - 0.2126 * amount) * color.r + (0.7152 + 0.2848 * amount) * color.g + (0.0722 - 0.0722 * amount) * color.b,
                            (0.2126 - 0.2126 * amount) * color.r + (0.7152 - 0.7152 * amount) * color.g + (0.0722 + 0.9278 * amount) * color.b,
                            color.a);
            }
        );
        break;
    case FilterOperation::SEPIA:
        fragmentShaderSource = STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                lowp float amount = 1.0 - u_amount;
                return vec4((0.393 + 0.607 * amount) * color.r + (0.769 - 0.769 * amount) * color.g + (0.189 - 0.189 * amount) * color.b,
                            (0.349 - 0.349 * amount) * color.r + (0.686 + 0.314 * amount) * color.g + (0.168 - 0.168 * amount) * color.b,
                            (0.272 - 0.272 * amount) * color.r + (0.534 - 0.534 * amount) * color.g + (0.131 + 0.869 * amount) * color.b,
                            color.a);
            }
        );
        break;
    case FilterOperation::SATURATE:
        fragmentShaderSource = STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4((0.213 + 0.787 * u_amount) * color.r + (0.715 - 0.715 * u_amount) * color.g + (0.072 - 0.072 * u_amount) * color.b,
                            (0.213 - 0.213 * u_amount) * color.r + (0.715 + 0.285 * u_amount) * color.g + (0.072 - 0.072 * u_amount) * color.b,
                            (0.213 - 0.213 * u_amount) * color.r + (0.715 - 0.715 * u_amount) * color.g + (0.072 + 0.928 * u_amount) * color.b,
                            color.a);
            }
        );
        break;
    case FilterOperation::HUE_ROTATE:
        fragmentShaderSource = STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                highp float pi = 3.14159265358979323846;
                highp float c = cos(u_amount * pi / 180.0);
                highp float s = sin(u_amount * pi / 180.0);
                return vec4(color.r * (0.213 + c * 0.787 - s * 0.213) + color.g * (0.715 - c * 0.715 - s * 0.715) + color.b * (0.072 - c * 0.072 + s * 0.928),
                            color.r * (0.213 - c * 0.213 + s * 0.143) + color.g * (0.715 + c * 0.285 + s * 0.140) + color.b * (0.072 - c * 0.072 - s * 0.283),
                            color.r * (0.213 - c * 0.213 - s * 0.787) +  color.g * (0.715 - c * 0.715 + s * 0.715) + color.b * (0.072 + c * 0.928 + s * 0.072),
                            color.a);
            }
        );
        break;
    case FilterOperation::INVERT:
        fragmentShaderSource = STANDARD_FILTER(
            lowp float invert(lowp float n) { return (1.0 - n) * u_amount + n * (1.0 - u_amount); }
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(invert(color.r), invert(color.g), invert(color.b), color.a);
            }
        );
        break;
    case FilterOperation::BRIGHTNESS:
        fragmentShaderSource = STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(color.rgb * (1.0 + u_amount), color.a);
            }
        );
        break;
    case FilterOperation::CONTRAST:
        fragmentShaderSource = STANDARD_FILTER(
            lowp float contrast(lowp float n) { return (n - 0.5) * u_amount + 0.5; }
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(contrast(color.r), contrast(color.g), contrast(color.b), color.a);
            }
        );
        break;
    case FilterOperation::OPACITY:
        fragmentShaderSource = STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(color.r, color.g, color.b, color.a * u_amount);
            }
        );
        break;
    default:
        break;
    }

    if (!fragmentShaderSource)
        return;
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, 0);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, 0);
    GLchar log[100];
    GLint len;
    GLuint programID = glCreateProgram();
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    glGetShaderInfoLog(fragmentShader, 100, &len, log);
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    m_vertexAttrib = glGetAttribLocation(programID, "a_vertex");
    m_texCoordAttrib = glGetAttribLocation(programID, "a_texCoord");
    m_textureUniformLocation = glGetUniformLocation(programID, "u_texture");
    switch (type) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
        m_uniformLocations.amount = glGetUniformLocation(programID, "u_amount");
        break;
    default:
        break;
    }
    m_id = programID;
    m_vertexShader = vertexShader;
    m_fragmentShader = fragmentShader;
}

PassRefPtr<StandardFilterProgram> StandardFilterProgram::create(FilterOperation::OperationType type)
{
    RefPtr<StandardFilterProgram> program = adoptRef(new StandardFilterProgram(type));
    if (!program->m_id)
        return 0;

    return program;
}

void StandardFilterProgram::prepare(const FilterOperation& operation)
{
    double amount = 0;
    switch (operation.getOperationType()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
        amount = static_cast<const BasicColorMatrixFilterOperation&>(operation).amount();
        break;
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
        amount = static_cast<const BasicComponentTransferFilterOperation&>(operation).amount();
        break;
    default:
        break;
    }
    glUseProgram(m_id);
    glUniform1f(m_uniformLocations.amount, amount);
}

PassRefPtr<StandardFilterProgram> TextureMapperShaderManager::getShaderForFilter(const FilterOperation& filter)
{
    RefPtr<StandardFilterProgram> program;
    FilterOperation::OperationType type = filter.getOperationType();
    FilterMap::iterator iterator = m_filterMap.find(type);
    if (iterator == m_filterMap.end()) {
        program = StandardFilterProgram::create(type);
        if (!program)
            return 0;

        m_filterMap.add(type, program);
    } else
        program = iterator->second;

    program->prepare(filter);
    return program;
}

#endif
};

#endif
