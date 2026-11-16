#ifndef _GLSSHADERLIBRARY_HPP
#define _GLSSHADERLIBRARY_HPP
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
 * \brief Shader case library.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "tcuTestCase.hpp"
#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"

#include <vector>

namespace deqp
{
namespace gls
{

class ShaderLibrary
{
public:
    ShaderLibrary(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &contextInfo);
    ~ShaderLibrary(void);

    std::vector<tcu::TestNode *> loadShaderFile(const char *fileName);

private:
    ShaderLibrary(const ShaderLibrary &);            // not allowed!
    ShaderLibrary &operator=(const ShaderLibrary &); // not allowed!

    tcu::TestContext &m_testCtx;
    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_contextInfo;
};

} // namespace gls
} // namespace deqp

#endif // _GLSSHADERLIBRARY_HPP
