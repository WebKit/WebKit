#ifndef _TES2CONTEXT_HPP
#define _TES2CONTEXT_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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
 * \brief OpenGL ES 2.0 test context.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestContext.hpp"

namespace glu
{
class RenderContext;
class ContextInfo;
} // namespace glu

namespace tcu
{
class RenderTarget;
}

namespace deqp
{
namespace gles2
{

class Context
{
public:
    Context(tcu::TestContext &testCtx);
    ~Context(void);

    tcu::TestContext &getTestContext(void)
    {
        return m_testCtx;
    }
    glu::RenderContext &getRenderContext(void)
    {
        return *m_renderCtx;
    }
    const glu::ContextInfo &getContextInfo(void)
    {
        return *m_contextInfo;
    }
    const tcu::RenderTarget &getRenderTarget(void) const;

private:
    tcu::TestContext &m_testCtx;
    glu::RenderContext *m_renderCtx;
    glu::ContextInfo *m_contextInfo;
};

} // namespace gles2
} // namespace deqp

#endif // _TES2CONTEXT_HPP
