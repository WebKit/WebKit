#ifndef _GLSFRAGOPINTERACTIONCASE_HPP
#define _GLSFRAGOPINTERACTIONCASE_HPP
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
 * \brief Shader - render state interaction case.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "rsgShader.hpp"
#include "rsgParameters.hpp"

namespace glu
{
class RenderContext;
class ContextInfo;
} // namespace glu

namespace sglr
{
class GLContext;
}

namespace deqp
{
namespace gls
{

class RandomShaderProgram;

class FragOpInteractionCase : public tcu::TestCase
{
public:
    FragOpInteractionCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                          const char *name, const rsg::ProgramParameters &params);
    ~FragOpInteractionCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    FragOpInteractionCase(const FragOpInteractionCase &);
    FragOpInteractionCase &operator=(const FragOpInteractionCase &);

    struct ReferenceContext;

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_ctxInfo;

    rsg::ProgramParameters m_params;

    rsg::Shader m_vertexShader;
    rsg::Shader m_fragmentShader;
    std::vector<const rsg::ShaderInput *> m_unifiedUniforms;

    gls::RandomShaderProgram *m_program;

    sglr::GLContext *m_glCtx;
    ReferenceContext *m_referenceCtx;

    uint32_t m_glProgram;
    uint32_t m_refProgram;

    tcu::IVec2 m_viewportSize;

    int m_iterNdx;
};

} // namespace gls
} // namespace deqp

#endif // _GLSFRAGOPINTERACTIONCASE_HPP
