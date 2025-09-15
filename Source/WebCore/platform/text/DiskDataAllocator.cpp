#include "config.h"
#include "DiskDataAllocator.h"

#if ENABLE(PARKABLE_STRINGS)
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/FileSystem.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#include <span>
#include <algorithm>
#include <map>

#if OS(UNIX)
#include <unistd.h> 
#include <errno.h> 
#endif

namespace WebCore {

namespace {
constexpr size_t MB = 1024 * 1024;
}

DiskDataAllocator::DiskDataAllocator()
{
    // Initialize without capacity limits by default
    // Capacity limits can be configured via setCapacityLimit() if needed
}

DiskDataAllocator::~DiskDataAllocator() = default;

DiskDataAllocator& DiskDataAllocator::instance()
{
    static NeverDestroyed<DiskDataAllocator> allocator;
    return allocator.get();
}

void DiskDataAllocator::setCapacityLimit(size_t maxCapacityMB)
{
    Locker locker { m_lock };
    m_hasCapacityLimit = true;
    m_maxCapacity = maxCapacityMB * MB;
}

// Removes disk usage restrictions for unlimited allocation
void DiskDataAllocator::disableCapacityLimit()
{
    Locker locker { m_lock };
    m_hasCapacityLimit = false;
    m_maxCapacity = 0;
}

bool DiskDataAllocator::mayWrite()
{
    Locker locker { m_lock };
    return m_mayWrite;
}

void DiskDataAllocator::setMayWriteForTesting(bool mayWrite)
{
    Locker locker { m_lock };
    m_mayWrite = mayWrite;
}

DiskDataMetadata DiskDataAllocator::findFreeChunk(size_t size)
{
    // 1. Exact fit (reuse chunk of exact size)
    // 2. Worst fit (use largest available chunk to reduce fragmentation)
    
    DiskDataMetadata chosenChunk { -1, 0 };
    size_t worstFitSize = 0;
    
    for (const auto& chunk : m_freeChunks) {
        size_t chunkSize = chunk.second;
        if (size == chunkSize) {
            chosenChunk = DiskDataMetadata { chunk.first, chunk.second };
            break;
        } else if (chunkSize > size && chunkSize >= worstFitSize) {
            // When sizes are equal, prefer higher offsets for consistent behavior
            chosenChunk = DiskDataMetadata { chunk.first, chunk.second };
            worstFitSize = chunkSize;
        }
    }
    
    if (chosenChunk.startOffset() != -1) {
        m_freeChunksSize -= size;
        m_freeChunks.erase(chosenChunk.startOffset());
        
        // Split chunk if necessary
        if (chosenChunk.size() > size) {
            int64_t remainderOffset = chosenChunk.startOffset() + static_cast<int64_t>(size);
            size_t remainderSize = chosenChunk.size() - size;
            auto insertResult = m_freeChunks.insert({remainderOffset, remainderSize});
            ASSERT_UNUSED(insertResult, insertResult.second);
            
            // Update chosen chunk size to requested size
            chosenChunk = DiskDataMetadata { chosenChunk.startOffset(), size };
        }
    }
    
    return chosenChunk;
}

void DiskDataAllocator::releaseChunk(const DiskDataMetadata& metadata)
{
    DiskDataMetadata chunk { metadata.startOffset(), metadata.size() };
    
    ASSERT(m_freeChunks.find(chunk.startOffset()) == m_freeChunks.end());
    
    // Use lower_bound for efficient left neighbor detection
    auto lowerBound = m_freeChunks.lower_bound(chunk.startOffset());
    ASSERT(m_freeChunks.upper_bound(chunk.startOffset()) == 
           m_freeChunks.lower_bound(chunk.startOffset()));
    
    if (lowerBound != m_freeChunks.begin()) {
        // There is a chunk to the left
        auto left = --lowerBound;
        int64_t leftChunkEnd = left->first + static_cast<int64_t>(left->second);
        ASSERT(leftChunkEnd <= chunk.startOffset());
        if (leftChunkEnd == chunk.startOffset()) {
            chunk = DiskDataMetadata { left->first, left->second + chunk.size() };
            m_freeChunksSize -= left->second;
            m_freeChunks.erase(left);
        }
    }
    
    auto right = m_freeChunks.upper_bound(chunk.startOffset());
    if (right != m_freeChunks.end()) {
        ASSERT(right->first != chunk.startOffset());
        int64_t chunkEnd = chunk.startOffset() + static_cast<int64_t>(chunk.size());
        ASSERT(chunkEnd <= right->first);
        if (right->first == chunkEnd) {
            chunk = DiskDataMetadata { chunk.startOffset(), chunk.size() + right->second };
            m_freeChunksSize -= right->second;
            m_freeChunks.erase(right);
        }
    }
    
    auto insertResult = m_freeChunks.insert({chunk.startOffset(), chunk.size()});
    ASSERT_UNUSED(insertResult, insertResult.second);
    m_freeChunksSize += chunk.size();
}

std::unique_ptr<ReservedChunk> DiskDataAllocator::tryReserveChunk(size_t size)
{
    Locker locker { m_lock };
    if (!m_mayWrite) {
        return nullptr;
    }

    DiskDataMetadata chosenChunk = findFreeChunk(size);
    if (chosenChunk.startOffset() < 0) {
        if (m_hasCapacityLimit && m_fileTail + size > m_maxCapacity) {
            return nullptr;
        }
        chosenChunk = DiskDataMetadata { m_fileTail, size };
        m_fileTail += size;
    }

#if ASSERT_ENABLED
    m_allocatedChunks.insert({chosenChunk.startOffset(), chosenChunk.size()});
#endif

    return makeUnique<ReservedChunk>(
        this, makeUnique<DiskDataMetadata>(
                chosenChunk.startOffset(), chosenChunk.size()));
}

std::unique_ptr<DiskDataMetadata> DiskDataAllocator::write(
    std::unique_ptr<ReservedChunk> chunk,
    const Vector<uint8_t>& data)
{
    std::unique_ptr<DiskDataMetadata> metadata = chunk->take();
    ASSERT(metadata);

    auto written = doWrite(metadata->startOffset(), data.span().first(metadata->size()));

    if (!written || metadata->size() != written.value()) {
        discard(WTFMove(metadata));

        // Assume that the error is not transient. This can happen if the disk is full for instance, in which case it is likely better not to try writing later.
        Locker locker { m_lock };
        m_mayWrite = false;
        return nullptr;
    }

    return metadata;
}

void DiskDataAllocator::read(const DiskDataMetadata& metadata,
                             Vector<uint8_t>& data)
{
    doRead(metadata.startOffset(), data.mutableSpan().first(metadata.size()));

#if ASSERT_ENABLED
    {
        Locker locker { m_lock };
        auto iterator = m_allocatedChunks.find(metadata.startOffset());
        ASSERT(iterator != m_allocatedChunks.end());
        ASSERT(metadata.size() == iterator->second);
    }
#endif
}

void DiskDataAllocator::discard(std::unique_ptr<DiskDataMetadata> metadata)
{
    Locker locker { m_lock };
    ASSERT(m_mayWrite || m_file.isValid());

#if ASSERT_ENABLED
    auto iterator = m_allocatedChunks.find(metadata->startOffset());
    ASSERT(iterator != m_allocatedChunks.end());
    ASSERT(metadata->size() == iterator->second);
    m_allocatedChunks.erase(iterator);
#endif

    releaseChunk(*metadata);
}

size_t DiskDataAllocator::diskFootprint() const
{
    Locker locker { m_lock };
    return m_fileTail;
}

size_t DiskDataAllocator::freeChunksSize() const
{
    Locker locker { m_lock };
    return m_freeChunksSize;
}

String DiskDataAllocator::getTempFilePath() const
{
    return m_filePath;
}

void DiskDataAllocator::provideTemporaryFile(FileSystem::FileHandle&& file, const String& filePath)
{
    Locker locker { m_lock };
    ASSERT(isMainThread());
    ASSERT(!m_file.isValid());
    ASSERT(!m_mayWrite);

    m_file = WTFMove(file);
    m_filePath = filePath;
    m_mayWrite = m_file.isValid();
}

std::optional<size_t> DiskDataAllocator::doWrite(int64_t offset, std::span<const uint8_t> data)
{
    if (!m_file.isValid())
        return std::nullopt;
    
#if OS(UNIX)
    // Use positioned I/O (pwrite) on UNIX platforms
    int fd = m_file.platformHandle();
    ssize_t written;
    do {
        written = ::pwrite(fd, data.data(), data.size(), offset);
    } while (written == -1 && errno == EINTR);
    
    if (written >= 0) {
        return static_cast<size_t>(written);
    }
    return std::nullopt;
#else
    // Use WebKit's FileHandle seek() + write() for non-UNIX platforms
    auto seekResult = m_file.seek(offset, FileSystem::FileSeekOrigin::Beginning);
    if (!seekResult) {
        return std::nullopt;
    }
    
    auto written = m_file.write(data);
    
    if (!written || written.value() != data.size()) {
    }
    
    return written;
#endif
}

void DiskDataAllocator::doRead(int64_t offset, std::span<uint8_t> data)
{
    if (!m_file.isValid()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
#if OS(UNIX)
    // Use positioned I/O (pread) on UNIX platforms
    int fd = m_file.platformHandle();
    ssize_t totalRead = 0;
    
    while (totalRead < static_cast<ssize_t>(data.size())) {
        ssize_t currentRead;
        auto remainingData = data.subspan(totalRead);
        do {
            currentRead = ::pread(fd, 
                                remainingData.data(), 
                                remainingData.size(), 
                                offset + totalRead);
        } while (currentRead == -1 && errno == EINTR);
        
        if (currentRead <= 0) {
            ASSERT_NOT_REACHED();
            return;
        }
        
        totalRead += currentRead;
    }
#else
    // Use WebKit's FileHandle seek() + read() for non-UNIX platforms
    auto seekResult = m_file.seek(offset, FileSystem::FileSeekOrigin::Beginning);
    if (!seekResult) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    auto read = m_file.read(data);
    
    if (!read || read.value() != data.size()) {
        ASSERT_NOT_REACHED();
    }
#endif
}

bool DiskDataAllocator::hasCapacityLimit() const
{
    Locker locker { m_lock };
    return m_hasCapacityLimit;
}

size_t DiskDataAllocator::maxCapacity() const
{
    Locker locker { m_lock };
    return m_maxCapacity;
}

size_t DiskDataAllocator::allocatedChunksCount() const
{
#if ASSERT_ENABLED
    Locker locker { m_lock };
    return m_allocatedChunks.size();
#else
    return 0;
#endif
}

bool DiskDataAllocator::isValidStateForTesting() const
{
    Locker locker { m_lock };
    
    size_t totalFreeSize = 0;
    for (const auto& chunk : m_freeChunks) {
        totalFreeSize += chunk.second;
        
        if (chunk.second == 0)
            return false;
            
        if (chunk.first < 0 || chunk.first >= m_fileTail)
            return false;
    }
    
    if (totalFreeSize != m_freeChunksSize)
        return false;
    
#if ASSERT_ENABLED
    // Verify no overlap between allocated and free chunks
    for (const auto& allocatedChunk : m_allocatedChunks) {
        int64_t allocStart = allocatedChunk.first;
        int64_t allocEnd = allocStart + static_cast<int64_t>(allocatedChunk.second);
        
        for (const auto& freeChunk : m_freeChunks) {
            int64_t freeStart = freeChunk.first;
            int64_t freeEnd = freeStart + static_cast<int64_t>(freeChunk.second);
            
            // Check for any overlap
            if (!(allocEnd <= freeStart || freeEnd <= allocStart))
                return false;
        }
    }
#endif
    
    return true;
}

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS) 
