#pragma once

#include "config.h"

#if ENABLE(PARKABLE_STRINGS)

#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#include <wtf/FileSystem.h>
#include <WebCore/DiskDataMetadata.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/MemoryPressureHandler.h>
#include <WebKit/SandboxExtension.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class DiskDataAllocator;
}

namespace WebKit {

class NetworkProcess;

// NetworkParkableStringStorage - Network Process storage for parkable strings
// Manages compressed string data storage for Web Content Processes.
// This class runs in the Network Process and provides disk-backed storage
// for parkable strings
// 
// Architecture:
// - Web Content Process: ParkableStringManager sends IPC requests
// - Network Process: NetworkParkableStringStorage handles actual disk I/O
// - Per-origin storage with quota management integration
class NetworkParkableStringStorage {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(NetworkParkableStringStorage);
    
public:
    NetworkParkableStringStorage(NetworkProcess&);
    ~NetworkParkableStringStorage();
    
    
    // Storage operations (called from NetworkStorageManager IPC handlers)
    bool storeCompressedString(const String& digest, const Vector<uint8_t>& compressedData);
    std::optional<Vector<uint8_t>> retrieveCompressedString(const String& digest);
    void discardString(const String& digest);
    void clearAllStrings();
    
    // Disk location query for on-demand IPC
    struct DiskLocation {
        uint64_t offset;
        uint64_t size;
    };
    std::optional<DiskLocation> getDiskLocation(const String& digest);
    
    // Lifecycle management
    void initialize();
    void shutdown();
    
    // Statistics and monitoring
    size_t diskFootprint() const;
    size_t memoryFootprint() const;
    
    // Testing support
    void clearAllForTesting();
    bool hasStringForTesting(const String& digest) const;
    
    // Hybrid approach - NetworkProcess initiated storage
    void handleCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, WebCore::ProcessIdentifier);
    void requestCandidatesFromWebContent(WebCore::ProcessIdentifier);
    void storeCandidates(const Vector<String>& digests, const Vector<Vector<uint8_t>>& compressedData, const Vector<uint32_t>& originalSizes, WebCore::ProcessIdentifier);
    void notifyStorageComplete(const String& digest, bool success, WebCore::ProcessIdentifier);
    
    // File access management using existing DiskDataAllocator
    void grantFileAccessToWebContent(WebCore::ProcessIdentifier);
    String getTempFilePath() const;
    
    bool shouldRequestMoreCandidates() const;

private:
    struct StoredStringMetadata {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        String digest;
        std::unique_ptr<WebCore::DiskDataMetadata> diskMetadata;
        size_t compressedSize;
        
        StoredStringMetadata(String&& digest, std::unique_ptr<WebCore::DiskDataMetadata>&& metadata, size_t size)
            : digest(WTFMove(digest)), diskMetadata(WTFMove(metadata)), compressedSize(size) { }
    };
    
    using StringStorageMap = HashMap<String, std::unique_ptr<StoredStringMetadata>>; // digest -> metadata
    
    // Initialize disk storage allocator
    void initializeDiskAllocator();
    
    struct CandidateRequest {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        uint32_t maxCount;
        uint64_t maxTotalSize;
        MonotonicTime requestTime;
    };
    
    // Single pending request instead of per-origin
    std::unique_ptr<CandidateRequest> m_pendingRequest WTF_GUARDED_BY_LOCK(m_lock);
    
    mutable Lock m_lock;
    StringStorageMap m_stringStorage WTF_GUARDED_BY_LOCK(m_lock);
    
    // Disk allocator reference (singleton from WebCore)
    WebCore::DiskDataAllocator* m_diskAllocator { nullptr };
    bool m_initialized { false };
    
    // File access state (single extension for all WebContent processes)
    bool m_fileAccessGranted { false };
    bool m_underMemoryPressure { false };
    MonotonicTime m_lastRequestTime;
    
    // NetworkProcess reference for IPC communication
    WeakPtr<NetworkProcess> m_networkProcess;
};

} // namespace WebKit

#endif // ENABLE(PARKABLE_STRINGS)
