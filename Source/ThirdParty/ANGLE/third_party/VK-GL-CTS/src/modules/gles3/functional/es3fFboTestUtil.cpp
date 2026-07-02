/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief FBO test utilities.
 *//*--------------------------------------------------------------------*/

#include "es3fFboTestUtil.hpp"
#include "sglrContextUtil.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <limits>

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace FboTestUtil
{

using std::string;
using std::vector;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

static rr::GenericVecType mapDataTypeToGenericVecType(glu::DataType type)
{
    switch (type)
    {
    case glu::TYPE_FLOAT_VEC4:
        return rr::GENERICVECTYPE_FLOAT;
    case glu::TYPE_INT_VEC4:
        return rr::GENERICVECTYPE_INT32;
    case glu::TYPE_UINT_VEC4:
        return rr::GENERICVECTYPE_UINT32;
    default:
        DE_ASSERT(false);
        return rr::GENERICVECTYPE_LAST;
    }
}

template <typename T>
static tcu::Vector<T, 4> castVectorSaturate(const tcu::Vec4 &in)
{
    return tcu::Vector<T, 4>(
        ((double)in.x() + 0.5 >= (double)std::numeric_limits<T>::max()) ?
            (std::numeric_limits<T>::max()) :
            (((double)in.x() - 0.5 <= (double)std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) :
                                                                               (T(in.x()))),
        ((double)in.y() + 0.5 >= (double)std::numeric_limits<T>::max()) ?
            (std::numeric_limits<T>::max()) :
            (((double)in.y() - 0.5 <= (double)std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) :
                                                                               (T(in.y()))),
        ((double)in.z() + 0.5 >= (double)std::numeric_limits<T>::max()) ?
            (std::numeric_limits<T>::max()) :
            (((double)in.z() - 0.5 <= (double)std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) :
                                                                               (T(in.z()))),
        ((double)in.w() + 0.5 >= (double)std::numeric_limits<T>::max()) ?
            (std::numeric_limits<T>::max()) :
            (((double)in.w() - 0.5 <= (double)std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) :
                                                                               (T(in.w()))));
}

FlatColorShader::FlatColorShader(glu::DataType outputType)
    : ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                    << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                    << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                    << sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType))
                    << sglr::pdec::Uniform("u_color", glu::TYPE_FLOAT_VEC4)
                    << sglr::pdec::VertexSource("#version 300 es\n"
                                                "in highp vec4 a_position;\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    gl_Position = a_position;\n"
                                                "}\n")
                    << sglr::pdec::FragmentSource(string("#version 300 es\n"
                                                         "uniform highp vec4 u_color;\n"
                                                         "layout(location = 0) out highp ") +
                                                  glu::getDataTypeName(outputType) +
                                                  " o_color;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "    o_color = " +
                                                  glu::getDataTypeName(outputType) +
                                                  "(u_color);\n"
                                                  "}\n"))
    , m_outputType(outputType)
{
}

void FlatColorShader::setColor(sglr::Context &context, uint32_t program, const tcu::Vec4 &color)
{
    int32_t location = context.getUniformLocation(program, "u_color");

    context.useProgram(program);
    context.uniform4fv(location, 1, color.getPtr());
}

void FlatColorShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                    const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];

        packet.position = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
    }
}

void FlatColorShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                     const rr::FragmentShadingContext &context) const
{
    const tcu::Vec4 color(m_uniforms[0].value.f4);
    const tcu::IVec4 icolor  = castVectorSaturate<int32_t>(color);
    const tcu::UVec4 uicolor = castVectorSaturate<uint32_t>(color);

    DE_UNREF(packets);

    if (m_outputType == glu::TYPE_FLOAT_VEC4)
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
    }
    else if (m_outputType == glu::TYPE_INT_VEC4)
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
    }
    else if (m_outputType == glu::TYPE_UINT_VEC4)
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
    }
    else
        DE_ASSERT(false);
}

GradientShader::GradientShader(glu::DataType outputType)
    : ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                    << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                    << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
                    << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                    << sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType))
                    << sglr::pdec::Uniform("u_gradientMin", glu::TYPE_FLOAT_VEC4)
                    << sglr::pdec::Uniform("u_gradientMax", glu::TYPE_FLOAT_VEC4)
                    << sglr::pdec::VertexSource("#version 300 es\n"
                                                "in highp vec4 a_position;\n"
                                                "in highp vec4 a_coord;\n"
                                                "out highp vec4 v_coord;\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    gl_Position = a_position;\n"
                                                "    v_coord = a_coord;\n"
                                                "}\n")
                    << sglr::pdec::FragmentSource(string("#version 300 es\n"
                                                         "in highp vec4 v_coord;\n"
                                                         "uniform highp vec4 u_gradientMin;\n"
                                                         "uniform highp vec4 u_gradientMax;\n"
                                                         "layout(location = 0) out highp ") +
                                                  glu::getDataTypeName(outputType) +
                                                  " o_color;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "    highp float x = v_coord.x;\n"
                                                  "    highp float y = v_coord.y;\n"
                                                  "    highp float f0 = (x + y) * 0.5;\n"
                                                  "    highp float f1 = 0.5 + (x - y) * 0.5;\n"
                                                  "    highp vec4 fv = vec4(f0, f1, 1.0f-f0, 1.0f-f1);\n"
                                                  "    o_color = " +
                                                  glu::getDataTypeName(outputType) +
                                                  "(u_gradientMin + (u_gradientMax-u_gradientMin)*fv);\n"
                                                  "}\n"))
    , m_outputType(outputType)
{
}

void GradientShader::setGradient(sglr::Context &ctx, uint32_t program, const tcu::Vec4 &gradientMin,
                                 const tcu::Vec4 &gradientMax)
{
    ctx.useProgram(program);
    ctx.uniform4fv(ctx.getUniformLocation(program, "u_gradientMin"), 1, gradientMin.getPtr());
    ctx.uniform4fv(ctx.getUniformLocation(program, "u_gradientMax"), 1, gradientMax.getPtr());
}

void GradientShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                   const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];

        packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
    }
}

void GradientShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                    const rr::FragmentShadingContext &context) const
{
    const tcu::Vec4 gradientMin(m_uniforms[0].value.f4);
    const tcu::Vec4 gradientMax(m_uniforms[1].value.f4);

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
            const float x         = coord.x();
            const float y         = coord.y();
            const float f0        = (x + y) * 0.5f;
            const float f1        = 0.5f + (x - y) * 0.5f;
            const tcu::Vec4 fv    = Vec4(f0, f1, 1.0f - f0, 1.0f - f1);

            const tcu::Vec4 color    = gradientMin + (gradientMax - gradientMin) * fv;
            const tcu::IVec4 icolor  = castVectorSaturate<int32_t>(color);
            const tcu::UVec4 uicolor = castVectorSaturate<uint32_t>(color);

            if (m_outputType == glu::TYPE_FLOAT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            else if (m_outputType == glu::TYPE_INT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
            else if (m_outputType == glu::TYPE_UINT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
            else
                DE_ASSERT(false);
        }
}

static string genTexFragmentShader(const vector<glu::DataType> &samplerTypes, glu::DataType outputType)
{
    const char *precision = "highp";
    std::ostringstream src;

    src << "#version 300 es\n"
        << "layout(location = 0) out highp " << glu::getDataTypeName(outputType) << " o_color0;\n";

    src << "in highp vec2 v_coord;\n";

    for (int samplerNdx = 0; samplerNdx < (int)samplerTypes.size(); samplerNdx++)
    {
        src << "uniform " << precision << " " << glu::getDataTypeName(samplerTypes[samplerNdx]) << " u_sampler"
            << samplerNdx << ";\n";
        src << "uniform " << precision << " vec4 u_texScale" << samplerNdx << ";\n";
        src << "uniform " << precision << " vec4 u_texBias" << samplerNdx << ";\n";
    }

    // Output scale & bias
    src << "uniform " << precision << " vec4 u_outScale0;\n"
        << "uniform " << precision << " vec4 u_outBias0;\n";

    src << "\n"
        << "void main (void)\n"
        << "{\n"
        << "    " << precision << " vec4 out0 = vec4(0.0);\n";

    // Texture input fetch and combine.
    for (int inNdx = 0; inNdx < (int)samplerTypes.size(); inNdx++)
        src << "\tout0 += vec4("
            << "texture(u_sampler" << inNdx << ", v_coord)) * u_texScale" << inNdx << " + u_texBias" << inNdx << ";\n";

    // Write output.
    src << "    o_color0 = " << glu::getDataTypeName(outputType) << "(out0 * u_outScale0 + u_outBias0);\n";

    src << "}\n";

    return src.str();
}

static sglr::pdec::ShaderProgramDeclaration genTexture2DShaderDecl(const DataTypes &samplerTypes,
                                                                   glu::DataType outputType)
{
    sglr::pdec::ShaderProgramDeclaration decl;

    decl << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT);
    decl << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT);
    decl << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT);
    decl << sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType));

    decl << sglr::pdec::VertexSource("#version 300 es\n"
                                     "in highp vec4 a_position;\n"
                                     "in highp vec2 a_coord;\n"
                                     "out highp vec2 v_coord;\n"
                                     "void main(void)\n"
                                     "{\n"
                                     "    gl_Position = a_position;\n"
                                     "    v_coord = a_coord;\n"
                                     "}\n");
    decl << sglr::pdec::FragmentSource(genTexFragmentShader(samplerTypes.vec, outputType));

    decl << sglr::pdec::Uniform("u_outScale0", glu::TYPE_FLOAT_VEC4);
    decl << sglr::pdec::Uniform("u_outBias0", glu::TYPE_FLOAT_VEC4);

    for (size_t ndx = 0; ndx < samplerTypes.vec.size(); ++ndx)
    {
        decl << sglr::pdec::Uniform(std::string("u_sampler") + de::toString(ndx), samplerTypes.vec[ndx]);
        decl << sglr::pdec::Uniform(std::string("u_texScale") + de::toString(ndx), glu::TYPE_FLOAT_VEC4);
        decl << sglr::pdec::Uniform(std::string("u_texBias") + de::toString(ndx), glu::TYPE_FLOAT_VEC4);
    }

    return decl;
}

Texture2DShader::Texture2DShader(const DataTypes &samplerTypes, glu::DataType outputType, const Vec4 &outScale,
                                 const Vec4 &outBias)
    : sglr::ShaderProgram(genTexture2DShaderDecl(samplerTypes, outputType))
    , m_outScale(outScale)
    , m_outBias(outBias)
    , m_outputType(outputType)
{
    m_inputs.resize(samplerTypes.vec.size());

    // Initialize units.
    for (int ndx = 0; ndx < (int)m_inputs.size(); ndx++)
    {
        m_inputs[ndx].unitNdx = ndx;
        m_inputs[ndx].scale   = Vec4(1.0f);
        m_inputs[ndx].bias    = Vec4(0.0f);
    }
}

void Texture2DShader::setUnit(int inputNdx, int unitNdx)
{
    m_inputs[inputNdx].unitNdx = unitNdx;
}

void Texture2DShader::setTexScaleBias(int inputNdx, const Vec4 &scale, const Vec4 &bias)
{
    m_inputs[inputNdx].scale = scale;
    m_inputs[inputNdx].bias  = bias;
}

void Texture2DShader::setOutScaleBias(const Vec4 &scale, const Vec4 &bias)
{
    m_outScale = scale;
    m_outBias  = bias;
}

void Texture2DShader::setUniforms(sglr::Context &gl, uint32_t program) const
{
    gl.useProgram(program);

    for (int texNdx = 0; texNdx < (int)m_inputs.size(); texNdx++)
    {
        string samplerName = string("u_sampler") + de::toString(texNdx);
        string scaleName   = string("u_texScale") + de::toString(texNdx);
        string biasName    = string("u_texBias") + de::toString(texNdx);

        gl.uniform1i(gl.getUniformLocation(program, samplerName.c_str()), m_inputs[texNdx].unitNdx);
        gl.uniform4fv(gl.getUniformLocation(program, scaleName.c_str()), 1, m_inputs[texNdx].scale.getPtr());
        gl.uniform4fv(gl.getUniformLocation(program, biasName.c_str()), 1, m_inputs[texNdx].bias.getPtr());
    }

    gl.uniform4fv(gl.getUniformLocation(program, "u_outScale0"), 1, m_outScale.getPtr());
    gl.uniform4fv(gl.getUniformLocation(program, "u_outBias0"), 1, m_outBias.getPtr());
}

void Texture2DShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                    const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];

        packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
    }
}

void Texture2DShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                     const rr::FragmentShadingContext &context) const
{
    const tcu::Vec4 outScale(m_uniforms[0].value.f4);
    const tcu::Vec4 outBias(m_uniforms[1].value.f4);

    tcu::Vec2 texCoords[4];
    tcu::Vec4 colors[4];

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        // setup tex coords
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
            texCoords[fragNdx]    = tcu::Vec2(coord.x(), coord.y());
        }

        // clear result
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            colors[fragNdx] = tcu::Vec4(0.0f);

        // sample each texture
        for (int ndx = 0; ndx < (int)m_inputs.size(); ndx++)
        {
            const sglr::rc::Texture2D *tex = m_uniforms[2 + ndx * 3].sampler.tex2D;
            const tcu::Vec4 scale(m_uniforms[2 + ndx * 3 + 1].value.f4);
            const tcu::Vec4 bias(m_uniforms[2 + ndx * 3 + 2].value.f4);
            tcu::Vec4 tmpColors[4];

            tex->sample4(tmpColors, texCoords);

            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                colors[fragNdx] += tmpColors[fragNdx] * scale + bias;
        }

        // write out
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 color    = colors[fragNdx] * outScale + outBias;
            const tcu::IVec4 icolor  = castVectorSaturate<int32_t>(color);
            const tcu::UVec4 uicolor = castVectorSaturate<uint32_t>(color);

            if (m_outputType == glu::TYPE_FLOAT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            else if (m_outputType == glu::TYPE_INT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
            else if (m_outputType == glu::TYPE_UINT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
            else
                DE_ASSERT(false);
        }
    }
}

TextureCubeShader::TextureCubeShader(glu::DataType samplerType, glu::DataType outputType)
    : sglr::ShaderProgram(
          sglr::pdec::ShaderProgramDeclaration()
          << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType))
          << sglr::pdec::Uniform("u_coordMat", glu::TYPE_FLOAT_MAT3) << sglr::pdec::Uniform("u_sampler0", samplerType)
          << sglr::pdec::Uniform("u_scale", glu::TYPE_FLOAT_VEC4) << sglr::pdec::Uniform("u_bias", glu::TYPE_FLOAT_VEC4)
          << sglr::pdec::VertexSource("#version 300 es\n"
                                      "in highp vec4 a_position;\n"
                                      "in mediump vec2 a_coord;\n"
                                      "uniform mat3 u_coordMat;\n"
                                      "out mediump vec3 v_coord;\n"
                                      "void main (void)\n"
                                      "{\n"
                                      "    gl_Position = a_position;\n"
                                      "    v_coord = u_coordMat * vec3(a_coord, 1.0);\n"
                                      "}\n")
          << sglr::pdec::FragmentSource(string("") +
                                        "#version 300 es\n"
                                        "uniform highp " +
                                        glu::getDataTypeName(samplerType) +
                                        " u_sampler0;\n"
                                        "uniform highp vec4 u_scale;\n"
                                        "uniform highp vec4 u_bias;\n"
                                        "in mediump vec3 v_coord;\n"
                                        "layout(location = 0) out highp " +
                                        glu::getDataTypeName(outputType) +
                                        " o_color;\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    o_color = " +
                                        glu::getDataTypeName(outputType) +
                                        "(vec4(texture(u_sampler0, v_coord)) * u_scale + u_bias);\n"
                                        "}\n"))
    , m_texScale(1.0f)
    , m_texBias(0.0f)
    , m_outputType(outputType)
{
}

void TextureCubeShader::setFace(tcu::CubeFace face)
{
    static const float s_cubeTransforms[][3 * 3] = {// Face -X: (x, y, 1) -> (-1, -(2*y-1), +(2*x-1))
                                                    {0.0f, 0.0f, -1.0f, 0.0f, -2.0f, 1.0f, 2.0f, 0.0f, -1.0f},
                                                    // Face +X: (x, y, 1) -> (+1, -(2*y-1), -(2*x-1))
                                                    {0.0f, 0.0f, 1.0f, 0.0f, -2.0f, 1.0f, -2.0f, 0.0f, 1.0f},
                                                    // Face -Y: (x, y, 1) -> (+(2*x-1), -1, -(2*y-1))
                                                    {2.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, -2.0f, 1.0f},
                                                    // Face +Y: (x, y, 1) -> (+(2*x-1), +1, +(2*y-1))
                                                    {2.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 2.0f, -1.0f},
                                                    // Face -Z: (x, y, 1) -> (-(2*x-1), -(2*y-1), -1)
                                                    {-2.0f, 0.0f, 1.0f, 0.0f, -2.0f, 1.0f, 0.0f, 0.0f, -1.0f},
                                                    // Face +Z: (x, y, 1) -> (+(2*x-1), -(2*y-1), +1)
                                                    {2.0f, 0.0f, -1.0f, 0.0f, -2.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
    DE_ASSERT(de::inBounds<int>(face, 0, tcu::CUBEFACE_LAST));
    m_coordMat = tcu::Mat3(s_cubeTransforms[face]);
}

void TextureCubeShader::setTexScaleBias(const Vec4 &scale, const Vec4 &bias)
{
    m_texScale = scale;
    m_texBias  = bias;
}

void TextureCubeShader::setUniforms(sglr::Context &gl, uint32_t program) const
{
    gl.useProgram(program);

    gl.uniform1i(gl.getUniformLocation(program, "u_sampler0"), 0);
    gl.uniformMatrix3fv(gl.getUniformLocation(program, "u_coordMat"), 1, GL_FALSE,
                        m_coordMat.getColumnMajorData().getPtr());
    gl.uniform4fv(gl.getUniformLocation(program, "u_scale"), 1, m_texScale.getPtr());
    gl.uniform4fv(gl.getUniformLocation(program, "u_bias"), 1, m_texBias.getPtr());
}

void TextureCubeShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                      const int numPackets) const
{
    tcu::Mat3 texCoordMat = tcu::Mat3(m_uniforms[0].value.m3);

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];
        tcu::Vec2 a_coord        = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx).xy();
        tcu::Vec3 v_coord        = texCoordMat * tcu::Vec3(a_coord.x(), a_coord.y(), 1.0f);

        packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] = tcu::Vec4(v_coord.x(), v_coord.y(), v_coord.z(), 0.0f);
    }
}

void TextureCubeShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                       const rr::FragmentShadingContext &context) const
{
    const tcu::Vec4 texScale(m_uniforms[2].value.f4);
    const tcu::Vec4 texBias(m_uniforms[3].value.f4);

    tcu::Vec3 texCoords[4];
    tcu::Vec4 colors[4];

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        const sglr::rc::TextureCube *tex = m_uniforms[1].sampler.texCube;

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
            texCoords[fragNdx]    = tcu::Vec3(coord.x(), coord.y(), coord.z());
        }

        tex->sample4(colors, texCoords);

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 color    = colors[fragNdx] * texScale + texBias;
            const tcu::IVec4 icolor  = castVectorSaturate<int32_t>(color);
            const tcu::UVec4 uicolor = castVectorSaturate<uint32_t>(color);

            if (m_outputType == glu::TYPE_FLOAT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            else if (m_outputType == glu::TYPE_INT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
            else if (m_outputType == glu::TYPE_UINT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
            else
                DE_ASSERT(false);
        }
    }
}

Texture2DArrayShader::Texture2DArrayShader(glu::DataType samplerType, glu::DataType outputType)
    : sglr::ShaderProgram(
          sglr::pdec::ShaderProgramDeclaration()
          << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType))
          << sglr::pdec::Uniform("u_sampler0", samplerType) << sglr::pdec::Uniform("u_scale", glu::TYPE_FLOAT_VEC4)
          << sglr::pdec::Uniform("u_bias", glu::TYPE_FLOAT_VEC4) << sglr::pdec::Uniform("u_layer", glu::TYPE_INT)
          << sglr::pdec::VertexSource("#version 300 es\n"
                                      "in highp vec4 a_position;\n"
                                      "in highp vec2 a_coord;\n"
                                      "out highp vec2 v_coord;\n"
                                      "void main (void)\n"
                                      "{\n"
                                      "    gl_Position = a_position;\n"
                                      "    v_coord = a_coord;\n"
                                      "}\n")
          << sglr::pdec::FragmentSource(string("") +
                                        "#version 300 es\n"
                                        "uniform highp " +
                                        glu::getDataTypeName(samplerType) +
                                        " u_sampler0;\n"
                                        "uniform highp vec4 u_scale;\n"
                                        "uniform highp vec4 u_bias;\n"
                                        "uniform highp int u_layer;\n"
                                        "in highp vec2 v_coord;\n"
                                        "layout(location = 0) out highp " +
                                        glu::getDataTypeName(outputType) +
                                        " o_color;\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    o_color = " +
                                        glu::getDataTypeName(outputType) +
                                        "(vec4(texture(u_sampler0, vec3(v_coord, u_layer))) * u_scale + u_bias);\n"
                                        "}\n"))
    , m_texScale(1.0f)
    , m_texBias(0.0f)
    , m_layer(0)
    , m_outputType(outputType)
{
}

void Texture2DArrayShader::setLayer(int layer)
{
    m_layer = layer;
}

void Texture2DArrayShader::setTexScaleBias(const Vec4 &scale, const Vec4 &bias)
{
    m_texScale = scale;
    m_texBias  = bias;
}

void Texture2DArrayShader::setUniforms(sglr::Context &gl, uint32_t program) const
{
    gl.useProgram(program);

    gl.uniform1i(gl.getUniformLocation(program, "u_sampler0"), 0);
    gl.uniform1i(gl.getUniformLocation(program, "u_layer"), m_layer);
    gl.uniform4fv(gl.getUniformLocation(program, "u_scale"), 1, m_texScale.getPtr());
    gl.uniform4fv(gl.getUniformLocation(program, "u_bias"), 1, m_texBias.getPtr());
}

void Texture2DArrayShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                         const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];

        packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
    }
}

void Texture2DArrayShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                          const rr::FragmentShadingContext &context) const
{
    const tcu::Vec4 texScale(m_uniforms[1].value.f4);
    const tcu::Vec4 texBias(m_uniforms[2].value.f4);
    const int layer = m_uniforms[3].value.i;

    tcu::Vec3 texCoords[4];
    tcu::Vec4 colors[4];

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        const sglr::rc::Texture2DArray *tex = m_uniforms[0].sampler.tex2DArray;

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
            texCoords[fragNdx]    = tcu::Vec3(coord.x(), coord.y(), float(layer));
        }

        tex->sample4(colors, texCoords);

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 color    = colors[fragNdx] * texScale + texBias;
            const tcu::IVec4 icolor  = castVectorSaturate<int32_t>(color);
            const tcu::UVec4 uicolor = castVectorSaturate<uint32_t>(color);

            if (m_outputType == glu::TYPE_FLOAT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            else if (m_outputType == glu::TYPE_INT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
            else if (m_outputType == glu::TYPE_UINT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
            else
                DE_ASSERT(false);
        }
    }
}

Texture3DShader::Texture3DShader(glu::DataType samplerType, glu::DataType outputType)
    : sglr::ShaderProgram(
          sglr::pdec::ShaderProgramDeclaration()
          << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType))
          << sglr::pdec::Uniform("u_sampler0", samplerType) << sglr::pdec::Uniform("u_scale", glu::TYPE_FLOAT_VEC4)
          << sglr::pdec::Uniform("u_bias", glu::TYPE_FLOAT_VEC4) << sglr::pdec::Uniform("u_depth", glu::TYPE_FLOAT)
          << sglr::pdec::VertexSource("#version 300 es\n"
                                      "in highp vec4 a_position;\n"
                                      "in highp vec2 a_coord;\n"
                                      "out highp vec2 v_coord;\n"
                                      "void main (void)\n"
                                      "{\n"
                                      "    gl_Position = a_position;\n"
                                      "    v_coord = a_coord;\n"
                                      "}\n")
          << sglr::pdec::FragmentSource(string("") +
                                        "#version 300 es\n"
                                        "uniform highp " +
                                        glu::getDataTypeName(samplerType) +
                                        " u_sampler0;\n"
                                        "uniform highp vec4 u_scale;\n"
                                        "uniform highp vec4 u_bias;\n"
                                        "uniform highp float u_depth;\n"
                                        "in highp vec2 v_coord;\n"
                                        "layout(location = 0) out highp " +
                                        glu::getDataTypeName(outputType) +
                                        " o_color;\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    o_color = " +
                                        glu::getDataTypeName(outputType) +
                                        "(vec4(texture(u_sampler0, vec3(v_coord, u_depth))) * u_scale + u_bias);\n"
                                        "}\n"))
    , m_texScale(1.0f)
    , m_texBias(0.0f)
    , m_depth(0.0f)
    , m_outputType(outputType)
{
}

void Texture3DShader::setDepth(float depth)
{
    m_depth = depth;
}

void Texture3DShader::setTexScaleBias(const Vec4 &scale, const Vec4 &bias)
{
    m_texScale = scale;
    m_texBias  = bias;
}

void Texture3DShader::setUniforms(sglr::Context &gl, uint32_t program) const
{
    gl.useProgram(program);

    gl.uniform1i(gl.getUniformLocation(program, "u_sampler0"), 0);
    gl.uniform1f(gl.getUniformLocation(program, "u_depth"), m_depth);
    gl.uniform4fv(gl.getUniformLocation(program, "u_scale"), 1, m_texScale.getPtr());
    gl.uniform4fv(gl.getUniformLocation(program, "u_bias"), 1, m_texBias.getPtr());
}

void Texture3DShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                    const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];

        packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
    }
}

void Texture3DShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                     const rr::FragmentShadingContext &context) const
{
    const tcu::Vec4 texScale(m_uniforms[1].value.f4);
    const tcu::Vec4 texBias(m_uniforms[2].value.f4);
    const float depth = m_uniforms[3].value.f;

    tcu::Vec3 texCoords[4];
    tcu::Vec4 colors[4];

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        const sglr::rc::Texture3D *tex = m_uniforms[0].sampler.tex3D;

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
            texCoords[fragNdx]    = tcu::Vec3(coord.x(), coord.y(), depth);
        }

        tex->sample4(colors, texCoords);

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 color    = colors[fragNdx] * texScale + texBias;
            const tcu::IVec4 icolor  = castVectorSaturate<int32_t>(color);
            const tcu::UVec4 uicolor = castVectorSaturate<uint32_t>(color);

            if (m_outputType == glu::TYPE_FLOAT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            else if (m_outputType == glu::TYPE_INT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
            else if (m_outputType == glu::TYPE_UINT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
            else
                DE_ASSERT(false);
        }
    }
}

DepthGradientShader::DepthGradientShader(glu::DataType outputType)
    : ShaderProgram(
          sglr::pdec::ShaderProgramDeclaration()
          << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
          << sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType))
          << sglr::pdec::Uniform("u_maxGradient", glu::TYPE_FLOAT)
          << sglr::pdec::Uniform("u_minGradient", glu::TYPE_FLOAT)
          << sglr::pdec::Uniform("u_color", glu::TYPE_FLOAT_VEC4)
          << sglr::pdec::VertexSource("#version 300 es\n"
                                      "in highp vec4 a_position;\n"
                                      "in highp vec4 a_coord;\n"
                                      "out highp vec4 v_coord;\n"
                                      "void main (void)\n"
                                      "{\n"
                                      "    gl_Position = a_position;\n"
                                      "    v_coord = a_coord;\n"
                                      "}\n")
          << sglr::pdec::FragmentSource(string("#version 300 es\n"
                                               "in highp vec4 v_coord;\n"
                                               "uniform highp float u_minGradient;\n"
                                               "uniform highp float u_maxGradient;\n"
                                               "uniform highp vec4 u_color;\n"
                                               "layout(location = 0) out highp ") +
                                        glu::getDataTypeName(outputType) +
                                        " o_color;\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    highp float x = v_coord.x;\n"
                                        "    highp float y = v_coord.y;\n"
                                        "    highp float f0 = (x + y) * 0.5;\n"
                                        "    gl_FragDepth = u_minGradient + (u_maxGradient-u_minGradient)*f0;\n"
                                        "    o_color = " +
                                        glu::getDataTypeName(outputType) +
                                        "(u_color);\n"
                                        "}\n"))
    , m_outputType(outputType)
    , u_minGradient(getUniformByName("u_minGradient"))
    , u_maxGradient(getUniformByName("u_maxGradient"))
    , u_color(getUniformByName("u_color"))
{
}

void DepthGradientShader::setUniforms(sglr::Context &ctx, uint32_t program, const float gradientMin,
                                      const float gradientMax, const tcu::Vec4 &color)
{
    ctx.useProgram(program);
    ctx.uniform1fv(ctx.getUniformLocation(program, "u_minGradient"), 1, &gradientMin);
    ctx.uniform1fv(ctx.getUniformLocation(program, "u_maxGradient"), 1, &gradientMax);
    ctx.uniform4fv(ctx.getUniformLocation(program, "u_color"), 1, color.getPtr());
}

void DepthGradientShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                        const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *packets[packetNdx];

        packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
    }
}

void DepthGradientShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                         const rr::FragmentShadingContext &context) const
{
    const float gradientMin(u_minGradient.value.f);
    const float gradientMax(u_maxGradient.value.f);
    const tcu::Vec4 color(u_color.value.f4);
    const tcu::IVec4 icolor(castVectorSaturate<int32_t>(color));
    const tcu::UVec4 uicolor(castVectorSaturate<uint32_t>(color));

    // running this shader without a depth buffer does not make any sense
    DE_ASSERT(context.fragmentDepths);

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
        {
            const tcu::Vec4 coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
            const float x         = coord.x();
            const float y         = coord.y();
            const float f0        = (x + y) * 0.5f;

            rr::writeFragmentDepth(context, packetNdx, fragNdx, 0, gradientMin + (gradientMax - gradientMin) * f0);

            if (m_outputType == glu::TYPE_FLOAT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            else if (m_outputType == glu::TYPE_INT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
            else if (m_outputType == glu::TYPE_UINT_VEC4)
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
            else
                DE_ASSERT(false);
        }
}

void clearColorBuffer(sglr::Context &ctx, const tcu::TextureFormat &format, const tcu::Vec4 &value)
{
    const tcu::TextureChannelClass fmtClass = tcu::getTextureChannelClass(format.type);

    switch (fmtClass)
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        ctx.clearBufferfv(GL_COLOR, 0, value.getPtr());
        break;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        ctx.clearBufferuiv(GL_COLOR, 0, value.asUint().getPtr());
        break;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        ctx.clearBufferiv(GL_COLOR, 0, value.asInt().getPtr());
        break;

    default:
        DE_ASSERT(false);
    }
}

void readPixels(sglr::Context &ctx, tcu::Surface &dst, int x, int y, int width, int height,
                const tcu::TextureFormat &format, const tcu::Vec4 &scale, const tcu::Vec4 &bias)
{
    tcu::TextureFormat readFormat   = getFramebufferReadFormat(format);
    glu::TransferFormat transferFmt = glu::getTransferFormat(readFormat);
    int alignment                   = 4; // \note GL_PACK_ALIGNMENT = 4 is assumed.
    int rowSize                     = deAlign32(readFormat.getPixelSize() * width, alignment);
    vector<uint8_t> data(rowSize * height);

    ctx.readPixels(x, y, width, height, transferFmt.format, transferFmt.dataType, &data[0]);

    // Convert to surface.
    tcu::ConstPixelBufferAccess src(readFormat, width, height, 1, rowSize, 0, &data[0]);

    dst.setSize(width, height);
    tcu::PixelBufferAccess dstAccess = dst.getAccess();

    for (int yo = 0; yo < height; yo++)
        for (int xo = 0; xo < width; xo++)
            dstAccess.setPixel(src.getPixel(xo, yo) * scale + bias, xo, yo);
}

static const char *getFboIncompleteReasonName(uint32_t reason)
{
    switch (reason)
    {
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
        return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "GL_FRAMEBUFFER_UNSUPPORTED";
    case GL_FRAMEBUFFER_COMPLETE:
        return "GL_FRAMEBUFFER_COMPLETE";
    default:
        return "UNKNOWN";
    }
}

FboIncompleteException::FboIncompleteException(uint32_t reason, const char *file, int line)
    : TestError("Framebuffer is not complete", getFboIncompleteReasonName(reason), file, line)
    , m_reason(reason)
{
}

const char *getFormatName(uint32_t format)
{
    switch (format)
    {
    case GL_RGB565:
        return "rgb565";
    case GL_RGB5_A1:
        return "rgb5_a1";
    case GL_RGBA4:
        return "rgba4";
    case GL_DEPTH_COMPONENT16:
        return "depth_component16";
    case GL_STENCIL_INDEX8:
        return "stencil_index8";
    case GL_RGBA32F:
        return "rgba32f";
    case GL_RGBA32I:
        return "rgba32i";
    case GL_RGBA32UI:
        return "rgba32ui";
    case GL_RGBA16F:
        return "rgba16f";
    case GL_RGBA16I:
        return "rgba16i";
    case GL_RGBA16UI:
        return "rgba16ui";
    case GL_RGBA8:
        return "rgba8";
    case GL_RGBA8I:
        return "rgba8i";
    case GL_RGBA8UI:
        return "rgba8ui";
    case GL_SRGB8_ALPHA8:
        return "srgb8_alpha8";
    case GL_RGB10_A2:
        return "rgb10_a2";
    case GL_RGB10_A2UI:
        return "rgb10_a2ui";
    case GL_RGBA8_SNORM:
        return "rgba8_snorm";
    case GL_RGB8:
        return "rgb8";
    case GL_R11F_G11F_B10F:
        return "r11f_g11f_b10f";
    case GL_RGB32F:
        return "rgb32f";
    case GL_RGB32I:
        return "rgb32i";
    case GL_RGB32UI:
        return "rgb32ui";
    case GL_RGB16F:
        return "rgb16f";
    case GL_RGB16I:
        return "rgb16i";
    case GL_RGB16UI:
        return "rgb16ui";
    case GL_RGB8_SNORM:
        return "rgb8_snorm";
    case GL_RGB8I:
        return "rgb8i";
    case GL_RGB8UI:
        return "rgb8ui";
    case GL_SRGB8:
        return "srgb8";
    case GL_RGB9_E5:
        return "rgb9_e5";
    case GL_RG32F:
        return "rg32f";
    case GL_RG32I:
        return "rg32i";
    case GL_RG32UI:
        return "rg32ui";
    case GL_RG16F:
        return "rg16f";
    case GL_RG16I:
        return "rg16i";
    case GL_RG16UI:
        return "rg16ui";
    case GL_RG8:
        return "rg8";
    case GL_RG8I:
        return "rg8i";
    case GL_RG8UI:
        return "rg8ui";
    case GL_RG8_SNORM:
        return "rg8_snorm";
    case GL_R32F:
        return "r32f";
    case GL_R32I:
        return "r32i";
    case GL_R32UI:
        return "r32ui";
    case GL_R16F:
        return "r16f";
    case GL_R16I:
        return "r16i";
    case GL_R16UI:
        return "r16ui";
    case GL_R8:
        return "r8";
    case GL_R8I:
        return "r8i";
    case GL_R8UI:
        return "r8ui";
    case GL_R8_SNORM:
        return "r8_snorm";
    case GL_DEPTH_COMPONENT32F:
        return "depth_component32f";
    case GL_DEPTH_COMPONENT24:
        return "depth_component24";
    case GL_DEPTH32F_STENCIL8:
        return "depth32f_stencil8";
    case GL_DEPTH24_STENCIL8:
        return "depth24_stencil8";

    default:
        TCU_FAIL("Unknown format");
    }
}

glu::DataType getFragmentOutputType(const tcu::TextureFormat &format)
{
    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_FLOAT_VEC4;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_VEC4;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_VEC4;

    default:
        DE_FATAL("Unknown format");
        return glu::TYPE_LAST;
    }
}

tcu::TextureFormat getFramebufferReadFormat(const tcu::TextureFormat &format)
{
    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT);

    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT32);

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::SIGNED_INT32);

    default:
        DE_FATAL("Unknown format");
        return tcu::TextureFormat();
    }
}

static int calculateU8ConversionError(int srcBits)
{
    if (srcBits > 0)
    {
        const int clampedBits = de::clamp<int>(srcBits, 0, 8);
        const int srcMaxValue = de::max((1 << clampedBits) - 1, 1);
        const int error       = int(deFloatCeil(255.0f * 2.0f / float(srcMaxValue)));

        return de::clamp<int>(error, 0, 255);
    }
    else
        return 1;
}

tcu::RGBA getFormatThreshold(const tcu::TextureFormat &format)
{
    const tcu::IVec4 bits = tcu::getTextureFormatMantissaBitDepth(format);

    return tcu::RGBA(calculateU8ConversionError(bits.x()), calculateU8ConversionError(bits.y()),
                     calculateU8ConversionError(bits.z()), calculateU8ConversionError(bits.w()));
}

tcu::RGBA getFormatThreshold(uint32_t glFormat)
{
    const tcu::TextureFormat format = glu::mapGLInternalFormat(glFormat);

    return getFormatThreshold(format);
}

static int getToSRGB8ConversionError(int srcBits)
{
    // \note These are pre-computed based on simulation results.
    static const int errors[] = {
        1,   // 0 bits - rounding
        255, // 1 bits
        157, // 2 bits
        106, // 3 bits
        74,  // 4 bits
        51,  // 5 bits
        34,  // 6 bits
        22,  // 7 bits
        13,  // 8 bits
        7,   // 9 bits
        4,   // 10 bits
        3,   // 11 bits
        2,   // 12 bits
        // 1 from this on
    };

    DE_ASSERT(srcBits >= 0);
    if (srcBits < DE_LENGTH_OF_ARRAY(errors))
        return errors[srcBits];
    else
        return 1;
}

tcu::RGBA getToSRGBConversionThreshold(const tcu::TextureFormat &src, const tcu::TextureFormat &dst)
{
    // Only SRGB8 and SRGB8_ALPHA8 formats are supported.
    DE_ASSERT(dst.type == tcu::TextureFormat::UNORM_INT8 && tcu::isSRGB(dst));

    const tcu::IVec4 bits  = tcu::getTextureFormatMantissaBitDepth(src);
    const bool dstHasAlpha = dst.order == tcu::TextureFormat::sRGBA;

    return tcu::RGBA(getToSRGB8ConversionError(bits.x()), getToSRGB8ConversionError(bits.y()),
                     getToSRGB8ConversionError(bits.z()), dstHasAlpha ? calculateU8ConversionError(bits.w()) : 0);
}

} // namespace FboTestUtil
} // namespace Functional
} // namespace gles3
} // namespace deqp
