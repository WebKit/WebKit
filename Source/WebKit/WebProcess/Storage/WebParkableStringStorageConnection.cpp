#include "config.h"
#include "WebParkableStringStorageConnection.h"

#if ENABLE(PARKABLE_STRINGS)

#include "NetworkProcessConnection.h"
#include "NetworkStorageManagerMessages.h"
#include "SandboxExtension.h"
#include "WebProcess.h"
#include <WebCore/ParkableStringManager.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebKit {

// Singleton accessor using NeverDestroyed pattern.
WebParkableStringStorageConnection& WebParkableStringStorageConnection::singleton()
{
    static NeverDestroyed<WebParkableStringStorageConnection> connection;
    return connection;
}

IPC::Connection& WebParkableStringStorageConnection::networkProcessConnection()
{
    return WebProcess::singleton().ensureNetworkProcessConnection().connection();
}


void WebParkableStringStorageConnection::notifyCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, CompletionHandler<void()>&& completionHandler)
{
    networkProcessConnection().sendWithAsyncReply(
        Messages::NetworkStorageManager::ParkableStringNotifyCandidatesAvailable(candidateCount, estimatedSize),
        WTFMove(completionHandler)
    );
}

void WebParkableStringStorageConnection::handleCandidateRequest(uint32_t maxCount, uint64_t maxTotalSize, CompletionHandler<void(Vector<String>&&, Vector<Vector<uint8_t>>&&, Vector<uint32_t>&&)>&& completionHandler)
{
    auto& manager = WebCore::ParkableStringManager::instance();
    
    Vector<String> digests;
    Vector<Vector<uint8_t>> compressedData;
    Vector<uint32_t> originalSizes;
    
    manager.handleCandidateRequest(maxCount, maxTotalSize, digests, compressedData, originalSizes);
    
    completionHandler(WTFMove(digests), WTFMove(compressedData), WTFMove(originalSizes));
}

void WebParkableStringStorageConnection::grantDiskReadAccess(const String& tempFilePath, WebKit::SandboxExtensionHandle&& sandboxExtensionHandle)
{
    m_tempFilePath = tempFilePath;
    
    if (auto created = SandboxExtension::create(WTFMove(sandboxExtensionHandle))) {
        m_sandboxExtension = created;
        if (!m_sandboxExtension->consume()) {
            m_sandboxExtension = nullptr;
            m_tempFilePath = String();
            return;
        }
    }
    
    auto& manager = WebCore::ParkableStringManager::instance();
    manager.grantFileAccess(tempFilePath);
}

void WebParkableStringStorageConnection::notifyStorageComplete(const String& digest, bool success)
{
    callOnMainThread([digest = digest.isolatedCopy(), success] {
        auto& manager = WebCore::ParkableStringManager::instance();
        
        if (success) {
            manager.onStringTransitionedToDisk(digest);
        }
    });
}

std::optional<std::pair<uint64_t, uint64_t>> WebParkableStringStorageConnection::requestDiskLocationSync(const String& digest)
{
    if (digest.isEmpty()) {
        return std::nullopt;
    }
    
    // Send synchronous message to NetworkStorageManager
    auto sendResult = networkProcessConnection().sendSync(Messages::NetworkStorageManager::ParkableStringGetDiskLocation(digest), 0);
    
    // Handle response with validation
    if (sendResult.succeeded()) {
        auto [offset, size] = sendResult.reply();
        if (offset && size && *offset > 0 && *size > 0) {
            return std::make_pair(*offset, *size);
        }
    }
    
    return std::nullopt;
}

std::optional<Vector<uint8_t>> WebParkableStringStorageConnection::readCompressedDataFromDisk(const String& digest)
{
    if (!m_sandboxExtension || m_tempFilePath.isEmpty()) {
        return std::nullopt;
    }
    
    // Request disk location via IPC
    auto locationPair = requestDiskLocationSync(digest);
    if (!locationPair) {
        return std::nullopt;
    }
    
    // Read from file using sandbox extension
    auto fileHandle = FileSystem::openFile(m_tempFilePath, FileSystem::FileOpenMode::Read);
    if (!fileHandle) {
        return std::nullopt;
    }
    
    // Seek to the specific offset
    if (!fileHandle.seek(locationPair->first, FileSystem::FileSeekOrigin::Beginning)) {
        return std::nullopt;
    }
    
    // Prepare buffer for compressed data
    Vector<uint8_t> compressedData;
    compressedData.resize(locationPair->second); // size
    
    // Read data from current position
    auto bytesRead = fileHandle.read(compressedData.span());
    
    if (!bytesRead || *bytesRead != locationPair->second) {
        return std::nullopt;
    }
    
    return compressedData;
}

} // namespace WebKit

#endif // ENABLE(PARKABLE_STRINGS)