#ifndef _TCUNULLCONTEXTFACTORY_HPP
#define _TCUNULLCONTEXTFACTORY_HPP
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
 * \brief Null GL Context Factory.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluContextFactory.hpp"

namespace tcu
{
namespace null
{

class NullGLContextFactory : public glu::ContextFactory
{
public:
    NullGLContextFactory(void);
    glu::RenderContext *createContext(const glu::RenderConfig &config, const tcu::CommandLine &,
                                      const glu::RenderContext *) const;
};

} // namespace null
} // namespace tcu

#endif // _TCUNULLCONTEXTFACTORY_HPP
