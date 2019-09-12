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

#include <limits>
#include <map>

#include "common/angleutils.h"
#include "libANGLE/angletypes.h"

namespace angle
{
struct Format;
enum class FormatID;
}  // namespace angle

namespace gl
{
struct FormatType;
struct InternalFormat;
class State;
}  // namespace gl

namespace egl
{
class AttributeMap;
}  // namespace egl

namespace rx
{
class ContextImpl;

class ResourceSerial
{
  public:
    constexpr ResourceSerial() : mValue(kDirty) {}
    explicit constexpr ResourceSerial(uintptr_t value) : mValue(value) {}
    constexpr bool operator==(ResourceSerial other) const { return mValue == other.mValue; }
    constexpr bool operator!=(ResourceSerial other) const { return mValue != other.mValue; }

    void dirty() { mValue = kDirty; }
    void clear() { mValue = kEmpty; }

    constexpr bool valid() const { return mValue != kEmpty && mValue != kDirty; }
    constexpr bool empty() const { return mValue == kEmpty; }

  private:
    constexpr static uintptr_t kDirty = std::numeric_limits<uintptr_t>::max();
    constexpr static uintptr_t kEmpty = 0;

    uintptr_t mValue;
};

class SerialFactory;

class Serial final
{
  public:
    constexpr Serial() : mValue(kInvalid) {}
    constexpr Serial(const Serial &other) = default;
    Serial &operator=(const Serial &other) = default;

    constexpr bool operator==(const Serial &other) const
    {
        return mValue != kInvalid && mValue == other.mValue;
    }
    constexpr bool operator==(uint32_t value) const
    {
        return mValue != kInvalid && mValue == static_cast<uint64_t>(value);
    }
    constexpr bool operator!=(const Serial &other) const
    {
        return mValue == kInvalid || mValue != other.mValue;
    }
    constexpr bool operator>(const Serial &other) const { return mValue > other.mValue; }
    constexpr bool operator>=(const Serial &other) const { return mValue >= other.mValue; }
    constexpr bool operator<(const Serial &other) const { return mValue < other.mValue; }
    constexpr bool operator<=(const Serial &other) const { return mValue <= other.mValue; }

    constexpr bool operator<(uint32_t value) const { return mValue < static_cast<uint64_t>(value); }

    // Useful for serialization.
    constexpr uint64_t getValue() const { return mValue; }

  private:
    friend class SerialFactory;
    constexpr explicit Serial(uint64_t value) : mValue(value) {}
    uint64_t mValue;
    static constexpr uint64_t kInvalid = 0;
};

class SerialFactory final : angle::NonCopyable
{
  public:
    SerialFactory() : mSerial(1) {}

    Serial generate()
    {
        ASSERT(mSerial != std::numeric_limits<uint64_t>::max());
        return Serial(mSerial++);
    }

  private:
    uint64_t mSerial;
};

using MipGenerationFunction = void (*)(size_t sourceWidth,
                                       size_t sourceHeight,
                                       size_t sourceDepth,
                                       const uint8_t *sourceData,
                                       size_t sourceRowPitch,
                                       size_t sourceDepthPitch,
                                       uint8_t *destData,
                                       size_t destRowPitch,
                                       size_t destDepthPitch);

typedef void (*PixelReadFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*PixelWriteFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*PixelCopyFunction)(const uint8_t *source, uint8_t *dest);

class FastCopyFunctionMap
{
  public:
    struct Entry
    {
        angle::FormatID formatID;
        PixelCopyFunction func;
    };

    constexpr FastCopyFunctionMap() : FastCopyFunctionMap(nullptr, 0) {}

    constexpr FastCopyFunctionMap(const Entry *data, size_t size) : mSize(size), mData(data) {}

    bool has(angle::FormatID formatID) const;
    PixelCopyFunction get(angle::FormatID formatID) const;

  private:
    size_t mSize;
    const Entry *mData;
};

struct PackPixelsParams
{
    PackPixelsParams();
    PackPixelsParams(const gl::Rectangle &area,
                     const angle::Format &destFormat,
                     GLuint outputPitch,
                     bool reverseRowOrderIn,
                     gl::Buffer *packBufferIn,
                     ptrdiff_t offset);

    gl::Rectangle area;
    const angle::Format *destFormat;
    GLuint outputPitch;
    gl::Buffer *packBuffer;
    bool reverseRowOrder;
    ptrdiff_t offset;
};

void PackPixels(const PackPixelsParams &params,
                const angle::Format &sourceFormat,
                int inputPitch,
                const uint8_t *source,
                uint8_t *destination);

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
    {}

    LoadImageFunction loadFunction;
    bool requiresConversion;
};

using LoadFunctionMap = LoadImageFunctionInfo (*)(GLenum);

bool ShouldUseDebugLayers(const egl::AttributeMap &attribs);
bool ShouldUseVirtualizedContexts(const egl::AttributeMap &attribs, bool defaultValue);

void CopyImageCHROMIUM(const uint8_t *sourceData,
                       size_t sourceRowPitch,
                       size_t sourcePixelBytes,
                       size_t sourceDepthPitch,
                       PixelReadFunction pixelReadFunction,
                       uint8_t *destData,
                       size_t destRowPitch,
                       size_t destPixelBytes,
                       size_t destDepthPitch,
                       PixelWriteFunction pixelWriteFunction,
                       GLenum destUnsizedFormat,
                       GLenum destComponentType,
                       size_t width,
                       size_t height,
                       size_t depth,
                       bool unpackFlipY,
                       bool unpackPremultiplyAlpha,
                       bool unpackUnmultiplyAlpha);

// Incomplete textures are 1x1 textures filled with black, used when samplers are incomplete.
// This helper class encapsulates handling incomplete textures. Because the GL back-end
// can take advantage of the driver's incomplete textures, and because clearing multisample
// textures is so difficult, we can keep an instance of this class in the back-end instead
// of moving the logic to the Context front-end.

// This interface allows us to call-back to init a multisample texture.
class MultisampleTextureInitializer
{
  public:
    virtual ~MultisampleTextureInitializer() {}
    virtual angle::Result initializeMultisampleTextureToBlack(const gl::Context *context,
                                                              gl::Texture *glTexture) = 0;
};

class IncompleteTextureSet final : angle::NonCopyable
{
  public:
    IncompleteTextureSet();
    ~IncompleteTextureSet();

    void onDestroy(const gl::Context *context);

    angle::Result getIncompleteTexture(const gl::Context *context,
                                       gl::TextureType type,
                                       MultisampleTextureInitializer *multisampleInitializer,
                                       gl::Texture **textureOut);

  private:
    gl::TextureMap mIncompleteTextures;
};

// The return value indicate if the data was updated or not.
template <int cols, int rows>
bool SetFloatUniformMatrix(unsigned int arrayElementOffset,
                           unsigned int elementCount,
                           GLsizei countIn,
                           GLboolean transpose,
                           const GLfloat *value,
                           uint8_t *targetData);

// Helper method to de-tranpose a matrix uniform for an API query.
void GetMatrixUniform(GLenum type, GLfloat *dataOut, const GLfloat *source, bool transpose);

template <typename NonFloatT>
void GetMatrixUniform(GLenum type, NonFloatT *dataOut, const NonFloatT *source, bool transpose);

const angle::Format &GetFormatFromFormatType(GLenum format, GLenum type);

angle::Result ComputeStartVertex(ContextImpl *contextImpl,
                                 const gl::IndexRange &indexRange,
                                 GLint baseVertex,
                                 GLint *firstVertexOut);

angle::Result GetVertexRangeInfo(const gl::Context *context,
                                 GLint firstVertex,
                                 GLsizei vertexOrIndexCount,
                                 gl::DrawElementsType indexTypeOrInvalid,
                                 const void *indices,
                                 GLint baseVertex,
                                 GLint *startVertexOut,
                                 size_t *vertexCountOut);

gl::Rectangle ClipRectToScissor(const gl::State &glState, const gl::Rectangle &rect, bool invertY);
}  // namespace rx

#endif  // LIBANGLE_RENDERER_RENDERER_UTILS_H_
