//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_binary_data.cpp:
//   Common code for the ANGLE trace replay large trace binary data definition.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

#include "common/mathutil.h"
#include "frame_capture_binary_data.h"

#include <array>
#include <string>

namespace angle
{

// Return current size of all binary data
size_t FrameCaptureBinaryData::totalSize() const
{
    return ((mBlockCount - 1) * mDataBlockSize) + mCurrentBlockOffset;
}

// Determine if any blocks have been saved to disk, i.e., if we have run out of resident
// blocks
bool FrameCaptureBinaryData::isSwapMode() const
{
    return (mStoredBlocks > 0);
}

void FrameCaptureBinaryData::storeResidentBlocks()
{
    // Write out all resident binary data blocks by calling storeBlock on each, deleting
    // front() from vector
    if (!isSwapMode())
    {
        while (mData.size() > 1)
        {
            storeBlock();
            mData.erase(mData.begin());
        }
    }

    storeBlock();
}

void FrameCaptureBinaryData::updateGetDataCache(size_t blockId)
{
    const ReplayBlockDescription &desc = mReplayBlockDescriptions[blockId];

    mCacheBlockId          = blockId;
    mCacheBlockBeginOffset = desc.beginDataOffset;
    mCacheBlockEndOffset   = desc.endDataOffset;
    mCacheBlockBaseAddress = desc.residentAddress;

    // The location for the swap block differs for load and store.  For store it will ultimately be
    // zero as it's unnecessary to utilize the full BinaryDataSize. For load, it will end up
    // as the last of the resident blocks.
    if (blockId >= mMaxResidentBlockIndex)
    {
        mCurrentTransientLoadedBlockId = blockId;
    }
}

// Resident blocks will have a valid memory address at residentAddress
bool FrameCaptureBinaryData::isBlockResident(size_t blockId) const
{
    return (mReplayBlockDescriptions[blockId].residentAddress != nullptr);
}

void FrameCaptureBinaryData::setBlockResident(size_t blockId, uint8_t *address)
{
    mReplayBlockDescriptions[blockId].residentAddress = address;
}

void FrameCaptureBinaryData::setBlockNonResident(size_t blockId)
{
    mReplayBlockDescriptions[blockId].residentAddress = nullptr;
}

void FrameCaptureBinaryData::setBlockSize(size_t blockSize)
{
    if (!gl::isPow2(blockSize))
    {
        FATAL() << "Binary Data File Blocksize specified is not a power of 2: " << blockSize;
    }
    mDataBlockSize = blockSize;
}

void FrameCaptureBinaryData::setBinaryDataSize(size_t binaryDataSize)
{
    if (!gl::isPow2(binaryDataSize))
    {
        FATAL() << "Binary Data File Binary Data Size specified is not a power of 2: "
                << binaryDataSize;
    }
    mMaxResidentBinarySize = binaryDataSize;
}

std::vector<uint8_t> &FrameCaptureBinaryData::prepareStoreBlock(size_t blockId)
{
    // Ensure mData has enough vectors up to and including the target index
    if (!isSwapMode())
    {
        mData.resize(mData.size() + 1);
    }

    mBlockCount = blockId + 1;

    mData.back().resize(mDataBlockSize);
    mCurrentBlockOffset = 0;

    return mData.back();
}

std::vector<uint8_t> &FrameCaptureBinaryData::prepareLoadBlock(size_t blockId)
{
    size_t destBlockIndex = std::min(blockId, mMaxResidentBlockIndex);

    // Ensure mData has enough vectors up to the target index
    if (destBlockIndex >= mData.size())
    {
        mData.resize(destBlockIndex + 1);
    }

    if (isSwapBlock(destBlockIndex))
    {
        // If not the same block, mark previous block occupying swap slot as non-resident
        if (blockId != mCurrentTransientLoadedBlockId)
        {
            // Since this is the swap block, we aren't actually freeing any memory. But we need
            // a way to indicate whether a transient block is loaded. This way each logical
            // block knows whether it is resident, and where.
            setBlockNonResident(mCurrentTransientLoadedBlockId);
        }
        // Track which logical block is now in the swap slot
        mCurrentTransientLoadedBlockId = blockId;
    }

    mData.back().resize(mDataBlockSize);
    mCurrentBlockOffset = 0;

    return mData.back();
}

// Write file index entries to the end of compressed binary data files
BinaryFileIndexInfo FrameCaptureBinaryData::appendFileIndex()
{
    BinaryFileIndexInfo indexInfo;
    indexInfo.version      = kLongTraceVersionId;
    indexInfo.blockSize    = mDataBlockSize;
    indexInfo.blockCount   = mBlockCount;
    indexInfo.residentSize = mMaxResidentBinarySize;
    indexInfo.indexOffset  = 0;

    if (mIsBinaryDataCompressed)
    {
        size_t indexDataOffset = mFileStream->getPosition();
        // Copy index entries (index data trailer) to end of compressed data file
        for (auto &entry : mFileIndex)
        {
            mFileStream->write(reinterpret_cast<const uint8_t *>(&entry), sizeof(FileBlockInfo));
        }
        indexInfo.indexOffset = indexDataOffset;
    }

    // Return index information for saving in JSON file
    return indexInfo;
}

// Read in file index data from a compressed file and construct an access index
void FrameCaptureBinaryData::constructBlockDescIndex(size_t indexOffset)
{
    if (mIsBinaryDataCompressed)
    {
        // Move to the beginning of the index data in the compressed file
        mFileStream->seek(indexOffset, kSeekBegin);

        // Populate the replay block description array
        for (size_t i = 0; i < mBlockCount; i++)
        {
            // Read in block's information data
            FileBlockInfo blockInfo;
            mFileStream->read(reinterpret_cast<uint8_t *>(&blockInfo), sizeof(FileBlockInfo));

            // Create and save a block description from the block information
            ReplayBlockDescription blockDesc = {};
            blockDesc.fileOffset             = blockInfo.fileOffset;
            blockDesc.beginDataOffset        = blockInfo.dataOffset;
            blockDesc.endDataOffset          = blockInfo.dataOffset + blockInfo.dataSize - 1;
            blockDesc.dataSize               = blockInfo.dataSize;
            mReplayBlockDescriptions.push_back(blockDesc);
        }
    }
    else
    {
        // Create block descriptions from fixed size calculations
        mFileStream->seek(0, kSeekEnd);
        size_t size = mFileStream->getPosition();
        mFileStream->seek(0, kSeekBegin);

        size_t remaining = size;
        while (remaining > 0)
        {
            // The final block is typically smaller than mDataBlockSize
            size_t dataSize = std::min(remaining, mDataBlockSize);
            size_t offset   = size - remaining;

            // Create and save a block description
            ReplayBlockDescription blockDesc = {};
            blockDesc.fileOffset             = offset;
            blockDesc.beginDataOffset        = offset;
            blockDesc.endDataOffset          = offset + dataSize - 1;
            blockDesc.dataSize               = dataSize;
            mReplayBlockDescriptions.push_back(blockDesc);
            remaining -= dataSize;
        }
    }
}

size_t FrameCaptureBinaryData::append(const void *data, size_t size)
{
    if (mData.empty())
    {
        prepareStoreBlock(0);
        mBlockCount = 1;
    }

    ASSERT(totalSize() % kBinaryAlignment == 0);
    size_t startingOffset       = totalSize();
    const size_t sizeToIncrease = rx::roundUpPow2(size, kBinaryAlignment);

    // If the requested data size will not fit into the current block, allocate
    // a new block
    if (mCurrentBlockOffset + sizeToIncrease > mDataBlockSize)
    {
        size_t newBlockId = (startingOffset + sizeToIncrease) / mDataBlockSize;

        if (!isSwapMode())
        {
            if (newBlockId > mMaxResidentBlockIndex)
            {
                // All resident blocks are full, store them to disk
                storeResidentBlocks();
            }
            else
            {
                // Resident blocks available, no need to store to disk
            }
        }
        else
        {
            // Resident blocks have been saved, write this block to disk
            storeBlock();
        }
        prepareStoreBlock(newBlockId);
        startingOffset = totalSize();
    }

    memcpy(mData.back().data() + mCurrentBlockOffset, data, size);
    mCurrentBlockOffset += sizeToIncrease;
    return startingOffset;
}

const uint8_t *FrameCaptureBinaryData::getData(size_t offset)
{
    // This is the fastpath for this function, misses should be negligible
    if (offset >= mCacheBlockBeginOffset && offset < mCacheBlockEndOffset)
    {
        return (mCacheBlockBaseAddress + (offset - mCacheBlockBeginOffset));
    }

    // Calculate new block id for binary data to be loaded
    size_t newBlockId = offset / mDataBlockSize;
    // Swap block into memory if it is nonresident
    if (!isBlockResident(newBlockId))
    {
        loadBlock(newBlockId);
    }
    // Update the fastpath cache variables
    updateGetDataCache(newBlockId);

    return (mCacheBlockBaseAddress + (offset - mCacheBlockBeginOffset));
}

void FrameCaptureBinaryData::clear()
{
    mCurrentBlockOffset = 0;
    mFileIndex.clear();
    mReplayBlockDescriptions.clear();
    mData.clear();
}

// Helper class for compression/decompression operations
class ZLibHelper
{
    // See the following file for details on these variables and helpers:
    // https://chromium.googlesource.com/chromium/src/+/master/third_party/zlib/google/compression_utils_portable.cc
    static constexpr int kZlibMemoryLevel           = 8;
    static constexpr int kWindowBitsToGetGzipHeader = 16;

  public:
    ZLibHelper(FrameCaptureBinaryData::Mode mode) : mMode(mode), mStream(), mInitialized(false)
    {
        int ret          = 0;
        mStream.zalloc   = Z_NULL;
        mStream.zfree    = Z_NULL;
        mStream.opaque   = Z_NULL;
        mStream.avail_in = 0;
        mStream.next_in  = Z_NULL;

        if (mMode == FrameCaptureBinaryData::Mode::Load)
        {
            ret = inflateInit2(&mStream, MAX_WBITS + kWindowBitsToGetGzipHeader);
        }
        else if (mMode == FrameCaptureBinaryData::Mode::Store)
        {
            ret = deflateInit2(&mStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                               MAX_WBITS + kWindowBitsToGetGzipHeader, kZlibMemoryLevel,
                               Z_DEFAULT_STRATEGY);
        }
        else
        {
            FATAL() << "Invalid Mode Enum in ZLibHelper";
        }

        if (ret != Z_OK)
        {
            FATAL() << "Zlib helper initialization failed: " << ret;
        }
        mInitialized = true;
    }

    ~ZLibHelper()
    {
        if (mInitialized)
        {
            if (mMode == FrameCaptureBinaryData::Mode::Load)
            {
                inflateEnd(&mStream);
            }
            else if (mMode == FrameCaptureBinaryData::Mode::Store)
            {
                deflateEnd(&mStream);
            }
            else
            {
                FATAL() << "Invalid Mode Enum in ZLibHelper";
            }
        }
    }

    z_stream *getStream() { return &mStream; }

    ZLibHelper(const ZLibHelper &)            = delete;
    ZLibHelper &operator=(const ZLibHelper &) = delete;
    ZLibHelper(ZLibHelper &&)                 = delete;
    ZLibHelper &operator=(ZLibHelper &&)      = delete;

  private:
    FrameCaptureBinaryData::Mode mMode;
    z_stream mStream;
    bool mInitialized;
};

// Configure binary data output parameters and prepare file for writing
void FrameCaptureBinaryData::initializeBinaryDataStore(bool compression,
                                                       const std::string &outDir,
                                                       const std::string &fileName)
{
    std::string binaryDataFileName = outDir + fileName;
    mStoredBlocks                  = 0;
    mIsBinaryDataCompressed        = compression;

    if ((mMaxResidentBinarySize / mDataBlockSize) <= 1)
    {
        FATAL() << "Error,insufficient resident memory specified or available";
    }
    mMaxResidentBlockIndex = (mMaxResidentBinarySize / mDataBlockSize) - 1;

    mFileStream = new FileStream(binaryDataFileName, Mode::Store);
}

// Optionally compress and then write a single data block to disk
void FrameCaptureBinaryData::storeBlock()
{
    std::vector<uint8_t> &storeBlock = mData.front();

    // The last block to be saved will be resized to fit used data
    if (mCaptureComplete && mData.size() == 1)
    {
        storeBlock.resize(mCurrentBlockOffset);
    }

    if (mIsBinaryDataCompressed)
    {
        // Use zlib library, based on example/doc here: https://zlib.net/zlib_how.html
        ZLibHelper compressor(Mode::Store);
        z_stream *zStream = compressor.getStream();

        int deflateStatus = 0;
        using ZlibBuffer  = std::array<unsigned char, kZlibBufferSize>;
        std::unique_ptr<ZlibBuffer> compressBuffer(new ZlibBuffer());

        FileBlockInfo fileIndexEntry;
        fileIndexEntry.fileOffset = mFileStream->getPosition();      // CompressedFileOffset
        fileIndexEntry.dataOffset = mStoredBlocks * mDataBlockSize;  // UncompressedOffset
        fileIndexEntry.dataSize   = storeBlock.size();               // Size of block
        // Save file index data
        mFileIndex.push_back(fileIndexEntry);

        const unsigned char *uncompressedDataPtr = storeBlock.data();
        size_t remainingBytesToCompress          = storeBlock.size();

        while (remainingBytesToCompress > 0)
        {
            size_t bytesToCompress =
                std::min(remainingBytesToCompress, static_cast<size_t>(kZlibBufferSize));
            zStream->avail_in = static_cast<uInt>(bytesToCompress);
            zStream->next_in  = const_cast<unsigned char *>(uncompressedDataPtr);

            do
            {
                zStream->avail_out = kZlibBufferSize;
                zStream->next_out  = compressBuffer->data();

                int flushMode = Z_NO_FLUSH;
                if (remainingBytesToCompress <= kZlibBufferSize)
                {
                    flushMode = Z_FINISH;
                }
                deflateStatus = deflate(zStream, flushMode);
                if (deflateStatus == Z_STREAM_ERROR)
                {
                    FATAL() << "Error during deflate: Z_STREAM_ERROR";
                }
                // This is the compressed data size about to be written
                unsigned bytesCompressed = kZlibBufferSize - zStream->avail_out;
                mFileStream->write(compressBuffer->data(), bytesCompressed);
            } while (zStream->avail_out == 0);

            uncompressedDataPtr += bytesToCompress;
            remainingBytesToCompress -= bytesToCompress;
        }
    }
    else
    {
        mFileStream->write(storeBlock.data(), storeBlock.size());
    }

    mStoredBlocks++;
}

BinaryFileIndexInfo FrameCaptureBinaryData::closeBinaryDataStore()
{
    mCaptureComplete = true;
    storeResidentBlocks();

    BinaryFileIndexInfo indexInfo;
    indexInfo = appendFileIndex();
    clear();
    return indexInfo;
}

// Sets up binary data loader with config data from the trace fixture
void FrameCaptureBinaryData::configureBinaryDataLoader(bool compression,
                                                       size_t blockCount,
                                                       size_t blockSize,
                                                       size_t residentSize,
                                                       size_t indexOffset,
                                                       const std::string &fileName)
{
    mIsBinaryDataCompressed        = compression;
    mFileName                      = fileName;
    mMaxResidentBinarySize         = residentSize;
    mDataBlockSize                 = blockSize;
    mBlockCount                    = blockCount;
    mMaxResidentBlockIndex         = (mMaxResidentBinarySize / mDataBlockSize) - 1;
    mCurrentTransientLoadedBlockId = mMaxResidentBlockIndex;
    mIndexOffset                   = indexOffset;
}

// Setup binary data file access, init index and preload data blocks up to limit
void FrameCaptureBinaryData::initializeBinaryDataLoader()
{
    // Create file stream manager
    mFileStream = new FileStream(mFileName.c_str(), Mode::Load);

    // Assemble binary data file/cache index
    constructBlockDescIndex(mIndexOffset);

    // Preload binary data blocks up to limit
    size_t blocksToPreload =
        std::min(mReplayBlockDescriptions.size(), (mMaxResidentBlockIndex + 1));
    for (size_t i = 0; i < blocksToPreload; i++)
    {
        loadBlock(i);
    }

    // Initialize getData cache
    updateGetDataCache(0);
}

// Load a single data block into memory
void FrameCaptureBinaryData::loadBlock(size_t blockId)
{
    std::vector<uint8_t> &uncompressedDataBlock = prepareLoadBlock(blockId);

    // Move to start of this data block in the data file
    mFileStream->seek(mReplayBlockDescriptions[blockId].fileOffset, kSeekBegin);

    if (mIsBinaryDataCompressed)
    {
        // Use zlib library, based on example/doc here: https://zlib.net/zlib_how.html
        ZLibHelper decompressor(Mode::Load);
        z_stream *zStream        = decompressor.getStream();
        int inflateStatus        = 0;
        size_t bytesDecompressed = 0;

        using ZlibBuffer = std::array<unsigned char, kZlibBufferSize>;
        std::unique_ptr<ZlibBuffer> compressedDataBuffer(new ZlibBuffer());
        zStream->avail_out = static_cast<uInt>(mDataBlockSize);
        zStream->next_out  = uncompressedDataBlock.data();

        do
        {
            if (zStream->avail_in == 0)
            {
                zStream->avail_in = static_cast<uInt>(
                    mFileStream->read(compressedDataBuffer->data(), kZlibBufferSize));
                zStream->next_in = compressedDataBuffer->data();
            }

            do
            {
                int availableOutputSpace = static_cast<int>(mDataBlockSize - mCurrentBlockOffset);
                zStream->avail_out       = availableOutputSpace;
                zStream->next_out        = uncompressedDataBlock.data() + mCurrentBlockOffset;
                inflateStatus            = inflate(zStream, Z_NO_FLUSH);
                ASSERT(inflateStatus != Z_STREAM_ERROR);
                if (inflateStatus == Z_NEED_DICT || inflateStatus == Z_DATA_ERROR ||
                    inflateStatus == Z_MEM_ERROR)
                {
                    FATAL() << "Zlib inflate failed: " << inflateStatus;
                }
                bytesDecompressed = availableOutputSpace - zStream->avail_out;
                mCurrentBlockOffset += bytesDecompressed;
            } while (zStream->avail_out == 0 && mCurrentBlockOffset < mDataBlockSize);
        } while (inflateStatus != Z_STREAM_END && mCurrentBlockOffset != mDataBlockSize);
    }
    else
    {
        mCurrentBlockOffset = mFileStream->read(uncompressedDataBlock.data(), mDataBlockSize);
    }

    // Except for the last block this resize will be a no-op
    uncompressedDataBlock.resize(mCurrentBlockOffset);
    // Indicate that this block is now loaded
    setBlockResident(blockId, uncompressedDataBlock.data());
}

void FrameCaptureBinaryData::closeBinaryDataLoader()
{
    clear();
}

int FileStreamSeek(FILE *stream, long long offset, int whence)
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return _fseeki64(stream, offset, whence);
#else
    return fseeko(stream, offset, whence);
#endif
}

long long FileStreamTell(FILE *stream)
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return _ftelli64(stream);
#else
    return ftello(stream);
#endif
}

void FileStream::write(const uint8_t *data, size_t size)
{
    if (fwrite(data, 1, size, mFile) != size)
    {
        if (ferror(mFile))
        {
            FATAL() << "Error writing " << size << " bytes to binary data file.";
        }
    }
    if (fflush(mFile) != 0)
    {
        FATAL() << "Error flushing data to binary data file.";
    }
}

size_t FileStream::read(uint8_t *buffer, size_t size)
{
    size_t readBytes = fread(buffer, 1, size, mFile);
    if (readBytes < size && ferror(mFile))
    {
        FATAL() << "Error reading from binary data file.";
    }
    return readBytes;
}

void FileStream::seek(long long offset, int whence)
{
    if (FileStreamSeek(mFile, static_cast<off_t>(offset), whence) != 0)
    {
        FATAL() << "Error seeking in binary data file with offset " << offset << " and whence "
                << whence;
    }
}

size_t FileStream::getPosition()
{
    long long offset = FileStreamTell(mFile);
    if (offset == -1)
    {
        FATAL() << "Error getting position in binary data file " << mFilePath;
    }
    angle::CheckedNumeric<size_t> checkedOffset(offset);
    size_t safeOffset = 0;
    if (!checkedOffset.AssignIfValid(&safeOffset))
    {
        FATAL() << "ANGLE file seek position offset out of range";
    }

    return safeOffset;
}
}  // namespace angle
