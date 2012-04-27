/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ShaderChromium.h"

#include "GraphicsContext.h"
#include "GraphicsContext3D.h"

#define SHADER0(Src) #Src
#define SHADER(Src) SHADER0(Src)

namespace WebCore {

VertexShaderPosTex::VertexShaderPosTex()
    : m_matrixLocation(-1)
{
}

void VertexShaderPosTex::init(GraphicsContext3D* context, unsigned program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    ASSERT(m_matrixLocation != -1);
}

String VertexShaderPosTex::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        varying vec2 v_texCoord;
        void main()
        {
            gl_Position = matrix * a_position;
            v_texCoord = a_texCoord;
        }
    );
}

VertexShaderPosTexYUVStretch::VertexShaderPosTexYUVStretch()
    : m_matrixLocation(-1)
    , m_yWidthScaleFactorLocation(-1)
    , m_uvWidthScaleFactorLocation(-1)
{
}

void VertexShaderPosTexYUVStretch::init(GraphicsContext3D* context, unsigned program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    m_yWidthScaleFactorLocation = context->getUniformLocation(program, "y_widthScaleFactor");
    m_uvWidthScaleFactorLocation = context->getUniformLocation(program, "uv_widthScaleFactor");
    ASSERT(m_matrixLocation != -1 && m_yWidthScaleFactorLocation != -1 && m_uvWidthScaleFactorLocation != -1);
}

String VertexShaderPosTexYUVStretch::getShaderString() const
{
    return SHADER(
        precision mediump float;
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        varying vec2 y_texCoord;
        varying vec2 uv_texCoord;
        uniform float y_widthScaleFactor;
        uniform float uv_widthScaleFactor;
        void main()
        {
            gl_Position = matrix * a_position;
            y_texCoord = vec2(y_widthScaleFactor * a_texCoord.x, a_texCoord.y);
            uv_texCoord = vec2(uv_widthScaleFactor * a_texCoord.x, a_texCoord.y);
        }
    );
}

VertexShaderPos::VertexShaderPos()
    : m_matrixLocation(-1)
{
}

void VertexShaderPos::init(GraphicsContext3D* context, unsigned program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    ASSERT(m_matrixLocation != -1);
}

String VertexShaderPos::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        uniform mat4 matrix;
        void main()
        {
            gl_Position = matrix * a_position;
        }
    );
}

VertexShaderPosTexTransform::VertexShaderPosTexTransform()
    : m_matrixLocation(-1)
    , m_texTransformLocation(-1)
{
}

void VertexShaderPosTexTransform::init(GraphicsContext3D* context, unsigned program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    m_texTransformLocation = context->getUniformLocation(program, "texTransform");
    ASSERT(m_matrixLocation != -1 && m_texTransformLocation != -1);
}

String VertexShaderPosTexTransform::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        uniform vec4 texTransform;
        varying vec2 v_texCoord;
        void main()
        {
            gl_Position = matrix * a_position;
            v_texCoord = a_texCoord * texTransform.zw + texTransform.xy;
        }
    );
}

VertexShaderQuad::VertexShaderQuad()
    : m_matrixLocation(-1)
    , m_pointLocation(-1)
{
}

String VertexShaderPosTexIdentity::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        varying vec2 v_texCoord;
        void main()
        {
            gl_Position = a_position;
            v_texCoord = (a_position.xy + vec2(1.0)) * 0.5;
        }
    );
}

void VertexShaderQuad::init(GraphicsContext3D* context, unsigned program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    m_pointLocation = context->getUniformLocation(program, "point");
    ASSERT(m_matrixLocation != -1 && m_pointLocation != -1);
}

String VertexShaderQuad::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        uniform vec2 point[4];
        varying vec2 v_texCoord;
        void main()
        {
            vec2 complement = abs(a_texCoord - 1.0);
            vec4 pos = vec4(0.0, 0.0, a_position.z, a_position.w);
            pos.xy += (complement.x * complement.y) * point[0];
            pos.xy += (a_texCoord.x * complement.y) * point[1];
            pos.xy += (a_texCoord.x * a_texCoord.y) * point[2];
            pos.xy += (complement.x * a_texCoord.y) * point[3];
            gl_Position = matrix * pos;
            v_texCoord = pos.xy + vec2(0.5);
        }
    );
}

VertexShaderTile::VertexShaderTile()
    : m_matrixLocation(-1)
    , m_pointLocation(-1)
    , m_vertexTexTransformLocation(-1)
{
}

void VertexShaderTile::init(GraphicsContext3D* context, unsigned program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    m_pointLocation = context->getUniformLocation(program, "point");
    m_vertexTexTransformLocation = context->getUniformLocation(program, "vertexTexTransform");
    ASSERT(m_matrixLocation != -1 && m_pointLocation != -1 && m_vertexTexTransformLocation != -1);
}

String VertexShaderTile::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        uniform vec2 point[4];
        uniform vec4 vertexTexTransform;
        varying vec2 v_texCoord;
        void main()
        {
            vec2 complement = abs(a_texCoord - 1.0);
            vec4 pos = vec4(0.0, 0.0, a_position.z, a_position.w);
            pos.xy += (complement.x * complement.y) * point[0];
            pos.xy += (a_texCoord.x * complement.y) * point[1];
            pos.xy += (a_texCoord.x * a_texCoord.y) * point[2];
            pos.xy += (complement.x * a_texCoord.y) * point[3];
            gl_Position = matrix * pos;
            v_texCoord = pos.xy * vertexTexTransform.zw + vertexTexTransform.xy;
        }
    );
}

VertexShaderVideoTransform::VertexShaderVideoTransform()
    : m_matrixLocation(-1)
    , m_texTransformLocation(-1)
    , m_texMatrixLocation(-1)
{
}

bool VertexShaderVideoTransform::init(GraphicsContext3D* context, unsigned program)
{
    m_matrixLocation = context->getUniformLocation(program, "matrix");
    m_texTransformLocation = context->getUniformLocation(program, "texTransform");
    m_texMatrixLocation = context->getUniformLocation(program, "texMatrix");
    return m_matrixLocation != -1 && m_texTransformLocation != -1 && m_texMatrixLocation != -1;
}

String VertexShaderVideoTransform::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        uniform vec4 texTransform;
        uniform mat4 texMatrix;
        varying vec2 v_texCoord;
        void main()
        {
            gl_Position = matrix * a_position;
            vec2 texCoord = vec2(texMatrix * vec4(a_texCoord.x, 1.0 - a_texCoord.y, 0.0, 1.0));
            v_texCoord = texCoord * texTransform.zw + texTransform.xy;
        }
    );
}

FragmentTexAlphaBinding::FragmentTexAlphaBinding()
    : m_samplerLocation(-1)
    , m_alphaLocation(-1)
{
}

void FragmentTexAlphaBinding::init(GraphicsContext3D* context, unsigned program)
{
    m_samplerLocation = context->getUniformLocation(program, "s_texture");
    m_alphaLocation = context->getUniformLocation(program, "alpha");

    ASSERT(m_samplerLocation != -1 && m_alphaLocation != -1);
}

FragmentTexOpaqueBinding::FragmentTexOpaqueBinding()
    : m_samplerLocation(-1)
{
}

void FragmentTexOpaqueBinding::init(GraphicsContext3D* context, unsigned program)
{
    m_samplerLocation = context->getUniformLocation(program, "s_texture");

    ASSERT(m_samplerLocation != -1);
}

String FragmentShaderRGBATexFlipAlpha::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        void main()
        {
            vec4 texColor = texture2D(s_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y));
            gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha;
        }
    );
}

bool FragmentShaderOESImageExternal::init(GraphicsContext3D* context, unsigned program)
{
    m_samplerLocation = context->getUniformLocation(program, "s_texture");

    return m_samplerLocation != -1;
}

String FragmentShaderOESImageExternal::getShaderString() const
{
    // Cannot use the SHADER() macro because of the '#' char
    return "#extension GL_OES_EGL_image_external : require \n"
           "precision mediump float;\n"
           "varying vec2 v_texCoord;\n"
           "uniform samplerExternalOES s_texture;\n"
           "void main()\n"
           "{\n"
           "    vec4 texColor = texture2D(s_texture, v_texCoord);\n"
           "    gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w);\n"
           "}\n";
}

String FragmentShaderRGBATexAlpha::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = texColor * alpha;
        }
    );
}

String FragmentShaderRGBATexRectFlipAlpha::getShaderString() const
{
    // This must be paired with VertexShaderPosTexTransform to pick up the texTransform uniform.
    // The necessary #extension preprocessing directive breaks the SHADER and SHADER0 macros.
    return "#extension GL_ARB_texture_rectangle : require\n"
            "precision mediump float;\n"
            "varying vec2 v_texCoord;\n"
            "uniform vec4 texTransform;\n"
            "uniform sampler2DRect s_texture;\n"
            "uniform float alpha;\n"
            "void main()\n"
            "{\n"
            "    vec4 texColor = texture2DRect(s_texture, vec2(v_texCoord.x, texTransform.w - v_texCoord.y));\n"
            "    gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha;\n"
            "}\n";
}

String FragmentShaderRGBATexRectAlpha::getShaderString() const
{
    return "#extension GL_ARB_texture_rectangle : require\n"
            "precision mediump float;\n"
            "varying vec2 v_texCoord;\n"
            "uniform sampler2DRect s_texture;\n"
            "uniform float alpha;\n"
            "void main()\n"
            "{\n"
            "    vec4 texColor = texture2DRect(s_texture, v_texCoord);\n"
            "    gl_FragColor = texColor * alpha;\n"
            "}\n";
}

String FragmentShaderRGBATexOpaque::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(texColor.rgb, 1.0);
        }
    );
}

String FragmentShaderRGBATex::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        void main()
        {
            gl_FragColor = texture2D(s_texture, v_texCoord);
        }
    );
}

String FragmentShaderRGBATexSwizzleAlpha::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha;
        }
    );
}

String FragmentShaderRGBATexSwizzleOpaque::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, 1.0);
        }
    );
}

FragmentShaderRGBATexAlphaAA::FragmentShaderRGBATexAlphaAA()
    : m_samplerLocation(-1)
    , m_alphaLocation(-1)
    , m_edgeLocation(-1)
{
}

void FragmentShaderRGBATexAlphaAA::init(GraphicsContext3D* context, unsigned program)
{
    m_samplerLocation = context->getUniformLocation(program, "s_texture");
    m_alphaLocation = context->getUniformLocation(program, "alpha");
    m_edgeLocation = context->getUniformLocation(program, "edge");

    ASSERT(m_samplerLocation != -1 && m_alphaLocation != -1 && m_edgeLocation != -1);
}

String FragmentShaderRGBATexAlphaAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        uniform vec3 edge[8];
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = texColor * alpha * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

FragmentTexClampAlphaAABinding::FragmentTexClampAlphaAABinding()
    : m_samplerLocation(-1)
    , m_alphaLocation(-1)
    , m_fragmentTexTransformLocation(-1)
    , m_edgeLocation(-1)
{
}

void FragmentTexClampAlphaAABinding::init(GraphicsContext3D* context, unsigned program)
{
    m_samplerLocation = context->getUniformLocation(program, "s_texture");
    m_alphaLocation = context->getUniformLocation(program, "alpha");
    m_fragmentTexTransformLocation = context->getUniformLocation(program, "fragmentTexTransform");
    m_edgeLocation = context->getUniformLocation(program, "edge");

    ASSERT(m_samplerLocation != -1 && m_alphaLocation != -1 && m_fragmentTexTransformLocation != -1 && m_edgeLocation != -1);
}

String FragmentShaderRGBATexClampAlphaAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        uniform vec4 fragmentTexTransform;
        uniform vec3 edge[8];
        void main()
        {
            vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw + fragmentTexTransform.xy;
            vec4 texColor = texture2D(s_texture, texCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = texColor * alpha * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

String FragmentShaderRGBATexClampSwizzleAlphaAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        uniform vec4 fragmentTexTransform;
        uniform vec3 edge[8];
        void main()
        {
            vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw + fragmentTexTransform.xy;
            vec4 texColor = texture2D(s_texture, texCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

FragmentShaderRGBATexAlphaMask::FragmentShaderRGBATexAlphaMask()
    : m_samplerLocation(-1)
    , m_maskSamplerLocation(-1)
    , m_alphaLocation(-1)
{
}

void FragmentShaderRGBATexAlphaMask::init(GraphicsContext3D* context, unsigned program)
{
    m_samplerLocation = context->getUniformLocation(program, "s_texture");
    m_maskSamplerLocation = context->getUniformLocation(program, "s_mask");
    m_alphaLocation = context->getUniformLocation(program, "alpha");
    ASSERT(m_samplerLocation != -1 && m_maskSamplerLocation != -1 && m_alphaLocation != -1);
}

String FragmentShaderRGBATexAlphaMask::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform sampler2D s_mask;
        uniform float alpha;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            vec4 maskColor = texture2D(s_mask, v_texCoord);
            gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha * maskColor.w;
        }
    );
}

FragmentShaderRGBATexAlphaMaskAA::FragmentShaderRGBATexAlphaMaskAA()
    : m_samplerLocation(-1)
    , m_maskSamplerLocation(-1)
    , m_alphaLocation(-1)
    , m_edgeLocation(-1)
{
}

void FragmentShaderRGBATexAlphaMaskAA::init(GraphicsContext3D* context, unsigned program)
{
    m_samplerLocation = context->getUniformLocation(program, "s_texture");
    m_maskSamplerLocation = context->getUniformLocation(program, "s_mask");
    m_alphaLocation = context->getUniformLocation(program, "alpha");
    m_edgeLocation = context->getUniformLocation(program, "edge");
    ASSERT(m_samplerLocation != -1 && m_maskSamplerLocation != -1 && m_alphaLocation != -1 && m_edgeLocation != -1);
}

String FragmentShaderRGBATexAlphaMaskAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform sampler2D s_mask;
        uniform float alpha;
        uniform vec3 edge[8];
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            vec4 maskColor = texture2D(s_mask, v_texCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha * maskColor.w * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

FragmentShaderYUVVideo::FragmentShaderYUVVideo()
    : m_yTextureLocation(-1)
    , m_uTextureLocation(-1)
    , m_vTextureLocation(-1)
    , m_alphaLocation(-1)
    , m_ccMatrixLocation(-1)
    , m_yuvAdjLocation(-1)
{
}

void FragmentShaderYUVVideo::init(GraphicsContext3D* context, unsigned program)
{
    m_yTextureLocation = context->getUniformLocation(program, "y_texture");
    m_uTextureLocation = context->getUniformLocation(program, "u_texture");
    m_vTextureLocation = context->getUniformLocation(program, "v_texture");
    m_alphaLocation = context->getUniformLocation(program, "alpha");
    m_ccMatrixLocation = context->getUniformLocation(program, "cc_matrix");
    m_yuvAdjLocation = context->getUniformLocation(program, "yuv_adj");

    ASSERT(m_yTextureLocation != -1 && m_uTextureLocation != -1 && m_vTextureLocation != -1
           && m_alphaLocation != -1 && m_ccMatrixLocation != -1 && m_yuvAdjLocation != -1);
}

String FragmentShaderYUVVideo::getShaderString() const
{
    return SHADER(
        precision mediump float;
        precision mediump int;
        varying vec2 y_texCoord;
        varying vec2 uv_texCoord;
        uniform sampler2D y_texture;
        uniform sampler2D u_texture;
        uniform sampler2D v_texture;
        uniform float alpha;
        uniform vec3 yuv_adj;
        uniform mat3 cc_matrix;
        void main()
        {
            float y_raw = texture2D(y_texture, y_texCoord).x;
            float u_unsigned = texture2D(u_texture, uv_texCoord).x;
            float v_unsigned = texture2D(v_texture, uv_texCoord).x;
            vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;
            vec3 rgb = cc_matrix * yuv;
            gl_FragColor = vec4(rgb, float(1)) * alpha;
        }
    );
}

FragmentShaderColor::FragmentShaderColor()
    : m_colorLocation(-1)
{
}

void FragmentShaderColor::init(GraphicsContext3D* context, unsigned program)
{
    m_colorLocation = context->getUniformLocation(program, "color");
    ASSERT(m_colorLocation != -1);
}

String FragmentShaderColor::getShaderString() const
{
    return SHADER(
        precision mediump float;
        uniform vec4 color;
        void main()
        {
            gl_FragColor = color;
        }
    );
}

FragmentShaderCheckerboard::FragmentShaderCheckerboard()
    : m_alphaLocation(-1)
    , m_texTransformLocation(-1)
    , m_frequencyLocation(-1)
{
}

void FragmentShaderCheckerboard::init(GraphicsContext3D* context, unsigned program)
{
    m_alphaLocation = context->getUniformLocation(program, "alpha");
    m_texTransformLocation = context->getUniformLocation(program, "texTransform");
    m_frequencyLocation = context->getUniformLocation(program, "frequency");
    ASSERT(m_alphaLocation != -1 && m_texTransformLocation != -1 && m_frequencyLocation != -1);
}

String FragmentShaderCheckerboard::getShaderString() const
{
    // Shader based on Example 13-17 of "OpenGL ES 2.0 Programming Guide"
    // by Munshi, Ginsburg, Shreiner.
    return SHADER(
        precision mediump float;
        precision mediump int;
        varying vec2 v_texCoord;
        uniform float alpha;
        uniform float frequency;
        uniform vec4 texTransform;
        void main()
        {
            vec4 color1 = vec4(1.0, 1.0, 1.0, 1.0);
            vec4 color2 = vec4(0.945, 0.945, 0.945, 1.0);
            vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * texTransform.zw + texTransform.xy;
            vec2 coord = mod(floor(texCoord * frequency * 2.0), 2.0);
            float picker = abs(coord.x - coord.y);
            gl_FragColor = mix(color1, color2, picker) * alpha;
        }
    );
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
