#ifndef _GLUPIXELTRANSFER_HPP
#define _GLUPIXELTRANSFER_HPP
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
 * \brief OpenGL ES Pixel Transfer Utilities.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"

namespace tcu
{

class ConstPixelBufferAccess;
class PixelBufferAccess;
class Surface;

} // namespace tcu

namespace glu
{

class RenderContext;

void readPixels(const RenderContext &context, int x, int y, const tcu::PixelBufferAccess &dst);
void texImage2D(const RenderContext &context, uint32_t target, int level, uint32_t internalFormat,
                const tcu::ConstPixelBufferAccess &src);
void texImage3D(const RenderContext &context, uint32_t target, int level, uint32_t internalFormat,
                const tcu::ConstPixelBufferAccess &src);
void texSubImage2D(const RenderContext &context, uint32_t target, int level, int x, int y,
                   const tcu::ConstPixelBufferAccess &src);
void texSubImage3D(const RenderContext &context, uint32_t target, int level, int x, int y, int z,
                   const tcu::ConstPixelBufferAccess &src);

} // namespace glu

#endif // _GLUPIXELTRANSFER_HPP
