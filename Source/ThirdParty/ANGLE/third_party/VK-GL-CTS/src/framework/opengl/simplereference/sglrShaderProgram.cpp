/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief SGLR shader program.
 *//*--------------------------------------------------------------------*/

#include "sglrShaderProgram.hpp"

namespace sglr
{
namespace pdec
{

ShaderProgramDeclaration::ShaderProgramDeclaration(void)
    : m_geometryDecl(rr::GEOMETRYSHADERINPUTTYPE_LAST, rr::GEOMETRYSHADEROUTPUTTYPE_LAST, 0, 0)
    , m_vertexShaderSet(false)
    , m_fragmentShaderSet(false)
    , m_geometryShaderSet(false)
{
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const VertexAttribute &v)
{
    m_vertexAttributes.push_back(v);
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const VertexToFragmentVarying &v)
{
    m_vertexToFragmentVaryings.push_back(v);
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const VertexToGeometryVarying &v)
{
    m_vertexToGeometryVaryings.push_back(v);
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const GeometryToFragmentVarying &v)
{
    m_geometryToFragmentVaryings.push_back(v);
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const FragmentOutput &v)
{
    m_fragmentOutputs.push_back(v);
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const Uniform &v)
{
    m_uniforms.push_back(v);
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const VertexSource &c)
{
    DE_ASSERT(!m_vertexShaderSet);
    m_vertexSource    = c.source;
    m_vertexShaderSet = true;
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const FragmentSource &c)
{
    DE_ASSERT(!m_fragmentShaderSet);
    m_fragmentSource    = c.source;
    m_fragmentShaderSet = true;
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const GeometrySource &c)
{
    DE_ASSERT(!m_geometryShaderSet);
    m_geometrySource    = c.source;
    m_geometryShaderSet = true;
    return *this;
}

ShaderProgramDeclaration &pdec::ShaderProgramDeclaration::operator<<(const GeometryShaderDeclaration &c)
{
    m_geometryDecl = c;
    return *this;
}

bool ShaderProgramDeclaration::valid(void) const
{
    if (!m_vertexShaderSet || !m_fragmentShaderSet)
        return false;

    if (m_fragmentOutputs.empty())
        return false;

    if (hasGeometryShader())
    {
        if (m_geometryDecl.inputType == rr::GEOMETRYSHADERINPUTTYPE_LAST ||
            m_geometryDecl.outputType == rr::GEOMETRYSHADEROUTPUTTYPE_LAST)
            return false;
    }
    else
    {
        if (m_geometryDecl.inputType != rr::GEOMETRYSHADERINPUTTYPE_LAST ||
            m_geometryDecl.outputType != rr::GEOMETRYSHADEROUTPUTTYPE_LAST || m_geometryDecl.numOutputVertices != 0 ||
            m_geometryDecl.numInvocations != 0)
            return false;
    }

    return true;
}

} // namespace pdec

ShaderProgram::ShaderProgram(const pdec::ShaderProgramDeclaration &decl)
    : rr::VertexShader(decl.getVertexInputCount(), decl.getVertexOutputCount())
    , rr::GeometryShader(decl.getGeometryInputCount(), decl.getGeometryOutputCount(), decl.m_geometryDecl.inputType,
                         decl.m_geometryDecl.outputType, decl.m_geometryDecl.numOutputVertices,
                         decl.m_geometryDecl.numInvocations)
    , rr::FragmentShader(decl.getFragmentInputCount(), decl.getFragmentOutputCount())
    , m_attributeNames(decl.getVertexInputCount())
    , m_uniforms(decl.m_uniforms.size())
    , m_vertSrc(decl.m_vertexSource)
    , m_fragSrc(decl.m_fragmentSource)
    , m_geomSrc(decl.hasGeometryShader() ? (decl.m_geometrySource) : (""))
    , m_hasGeometryShader(decl.hasGeometryShader())
{
    DE_ASSERT(decl.valid());

    // Set up shader IO

    for (size_t ndx = 0; ndx < decl.m_vertexAttributes.size(); ++ndx)
    {
        this->rr::VertexShader::m_inputs[ndx].type = decl.m_vertexAttributes[ndx].type;
        m_attributeNames[ndx]                      = decl.m_vertexAttributes[ndx].name;
    }

    if (m_hasGeometryShader)
    {
        for (size_t ndx = 0; ndx < decl.m_vertexToGeometryVaryings.size(); ++ndx)
        {
            this->rr::VertexShader::m_outputs[ndx].type      = decl.m_vertexToGeometryVaryings[ndx].type;
            this->rr::VertexShader::m_outputs[ndx].flatshade = decl.m_vertexToGeometryVaryings[ndx].flatshade;

            this->rr::GeometryShader::m_inputs[ndx] = this->rr::VertexShader::m_outputs[ndx];
        }
        for (size_t ndx = 0; ndx < decl.m_geometryToFragmentVaryings.size(); ++ndx)
        {
            this->rr::GeometryShader::m_outputs[ndx].type      = decl.m_geometryToFragmentVaryings[ndx].type;
            this->rr::GeometryShader::m_outputs[ndx].flatshade = decl.m_geometryToFragmentVaryings[ndx].flatshade;

            this->rr::FragmentShader::m_inputs[ndx] = this->rr::GeometryShader::m_outputs[ndx];
        }
    }
    else
    {
        for (size_t ndx = 0; ndx < decl.m_vertexToFragmentVaryings.size(); ++ndx)
        {
            this->rr::VertexShader::m_outputs[ndx].type      = decl.m_vertexToFragmentVaryings[ndx].type;
            this->rr::VertexShader::m_outputs[ndx].flatshade = decl.m_vertexToFragmentVaryings[ndx].flatshade;

            this->rr::FragmentShader::m_inputs[ndx] = this->rr::VertexShader::m_outputs[ndx];
        }
    }

    for (size_t ndx = 0; ndx < decl.m_fragmentOutputs.size(); ++ndx)
        this->rr::FragmentShader::m_outputs[ndx].type = decl.m_fragmentOutputs[ndx].type;

    // Set up uniforms

    for (size_t ndx = 0; ndx < decl.m_uniforms.size(); ++ndx)
    {
        this->m_uniforms[ndx].name = decl.m_uniforms[ndx].name;
        this->m_uniforms[ndx].type = decl.m_uniforms[ndx].type;
    }
}

ShaderProgram::~ShaderProgram(void)
{
}

const UniformSlot &ShaderProgram::getUniformByName(const char *name) const
{
    DE_ASSERT(name);

    for (size_t ndx = 0; ndx < m_uniforms.size(); ++ndx)
        if (m_uniforms[ndx].name == std::string(name))
            return m_uniforms[ndx];

    DE_FATAL("Invalid uniform name, uniform not found.");
    return m_uniforms[0];
}

void ShaderProgram::shadePrimitives(rr::GeometryEmitter &output, int verticesIn, const rr::PrimitivePacket *packets,
                                    const int numPackets, int invocationID) const
{
    DE_UNREF(output);
    DE_UNREF(verticesIn && packets && numPackets && invocationID);

    // Should never be called.
    DE_ASSERT(false);
}

} // namespace sglr
