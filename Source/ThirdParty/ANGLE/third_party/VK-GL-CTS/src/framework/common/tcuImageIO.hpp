#ifndef _TCUIMAGEIO_HPP
#define _TCUIMAGEIO_HPP
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
 * \brief Image IO.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

namespace tcu
{

class Archive;
class TextureLevel;
class ConstPixelBufferAccess;
class CompressedTexture;

namespace ImageIO
{

void loadImage(TextureLevel &dst, const tcu::Archive &archive, const char *fileName);

void loadPNG(TextureLevel &dst, const tcu::Archive &archive, const char *fileName);
void savePNG(const ConstPixelBufferAccess &src, const char *fileName);

void loadPKM(CompressedTexture &dst, const tcu::Archive &archive, const char *fileName);

} // namespace ImageIO
} // namespace tcu

#endif // _TCUIMAGEIO_HPP
