/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief sglr-rsg adaptation.
 *//*--------------------------------------------------------------------*/

#include "glsRandomShaderProgram.hpp"
#include "rsgShader.hpp"

namespace deqp
{
namespace gls
{

using std::vector;

static rr::GenericVecType mapToGenericVecType(const rsg::VariableType &varType)
{
    if (varType.isFloatOrVec())
        return rr::GENERICVECTYPE_FLOAT;
    else if (varType.isIntOrVec())
        return rr::GENERICVECTYPE_INT32;
    else
    {
        DE_ASSERT(false);
        return rr::GENERICVECTYPE_LAST;
    }
}

static glu::DataType mapToBasicType(const rsg::VariableType &varType)
{
    if (varType.isFloatOrVec() || varType.isIntOrVec() || varType.isBoolOrVec())
    {
        const glu::DataType scalarType = varType.isFloatOrVec() ? glu::TYPE_FLOAT :
                                         varType.isIntOrVec()   ? glu::TYPE_INT :
                                         varType.isBoolOrVec()  ? glu::TYPE_BOOL :
                                                                  glu::TYPE_LAST;
        const int numComps             = varType.getNumElements();

        DE_ASSERT(de::inRange(numComps, 1, 4));
        return glu::DataType(scalarType + numComps - 1);
    }
    else if (varType.getBaseType() == rsg::VariableType::TYPE_SAMPLER_2D)
        return glu::TYPE_SAMPLER_2D;
    else if (varType.getBaseType() == rsg::VariableType::TYPE_SAMPLER_CUBE)
        return glu::TYPE_SAMPLER_CUBE;
    else
    {
        DE_ASSERT(false);
        return glu::TYPE_LAST;
    }
}

static void generateProgramDeclaration(sglr::pdec::ShaderProgramDeclaration &decl, const rsg::Shader &vertexShader,
                                       const rsg::Shader &fragmentShader, int numUnifiedUniforms,
                                       const rsg::ShaderInput *const *unifiedUniforms)
{
    decl << sglr::pdec::VertexSource(vertexShader.getSource())
         << sglr::pdec::FragmentSource(fragmentShader.getSource());

    for (vector<rsg::ShaderInput *>::const_iterator vtxInIter = vertexShader.getInputs().begin();
         vtxInIter != vertexShader.getInputs().end(); ++vtxInIter)
    {
        const rsg::ShaderInput *vertexInput = *vtxInIter;
        decl << sglr::pdec::VertexAttribute(vertexInput->getVariable()->getName(),
                                            mapToGenericVecType(vertexInput->getVariable()->getType()));
    }

    for (vector<rsg::ShaderInput *>::const_iterator fragInIter = fragmentShader.getInputs().begin();
         fragInIter != fragmentShader.getInputs().end(); ++fragInIter)
    {
        const rsg::ShaderInput *fragInput = *fragInIter;
        decl << sglr::pdec::VertexToFragmentVarying(mapToGenericVecType(fragInput->getVariable()->getType()));
    }

    for (int uniformNdx = 0; uniformNdx < numUnifiedUniforms; uniformNdx++)
    {
        const rsg::ShaderInput *uniform = unifiedUniforms[uniformNdx];
        decl << sglr::pdec::Uniform(uniform->getVariable()->getName(),
                                    mapToBasicType(uniform->getVariable()->getType()));
    }

    decl << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT);
}

static sglr::pdec::ShaderProgramDeclaration generateProgramDeclaration(const rsg::Shader &vertexShader,
                                                                       const rsg::Shader &fragmentShader,
                                                                       int numUnifiedUniforms,
                                                                       const rsg::ShaderInput *const *unifiedUniforms)
{
    sglr::pdec::ShaderProgramDeclaration decl;
    generateProgramDeclaration(decl, vertexShader, fragmentShader, numUnifiedUniforms, unifiedUniforms);
    return decl;
}

static const rsg::Variable *findShaderOutputByName(const rsg::Shader &shader, const char *name)
{
    vector<const rsg::Variable *> outputs;
    shader.getOutputs(outputs);

    for (vector<const rsg::Variable *>::const_iterator iter = outputs.begin(); iter != outputs.end(); ++iter)
    {
        if (strcmp((*iter)->getName(), name) == 0)
            return *iter;
    }

    return nullptr;
}

static const rsg::Variable *findShaderOutputByLocation(const rsg::Shader &shader, int location)
{
    vector<const rsg::Variable *> outputs;
    shader.getOutputs(outputs);

    for (vector<const rsg::Variable *>::const_iterator iter = outputs.begin(); iter != outputs.end(); iter++)
    {
        if ((*iter)->getLayoutLocation() == location)
            return *iter;
    }

    return nullptr;
}

RandomShaderProgram::RandomShaderProgram(const rsg::Shader &vertexShader, const rsg::Shader &fragmentShader,
                                         int numUnifiedUniforms, const rsg::ShaderInput *const *unifiedUniforms)
    : sglr::ShaderProgram(generateProgramDeclaration(vertexShader, fragmentShader, numUnifiedUniforms, unifiedUniforms))
    , m_vertexShader(vertexShader)
    , m_fragmentShader(fragmentShader)
    , m_numUnifiedUniforms(numUnifiedUniforms)
    , m_unifiedUniforms(unifiedUniforms)
    , m_positionVar(findShaderOutputByName(vertexShader, "gl_Position"))
    , m_fragColorVar(findShaderOutputByLocation(fragmentShader, 0))
    , m_execCtx(m_sampler2DMap, m_samplerCubeMap)
{
    TCU_CHECK_INTERNAL(m_positionVar && m_positionVar->getType().getBaseType() == rsg::VariableType::TYPE_FLOAT &&
                       m_positionVar->getType().getNumElements() == 4);
    TCU_CHECK_INTERNAL(m_fragColorVar && m_fragColorVar->getType().getBaseType() == rsg::VariableType::TYPE_FLOAT &&
                       m_fragColorVar->getType().getNumElements() == 4);

    // Build list of vertex outputs.
    for (vector<rsg::ShaderInput *>::const_iterator fragInIter = fragmentShader.getInputs().begin();
         fragInIter != fragmentShader.getInputs().end(); ++fragInIter)
    {
        const rsg::ShaderInput *fragInput = *fragInIter;
        const rsg::Variable *vertexOutput = findShaderOutputByName(vertexShader, fragInput->getVariable()->getName());

        TCU_CHECK_INTERNAL(vertexOutput);
        m_vertexOutputs.push_back(vertexOutput);
    }
}

void RandomShaderProgram::refreshUniforms(void) const
{
    DE_ASSERT(m_numUnifiedUniforms == (int)m_uniforms.size());

    for (int uniformNdx = 0; uniformNdx < m_numUnifiedUniforms; uniformNdx++)
    {
        const rsg::Variable *uniformVar      = m_unifiedUniforms[uniformNdx]->getVariable();
        const rsg::VariableType &uniformType = uniformVar->getType();
        const sglr::UniformSlot &uniformSlot = m_uniforms[uniformNdx];

        m_execCtx.getValue(uniformVar) =
            rsg::ConstValueAccess(uniformType, (const rsg::Scalar *)&uniformSlot.value).value();
    }
}

void RandomShaderProgram::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                        const int numPackets) const
{
    // \todo [2013-12-13 pyry] Do only when necessary.
    refreshUniforms();

    int packetOffset = 0;

    while (packetOffset < numPackets)
    {
        const int numToExecute = de::min(numPackets - packetOffset, (int)rsg::EXEC_VEC_WIDTH);

        // Fetch attributes.
        for (int attribNdx = 0; attribNdx < (int)m_vertexShader.getInputs().size(); ++attribNdx)
        {
            const rsg::Variable *attribVar      = m_vertexShader.getInputs()[attribNdx]->getVariable();
            const rsg::VariableType &attribType = attribVar->getType();
            const int numComponents             = attribType.getNumElements();
            rsg::ExecValueAccess access         = m_execCtx.getValue(attribVar);

            DE_ASSERT(attribType.isFloatOrVec() && de::inRange(numComponents, 1, 4));

            for (int ndx = 0; ndx < numToExecute; ndx++)
            {
                const int packetNdx            = ndx + packetOffset;
                const rr::VertexPacket *packet = packets[packetNdx];
                const tcu::Vec4 attribValue =
                    rr::readVertexAttribFloat(inputs[attribNdx], packet->instanceNdx, packet->vertexNdx);

                access.component(0).asFloat(ndx) = attribValue[0];
                if (numComponents >= 2)
                    access.component(1).asFloat(ndx) = attribValue[1];
                if (numComponents >= 3)
                    access.component(2).asFloat(ndx) = attribValue[2];
                if (numComponents >= 4)
                    access.component(3).asFloat(ndx) = attribValue[3];
            }
        }

        m_vertexShader.execute(m_execCtx);

        // Store position
        {
            const rsg::ExecConstValueAccess access = m_execCtx.getValue(m_positionVar);

            for (int ndx = 0; ndx < numToExecute; ndx++)
            {
                const int packetNdx      = ndx + packetOffset;
                rr::VertexPacket *packet = packets[packetNdx];

                packet->position[0] = access.component(0).asFloat(ndx);
                packet->position[1] = access.component(1).asFloat(ndx);
                packet->position[2] = access.component(2).asFloat(ndx);
                packet->position[3] = access.component(3).asFloat(ndx);
            }
        }

        // Other varyings
        for (int varNdx = 0; varNdx < (int)m_vertexOutputs.size(); varNdx++)
        {
            const rsg::Variable *var               = m_vertexOutputs[varNdx];
            const rsg::VariableType &varType       = var->getType();
            const int numComponents                = varType.getNumElements();
            const rsg::ExecConstValueAccess access = m_execCtx.getValue(var);

            DE_ASSERT(varType.isFloatOrVec() && de::inRange(numComponents, 1, 4));

            for (int ndx = 0; ndx < numToExecute; ndx++)
            {
                const int packetNdx            = ndx + packetOffset;
                rr::VertexPacket *const packet = packets[packetNdx];
                float *const dst               = packet->outputs[varNdx].getAccess<float>();

                dst[0] = access.component(0).asFloat(ndx);
                if (numComponents >= 2)
                    dst[1] = access.component(1).asFloat(ndx);
                if (numComponents >= 3)
                    dst[2] = access.component(2).asFloat(ndx);
                if (numComponents >= 4)
                    dst[3] = access.component(3).asFloat(ndx);
            }
        }

        packetOffset += numToExecute;
    }
}

void RandomShaderProgram::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                         const rr::FragmentShadingContext &context) const
{
    const rsg::ExecConstValueAccess fragColorAccess = m_execCtx.getValue(m_fragColorVar);
    int packetOffset                                = 0;

    DE_STATIC_ASSERT(rsg::EXEC_VEC_WIDTH % rr::NUM_FRAGMENTS_PER_PACKET == 0);

    while (packetOffset < numPackets)
    {
        const int numPacketsToExecute =
            de::min(numPackets - packetOffset, (int)rsg::EXEC_VEC_WIDTH / (int)rr::NUM_FRAGMENTS_PER_PACKET);

        // Interpolate varyings.
        for (int varNdx = 0; varNdx < (int)m_fragmentShader.getInputs().size(); ++varNdx)
        {
            const rsg::Variable *var         = m_fragmentShader.getInputs()[varNdx]->getVariable();
            const rsg::VariableType &varType = var->getType();
            const int numComponents          = varType.getNumElements();
            rsg::ExecValueAccess access      = m_execCtx.getValue(var);

            DE_ASSERT(varType.isFloatOrVec() && de::inRange(numComponents, 1, 4));

            for (int packetNdx = 0; packetNdx < numPacketsToExecute; packetNdx++)
            {
                const rr::FragmentPacket &packet = packets[packetOffset + packetNdx];

                for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; fragNdx++)
                {
                    const tcu::Vec4 varValue = rr::readVarying<float>(packet, context, varNdx, fragNdx);
                    const int dstNdx         = packetNdx * rr::NUM_FRAGMENTS_PER_PACKET + fragNdx;

                    access.component(0).asFloat(dstNdx) = varValue[0];
                    if (numComponents >= 2)
                        access.component(1).asFloat(dstNdx) = varValue[1];
                    if (numComponents >= 3)
                        access.component(2).asFloat(dstNdx) = varValue[2];
                    if (numComponents >= 4)
                        access.component(3).asFloat(dstNdx) = varValue[3];
                }
            }
        }

        m_fragmentShader.execute(m_execCtx);

        // Store color
        for (int packetNdx = 0; packetNdx < numPacketsToExecute; packetNdx++)
        {
            for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; fragNdx++)
            {
                const int srcNdx = packetNdx * rr::NUM_FRAGMENTS_PER_PACKET + fragNdx;
                const tcu::Vec4 color(
                    fragColorAccess.component(0).asFloat(srcNdx), fragColorAccess.component(1).asFloat(srcNdx),
                    fragColorAccess.component(2).asFloat(srcNdx), fragColorAccess.component(3).asFloat(srcNdx));

                rr::writeFragmentOutput(context, packetOffset + packetNdx, fragNdx, 0, color);
            }
        }

        packetOffset += numPacketsToExecute;
    }
}

} // namespace gls
} // namespace deqp
