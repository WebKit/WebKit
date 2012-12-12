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

#define STRINGIFY(...) #__VA_ARGS__

namespace WebCore {


static inline bool compositingLogEnabled()
{
#if !LOG_DISABLED
    return LogCompositing.state == WTFLogChannelOn;
#else
    return false;
#endif
}

TextureMapperShaderProgram::TextureMapperShaderProgram(PassRefPtr<GraphicsContext3D> context, const String& vertex, const String& fragment)
    : m_context(context)
{
    m_vertexShader = m_context->createShader(GraphicsContext3D::VERTEX_SHADER);
    m_fragmentShader = m_context->createShader(GraphicsContext3D::FRAGMENT_SHADER);
    m_context->shaderSource(m_vertexShader, vertex);
    m_context->shaderSource(m_fragmentShader, fragment);
    m_id = m_context->createProgram();
    m_context->compileShader(m_vertexShader);
    m_context->compileShader(m_fragmentShader);
    m_context->attachShader(m_id, m_vertexShader);
    m_context->attachShader(m_id, m_fragmentShader);
    m_context->linkProgram(m_id);

    if (!compositingLogEnabled())
        return;

    if (m_context->getError() == GraphicsContext3D::NO_ERROR)
        return;

    String log = m_context->getShaderInfoLog(m_vertexShader);
    LOG(Compositing, "Vertex shader log: %s\n", log.utf8().data());
    log = m_context->getShaderInfoLog(m_fragmentShader);
    LOG(Compositing, "Fragment shader log: %s\n", log.utf8().data());
    log = m_context->getProgramInfoLog(m_id);
    LOG(Compositing, "Program log: %s\n", log.utf8().data());
}

GC3Duint TextureMapperShaderProgram::getLocation(const AtomicString& name, VariableType type)
{
    HashMap<AtomicString, GC3Duint>::iterator it = m_variables.find(name);
    if (it != m_variables.end())
        return it->value;

    GC3Duint location = 0;
    switch (type) {
    case UniformVariable:
        location = m_context->getUniformLocation(m_id, name);
        break;
    case AttribVariable:
        location = m_context->getAttribLocation(m_id, name);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    m_variables.add(name, location);
    return location;
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

struct ShaderSpec {
    String vertexShader;
    String fragmentShader;
    ShaderSpec(const char* vertex = 0, const char* fragment = 0)
        : vertexShader(vertex ? String(ASCIILiteral(vertex)) : String())
        , fragmentShader(fragment ? String(ASCIILiteral(fragment)) : String())
    {
    }
};

static void getShaderSpec(TextureMapperShaderManager::ShaderKey key, String& vertexSource, String& fragmentSource)
{
    static Vector<ShaderSpec> specs = Vector<ShaderSpec>();
    static const char* fragmentOpacityAndMask =
        STRINGIFY(
            precision mediump float;
            uniform sampler2D s_sampler;
            uniform sampler2D s_mask;
            uniform lowp float u_opacity;
            varying highp vec2 v_sourceTexCoord;
            varying highp vec2 v_maskTexCoord;
            void main(void)
            {
                lowp vec4 color = texture2D(s_sampler, v_sourceTexCoord);
                lowp vec4 maskColor = texture2D(s_mask, v_maskTexCoord);
                lowp float fragmentAlpha = u_opacity * maskColor.a;
                gl_FragColor = vec4(color.rgb * fragmentAlpha, color.a * fragmentAlpha);
            }
        );

    static const char* fragmentRectOpacityAndMask =
        STRINGIFY(
            precision mediump float;
            uniform sampler2DRect s_sampler;
            uniform sampler2DRect s_mask;
            uniform lowp float u_opacity;
            varying highp vec2 v_sourceTexCoord;
            varying highp vec2 v_maskTexCoord;
            void main(void)
            {
                lowp vec4 color = texture2DRect(s_sampler, v_sourceTexCoord);
                lowp vec4 maskColor = texture2DRect(s_mask, v_maskTexCoord);
                lowp float fragmentAlpha = u_opacity * maskColor.a;
                gl_FragColor = vec4(color.rgb * fragmentAlpha, color.a * fragmentAlpha);
            }
        );

    static const char* vertexOpacityAndMask =
        STRINGIFY(
            uniform mat4 u_matrix;
            uniform lowp float u_flip;
            uniform lowp vec2 u_textureSize;
            attribute vec4 a_vertex;
            varying highp vec2 v_sourceTexCoord;
            varying highp vec2 v_maskTexCoord;
            void main(void)
            {
                v_sourceTexCoord = vec2(a_vertex.x, mix(a_vertex.y, 1. - a_vertex.y, u_flip)) * u_textureSize;
                v_maskTexCoord = vec2(a_vertex);
                gl_Position = u_matrix * a_vertex;
            }
        );

    static const char* fragmentSimple =
        STRINGIFY(
            precision mediump float;
            uniform sampler2D s_sampler;
            uniform lowp float u_opacity;
            varying highp vec2 v_sourceTexCoord;
            void main(void)
            {
                lowp vec4 color = texture2D(s_sampler, v_sourceTexCoord);
                gl_FragColor = vec4(color.rgb * u_opacity, color.a * u_opacity);
            }
        );

    static const char* fragmentAntialiasingNoMask =
        STRINGIFY(
            precision mediump float;
            uniform sampler2D s_sampler;
            varying highp vec2 v_sourceTexCoord;
            uniform lowp float u_opacity;
            uniform vec3 u_expandedQuadEdgesInScreenSpace[8];

                // The data passed in u_expandedQuadEdgesInScreenSpace is merely the
                // pre-scaled coeffecients of the line equations describing the four edges
                // of the expanded quad in screen space and the rectangular bounding box
                // of the expanded quad.
                //
            float normalizedDistance(vec3 edgeCoefficient)
            {
                // We are doing a simple distance calculation here according to the formula:
                // (A*p.x + B*p.y + C) / sqrt(A^2 + B^2) = distance from line to p
                // Note that A, B and C have already been scaled by 1 / sqrt(A^2 + B^2).
                return clamp(dot(edgeCoefficient, vec3(gl_FragCoord.xy, 1.)), 0., 1.);
            }

            float antialiasQuad(vec3 coefficient1, vec3 coefficient2, vec3 coefficient3, vec3 coefficient4)
            {
                // Now we want to reduce the alpha value of the fragment if it is close to the
                // edges of the expanded quad (or rectangular bounding box -- which seems to be
                // important for backfacing quads). Note that we are combining the contribution
                // from the (top || bottom) and (left || right) edge by simply multiplying. This follows
                // the approach described at: http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter22.html,
                // in this case without using Gaussian weights.
                return min(normalizedDistance(coefficient1), normalizedDistance(coefficient2))
                    * min(normalizedDistance(coefficient3), normalizedDistance(coefficient4));
            }

            float antialias()
            {
                float antialiasValueForQuad = 
                    antialiasQuad(u_expandedQuadEdgesInScreenSpace[0],
                        u_expandedQuadEdgesInScreenSpace[1],
                        u_expandedQuadEdgesInScreenSpace[2],
                        u_expandedQuadEdgesInScreenSpace[3]);

                float antialiasValueForBoundingRect = 
                    antialiasQuad(u_expandedQuadEdgesInScreenSpace[4],
                        u_expandedQuadEdgesInScreenSpace[5],
                        u_expandedQuadEdgesInScreenSpace[6],
                        u_expandedQuadEdgesInScreenSpace[7]);

                return min(antialiasValueForQuad, antialiasValueForBoundingRect);           
            }

            void main()
            {
                vec4 sampledColor = texture2D(s_sampler, clamp(v_sourceTexCoord, 0.0, 1.0));
                gl_FragColor = sampledColor * u_opacity * antialias();
            }
        );

    static const char* fragmentRectSimple =
        STRINGIFY(
            precision mediump float;
            uniform sampler2DRect s_sampler;
            uniform lowp float u_opacity;
            varying highp vec2 v_sourceTexCoord;
            void main(void)
            {
                lowp vec4 color = texture2DRect(s_sampler, v_sourceTexCoord);
                gl_FragColor = vec4(color.rgb * u_opacity, color.a * u_opacity);
            }
        );

    static const char* vertexSimple =
        STRINGIFY(
            uniform mat4 u_matrix;
            uniform lowp float u_flip;
            uniform lowp vec2 u_textureSize;
            attribute vec4 a_vertex;
            varying highp vec2 v_sourceTexCoord;
            void main(void)
            {
                v_sourceTexCoord = vec2(a_vertex.x, mix(a_vertex.y, 1. - a_vertex.y, u_flip)) * u_textureSize;
                gl_Position = u_matrix * a_vertex;
            }
        );

    static const char* vertexSolidColor =
        STRINGIFY(
            uniform mat4 u_matrix;
            attribute vec4 a_vertex;
            void main(void)
            {
                gl_Position = u_matrix * a_vertex;
            }
        );


    static const char* fragmentSolidColor =
        STRINGIFY(
            precision mediump float;
            uniform vec4 u_color;
            void main(void)
            {
                gl_FragColor = u_color;
            }
        );

    static const char* vertexFilter =
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
        "uniform sampler2D s_sampler;\n"#__VA_ARGS__ \
        "void main(void)\n { gl_FragColor = shade(texture2D(s_sampler, v_texCoord)); }"

    static const char* fragmentGrayscaleFilter =
        STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                lowp float amount = 1.0 - u_amount;
                return vec4((0.2126 + 0.7874 * amount) * color.r + (0.7152 - 0.7152 * amount) * color.g + (0.0722 - 0.0722 * amount) * color.b,
                            (0.2126 - 0.2126 * amount) * color.r + (0.7152 + 0.2848 * amount) * color.g + (0.0722 - 0.0722 * amount) * color.b,
                            (0.2126 - 0.2126 * amount) * color.r + (0.7152 - 0.7152 * amount) * color.g + (0.0722 + 0.9278 * amount) * color.b,
                            color.a);
            }
        );

    static const char* fragmentSepiaFilter =
        STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                lowp float amount = 1.0 - u_amount;
                return vec4((0.393 + 0.607 * amount) * color.r + (0.769 - 0.769 * amount) * color.g + (0.189 - 0.189 * amount) * color.b,
                            (0.349 - 0.349 * amount) * color.r + (0.686 + 0.314 * amount) * color.g + (0.168 - 0.168 * amount) * color.b,
                            (0.272 - 0.272 * amount) * color.r + (0.534 - 0.534 * amount) * color.g + (0.131 + 0.869 * amount) * color.b,
                            color.a);
            }
        );

    static const char* fragmentSaturateFilter =
        STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4((0.213 + 0.787 * u_amount) * color.r + (0.715 - 0.715 * u_amount) * color.g + (0.072 - 0.072 * u_amount) * color.b,
                            (0.213 - 0.213 * u_amount) * color.r + (0.715 + 0.285 * u_amount) * color.g + (0.072 - 0.072 * u_amount) * color.b,
                            (0.213 - 0.213 * u_amount) * color.r + (0.715 - 0.715 * u_amount) * color.g + (0.072 + 0.928 * u_amount) * color.b,
                            color.a);
            }
        );

    static const char* fragmentHueRotateFilter =
        STANDARD_FILTER(
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

    static const char* fragmentInvertFilter =
        STANDARD_FILTER(
            lowp float invert(lowp float n) { return (1.0 - n) * u_amount + n * (1.0 - u_amount); }
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(invert(color.r), invert(color.g), invert(color.b), color.a);
            }
        );

    static const char* fragmentBrightnessFilter =
        STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(color.rgb * (1.0 + u_amount), color.a);
            }
        );

    static const char* fragmentContrastFilter =
        STANDARD_FILTER(
            lowp float contrast(lowp float n) { return (n - 0.5) * u_amount + 0.5; }
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(contrast(color.r), contrast(color.g), contrast(color.b), color.a);
            }
        );

    static const char* fragmentOpacityFilter =
        STANDARD_FILTER(
            lowp vec4 shade(lowp vec4 color)
            {
                return vec4(color.r, color.g, color.b, color.a * u_amount);
            }
        );

#define BLUR_CONSTANTS "#define GAUSSIAN_KERNEL_HALF_WIDTH 11\n#define GAUSSIAN_KERNEL_STEP 0.2\n"

    static const char* fragmentBlurFilter =
        BLUR_CONSTANTS
        STRINGIFY(
            // Create a normal distribution of 21 values between -2 and 2.
            precision mediump float;
            varying highp vec2 v_texCoord;
            uniform lowp vec2 u_blurRadius;
            uniform sampler2D s_sampler;
            uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];

            lowp vec4 sampleColor(float radius)
            {
                vec2 coord = v_texCoord + radius * u_blurRadius;
                return texture2D(s_sampler, coord) * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
            }

            vec4 blur()
            {
                vec4 total = sampleColor(0.) * u_gaussianKernel[0];
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

    static const char* fragmentShadowFilter1 =
        BLUR_CONSTANTS
        STRINGIFY(
            precision mediump float;
            varying highp vec2 v_texCoord;
            uniform lowp float u_blurRadius;
            uniform lowp vec2 u_shadowOffset;
            uniform sampler2D s_sampler;
            uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];

            lowp float sampleAlpha(float radius)
            {
                vec2 coord = v_texCoord - u_shadowOffset + vec2(radius * u_blurRadius, 0.);
                return texture2D(s_sampler, coord).a * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
            }

            lowp float shadowBlurHorizontal()
            {
                float total = sampleAlpha(0.) * u_gaussianKernel[0];
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

    // Second pass: vertical alpha blur and composite with origin.
    static const char* fragmentShadowFilter2 =
        BLUR_CONSTANTS
        STRINGIFY(
            precision mediump float;
            varying highp vec2 v_texCoord;
            uniform lowp float u_blurRadius;
            uniform lowp vec4 u_shadowColor;
            uniform sampler2D s_sampler;
            uniform sampler2D s_contentTexture;
            uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];

            lowp float sampleAlpha(float r)
            {
                vec2 coord = v_texCoord + vec2(0., r * u_blurRadius);
                return texture2D(s_sampler, coord).a * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
            }

            lowp float shadowBlurVertical()
            {
                float total = sampleAlpha(0.) * u_gaussianKernel[0];
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
                gl_FragColor = sourceOver(texture2D(s_contentTexture, v_texCoord), shadowBlurVertical() * u_shadowColor);
            }
        );

    if (specs.isEmpty()) {
        specs.resize(TextureMapperShaderManager::LastFilter);
        specs[TextureMapperShaderManager::Default] = ShaderSpec(vertexSimple, fragmentSimple);
        specs[TextureMapperShaderManager::SolidColor] = ShaderSpec(vertexSolidColor, fragmentSolidColor);
        specs[TextureMapperShaderManager::Rect] = ShaderSpec(vertexSimple, fragmentRectSimple);
        specs[TextureMapperShaderManager::Masked] = ShaderSpec(vertexOpacityAndMask, fragmentOpacityAndMask);
        specs[TextureMapperShaderManager::MaskedRect] = ShaderSpec(vertexOpacityAndMask, fragmentRectOpacityAndMask);
        specs[TextureMapperShaderManager::Antialiased] = ShaderSpec(vertexSimple, fragmentAntialiasingNoMask);
        specs[TextureMapperShaderManager::GrayscaleFilter] = ShaderSpec(vertexFilter, fragmentGrayscaleFilter);
        specs[TextureMapperShaderManager::SepiaFilter] = ShaderSpec(vertexFilter, fragmentSepiaFilter);
        specs[TextureMapperShaderManager::SaturateFilter] = ShaderSpec(vertexFilter, fragmentSaturateFilter);
        specs[TextureMapperShaderManager::HueRotateFilter] = ShaderSpec(vertexFilter, fragmentHueRotateFilter);
        specs[TextureMapperShaderManager::BrightnessFilter] = ShaderSpec(vertexFilter, fragmentBrightnessFilter);
        specs[TextureMapperShaderManager::ContrastFilter] = ShaderSpec(vertexFilter, fragmentContrastFilter);
        specs[TextureMapperShaderManager::InvertFilter] = ShaderSpec(vertexFilter, fragmentInvertFilter);
        specs[TextureMapperShaderManager::OpacityFilter] = ShaderSpec(vertexFilter, fragmentOpacityFilter);
        specs[TextureMapperShaderManager::BlurFilter] = ShaderSpec(vertexFilter, fragmentBlurFilter);
        specs[TextureMapperShaderManager::ShadowFilterPass1] = ShaderSpec(vertexFilter, fragmentShadowFilter1);
        specs[TextureMapperShaderManager::ShadowFilterPass2] = ShaderSpec(vertexFilter, fragmentShadowFilter2);
    }

    ASSERT(specs.size() > key);
    ShaderSpec& spec = specs[key];
    vertexSource = spec.vertexShader;
    fragmentSource = spec.fragmentShader;
}

TextureMapperShaderManager::TextureMapperShaderManager(GraphicsContext3D* context)
    : m_context(context)
{
}

TextureMapperShaderManager::~TextureMapperShaderManager()
{
}

PassRefPtr<TextureMapperShaderProgram> TextureMapperShaderManager::getShaderProgram(ShaderKey key)
{
    TextureMapperShaderProgramMap::iterator it = m_programs.find(key);
    if (it != m_programs.end())
        return it->value;

    String vertexShader;
    String fragmentShader;
    getShaderSpec(key, vertexShader, fragmentShader);
    RefPtr<TextureMapperShaderProgram> program = TextureMapperShaderProgram::create(m_context, vertexShader, fragmentShader);
    m_programs.add(key, program);
    return program;
}
};

#endif
