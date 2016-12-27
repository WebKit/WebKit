//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Format:
//   A universal description of texture storage. Across multiple
//   renderer back-ends, there are common formats and some distinct
//   permutations, this enum encapsulates them all.

#include "libANGLE/renderer/Format.h"

#include "image_util/copyimage.h"

using namespace rx;

namespace angle
{

namespace
{

const FastCopyFunctionMap &GetFastCopyFunctionsMap(Format::ID formatID)
{
    switch (formatID)
    {
        case Format::ID::B8G8R8A8_UNORM:
        {
            static FastCopyFunctionMap fastCopyMap;
            if (fastCopyMap.empty())
            {
                fastCopyMap[gl::FormatType(GL_RGBA, GL_UNSIGNED_BYTE)] = CopyBGRA8ToRGBA8;
            }
            return fastCopyMap;
        }
        default:
        {
            static FastCopyFunctionMap emptyMap;
            return emptyMap;
        }
    }
}

}  // anonymous namespace

Format::Format(ID id,
               GLenum glFormat,
               GLenum fboFormat,
               MipGenerationFunction mipGen,
               ColorReadFunction colorRead,
               GLenum componentType,
               GLuint redBits,
               GLuint greenBits,
               GLuint blueBits,
               GLuint alphaBits,
               GLuint depthBits,
               GLuint stencilBits)
    : id(id),
      glInternalFormat(glFormat),
      fboImplementationInternalFormat(fboFormat),
      mipGenerationFunction(mipGen),
      colorReadFunction(colorRead),
      fastCopyFunctions(GetFastCopyFunctionsMap(id)),
      componentType(componentType),
      redBits(redBits),
      greenBits(greenBits),
      blueBits(blueBits),
      alphaBits(alphaBits),
      depthBits(depthBits),
      stencilBits(stencilBits)
{
}

}  // namespace angle
