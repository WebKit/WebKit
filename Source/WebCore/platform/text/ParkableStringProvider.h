#pragma once

#include "config.h"

#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <optional>

#if ENABLE(PARKABLE_STRINGS)

namespace WebCore {

// Abstract provider interface for parkable string storage operations.
// This interface enables WebCore's parkable string system to communicate
// with WebKit's NetworkProcess for intelligent disk storage without
// creating direct dependencies between layers.
class WEBCORE_EXPORT ParkableStringProvider {
public:
    virtual ~ParkableStringProvider();
    
    // Singleton access
    static ParkableStringProvider& singleton();
    static void setSharedProvider(ParkableStringProvider&);
    
    // Lightweight notification to NetworkProcess about available compressed strings
    using NotificationCallback = CompletionHandler<void()>;
    virtual void notifyCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, NotificationCallback&&) = 0;
    
    // Handle NetworkProcess request for actual compressed data within specified limits
    using CandidateRequestCallback = CompletionHandler<void(Vector<String>&& digests, Vector<Vector<uint8_t>>&& compressedData, Vector<uint32_t>&& originalSizes)>;
    virtual void handleCandidateRequest(uint32_t maxCount, uint64_t maxTotalSize, CandidateRequestCallback&&) = 0;
    
    // Handle NetworkProcess notification of storage completion
    virtual void notifyStorageComplete(const String& digest, bool success) = 0;
    
    // Request disk location for compressed data via synchronous IPC
    virtual std::optional<std::pair<uint64_t, uint64_t>> requestDiskLocationSync(const String& digest) = 0;
    
    // Read compressed data directly from disk (WebKit layer handles sandbox extensions)
    virtual std::optional<Vector<uint8_t>> readCompressedDataFromDisk(const String& digest) = 0;
    
protected:
    ParkableStringProvider();
    
private:
    static ParkableStringProvider* s_sharedProvider;
};

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
