//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// InputLayoutCache.h: Defines InputLayoutCache, a class that builds and caches
// D3D11 input layouts.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_INPUTLAYOUTCACHE_H_
#define LIBANGLE_RENDERER_D3D_D3D11_INPUTLAYOUTCACHE_H_

#include <GLES2/gl2.h>

#include <cstddef>

#include <array>
#include <map>

#include "common/angleutils.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Error.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace gl
{
class Program;
}

namespace rx
{
struct TranslatedAttribute;
struct TranslatedIndexData;
struct SourceIndexData;
class ProgramD3D;

class InputLayoutCache : angle::NonCopyable
{
  public:
    InputLayoutCache();
    virtual ~InputLayoutCache();

    void initialize(ID3D11Device *device, ID3D11DeviceContext *context);
    void clear();
    void markDirty();

    gl::Error applyVertexBuffers(const gl::State &state,
                                 const std::vector<TranslatedAttribute> &vertexArrayAttribs,
                                 const std::vector<TranslatedAttribute> &currentValueAttribs,
                                 GLenum mode,
                                 GLint start,
                                 TranslatedIndexData *indexInfo,
                                 GLsizei numIndicesPerInstance);

    gl::Error updateVertexOffsetsForPointSpritesEmulation(GLint startVertex,
                                                          GLsizei emulatedInstanceId);

    // Useful for testing
    void setCacheSize(unsigned int cacheSize) { mCacheSize = cacheSize; }

  private:
    struct PackedAttributeLayout
    {
        PackedAttributeLayout()
            : numAttributes(0),
              flags(0)
        {
        }

        void addAttributeData(GLenum glType,
                              UINT semanticIndex,
                              gl::VertexFormatType vertexFormatType,
                              unsigned int divisor);

        bool operator<(const PackedAttributeLayout &other) const;

        enum Flags
        {
            FLAG_USES_INSTANCED_SPRITES     = 0x1,
            FLAG_INSTANCED_SPRITES_ACTIVE   = 0x2,
            FLAG_INSTANCED_RENDERING_ACTIVE = 0x4,
        };

        size_t numAttributes;
        unsigned int flags;
        uint32_t attributeData[gl::MAX_VERTEX_ATTRIBS];
    };

    gl::Error updateInputLayout(const gl::State &state,
                                GLenum mode,
                                const AttribIndexArray &sortedSemanticIndices,
                                GLsizei numIndicesPerInstance);
    gl::Error createInputLayout(const AttribIndexArray &sortedSemanticIndices,
                                GLenum mode,
                                gl::Program *program,
                                GLsizei numIndicesPerInstance,
                                ID3D11InputLayout **inputLayoutOut);

    std::map<PackedAttributeLayout, ID3D11InputLayout *> mLayoutMap;

    ID3D11InputLayout *mCurrentIL;
    std::array<ID3D11Buffer *, gl::MAX_VERTEX_ATTRIBS> mCurrentBuffers;
    std::array<UINT, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexStrides;
    std::array<UINT, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexOffsets;
    std::vector<const TranslatedAttribute *> mCurrentAttributes;

    ID3D11Buffer *mPointSpriteVertexBuffer;
    ID3D11Buffer *mPointSpriteIndexBuffer;

    unsigned int mCacheSize;

    ID3D11Device *mDevice;
    ID3D11DeviceContext *mDeviceContext;
    D3D_FEATURE_LEVEL mFeatureLevel;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_D3D11_INPUTLAYOUTCACHE_H_
