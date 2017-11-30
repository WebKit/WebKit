//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer11.h: Defines the rx::Buffer11 class which implements rx::BufferImpl via rx::BufferD3D.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_BUFFER11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_BUFFER11_H_

#include <array>
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
struct PackPixelsParams;
class Renderer11;
struct SourceIndexData;
struct TranslatedAttribute;

// The order of this enum governs priority of 'getLatestBufferStorage'.
enum BufferUsage
{
    BUFFER_USAGE_SYSTEM_MEMORY,
    BUFFER_USAGE_STAGING,
    BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK,
    BUFFER_USAGE_INDEX,
    // TODO: possibly share this buffer type with shader storage buffers.
    BUFFER_USAGE_INDIRECT,
    BUFFER_USAGE_PIXEL_UNPACK,
    BUFFER_USAGE_PIXEL_PACK,
    BUFFER_USAGE_UNIFORM,
    BUFFER_USAGE_EMULATED_INDEXED_VERTEX,

    BUFFER_USAGE_COUNT,
};

typedef size_t DataRevision;

class Buffer11 : public BufferD3D
{
  public:
    Buffer11(const gl::BufferState &state, Renderer11 *renderer);
    ~Buffer11() override;

    gl::ErrorOrResult<ID3D11Buffer *> getBuffer(const gl::Context *context, BufferUsage usage);
    gl::ErrorOrResult<ID3D11Buffer *> getEmulatedIndexedBuffer(const gl::Context *context,
                                                               SourceIndexData *indexInfo,
                                                               const TranslatedAttribute &attribute,
                                                               GLint startVertex);
    gl::Error getConstantBufferRange(const gl::Context *context,
                                     GLintptr offset,
                                     GLsizeiptr size,
                                     const d3d11::Buffer **bufferOut,
                                     UINT *firstConstantOut,
                                     UINT *numConstantsOut);
    gl::ErrorOrResult<const d3d11::ShaderResourceView *> getSRV(const gl::Context *context,
                                                                DXGI_FORMAT srvFormat);
    bool isMapped() const { return mMappedStorage != nullptr; }
    gl::Error packPixels(const gl::Context *context,
                         const gl::FramebufferAttachment &readAttachment,
                         const PackPixelsParams &params);
    size_t getTotalCPUBufferMemoryBytes() const;

    // BufferD3D implementation
    size_t getSize() const override;
    bool supportsDirectBinding() const override;
    gl::Error getData(const gl::Context *context, const uint8_t **outData) override;
    void initializeStaticData(const gl::Context *context) override;
    void invalidateStaticData(const gl::Context *context) override;

    // BufferImpl implementation
    gl::Error setData(const gl::Context *context,
                      gl::BufferBinding target,
                      const void *data,
                      size_t size,
                      gl::BufferUsage usage) override;
    gl::Error setSubData(const gl::Context *context,
                         gl::BufferBinding target,
                         const void *data,
                         size_t size,
                         size_t offset) override;
    gl::Error copySubData(const gl::Context *context,
                          BufferImpl *source,
                          GLintptr sourceOffset,
                          GLintptr destOffset,
                          GLsizeiptr size) override;
    gl::Error map(const gl::Context *context, GLenum access, void **mapPtr) override;
    gl::Error mapRange(const gl::Context *context,
                       size_t offset,
                       size_t length,
                       GLbitfield access,
                       void **mapPtr) override;
    gl::Error unmap(const gl::Context *context, GLboolean *result) override;
    gl::Error markTransformFeedbackUsage(const gl::Context *context) override;

    // We use two set of dirty events. Static buffers are marked dirty whenever
    // data changes, because they must be re-translated. Direct buffers only need to be
    // updated when the underlying ID3D11Buffer pointer changes - hopefully far less often.
    OnBufferDataDirtyChannel *getStaticBroadcastChannel();
    OnBufferDataDirtyChannel *getDirectBroadcastChannel();

  private:
    class BufferStorage;
    class EmulatedIndexedStorage;
    class NativeStorage;
    class PackStorage;
    class SystemMemoryStorage;

    struct ConstantBufferCacheEntry
    {
        ConstantBufferCacheEntry() : storage(nullptr), lruCount(0) {}

        BufferStorage *storage;
        unsigned int lruCount;
    };

    void markBufferUsage(BufferUsage usage);
    gl::Error garbageCollection(const gl::Context *context, BufferUsage currentUsage);
    gl::ErrorOrResult<NativeStorage *> getStagingStorage(const gl::Context *context);
    gl::ErrorOrResult<PackStorage *> getPackStorage(const gl::Context *context);
    gl::ErrorOrResult<SystemMemoryStorage *> getSystemMemoryStorage(const gl::Context *context);

    gl::Error updateBufferStorage(const gl::Context *context,
                                  BufferStorage *storage,
                                  size_t sourceOffset,
                                  size_t storageSize);
    gl::ErrorOrResult<BufferStorage *> getBufferStorage(const gl::Context *context,
                                                        BufferUsage usage);
    gl::ErrorOrResult<BufferStorage *> getLatestBufferStorage(const gl::Context *context) const;

    gl::ErrorOrResult<BufferStorage *> getConstantBufferRangeStorage(const gl::Context *context,
                                                                     GLintptr offset,
                                                                     GLsizeiptr size);

    BufferStorage *allocateStorage(BufferUsage usage);
    void updateDeallocThreshold(BufferUsage usage);

    // Free the storage if we decide it isn't being used very often.
    gl::Error checkForDeallocation(const gl::Context *context, BufferUsage usage);

    // For some cases of uniform buffer storage, we can't deallocate system memory storage.
    bool canDeallocateSystemMemory() const;

    // Updates data revisions and latest storage.
    void onCopyStorage(BufferStorage *dest, BufferStorage *source);
    void onStorageUpdate(BufferStorage *updatedStorage);

    Renderer11 *mRenderer;
    size_t mSize;

    BufferStorage *mMappedStorage;

    // Buffer storages are sorted by usage. It's important that the latest buffer storage picks
    // the lowest usage in the case where two storages are tied on data revision - this ensures
    // we never do anything dangerous like map a uniform buffer over a staging or system memory
    // copy.
    std::array<BufferStorage *, BUFFER_USAGE_COUNT> mBufferStorages;
    BufferStorage *mLatestBufferStorage;

    // These two arrays are used to track when to free unused storage.
    std::array<unsigned int, BUFFER_USAGE_COUNT> mDeallocThresholds;
    std::array<unsigned int, BUFFER_USAGE_COUNT> mIdleness;

    // Cache of D3D11 constant buffer for specific ranges of buffer data.
    // This is used to emulate UBO ranges on 11.0 devices.
    // Constant buffers are indexed by there start offset.
    typedef std::map<GLintptr /*offset*/, ConstantBufferCacheEntry> ConstantBufferCache;
    ConstantBufferCache mConstantBufferRangeStoragesCache;
    size_t mConstantBufferStorageAdditionalSize;
    unsigned int mMaxConstantBufferLruCount;

    OnBufferDataDirtyChannel mStaticBroadcastChannel;
    OnBufferDataDirtyChannel mDirectBroadcastChannel;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_BUFFER11_H_
