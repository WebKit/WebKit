#ifndef _SGLRREFERENCEUTILS_HPP
#define _SGLRREFERENCEUTILS_HPP
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
 * \brief Reference context utils
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "rrVertexAttrib.hpp"
#include "rrPrimitiveTypes.hpp"
#include "rrShaders.hpp"
#include "rrRenderState.hpp"
#include "gluRenderContext.hpp"

namespace sglr
{
namespace rr_util
{

rr::VertexAttribType mapGLPureIntegerVertexAttributeType(uint32_t type);
rr::VertexAttribType mapGLFloatVertexAttributeType(uint32_t type, bool normalizedInteger, int size,
                                                   glu::ContextType ctxType);
int mapGLSize(int size);
rr::PrimitiveType mapGLPrimitiveType(uint32_t type);
rr::IndexType mapGLIndexType(uint32_t type);
rr::GeometryShaderOutputType mapGLGeometryShaderOutputType(uint32_t primitive);
rr::GeometryShaderInputType mapGLGeometryShaderInputType(uint32_t primitive);
rr::TestFunc mapGLTestFunc(uint32_t func);
rr::StencilOp mapGLStencilOp(uint32_t op);
rr::BlendEquation mapGLBlendEquation(uint32_t equation);
rr::BlendEquationAdvanced mapGLBlendEquationAdvanced(uint32_t equation);
rr::BlendFunc mapGLBlendFunc(uint32_t func);

} // namespace rr_util
} // namespace sglr

#endif // _SGLRREFERENCEUTILS_HPP
