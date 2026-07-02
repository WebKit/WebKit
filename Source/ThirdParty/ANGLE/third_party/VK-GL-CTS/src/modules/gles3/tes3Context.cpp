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
 * \brief OpenGL ES 3.0 test context.
 *//*--------------------------------------------------------------------*/

#include "tes3Context.hpp"
#include "gluRenderContext.hpp"
#include "gluRenderConfig.hpp"
#include "gluFboRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "tcuCommandLine.hpp"
#include "glwWrapper.hpp"

namespace deqp
{
namespace gles3
{

Context::Context(tcu::TestContext &testCtx, glu::ApiType apiType)
    : m_testCtx(testCtx)
    , m_renderCtx(nullptr)
    , m_contextInfo(nullptr)
    , m_apiType(apiType)
{
    try
    {
        m_renderCtx   = glu::createDefaultRenderContext(m_testCtx.getPlatform(), m_testCtx.getCommandLine(), m_apiType);
        m_contextInfo = glu::ContextInfo::create(*m_renderCtx);

        // Set up function table for transparent wrapper.
        glw::setCurrentThreadFunctions(&m_renderCtx->getFunctions());
    }
    catch (...)
    {
        glw::setCurrentThreadFunctions(nullptr);

        delete m_contextInfo;
        delete m_renderCtx;

        throw;
    }
}

Context::~Context(void)
{
    // Remove functions from wrapper.
    glw::setCurrentThreadFunctions(nullptr);

    delete m_contextInfo;
    delete m_renderCtx;
}

const tcu::RenderTarget &Context::getRenderTarget(void) const
{
    return m_renderCtx->getRenderTarget();
}

} // namespace gles3
} // namespace deqp
