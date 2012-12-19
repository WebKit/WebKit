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

#include <wtf/text/StringBuilder.h>

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

static const char* vertexShaderSource =
    STRINGIFY(
        uniform mat4 u_matrix;
        uniform float u_flip;
        uniform vec2 u_textureSize;
        attribute vec4 a_vertex;
        varying vec2 v_texCoord;
        varying vec2 v_maskTexCoord;

        void main(void)
        {
            vec4 position = a_vertex;
            v_texCoord = vec2(position.x, mix(position.y, 1. - position.y, u_flip)) * u_textureSize;
            v_maskTexCoord = vec2(position);
            gl_Position = u_matrix * a_vertex;
        }
    );

#define GLSL_DIRECTIVE(...) "#"#__VA_ARGS__"\n"
#define DEFINE_APPLIER(Name) \
    "#ifdef ENABLE_"#Name"\n" \
    "#define apply"#Name"IfNeeded apply"#Name"\n"\
    "#else\n"\
    "#define apply"#Name"IfNeeded noop\n"\
    "#endif\n"

#define RECT_TEXTURE_DIRECTIVE \
    GLSL_DIRECTIVE(ifdef ENABLE_Rect) \
        GLSL_DIRECTIVE(define SamplerType sampler2DRect) \
        GLSL_DIRECTIVE(define SamplerFunction texture2DRect) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(define SamplerType sampler2D) \
        GLSL_DIRECTIVE(define SamplerFunction texture2D) \
    GLSL_DIRECTIVE(endif)

#define ENABLE_APPLIER(Name) "#define ENABLE_"#Name"\n"
#define BLUR_CONSTANTS \
    GLSL_DIRECTIVE(define GAUSSIAN_KERNEL_HALF_WIDTH 11) \
    GLSL_DIRECTIVE(define GAUSSIAN_KERNEL_STEP 0.2)


static const char* fragmentTemplate =
    DEFINE_APPLIER(Texture)
    DEFINE_APPLIER(SolidColor)
    DEFINE_APPLIER(Opacity)
    DEFINE_APPLIER(Mask)
    DEFINE_APPLIER(Antialias)
    DEFINE_APPLIER(GrayscaleFilter)
    DEFINE_APPLIER(SepiaFilter)
    DEFINE_APPLIER(SaturateFilter)
    DEFINE_APPLIER(HueRotateFilter)
    DEFINE_APPLIER(InvertFilter)
    DEFINE_APPLIER(BrightnessFilter)
    DEFINE_APPLIER(ContrastFilter)
    DEFINE_APPLIER(OpacityFilter)
    DEFINE_APPLIER(BlurFilter)
    DEFINE_APPLIER(AlphaBlur)
    DEFINE_APPLIER(ContentTexture)
    RECT_TEXTURE_DIRECTIVE
    BLUR_CONSTANTS
    STRINGIFY(
        precision mediump float;
        uniform SamplerType s_sampler;
        uniform sampler2D s_mask;
        uniform sampler2D s_contentTexture;
        uniform float u_opacity;
        varying vec2 v_texCoord;
        varying vec2 v_maskTexCoord;
        uniform vec3 u_expandedQuadEdgesInScreenSpace[8];
        uniform float u_filterAmount;
        uniform vec2 u_blurRadius;
        uniform vec2 u_shadowOffset;
        uniform vec4 u_color;
        uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];

        void noop(vec4) { }

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

        void applyTexture(inout vec4 color) { color = SamplerFunction(s_sampler, v_texCoord); }
        void applyOpacity(inout vec4 color) { color *= u_opacity; }
        void applyAntialias(inout vec4 color) { color *= antialias(); }
        void applyMask(inout vec4 color) { color *= texture2D(s_mask, v_maskTexCoord).a; }

        void applyGrayscaleFilter(inout vec4 color)
        {
            float amount = 1.0 - u_filterAmount;
            color = vec4((0.2126 + 0.7874 * amount) * color.r + (0.7152 - 0.7152 * amount) * color.g + (0.0722 - 0.0722 * amount) * color.b,
                (0.2126 - 0.2126 * amount) * color.r + (0.7152 + 0.2848 * amount) * color.g + (0.0722 - 0.0722 * amount) * color.b,
                (0.2126 - 0.2126 * amount) * color.r + (0.7152 - 0.7152 * amount) * color.g + (0.0722 + 0.9278 * amount) * color.b,
                color.a);
        }

        void applySepiaFilter(inout vec4 color)
        {
            float amount = 1.0 - u_filterAmount;
            color = vec4((0.393 + 0.607 * amount) * color.r + (0.769 - 0.769 * amount) * color.g + (0.189 - 0.189 * amount) * color.b,
                (0.349 - 0.349 * amount) * color.r + (0.686 + 0.314 * amount) * color.g + (0.168 - 0.168 * amount) * color.b,
                (0.272 - 0.272 * amount) * color.r + (0.534 - 0.534 * amount) * color.g + (0.131 + 0.869 * amount) * color.b,
                color.a);
        }

        void applySaturateFilter(inout vec4 color)
        {
            color = vec4((0.213 + 0.787 * u_filterAmount) * color.r + (0.715 - 0.715 * u_filterAmount) * color.g + (0.072 - 0.072 * u_filterAmount) * color.b,
                (0.213 - 0.213 * u_filterAmount) * color.r + (0.715 + 0.285 * u_filterAmount) * color.g + (0.072 - 0.072 * u_filterAmount) * color.b,
                (0.213 - 0.213 * u_filterAmount) * color.r + (0.715 - 0.715 * u_filterAmount) * color.g + (0.072 + 0.928 * u_filterAmount) * color.b,
                color.a);
        }

        void applyHueRotateFilter(inout vec4 color)
        {
            float pi = 3.14159265358979323846;
            float c = cos(u_filterAmount * pi / 180.0);
            float s = sin(u_filterAmount * pi / 180.0);
            color = vec4(color.r * (0.213 + c * 0.787 - s * 0.213) + color.g * (0.715 - c * 0.715 - s * 0.715) + color.b * (0.072 - c * 0.072 + s * 0.928),
                color.r * (0.213 - c * 0.213 + s * 0.143) + color.g * (0.715 + c * 0.285 + s * 0.140) + color.b * (0.072 - c * 0.072 - s * 0.283),
                color.r * (0.213 - c * 0.213 - s * 0.787) +  color.g * (0.715 - c * 0.715 + s * 0.715) + color.b * (0.072 + c * 0.928 + s * 0.072),
                color.a);
        }

        float invert(float n) { return (1.0 - n) * u_filterAmount + n * (1.0 - u_filterAmount); }
        void applyInvertFilter(inout vec4 color)
        {
            color = vec4(invert(color.r), invert(color.g), invert(color.b), color.a);
        }

        void applyBrightnessFilter(inout vec4 color)
        {
            color = vec4(color.rgb * (1.0 + u_filterAmount), color.a);
        }

        float contrast(float n) { return (n - 0.5) * u_filterAmount + 0.5; }
        void applyContrastFilter(inout vec4 color)
        {
            color = vec4(contrast(color.r), contrast(color.g), contrast(color.b), color.a);
        }

        void applyOpacityFilter(inout vec4 color)
        {
            color = vec4(color.r, color.g, color.b, color.a * u_filterAmount);
        }

        vec4 sampleColorAtRadius(float radius)
        {
            vec2 coord = v_texCoord + radius * u_blurRadius;
            return SamplerFunction(s_sampler, coord) * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
        }

        float sampleAlphaAtRadius(float radius)
        {
            vec2 coord = v_texCoord - u_shadowOffset + radius * u_blurRadius;
            return SamplerFunction(s_sampler, coord).a * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
        }

        void applyBlurFilter(inout vec4 color)
        {
            vec4 total = sampleColorAtRadius(0.) * u_gaussianKernel[0];
            for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
                total += sampleColorAtRadius(float(i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                total += sampleColorAtRadius(float(-1 * i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
            }

            color = total;
        }

        void applyAlphaBlur(inout vec4 color)
        {
            float total = sampleAlphaAtRadius(0.) * u_gaussianKernel[0];
            for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
                total += sampleAlphaAtRadius(float(i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
                total += sampleAlphaAtRadius(float(-1 * i) * GAUSSIAN_KERNEL_STEP) * u_gaussianKernel[i];
            }

            color *= total;
        }

        vec4 sourceOver(vec4 src, vec4 dst) { return src + dst * (1. - dst.a); }

        void applyContentTexture(inout vec4 color)
        {
            vec4 contentColor = texture2D(s_contentTexture, v_texCoord);
            color = sourceOver(contentColor, color);
        }

        void applySolidColor(inout vec4 color)
        {
            color *= u_color;
        }

        void main(void)
        {
            vec4 color = vec4(1., 1., 1., 1.);
            applyTextureIfNeeded(color);
            applySolidColorIfNeeded(color);
            applyAntialiasIfNeeded(color);
            applyMaskIfNeeded(color);
            applyOpacityIfNeeded(color);
            applyGrayscaleFilterIfNeeded(color);
            applySepiaFilterIfNeeded(color);
            applySaturateFilterIfNeeded(color);
            applyHueRotateFilterIfNeeded(color);
            applyInvertFilterIfNeeded(color);
            applyBrightnessFilterIfNeeded(color);
            applyContrastFilterIfNeeded(color);
            applyOpacityFilterIfNeeded(color);
            applyBlurFilterIfNeeded(color);
            applyAlphaBlurIfNeeded(color);
            applyContentTextureIfNeeded(color);
            gl_FragColor = color;
        }
    );


static void getShaderSpec(TextureMapperShaderManager::Options options, String& vertexSource, String& fragmentSource)
{
    StringBuilder fragmentBuilder;
#define SET_APPLIER_FROM_OPTIONS(Applier) \
    if (options & TextureMapperShaderManager::Applier) \
        fragmentBuilder.append(ENABLE_APPLIER(Applier));

    SET_APPLIER_FROM_OPTIONS(Matrix)
    SET_APPLIER_FROM_OPTIONS(Texture)
    SET_APPLIER_FROM_OPTIONS(Rect)
    SET_APPLIER_FROM_OPTIONS(SolidColor)
    SET_APPLIER_FROM_OPTIONS(Opacity)
    SET_APPLIER_FROM_OPTIONS(Mask)
    SET_APPLIER_FROM_OPTIONS(Antialias)
    SET_APPLIER_FROM_OPTIONS(GrayscaleFilter)
    SET_APPLIER_FROM_OPTIONS(SepiaFilter)
    SET_APPLIER_FROM_OPTIONS(SaturateFilter)
    SET_APPLIER_FROM_OPTIONS(HueRotateFilter)
    SET_APPLIER_FROM_OPTIONS(BrightnessFilter)
    SET_APPLIER_FROM_OPTIONS(ContrastFilter)
    SET_APPLIER_FROM_OPTIONS(InvertFilter)
    SET_APPLIER_FROM_OPTIONS(OpacityFilter)
    SET_APPLIER_FROM_OPTIONS(BlurFilter)
    SET_APPLIER_FROM_OPTIONS(AlphaBlur)
    SET_APPLIER_FROM_OPTIONS(ContentTexture)

    fragmentBuilder.append(fragmentTemplate);
    vertexSource = vertexShaderSource;
    fragmentSource = fragmentBuilder.toString();
}

TextureMapperShaderManager::TextureMapperShaderManager(GraphicsContext3D* context)
    : m_context(context)
{
}

TextureMapperShaderManager::~TextureMapperShaderManager()
{
}

PassRefPtr<TextureMapperShaderProgram> TextureMapperShaderManager::getShaderProgram(Options options)
{
    TextureMapperShaderProgramMap::iterator it = m_programs.find(options);
    if (it != m_programs.end())
        return it->value;

    String vertexShader;
    String fragmentShader;
    getShaderSpec(options, vertexShader, fragmentShader);
    RefPtr<TextureMapperShaderProgram> program = TextureMapperShaderProgram::create(m_context, vertexShader, fragmentShader);
    m_programs.add(options, program);
    return program;
}
};

#endif
