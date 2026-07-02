#ifndef _GLSRANDOMSHADERPROGRAM_HPP
#define _GLSRANDOMSHADERPROGRAM_HPP
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

#include "tcuDefs.hpp"
#include "sglrContext.hpp"
#include "rsgExecutionContext.hpp"

namespace rsg
{
class Shader;
class ShaderInput;
} // namespace rsg

namespace deqp
{
namespace gls
{

class RandomShaderProgram : public sglr::ShaderProgram
{
public:
    RandomShaderProgram(const rsg::Shader &vertexShader, const rsg::Shader &fragmentShader, int numUnifiedUniforms,
                        const rsg::ShaderInput *const *unifiedUniforms);

private:
    virtual void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                               const int numPackets) const;
    virtual void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                const rr::FragmentShadingContext &context) const;

    void refreshUniforms(void) const;

    const rsg::Shader &m_vertexShader;
    const rsg::Shader &m_fragmentShader;
    const int m_numUnifiedUniforms;
    const rsg::ShaderInput *const *m_unifiedUniforms;

    const rsg::Variable *m_positionVar;
    std::vector<const rsg::Variable *>
        m_vertexOutputs; //!< Other vertex outputs in the order they are passed to fragment shader.
    const rsg::Variable *m_fragColorVar;

    rsg::Sampler2DMap m_sampler2DMap;
    rsg::SamplerCubeMap m_samplerCubeMap;
    mutable rsg::ExecutionContext m_execCtx;
};

} // namespace gls
} // namespace deqp

#endif // _GLSRANDOMSHADERPROGRAM_HPP
