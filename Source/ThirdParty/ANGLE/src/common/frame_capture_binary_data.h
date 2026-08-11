//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_binary_data.h:
//   Common code for the ANGLE trace replay large trace binary data definition.
//

#ifndef FRAME_CAPTURE_BINARY_DATA_H_
#define FRAME_CAPTURE_BINARY_DATA_H_

#include "common/debug.h"

#include <stddef.h>
#include <fstream>
#include <vector>

namespace angle
{

constexpr size_t kDefaultBinaryDataSize = 0x80000000;
constexpr size_t kDefaultDataBlockSize  = 256 * 1024 * 1024;
constexpr size_t kBinaryAlignment       = 16;
// The zlib doc recommends buffer sizes on the order of 128K or 256K bytes
constexpr size_t kZlibBufferSize     = 256 * 1024;
constexpr uint32_t kInvalidBlockId   = 0xFFFFFFFF;
constexpr size_t kLongTraceVersionId = 1;

// Index information ultimately saved in trace JSON file
struct BinaryFileIndexInfo
{
    size_t version;       // Long file data description version number
    size_t blockSize;     // Size of binary data blocks in bytes
    size_t blockCount;    // Number of FileBlockInfo structures in file index trailer
    size_t residentSize;  // Max amount of device memory used for binary data storage
    size_t indexOffset;   // Offset in gzip file specifying start of file block descriptions
    BinaryFileIndexInfo() : version(1), blockSize(0), blockCount(0), residentSize(0), indexOffset(0)
    {}
};

class FileStream;

class FrameCaptureBinaryData
{
  public:
    enum class Mode
    {
        Load,
        Store
    };

    // Describes a block's location in the binary data file
    struct FileBlockInfo
    {
        size_t fileOffset;  // Offset within the binary disk file
        size_t dataOffset;  // Starting offset in the logical flat data view
        size_t dataSize;    // Actual size of data in this block

        FileBlockInfo() : fileOffset(0), dataOffset(0), dataSize(0) {}
    };

    // Describes a block's state during replay
    struct ReplayBlockDescription
    {
        size_t fileOffset;         // Seek offset in binary data disk file
        size_t beginDataOffset;    // Beginning flat binary data offset
        size_t endDataOffset;      // Ending flat binary data offset (inclusive)
        size_t dataSize;           // Size of data in this block
        uint8_t *residentAddress;  // Memory address if resident, nullptr otherwise
    };

    std::vector<std::vector<uint8_t>> &data() { return mData; }
    bool isSwapBlock(size_t blockId) { return blockId == mMaxResidentBlockIndex; }
    size_t totalSize() const;
    bool isSwapMode() const;
    bool isBlockResident(size_t blockId) const;
    void setBlockResident(size_t blockId, uint8_t *address);
    void setBlockNonResident(size_t blockId);
    void setBinaryDataSize(size_t binaryDataSize);
    void setBlockSize(size_t blockSize);

    void storeResidentBlocks();
    // Format data for appending to compressed binary file
    BinaryFileIndexInfo appendFileIndex();
    // Create binary memory block description index from binary file information
    void constructBlockDescIndex(size_t indexOffset);

    size_t append(const void *data, size_t size);
    void clear();
    const uint8_t *getData(size_t offset);

    void initializeBinaryDataStore(bool compression,
                                   const std::string &outDir,
                                   const std::string &fileName);
    void storeBlock();
    BinaryFileIndexInfo closeBinaryDataStore();
    void configureBinaryDataLoader(bool compression,
                                   size_t blockCount,
                                   size_t blockSize,
                                   size_t residentSize,
                                   size_t indexOffset,
                                   const std::string &fileName);
    void initializeBinaryDataLoader();
    void loadBlock(size_t blockId);
    void closeBinaryDataLoader();
    void updateGetDataCache(size_t blockId);
    std::vector<uint8_t> &prepareLoadBlock(size_t blockId);
    std::vector<uint8_t> &prepareStoreBlock(size_t blockId);

  private:
    bool mIsBinaryDataCompressed;
    std::string mFileName;
    size_t mIndexOffset = 0;

    std::vector<FileBlockInfo> mFileIndex;

    uint32_t mStoredBlocks                = 0;
    size_t mCurrentTransientLoadedBlockId = kInvalidBlockId;
    size_t mCurrentBlockOffset            = 0;
    size_t mMaxResidentBinarySize         = kDefaultBinaryDataSize;
    size_t mMaxResidentBlockIndex         = (kDefaultBinaryDataSize / kDefaultDataBlockSize) - 1;

    // Binary data information
    size_t mDataBlockSize = kDefaultDataBlockSize;
    size_t mBlockCount    = 0;

    // getData() fastpath cache information
    size_t mCacheBlockId            = kInvalidBlockId;
    size_t mCacheBlockBeginOffset   = 0;
    size_t mCacheBlockEndOffset     = 0;
    uint8_t *mCacheBlockBaseAddress = nullptr;

    std::vector<ReplayBlockDescription> mReplayBlockDescriptions;

    // Chrome's allocator disallows creating one allocation that's bigger than 2GB, so the following
    // is one large buffer that is split in multiple pieces in memory.
    // Note: Make sure compression on write is disabled with a Chrome capture
    // (ANGLE_CAPTURE_COMPRESSION=0), since that would require bundling all data in one big
    // allocation.
    std::vector<std::vector<uint8_t>> mData;
    // Indicator that capture is complete and store can be finalized
    bool mCaptureComplete = false;

    FileStream *mFileStream = nullptr;
};

constexpr int kSeekBegin = SEEK_SET;
constexpr int kSeekEnd   = SEEK_END;

class FileStream
{
  public:
    explicit FileStream(const std::string &filePath, FrameCaptureBinaryData::Mode mode)
        : mMode(mode), mFile(nullptr), mFilePath(filePath)
    {
        std::string openMode;
        if (mMode == FrameCaptureBinaryData::Mode::Store)
        {
            openMode = "wb+";
        }
        else if (mMode == FrameCaptureBinaryData::Mode::Load)
        {
            openMode = "rb";
        }
        else
        {
            FATAL() << "Invalid Mode enum in FileStream helper";
        }
        mFile = fopen(mFilePath.c_str(), openMode.c_str());

        if (!mFile)
        {
            FATAL() << "Could not open binary data file " << mFilePath;
        }
    }

    ~FileStream()
    {
        if (mFile)
        {
            fclose(mFile);
            mFile = nullptr;
        }
    }

    void write(const uint8_t *data, size_t size);
    size_t read(uint8_t *buffer, size_t size);
    void seek(long long offset, int whence);
    size_t getPosition();

  private:
    FrameCaptureBinaryData::Mode mMode;
    FILE *mFile;
    std::string mFilePath;
};
}  // namespace angle
#endif  // FRAME_CAPTURE_BINARY_DATA_H_
