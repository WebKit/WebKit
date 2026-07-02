#ifndef _GLUDUMMYRENDERCONTEXT_HPP
#define _GLUDUMMYRENDERCONTEXT_HPP
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
 * \brief A RenderContext representing absence of a real render context.
 *
 * \todo Remove this when the need for a render context in test case
 *         constructors no longer exists.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"
#include "glwFunctions.hpp"

namespace glu
{

/*--------------------------------------------------------------------*//*!
 * \brief RenderContext that can be used when no render context is present.
 *
 * Some patterns (e.g. a test class inheriting from glu::CallLogWrapper)
 * currently depend on having access to the glw::Functions already in test
 * case constructor; in such situations there may not be a proper render
 * context available (like in test case list dumping mode). This is a
 * simple workaround for that: a empty render context with a glw::Functions
 * containing just null pointers.
 *//*--------------------------------------------------------------------*/
class EmptyRenderContext : public RenderContext
{
public:
    explicit EmptyRenderContext(ContextType ctxType = ContextType()) : m_ctxType(ctxType)
    {
    }

    virtual ContextType getType(void) const
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
    virtual void postIterate(void)
    {
    }

private:
    const ContextType m_ctxType;
    tcu::RenderTarget m_renderTarget;
    glw::Functions m_functions;
};

} // namespace glu

#endif // _GLUDUMMYRENDERCONTEXT_HPP
