#pragma once

#include "config.h"

#if ENABLE(PARKABLE_STRINGS)

#include <wtf/CompletionHandler.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#include <optional>

namespace WebKit {

// WebParkableStringStorageConnection - WebProcess connection for parkable string storage
// Handles lightweight notifications from WebProcess to NetworkProcess.
// NetworkProcess makes intelligent storage decisions and sends controlled requests back.
// WebContent responds with actual compressed data only when specifically requested.
class WebParkableStringStorageConnection {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebParkableStringStorageConnection);
    
public:
    static WebParkableStringStorageConnection& singleton();
    
    // Notification system for NetworkProcess-controlled storage
    void notifyCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, CompletionHandler<void()>&&);
    
    // Handle NetworkProcess request for actual compressed data within specified limits
    void handleCandidateRequest(uint32_t maxCount, uint64_t maxTotalSize, CompletionHandler<void(Vector<String>&&, Vector<Vector<uint8_t>>&&, Vector<uint32_t>&&)>&&);
    
    // Handle NetworkProcess grant of read-only file access for sync unpark operations
    void grantDiskReadAccess(const String& tempFilePath, WebKit::SandboxExtensionHandle&& sandboxExtensionHandle);
    
    // Handle NetworkProcess notification of storage completion
    void notifyStorageComplete(const String& digest, bool success);
    
    // Request disk location for compressed data via synchronous IPC
    std::optional<std::pair<uint64_t, uint64_t>> requestDiskLocationSync(const String& digest);
    
    // Read compressed data directly from disk
    std::optional<Vector<uint8_t>> readCompressedDataFromDisk(const String& digest);

private:
    WebParkableStringStorageConnection() = default;
    ~WebParkableStringStorageConnection() = default;
    
    class IPC::Connection& networkProcessConnection();
    
    RefPtr<class SandboxExtension> m_sandboxExtension;
    String m_tempFilePath;
    
    friend class WTF::NeverDestroyed<WebParkableStringStorageConnection>;
};


} // namespace WebKit

#endif // ENABLE(PARKABLE_STRINGS)
