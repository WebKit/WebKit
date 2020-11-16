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
#include "TextureMapperShaderProgram.h"

#if USE(TEXTURE_MAPPER_GL)

#include "GLContext.h"
#include "Logging.h"
#include "TextureMapperGL.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline bool compositingLogEnabled()
{
#if !LOG_DISABLED
    return LogCompositing.state == WTFLogChannelState::On;
#else
    return false;
#endif
}

#define STRINGIFY(...) #__VA_ARGS__

#define GLSL_DIRECTIVE(...) "#"#__VA_ARGS__"\n"

#define TEXTURE_SPACE_MATRIX_PRECISION_DIRECTIVE \
    GLSL_DIRECTIVE(ifdef GL_FRAGMENT_PRECISION_HIGH) \
        GLSL_DIRECTIVE(define TextureSpaceMatrixPrecision highp) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(define TextureSpaceMatrixPrecision mediump) \
    GLSL_DIRECTIVE(endif)


// Input/output variables definition for both GLES and OpenGL < 3.2.
// The default precision directive is only needed for GLES.
static const char* vertexTemplateLT320Vars =
#if USE(OPENGL_ES)
    TEXTURE_SPACE_MATRIX_PRECISION_DIRECTIVE
#endif
#if USE(OPENGL_ES)
    STRINGIFY(
        precision TextureSpaceMatrixPrecision float;
    )
#endif
    STRINGIFY(
        attribute vec4 a_vertex;
        varying vec2 v_texCoord;
        varying vec2 v_transformedTexCoord;
        varying float v_antialias;
        varying vec4 v_nonProjectedPosition;
    );

#if !USE(OPENGL_ES)
// Input/output variables definition for OpenGL >= 3.2.
static const char* vertexTemplateGE320Vars =
    STRINGIFY(
        in vec4 a_vertex;
        out vec2 v_texCoord;
        out vec2 v_transformedTexCoord;
        out float v_antialias;
        out vec4 v_nonProjectedPosition;
    );
#endif

static const char* vertexTemplateCommon =
    STRINGIFY(
        uniform mat4 u_modelViewMatrix;
        uniform mat4 u_projectionMatrix;
        uniform mat4 u_textureSpaceMatrix;

        void noop(inout vec2 dummyParameter) { }

        vec4 toViewportSpace(vec2 pos) { return vec4(pos, 0., 1.) * u_modelViewMatrix; }

        // This function relies on the assumption that we get edge triangles with control points,
        // a control point being the nearest point to the coordinate that is on the edge.
        void applyAntialiasing(inout vec2 position)
        {
            // We count on the fact that quad passed in is always a unit rect,
            // and the transformation matrix applies the real rect.
            const vec2 center = vec2(0.5, 0.5);
            const float antialiasInflationDistance = 1.;

            // We pass the control point as the zw coordinates of the vertex.
            // The control point is the point on the edge closest to the current position.
            // The control point is used to compute the antialias value.
            vec2 controlPoint = a_vertex.zw;

            // First we calculate the distance in viewport space.
            vec4 centerInViewportCoordinates = toViewportSpace(center);
            vec4 controlPointInViewportCoordinates = toViewportSpace(controlPoint);
            float viewportSpaceDistance = distance(centerInViewportCoordinates, controlPointInViewportCoordinates);

            // We add the inflation distance to the computed distance, and compute the ratio.
            float inflationRatio = (viewportSpaceDistance + antialiasInflationDistance) / viewportSpaceDistance;

            // v_antialias needs to be 0 for the outer edge and 1. for the inner edge.
            // Since the controlPoint is equal to the position in the edge vertices, the value is always 0 for those.
            // For the center point, the distance is always 0.5, so we normalize to 1. by multiplying by 2.
            // By multplying by inflationRatio and dividing by (inflationRatio - 1),
            // We make sure that the varying interpolates between 0 (outer edge), 1 (inner edge) and n > 1 (center).
            v_antialias = distance(controlPoint, position) * 2. * inflationRatio / (inflationRatio - 1.);

            // Now inflate the actual position. By using this formula instead of inflating position directly,
            // we ensure that the center vertex is never inflated.
            position = center + (position - center) * inflationRatio;
        }

        void main(void)
        {
            vec2 position = a_vertex.xy;
            applyAntialiasingIfNeeded(position);

            v_texCoord = position;
            vec4 clampedPosition = clamp(vec4(position, 0., 1.), 0., 1.);
            v_transformedTexCoord = (u_textureSpaceMatrix * clampedPosition).xy;
            v_nonProjectedPosition = u_modelViewMatrix * vec4(position, 0., 1.);
            gl_Position = u_projectionMatrix * v_nonProjectedPosition;
        }
    );

#define RECT_TEXTURE_DIRECTIVE \
    GLSL_DIRECTIVE(ifdef ENABLE_Rect) \
        GLSL_DIRECTIVE(define SamplerType sampler2DRect) \
        GLSL_DIRECTIVE(define SamplerFunction texture2DRect) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(define SamplerType sampler2D) \
        GLSL_DIRECTIVE(define SamplerFunction texture2D) \
    GLSL_DIRECTIVE(endif)

#define ANTIALIASING_TEX_COORD_DIRECTIVE \
    GLSL_DIRECTIVE(if defined(ENABLE_Antialiasing) && defined(ENABLE_Texture)) \
        GLSL_DIRECTIVE(define transformTexCoord fragmentTransformTexCoord) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(define transformTexCoord vertexTransformTexCoord) \
    GLSL_DIRECTIVE(endif)

#define ENABLE_APPLIER(Name) "#define ENABLE_"#Name"\n#define apply"#Name"IfNeeded apply"#Name"\n"
#define DISABLE_APPLIER(Name) "#define apply"#Name"IfNeeded noop\n"
#define BLUR_CONSTANTS \
    GLSL_DIRECTIVE(define GAUSSIAN_KERNEL_HALF_WIDTH 11) \
    GLSL_DIRECTIVE(define GAUSSIAN_KERNEL_STEP 0.2)


#define OES_EGL_IMAGE_EXTERNAL_DIRECTIVE \
    GLSL_DIRECTIVE(ifdef ENABLE_TextureExternalOES) \
        GLSL_DIRECTIVE(extension GL_OES_EGL_image_external : require) \
        GLSL_DIRECTIVE(define SamplerExternalOESType samplerExternalOES) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(define SamplerExternalOESType sampler2D) \
    GLSL_DIRECTIVE(endif)

// The max number of stacked rounded rectangle clips allowed is 10, which is also the
// max number of transforms that we can get. We need 3 components for each rounded
// rectangle so we need 30 components to receive the 10 rectangles.
//
// Keep this is sync with the values defined in ClipStack.h
#define ROUNDED_RECT_CONSTANTS                       \
    GLSL_DIRECTIVE(define ROUNDED_RECT_MAX_RECTS 10) \
    GLSL_DIRECTIVE(define ROUNDED_RECT_ARRAY_SIZE 30) \
    GLSL_DIRECTIVE(define ROUNDED_RECT_INVERSE_TRANSFORM_ARRAY_SIZE 10)

// Common header for all versions. We define the matrices variables here to keep the precision
// directives scope: the first one applies to the matrices variables and the next one to the
// rest of them. The precision is only used in GLES.
static const char* fragmentTemplateHeaderCommon =
    RECT_TEXTURE_DIRECTIVE
    ANTIALIASING_TEX_COORD_DIRECTIVE
    BLUR_CONSTANTS
    ROUNDED_RECT_CONSTANTS
    OES_EGL_IMAGE_EXTERNAL_DIRECTIVE
#if USE(OPENGL_ES)
    TEXTURE_SPACE_MATRIX_PRECISION_DIRECTIVE
    STRINGIFY(
        precision TextureSpaceMatrixPrecision float;
    )
#endif
    STRINGIFY(
        uniform mat4 u_textureSpaceMatrix;
        uniform mat4 u_textureColorSpaceMatrix;
    )
#if USE(OPENGL_ES)
    STRINGIFY(
        precision mediump float;
    )
#endif
    ;

// Input/output variables definition for both GLES and OpenGL < 3.2.
static const char* fragmentTemplateLT320Vars =
    STRINGIFY(
        varying float v_antialias;
        varying vec2 v_texCoord;
        varying vec2 v_transformedTexCoord;
        varying vec4 v_nonProjectedPosition;
    );

#if !USE(OPENGL_ES)
// Input/output variables definition for OpenGL >= 3.2.
static const char* fragmentTemplateGE320Vars =
    STRINGIFY(
        in float v_antialias;
        in vec2 v_texCoord;
        in vec2 v_transformedTexCoord;
        in vec4 v_nonProjectedPosition;
    );
#endif

static const char* fragmentTemplateCommon =
    STRINGIFY(
        uniform SamplerType s_sampler;
        uniform SamplerType s_samplerY;
        uniform SamplerType s_samplerU;
        uniform SamplerType s_samplerV;
        uniform sampler2D s_contentTexture;
        uniform SamplerExternalOESType s_externalOESTexture;
        uniform float u_opacity;
        uniform float u_filterAmount;
        uniform mat3 u_yuvToRgb;
        uniform vec2 u_blurRadius;
        uniform vec2 u_shadowOffset;
        uniform vec4 u_color;
        uniform float u_gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];
        uniform int u_roundedRectNumber;
        uniform vec4 u_roundedRect[ROUNDED_RECT_ARRAY_SIZE];
        uniform mat4 u_roundedRectInverseTransformMatrix[ROUNDED_RECT_INVERSE_TRANSFORM_ARRAY_SIZE];

        void noop(inout vec4 dummyParameter) { }
        void noop(inout vec4 dummyParameter, vec2 texCoord) { }
        void noop(inout vec2 dummyParameter) { }

        float antialias() { return smoothstep(0., 1., v_antialias); }

        vec2 fragmentTransformTexCoord()
        {
            vec4 clampedPosition = clamp(vec4(v_texCoord, 0., 1.), 0., 1.);
            return (u_textureSpaceMatrix * clampedPosition).xy;
        }

        vec2 vertexTransformTexCoord() { return v_transformedTexCoord; }

        void applyManualRepeat(inout vec2 pos) { pos = fract(pos); }

        void applyTextureRGB(inout vec4 color, vec2 texCoord) { color = u_textureColorSpaceMatrix * SamplerFunction(s_sampler, texCoord); }

        vec3 yuvToRgb(float y, float u, float v)
        {
            // yuv is either bt601 or bt709 so the offset is the same
            vec3 yuv = vec3(y - 0.0625, u - 0.5, v - 0.5);
            return yuv * u_yuvToRgb;
        }
        void applyTextureYUV(inout vec4 color, vec2 texCoord)
        {
            float y = SamplerFunction(s_samplerY, texCoord).r;
            float u = SamplerFunction(s_samplerU, texCoord).r;
            float v = SamplerFunction(s_samplerV, texCoord).r;
            vec4 data = vec4(yuvToRgb(y, u, v), 1.0);
            color = u_textureColorSpaceMatrix * data;
        }
        void applyTextureNV12(inout vec4 color, vec2 texCoord)
        {
            float y = SamplerFunction(s_samplerY, texCoord).r;
            vec2 uv = SamplerFunction(s_samplerU, texCoord).rg;
            vec4 data = vec4(yuvToRgb(y, uv.x, uv.y), 1.0);
            color = u_textureColorSpaceMatrix * data;
        }
        void applyTextureNV21(inout vec4 color, vec2 texCoord)
        {
            float y = SamplerFunction(s_samplerY, texCoord).r;
            vec2 uv = SamplerFunction(s_samplerU, texCoord).gr;
            vec4 data = vec4(yuvToRgb(y, uv.x, uv.y), 1.0);
            color = u_textureColorSpaceMatrix * data;
        }
        void applyTexturePackedYUV(inout vec4 color, vec2 texCoord)
        {
            vec4 data = SamplerFunction(s_sampler, texCoord);
            color = u_textureColorSpaceMatrix * vec4(yuvToRgb(data.b, data.g, data.r), data.a);
        }
        void applyOpacity(inout vec4 color) { color *= u_opacity; }
        void applyAntialiasing(inout vec4 color) { color *= antialias(); }

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
            color = vec4(color.rgb * u_filterAmount, color.a);
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

        vec4 sampleColorAtRadius(float radius, vec2 texCoord)
        {
            vec2 coord = texCoord + radius * u_blurRadius;
            return SamplerFunction(s_sampler, coord);
        }

        float sampleAlphaAtRadius(float radius, vec2 texCoord)
        {
            vec2 coord = texCoord - u_shadowOffset + radius * u_blurRadius;
            return SamplerFunction(s_sampler, coord).a * float(coord.x > 0. && coord.y > 0. && coord.x < 1. && coord.y < 1.);
        }

        void applyBlurFilter(inout vec4 color, vec2 texCoord)
        {
            vec4 total = sampleColorAtRadius(0., texCoord) * u_gaussianKernel[0];
            for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
                total += sampleColorAtRadius(float(i) * GAUSSIAN_KERNEL_STEP, texCoord) * u_gaussianKernel[i];
                total += sampleColorAtRadius(float(-1 * i) * GAUSSIAN_KERNEL_STEP, texCoord) * u_gaussianKernel[i];
            }

            color = total;
        }

        void applyAlphaBlur(inout vec4 color, vec2 texCoord)
        {
            float total = sampleAlphaAtRadius(0., texCoord) * u_gaussianKernel[0];
            for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
                total += sampleAlphaAtRadius(float(i) * GAUSSIAN_KERNEL_STEP, texCoord) * u_gaussianKernel[i];
                total += sampleAlphaAtRadius(float(-1 * i) * GAUSSIAN_KERNEL_STEP, texCoord) * u_gaussianKernel[i];
            }

            color *= total;
        }

        vec4 sourceOver(vec4 src, vec4 dst) { return src + dst * (1. - src.a); }

        void applyContentTexture(inout vec4 color, vec2 texCoord)
        {
            vec4 contentColor = texture2D(s_contentTexture, texCoord);
            color = sourceOver(contentColor, color);
        }

        void applyTextureExternalOES(inout vec4 color, vec2 texCoord)
        {
            vec4 contentColor = texture2D(s_externalOESTexture, texCoord);
            color = sourceOver(contentColor, color);
        }

        void applySolidColor(inout vec4 color) { color *= u_color; }

        float ellipsisDist(vec2 p, vec2 radius)
        {
            if (radius == vec2(0, 0))
                return 0.0;

            vec2 p0 = p / radius;
            vec2 p1 = 2.0 * p0 / radius;

            return (dot(p0, p0) - 1.0) / length (p1);
        }

        float ellipsisCoverage(vec2 point, vec2 center, vec2 radius)
        {
            float d = ellipsisDist(point - center, radius);
            return clamp(0.5 - d, 0.0, 1.0);
        }

        float roundedRectCoverage(vec2 p, int index)
        {
            vec4 bounds = vec4(u_roundedRect[index].xy, u_roundedRect[index].xy + u_roundedRect[index].zw);

            if (p.x < bounds.x || p.y < bounds.y || p.x >= bounds.z || p.y >= bounds.w)
                return 0.0;

            vec2 topLeftRadii = u_roundedRect[index + 1].xy;
            vec2 topRightRadii = u_roundedRect[index + 1].zw;
            vec2 bottomLeftRadii = u_roundedRect[index + 2].xy;
            vec2 bottomRightRadii = u_roundedRect[index + 2].zw;

            vec2 topLeftCenter = bounds.xy + topLeftRadii;
            vec2 topRightCenter = bounds.zy + (topRightRadii * vec2(-1, 1));
            vec2 bottomLeftCenter = bounds.xw + (bottomLeftRadii * vec2(1, -1));
            vec2 bottomRightCenter = bounds.zw + (bottomRightRadii * vec2(-1, -1));

            if (p.x < topLeftCenter.x && p.y < topLeftCenter.y)
                return ellipsisCoverage(p, topLeftCenter, topLeftRadii);

            if (p.x > topRightCenter.x && p.y < topRightCenter.y)
                return ellipsisCoverage(p, topRightCenter, topRightRadii);

            if (p.x < bottomLeftCenter.x && p.y > bottomLeftCenter.y)
                return ellipsisCoverage(p, bottomLeftCenter, bottomLeftRadii);

            if (p.x > bottomRightCenter.x && p.y > bottomRightCenter.y)
                return ellipsisCoverage(p, bottomRightCenter, bottomRightRadii);

            return 1.0;
        }

        void applyRoundedRectClip(inout vec4 color)
        {
            // This works by checking whether the fragment position, once the transform is applied,
            // is inside the defined rounded rectangle or not.
            //
            // We can't use gl_fragCoord for the fragment position because thats the projected point
            // and the projection screws the Z component. We need the real 3D position that comes from
            // the nonProjectedPosition variable.
            int nRects = u_roundedRectNumber;
            if (nRects > ROUNDED_RECT_MAX_RECTS)
                nRects = ROUNDED_RECT_MAX_RECTS;

            for (int rectIndex = 0; rectIndex < nRects; rectIndex++) {
                vec4 fragCoord = u_roundedRectInverseTransformMatrix[rectIndex] * v_nonProjectedPosition;
                color *= roundedRectCoverage(fragCoord.xy, rectIndex * 3);
            }
        }

        void main(void)
        {
            vec4 color = vec4(1., 1., 1., 1.);
            vec2 texCoord = transformTexCoord();
            applyManualRepeatIfNeeded(texCoord);
            applyTextureRGBIfNeeded(color, texCoord);
            applyTextureYUVIfNeeded(color, texCoord);
            applyTextureNV12IfNeeded(color, texCoord);
            applyTextureNV21IfNeeded(color, texCoord);
            applyTexturePackedYUVIfNeeded(color, texCoord);
            applySolidColorIfNeeded(color);
            applyAntialiasingIfNeeded(color);
            applyOpacityIfNeeded(color);
            applyGrayscaleFilterIfNeeded(color);
            applySepiaFilterIfNeeded(color);
            applySaturateFilterIfNeeded(color);
            applyHueRotateFilterIfNeeded(color);
            applyInvertFilterIfNeeded(color);
            applyBrightnessFilterIfNeeded(color);
            applyContrastFilterIfNeeded(color);
            applyOpacityFilterIfNeeded(color);
            applyBlurFilterIfNeeded(color, texCoord);
            applyAlphaBlurIfNeeded(color, texCoord);
            applyContentTextureIfNeeded(color, texCoord);
            applyTextureExternalOESIfNeeded(color, texCoord);
            applyRoundedRectClipIfNeeded(color);
            gl_FragColor = color;
        }
    );

Ref<TextureMapperShaderProgram> TextureMapperShaderProgram::create(TextureMapperShaderProgram::Options options)
{
#define SET_APPLIER_FROM_OPTIONS(Applier) \
    optionsApplierBuilder.append(\
        (options & TextureMapperShaderProgram::Applier) ? ENABLE_APPLIER(Applier) : DISABLE_APPLIER(Applier))

    StringBuilder optionsApplierBuilder;
    SET_APPLIER_FROM_OPTIONS(TextureRGB);
    SET_APPLIER_FROM_OPTIONS(TextureYUV);
    SET_APPLIER_FROM_OPTIONS(TextureNV12);
    SET_APPLIER_FROM_OPTIONS(TextureNV21);
    SET_APPLIER_FROM_OPTIONS(TexturePackedYUV);
    SET_APPLIER_FROM_OPTIONS(Rect);
    SET_APPLIER_FROM_OPTIONS(SolidColor);
    SET_APPLIER_FROM_OPTIONS(Opacity);
    SET_APPLIER_FROM_OPTIONS(Antialiasing);
    SET_APPLIER_FROM_OPTIONS(GrayscaleFilter);
    SET_APPLIER_FROM_OPTIONS(SepiaFilter);
    SET_APPLIER_FROM_OPTIONS(SaturateFilter);
    SET_APPLIER_FROM_OPTIONS(HueRotateFilter);
    SET_APPLIER_FROM_OPTIONS(BrightnessFilter);
    SET_APPLIER_FROM_OPTIONS(ContrastFilter);
    SET_APPLIER_FROM_OPTIONS(InvertFilter);
    SET_APPLIER_FROM_OPTIONS(OpacityFilter);
    SET_APPLIER_FROM_OPTIONS(BlurFilter);
    SET_APPLIER_FROM_OPTIONS(AlphaBlur);
    SET_APPLIER_FROM_OPTIONS(ContentTexture);
    SET_APPLIER_FROM_OPTIONS(ManualRepeat);
    SET_APPLIER_FROM_OPTIONS(TextureExternalOES);
    SET_APPLIER_FROM_OPTIONS(RoundedRectClip);

    StringBuilder vertexShaderBuilder;

    // OpenGL >= 3.2 requires a #version directive at the beginning of the code.
#if !USE(OPENGL_ES)
    unsigned glVersion = GLContext::current()->version();
    if (glVersion >= 320)
        vertexShaderBuilder.append(GLSL_DIRECTIVE(version 150));
#endif

    // Append the options.
    vertexShaderBuilder.append(optionsApplierBuilder.toString());

    // Append the appropriate input/output variable definitions.
#if USE(OPENGL_ES)
    vertexShaderBuilder.append(vertexTemplateLT320Vars);
#else
    if (glVersion >= 320)
        vertexShaderBuilder.append(vertexTemplateGE320Vars);
    else
        vertexShaderBuilder.append(vertexTemplateLT320Vars);
#endif

    // Append the common code.
    vertexShaderBuilder.append(vertexTemplateCommon);

    StringBuilder fragmentShaderBuilder;

    // OpenGL >= 3.2 requires a #version directive at the beginning of the code.
#if !USE(OPENGL_ES)
    if (glVersion >= 320)
        fragmentShaderBuilder.append(GLSL_DIRECTIVE(version 150));
#endif

    // Append the options.
    fragmentShaderBuilder.append(optionsApplierBuilder.toString());

    // Append the common header.
    fragmentShaderBuilder.append(fragmentTemplateHeaderCommon);

    // Append the appropriate input/output variable definitions.
#if USE(OPENGL_ES)
    fragmentShaderBuilder.append(fragmentTemplateLT320Vars);
#else
    if (glVersion >= 320)
        fragmentShaderBuilder.append(fragmentTemplateGE320Vars);
    else
        fragmentShaderBuilder.append(fragmentTemplateLT320Vars);
#endif

    // Append the common code.
    fragmentShaderBuilder.append(fragmentTemplateCommon);

    return adoptRef(*new TextureMapperShaderProgram(vertexShaderBuilder.toString(), fragmentShaderBuilder.toString()));
}

#if !LOG_DISABLED
static CString getShaderLog(GLuint shader)
{
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return { };

    Vector<GLchar> info(logLength);
    GLsizei infoLength = 0;
    glGetShaderInfoLog(shader, logLength, &infoLength, info.data());

    size_t stringLength = std::max(infoLength, 0);
    return { info.data(), stringLength };
}

static CString getProgramLog(GLuint program)
{
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return { };

    Vector<GLchar> info(logLength);
    GLsizei infoLength = 0;
    glGetProgramInfoLog(program, logLength, &infoLength, info.data());

    size_t stringLength = std::max(infoLength, 0);
    return { info.data(), stringLength };
}
#endif

TextureMapperShaderProgram::TextureMapperShaderProgram(const String& vertex, const String& fragment)
{
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    {
        CString vertexCString = vertex.utf8();
        const char* data = vertexCString.data();
        int length = vertexCString.length();
        glShaderSource(m_vertexShader, 1, &data, &length);
    }
    glCompileShader(m_vertexShader);

    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    {
        CString fragmentCString = fragment.utf8();
        const char* data = fragmentCString.data();
        int length = fragmentCString.length();
        glShaderSource(m_fragmentShader, 1, &data, &length);
    }
    glCompileShader(m_fragmentShader);

    m_id = glCreateProgram();
    glAttachShader(m_id, m_vertexShader);
    glAttachShader(m_id, m_fragmentShader);
    glLinkProgram(m_id);

    if (!compositingLogEnabled() || glGetError() == GL_NO_ERROR)
        return;

    LOG(Compositing, "Vertex shader log: %s\n", getShaderLog(m_vertexShader).data());
    LOG(Compositing, "Fragment shader log: %s\n", getShaderLog(m_fragmentShader).data());
    LOG(Compositing, "Program log: %s\n", getProgramLog(m_id).data());
}

TextureMapperShaderProgram::~TextureMapperShaderProgram()
{
    if (!m_id)
        return;

    glDetachShader(m_id, m_vertexShader);
    glDeleteShader(m_vertexShader);
    glDetachShader(m_id, m_fragmentShader);
    glDeleteShader(m_fragmentShader);
    glDeleteProgram(m_id);
}

void TextureMapperShaderProgram::setMatrix(GLuint location, const TransformationMatrix& matrix)
{
    auto floatMatrix = matrix.toColumnMajorFloatArray();
    glUniformMatrix4fv(location, 1, false, floatMatrix.data());
}

GLuint TextureMapperShaderProgram::getLocation(VariableID variable, ASCIILiteral name, VariableType type)
{
    auto addResult = m_variables.ensure(variable,
        [this, &name, type] {
            switch (type) {
            case UniformVariable:
                return glGetUniformLocation(m_id, name);
            case AttribVariable:
                return glGetAttribLocation(m_id, name);
            }
            ASSERT_NOT_REACHED();
            return 0;
        });
    return addResult.iterator->value;
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)
