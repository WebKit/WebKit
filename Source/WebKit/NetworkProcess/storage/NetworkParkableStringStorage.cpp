#include "config.h"
#include "NetworkParkableStringStorage.h"

#if ENABLE(PARKABLE_STRINGS)

#include "SandboxExtension.h"
#include <WebCore/DiskDataAllocator.h>
#include <WebCore/DiskDataMetadata.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/FileSystem.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/MemoryPressureHandler.h>
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkProcess.h"
#include <wtf/MonotonicTime.h>
#include <algorithm>

namespace WebKit {

// Constructor that asserts main thread usage.
NetworkParkableStringStorage::NetworkParkableStringStorage(NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
{
    ASSERT(isMainThread());
}

// Destructor that calls shutdown to clean up resources.
NetworkParkableStringStorage::~NetworkParkableStringStorage()
{
    shutdown();
}

// Sets up DiskDataAllocator for parkable string storage.
void NetworkParkableStringStorage::initialize()
{
    ASSERT(isMainThread());
    
    if (m_initialized)
        return;
        
    initializeDiskAllocator();
    m_initialized = true;
}

void NetworkParkableStringStorage::initializeDiskAllocator()
{
    // Use the singleton DiskDataAllocator instance
    m_diskAllocator = &WebCore::DiskDataAllocator::instance();
    
    m_diskAllocator->disableCapacityLimit();
    
    auto [tempFilePath, tempFileHandle] = FileSystem::openTemporaryFile("NetworkParkableStrings"_s, ".data"_s);
    
    if (tempFileHandle.isValid()) {
        m_diskAllocator->provideTemporaryFile(WTFMove(tempFileHandle), tempFilePath);
    }
}

void NetworkParkableStringStorage::shutdown()
{       
    Locker locker { m_lock };
    m_stringStorage.clear();
    m_pendingRequest = nullptr;
    m_diskAllocator = nullptr;
    m_initialized = false;
}

// Stores a compressed string in the disk allocator.
bool NetworkParkableStringStorage::storeCompressedString(const String& digest, const Vector<uint8_t>& compressedData)
{
    if (!m_initialized) {
        initialize();
        if (!m_initialized || !m_diskAllocator)
            return false;
    }
    
    if (!m_diskAllocator->mayWrite())
        return false;
    
    Locker locker { m_lock };
    
    if (m_stringStorage.contains(digest)) {
        return true;
    }
    
    locker.unlockEarly();
    
    // Reserve chunk for compressed data using DiskDataAllocator
    auto reservedChunk = m_diskAllocator->tryReserveChunk(compressedData.size());
    if (!reservedChunk)
        return false;
    
    // Write compressed data to disk
    auto diskMetadata = m_diskAllocator->write(WTFMove(reservedChunk), compressedData);
    if (!diskMetadata)
        return false;
    
    // Update our metadata tracking
    Locker newLocker { m_lock };
    auto storedMetadata = makeUnique<StoredStringMetadata>(String { digest }, WTFMove(diskMetadata), compressedData.size());
    m_stringStorage.set(digest, WTFMove(storedMetadata));
    
    return true;
}

// Retrieves a compressed string from the disk allocator.
std::optional<Vector<uint8_t>> NetworkParkableStringStorage::retrieveCompressedString(const String& digest)
{
    if (!m_initialized || !m_diskAllocator)
        return std::nullopt;
    
    Locker locker { m_lock };
    
    auto stringIterator = m_stringStorage.find(digest);
    if (stringIterator == m_stringStorage.end())
        return std::nullopt;
    
    auto& metadata = *stringIterator->value;
    auto* diskMetadata = metadata.diskMetadata.get();
    size_t compressedSize = metadata.compressedSize;
    
    locker.unlockEarly();
    
    // Read compressed data from disk
    Vector<uint8_t> compressedData;
    compressedData.resize(compressedSize);
    
    m_diskAllocator->read(*diskMetadata, compressedData);
    
    return compressedData;
}

void NetworkParkableStringStorage::discardString(const String& digest)
{
    if (!m_initialized || !m_diskAllocator)
        return;
    
    Locker locker { m_lock };
    
    auto stringIterator = m_stringStorage.find(digest);
    if (stringIterator == m_stringStorage.end())
        return;
    
    auto metadata = WTFMove(stringIterator->value);
    m_stringStorage.remove(stringIterator);
    
    locker.unlockEarly();
    
    // Discard disk space
    m_diskAllocator->discard(WTFMove(metadata->diskMetadata));
}

std::optional<NetworkParkableStringStorage::DiskLocation> NetworkParkableStringStorage::getDiskLocation(const String& digest)
{
    if (!m_initialized || !m_diskAllocator)
        return std::nullopt;
    
    Locker locker { m_lock };
    
    auto stringIterator = m_stringStorage.find(digest);
    if (stringIterator == m_stringStorage.end())
        return std::nullopt;
    
    auto& metadata = *stringIterator->value;
    if (!metadata.diskMetadata)
        return std::nullopt;
        
    return DiskLocation { static_cast<uint64_t>(metadata.diskMetadata->startOffset()), metadata.diskMetadata->size() };
}

void NetworkParkableStringStorage::clearAllStrings()
{
    if (!m_initialized || !m_diskAllocator)
        return;
    
    Locker locker { m_lock };
    
    auto stringStorage = WTFMove(m_stringStorage);
    m_stringStorage.clear();
    
    locker.unlockEarly();
    
    // Discard all disk data
    for (auto& pair : stringStorage) {
        m_diskAllocator->discard(WTFMove(pair.value->diskMetadata));
    }
}

// Returns total disk usage across all origin storages.
size_t NetworkParkableStringStorage::diskFootprint() const
{
    if (!m_diskAllocator)
        return 0;
    return m_diskAllocator->diskFootprint();
}

// Returns memory usage of storage metadata structures.
size_t NetworkParkableStringStorage::memoryFootprint() const
{
    Locker locker { m_lock };
    
    size_t total = 0;
    for (const auto& stringPair : m_stringStorage) {
        total += stringPair.key.sizeInBytes(); // digest string
        total += sizeof(StoredStringMetadata);
    }
    return total;
}

// Test helper performing complete shutdown and cleanup.
void NetworkParkableStringStorage::clearAllForTesting()
{
    shutdown();
}

// Test helper checking if string exists for specific origin.
bool NetworkParkableStringStorage::hasStringForTesting(const String& digest) const
{
    Locker locker { m_lock };
    return m_stringStorage.contains(digest);
}

// Grants file access to WebContent process via SandboxExtension.
void NetworkParkableStringStorage::grantFileAccessToWebContent(WebCore::ProcessIdentifier processIdentifier)
{
    if (!m_diskAllocator)
        return;
        
    String tempFilePath = getTempFilePath();
    if (tempFilePath.isEmpty())
        return;
        
    // Check if already granted
    if (m_fileAccessGranted)
        return;
        
    m_fileAccessGranted = true;
    
    // Send IPC to WebContent to grant access
    if (auto* networkProcess = m_networkProcess.get()) {
        if (auto* connection = networkProcess->webProcessConnection(processIdentifier)) {
            // Create a new extension handle for this specific WebProcess
            if (auto extensionHandle = SandboxExtension::createHandle(tempFilePath, SandboxExtension::Type::ReadOnly)) {
                connection->connection().send(Messages::NetworkProcessConnection::ParkableStringGrantFileAccess(tempFilePath, WTFMove(*extensionHandle)), 0);
            }
        }
    }
}

// Gets the temporary file path used by DiskDataAllocator.
String NetworkParkableStringStorage::getTempFilePath() const
{
    if (!m_diskAllocator)
        return String();
    return m_diskAllocator->getTempFilePath();
}

// Handle single string notification from WebContent.
void NetworkParkableStringStorage::handleCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, WebCore::ProcessIdentifier processIdentifier)
{
    if (!m_initialized)
        return;
        
    if (!shouldRequestMoreCandidates())
        return;
        
    // Grant read-only file access for WebContent to read compressed data later
    grantFileAccessToWebContent(processIdentifier);
    
    // Request the single string from WebContent
    requestCandidatesFromWebContent(processIdentifier);
}

// Request single string from WebContent with unlimited disk space.
void NetworkParkableStringStorage::requestCandidatesFromWebContent(WebCore::ProcessIdentifier processIdentifier)
{
    uint32_t maxCount = 1;  // Only one string at a time
    uint64_t maxTotalSize = SIZE_MAX; // No size limit with unlimited disk

    {
        Locker locker { m_lock };
        auto request = makeUnique<CandidateRequest>();
        request->maxCount = maxCount;
        request->maxTotalSize = maxTotalSize;
        request->requestTime = MonotonicTime::now();
        m_pendingRequest = WTFMove(request);
    }
    
    m_lastRequestTime = MonotonicTime::now();
    
    // Send controlled request with precise limits to WebContent
    if (auto* networkProcess = m_networkProcess.get()) {
        if (auto* connection = networkProcess->webProcessConnection(processIdentifier)) {
            connection->connection().sendWithAsyncReply(
                Messages::NetworkProcessConnection::ParkableStringRequestCandidates(maxCount, maxTotalSize),
                [this, processIdentifier](Vector<String>&& digests, Vector<Vector<uint8_t>>&& compressedData, Vector<uint32_t>&& originalSizes) {
                    // Handle WebContent's response with actual compressed data
                    storeCandidates(digests, compressedData, originalSizes, processIdentifier);
                }
            );
        }
    }
}

// Processes single compressed string from WebContent.
void NetworkParkableStringStorage::storeCandidates(const Vector<String>& digests, const Vector<Vector<uint8_t>>& compressedData, const Vector<uint32_t>& originalSizes, WebCore::ProcessIdentifier processIdentifier)
{
    if (!m_initialized || !m_diskAllocator)
        return;
        
    {
        Locker locker { m_lock };
        m_pendingRequest = nullptr;
    }
    
    if (digests.isEmpty() || compressedData.isEmpty()) {
        return;
    }
    
    const String& digest = digests[0];
    const Vector<uint8_t>& data = compressedData[0];
    
    if (data.isEmpty()) {
        return;
    }
    
    auto success = storeCompressedString(digest, data);

    notifyStorageComplete(digest, success, processIdentifier);
}

// Notifies WebContent process about storage completion.
void NetworkParkableStringStorage::notifyStorageComplete(const String& digest, bool success, WebCore::ProcessIdentifier processIdentifier)
{
    // Send IPC to WebContent to notify about storage completion with disk metadata
    if (auto* networkProcess = m_networkProcess.get()) {
        if (auto* connection = networkProcess->webProcessConnection(processIdentifier)) {
            connection->connection().send(Messages::NetworkProcessConnection::ParkableStringStorageComplete(digest, success), 0);
        }
    }
}

// Always request the single string candidate with unlimited disk space.
bool NetworkParkableStringStorage::shouldRequestMoreCandidates() const
{
    // Prevent duplicate concurrent requests
    {
        Locker locker { m_lock };
        if (m_pendingRequest)
            return false;
    }
    
    return true;
}

} // namespace WebKit

#endif // ENABLE(PARKABLE_STRINGS)
