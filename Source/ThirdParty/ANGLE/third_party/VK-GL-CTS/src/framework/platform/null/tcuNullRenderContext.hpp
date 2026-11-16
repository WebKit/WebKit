#ifndef _TCUNULLRENDERCONTEXT_HPP
#define _TCUNULLRENDERCONTEXT_HPP
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
 * \brief Render context implementation that does no rendering.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"
#include "glwFunctions.hpp"

namespace glu
{

class Platform;
struct RenderConfig;

} // namespace glu

namespace tcu
{
namespace null
{

class Context;

/*--------------------------------------------------------------------*//*!
 * \brief Dummy render context.
 *
 * Dummy render context implements basic object management functionality
 * but doesn't do actual rendering. It is intended to be used for
 * checking test code for memory access and initialization bugs.
 *//*--------------------------------------------------------------------*/
class RenderContext : public glu::RenderContext
{
public:
    RenderContext(const glu::RenderConfig &config);
    virtual ~RenderContext(void);

    virtual glu::ContextType getType(void) const
    {
        return m_ctxType;
    }
    virtual const glw::Functions &getFunctions(void) const
    {
        return m_functions;
    }
    virtual const tcu::RenderTarget &getRenderTarget(void) const
    {
        return m_renderTarget;
    }
    virtual uint32_t getDefaultFramebuffer(void) const
    {
        return 0;
    }

    virtual void postIterate(void);

    virtual void makeCurrent(void);

private:
    const glu::ContextType m_ctxType;
    const tcu::RenderTarget m_renderTarget;

    Context *m_context;
    glw::Functions m_functions;
};

} // namespace null
} // namespace tcu

#endif // _TCUNULLRENDERCONTEXT_HPP
