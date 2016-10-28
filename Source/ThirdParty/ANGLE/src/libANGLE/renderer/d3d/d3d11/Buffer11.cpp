//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer11.cpp Defines the Buffer11 class.

#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"

#include <memory>

#include "common/MemoryBuffer.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"

namespace rx
{

namespace
{

template <typename T>
GLuint ReadIndexValueFromIndices(const uint8_t *data, size_t index)
{
    return reinterpret_cast<const T *>(data)[index];
}
typedef GLuint (*ReadIndexValueFunction)(const uint8_t *data, size_t index);

enum class CopyResult
{
    RECREATED,
    NOT_RECREATED,
};

}  // anonymous namespace

namespace gl_d3d11
{

D3D11_MAP GetD3DMapTypeFromBits(BufferUsage usage, GLbitfield access)
{
    bool readBit  = ((access & GL_MAP_READ_BIT) != 0);
    bool writeBit = ((access & GL_MAP_WRITE_BIT) != 0);

    ASSERT(readBit || writeBit);

    // Note : we ignore the discard bit, because in D3D11, staging buffers
    //  don't accept the map-discard flag (discard only works for DYNAMIC usage)

    if (readBit && !writeBit)
    {
        return D3D11_MAP_READ;
    }
    else if (writeBit && !readBit)
    {
        // Special case for uniform storage - we only allow full buffer updates.
        return usage == BUFFER_USAGE_UNIFORM ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
    }
    else if (writeBit && readBit)
    {
        return D3D11_MAP_READ_WRITE;
    }
    else
    {
        UNREACHABLE();
        return D3D11_MAP_READ;
    }
}
}  // namespace gl_d3d11

// Each instance of Buffer11::BufferStorage is specialized for a class of D3D binding points
// - vertex/transform feedback buffers
// - index buffers
// - pixel unpack buffers
// - uniform buffers
class Buffer11::BufferStorage : angle::NonCopyable
{
  public:
    virtual ~BufferStorage() {}

    DataRevision getDataRevision() const { return mRevision; }
    BufferUsage getUsage() const { return mUsage; }
    size_t getSize() const { return mBufferSize; }
    void setDataRevision(DataRevision rev) { mRevision = rev; }

    virtual bool isMappable(GLbitfield access) const = 0;

    virtual gl::ErrorOrResult<CopyResult> copyFromStorage(BufferStorage *source,
                                                          size_t sourceOffset,
                                                          size_t size,
                                                          size_t destOffset) = 0;
    virtual gl::Error resize(size_t size, bool preserveData) = 0;

    virtual gl::Error map(size_t offset,
                          size_t length,
                          GLbitfield access,
                          uint8_t **mapPointerOut) = 0;
    virtual void unmap() = 0;

    gl::Error setData(const uint8_t *data, size_t offset, size_t size);

  protected:
    BufferStorage(Renderer11 *renderer, BufferUsage usage);

    Renderer11 *mRenderer;
    DataRevision mRevision;
    const BufferUsage mUsage;
    size_t mBufferSize;
};

// A native buffer storage represents an underlying D3D11 buffer for a particular
// type of storage.
class Buffer11::NativeStorage : public Buffer11::BufferStorage
{
  public:
    NativeStorage(Renderer11 *renderer,
                  BufferUsage usage,
                  const angle::BroadcastChannel *onStorageChanged);
    ~NativeStorage() override;

    bool isMappable(GLbitfield access) const override;

    ID3D11Buffer *getNativeStorage() const { return mNativeStorage; }
    gl::ErrorOrResult<CopyResult> copyFromStorage(BufferStorage *source,
                                                  size_t sourceOffset,
                                                  size_t size,
                                                  size_t destOffset) override;
    gl::Error resize(size_t size, bool preserveData) override;

    gl::Error map(size_t offset,
                  size_t length,
                  GLbitfield access,
                  uint8_t **mapPointerOut) override;
    void unmap() override;

    gl::ErrorOrResult<ID3D11ShaderResourceView *> getSRVForFormat(DXGI_FORMAT srvFormat);

  private:
    static void FillBufferDesc(D3D11_BUFFER_DESC *bufferDesc,
                               Renderer11 *renderer,
                               BufferUsage usage,
                               unsigned int bufferSize);
    void clearSRVs();

    ID3D11Buffer *mNativeStorage;
    const angle::BroadcastChannel *mOnStorageChanged;
    std::map<DXGI_FORMAT, ID3D11ShaderResourceView *> mBufferResourceViews;
};

// A emulated indexed buffer storage represents an underlying D3D11 buffer for data
// that has been expanded to match the indices list used. This storage is only
// used for FL9_3 pointsprite rendering emulation.
class Buffer11::EmulatedIndexedStorage : public Buffer11::BufferStorage
{
  public:
    EmulatedIndexedStorage(Renderer11 *renderer);
    ~EmulatedIndexedStorage() override;

    bool isMappable(GLbitfield access) const override { return true; }

    gl::ErrorOrResult<ID3D11Buffer *> getNativeStorage(SourceIndexData *indexInfo,
                                                       const TranslatedAttribute &attribute,
                                                       GLint startVertex);

    gl::ErrorOrResult<CopyResult> copyFromStorage(BufferStorage *source,
                                                  size_t sourceOffset,
                                                  size_t size,
                                                  size_t destOffset) override;

    gl::Error resize(size_t size, bool preserveData) override;

    gl::Error map(size_t offset,
                  size_t length,
                  GLbitfield access,
                  uint8_t **mapPointerOut) override;
    void unmap() override;

  private:
    ID3D11Buffer *mNativeStorage;       // contains expanded data for use by D3D
    MemoryBuffer mMemoryBuffer;         // original data (not expanded)
    MemoryBuffer mIndicesMemoryBuffer;  // indices data
};

// Pack storage represents internal storage for pack buffers. We implement pack buffers
// as CPU memory, tied to a staging texture, for asynchronous texture readback.
class Buffer11::PackStorage : public Buffer11::BufferStorage
{
  public:
    explicit PackStorage(Renderer11 *renderer);
    ~PackStorage() override;

    bool isMappable(GLbitfield access) const override { return true; }
    gl::ErrorOrResult<CopyResult> copyFromStorage(BufferStorage *source,
                                                  size_t sourceOffset,
                                                  size_t size,
                                                  size_t destOffset) override;
    gl::Error resize(size_t size, bool preserveData) override;

    gl::Error map(size_t offset,
                  size_t length,
                  GLbitfield access,
                  uint8_t **mapPointerOut) override;
    void unmap() override;

    gl::Error packPixels(const gl::FramebufferAttachment &readAttachment,
                         const PackPixelsParams &params);

  private:
    gl::Error flushQueuedPackCommand();

    TextureHelper11 mStagingTexture;
    MemoryBuffer mMemoryBuffer;
    std::unique_ptr<PackPixelsParams> mQueuedPackCommand;
    PackPixelsParams mPackParams;
    bool mDataModified;
};

// System memory storage stores a CPU memory buffer with our buffer data.
// For dynamic data, it's much faster to update the CPU memory buffer than
// it is to update a D3D staging buffer and read it back later.
class Buffer11::SystemMemoryStorage : public Buffer11::BufferStorage
{
  public:
    explicit SystemMemoryStorage(Renderer11 *renderer);
    ~SystemMemoryStorage() override {}

    bool isMappable(GLbitfield access) const override { return true; }
    gl::ErrorOrResult<CopyResult> copyFromStorage(BufferStorage *source,
                                                  size_t sourceOffset,
                                                  size_t size,
                                                  size_t destOffset) override;
    gl::Error resize(size_t size, bool preserveData) override;

    gl::Error map(size_t offset,
                  size_t length,
                  GLbitfield access,
                  uint8_t **mapPointerOut) override;
    void unmap() override;

    MemoryBuffer *getSystemCopy() { return &mSystemCopy; }

  protected:
    MemoryBuffer mSystemCopy;
};

Buffer11::Buffer11(Renderer11 *renderer)
    : BufferD3D(renderer),
      mRenderer(renderer),
      mSize(0),
      mMappedStorage(nullptr),
      mBufferStorages({}),
      mDeallocThresholds({}),
      mIdleness({}),
      mConstantBufferStorageAdditionalSize(0),
      mMaxConstantBufferLruCount(0)
{
}

Buffer11::~Buffer11()
{
    for (auto &storage : mBufferStorages)
    {
        SafeDelete(storage);
    }

    for (auto &p : mConstantBufferRangeStoragesCache)
    {
        SafeDelete(p.second.storage);
    }

    mRenderer->onBufferDelete(this);
}

gl::Error Buffer11::setData(GLenum target, const void *data, size_t size, GLenum usage)
{
    updateD3DBufferUsage(usage);
    ANGLE_TRY(setSubData(target, data, size, 0));
    return gl::NoError();
}

gl::Error Buffer11::getData(const uint8_t **outData)
{
    SystemMemoryStorage *systemMemoryStorage = nullptr;
    ANGLE_TRY_RESULT(getSystemMemoryStorage(), systemMemoryStorage);

    ASSERT(systemMemoryStorage->getSize() >= mSize);

    *outData = systemMemoryStorage->getSystemCopy()->data();
    return gl::NoError();
}

gl::ErrorOrResult<Buffer11::SystemMemoryStorage *> Buffer11::getSystemMemoryStorage()
{
    BufferStorage *storage = nullptr;
    ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_SYSTEM_MEMORY), storage);
    return GetAs<SystemMemoryStorage>(storage);
}

gl::Error Buffer11::setSubData(GLenum target, const void *data, size_t size, size_t offset)
{
    size_t requiredSize = size + offset;

    if (data && size > 0)
    {
        // Use system memory storage for dynamic buffers.
        // Try using a constant storage for constant buffers
        BufferStorage *writeBuffer = nullptr;
        if (target == GL_UNIFORM_BUFFER && offset == 0 && size >= mSize)
        {
            ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_UNIFORM), writeBuffer);
        }
        else if (supportsDirectBinding())
        {
            ANGLE_TRY_RESULT(getStagingStorage(), writeBuffer);
        }
        else
        {
            ANGLE_TRY_RESULT(getSystemMemoryStorage(), writeBuffer);
        }

        ASSERT(writeBuffer);

        // Explicitly resize the staging buffer, preserving data if the new data will not
        // completely fill the buffer
        if (writeBuffer->getSize() < requiredSize)
        {
            bool preserveData = (offset > 0);
            ANGLE_TRY(writeBuffer->resize(requiredSize, preserveData));
        }

        writeBuffer->setData(static_cast<const uint8_t *>(data), offset, size);
        writeBuffer->setDataRevision(writeBuffer->getDataRevision() + 1);
    }

    mSize = std::max(mSize, requiredSize);
    invalidateStaticData();

    return gl::NoError();
}

gl::Error Buffer11::copySubData(BufferImpl *source,
                                GLintptr sourceOffset,
                                GLintptr destOffset,
                                GLsizeiptr size)
{
    Buffer11 *sourceBuffer = GetAs<Buffer11>(source);
    ASSERT(sourceBuffer != nullptr);

    BufferStorage *copyDest = nullptr;
    ANGLE_TRY_RESULT(getLatestBufferStorage(), copyDest);

    if (!copyDest)
    {
        ANGLE_TRY_RESULT(getStagingStorage(), copyDest);
    }

    BufferStorage *copySource = nullptr;
    ANGLE_TRY_RESULT(sourceBuffer->getLatestBufferStorage(), copySource);

    if (!copySource || !copyDest)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate internal staging buffer.");
    }

    // If copying to/from a pixel pack buffer, we must have a staging or
    // pack buffer partner, because other native buffers can't be mapped
    if (copyDest->getUsage() == BUFFER_USAGE_PIXEL_PACK && !copySource->isMappable(GL_MAP_READ_BIT))
    {
        ANGLE_TRY_RESULT(sourceBuffer->getStagingStorage(), copySource);
    }
    else if (copySource->getUsage() == BUFFER_USAGE_PIXEL_PACK &&
             !copyDest->isMappable(GL_MAP_WRITE_BIT))
    {
        ANGLE_TRY_RESULT(getStagingStorage(), copyDest);
    }

    // D3D11 does not allow overlapped copies until 11.1, and only if the
    // device supports D3D11_FEATURE_DATA_D3D11_OPTIONS::CopyWithOverlap
    // Get around this via a different source buffer
    if (copySource == copyDest)
    {
        if (copySource->getUsage() == BUFFER_USAGE_STAGING)
        {
            ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK),
                             copySource);
        }
        else
        {
            ANGLE_TRY_RESULT(getStagingStorage(), copySource);
        }
    }

    CopyResult copyResult = CopyResult::NOT_RECREATED;
    ANGLE_TRY_RESULT(copyDest->copyFromStorage(copySource, sourceOffset, size, destOffset),
                     copyResult);
    copyDest->setDataRevision(copyDest->getDataRevision() + 1);

    mSize = std::max<size_t>(mSize, destOffset + size);
    invalidateStaticData();

    return gl::NoError();
}

gl::Error Buffer11::map(GLenum access, GLvoid **mapPtr)
{
    // GL_OES_mapbuffer uses an enum instead of a bitfield for it's access, convert to a bitfield
    // and call mapRange.
    ASSERT(access == GL_WRITE_ONLY_OES);
    return mapRange(0, mSize, GL_MAP_WRITE_BIT, mapPtr);
}

gl::Error Buffer11::mapRange(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr)
{
    ASSERT(!mMappedStorage);

    BufferStorage *latestStorage = nullptr;
    ANGLE_TRY_RESULT(getLatestBufferStorage(), latestStorage);

    if (latestStorage && (latestStorage->getUsage() == BUFFER_USAGE_PIXEL_PACK ||
                          latestStorage->getUsage() == BUFFER_USAGE_STAGING))
    {
        // Latest storage is mappable.
        mMappedStorage = latestStorage;
    }
    else
    {
        // Fall back to using the staging buffer if the latest storage does not exist or is not
        // CPU-accessible.
        ANGLE_TRY_RESULT(getStagingStorage(), mMappedStorage);
    }

    if (!mMappedStorage)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate mappable internal buffer.");
    }

    if ((access & GL_MAP_WRITE_BIT) > 0)
    {
        // Update the data revision immediately, since the data might be changed at any time
        mMappedStorage->setDataRevision(mMappedStorage->getDataRevision() + 1);
        invalidateStaticData();
    }

    uint8_t *mappedBuffer = nullptr;
    ANGLE_TRY(mMappedStorage->map(offset, length, access, &mappedBuffer));
    ASSERT(mappedBuffer);

    *mapPtr = static_cast<GLvoid *>(mappedBuffer);
    return gl::NoError();
}

gl::Error Buffer11::unmap(GLboolean *result)
{
    ASSERT(mMappedStorage);
    mMappedStorage->unmap();
    mMappedStorage = nullptr;

    // TODO: detect if we had corruption. if so, return false.
    *result = GL_TRUE;

    return gl::NoError();
}

gl::Error Buffer11::markTransformFeedbackUsage()
{
    BufferStorage *transformFeedbackStorage = nullptr;
    ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK),
                     transformFeedbackStorage);

    if (transformFeedbackStorage)
    {
        transformFeedbackStorage->setDataRevision(transformFeedbackStorage->getDataRevision() + 1);
    }

    invalidateStaticData();
    return gl::NoError();
}

void Buffer11::updateDeallocThreshold(BufferUsage usage)
{
    // The following strategy was tuned on the Oort online benchmark (http://oortonline.gl/)
    // as well as a custom microbenchmark (IndexConversionPerfTest.Run/index_range_d3d11)

    // First readback: 8 unmodified uses before we free buffer memory.
    // After that, double the threshold each time until we reach the max.
    if (mDeallocThresholds[usage] == 0)
    {
        mDeallocThresholds[usage] = 8;
    }
    else if (mDeallocThresholds[usage] < std::numeric_limits<unsigned int>::max() / 2u)
    {
        mDeallocThresholds[usage] *= 2u;
    }
    else
    {
        mDeallocThresholds[usage] = std::numeric_limits<unsigned int>::max();
    }
}

// Free the storage if we decide it isn't being used very often.
gl::Error Buffer11::checkForDeallocation(BufferUsage usage)
{
    mIdleness[usage]++;

    BufferStorage *&storage = mBufferStorages[usage];
    if (storage != nullptr && mIdleness[usage] > mDeallocThresholds[usage])
    {
        BufferStorage *latestStorage = nullptr;
        ANGLE_TRY_RESULT(getLatestBufferStorage(), latestStorage);
        if (latestStorage != storage)
        {
            SafeDelete(storage);
        }
    }

    return gl::NoError();
}

gl::Error Buffer11::markBufferUsage(BufferUsage usage)
{
    mIdleness[usage] = 0;

    if (usage != BUFFER_USAGE_SYSTEM_MEMORY)
    {
        ANGLE_TRY(checkForDeallocation(BUFFER_USAGE_SYSTEM_MEMORY));
    }

    if (usage != BUFFER_USAGE_STAGING)
    {
        ANGLE_TRY(checkForDeallocation(BUFFER_USAGE_STAGING));
    }

    return gl::NoError();
}

gl::ErrorOrResult<ID3D11Buffer *> Buffer11::getBuffer(BufferUsage usage)
{
    BufferStorage *storage = nullptr;
    ANGLE_TRY_RESULT(getBufferStorage(usage), storage);
    return GetAs<NativeStorage>(storage)->getNativeStorage();
}

gl::ErrorOrResult<ID3D11Buffer *> Buffer11::getEmulatedIndexedBuffer(
    SourceIndexData *indexInfo,
    const TranslatedAttribute &attribute,
    GLint startVertex)
{
    ASSERT(indexInfo);

    BufferStorage *untypedStorage = nullptr;
    ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_EMULATED_INDEXED_VERTEX), untypedStorage);

    EmulatedIndexedStorage *emulatedStorage = GetAs<EmulatedIndexedStorage>(untypedStorage);

    ID3D11Buffer *nativeStorage = nullptr;
    ANGLE_TRY_RESULT(emulatedStorage->getNativeStorage(indexInfo, attribute, startVertex),
                     nativeStorage);

    return nativeStorage;
}

gl::ErrorOrResult<ID3D11Buffer *> Buffer11::getConstantBufferRange(GLintptr offset, GLsizeiptr size)
{
    BufferStorage *bufferStorage = nullptr;

    if (offset == 0 || mRenderer->getRenderer11DeviceCaps().supportsConstantBufferOffsets)
    {
        ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_UNIFORM), bufferStorage);
    }
    else
    {
        ANGLE_TRY_RESULT(getConstantBufferRangeStorage(offset, size), bufferStorage);
    }

    return GetAs<NativeStorage>(bufferStorage)->getNativeStorage();
}

gl::ErrorOrResult<ID3D11ShaderResourceView *> Buffer11::getSRV(DXGI_FORMAT srvFormat)
{
    BufferStorage *storage = nullptr;
    ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_PIXEL_UNPACK), storage);
    NativeStorage *nativeStorage = GetAs<NativeStorage>(storage);
    return nativeStorage->getSRVForFormat(srvFormat);
}

gl::Error Buffer11::packPixels(const gl::FramebufferAttachment &readAttachment,
                               const PackPixelsParams &params)
{
    PackStorage *packStorage = nullptr;
    ANGLE_TRY_RESULT(getPackStorage(), packStorage);

    BufferStorage *latestStorage = nullptr;
    ANGLE_TRY_RESULT(getLatestBufferStorage(), latestStorage);

    ASSERT(packStorage);
    ANGLE_TRY(packStorage->packPixels(readAttachment, params));
    packStorage->setDataRevision(latestStorage ? latestStorage->getDataRevision() + 1 : 1);

    return gl::NoError();
}

size_t Buffer11::getTotalCPUBufferMemoryBytes() const
{
    size_t allocationSize = 0;

    BufferStorage *staging = mBufferStorages[BUFFER_USAGE_STAGING];
    allocationSize += staging ? staging->getSize() : 0;

    BufferStorage *sysMem = mBufferStorages[BUFFER_USAGE_SYSTEM_MEMORY];
    allocationSize += sysMem ? sysMem->getSize() : 0;

    return allocationSize;
}

gl::ErrorOrResult<Buffer11::BufferStorage *> Buffer11::getBufferStorage(BufferUsage usage)
{
    ASSERT(0 <= usage && usage < BUFFER_USAGE_COUNT);
    BufferStorage *&newStorage = mBufferStorages[usage];

    if (!newStorage)
    {
        newStorage = allocateStorage(usage);
    }

    // resize buffer
    if (newStorage->getSize() < mSize)
    {
        ANGLE_TRY(newStorage->resize(mSize, true));
    }

    ANGLE_TRY(updateBufferStorage(newStorage, 0, mSize));
    ANGLE_TRY(markBufferUsage(usage));

    return newStorage;
}

Buffer11::BufferStorage *Buffer11::allocateStorage(BufferUsage usage)
{
    updateDeallocThreshold(usage);
    switch (usage)
    {
        case BUFFER_USAGE_PIXEL_PACK:
            return new PackStorage(mRenderer);
        case BUFFER_USAGE_SYSTEM_MEMORY:
            return new SystemMemoryStorage(mRenderer);
        case BUFFER_USAGE_EMULATED_INDEXED_VERTEX:
            return new EmulatedIndexedStorage(mRenderer);
        case BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK:
            return new NativeStorage(mRenderer, usage, &mDirectBroadcastChannel);
        default:
            return new NativeStorage(mRenderer, usage, nullptr);
    }
}

gl::ErrorOrResult<Buffer11::BufferStorage *> Buffer11::getConstantBufferRangeStorage(
    GLintptr offset,
    GLsizeiptr size)
{
    BufferStorage *newStorage;

    {
        // Keep the cacheEntry in a limited scope because it may be invalidated later in the code if
        // we need to reclaim some space.
        ConstantBufferCacheEntry *cacheEntry = &mConstantBufferRangeStoragesCache[offset];

        if (!cacheEntry->storage)
        {
            cacheEntry->storage  = allocateStorage(BUFFER_USAGE_UNIFORM);
            cacheEntry->lruCount = ++mMaxConstantBufferLruCount;
        }

        cacheEntry->lruCount = ++mMaxConstantBufferLruCount;
        newStorage           = cacheEntry->storage;
    }

    if (newStorage->getSize() < static_cast<size_t>(size))
    {
        size_t maximumAllowedAdditionalSize = 2 * getSize();

        size_t sizeDelta = size - newStorage->getSize();

        while (mConstantBufferStorageAdditionalSize + sizeDelta > maximumAllowedAdditionalSize)
        {
            auto iter = std::min_element(std::begin(mConstantBufferRangeStoragesCache),
                                         std::end(mConstantBufferRangeStoragesCache),
                                         [](const ConstantBufferCache::value_type &a,
                                            const ConstantBufferCache::value_type &b)
                                         {
                                             return a.second.lruCount < b.second.lruCount;
                                         });

            ASSERT(iter->second.storage != newStorage);
            ASSERT(mConstantBufferStorageAdditionalSize >= iter->second.storage->getSize());

            mConstantBufferStorageAdditionalSize -= iter->second.storage->getSize();
            SafeDelete(iter->second.storage);
            mConstantBufferRangeStoragesCache.erase(iter);
        }

        ANGLE_TRY(newStorage->resize(size, false));
        mConstantBufferStorageAdditionalSize += sizeDelta;

        // We don't copy the old data when resizing the constant buffer because the data may be
        // out-of-date therefore we reset the data revision and let updateBufferStorage() handle the
        // copy.
        newStorage->setDataRevision(0);
    }

    ANGLE_TRY(updateBufferStorage(newStorage, offset, size));
    ANGLE_TRY(markBufferUsage(BUFFER_USAGE_UNIFORM));
    return newStorage;
}

gl::Error Buffer11::updateBufferStorage(BufferStorage *storage,
                                        size_t sourceOffset,
                                        size_t storageSize)
{
    BufferStorage *latestBuffer = nullptr;
    ANGLE_TRY_RESULT(getLatestBufferStorage(), latestBuffer);

    if (latestBuffer && latestBuffer->getDataRevision() > storage->getDataRevision())
    {
        // Copy through a staging buffer if we're copying from or to a non-staging, mappable
        // buffer storage. This is because we can't map a GPU buffer, and copy CPU
        // data directly. If we're already using a staging buffer we're fine.
        if (latestBuffer->getUsage() != BUFFER_USAGE_STAGING &&
            storage->getUsage() != BUFFER_USAGE_STAGING &&
            (!latestBuffer->isMappable(GL_MAP_READ_BIT) || !storage->isMappable(GL_MAP_WRITE_BIT)))
        {
            NativeStorage *stagingBuffer = nullptr;
            ANGLE_TRY_RESULT(getStagingStorage(), stagingBuffer);

            CopyResult copyResult = CopyResult::NOT_RECREATED;
            ANGLE_TRY_RESULT(
                stagingBuffer->copyFromStorage(latestBuffer, 0, latestBuffer->getSize(), 0),
                copyResult);
            stagingBuffer->setDataRevision(latestBuffer->getDataRevision());

            latestBuffer = stagingBuffer;
        }

        CopyResult copyResult = CopyResult::NOT_RECREATED;
        ANGLE_TRY_RESULT(storage->copyFromStorage(latestBuffer, sourceOffset, storageSize, 0),
                         copyResult);
        // If the D3D buffer has been recreated, we should update our serial.
        if (copyResult == CopyResult::RECREATED)
        {
            updateSerial();
        }
        storage->setDataRevision(latestBuffer->getDataRevision());
    }

    return gl::NoError();
}

gl::ErrorOrResult<Buffer11::BufferStorage *> Buffer11::getLatestBufferStorage() const
{
    // Even though we iterate over all the direct buffers, it is expected that only
    // 1 or 2 will be present.
    BufferStorage *latestStorage = nullptr;
    DataRevision latestRevision = 0;
    for (auto &storage : mBufferStorages)
    {
        if (storage && (!latestStorage || storage->getDataRevision() > latestRevision))
        {
            latestStorage  = storage;
            latestRevision = storage->getDataRevision();
        }
    }

    // resize buffer
    if (latestStorage && latestStorage->getSize() < mSize)
    {
        ANGLE_TRY(latestStorage->resize(mSize, true));
    }

    return latestStorage;
}

gl::ErrorOrResult<Buffer11::NativeStorage *> Buffer11::getStagingStorage()
{
    BufferStorage *stagingStorage = nullptr;
    ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_STAGING), stagingStorage);
    return GetAs<NativeStorage>(stagingStorage);
}

gl::ErrorOrResult<Buffer11::PackStorage *> Buffer11::getPackStorage()
{
    BufferStorage *packStorage = nullptr;
    ANGLE_TRY_RESULT(getBufferStorage(BUFFER_USAGE_PIXEL_PACK), packStorage);
    return GetAs<PackStorage>(packStorage);
}

bool Buffer11::supportsDirectBinding() const
{
    // Do not support direct buffers for dynamic data. The streaming buffer
    // offers better performance for data which changes every frame.
    return (mUsage == D3DBufferUsage::STATIC);
}

void Buffer11::initializeStaticData()
{
    BufferD3D::initializeStaticData();

    // Notify when static data changes.
    mStaticBroadcastChannel.signal();
}

void Buffer11::invalidateStaticData()
{
    BufferD3D::invalidateStaticData();

    // Notify when static data changes.
    mStaticBroadcastChannel.signal();
}

angle::BroadcastChannel *Buffer11::getStaticBroadcastChannel()
{
    return &mStaticBroadcastChannel;
}

angle::BroadcastChannel *Buffer11::getDirectBroadcastChannel()
{
    return &mDirectBroadcastChannel;
}

// Buffer11::BufferStorage implementation

Buffer11::BufferStorage::BufferStorage(Renderer11 *renderer, BufferUsage usage)
    : mRenderer(renderer), mRevision(0), mUsage(usage), mBufferSize(0)
{
}

gl::Error Buffer11::BufferStorage::setData(const uint8_t *data, size_t offset, size_t size)
{
    ASSERT(isMappable(GL_MAP_WRITE_BIT));

    uint8_t *writePointer = nullptr;
    ANGLE_TRY(map(offset, size, GL_MAP_WRITE_BIT, &writePointer));

    memcpy(writePointer, data, size);

    unmap();

    return gl::NoError();
}

// Buffer11::NativeStorage implementation

Buffer11::NativeStorage::NativeStorage(Renderer11 *renderer,
                                       BufferUsage usage,
                                       const angle::BroadcastChannel *onStorageChanged)
    : BufferStorage(renderer, usage), mNativeStorage(nullptr), mOnStorageChanged(onStorageChanged)
{
}

Buffer11::NativeStorage::~NativeStorage()
{
    SafeRelease(mNativeStorage);
    clearSRVs();
}

bool Buffer11::NativeStorage::isMappable(GLbitfield access) const
{
    if ((access & GL_MAP_READ_BIT) != 0)
    {
        // Read is more exclusive than write mappability.
        return (mUsage == BUFFER_USAGE_STAGING);
    }
    ASSERT((access & GL_MAP_WRITE_BIT) != 0);
    return (mUsage == BUFFER_USAGE_STAGING || mUsage == BUFFER_USAGE_UNIFORM);
}

// Returns true if it recreates the direct buffer
gl::ErrorOrResult<CopyResult> Buffer11::NativeStorage::copyFromStorage(BufferStorage *source,
                                                                       size_t sourceOffset,
                                                                       size_t size,
                                                                       size_t destOffset)
{
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    size_t requiredSize = destOffset + size;
    bool createBuffer   = !mNativeStorage || mBufferSize < requiredSize;

    // (Re)initialize D3D buffer if needed
    bool preserveData = (destOffset > 0);
    if (createBuffer)
    {
        resize(requiredSize, preserveData);
    }

    size_t clampedSize = size;
    if (mUsage == BUFFER_USAGE_UNIFORM)
    {
        clampedSize = std::min(clampedSize, mBufferSize - destOffset);
    }

    if (source->getUsage() == BUFFER_USAGE_PIXEL_PACK ||
        source->getUsage() == BUFFER_USAGE_SYSTEM_MEMORY)
    {
        ASSERT(source->isMappable(GL_MAP_READ_BIT) && isMappable(GL_MAP_WRITE_BIT));

        // Uniform buffers must be mapped with write/discard.
        ASSERT(!(preserveData && mUsage == BUFFER_USAGE_UNIFORM));

        uint8_t *sourcePointer = nullptr;
        ANGLE_TRY(source->map(sourceOffset, clampedSize, GL_MAP_READ_BIT, &sourcePointer));

        uint8_t *destPointer = nullptr;
        ANGLE_TRY(map(destOffset, clampedSize, GL_MAP_WRITE_BIT, &destPointer));

        memcpy(destPointer, sourcePointer, clampedSize);

        unmap();
        source->unmap();
    }
    else
    {
        D3D11_BOX srcBox;
        srcBox.left   = static_cast<unsigned int>(sourceOffset);
        srcBox.right  = static_cast<unsigned int>(sourceOffset + clampedSize);
        srcBox.top    = 0;
        srcBox.bottom = 1;
        srcBox.front  = 0;
        srcBox.back   = 1;

        ID3D11Buffer *sourceBuffer = GetAs<NativeStorage>(source)->getNativeStorage();

        context->CopySubresourceRegion(mNativeStorage, 0, static_cast<unsigned int>(destOffset), 0,
                                       0, sourceBuffer, 0, &srcBox);
    }

    return createBuffer ? CopyResult::RECREATED : CopyResult::NOT_RECREATED;
}

gl::Error Buffer11::NativeStorage::resize(size_t size, bool preserveData)
{
    ID3D11Device *device         = mRenderer->getDevice();
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    D3D11_BUFFER_DESC bufferDesc;
    FillBufferDesc(&bufferDesc, mRenderer, mUsage, static_cast<unsigned int>(size));

    ID3D11Buffer *newBuffer;
    HRESULT result = device->CreateBuffer(&bufferDesc, nullptr, &newBuffer);

    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to create internal buffer, result: 0x%X.",
                         result);
    }

    d3d11::SetDebugName(newBuffer, "Buffer11::NativeStorage");

    if (mNativeStorage && preserveData)
    {
        // We don't call resize if the buffer is big enough already.
        ASSERT(mBufferSize <= size);

        D3D11_BOX srcBox;
        srcBox.left   = 0;
        srcBox.right  = static_cast<unsigned int>(mBufferSize);
        srcBox.top    = 0;
        srcBox.bottom = 1;
        srcBox.front  = 0;
        srcBox.back   = 1;

        context->CopySubresourceRegion(newBuffer, 0, 0, 0, 0, mNativeStorage, 0, &srcBox);
    }

    // No longer need the old buffer
    SafeRelease(mNativeStorage);
    mNativeStorage = newBuffer;

    mBufferSize = bufferDesc.ByteWidth;

    // Free the SRVs.
    clearSRVs();

    // Notify that the storage has changed.
    if (mOnStorageChanged)
    {
        mOnStorageChanged->signal();
    }

    return gl::NoError();
}

// static
void Buffer11::NativeStorage::FillBufferDesc(D3D11_BUFFER_DESC *bufferDesc,
                                             Renderer11 *renderer,
                                             BufferUsage usage,
                                             unsigned int bufferSize)
{
    bufferDesc->ByteWidth           = bufferSize;
    bufferDesc->MiscFlags           = 0;
    bufferDesc->StructureByteStride = 0;

    switch (usage)
    {
        case BUFFER_USAGE_STAGING:
            bufferDesc->Usage          = D3D11_USAGE_STAGING;
            bufferDesc->BindFlags      = 0;
            bufferDesc->CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            break;

        case BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK:
            bufferDesc->Usage     = D3D11_USAGE_DEFAULT;
            bufferDesc->BindFlags = D3D11_BIND_VERTEX_BUFFER;

            if (renderer->isES3Capable())
            {
                bufferDesc->BindFlags |= D3D11_BIND_STREAM_OUTPUT;
            }

            bufferDesc->CPUAccessFlags = 0;
            break;

        case BUFFER_USAGE_INDEX:
            bufferDesc->Usage          = D3D11_USAGE_DEFAULT;
            bufferDesc->BindFlags      = D3D11_BIND_INDEX_BUFFER;
            bufferDesc->CPUAccessFlags = 0;
            break;

        case BUFFER_USAGE_PIXEL_UNPACK:
            bufferDesc->Usage          = D3D11_USAGE_DEFAULT;
            bufferDesc->BindFlags      = D3D11_BIND_SHADER_RESOURCE;
            bufferDesc->CPUAccessFlags = 0;
            break;

        case BUFFER_USAGE_UNIFORM:
            bufferDesc->Usage          = D3D11_USAGE_DYNAMIC;
            bufferDesc->BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            bufferDesc->CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            // Constant buffers must be of a limited size, and aligned to 16 byte boundaries
            // For our purposes we ignore any buffer data past the maximum constant buffer size
            bufferDesc->ByteWidth = roundUp(bufferDesc->ByteWidth, 16u);

            // Note: it seems that D3D11 allows larger buffers on some platforms, but not all.
            // (Windows 10 seems to allow larger constant buffers, but not Windows 7)
            bufferDesc->ByteWidth =
                std::min<UINT>(bufferDesc->ByteWidth,
                               static_cast<UINT>(renderer->getNativeCaps().maxUniformBlockSize));
            break;

        default:
            UNREACHABLE();
    }
}

gl::Error Buffer11::NativeStorage::map(size_t offset,
                                       size_t length,
                                       GLbitfield access,
                                       uint8_t **mapPointerOut)
{
    ASSERT(isMappable(access));

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();
    D3D11_MAP d3dMapType         = gl_d3d11::GetD3DMapTypeFromBits(mUsage, access);
    UINT d3dMapFlag              = ((access & GL_MAP_UNSYNCHRONIZED_BIT) != 0 ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0);

    HRESULT result = context->Map(mNativeStorage, 0, d3dMapType, d3dMapFlag, &mappedResource);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to map native storage in Buffer11::NativeStorage::map");
    }
    ASSERT(mappedResource.pData);
    *mapPointerOut = static_cast<uint8_t *>(mappedResource.pData) + offset;
    return gl::Error(GL_NO_ERROR);
}

void Buffer11::NativeStorage::unmap()
{
    ASSERT(isMappable(GL_MAP_WRITE_BIT) || isMappable(GL_MAP_READ_BIT));
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();
    context->Unmap(mNativeStorage, 0);
}

gl::ErrorOrResult<ID3D11ShaderResourceView *> Buffer11::NativeStorage::getSRVForFormat(
    DXGI_FORMAT srvFormat)
{
    auto bufferSRVIt = mBufferResourceViews.find(srvFormat);

    if (bufferSRVIt != mBufferResourceViews.end())
    {
        return bufferSRVIt->second;
    }

    ID3D11Device *device                = mRenderer->getDevice();
    ID3D11ShaderResourceView *bufferSRV = nullptr;

    const d3d11::DXGIFormatSize &dxgiFormatInfo = d3d11::GetDXGIFormatSizeInfo(srvFormat);

    D3D11_SHADER_RESOURCE_VIEW_DESC bufferSRVDesc;
    bufferSRVDesc.Buffer.ElementOffset = 0;
    bufferSRVDesc.Buffer.ElementWidth  = static_cast<UINT>(mBufferSize) / dxgiFormatInfo.pixelBytes;
    bufferSRVDesc.ViewDimension        = D3D11_SRV_DIMENSION_BUFFER;
    bufferSRVDesc.Format               = srvFormat;

    HRESULT result = device->CreateShaderResourceView(mNativeStorage, &bufferSRVDesc, &bufferSRV);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Error creating buffer SRV in Buffer11::NativeStorage::getSRVForFormat");
    }

    mBufferResourceViews[srvFormat] = bufferSRV;

    return bufferSRV;
}

void Buffer11::NativeStorage::clearSRVs()
{
    for (auto &srv : mBufferResourceViews)
    {
        SafeRelease(srv.second);
    }
    mBufferResourceViews.clear();
}

// Buffer11::EmulatedIndexStorage implementation

Buffer11::EmulatedIndexedStorage::EmulatedIndexedStorage(Renderer11 *renderer)
    : BufferStorage(renderer, BUFFER_USAGE_EMULATED_INDEXED_VERTEX), mNativeStorage(nullptr)
{
}

Buffer11::EmulatedIndexedStorage::~EmulatedIndexedStorage()
{
    SafeRelease(mNativeStorage);
}

gl::ErrorOrResult<ID3D11Buffer *> Buffer11::EmulatedIndexedStorage::getNativeStorage(
    SourceIndexData *indexInfo,
    const TranslatedAttribute &attribute,
    GLint startVertex)
{
    // If a change in the indices applied from the last draw call is detected, then the emulated
    // indexed buffer needs to be invalidated.  After invalidation, the change detected flag should
    // be cleared to avoid unnecessary recreation of the buffer.
    if (mNativeStorage == nullptr || indexInfo->srcIndicesChanged)
    {
        SafeRelease(mNativeStorage);

        // Copy the source index data. This ensures that the lifetime of the indices pointer
        // stays with this storage until the next time we invalidate.
        size_t indicesDataSize = 0;
        switch (indexInfo->srcIndexType)
        {
            case GL_UNSIGNED_INT:
                indicesDataSize = sizeof(GLuint) * indexInfo->srcCount;
                break;
            case GL_UNSIGNED_SHORT:
                indicesDataSize = sizeof(GLushort) * indexInfo->srcCount;
                break;
            case GL_UNSIGNED_BYTE:
                indicesDataSize = sizeof(GLubyte) * indexInfo->srcCount;
                break;
            default:
                indicesDataSize = sizeof(GLushort) * indexInfo->srcCount;
                break;
        }

        if (!mIndicesMemoryBuffer.resize(indicesDataSize))
        {
            return gl::Error(GL_OUT_OF_MEMORY,
                             "Error resizing index memory buffer in "
                             "Buffer11::EmulatedIndexedStorage::getNativeStorage");
        }

        memcpy(mIndicesMemoryBuffer.data(), indexInfo->srcIndices, indicesDataSize);

        indexInfo->srcIndicesChanged = false;
    }

    if (!mNativeStorage)
    {
        unsigned int offset = 0;
        ANGLE_TRY_RESULT(attribute.computeOffset(startVertex), offset);

        // Expand the memory storage upon request and cache the results.
        unsigned int expandedDataSize =
            static_cast<unsigned int>((indexInfo->srcCount * attribute.stride) + offset);
        MemoryBuffer expandedData;
        if (!expandedData.resize(expandedDataSize))
        {
            return gl::Error(
                GL_OUT_OF_MEMORY,
                "Error resizing buffer in Buffer11::EmulatedIndexedStorage::getNativeStorage");
        }

        // Clear the contents of the allocated buffer
        ZeroMemory(expandedData.data(), expandedDataSize);

        uint8_t *curr      = expandedData.data();
        const uint8_t *ptr = static_cast<const uint8_t *>(indexInfo->srcIndices);

        // Ensure that we start in the correct place for the emulated data copy operation to
        // maintain offset behaviors.
        curr += offset;

        ReadIndexValueFunction readIndexValue = ReadIndexValueFromIndices<GLushort>;

        switch (indexInfo->srcIndexType)
        {
            case GL_UNSIGNED_INT:
                readIndexValue = ReadIndexValueFromIndices<GLuint>;
                break;
            case GL_UNSIGNED_SHORT:
                readIndexValue = ReadIndexValueFromIndices<GLushort>;
                break;
            case GL_UNSIGNED_BYTE:
                readIndexValue = ReadIndexValueFromIndices<GLubyte>;
                break;
        }

        // Iterate over the cached index data and copy entries indicated into the emulated buffer.
        for (GLuint i = 0; i < indexInfo->srcCount; i++)
        {
            GLuint idx = readIndexValue(ptr, i);
            memcpy(curr, mMemoryBuffer.data() + (attribute.stride * idx), attribute.stride);
            curr += attribute.stride;
        }

        // Finally, initialize the emulated indexed native storage object with the newly copied data
        // and free the temporary buffers used.
        ID3D11Device *device = mRenderer->getDevice();

        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.ByteWidth           = expandedDataSize;
        bufferDesc.MiscFlags           = 0;
        bufferDesc.StructureByteStride = 0;
        bufferDesc.Usage               = D3D11_USAGE_DEFAULT;
        bufferDesc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags      = 0;

        D3D11_SUBRESOURCE_DATA subResourceData = {expandedData.data(), 0, 0};

        HRESULT result = device->CreateBuffer(&bufferDesc, &subResourceData, &mNativeStorage);
        if (FAILED(result))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Could not create emulated index data buffer: %08lX",
                             result);
        }
        d3d11::SetDebugName(mNativeStorage, "Buffer11::EmulatedIndexedStorage");
    }

    return mNativeStorage;
}

gl::ErrorOrResult<CopyResult> Buffer11::EmulatedIndexedStorage::copyFromStorage(
    BufferStorage *source,
    size_t sourceOffset,
    size_t size,
    size_t destOffset)
{
    ASSERT(source->isMappable(GL_MAP_READ_BIT));
    uint8_t *sourceData = nullptr;
    ANGLE_TRY(source->map(sourceOffset, size, GL_MAP_READ_BIT, &sourceData));
    ASSERT(destOffset + size <= mMemoryBuffer.size());
    memcpy(mMemoryBuffer.data() + destOffset, sourceData, size);
    source->unmap();
    return CopyResult::RECREATED;
}

gl::Error Buffer11::EmulatedIndexedStorage::resize(size_t size, bool preserveData)
{
    if (mMemoryBuffer.size() < size)
    {
        if (!mMemoryBuffer.resize(size))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to resize EmulatedIndexedStorage");
        }
        mBufferSize = size;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::EmulatedIndexedStorage::map(size_t offset,
                                                size_t length,
                                                GLbitfield access,
                                                uint8_t **mapPointerOut)
{
    ASSERT(!mMemoryBuffer.empty() && offset + length <= mMemoryBuffer.size());
    *mapPointerOut = mMemoryBuffer.data() + offset;
    return gl::Error(GL_NO_ERROR);
}

void Buffer11::EmulatedIndexedStorage::unmap()
{
    // No-op
}

// Buffer11::PackStorage implementation

Buffer11::PackStorage::PackStorage(Renderer11 *renderer)
    : BufferStorage(renderer, BUFFER_USAGE_PIXEL_PACK), mStagingTexture(), mDataModified(false)
{
}

Buffer11::PackStorage::~PackStorage()
{
}

gl::ErrorOrResult<CopyResult> Buffer11::PackStorage::copyFromStorage(BufferStorage *source,
                                                                     size_t sourceOffset,
                                                                     size_t size,
                                                                     size_t destOffset)
{
    ANGLE_TRY(flushQueuedPackCommand());

    // We copy through a staging buffer when drawing with a pack buffer, or for other cases where we
    // access the pack buffer
    ASSERT(source->isMappable(GL_MAP_READ_BIT) && source->getUsage() == BUFFER_USAGE_STAGING);
    uint8_t *sourceData = nullptr;
    ANGLE_TRY(source->map(sourceOffset, size, GL_MAP_READ_BIT, &sourceData));
    ASSERT(destOffset + size <= mMemoryBuffer.size());
    memcpy(mMemoryBuffer.data() + destOffset, sourceData, size);
    source->unmap();
    return CopyResult::NOT_RECREATED;
}

gl::Error Buffer11::PackStorage::resize(size_t size, bool preserveData)
{
    if (size != mBufferSize)
    {
        if (!mMemoryBuffer.resize(size))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to resize internal buffer storage.");
        }
        mBufferSize = size;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::PackStorage::map(size_t offset,
                                     size_t length,
                                     GLbitfield access,
                                     uint8_t **mapPointerOut)
{
    ASSERT(offset + length <= getSize());
    // TODO: fast path
    //  We might be able to optimize out one or more memcpy calls by detecting when
    //  and if D3D packs the staging texture memory identically to how we would fill
    //  the pack buffer according to the current pack state.

    ANGLE_TRY(flushQueuedPackCommand());

    mDataModified = (mDataModified || (access & GL_MAP_WRITE_BIT) != 0);

    *mapPointerOut = mMemoryBuffer.data() + offset;
    return gl::Error(GL_NO_ERROR);
}

void Buffer11::PackStorage::unmap()
{
    // No-op
}

gl::Error Buffer11::PackStorage::packPixels(const gl::FramebufferAttachment &readAttachment,
                                            const PackPixelsParams &params)
{
    ANGLE_TRY(flushQueuedPackCommand());

    RenderTarget11 *renderTarget = nullptr;
    ANGLE_TRY(readAttachment.getRenderTarget(&renderTarget));

    ID3D11Resource *renderTargetResource = renderTarget->getTexture();
    ASSERT(renderTargetResource);

    unsigned int srcSubresource = renderTarget->getSubresourceIndex();
    TextureHelper11 srcTexture =
        TextureHelper11::MakeAndReference(renderTargetResource, renderTarget->getFormatSet());

    mQueuedPackCommand.reset(new PackPixelsParams(params));

    gl::Extents srcTextureSize(params.area.width, params.area.height, 1);
    if (!mStagingTexture.getResource() || mStagingTexture.getFormat() != srcTexture.getFormat() ||
        mStagingTexture.getExtents() != srcTextureSize)
    {
        ANGLE_TRY_RESULT(
            CreateStagingTexture(srcTexture.getTextureType(), srcTexture.getFormatSet(),
                                 srcTextureSize, StagingAccess::READ, mRenderer->getDevice()),
            mStagingTexture);
    }

    // ReadPixels from multisampled FBOs isn't supported in current GL
    ASSERT(srcTexture.getSampleCount() <= 1);

    ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();
    D3D11_BOX srcBox;
    srcBox.left   = params.area.x;
    srcBox.right  = params.area.x + params.area.width;
    srcBox.top    = params.area.y;
    srcBox.bottom = params.area.y + params.area.height;

    // Select the correct layer from a 3D attachment
    srcBox.front = 0;
    if (mStagingTexture.getTextureType() == GL_TEXTURE_3D)
    {
        srcBox.front = static_cast<UINT>(readAttachment.layer());
    }
    srcBox.back = srcBox.front + 1;

    // Asynchronous copy
    immediateContext->CopySubresourceRegion(mStagingTexture.getResource(), 0, 0, 0, 0,
                                            srcTexture.getResource(), srcSubresource, &srcBox);

    return gl::NoError();
}

gl::Error Buffer11::PackStorage::flushQueuedPackCommand()
{
    ASSERT(mMemoryBuffer.size() > 0);

    if (mQueuedPackCommand)
    {
        ANGLE_TRY(
            mRenderer->packPixels(mStagingTexture, *mQueuedPackCommand, mMemoryBuffer.data()));
        mQueuedPackCommand.reset(nullptr);
    }

    return gl::NoError();
}

// Buffer11::SystemMemoryStorage implementation

Buffer11::SystemMemoryStorage::SystemMemoryStorage(Renderer11 *renderer)
    : Buffer11::BufferStorage(renderer, BUFFER_USAGE_SYSTEM_MEMORY)
{
}

gl::ErrorOrResult<CopyResult> Buffer11::SystemMemoryStorage::copyFromStorage(BufferStorage *source,
                                                                             size_t sourceOffset,
                                                                             size_t size,
                                                                             size_t destOffset)
{
    ASSERT(source->isMappable(GL_MAP_READ_BIT));
    uint8_t *sourceData = nullptr;
    ANGLE_TRY(source->map(sourceOffset, size, GL_MAP_READ_BIT, &sourceData));
    ASSERT(destOffset + size <= mSystemCopy.size());
    memcpy(mSystemCopy.data() + destOffset, sourceData, size);
    source->unmap();
    return CopyResult::RECREATED;
}

gl::Error Buffer11::SystemMemoryStorage::resize(size_t size, bool preserveData)
{
    if (mSystemCopy.size() < size)
    {
        if (!mSystemCopy.resize(size))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to resize SystemMemoryStorage");
        }
        mBufferSize = size;
    }

    return gl::NoError();
}

gl::Error Buffer11::SystemMemoryStorage::map(size_t offset,
                                             size_t length,
                                             GLbitfield access,
                                             uint8_t **mapPointerOut)
{
    ASSERT(!mSystemCopy.empty() && offset + length <= mSystemCopy.size());
    *mapPointerOut = mSystemCopy.data() + offset;
    return gl::Error(GL_NO_ERROR);
}

void Buffer11::SystemMemoryStorage::unmap()
{
    // No-op
}
}  // namespace rx
