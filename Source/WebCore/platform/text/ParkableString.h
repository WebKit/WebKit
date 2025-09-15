#pragma once

#include "config.h"

#if ENABLE(PARKABLE_STRINGS)

#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/MonotonicTime.h>
#include <wtf/FileSystem.h>
#include <JavaScriptCore/ArrayBuffer.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class ParkableStringManager;
struct BackgroundTaskParams;
class DiskDataMetadata;

// ParkableStringImpl - The core implementation of a string that can be "parked" (compressed and moved to reduce memory usage).
// State transitions are managed by ParkableStringManager to ensure proper main thread synchronization and string deduplication.
class WEBCORE_EXPORT ParkableStringImpl final : public WTF::ThreadSafeRefCounted<ParkableStringImpl> {
    friend class ParkableStringManager;
public:
    enum class ParkingMode {
        SynchronousOnly,
        CompressOnly,
        WriteToDisk,
        CompressThenWriteToDisk
    };
    
    enum class State {
        Unparked,
        Parked,
        OnDisk,
        DiskCorrupted
    };
    
    enum class Age {
        Young = 0,
        Old = 1,
        VeryOld = 2
    };
    
    enum class Status {
        UnreferencedExternally,
        TooManyReferences,
        Locked
    };

    enum class AgeOrParkResult {
        SuccessOrTransientFailure,  // Operation succeeded or failed temporarily (can retry)
        NonTransientFailure         // Operation failed permanently (don't retry)
    };

    // Hash-based deduplication support
    static constexpr size_t kDigestSize = 32;
    using SecureDigest = WTF::Vector<uint8_t, kDigestSize>;
    static std::unique_ptr<SecureDigest> hashString(WTF::StringImpl* string);

    static WTF::Ref<ParkableStringImpl> create(WTF::RefPtr<WTF::StringImpl>);
    
    static WTF::Ref<ParkableStringImpl> makeNonParkable(WTF::RefPtr<WTF::StringImpl>);
    static WTF::Ref<ParkableStringImpl> makeParkable(WTF::RefPtr<WTF::StringImpl>, std::unique_ptr<SecureDigest> digest);

    ParkableStringImpl(const ParkableStringImpl&) = delete;
    ParkableStringImpl& operator=(const ParkableStringImpl&) = delete;
    
    ~ParkableStringImpl();

    bool isNull() const;
    size_t length() const {
        if (!mayBeParked())
            return m_string ? m_string->length() : 0;
        return m_metadata->length;
    }
    bool is8Bit() const {
        if (!mayBeParked())
            return m_string ? m_string->is8Bit() : true;
        return m_metadata->is8Bit;
    }
    size_t sizeInBytes() const;
    
    struct MemoryUsage {
        size_t thisSize;
        const void* stringImpl;
        size_t stringImplSize;
    };
    MemoryUsage memoryUsageForSnapshot() const;
    size_t memoryFootprintForDump() const;
    
    bool mayBeParked() const { return !!m_metadata; }
    
    bool hasCompressedData() const;
    bool hasCompressedDataNoLock() const WTF_REQUIRES_LOCK(m_metadata->lock);
    bool isParked() const;
    bool isCompressionFailed() const;
    size_t compressedSize() const;
    
    const SecureDigest* digest() const { 
        return m_metadata ? m_metadata->digest.get() : nullptr; 
    }
    
    String digestString() const;
    
    std::optional<Vector<uint8_t>> compressedData() const;
    
    // String access
    WTF::String toString();
    WTF::RefPtr<WTF::StringImpl> impl();
    
    // Locking mechanism to prevent parking\
    void lock();   // Increment lock count, prevents parking
    void unlock(); // Decrement lock count
    void lockWithoutMakingYoung();
    
    // Parking operations
    bool park(ParkingMode mode = ParkingMode::CompressOnly);
    AgeOrParkResult maybeParkString();

    // Disk storage info
    bool isOnDisk() const;
    State currentState() const;
    Age currentAge() const;
    
    // Disk storage operations
    bool hasOnDiskData() const;
    size_t onDiskSize() const;
    const String& diskPath() const;

    // Core parking operations
    bool parkInternal(ParkingMode mode) WTF_REQUIRES_LOCK(m_metadata->lock);
    void unpark() WTF_REQUIRES_LOCK(m_metadata->lock);
    WTF::String unparkInternal() WTF_REQUIRES_LOCK(m_metadata->lock);
    WTF::String unparkFromCompressed() WTF_REQUIRES_LOCK(m_metadata->lock);
    WTF::String unparkFromDisk() WTF_REQUIRES_LOCK(m_metadata->lock);

    // State management
    bool canParkNow() const WTF_REQUIRES_LOCK(m_metadata->lock);
    void discardUncompressedData() WTF_REQUIRES_LOCK(m_metadata->lock);
    void discardCompressedData() WTF_REQUIRES_LOCK(m_metadata->lock);
    void makeYoung() WTF_REQUIRES_LOCK(m_metadata->lock);
    void ageString() WTF_REQUIRES_LOCK(m_metadata->lock);
    Status currentStatus() const WTF_REQUIRES_LOCK(m_metadata->lock);
    
    // Helper methods
    bool isParkedNoLock() const WTF_REQUIRES_LOCK(m_metadata->lock);
    bool isOnDiskNoLock() const WTF_REQUIRES_LOCK(m_metadata->lock);
    
    // Access compressed data
    std::optional<Vector<uint8_t>> compressedDataNoLock() const WTF_REQUIRES_LOCK(m_metadata->lock);

private:
    explicit ParkableStringImpl(WTF::RefPtr<WTF::StringImpl>, bool parkable = true);
    explicit ParkableStringImpl(WTF::RefPtr<WTF::StringImpl>, std::unique_ptr<SecureDigest> digest);
    
    // metadata allocation
    struct ParkableMetadata {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        ParkableMetadata(WTF::String string, std::unique_ptr<SecureDigest> digest);
        ParkableMetadata(const ParkableMetadata&) = delete;
        ParkableMetadata& operator=(const ParkableMetadata&) = delete;
        
        mutable WTF::Lock lock;
        
        unsigned lockCount WTF_GUARDED_BY_LOCK(lock) { 0 };
        
        State state WTF_GUARDED_BY_LOCK(lock) { State::Unparked };
        Age age WTF_GUARDED_BY_LOCK(lock) { Age::Young };
        
        bool hasCompressedData WTF_GUARDED_BY_LOCK(lock) { false };
        bool hasOnDiskData WTF_GUARDED_BY_LOCK(lock) { false };
        bool backgroundTaskInProgress WTF_GUARDED_BY_LOCK(lock) { false };
        bool compressionFailed WTF_GUARDED_BY_LOCK(lock) { false };
        
        // Data storage
        std::unique_ptr<WTF::Vector<uint8_t>> compressedData WTF_GUARDED_BY_LOCK(lock);
        std::unique_ptr<DiskDataMetadata> diskMetadata WTF_GUARDED_BY_LOCK(lock);
        
        // Deduplication digest
        const std::unique_ptr<SecureDigest> digest;
        
        // Cached string properties
        const bool is8Bit;
        const unsigned length;
    };
    
    // Compression/decompression helpers
    static std::unique_ptr<WTF::Vector<uint8_t>> compressData(const WTF::Vector<uint8_t>& data);
    std::optional<WTF::String> decompress() WTF_REQUIRES_LOCK(m_metadata->lock);
    
    // Background task
    void scheduleCompressionTask(ParkingMode mode) WTF_REQUIRES_LOCK(m_metadata->lock);
    static void compressInBackground(std::unique_ptr<BackgroundTaskParams> params);

    // Background operations callback
    void onCompressionCompleteOnMainThread(std::unique_ptr<BackgroundTaskParams> params, std::unique_ptr<WTF::Vector<uint8_t>> compressedData);
    
    // NetworkProcess-controlled state transitions (require metadata)
    void transitionToOnDiskWithoutMetadata();
    
    // Compression operations  
    static std::optional<WTF::String> decompressData(const WTF::Vector<uint8_t>& compressedData, bool is8Bit, unsigned length);
    
    WTF::RefPtr<WTF::StringImpl> m_string;
    
    const std::unique_ptr<ParkableMetadata> m_metadata;

    static constexpr size_t kMinimumSizeForParking = 10 * 1024; // 10KB
    static constexpr size_t kMinimumSizeForDisk = 50 * 1024;    // 50KB for disk storage
    
#if ASSERT_ENABLED
    void assertOnValidThread() const { ASSERT(isMainThread()); }
#else
    void assertOnValidThread() const { }
#endif
};

// ParkableString - A string wrapper that provides memory-efficient storage for large strings through compression.
class WEBCORE_EXPORT ParkableString final {
public:
    ParkableString();
    explicit ParkableString(WTF::RefPtr<WTF::StringImpl>);
    
    ParkableString(const ParkableString&);
    ParkableString& operator=(const ParkableString&);
    ParkableString(ParkableString&&);
    ParkableString& operator=(ParkableString&&);
    
    ~ParkableString();

    bool isNull() const;
    size_t length() const;
    bool is8Bit() const;
    size_t sizeInBytes() const;
    
    bool mayBeParked() const;
    bool isParked() const;
    size_t compressedSize() const;
    
    WTF::String toString() const;
    WTF::RefPtr<WTF::StringImpl> impl() const;
    
    ParkableStringImpl* Impl() const;
    
    size_t memoryFootprintForDump() const;
    
    void lock();
    void unlock();
    
    bool park(ParkableStringImpl::ParkingMode mode = ParkableStringImpl::ParkingMode::CompressOnly);

    bool isOnDisk() const;
    size_t onDiskSize() const;

private:
    WTF::RefPtr<ParkableStringImpl> m_impl;
};

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
