#pragma once

#include "config.h"

#if ENABLE(PARKABLE_STRINGS)

#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>
#include "ParkableString.h"


namespace WebCore {

// Manages all the ParkableStrings, and parks eligible strings after the
// renderer has been backgrounded.
// Main Thread only.
// When a `ParkableString` is unparked on a background thread, a task is posted
// to the main thread to update the entries in the manager. Hence, it is
// possible to temporarily have an unparked `ParkableString` inaccessible
// through `unparked_strings_`. This can cause aging of the string to be
// delayed or a variation on the sizes recorded in 'ComputeStatistics()`.
class WEBCORE_EXPORT ParkableStringManager {
public:
    // Hash traits for secure digest-based deduplication
    // Compares digest contents, not pointers, and uses first 4 bytes as hash
    struct SecureDigestHashTraits : WTF::GenericHashTraits<const ParkableStringImpl::SecureDigest*> {
        static unsigned hash(const ParkableStringImpl::SecureDigest* digest) {
            // The first bytes of the hash are as good as anything else.
            return *reinterpret_cast<const unsigned*>(digest->begin());
        }

        static bool equal(const ParkableStringImpl::SecureDigest* const a, 
                         const ParkableStringImpl::SecureDigest* const b) {
            return a == b || *a == *b;
        }

        static constexpr bool safeToCompareToEmptyOrDeleted = false;
    };
    
    // Hash-based string map for deduplication
    using StringMap = WTF::HashMap<const ParkableStringImpl::SecureDigest*, 
                                   WTF::RefPtr<ParkableStringImpl>, 
                                   SecureDigestHashTraits>;

    static ParkableStringManager& instance();
    
    ~ParkableStringManager();
    
    // String lifecycle management with deduplication
    WTF::RefPtr<ParkableStringImpl> add(WTF::RefPtr<WTF::StringImpl>);
    WTF::RefPtr<ParkableStringImpl> add(WTF::RefPtr<WTF::StringImpl>, std::unique_ptr<ParkableStringImpl::SecureDigest> digest);
    void remove(ParkableStringImpl*);
    
    // Determine if a string should be parkable
    static bool shouldPark(const WTF::StringImpl& string);
    
    // State transition callbacks (called from ParkableStringImpl)
    void completeParked(ParkableStringImpl*);
    void completeUnpark(ParkableStringImpl*, Seconds elapsed = 0_s, Seconds diskElapsed = 0_s);
    void completeWrittenToDisk(ParkableStringImpl*, Seconds diskElapsed = 0_s);
    
    // Main thread callbacks
    void onParked(ParkableStringImpl*);
    void onUnparked(ParkableStringImpl*);
    void onWrittenToDisk(ParkableStringImpl*);
    
    // Lightweight notifications (WebContent -> NetworkProcess)
    void notifyCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, CompletionHandler<void()>&& callback);
    
    // Controlled requests handler (NetworkProcess -> WebContent)
    void handleCandidateRequest(uint32_t maxCount, uint64_t maxTotalSize, Vector<String>& outDigests, Vector<Vector<uint8_t>>& outCompressedData, Vector<uint32_t>& outOriginalSizes);
    
    // Read-only disk access for sync unpark (NetworkProcess grants access)
    std::optional<Vector<uint8_t>> readCompressedDataFromDisk(const String& digest);
    
    // IPC helper to request disk location from NetworkProcess
    std::optional<std::pair<uint64_t, uint64_t>> requestDiskLocationSync(const String& digest);
    
    // Storage configuration
    void configureDiskCapacity(size_t capacityMB);
    void disableDiskCapacityLimit();
    
    // Statistics and testing
    void recordParkingThreadTime(Seconds time) { m_totalParkingThreadTime += time; }
    void recordUnparkingTime(Seconds time) { m_totalUnparkingTime += time; }
    void recordDiskWriteTime(Seconds time) { m_totalDiskWriteTime += time; }
    void recordDiskReadTime(Seconds time) { m_totalDiskReadTime += time; }
    void recordCompression() { ++m_totalCompressions; }
    void recordDecompression() { ++m_totalDecompressions; }
    
    size_t size() const;
    bool isOnParkedMapForTesting(ParkableStringImpl* string);
    bool isOnDiskMapForTesting(ParkableStringImpl* string);
    void resetForTesting();
    
    // Memory management
    void purgeMemory();

    void scheduleAgingTaskIfNeeded();
    void ageStringsAndPark();
    void parkAll(ParkableStringImpl::ParkingMode mode);
    
    void setRendererBackgrounded(bool backgrounded);
    void onMemoryPressure();
    
    struct MemoryStatistics {
        size_t totalStrings { 0 };
        size_t unparkedStrings { 0 };
        size_t parkedStrings { 0 };
        size_t onDiskStrings { 0 };
        size_t totalUncompressedSize { 0 };
        size_t totalCompressedSize { 0 };
        size_t totalDiskSize { 0 };
        size_t metadataOverhead { 0 };
        double averageCompressionRatio { 0.0 };
        size_t compressionSavings { 0 };
        
        // Performance metrics
        size_t totalCompressions { 0 };
        size_t totalDecompressions { 0 };
        Seconds totalCompressionTime { 0_s };
        Seconds totalDecompressionTime { 0_s };
        Seconds totalDiskWriteTime { 0_s };
        Seconds totalDiskReadTime { 0_s };
    };
    
    MemoryStatistics getMemoryStatistics() const;
    
    void grantFileAccess(const String& tempFilePath);
    
    bool hasFileAccess() const { return m_hasFileAccess; }
    const String& tempFilePath() const { return m_tempFilePath; }
    
    void onStringTransitionedToDisk(const String& digest);
    
private:
    ParkableStringManager();
    
    friend class WTF::NeverDestroyed<ParkableStringManager>;
    
    bool moveString(ParkableStringImpl*, StringMap* from, StringMap* to);
    
    void removeOnMainThread(ParkableStringImpl*);
    
    WTF::Vector<ParkableStringImpl*> enumerateStrings(const StringMap& strings);
    bool hasPendingWork() const;
    bool isPaused() const;
    static Seconds agingInterval();
    
    mutable WTF::Lock m_lock;
    
    StringMap m_unparkedStrings;
    StringMap m_parkedStrings;
    StringMap m_onDiskStrings;
    
    Seconds m_totalParkingThreadTime;
    Seconds m_totalUnparkingTime;
    Seconds m_totalDiskWriteTime;
    Seconds m_totalDiskReadTime;
    
    // Performance counters
    mutable size_t m_totalCompressions { 0 };
    mutable size_t m_totalDecompressions { 0 };
    
    // Background aging system state
    bool m_hasPendingAgingTask { false };
    bool m_firstStringAgingWasDelayed { false };
    bool m_rendererBackgrounded { false };
    
    // Aging constants
    static const Seconds kFirstParkingDelay;
    static const Seconds kAgingInterval;
    static const Seconds kLessAggressiveAgingInterval;
    
    String m_tempFilePath;
    bool m_hasFileAccess { false };
    
    static ParkableStringManager* s_instance;
};

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
