#ifndef _GLSSHADERLIBRARYCASE_HPP
#define _GLSSHADERLIBRARYCASE_HPP
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
 * \brief Shader test case.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "gluShaderLibrary.hpp"
#include "tcuTestCase.hpp"

namespace glu
{
class RenderContext;
class ContextInfo;
} // namespace glu

namespace deqp
{
namespace gls
{

// ShaderCase node.

class ShaderLibraryCase : public tcu::TestCase
{
public:
    // Methods.
    ShaderLibraryCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &contextInfo,
                      const char *caseName, const char *description,
                      const glu::sl::ShaderCaseSpecification &specification);
    virtual ~ShaderLibraryCase(void);

private:
    void init(void);
    bool execute(void);
    IterateResult iterate(void);

    ShaderLibraryCase(const ShaderLibraryCase &);            // not allowed!
    ShaderLibraryCase &operator=(const ShaderLibraryCase &); // not allowed!

    // Member variables.
    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_contextInfo;
    const glu::sl::ShaderCaseSpecification m_spec;
};

} // namespace gls
} // namespace deqp

#endif // _GLSSHADERLIBRARYCASE_HPP
