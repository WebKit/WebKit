//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// renderer_utils:
//   Helper methods pertaining to most or all back-ends.
//

#ifndef LIBANGLE_RENDERER_RENDERER_UTILS_H_
#define LIBANGLE_RENDERER_RENDERER_UTILS_H_

#include <cstdint>

#include <map>

#include "libANGLE/angletypes.h"

namespace angle
{
struct Format;
}

namespace gl
{
struct FormatType;
struct InternalFormat;
}

namespace rx
{

using MipGenerationFunction = void (*)(size_t sourceWidth,
                                       size_t sourceHeight,
                                       size_t sourceDepth,
                                       const uint8_t *sourceData,
                                       size_t sourceRowPitch,
                                       size_t sourceDepthPitch,
                                       uint8_t *destData,
                                       size_t destRowPitch,
                                       size_t destDepthPitch);

typedef void (*ColorReadFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*ColorWriteFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*ColorCopyFunction)(const uint8_t *source, uint8_t *dest);

class FastCopyFunctionMap
{
  public:
    struct Entry
    {
        GLenum format;
        GLenum type;
        ColorCopyFunction func;
    };

    constexpr FastCopyFunctionMap() : FastCopyFunctionMap(nullptr, 0) {}

    constexpr FastCopyFunctionMap(const Entry *data, size_t size) : mSize(size), mData(data) {}

    bool has(const gl::FormatType &formatType) const;
    ColorCopyFunction get(const gl::FormatType &formatType) const;

  private:
    size_t mSize;
    const Entry *mData;
};

struct PackPixelsParams
{
    PackPixelsParams();
    PackPixelsParams(const gl::Rectangle &area,
                     GLenum format,
                     GLenum type,
                     GLuint outputPitch,
                     const gl::PixelPackState &pack,
                     ptrdiff_t offset);

    gl::Rectangle area;
    GLenum format;
    GLenum type;
    GLuint outputPitch;
    gl::Buffer *packBuffer;
    gl::PixelPackState pack;
    ptrdiff_t offset;
};

void PackPixels(const PackPixelsParams &params,
                const angle::Format &sourceFormat,
                int inputPitch,
                const uint8_t *source,
                uint8_t *destination);

ColorWriteFunction GetColorWriteFunction(const gl::FormatType &formatType);
ColorCopyFunction GetFastCopyFunction(const FastCopyFunctionMap &fastCopyFunctions,
                                      const gl::FormatType &formatType);

using InitializeTextureDataFunction = void (*)(size_t width,
                                               size_t height,
                                               size_t depth,
                                               uint8_t *output,
                                               size_t outputRowPitch,
                                               size_t outputDepthPitch);

using LoadImageFunction = void (*)(size_t width,
                                   size_t height,
                                   size_t depth,
                                   const uint8_t *input,
                                   size_t inputRowPitch,
                                   size_t inputDepthPitch,
                                   uint8_t *output,
                                   size_t outputRowPitch,
                                   size_t outputDepthPitch);

struct LoadImageFunctionInfo
{
    LoadImageFunctionInfo() : loadFunction(nullptr), requiresConversion(false) {}
    LoadImageFunctionInfo(LoadImageFunction loadFunction, bool requiresConversion)
        : loadFunction(loadFunction), requiresConversion(requiresConversion)
    {
    }

    LoadImageFunction loadFunction;
    bool requiresConversion;
};

using LoadFunctionMap = LoadImageFunctionInfo (*)(GLenum);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_RENDERER_UTILS_H_
