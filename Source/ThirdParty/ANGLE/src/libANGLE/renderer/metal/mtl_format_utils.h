//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_format_utils.h:
//      Declares Format conversion utilities classes that convert from angle formats
//      to respective MTLPixelFormat and MTLVertexFormat.
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_FORMAT_UTILS_H_
#define LIBANGLE_RENDERER_METAL_MTL_FORMAT_UTILS_H_

#import <Metal/Metal.h>

#include "common/angleutils.h"
#include "libANGLE/Caps.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/copyvertex.h"

namespace rx
{
class DisplayMtl;

namespace mtl
{

struct FormatBase
{
    inline bool operator==(const FormatBase &rhs) const
    {
        return intendedFormatId == rhs.intendedFormatId && actualFormatId == rhs.actualFormatId;
    }

    inline bool operator!=(const FormatBase &rhs) const { return !((*this) == rhs); }

    const angle::Format &actualAngleFormat() const;
    const angle::Format &intendedAngleFormat() const;

    angle::FormatID actualFormatId   = angle::FormatID::NONE;
    angle::FormatID intendedFormatId = angle::FormatID::NONE;
};

// Pixel format
struct Format : public FormatBase
{
    Format() = default;

    const gl::InternalFormat &intendedInternalFormat() const;

    bool valid() const { return metalFormat != MTLPixelFormatInvalid; }

    static bool FormatRenderable(MTLPixelFormat format);
    static bool FormatCPUReadable(MTLPixelFormat format);

    MTLPixelFormat metalFormat = MTLPixelFormatInvalid;

  private:
    void init(const DisplayMtl *display, angle::FormatID intendedFormatId);

    friend class FormatTable;
};

// Vertex format
struct VertexFormat : public FormatBase
{
    VertexFormat() = default;

    MTLVertexFormat metalFormat = MTLVertexFormatInvalid;

    VertexCopyFunction vertexLoadFunction = nullptr;

  private:
    void init(angle::FormatID angleFormatId, bool tightlyPacked = false);

    friend class FormatTable;
};

class FormatTable final : angle::NonCopyable
{
  public:
    FormatTable()  = default;
    ~FormatTable() = default;

    angle::Result initialize(const DisplayMtl *display);

    void generateTextureCaps(const DisplayMtl *display,
                             gl::TextureCapsMap *capsMapOut,
                             std::vector<GLenum> *compressedFormatsOut) const;

    const Format &getPixelFormat(angle::FormatID angleFormatId) const;

    // tightlyPacked means this format will be used in a tightly packed vertex buffer.
    // In that case, it's easier to just convert everything to float to ensure
    // Metal alignment requirements between 2 elements inside the buffer will be met regardless
    // of how many components each element has.
    const VertexFormat &getVertexFormat(angle::FormatID angleFormatId, bool tightlyPacked) const;

  private:
    std::array<Format, angle::kNumANGLEFormats> mPixelFormatTable;
    // One for tightly packed buffers, one for general cases.
    std::array<VertexFormat, angle::kNumANGLEFormats> mVertexFormatTables[2];
};

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_MTL_FORMAT_UTILS_H_ */
