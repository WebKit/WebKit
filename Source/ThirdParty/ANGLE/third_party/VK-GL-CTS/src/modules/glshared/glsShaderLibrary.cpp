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
 * \brief Compiler test case.
 *//*--------------------------------------------------------------------*/

#include "glsShaderLibrary.hpp"
#include "glsShaderLibraryCase.hpp"

namespace deqp
{
namespace gls
{

namespace
{

class CaseFactory : public glu::sl::ShaderCaseFactory
{
public:
    CaseFactory(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &contextInfo)
        : m_testCtx(testCtx)
        , m_renderCtx(renderCtx)
        , m_contextInfo(contextInfo)
    {
    }

    tcu::TestCaseGroup *createGroup(const std::string &name, const std::string &description,
                                    const std::vector<tcu::TestNode *> &children)
    {
        return new tcu::TestCaseGroup(m_testCtx, name.c_str(), description.c_str(), children);
    }

    tcu::TestCase *createCase(const std::string &name, const std::string &description,
                              const glu::sl::ShaderCaseSpecification &spec)
    {
        return new ShaderLibraryCase(m_testCtx, m_renderCtx, m_contextInfo, name.c_str(), description.c_str(), spec);
    }

private:
    tcu::TestContext &m_testCtx;
    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_contextInfo;
};

} // namespace

ShaderLibrary::ShaderLibrary(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                             const glu::ContextInfo &contextInfo)
    : m_testCtx(testCtx)
    , m_renderCtx(renderCtx)
    , m_contextInfo(contextInfo)
{
}

ShaderLibrary::~ShaderLibrary(void)
{
}

std::vector<tcu::TestNode *> ShaderLibrary::loadShaderFile(const char *fileName)
{
    CaseFactory caseFactory(m_testCtx, m_renderCtx, m_contextInfo);

    return glu::sl::parseFile(m_testCtx.getArchive(), fileName, &caseFactory);
}

} // namespace gls
} // namespace deqp
