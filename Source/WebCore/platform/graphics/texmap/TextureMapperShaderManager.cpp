/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2012 Igalia S.L.
 Copyright (C) 2011 Google Inc. All rights reserved.

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

#include "LengthFunctions.h"
#include "Logging.h"
#include "TextureMapperGL.h"

namespace WebCore {

#define STRINGIFY(...) #__VA_ARGS__

static const char* fragmentShaderSourceOpacityAndMask =
    STRINGIFY(
        precision mediump float;
        uniform sampler2D s_source;
        uniform sampler2D s_mask;
        uniform lowp float u_opacity;
        varying highp vec2 v_sourceTexCoord;
        varying highp vec2 v_maskTexCoord;
        void main(void)
        {
            lowp vec4 color = texture2D(s_source, v_sourceTexCoord);
            lowp vec4 maskColor = texture2D(s_mask, v_maskTexCoord);
            lowp float fragmentAlpha = u_opacity * maskColor.a;
            gl_FragColor = vec4(color.rgb * fragmentAlpha, color.a * fragmentAlpha);
        }
    );

static const char* fragmentShaderSourceRectOpacityAndMask =
    STRINGIFY(
        precision mediump float;
        uniform sampler2DRect s_source;
        uniform sampler2DRect s_mask;
        uniform lowp float u_opacity;
        varying highp vec2 v_sourceTexCoord;
        varying highp vec2 v_maskTexCoord;
        void main(void)
        {
            lowp vec4 color = texture2DRect(s_source, v_sourceTexCoord);
            lowp vec4 maskColor = texture2DRect(s_mask, v_maskTexCoord);
            lowp float fragmentAlpha = u_opacity * maskColor.a;
            gl_FragColor = vec4(color.rgb * fragmentAlpha, color.a * fragmentAlpha);
        }
    );

static const char* vertexShaderSourceOpacityAndMask =
    STRINGIFY(
        uniform mat4 u_matrix;
        uniform lowp float u_flip;
        attribute vec4 a_vertex;
        varying highp vec2 v_sourceTexCoord;
        varying highp vec2 v_maskTexCoord;
        void main(void)
        {
            v_sourceTexCoord = vec2(a_vertex.x, mix(a_vertex.y, 1. - a_vertex.y, u_flip));
            v_maskTexCoord = vec2(a_vertex);
            gl_Position = u_matrix * a_vertex;
        }
    );

static const char* fragmentShaderSourceSimple =
    STRINGIFY(
        precision mediump float;
        uniform sampler2D s_source;
        uniform lowp float u_opacity;
        varying highp vec2 v_sourceTexCoord;
        void main(void)
        {
            lowp vec4 color = texture2D(s_source, v_sourceTexCoord);
            gl_FragColor = vec4(color.rgb * u_opacity, color.a * u_opacity);
        }
    );

static const char* fragmentShaderSourceAntialiasingNoMask =
    STRINGIFY(
        precision mediump float;
        uniform sampler2D s_source;
        varying highp vec2 v_sourceTexCoord;
        uniform lowp float u_opacity;
        uniform vec3 u_expandedQuadEdgesInScreenSpace[8];
        void main()
        {
            vec4 sampledColor = texture2D(s_source, clamp(v_sourceTexCoord, 0.0, 1.0));
            vec3 pos = vec3(gl_FragCoord.xy, 1);

            // The data passed in u_expandedQuadEdgesInScreenSpace is merely the
            // pre-scaled coeffecients of the line equations describing the four edges
            // of the expanded quad in screen space and the rectangular bounding box
            // of the expanded quad.
            //
            // We are doing a simple distance calculation here according to the formula:
            // (A*p.x + B*p.y + C) / sqrt(A^2 + B^2) = distance from line to p
            // Note that A, B and C have already been scaled by 1 / sqrt(A^2 + B^2).
            float a0 = clamp(dot(u_expandedQuadEdgesInScreenSpace[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(u_expandedQuadEdgesInScreenSpace[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(u_expandedQuadEdgesInScreenSpace[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(u_expandedQuadEdgesInScreenSpace[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(u_expandedQuadEdgesInScreenSpace[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(u_expandedQuadEdgesInScreenSpace[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(u_expandedQuadEdgesInScreenSpace[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(u_expandedQuadEdgesInScreenSpace[7], pos), 0.0, 1.0);

            // Now we want to reduce the alpha value of the fragment if it is close to the
            // edges of the expanded quad (or rectangular bounding box -- which seems to be
            // important for backfacing quads). Note that we are combining the contribution
            // from the (top || bottom) and (left || right) edge by simply multiplying. This follows
            // the approach described at: http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter22.html,
            // in this case without using Gaussian weights.
            gl_FragColor = sampledColor * u_opacity * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );

static const char* fragmentShaderSourceRectSimple =
    STRINGIFY(
        precision mediump float;
        uniform sampler2DRect s_source;
        uniform lowp vec2 u_textureSize;
        uniform lowp float u_opacity;
        varying highp vec2 v_sourceTexCoord;
        void main(void)
        {
            lowp vec4 color = texture2DRect(s_source, u_textureSize * v_sourceTexCoord);
            gl_FragColor = vec4(color.rgb * u_opacity, color.a * u_opacity);
        }
    );

static const char* vertexShaderSourceSimple =
    STRINGIFY(
        uniform mat4 u_matrix;
        uniform lowp float u_flip;
        attribute vec4 a_vertex;
        varying highp vec2 v_sourceTexCoord;
        void main(void)
        {
            v_sourceTexCoord = vec2(a_vertex.x, mix(a_vertex.y, 1. - a_vertex.y, u_flip));
            gl_Position = u_matrix * a_vertex;
        }
    );

static const char* vertexShaderSourceSolidColor =
    STRINGIFY(
        uniform mat4 u_matrix;
        attribute vec4 a_vertex;
        void main(void)
        {
            gl_Position = u_matrix * a_vertex;
        }
    );


static const char* fragmentShaderSourceSolidColor =
    STRINGIFY(
        precision mediump float;
        uniform vec4 u_color;
        void main(void)
        {
            gl_FragColor = u_color;
        }
    );

PassRefPtr<TextureMapperShaderProgramSolidColor> TextureMapperShaderManager::solidColorProgram()
{
    return static_pointer_cast<TextureMapperShaderProgramSolidColor>(getShaderProgram(SolidColor));
}

PassRefPtr<TextureMapperShaderProgramAntialiasingNoMask> TextureMapperShaderManager::antialiasingNoMaskProgram()
{
    return static_pointer_cast<TextureMapperShaderProgramAntialiasingNoMask>(getShaderProgram(AntialiasingNoMask));
}

PassRefPtr<TextureMapperShaderProgram> TextureMapperShaderManager::getShaderProgram(ShaderType shaderType)
{
    RefPtr<TextureMapperShaderProgram> program;
    if (shaderType == Invalid)
        return program;

    TextureMapperShaderProgramMap::iterator it = m_textureMapperShaderProgramMap.find(shaderType);
    if (it != m_textureMapperShaderProgramMap.end())
        return it->value;

    switch (shaderType) {
    case Simple:
        program = TextureMapperShaderProgramSimple::create(m_context);
        break;
    case RectSimple:
        program = TextureMapperShaderProgramRectSimple::create(m_context);
        break;
    case AntialiasingNoMask:
        program = TextureMapperShaderProgramAntialiasingNoMask::create(m_context);
        break;
    case OpacityAndMask:
        program = TextureMapperShaderProgramOpacityAndMask::create(m_context);
        break;
    case RectOpacityAndMask:
        program = TextureMapperShaderProgramRectOpacityAndMask::create(m_context);
        break;
    case SolidColor:
        program = TextureMapperShaderProgramSolidColor::create(m_context);
        break;
    case Invalid:
        ASSERT_NOT_REACHED();
    }
    m_textureMapperShaderProgramMap.add(shaderType, program);
    return program;
}

TextureMapperShaderProgram::TextureMapperShaderProgram(GraphicsContext3D* context, const char* vertexShaderSource, const char* fragmentShaderSource)
    : m_context(context)
    , m_id(0)
    , m_vertexAttrib(0)
    , m_vertexShader(0)
    , m_fragmentShader(0)
    , m_matrixLocation(-1)
    , m_flipLocation(-1)
    , m_textureSizeLocation(-1)
    , m_sourceTextureLocation(-1)
    , m_opacityLocation(-1)
    , m_maskTextureLocation(-1)
    , m_vertexShaderSource(vertexShaderSource)
    , m_fragmentShaderSource(fragmentShaderSource)
{
}

void TextureMapperShaderProgram::initializeProgram()
{
    const char* vertexShaderSourceProgram = vertexShaderSource();
    const char* fragmentShaderSourceProgram = fragmentShaderSource();
    Platform3DObject vertexShader = m_context->createShader(GraphicsContext3D::VERTEX_SHADER);
    Platform3DObject fragmentShader = m_context->createShader(GraphicsContext3D::FRAGMENT_SHADER);
    m_context->shaderSource(vertexShader, vertexShaderSourceProgram);
    m_context->shaderSource(fragmentShader, fragmentShaderSourceProgram);
    Platform3DObject programID = m_context->createProgram();
    m_context->compileShader(vertexShader);
    m_context->compileShader(fragmentShader);

    m_context->attachShader(programID, vertexShader);
    m_context->attachShader(programID, fragmentShader);
    m_context->linkProgram(programID);

    m_vertexAttrib = m_context->getAttribLocation(programID, "a_vertex");

    m_id = programID;
    m_vertexShader = vertexShader;
    m_fragmentShader = fragmentShader;
}

void TextureMapperShaderProgram::getUniformLocation(GC3Dint &variable, const char* name)
{
    variable = m_context->getUniformLocation(m_id, name);
    ASSERT(variable >= 0);
}

TextureMapperShaderProgram::~TextureMapperShaderProgram()
{
    Platform3DObject programID = m_id;
    if (!programID)
        return;

    m_context->detachShader(programID, m_vertexShader);
    m_context->deleteShader(m_vertexShader);
    m_context->detachShader(programID, m_fragmentShader);
    m_context->deleteShader(m_fragmentShader);
    m_context->deleteProgram(programID);
}

TextureMapperShaderProgramSimple::TextureMapperShaderProgramSimple(GraphicsContext3D* context)
    : TextureMapperShaderProgram(context, vertexShaderSourceSimple, fragmentShaderSourceSimple)
{
    initializeProgram();
    getUniformLocation(m_flipLocation, "u_flip");
    getUniformLocation(m_matrixLocation, "u_matrix");
    getUniformLocation(m_sourceTextureLocation, "s_source");
    getUniformLocation(m_opacityLocation, "u_opacity");
}

TextureMapperShaderProgramSolidColor::TextureMapperShaderProgramSolidColor(GraphicsContext3D* context)
    : TextureMapperShaderProgram(context, vertexShaderSourceSolidColor, fragmentShaderSourceSolidColor)
{
    initializeProgram();
    getUniformLocation(m_matrixLocation, "u_matrix");
    getUniformLocation(m_colorLocation, "u_color");
}

TextureMapperShaderProgramRectSimple::TextureMapperShaderProgramRectSimple(GraphicsContext3D* context)
    : TextureMapperShaderProgram(context, vertexShaderSourceSimple, fragmentShaderSourceRectSimple)
{
    initializeProgram();
    getUniformLocation(m_matrixLocation, "u_matrix");
    getUniformLocation(m_flipLocation, "u_flip");
    getUniformLocation(m_textureSizeLocation, "u_textureSize");
    getUniformLocation(m_sourceTextureLocation, "s_source");
    getUniformLocation(m_opacityLocation, "u_opacity");
}

TextureMapperShaderProgramOpacityAndMask::TextureMapperShaderProgramOpacityAndMask(GraphicsContext3D* context)
    : TextureMapperShaderProgram(context, vertexShaderSourceOpacityAndMask, fragmentShaderSourceOpacityAndMask)
{
    initializeProgram();
    getUniformLocation(m_matrixLocation, "u_matrix");
    getUniformLocation(m_flipLocation, "u_flip");
    getUniformLocation(m_sourceTextureLocation, "s_source");
    getUniformLocation(m_maskTextureLocation, "s_mask");
    getUniformLocation(m_opacityLocation, "u_opacity");
}

TextureMapperShaderProgramRectOpacityAndMask::TextureMapperShaderProgramRectOpacityAndMask(GraphicsContext3D* context)
    : TextureMapperShaderProgram(context, vertexShaderSourceOpacityAndMask, fragmentShaderSourceRectOpacityAndMask)
{
    initializeProgram();
    getUniformLocation(m_matrixLocation, "u_matrix");
    getUniformLocation(m_flipLocation, "u_flip");
    getUniformLocation(m_sourceTextureLocation, "s_source");
    getUniformLocation(m_maskTextureLocation, "s_mask");
    getUniformLocation(m_opacityLocation, "u_opacity");
}

TextureMapperShaderProgramAntialiasingNoMask::TextureMapperShaderProgramAntialiasingNoMask(GraphicsContext3D* context)
    : TextureMapperShaderProgram(context, vertexShaderSourceSimple, fragmentShaderSourceAntialiasingNoMask)
{
    initializeProgram();
    getUniformLocation(m_matrixLocation, "u_matrix");
    getUniformLocation(m_sourceTextureLocation, "s_source");
    getUniformLocation(m_opacityLocation, "u_opacity");
    getUniformLocation(m_expandedQuadEdgesInScreenSpaceLocation, "u_expandedQuadEdgesInScreenSpace");
    getUniformLocation(m_flipLocation, "u_flip");
}

TextureMapperShaderManager::TextureMapperShaderManager(GraphicsContext3D* context)
    : m_context(context)
{
}

TextureMapperShaderManager::~TextureMapperShaderManager()
{
}

#if ENABLE(CSS_FILTERS)

// Create a normal distribution of 21 values between -2 and 2.
#define GAUSSIAN_KERNEL_HALF_WIDTH 11
#define GAUSSIAN_KERNEL_STEP 0.2

StandardFilterProgram::~StandardFilterProgram()
{
    m_context->detachShader(m_id, m_vertexShader);
    m_context->deleteShader(m_vertexShader);
    m_context->detachShader(m_id, m_fragmentShader);
    m_context->deleteShader(m_fragmentShader);
    m_context->deleteProgram(m_id);
}

StandardFilterProgram::StandardFilterProgram(GraphicsContext3D* context, FilterOperation::OperationType type, unsigned pass)
    : m_context(context)
    , m_id(0)
{
    const char* vertexShaderSource =
            STRINGIFY(
                attribute vec4 a_vertex;
                attribute vec4 a_texCoord;
                varying highp vec2 v_texCoord;
                void main(void)
                {
                    v_texCoord = vec2(a_texCoord);
                    gl_Position = a_vertex;
                }
            );

#define STANDARD_FILTER(...) \
        "precision mediump float;\n"\
        "varying highp vec2 v_texCoord;\n"\
        "uniform highp float u_amount;\n"\
        "uniform sampler2D u_texture;\n"\
        #__VA_ARGS__ \
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
    case FilterOperation::BLUR:
        fragmentShaderSource = STRINGIFY(
            precision mediump float;
            varying highp vec2 v_texCoord;
            uniform lowp vec2 u_blurRadius;
            uniform sampler2D u_texture;
            uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];

            lowp vec4 sampleColor(float radius)
            {
                vec2 coord = v_texCoord + radius * u_blurRadius;
                return texture2D(u_texture, coord) * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
            }

            vec4 blur()
            {
                vec4 total = sampleColor(0) * u_gaussianKernel[0];
                for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
                    total += sampleColor(float(i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                    total += sampleColor(float(-1 * i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                }

                return total;
            }

            void main(void)
            {
                gl_FragColor = blur();
            }
        );
        break;
    case FilterOperation::DROP_SHADOW:
        switch (pass) {
        case 0: {
            // First pass: horizontal alpha blur.
            fragmentShaderSource = STRINGIFY(
                precision mediump float;
                varying highp vec2 v_texCoord;
                uniform lowp float u_shadowBlurRadius;
                uniform lowp vec2 u_shadowOffset;
                uniform sampler2D u_texture;
                uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];

                lowp float sampleAlpha(float radius)
                {
                    vec2 coord = v_texCoord - u_shadowOffset + vec2(radius * u_shadowBlurRadius, 0.);
                    return texture2D(u_texture, coord).a * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
                }

                lowp float shadowBlurHorizontal()
                {
                    float total = sampleAlpha(0) * u_gaussianKernel[0];
                    for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
                        total += sampleAlpha(float(i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                        total += sampleAlpha(float(-1 * i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                    }

                    return total;
                }

                void main(void)
                {
                    gl_FragColor = vec4(1., 1., 1., 1.) * shadowBlurHorizontal();
                }
            );
            break;
            }
        case 1: {
            // Second pass: vertical alpha blur and composite with origin.
            fragmentShaderSource = STRINGIFY(
                precision mediump float;
                varying highp vec2 v_texCoord;
                uniform lowp float u_shadowBlurRadius;
                uniform lowp vec4 u_shadowColor;
                uniform sampler2D u_texture;
                uniform sampler2D u_contentTexture;
                uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];

                lowp float sampleAlpha(float r)
                {
                    vec2 coord = v_texCoord + vec2(0., r * u_shadowBlurRadius);
                    return texture2D(u_texture, coord).a * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
                }

                lowp float shadowBlurVertical()
                {
                    float total = sampleAlpha(0) * u_gaussianKernel[0];
                    for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
                        total += sampleAlpha(float(i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                        total += sampleAlpha(float(-1 * i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                    }

                    return total;
                }

                lowp vec4 sourceOver(lowp vec4 source, lowp vec4 destination)
                {
                    // Composite the shadow with the original texture.
                    return source + destination * (1. - source.a);
                }

                void main(void)
                {
                    gl_FragColor = sourceOver(texture2D(u_contentTexture, v_texCoord), shadowBlurVertical() * u_shadowColor);
                }
            );
            break;
            }
            break;
        }
    default:
        break;
    }

    if (!fragmentShaderSource)
        return;
    Platform3DObject vertexShader = m_context->createShader(GraphicsContext3D::VERTEX_SHADER);
    Platform3DObject fragmentShader = m_context->createShader(GraphicsContext3D::FRAGMENT_SHADER);
    m_context->shaderSource(vertexShader, vertexShaderSource);
    m_context->shaderSource(fragmentShader, fragmentShaderSource);
    Platform3DObject programID = m_context->createProgram();
    m_context->compileShader(vertexShader);
    m_context->compileShader(fragmentShader);
#if !LOG_DISABLED
    String log;
    m_context->getShaderInfoLog(fragmentShader);
    WTFLog(&LogCompositing, "%s\n", log.ascii().data());
#endif
    m_context->attachShader(programID, vertexShader);
    m_context->attachShader(programID, fragmentShader);
    m_context->linkProgram(programID);

    m_vertexAttrib = m_context->getAttribLocation(programID, "a_vertex");
    m_texCoordAttrib = m_context->getAttribLocation(programID, "a_texCoord");
    m_textureUniformLocation = m_context->getUniformLocation(programID, "u_texture");
    switch (type) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
        m_uniformLocations.amount = m_context->getUniformLocation(programID, "u_amount");
        break;
    case FilterOperation::BLUR:
        m_uniformLocations.blur.radius = m_context->getUniformLocation(programID, "u_blurRadius");
        m_uniformLocations.blur.gaussianKernel = m_context->getUniformLocation(programID, "u_gaussianKernel");
        break;
    case FilterOperation::DROP_SHADOW:
        m_uniformLocations.shadow.blurRadius = m_context->getUniformLocation(programID, "u_shadowBlurRadius");
        m_uniformLocations.shadow.gaussianKernel = m_context->getUniformLocation(programID, "u_gaussianKernel");
        if (!pass)
            m_uniformLocations.shadow.offset = m_context->getUniformLocation(programID, "u_shadowOffset");
        else {
            // We only need the color and the content texture in the second pass, the first pass is only a horizontal alpha blur.
            m_uniformLocations.shadow.color = m_context->getUniformLocation(programID, "u_shadowColor");
            m_uniformLocations.shadow.contentTexture = m_context->getUniformLocation(programID, "u_contentTexture");
        }
        break;
    default:
        break;
    }
    m_id = programID;
    m_vertexShader = vertexShader;
    m_fragmentShader = fragmentShader;
}

PassRefPtr<StandardFilterProgram> StandardFilterProgram::create(GraphicsContext3D* context, FilterOperation::OperationType type, unsigned pass)
{
    RefPtr<StandardFilterProgram> program = adoptRef(new StandardFilterProgram(context, type, pass));
    if (!program->m_id)
        return 0;

    return program;
}

static inline float gauss(float x)
{
    return exp(-(x * x) / 2.);
}

static float* gaussianKernel()
{
    static bool prepared = false;
    static float kernel[GAUSSIAN_KERNEL_HALF_WIDTH] = {0, };

    if (prepared)
        return kernel;

    kernel[0] = gauss(0);
    float sum = kernel[0];
    for (unsigned i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; ++i) {
        kernel[i] = gauss(i * GAUSSIAN_KERNEL_STEP);
        sum += 2 * kernel[i];
    }

    // Normalize the kernel
    float scale = 1 / sum;
    for (unsigned i = 0; i < GAUSSIAN_KERNEL_HALF_WIDTH; ++i)
        kernel[i] *= scale;

    prepared = true;
    return kernel;
}

void StandardFilterProgram::prepare(const FilterOperation& operation, unsigned pass, const IntSize& size, GC3Duint contentTexture)
{
    m_context->useProgram(m_id);
    switch (operation.getOperationType()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
        m_context->uniform1f(m_uniformLocations.amount, static_cast<const BasicColorMatrixFilterOperation&>(operation).amount());
        break;
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
        m_context->uniform1f(m_uniformLocations.amount, static_cast<const BasicComponentTransferFilterOperation&>(operation).amount());
        break;
    case FilterOperation::BLUR: {
        const BlurFilterOperation& blur = static_cast<const BlurFilterOperation&>(operation);
        FloatSize radius;

        // Blur is done in two passes, first horizontally and then vertically. The same shader is used for both.
        if (pass)
            radius.setHeight(floatValueForLength(blur.stdDeviation(), size.height()) / size.height());
        else
            radius.setWidth(floatValueForLength(blur.stdDeviation(), size.width()) / size.width());

        m_context->uniform2f(m_uniformLocations.blur.radius, radius.width(), radius.height());
        m_context->uniform1fv(m_uniformLocations.blur.gaussianKernel, GAUSSIAN_KERNEL_HALF_WIDTH, gaussianKernel());
        break;
    }
    case FilterOperation::DROP_SHADOW: {
        const DropShadowFilterOperation& shadow = static_cast<const DropShadowFilterOperation&>(operation);
        switch (pass) {
        case 0:
            // First pass: vertical alpha blur.
            m_context->uniform2f(m_uniformLocations.shadow.offset, float(shadow.location().x()) / float(size.width()), float(shadow.location().y()) / float(size.height()));
            m_context->uniform1f(m_uniformLocations.shadow.blurRadius, shadow.stdDeviation() / float(size.width()));
            m_context->uniform1fv(m_uniformLocations.shadow.gaussianKernel, GAUSSIAN_KERNEL_HALF_WIDTH, gaussianKernel());
            break;
        case 1:
            // Second pass: we need the shadow color and the content texture for compositing.
            m_context->uniform1f(m_uniformLocations.shadow.blurRadius, shadow.stdDeviation() / float(size.height()));
            m_context->uniform1fv(m_uniformLocations.shadow.gaussianKernel, GAUSSIAN_KERNEL_HALF_WIDTH, gaussianKernel());
            m_context->activeTexture(GraphicsContext3D::TEXTURE1);
            m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, contentTexture);
            m_context->uniform1i(m_uniformLocations.shadow.contentTexture, 1);
            float r, g, b, a;
            shadow.color().getRGBA(r, g, b, a);
            m_context->uniform4f(m_uniformLocations.shadow.color, r, g, b, a);
            break;
        }
        break;
    }
    default:
        break;
    }
}

PassRefPtr<StandardFilterProgram> TextureMapperShaderManager::getShaderForFilter(const FilterOperation& filter, unsigned pass)
{
    RefPtr<StandardFilterProgram> program;
    FilterOperation::OperationType type = filter.getOperationType();
    int key = int(type) | (pass << 16);
    FilterMap::iterator iterator = m_filterMap.find(key);
    if (iterator == m_filterMap.end()) {
        program = StandardFilterProgram::create(m_context, type, pass);
        if (!program)
            return 0;

        m_filterMap.add(key, program);
    } else
        program = iterator->value;

    return program;
}

unsigned TextureMapperShaderManager::getPassesRequiredForFilter(const FilterOperation& operation) const
{
    switch (operation.getOperationType()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
        return 1;
    case FilterOperation::BLUR:
    case FilterOperation::DROP_SHADOW:
        // We use two-passes (vertical+horizontal) for blur and drop-shadow.
        return 2;
    default:
        return 0;
    }

}

#endif
};

#endif
