//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BlobCache: Stores keyed blobs in memory to support EGL_ANDROID_blob_cache.
// Can be used in conjunction with the platform layer to warm up the cache from
// disk.  MemoryProgramCache uses this to handle caching of compiled programs.

#include "libANGLE/BlobCache.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/histogram_macros.h"
#include "platform/PlatformMethods.h"

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

namespace egl
{

// In oder to store more cache in blob cache, compress cacheData to compressedData
// before being stored.
bool CompressBlobCacheData(const size_t cacheSize,
                           const uint8_t *cacheData,
                           angle::MemoryBuffer *compressedData)
{
    uLong uncompressedSize       = static_cast<uLong>(cacheSize);
    uLong expectedCompressedSize = zlib_internal::GzipExpectedCompressedSize(uncompressedSize);

    // Allocate memory.
    if (!compressedData->resize(expectedCompressedSize))
    {
        ERR() << "Failed to allocate memory for compression";
        return false;
    }

    int zResult = zlib_internal::GzipCompressHelper(compressedData->data(), &expectedCompressedSize,
                                                    cacheData, uncompressedSize, nullptr, nullptr);

    if (zResult != Z_OK)
    {
        ERR() << "Failed to compress cache data: " << zResult;
        return false;
    }

    // Resize it to expected size.
    if (!compressedData->resize(expectedCompressedSize))
    {
        return false;
    }

    return true;
}

bool DecompressBlobCacheData(const uint8_t *compressedData,
                             const size_t compressedSize,
                             angle::MemoryBuffer *uncompressedData)
{
    // Call zlib function to decompress.
    uint32_t uncompressedSize =
        zlib_internal::GetGzipUncompressedSize(compressedData, compressedSize);

    // Allocate enough memory.
    if (!uncompressedData->resize(uncompressedSize))
    {
        ERR() << "Failed to allocate memory for decompression";
        return false;
    }

    uLong destLen = uncompressedSize;
    int zResult   = zlib_internal::GzipUncompressHelper(
          uncompressedData->data(), &destLen, compressedData, static_cast<uLong>(compressedSize));

    if (zResult != Z_OK)
    {
        ERR() << "Failed to decompress data: " << zResult << "\n";
        return false;
    }

    // Resize it to expected size.
    if (!uncompressedData->resize(destLen))
    {
        return false;
    }

    return true;
}

BlobCache::BlobCache(size_t maxCacheSizeBytes)
    : mBlobCache(maxCacheSizeBytes), mSetBlobFunc(nullptr), mGetBlobFunc(nullptr)
{}

BlobCache::~BlobCache() {}

void BlobCache::put(const BlobCache::Key &key, angle::MemoryBuffer &&value)
{
    if (areBlobCacheFuncsSet())
    {
        std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
        // Store the result in the application's cache
        mSetBlobFunc(key.data(), key.size(), value.data(), value.size());
    }
    else
    {
        populate(key, std::move(value), CacheSource::Memory);
    }
}

bool BlobCache::compressAndPut(const BlobCache::Key &key,
                               angle::MemoryBuffer &&uncompressedValue,
                               size_t *compressedSize)
{
    angle::MemoryBuffer compressedValue;
    if (!CompressBlobCacheData(uncompressedValue.size(), uncompressedValue.data(),
                               &compressedValue))
    {
        return false;
    }
    if (compressedSize != nullptr)
        *compressedSize = compressedValue.size();
    put(key, std::move(compressedValue));
    return true;
}

void BlobCache::putApplication(const BlobCache::Key &key, const angle::MemoryBuffer &value)
{
    if (areBlobCacheFuncsSet())
    {
        std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
        mSetBlobFunc(key.data(), key.size(), value.data(), value.size());
    }
}

void BlobCache::populate(const BlobCache::Key &key, angle::MemoryBuffer &&value, CacheSource source)
{
    std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
    CacheEntry newEntry;
    newEntry.first  = std::move(value);
    newEntry.second = source;

    // Cache it inside blob cache only if caching inside the application is not possible.
    mBlobCache.put(key, std::move(newEntry), newEntry.first.size());
}

bool BlobCache::get(angle::ScratchBuffer *scratchBuffer,
                    const BlobCache::Key &key,
                    BlobCache::Value *valueOut,
                    size_t *bufferSizeOut)
{
    // Look into the application's cache, if there is such a cache
    if (areBlobCacheFuncsSet())
    {
        std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
        EGLsizeiANDROID valueSize = mGetBlobFunc(key.data(), key.size(), nullptr, 0);
        if (valueSize <= 0)
        {
            return false;
        }

        angle::MemoryBuffer *scratchMemory;
        bool result = scratchBuffer->get(valueSize, &scratchMemory);
        if (!result)
        {
            ERR() << "Failed to allocate memory for binary blob";
            return false;
        }

        EGLsizeiANDROID originalValueSize = valueSize;
        valueSize = mGetBlobFunc(key.data(), key.size(), scratchMemory->data(), valueSize);

        // Make sure the key/value pair still exists/is unchanged after the second call
        // (modifications to the application cache by another thread are a possibility)
        if (valueSize != originalValueSize)
        {
            // This warning serves to find issues with the application cache, none of which are
            // currently known to be thread-safe.  If such a use ever arises, this WARN can be
            // removed.
            WARN() << "Binary blob no longer available in cache (removed by a thread?)";
            return false;
        }

        *valueOut      = BlobCache::Value(scratchMemory->data(), scratchMemory->size());
        *bufferSizeOut = valueSize;
        return true;
    }

    std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
    // Otherwise we are doing caching internally, so try to find it there
    const CacheEntry *entry;
    bool result = mBlobCache.get(key, &entry);

    if (result)
    {

        *valueOut      = BlobCache::Value(entry->first.data(), entry->first.size());
        *bufferSizeOut = entry->first.size();
    }

    return result;
}

bool BlobCache::getAt(size_t index, const BlobCache::Key **keyOut, BlobCache::Value *valueOut)
{
    std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
    const CacheEntry *valueBuf;
    bool result = mBlobCache.getAt(index, keyOut, &valueBuf);
    if (result)
    {
        *valueOut = BlobCache::Value(valueBuf->first.data(), valueBuf->first.size());
    }
    return result;
}

BlobCache::GetAndDecompressResult BlobCache::getAndDecompress(
    angle::ScratchBuffer *scratchBuffer,
    const BlobCache::Key &key,
    angle::MemoryBuffer *uncompressedValueOut)
{
    ASSERT(uncompressedValueOut);

    Value compressedValue;
    size_t compressedSize;
    if (!get(scratchBuffer, key, &compressedValue, &compressedSize))
    {
        return GetAndDecompressResult::NotFound;
    }

    {
        // This needs to be locked because `DecompressBlobCacheData` is reading shared memory from
        // `compressedValue.data()`.
        std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
        if (!DecompressBlobCacheData(compressedValue.data(), compressedSize, uncompressedValueOut))
        {
            return GetAndDecompressResult::DecompressFailure;
        }
    }

    return GetAndDecompressResult::GetSuccess;
}

void BlobCache::remove(const BlobCache::Key &key)
{
    std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
    mBlobCache.eraseByKey(key);
}

void BlobCache::setBlobCacheFuncs(EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get)
{
    std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
    mSetBlobFunc = set;
    mGetBlobFunc = get;
}

bool BlobCache::areBlobCacheFuncsSet() const
{
    std::scoped_lock<std::mutex> lock(mBlobCacheMutex);
    // Either none or both of the callbacks should be set.
    ASSERT((mSetBlobFunc != nullptr) == (mGetBlobFunc != nullptr));

    return mSetBlobFunc != nullptr && mGetBlobFunc != nullptr;
}

}  // namespace egl
