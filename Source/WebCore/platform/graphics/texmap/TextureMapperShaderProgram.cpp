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

#if USE(TEXTURE_MAPPER)

#include "GLContext.h"
#include "Logging.h"
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

// ES 3.0 Compatibility Macros
#define ES3_COMPATIBILITY_VERTEX \
    GLSL_DIRECTIVE(if __VERSION__ >= 300) \
        GLSL_DIRECTIVE(define texture2D texture) \
        GLSL_DIRECTIVE(define texture2DProj textureProj) \
        GLSL_DIRECTIVE(define texture2DLod textureLod) \
        GLSL_DIRECTIVE(define texture2DProjLod textureProjLod) \
        GLSL_DIRECTIVE(define textureCube texture) \
        GLSL_DIRECTIVE(define textureCubeLod textureLod) \
        GLSL_DIRECTIVE(define attribute in) \
        GLSL_DIRECTIVE(define varying out) \
    GLSL_DIRECTIVE(endif)

#define ES3_COMPATIBILITY_FRAGMENT \
    GLSL_DIRECTIVE(if __VERSION__ >= 300) \
        GLSL_DIRECTIVE(define texture2D texture) \
        GLSL_DIRECTIVE(define texture2DProj textureProj) \
        GLSL_DIRECTIVE(define texture2DLod textureLod) \
        GLSL_DIRECTIVE(define texture2DProjLod textureProjLod) \
        GLSL_DIRECTIVE(define textureCube texture) \
        GLSL_DIRECTIVE(define textureCubeLod textureLod) \
        GLSL_DIRECTIVE(define varying in) \
        "out mediump vec4 fragColor;\n" \
        GLSL_DIRECTIVE(define gl_FragColor fragColor) \
    GLSL_DIRECTIVE(endif)

// Input/output variables definition for OpenGL ES < 3.2.
static const char* vertexTemplateLT320Vars =
    TEXTURE_SPACE_MATRIX_PRECISION_DIRECTIVE
    STRINGIFY(
        precision TextureSpaceMatrixPrecision float;
    )
    STRINGIFY(
        attribute vec4 a_vertex;
        varying vec2 v_texCoord;
        varying vec2 v_transformedTexCoord;
        varying float v_antialias;
        varying vec4 v_nonProjectedPosition;
    );
// clang-format off
static const char* vertexTemplateCommon =
    STRINGIFY(
        uniform mat4 u_modelViewMatrix;
        uniform mat4 u_projectionMatrix;
        uniform mat4 u_textureSpaceMatrix;

        void noop(vec2 position) { }

        vec4 toViewportSpace(vec2 pos) { return u_modelViewMatrix * vec4(pos, 0., 1.); }

        // This function relies on the assumption that we get edge triangles with control points,
        // a control point being the nearest point to the coordinate that is on the edge.
        void applyAntialiasing(vec2 position)
        {
            const vec2 center = vec2(0.5, 0.5);
            const float antialiasInflationDistance = 1.;
            // We pass the control point as the zw coordinates of the vertex.
            // The control point is the point on the edge closest to the current position.
            vec2 controlPoint = a_vertex.zw;
            bool isCenter = distance(position, controlPoint) > 0.;
            if (isCenter) {
                // v_antialias needs to be 0 for the outer edge and 1. for the inner edge.
                // We make sure that the varying interpolates between 0 (outer edge), 1 (inner edge) and n > 1 (center).
                // Mathematically, v_antialias for the center is:
                //
                //    v_antialias = (viewportSpaceDistance + antialiasInflationDistance) / antialiasInflationDistance
                //
                // Because we use homogeneous coordinates for the viewport space, we use it for v_antialias, too.
                // The denominator is v_nonProjectedPosition.w. So. multiply the numerator by v_nonProjectedPosition.w:
                //
                //    v_antialias = (viewportSpaceDistance + antialiasInflationDistance) * v_nonProjectedPosition.w / antialiasInflationDistance

                vec4 controlPointInViewportCoordinates = toViewportSpace(controlPoint);
                // Calculate the distance after the reduction to common denominator.
                float viewportSpaceDistance = distance(v_nonProjectedPosition.xy * controlPointInViewportCoordinates.w, controlPointInViewportCoordinates.xy * v_nonProjectedPosition.w);
                // Calculate the distance multiplied by v_nonProjectedPosition.w.
                // FIXME: The case of controlPointInViewportCoordinates.w <= 0.
                if (controlPointInViewportCoordinates.w > 0.)
                    viewportSpaceDistance /= controlPointInViewportCoordinates.w;
                v_antialias = (viewportSpaceDistance + antialiasInflationDistance * v_nonProjectedPosition.w) / antialiasInflationDistance;
            } else {
                vec4 centerInViewportCoordinates = toViewportSpace(center);
                // Calculate the 2D direction from the center to the vertex in the viewport space (homogeneous coordinates).
                // Subtract after the reduction to common denominator, centerInViewportCoordinates.w * v_nonProjectedPosition.w.
                vec2 direction = v_nonProjectedPosition.xy * centerInViewportCoordinates.w - centerInViewportCoordinates.xy * v_nonProjectedPosition.w;
                if (length(direction) > 0.) {
                    float oldDistance = distance(v_nonProjectedPosition.xyz, centerInViewportCoordinates.xyz);
                    // Move the vertex toward the direction from the center to the vertex.
                    v_nonProjectedPosition += vec4(normalize(direction) * antialiasInflationDistance * v_nonProjectedPosition.w, 0., 0.);
                    float newDistance = distance(v_nonProjectedPosition.xyz, centerInViewportCoordinates.xyz);
                    // Move v_texCoord based on 3D distance inflation ratio.
                    v_texCoord += normalize(position - center) * (newDistance - oldDistance) / oldDistance;
                }
                v_antialias = 0.;
            }
        }

        void main(void)
        {
            vec2 position = a_vertex.xy;

            v_texCoord = position;
            v_transformedTexCoord = (u_textureSpaceMatrix * vec4(position, 0., 1.)).xy;
            v_nonProjectedPosition = toViewportSpace(position);
            applyAntialiasingIfNeeded(position);
            gl_Position = u_projectionMatrix * v_nonProjectedPosition;
        }
    );
// clang-format on
#define ANTIALIASING_TEX_COORD_DIRECTIVE \
    GLSL_DIRECTIVE(if defined(ENABLE_Antialiasing)) \
        GLSL_DIRECTIVE(define transformTexCoord fragmentTransformTexCoord) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(define transformTexCoord vertexTransformTexCoord) \
    GLSL_DIRECTIVE(endif)

#define ENABLE_APPLIER(Name) "#define ENABLE_"#Name"\n#define apply"#Name"IfNeeded apply"#Name"\n"
#define DISABLE_APPLIER(Name) "#define apply"#Name"IfNeeded noop\n"
#define BLUR_CONSTANTS \
    GLSL_DIRECTIVE(define GAUSSIAN_KERNEL_MAX_HALF_SIZE 6)


#define OES_EGL_IMAGE_EXTERNAL_DIRECTIVE \
    GLSL_DIRECTIVE(ifdef ENABLE_TextureExternalOES) \
        GLSL_DIRECTIVE(if __VERSION__ >= 300) \
            GLSL_DIRECTIVE(extension GL_OES_EGL_image_external_essl3 : require) \
        GLSL_DIRECTIVE(else) \
            GLSL_DIRECTIVE(extension GL_OES_EGL_image_external : require) \
        GLSL_DIRECTIVE(endif) \
        GLSL_DIRECTIVE(define SamplerExternalOESType samplerExternalOES) \
        STRINGIFY( \
            precision mediump samplerExternalOES;\n \
        ) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(define SamplerExternalOESType sampler2D) \
    GLSL_DIRECTIVE(endif)

// clang-format off
#define EXT_YUV_TARGET_DIRECTIVE \
    STRINGIFY(\n) \
    GLSL_DIRECTIVE(ifdef ENABLE_TextureExternalOESYUV) \
        GLSL_DIRECTIVE(extension GL_EXT_YUV_target : require) \
        GLSL_DIRECTIVE(define SamplerExternalOESYUVType __samplerExternal2DY2YEXT) \
    GLSL_DIRECTIVE(else) \
        GLSL_DIRECTIVE(if __VERSION__ >= 300) \
            GLSL_DIRECTIVE(extension GL_OES_EGL_image_external_essl3 : require) \
        GLSL_DIRECTIVE(else) \
            GLSL_DIRECTIVE(extension GL_OES_EGL_image_external : require) \
        GLSL_DIRECTIVE(endif) \
        GLSL_DIRECTIVE(define SamplerExternalOESYUVType samplerExternalOES) \
    GLSL_DIRECTIVE(endif)
// clang-format on

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
// rest of them.
// clang-format off
static const char* fragmentTemplateHeaderCommon =
    ANTIALIASING_TEX_COORD_DIRECTIVE
    BLUR_CONSTANTS
    ROUNDED_RECT_CONSTANTS
    OES_EGL_IMAGE_EXTERNAL_DIRECTIVE
    TEXTURE_SPACE_MATRIX_PRECISION_DIRECTIVE;

static const char* fragmentTemplateHeaderCommonVariables =
    STRINGIFY(
        precision TextureSpaceMatrixPrecision float;
    )
    STRINGIFY(
        uniform mat4 u_textureSpaceMatrix;
        uniform mat4 u_textureColorSpaceMatrix;
    )
    STRINGIFY(
        precision mediump float;
    );

// Input/output variables definition for both OpenGL ES < 3.2.
static const char* fragmentTemplateLT320Vars =
    STRINGIFY(
        varying float v_antialias;
        varying vec2 v_texCoord;
        varying vec2 v_transformedTexCoord;
        varying vec4 v_nonProjectedPosition;
    );

static const char* fragmentTemplateYUVToRGB =
    "\n"
    GLSL_DIRECTIVE(ifdef ENABLE_TextureExternalOESYUV)
    STRINGIFY(
        void applyTextureExternalOESYUV(inout vec4 color, vec2 texCoord)
        {
            vec4 contentColor = vec4(yuv_2_rgb(texture2D(s_yuvToRgbSampler, texCoord).rgb, itu_601).rgb, 1.0);
            color = sourceOver(contentColor, color);
        }
    )
    "\n"
    GLSL_DIRECTIVE(else)
    STRINGIFY(
        void applyTextureExternalOESYUV(inout vec4 color, vec2 texCoord)
        {
            vec4 contentColor = texture2D(s_yuvToRgbSampler, texCoord);
            color = sourceOver(contentColor, color);
        }
    )
    "\n"
    GLSL_DIRECTIVE(endif);

static const char* fragmentTemplateYUVToRGBNoop =
    "\n"
    STRINGIFY(
        void applyTextureExternalOESYUV(inout vec4 color, vec2 texCoord)
        {
            vec4 contentColor = texture2D(s_yuvToRgbSampler, texCoord);
            color = sourceOver(contentColor, color);
        }
    );

// PQ tone-mapping function with conditional highp precision.
// Only enabled when highp is available in fragment shaders, because we get banding artifacts with mediump
// precision.
static const char* fragmentTemplateToneMapPq =
    "\n"
    GLSL_DIRECTIVE(ifdef GL_FRAGMENT_PRECISION_HIGH)
    STRINGIFY(
        void applyToneMapPQ(inout vec4 color)
        {
            // Reference PQ EOTF (ITU-R BT.2100)
            highp float m1 = 0.1593017578125;
            highp float m2 = 78.84375;
            highp float c1 = 0.8359375;
            highp float c2 = 18.8515625;
            highp float c3 = 18.6875;

            highp vec3 pqPowM2 = pow(max(color.rgb, vec3(0.0)), vec3(1.0 / m2));
            highp vec3 numerator = max(pqPowM2 - c1, vec3(0.0));
            highp vec3 denominator = c2 - c3 * pqPowM2;
            highp vec3 linear = pow(numerator / denominator, vec3(1.0 / m1));

            // Normalize using HDR reference white (ITU-R BT.2408)
            const highp float hdrReferenceWhite = 203.0;
            const highp float pqMaxNits = 10000.0;
            highp vec3 normalized = linear * (pqMaxNits / hdrReferenceWhite);

            // Tone-map using maxRGB-based Reinhard, which preserves saturation.
            // Simplified version of that Chromium does, described in
            // https://docs.google.com/document/d/17T2ek1i2R7tXdfHCnM-i5n6__RoYe0JyMfKmTEjoGR8/edit?tab=t.0#heading=h.h00l7d53phqy
            highp float maxRGB = max(max(normalized.r, normalized.g), normalized.b);
            highp vec3 toneMapped = normalized / (1.0 + maxRGB);

            // Convert from BT.2020 to BT.709 color primaries
            highp mat3 bt2020ToBt709 = mat3(
                1.6605, -0.1246, -0.0182,
                -0.5876, 1.1329, -0.1006,
                -0.0728, -0.0083, 1.1187
            );
            highp vec3 bt709Linear = bt2020ToBt709 * toneMapped;

            // Apply inverse EOTF for sRGB gamma encoding (IEC 61966-2)
            bvec3 cutoff = lessThan(bt709Linear, vec3(0.0031308));
            highp vec3 higher = vec3(1.055) * pow(bt709Linear, vec3(1.0 / 2.4)) - vec3(0.055);
            highp vec3 lower = bt709Linear * vec3(12.92);
            highp vec3 srgb = mix(higher, lower, vec3(cutoff));

            color = vec4(srgb, color.a);
        }
    )
    "\n"
    GLSL_DIRECTIVE(else)
    STRINGIFY(
        void applyToneMapPQ(inout vec4 color) { }
    )
    "\n"
    GLSL_DIRECTIVE(endif);

static const char* fragmentTemplateCommon =
    STRINGIFY(
        uniform sampler2D s_sampler;
        uniform sampler2D s_samplerY;
        uniform sampler2D s_samplerU;
        uniform sampler2D s_samplerV;
        uniform sampler2D s_samplerA;
        uniform sampler2D s_contentTexture;
        uniform SamplerExternalOESType s_externalOESTexture;
        uniform SamplerExternalOESYUVType s_yuvToRgbSampler;
        uniform float u_opacity;
        uniform float u_filterAmount;
        uniform mat4 u_yuvToRgb;
        uniform vec4 u_color;
        uniform vec2 u_texelSize;
        uniform vec2 u_uvMax;
        uniform float u_gaussianKernel[GAUSSIAN_KERNEL_MAX_HALF_SIZE];
        uniform float u_gaussianKernelOffset[GAUSSIAN_KERNEL_MAX_HALF_SIZE];
        uniform int u_gaussianKernelHalfSize;
        uniform vec2 u_blurDirection;
        uniform int u_roundedRectNumber;
        uniform vec4 u_roundedRect[ROUNDED_RECT_ARRAY_SIZE];
        uniform mat4 u_roundedRectInverseTransformMatrix[ROUNDED_RECT_INVERSE_TRANSFORM_ARRAY_SIZE];

        void noop(inout vec4 dummyParameter) { }
        void noop(inout vec4 dummyParameter, vec2 texCoord) { }
        void noop(inout vec2 dummyParameter) { }

        float antialias()
        {
            if (v_nonProjectedPosition.w <= 0.)
                return 1.;
            return smoothstep(0., 1., v_antialias / v_nonProjectedPosition.w);
        }

        vec2 fragmentTransformTexCoord()
        {
            vec4 clampedPosition = clamp(vec4(v_texCoord, 0., 1.), 0., 1.);
            return (u_textureSpaceMatrix * clampedPosition).xy;
        }

        vec2 vertexTransformTexCoord() { return v_transformedTexCoord; }

        void applyManualRepeat(inout vec2 pos) { pos = fract(pos); }

        void applyClampUVBounds(inout vec2 texCoord)
        {
            vec2 uvMax = u_uvMax - u_texelSize / 2.;
            texCoord = clamp(texCoord, vec2(0.), uvMax);
        }

        void applyTextureRGB(inout vec4 color, vec2 texCoord) { color = u_textureColorSpaceMatrix * texture2D(s_sampler, texCoord); }

        void applyPremultiply(inout vec4 color) { color = vec4(color.rgb * color.a, color.a); }

        void applyToneMapPQ(inout vec4 color);

        void applyTextureExternalOESYUV(inout vec4 color, vec2 texCoord);

        vec3 yuvToRgb(float y, float u, float v)
        {
            vec4 rgb = vec4(y, u, v, 1.0) * u_yuvToRgb;
            return rgb.xyz;
        }
        void applyTextureYUV(inout vec4 color, vec2 texCoord)
        {
            float y = texture2D(s_samplerY, texCoord).r;
            float u = texture2D(s_samplerU, texCoord).r;
            float v = texture2D(s_samplerV, texCoord).r;
            vec4 data = vec4(yuvToRgb(y, u, v), 1.0);
            color = u_textureColorSpaceMatrix * data;
        }
        void applyTextureYUVA(inout vec4 color, vec2 texCoord)
        {
            float y = texture2D(s_samplerY, texCoord).r;
            float u = texture2D(s_samplerU, texCoord).r;
            float v = texture2D(s_samplerV, texCoord).r;
            float a = texture2D(s_samplerA, texCoord).r;
            vec4 data = vec4(yuvToRgb(y, u, v), a);
            color = u_textureColorSpaceMatrix * data;
        }
        void applyTextureNV12(inout vec4 color, vec2 texCoord)
        {
            float y = texture2D(s_samplerY, texCoord).r;
            vec2 uv = texture2D(s_samplerU, texCoord).rg;
            vec4 data = vec4(yuvToRgb(y, uv.x, uv.y), 1.0);
            color = u_textureColorSpaceMatrix * data;
        }
        void applyTextureNV21(inout vec4 color, vec2 texCoord)
        {
            float y = texture2D(s_samplerY, texCoord).r;
            vec2 uv = texture2D(s_samplerU, texCoord).gr;
            vec4 data = vec4(yuvToRgb(y, uv.x, uv.y), 1.0);
            color = u_textureColorSpaceMatrix * data;
        }
        void applyTexturePackedYUV(inout vec4 color, vec2 texCoord)
        {
            vec4 data = texture2D(s_sampler, texCoord);
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

        float invert(float n, float a) { return (a - n) * u_filterAmount + n * (1.0 - u_filterAmount); }
        void applyInvertFilter(inout vec4 color)
        {
            color = vec4(invert(color.r, color.a), invert(color.g, color.a), invert(color.b, color.a), color.a);
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
            color *= u_filterAmount;
        }

        void applyTextureCopy(inout vec4 color, vec2 texCoord)
        {
            vec2 min = (u_textureSpaceMatrix * vec4(0., 0., 0., 1.)).xy + u_texelSize / 2.;
            vec2 max = (u_textureSpaceMatrix * vec4(1., 1., 0., 1.)).xy - u_texelSize / 2.;

            vec2 coord = clamp(texCoord, min, max);

            color = texture2D(s_sampler, coord);
        }

        void applyBlurFilter(inout vec4 color, vec2 texCoord)
        {
            vec2 step = u_blurDirection * u_texelSize;
            vec2 min = (u_textureSpaceMatrix * vec4(0., 0., 0., 1.)).xy + u_texelSize / 2.;
            vec2 max = (u_textureSpaceMatrix * vec4(1., 1., 0., 1.)).xy - u_texelSize / 2.;

            vec4 total = texture2D(s_sampler, texCoord) * u_gaussianKernel[0];

            for (int i = 1; i < GaussianKernelHalfSize; i++) {
                vec2 offset = step * u_gaussianKernelOffset[i];
                total += texture2D(s_sampler, clamp(texCoord + offset, min, max)) * u_gaussianKernel[i];
                total += texture2D(s_sampler, clamp(texCoord - offset, min, max)) * u_gaussianKernel[i];
            }

            color = total;
        }

        void applyAlphaBlur(inout vec4 color, vec2 texCoord)
        {
            vec2 step = u_blurDirection * u_texelSize;
            vec2 min = (u_textureSpaceMatrix * vec4(0., 0., 0., 1.)).xy + u_texelSize / 2.;
            vec2 max = (u_textureSpaceMatrix * vec4(1., 1., 0., 1.)).xy - u_texelSize / 2.;

            float total = texture2D(s_sampler, texCoord).a * u_gaussianKernel[0];

            for (int i = 1; i < GaussianKernelHalfSize; i++) {
                vec2 offset = step * u_gaussianKernelOffset[i];
                total += texture2D(s_sampler, clamp(texCoord + offset, min, max)).a * u_gaussianKernel[i];
                total += texture2D(s_sampler, clamp(texCoord - offset, min, max)).a * u_gaussianKernel[i];
            }

            color = vec4(0., 0., 0., total);
        }

        void applyAlphaToShadow(inout vec4 color, vec2 texCoord)
        {
            vec2 coord = clamp(texCoord, u_texelSize / 2., vec2(1., 1.) - u_texelSize / 2.);
            color *= u_color;
            color *= texture2D(s_sampler, coord).a;
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

        float roundedRectCoverage(vec2 p, vec4 bounds, vec2 topLeftRadii, vec2 topRightRadii, vec2 bottomLeftRadii, vec2 bottomRightRadii)
        {
            if (p.x < bounds.x || p.y < bounds.y || p.x >= bounds.z || p.y >= bounds.w)
                return 0.0;

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
            //
            // This implementation is not optimal, but it's done this way in order to overcome rpi3's
            // proprietary video driver limitations (see https://bugs.webkit.org/show_bug.cgi?id=219739).

            for (int rectIndex = 0; rectIndex < ROUNDED_RECT_MAX_RECTS; rectIndex++) {
                if (rectIndex >= u_roundedRectNumber)
                    break;

                vec4 fragCoord = u_roundedRectInverseTransformMatrix[rectIndex] * v_nonProjectedPosition;
                vec4 bounds = vec4(u_roundedRect[rectIndex * 3].xy, u_roundedRect[rectIndex * 3].xy + u_roundedRect[rectIndex * 3].zw);
                vec2 topLeftRadii = u_roundedRect[(rectIndex * 3) + 1].xy;
                vec2 topRightRadii = u_roundedRect[(rectIndex * 3) + 1].zw;
                vec2 bottomLeftRadii = u_roundedRect[(rectIndex * 3) + 2].xy;
                vec2 bottomRightRadii = u_roundedRect[(rectIndex * 3) + 2].zw;
                float coverage = roundedRectCoverage(fragCoord.xy, bounds, topLeftRadii, topRightRadii, bottomLeftRadii, bottomRightRadii);

                // Pixels outside the rect have coverage 0.0.
                // Pixels inside the rect have coverage 1.0.
                // Pixels on the border of the rounded parts have coverage between 0.0 and 1.0.

                // Discard the fragments that are outside the rect.
                if (coverage == 0.0)
                    discard;

                // By multiplying the color by the coverage, pixels on the border of the rounded corners get
                // a bit more transparent.
                // If blending is enabled, this does some antialiasing on the border pixels.
                // If blending is disabled it means that we're rendering a holepunch buffer, so the color
                // is always (0,0,0,0). In this case, multiplying by the coverage doesn't cause any effect
                // and no antialiasing is done.
                color *= coverage;
            }
        }

        void main(void)
        {
            vec4 color = vec4(1., 1., 1., 1.);
            vec2 texCoord = transformTexCoord();
            applyManualRepeatIfNeeded(texCoord);
            applyClampUVBoundsIfNeeded(texCoord);
            applyTextureRGBIfNeeded(color, texCoord);
            applyTextureYUVIfNeeded(color, texCoord);
            applyTextureYUVAIfNeeded(color, texCoord);
            applyTextureNV12IfNeeded(color, texCoord);
            applyTextureNV21IfNeeded(color, texCoord);
            applyTexturePackedYUVIfNeeded(color, texCoord);
            applyToneMapPQIfNeeded(color);
            applyPremultiplyIfNeeded(color);
            applySolidColorIfNeeded(color);
            applyAlphaBlurIfNeeded(color, texCoord);
            applyAlphaToShadowIfNeeded(color, texCoord);
            applyContentTextureIfNeeded(color, texCoord);
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
            applyTextureCopyIfNeeded(color, texCoord);
            applyBlurFilterIfNeeded(color, texCoord);
            applyTextureExternalOESIfNeeded(color, texCoord);
            applyTextureExternalOESYUVIfNeeded(color, texCoord);
            applyRoundedRectClipIfNeeded(color);
            gl_FragColor = color;
        }
    );
// clang-format on

Ref<TextureMapperShaderProgram> TextureMapperShaderProgram::create(TextureMapperShaderProgram::Options options)
{
#define SET_APPLIER_FROM_OPTIONS(Applier) \
    optionsApplierBuilder.append(\
        (options & TextureMapperShaderProgram::Applier) ? unsafeSpan(ENABLE_APPLIER(Applier)) : unsafeSpan(DISABLE_APPLIER(Applier)))

    unsigned glVersion = GLContext::current()->version();

    StringBuilder optionsApplierBuilder;
    SET_APPLIER_FROM_OPTIONS(TextureRGB);
    SET_APPLIER_FROM_OPTIONS(TextureYUV);
    SET_APPLIER_FROM_OPTIONS(TextureYUVA);
    SET_APPLIER_FROM_OPTIONS(TextureNV12);
    SET_APPLIER_FROM_OPTIONS(TextureNV21);
    SET_APPLIER_FROM_OPTIONS(TexturePackedYUV);
    SET_APPLIER_FROM_OPTIONS(ToneMapPQ);
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
    SET_APPLIER_FROM_OPTIONS(TextureCopy);
    SET_APPLIER_FROM_OPTIONS(BlurFilter);
    SET_APPLIER_FROM_OPTIONS(AlphaBlur);
    SET_APPLIER_FROM_OPTIONS(AlphaToShadow);
    SET_APPLIER_FROM_OPTIONS(ContentTexture);
    SET_APPLIER_FROM_OPTIONS(ManualRepeat);
    SET_APPLIER_FROM_OPTIONS(ClampUVBounds);
    SET_APPLIER_FROM_OPTIONS(TextureExternalOES);
    SET_APPLIER_FROM_OPTIONS(TextureExternalOESYUV);
    SET_APPLIER_FROM_OPTIONS(RoundedRectClip);
    SET_APPLIER_FROM_OPTIONS(Premultiply);

    StringBuilder vertexShaderBuilder;

    if (glVersion >= 300) {
        vertexShaderBuilder.append(unsafeSpan(GLSL_DIRECTIVE(version 300 es)));
        vertexShaderBuilder.append(unsafeSpan(ES3_COMPATIBILITY_VERTEX));
    }

    // Append the options.
    vertexShaderBuilder.append(optionsApplierBuilder.toString());

    // Append the appropriate input/output variable definitions.
    vertexShaderBuilder.append(unsafeSpan(vertexTemplateLT320Vars));

    // Append the common code.
    vertexShaderBuilder.append(unsafeSpan(vertexTemplateCommon));

    StringBuilder fragmentShaderBuilder;

    if (glVersion >= 300) {
        fragmentShaderBuilder.append(unsafeSpan(GLSL_DIRECTIVE(version 300 es)));
        fragmentShaderBuilder.append(unsafeSpan(ES3_COMPATIBILITY_FRAGMENT));
    }

    // Append the options.
    fragmentShaderBuilder.append(optionsApplierBuilder.toString());

    if (glVersion >= 300)
        fragmentShaderBuilder.append(unsafeSpan(GLSL_DIRECTIVE(define GaussianKernelHalfSize u_gaussianKernelHalfSize)));
    else
        fragmentShaderBuilder.append(unsafeSpan(GLSL_DIRECTIVE(define GaussianKernelHalfSize GAUSSIAN_KERNEL_MAX_HALF_SIZE)));

    // Append the common header.
    fragmentShaderBuilder.append(unsafeSpan(fragmentTemplateHeaderCommon));

    if (GLContext::current()->glExtensions().EXT_YUV_target)
        fragmentShaderBuilder.append(unsafeSpan(EXT_YUV_TARGET_DIRECTIVE));
    else
        fragmentShaderBuilder.append(unsafeSpan(GLSL_DIRECTIVE(define SamplerExternalOESYUVType sampler2D)));

    fragmentShaderBuilder.append(unsafeSpan(fragmentTemplateHeaderCommonVariables));

    // Append the appropriate input/output variable definitions.
    fragmentShaderBuilder.append(unsafeSpan(fragmentTemplateLT320Vars));

    // Append the common code.
    fragmentShaderBuilder.append(unsafeSpan(fragmentTemplateCommon));

    // Append the PQ tone mapping function.
    fragmentShaderBuilder.append(unsafeSpan(fragmentTemplateToneMapPq));

    if (GLContext::current()->glExtensions().EXT_YUV_target)
        fragmentShaderBuilder.append(unsafeSpan(fragmentTemplateYUVToRGB));
    else
        fragmentShaderBuilder.append(unsafeSpan(fragmentTemplateYUVToRGBNoop));

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
    glGetShaderInfoLog(shader, logLength, &infoLength, info.mutableSpan().data());

    size_t stringLength = std::max(infoLength, 0);
    return byteCast<char>(info.span().first(stringLength));
}

static CString getProgramLog(GLuint program)
{
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return { };

    Vector<GLchar> info(logLength);
    GLsizei infoLength = 0;
    glGetProgramInfoLog(program, logLength, &infoLength, info.mutableSpan().data());

    size_t stringLength = std::max(infoLength, 0);
    return byteCast<char>(info.span().first(stringLength));
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

#endif // USE(TEXTURE_MAPPER)
