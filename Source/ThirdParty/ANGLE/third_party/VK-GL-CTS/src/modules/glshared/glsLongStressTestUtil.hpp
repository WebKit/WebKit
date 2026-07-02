#ifndef _GLSLONGSTRESSTESTUTIL_HPP
#define _GLSLONGSTRESSTESTUTIL_HPP
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
 * \brief Utilities for tests with gls::LongStressCase.
 *//*--------------------------------------------------------------------*/

#include "glsLongStressCase.hpp"
#include "gluShaderUtil.hpp"

#include <map>
#include <string>

namespace deqp
{
namespace gls
{
namespace LongStressTestUtil
{

class ProgramLibrary
{
public:
    ProgramLibrary(glu::GLSLVersion glslVersion);

    gls::ProgramContext generateBufferContext(int numUnusedAttributes) const;
    gls::ProgramContext generateTextureContext(int numTextureObjects, int texWid, int texHei,
                                               float positionFactor) const;
    gls::ProgramContext generateBufferAndTextureContext(int numTextures, int texWid, int texHei) const;
    gls::ProgramContext generateFragmentPointLightContext(int texWid, int texHei) const;
    gls::ProgramContext generateVertexUniformLoopLightContext(int texWid, int texHei) const;

private:
    std::string substitute(const std::string &) const;
    std::string substitute(const std::string &, const std::map<std::string, std::string> &) const;

    glu::GLSLVersion m_glslVersion;
};

} // namespace LongStressTestUtil
} // namespace gls
} // namespace deqp

#endif // _GLSLONGSTRESSTESTUTIL_HPP
