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

#include "tcuNullContextFactory.hpp"
#include "tcuNullRenderContext.hpp"

namespace tcu
{
namespace null
{

NullGLContextFactory::NullGLContextFactory(void) : glu::ContextFactory("null", "Null Render Context")
{
}

glu::RenderContext *NullGLContextFactory::createContext(const glu::RenderConfig &config, const tcu::CommandLine &,
                                                        const glu::RenderContext *) const
{
    return new RenderContext(config);
}

} // namespace null
} // namespace tcu
