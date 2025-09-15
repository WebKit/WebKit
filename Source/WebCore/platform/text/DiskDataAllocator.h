#pragma once

#include "config.h"

#if ENABLE(PARKABLE_STRINGS)

#include "DiskDataMetadata.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/FileSystem.h>
#include <wtf/FileHandle.h>
#include <span>
#include <map>

namespace WebCore {

// DiskDataAllocator - Centralized chunk-based disk storage allocator
// Manages allocation and deallocation of chunks within a single disk file
class WEBCORE_EXPORT DiskDataAllocator {
    WTF_MAKE_NONCOPYABLE(DiskDataAllocator);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~DiskDataAllocator();
    
    // Singleton access
    static DiskDataAllocator& instance();
   
    // Can be called before first use to set capacity limits
    void setCapacityLimit(size_t maxCapacityMB);
    void disableCapacityLimit();

    void provideTemporaryFile(FileSystem::FileHandle&&, const String& filePath = { });
    
    bool mayWrite();
    
    // The ReservedChunk must be either written via Write() or will be automatically discarded on destruction
    std::unique_ptr<ReservedChunk> tryReserveChunk(size_t size);
    std::unique_ptr<DiskDataMetadata> write(std::unique_ptr<ReservedChunk>, const Vector<uint8_t>& data);
    void read(const DiskDataMetadata&, Vector<uint8_t>& data);
    // Discards allocated space, making it available for reuse
    void discard(std::unique_ptr<DiskDataMetadata>);
    
    // Statistics
    size_t diskFootprint() const;
    size_t freeChunksSize() const;

    String getTempFilePath() const;
    void setMayWriteForTesting(bool mayWrite);
    
    // Testing utilities
    bool hasCapacityLimit() const;
    size_t maxCapacity() const;
    size_t allocatedChunksCount() const;

    bool isValidStateForTesting() const;

protected:
    DiskDataAllocator();
    mutable Lock m_lock;
    // std::map for free chunks, required for lower_bound/upper_bound operations in allocation algorithm
    std::map<int64_t, size_t> m_freeChunks WTF_GUARDED_BY_LOCK(m_lock);
    size_t m_freeChunksSize WTF_GUARDED_BY_LOCK(m_lock) { 0 };

private:
    virtual std::optional<size_t> doWrite(int64_t offset, std::span<const uint8_t> data);
    virtual void doRead(int64_t offset, std::span<uint8_t> data);
    
    DiskDataMetadata findFreeChunk(size_t size) WTF_REQUIRES_LOCK(m_lock);
    void releaseChunk(const DiskDataMetadata&) WTF_REQUIRES_LOCK(m_lock);
    
    // File backend
    FileSystem::FileHandle m_file;
    String m_filePath;
    int64_t m_fileTail WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    
    bool m_hasCapacityLimit { false };
    size_t m_maxCapacity { 0 };
    bool m_mayWrite { false };
    
#if ASSERT_ENABLED
    std::map<int64_t, size_t> m_allocatedChunks WTF_GUARDED_BY_LOCK(m_lock);
#endif

    friend class InMemoryDataAllocator;

    friend class NeverDestroyed<DiskDataAllocator>;
};

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)

