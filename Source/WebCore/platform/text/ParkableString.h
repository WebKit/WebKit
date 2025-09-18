#pragma once

#include "config.h"

#if ENABLE(PARKABLE_STRINGS)

#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Threading.h>
#include <wtf/MonotonicTime.h>
#include <wtf/FileSystem.h>
#include <JavaScriptCore/ArrayBuffer.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class ParkableStringManager;

struct BackgroundTaskParams;

// ParkableString represents a string that may be parked in memory, that it its
// underlying memory address may change. Its content can be retrieved with the
// |ToString()| method.
// As a consequence, the inner pointer should never be cached, and only touched
// through a string returned by the |ToString()| method.
// It is safe to call `ToString()` and destroy ParkableStrings from any thread,
// although the interactions with the ParkableStringManager must always be
// performed on the main thread.

class WEBCORE_EXPORT ParkableStringImpl final : public WTF::ThreadSafeRefCounted<ParkableStringImpl> {
    friend class ParkableStringManager;
public:
    enum class ParkingMode {
        SynchronousOnly,
        CompressOnly
    };
    
    enum class AgeOrParkResult {
        SuccessOrTransientFailure,  // Operation succeeded or can be retried
        NonTransientFailure         // Operation failed permanently
    };
    
    enum class State {
        Unparked, 
        Parked
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

    // Hash-based deduplication support
    static constexpr size_t kDigestSize = 32; // SHA256
    using SecureDigest = WTF::Vector<uint8_t, kDigestSize>;
    
    static std::unique_ptr<SecureDigest> hashString(WTF::StringImpl* string);

    static WTF::Ref<ParkableStringImpl> makeNonParkable(WTF::RefPtr<WTF::StringImpl>);
    static WTF::Ref<ParkableStringImpl> makeParkable(WTF::RefPtr<WTF::StringImpl>, std::unique_ptr<SecureDigest> digest);

    ParkableStringImpl(const ParkableStringImpl&) = delete;
    ParkableStringImpl& operator=(const ParkableStringImpl&) = delete;
    
    ~ParkableStringImpl();

    // Basic string properties (always available, no metadata required)
    size_t length() const {
        if (!mayBeParked())
            return m_string.length();
        return m_metadata->length;
    }
    bool is8Bit() const {
        if (!mayBeParked())
            return m_string.is8Bit();
        return m_metadata->is8Bit;
    }
    size_t sizeInBytes() const;
    
    // Memory profiling support
    struct MemoryUsage {
        size_t thisSize;              // Size of ParkableStringImpl + metadata + compressed data
        const void* stringImpl;      // Pointer to underlying StringImpl (for tracking) 
        size_t stringImplSize;       // Size of StringImpl + character data
    };
    MemoryUsage memoryUsageForSnapshot() const;
    size_t memoryFootprintForDump() const;
    
    // Parking eligibility check
    bool mayBeParked() const { return !!m_metadata; }
    
    // Parking-related queries (require metadata)
    bool hasCompressedData() const;
    bool hasCompressedDataNoLock() const WTF_REQUIRES_LOCK(m_metadata->lock);
    bool isParked() const; 
    bool isCompressionFailed() const; 
    size_t compressedSize() const; 
    
    // Hash-based deduplication support
    const SecureDigest* digest() const { 
        return m_metadata ? m_metadata->digest.get() : nullptr; 
    }
    
    bool isOnOwningThread() const;
    
    WTF::String toString();
    WTF::RefPtr<WTF::StringImpl> impl();
    
    // Locking mechanism to prevent parking (requires metadata)
    void lock();   // Increment lock count, prevents parking
    void unlock(); // Decrement lock count
    void lockWithoutMakingYoung(); // Used in background tasks
    
    bool park(ParkingMode mode = ParkingMode::CompressOnly);
    AgeOrParkResult maybeParkString();

    State currentState() const;
    Age currentAge() const;

    // Core parking operations
    bool parkInternal(ParkingMode mode) WTF_REQUIRES_LOCK(m_metadata->lock);
    void unpark() WTF_REQUIRES_LOCK(m_metadata->lock);
    WTF::String unparkInternal() WTF_REQUIRES_LOCK(m_metadata->lock);
    WTF::String unparkFromCompressed() WTF_REQUIRES_LOCK(m_metadata->lock);

    // State management
    bool canParkNow() const WTF_REQUIRES_LOCK(m_metadata->lock);
    void discardUncompressedData() WTF_REQUIRES_LOCK(m_metadata->lock);
    void discardCompressedData() WTF_REQUIRES_LOCK(m_metadata->lock);
    void makeYoung() WTF_REQUIRES_LOCK(m_metadata->lock);
    void ageString() WTF_REQUIRES_LOCK(m_metadata->lock);
    Status currentStatus() const WTF_REQUIRES_LOCK(m_metadata->lock);
    
    // Helper methods for memory usage calculation
    bool isParkedNoLock() const WTF_REQUIRES_LOCK(m_metadata->lock);

    // Public compression/decompression methods for testing
    static std::unique_ptr<WTF::Vector<uint8_t>> compressData(std::span<const uint8_t> data);
#if PLATFORM(COCOA)
    static std::unique_ptr<WTF::Vector<uint8_t>> compressDataWithAppleCompression(std::span<const uint8_t> data);
#endif
    static std::unique_ptr<WTF::Vector<uint8_t>> compressDataWithZlib(std::span<const uint8_t> data);
    static std::optional<WTF::String> decompressData(const WTF::Vector<uint8_t>& compressedData, bool is8Bit, unsigned length);
#if PLATFORM(COCOA)
    static std::optional<WTF::String> decompressDataWithAppleCompression(const WTF::Vector<uint8_t>& compressedData, bool is8Bit, unsigned length);
#endif
    static std::optional<WTF::String> decompressDataWithZlib(const WTF::Vector<uint8_t>& compressedData, bool is8Bit, unsigned length);
    
    int lockDepthForTesting() const;

private:
    explicit ParkableStringImpl(WTF::RefPtr<WTF::StringImpl>&&, std::unique_ptr<SecureDigest> digest);
    
    // Conditional metadata allocation
    struct ParkableMetadata {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ParkableMetadata);
        ParkableMetadata(WTF::String string, std::unique_ptr<SecureDigest> digest);
        ParkableMetadata(const ParkableMetadata&) = delete;
        ParkableMetadata& operator=(const ParkableMetadata&) = delete;

        mutable WTF::Lock lock;
        
        // Lock count to prevent parking while string is in use
        unsigned lockCount WTF_GUARDED_BY_LOCK(lock) { 0 };
        
        State state WTF_GUARDED_BY_LOCK(lock) { State::Unparked };
        Age age WTF_GUARDED_BY_LOCK(lock) { Age::Young };
        
        // Background task tracking
        bool backgroundTaskInProgress { false }; 
        bool compressionFailed WTF_GUARDED_BY_LOCK(lock) { false };
        
        std::unique_ptr<WTF::Vector<uint8_t>> compressedData WTF_GUARDED_BY_LOCK(lock);
        
        const std::unique_ptr<SecureDigest> digest;
        
        const bool is8Bit;
        const unsigned length;
    };
    
    void scheduleCompressionTask(ParkingMode mode) WTF_REQUIRES_LOCK(m_metadata->lock);
    
    static void compressInBackground(std::unique_ptr<BackgroundTaskParams> params);

    void onCompressionCompleteOnMainThread(std::unique_ptr<BackgroundTaskParams> params, std::unique_ptr<WTF::Vector<uint8_t>> compressedData, WTF::Seconds parkingThreadTime);
    
    WTF::String m_string;
    
    const std::unique_ptr<ParkableMetadata> m_metadata;
    
#if ASSERT_ENABLED
    const uint32_t m_owningThreadUID;
#endif
    
    static constexpr size_t kMinimumSizeForParking = 10 * 1024; // 10KB
    
#if ASSERT_ENABLED
    void assertOnValidThread() const { ASSERT(isOnOwningThread()); }
#else
    void assertOnValidThread() const { }
#endif
};

// ParkableString - A string wrapper that provides memory-efficient storage
// for large strings through compression.
// 
// This is the main public interface that users should interact with.
// It behaves like a normal string but can automatically compress itself
// to save memory when not actively being used.

class WEBCORE_EXPORT ParkableString final {
public:
    ParkableString();
    explicit ParkableString(WTF::RefPtr<WTF::StringImpl>);
    
    ParkableString(const ParkableString&);
    ParkableString& operator=(const ParkableString&);
    ParkableString(ParkableString&&);
    ParkableString& operator=(ParkableString&&);
    
    ~ParkableString();

    // Basic properties
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

private:
    WTF::RefPtr<ParkableStringImpl> m_impl;
};

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
