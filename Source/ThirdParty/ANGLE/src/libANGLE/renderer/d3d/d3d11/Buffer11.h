//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer11.h: Defines the rx::Buffer11 class which implements rx::BufferImpl via rx::BufferD3D.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_BUFFER11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_BUFFER11_H_

#include <map>

#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace gl
{
class FramebufferAttachment;
}

namespace rx
{
class Renderer11;
struct SourceIndexData;
struct TranslatedAttribute;

enum BufferUsage
{
    BUFFER_USAGE_STAGING,
    BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK,
    BUFFER_USAGE_INDEX,
    BUFFER_USAGE_PIXEL_UNPACK,
    BUFFER_USAGE_PIXEL_PACK,
    BUFFER_USAGE_UNIFORM,
    BUFFER_USAGE_SYSTEM_MEMORY,
    BUFFER_USAGE_EMULATED_INDEXED_VERTEX,

    BUFFER_USAGE_COUNT,
};

struct PackPixelsParams
{
    PackPixelsParams();
    PackPixelsParams(const gl::Rectangle &area, GLenum format, GLenum type, GLuint outputPitch,
                     const gl::PixelPackState &pack, ptrdiff_t offset);

    gl::Rectangle area;
    GLenum format;
    GLenum type;
    GLuint outputPitch;
    gl::Buffer *packBuffer;
    gl::PixelPackState pack;
    ptrdiff_t offset;
};

typedef size_t DataRevision;

class Buffer11 : public BufferD3D
{
  public:
    Buffer11(Renderer11 *renderer);
    virtual ~Buffer11();

    gl::ErrorOrResult<ID3D11Buffer *> getBuffer(BufferUsage usage);
    gl::ErrorOrResult<ID3D11Buffer *> getEmulatedIndexedBuffer(SourceIndexData *indexInfo,
                                                               const TranslatedAttribute &attribute,
                                                               GLint startVertex);
    gl::ErrorOrResult<ID3D11Buffer *> getConstantBufferRange(GLintptr offset, GLsizeiptr size);
    gl::ErrorOrResult<ID3D11ShaderResourceView *> getSRV(DXGI_FORMAT srvFormat);
    bool isMapped() const { return mMappedStorage != nullptr; }
    gl::Error packPixels(const gl::FramebufferAttachment &readAttachment,
                         const PackPixelsParams &params);
    size_t getTotalCPUBufferMemoryBytes() const;

    // BufferD3D implementation
    size_t getSize() const override { return mSize; }
    bool supportsDirectBinding() const override;
    gl::Error getData(const uint8_t **outData) override;
    void initializeStaticData() override;
    void invalidateStaticData() override;

    // BufferImpl implementation
    gl::Error setData(const void *data, size_t size, GLenum usage) override;
    gl::Error setSubData(const void *data, size_t size, size_t offset) override;
    gl::Error copySubData(BufferImpl *source,
                          GLintptr sourceOffset,
                          GLintptr destOffset,
                          GLsizeiptr size) override;
    gl::Error map(GLenum access, GLvoid **mapPtr) override;
    gl::Error mapRange(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr) override;
    gl::Error unmap(GLboolean *result) override;
    gl::Error markTransformFeedbackUsage() override;

    // We use two set of dirty callbacks for two events. Static buffers are marked dirty whenever
    // the data is changed, because they must be re-translated. Direct buffers only need to be
    // updated when the underlying ID3D11Buffer pointer changes - hopefully far less often.
    void addStaticBufferDirtyCallback(const NotificationCallback *callback);
    void removeStaticBufferDirtyCallback(const NotificationCallback *callback);

    void addDirectBufferDirtyCallback(const NotificationCallback *callback);
    void removeDirectBufferDirtyCallback(const NotificationCallback *callback);

  private:
    class BufferStorage;
    class EmulatedIndexedStorage;
    class NativeStorage;
    class PackStorage;
    class SystemMemoryStorage;

    struct ConstantBufferCacheEntry
    {
        ConstantBufferCacheEntry() : storage(nullptr), lruCount(0) { }

        BufferStorage *storage;
        unsigned int lruCount;
    };

    gl::Error markBufferUsage();
    gl::ErrorOrResult<NativeStorage *> getStagingStorage();
    gl::ErrorOrResult<PackStorage *> getPackStorage();
    gl::ErrorOrResult<SystemMemoryStorage *> getSystemMemoryStorage();

    gl::Error updateBufferStorage(BufferStorage *storage, size_t sourceOffset, size_t storageSize);
    gl::ErrorOrResult<BufferStorage *> getBufferStorage(BufferUsage usage);
    gl::ErrorOrResult<BufferStorage *> getLatestBufferStorage() const;

    gl::ErrorOrResult<BufferStorage *> getConstantBufferRangeStorage(GLintptr offset,
                                                                     GLsizeiptr size);

    BufferStorage *allocateStorage(BufferUsage usage) const;

    Renderer11 *mRenderer;
    size_t mSize;

    BufferStorage *mMappedStorage;

    std::vector<BufferStorage *> mBufferStorages;

    // Cache of D3D11 constant buffer for specific ranges of buffer data.
    // This is used to emulate UBO ranges on 11.0 devices.
    // Constant buffers are indexed by there start offset.
    typedef std::map<GLintptr /*offset*/, ConstantBufferCacheEntry> ConstantBufferCache;
    ConstantBufferCache mConstantBufferRangeStoragesCache;
    size_t mConstantBufferStorageAdditionalSize;
    unsigned int mMaxConstantBufferLruCount;

    typedef std::pair<ID3D11Buffer *, ID3D11ShaderResourceView *> BufferSRVPair;
    std::map<DXGI_FORMAT, BufferSRVPair> mBufferResourceViews;

    unsigned int mReadUsageCount;

    NotificationSet mStaticBufferDirtyCallbacks;
    NotificationSet mDirectBufferDirtyCallbacks;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_D3D11_BUFFER11_H_
