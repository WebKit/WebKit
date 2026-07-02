#ifndef _TCUWGLCONTEXTFACTORY_HPP
#define _TCUWGLCONTEXTFACTORY_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief WGL GL context factory.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluContextFactory.hpp"
#include "tcuWGL.hpp"

namespace tcu
{
namespace wgl
{

class ContextFactory : public glu::ContextFactory
{
public:
    ContextFactory(HINSTANCE instance);
    virtual glu::RenderContext *createContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine,
                                              const glu::RenderContext *sharedContext) const;

private:
    const HINSTANCE m_instance;
    Core m_wglCore;
};

} // namespace wgl
} // namespace tcu

#endif // _TCUWGLCONTEXTFACTORY_HPP
