#include "config.h"
#include "ParkableStringManager.h"

#if ENABLE(PARKABLE_STRINGS)

#include "ParkableString.h"
#include "ParkableStringProvider.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/StringImpl.h>
#include <wtf/FileSystem.h>
#include <WebCore/Document.h>
#include <WebCore/SecurityOrigin.h>

namespace WebCore {

const Seconds ParkableStringManager::kFirstParkingDelay { 5_s };
const Seconds ParkableStringManager::kAgingInterval { 2_s };
const Seconds ParkableStringManager::kLessAggressiveAgingInterval { 10_s };

// Returns singleton instance using WebKit's NeverDestroyed pattern.
ParkableStringManager& ParkableStringManager::instance()
{
    static NeverDestroyed<ParkableStringManager> manager;
    return manager;
}

// Constructor that initializes statistics counters.
ParkableStringManager::ParkableStringManager() 
    : m_totalParkingThreadTime(0_s)
    , m_totalUnparkingTime(0_s)
    , m_totalDiskWriteTime(0_s)
    , m_totalDiskReadTime(0_s)
{
    // Lazy initialization - disk storage will be initialized when first needed
}

ParkableStringManager::~ParkableStringManager() = default;

bool ParkableStringManager::shouldPark(const StringImpl& string)
{
    static constexpr unsigned kSizeThreshold = 10240;
    return string.length() > kSizeThreshold && isMainThread();
}

RefPtr<ParkableStringImpl> ParkableStringManager::add(RefPtr<StringImpl> string)
{
    ASSERT(isMainThread());
    
    if (!string)
        return nullptr;

    ASSERT(shouldPark(*string));
    
    auto digest = ParkableStringImpl::hashString(string.get());
    if (!digest)
        return nullptr;
    
    return add(WTFMove(string), WTFMove(digest));
}

RefPtr<ParkableStringImpl> ParkableStringManager::add(RefPtr<StringImpl> string, std::unique_ptr<ParkableStringImpl::SecureDigest> digest)
{
    ASSERT(isMainThread());
    ASSERT(string);
    ASSERT(digest);
    
    Locker locker { m_lock };
    
    auto it = m_unparkedStrings.find(digest.get());
    if (it != m_unparkedStrings.end()) {
        return it->value;
    }
    
    // Check parked strings
    it = m_parkedStrings.find(digest.get());
    if (it != m_parkedStrings.end()) {
        return it->value;
    }
    
    // Check on-disk strings
    it = m_onDiskStrings.find(digest.get());
    if (it != m_onDiskStrings.end()) {
        return it->value;
    }
    
    // No deduplication hit - create new parkable string
    auto parkableString = ParkableStringImpl::makeParkable(WTFMove(string), WTFMove(digest));
    
    // Insert into unparked strings map using digest as key
    auto insertResult = m_unparkedStrings.set(parkableString->digest(), parkableString.copyRef());
    ASSERT_UNUSED(insertResult, insertResult.isNewEntry);
    
    // Release lock before scheduling aging task
    locker.unlockEarly();
    
    // Schedule aging task for new string
    scheduleAgingTaskIfNeeded();
    
    return WTFMove(parkableString);
}

void ParkableStringManager::remove(ParkableStringImpl* string)
{
    if (!string)
        return;
    
    // If we're not on the main thread, post to main thread
    if (!isMainThread()) {
        RunLoop::main().dispatch([this, protectedString = RefPtr<ParkableStringImpl>(string)]() {
            removeOnMainThread(protectedString.get());
        });
        return;
    }
    
    removeOnMainThread(string);
}

// Main thread implementation of string removal from hash maps.
void ParkableStringManager::removeOnMainThread(ParkableStringImpl* string)
{
    ASSERT(isMainThread());
    
    if (!string || !string->mayBeParked() || !string->digest())
        return;
    
    Locker locker { m_lock };
    
    // Use digest-based lookup to find and remove from the appropriate map
    const auto* digest = string->digest();

    if (m_unparkedStrings.remove(digest))
        return;
    if (m_parkedStrings.remove(digest))
        return;
    m_onDiskStrings.remove(digest);
}

// Moves string between different state maps using digest-based lookup.
bool ParkableStringManager::moveString(ParkableStringImpl* string, StringMap* from, StringMap* to)
{
    ASSERT(string);
    ASSERT(string->digest());
    
    const auto* digest = string->digest();
    
    auto it = from->find(digest);
    if (it == from->end())
        return false;
    
    RefPtr<ParkableStringImpl> stringRef = it->value;
    from->remove(it);
    
    auto insertResult = to->set(digest, WTFMove(stringRef));
    ASSERT_UNUSED(insertResult, insertResult.isNewEntry);
    
    return true;
}

// ===== Thread-Routing Methods =====

void ParkableStringManager::completeParked(ParkableStringImpl* string)
{
    if (isMainThread()) {
        onParked(string);
        return;
    }
    
    callOnMainThread([protectedString = RefPtr<ParkableStringImpl>(string)]() {
        ParkableStringManager::instance().onParked(protectedString.get());
    });
}

void ParkableStringManager::completeUnpark(ParkableStringImpl* string, Seconds elapsed, Seconds diskElapsed)
{
    if (isMainThread()) {
        recordUnparkingTime(elapsed);
        recordDecompression();
        if (diskElapsed > 0_s)
            recordDiskReadTime(diskElapsed);
        onUnparked(string);
        return;
    }
    
    callOnMainThread([protectedString = RefPtr<ParkableStringImpl>(string), elapsed, diskElapsed]() {
        auto& manager = ParkableStringManager::instance();
        manager.recordUnparkingTime(elapsed);
        manager.recordDecompression();
        if (diskElapsed > 0_s)
            manager.recordDiskReadTime(diskElapsed);
        manager.onUnparked(protectedString.get());
    });
}

void ParkableStringManager::completeWrittenToDisk(ParkableStringImpl* string, Seconds diskElapsed)
{
    if (isMainThread()) {
        if (diskElapsed > 0_s)
            recordDiskWriteTime(diskElapsed);
        onWrittenToDisk(string);
        return;
    }
    
    callOnMainThread([protectedString = RefPtr<ParkableStringImpl>(string), diskElapsed]() {
        auto& manager = ParkableStringManager::instance();
        if (diskElapsed > 0_s)
            manager.recordDiskWriteTime(diskElapsed);
        manager.onWrittenToDisk(protectedString.get());
    });
}

// ===== Main Thread Callbacks =====

void ParkableStringManager::onParked(ParkableStringImpl* string)
{
    ASSERT(isMainThread());
    ASSERT(string);
    
    {
        Locker locker { m_lock };
        moveString(string, &m_unparkedStrings, &m_parkedStrings);
    }
}

void ParkableStringManager::onUnparked(ParkableStringImpl* string)
{
    ASSERT(isMainThread());
    ASSERT(string);
    
    {
        Locker locker { m_lock };
        if (!moveString(string, &m_parkedStrings, &m_unparkedStrings))
            moveString(string, &m_onDiskStrings, &m_unparkedStrings);
    }

    scheduleAgingTaskIfNeeded();
}

void ParkableStringManager::onWrittenToDisk(ParkableStringImpl* string)
{
    ASSERT(isMainThread());
    ASSERT(string);
    
    Locker locker { m_lock };
    moveString(string, &m_parkedStrings, &m_onDiskStrings);
}

// Returns total count of tracked strings across all three state maps.
size_t ParkableStringManager::size() const
{
    ASSERT(isMainThread());
    Locker locker { m_lock };
    return m_unparkedStrings.size() + m_parkedStrings.size() + m_onDiskStrings.size();
}

bool ParkableStringManager::isOnParkedMapForTesting(ParkableStringImpl* string)
{
    if (!string || !string->digest())
        return false;
        
    Locker locker { m_lock };
    return m_parkedStrings.contains(string->digest());
}

bool ParkableStringManager::isOnDiskMapForTesting(ParkableStringImpl* string)
{
    if (!string || !string->digest())
        return false;
        
    Locker locker { m_lock };
    return m_onDiskStrings.contains(string->digest());
}

void ParkableStringManager::purgeMemory()
{
    ASSERT(isMainThread());
    parkAll(ParkableStringImpl::ParkingMode::CompressOnly);
}

// Test helper to clear all manager state and reset statistics.
void ParkableStringManager::resetForTesting()
{
    ASSERT(isMainThread());
    Locker locker { m_lock };
    
    m_unparkedStrings.clear();
    m_parkedStrings.clear();
    m_onDiskStrings.clear();
    
    // Reset statistics
    m_totalParkingThreadTime = 0_s;
    m_totalUnparkingTime = 0_s;
    m_totalDiskWriteTime = 0_s;
    m_totalDiskReadTime = 0_s;
    m_totalCompressions = 0;
    m_totalDecompressions = 0;
    
    // Reset aging system state
    m_hasPendingAgingTask = false;
    m_firstStringAgingWasDelayed = false;
    m_rendererBackgrounded = false;
}

// ===== Network Process Storage Integration =====

void ParkableStringManager::handleCandidateRequest(uint32_t maxCount, uint64_t maxTotalSize, Vector<String>& outDigests, Vector<Vector<uint8_t>>& outCompressedData, Vector<uint32_t>& outOriginalSizes)
{
    ASSERT(isMainThread());
    
    uint32_t count = 0;
    uint64_t totalSize = 0;
    
    {
        Locker locker { m_lock };
        
        for (const auto& pair : m_parkedStrings) {
            if (count >= maxCount || totalSize >= maxTotalSize)
                break;
                
            ParkableStringImpl* string = pair.value.get();
            if (!string || !string->hasCompressedData())
                continue;
                
            auto compressedBytes = string->compressedData();
            if (!compressedBytes || compressedBytes->isEmpty())
                continue;
                
            if (totalSize + compressedBytes->size() > maxTotalSize)
                break;
                
            outDigests.append(string->digestString());
            outCompressedData.append(*compressedBytes);
            outOriginalSizes.append(string->length());
            
            totalSize += compressedBytes->size();
            count++;
        }
    }
}

std::optional<std::pair<uint64_t, uint64_t>> ParkableStringManager::requestDiskLocationSync(const String& digest)
{
#if ENABLE(PARKABLE_STRINGS)
    // Use provider pattern for synchronous IPC
    auto& provider = ParkableStringProvider::singleton();
    auto result = provider.requestDiskLocationSync(digest);
    return result;
#else
    UNUSED_PARAM(digest);
    return std::nullopt;
#endif
}

std::optional<Vector<uint8_t>> ParkableStringManager::readCompressedDataFromDisk(const String& digest)
{
#if ENABLE(PARKABLE_STRINGS)
    if (!m_hasFileAccess || m_tempFilePath.isEmpty()) {
        return std::nullopt;
    }
    
    // Delegate to provider layer which handles sandbox extensions
    auto& provider = ParkableStringProvider::singleton();
    return provider.readCompressedDataFromDisk(digest);
#else
    UNUSED_PARAM(digest);
    return std::nullopt;
#endif
}

// ===== Memory Dump Provider Implementation =====

ParkableStringManager::MemoryStatistics ParkableStringManager::getMemoryStatistics() const
{
    ASSERT(isMainThread());
    Locker locker { m_lock };
    
    MemoryStatistics stats;
    stats.unparkedStrings = m_unparkedStrings.size();
    stats.parkedStrings = m_parkedStrings.size();
    stats.onDiskStrings = m_onDiskStrings.size();
    stats.totalStrings = stats.unparkedStrings + stats.parkedStrings + stats.onDiskStrings;
    
    size_t totalCompressedBytes = 0;
    size_t totalUncompressedBytes = 0;
    size_t totalDiskBytes = 0;
    size_t metadataBytes = 0;
    
    // Calculate stats for all string maps
    auto calculateForMap = [&](const StringMap& map) {
        for (const auto& pair : map) {
            ParkableStringImpl* string = pair.value.get();
            if (!string)
                continue;
                
            auto usage = string->memoryUsageForSnapshot();
            metadataBytes += usage.thisSize;
            
            // For uncompressed size, use the logical string size, not just StringImpl overhead
            totalUncompressedBytes += string->sizeInBytes();
            
            if (string->hasCompressedData())
                totalCompressedBytes += string->compressedSize();
            
            if (string->hasOnDiskData())
                totalDiskBytes += string->onDiskSize();
        }
    };
    
    calculateForMap(m_unparkedStrings);
    calculateForMap(m_parkedStrings);
    calculateForMap(m_onDiskStrings);
    
    stats.totalUncompressedSize = totalUncompressedBytes;
    stats.totalCompressedSize = totalCompressedBytes;
    stats.totalDiskSize = totalDiskBytes;
    stats.metadataOverhead = metadataBytes;
    
    // Calculate compression ratio and savings
    if (totalUncompressedBytes > 0) {
        if (totalCompressedBytes > 0) {
            stats.averageCompressionRatio = static_cast<double>(totalCompressedBytes) / totalUncompressedBytes;
            stats.compressionSavings = (totalUncompressedBytes > totalCompressedBytes) ? 
                                     (totalUncompressedBytes - totalCompressedBytes) : 0;
        } else {
            stats.averageCompressionRatio = 0.0;
            stats.compressionSavings = 0;
        }
    }
    
    // Performance metrics
    stats.totalCompressions = m_totalCompressions;
    stats.totalDecompressions = m_totalDecompressions;
    stats.totalCompressionTime = m_totalParkingThreadTime;
    stats.totalDecompressionTime = m_totalUnparkingTime;
    stats.totalDiskWriteTime = m_totalDiskWriteTime;
    stats.totalDiskReadTime = m_totalDiskReadTime;
    
    return stats;
}

// ===== Background Aging System =====

// Schedules aging task if needed and not already pending.
// Called when new strings are added or when strings are unparked.
void ParkableStringManager::scheduleAgingTaskIfNeeded()
{
    ASSERT(isMainThread());
    
    if (isPaused())
        return;
    
    if (m_hasPendingAgingTask)
        return;
    
    if (!hasPendingWork())
        return;
    
    Seconds delay = agingInterval();
    
    // Delay the first aging tick, since this renderer may be short-lived
    if (!m_firstStringAgingWasDelayed) {
        delay = kFirstParkingDelay;
        m_firstStringAgingWasDelayed = true;
    }
    
    m_hasPendingAgingTask = true;
    
    RunLoop::main().dispatch([this]() {
        ageStringsAndPark();
    });
}

// Main aging loop that processes all strings and tracks progress.
void ParkableStringManager::ageStringsAndPark()
{
    ASSERT(isMainThread());
    
    m_hasPendingAgingTask = false;
    
    if (isPaused())
        return;
    
    Vector<RefPtr<ParkableStringImpl>> unparkedStrings;
    Vector<RefPtr<ParkableStringImpl>> parkedStrings;
    
    {
        Locker locker { m_lock };
        
        for (auto& pair : m_unparkedStrings) {
            if (pair.value)
                unparkedStrings.append(pair.value);
        }
        
        for (auto& pair : m_parkedStrings) {
            if (pair.value)
                parkedStrings.append(pair.value);
        }
    }
    
    bool canMakeProgress = false;
    
    // Process unparked strings
    for (auto& string : unparkedStrings) {
        if (!string)
            continue;
            
        auto result = string->maybeParkString();
        if (result == ParkableStringImpl::AgeOrParkResult::SuccessOrTransientFailure) {
            canMakeProgress = true;
        }
    }
    
    // Process parked strings
    for (auto& string : parkedStrings) {
        if (!string)
            continue;
            
        auto result = string->maybeParkString();
        if (result == ParkableStringImpl::AgeOrParkResult::SuccessOrTransientFailure) {
            canMakeProgress = true;
        }
    }
    
    // Only reschedule if we have pending work and can make progress
    bool reschedule = hasPendingWork() && canMakeProgress;
    if (reschedule) {
        scheduleAgingTaskIfNeeded();
    }
}

// Parks all unparked strings with the specified mode.
void ParkableStringManager::parkAll(ParkableStringImpl::ParkingMode mode)
{
    ASSERT(isMainThread());
    
    Vector<RefPtr<ParkableStringImpl>> stringsToProcess;
    {
        Locker locker { m_lock };
        for (auto& pair : m_unparkedStrings) {
            if (pair.value)
                stringsToProcess.append(pair.value);
        }
    }
    
    // Park all strings
    for (auto& string : stringsToProcess) {
        if (string)
            string->park(mode);
    }
}

// Sets renderer background state and triggers aging if needed. 
void ParkableStringManager::setRendererBackgrounded(bool backgrounded)
{
    ASSERT(isMainThread());
    
    bool wasPaused = isPaused();
    m_rendererBackgrounded = backgrounded;
    
    if (wasPaused && !isPaused() && hasPendingWork()) {
        scheduleAgingTaskIfNeeded();
    }
}

// Handles memory pressure by immediately parking all strings.
void ParkableStringManager::onMemoryPressure()
{
    ASSERT(isMainThread());
    parkAll(ParkableStringImpl::ParkingMode::CompressOnly);
}

// ===== Background Aging Helpers =====

// Enumerates all strings in a map into a vector.
Vector<ParkableStringImpl*> ParkableStringManager::enumerateStrings(const StringMap& strings)
{
    Vector<ParkableStringImpl*> result;
    result.reserveInitialCapacity(strings.size());
    
    for (const auto& pair : strings) {
        if (pair.value)
            result.append(pair.value.get());
    }
    
    return result;
}

// Checks if there are strings that need aging or parking.
bool ParkableStringManager::hasPendingWork() const
{
    Locker locker { m_lock };
    return !m_unparkedStrings.isEmpty() || !m_parkedStrings.isEmpty();
}

// Determines if aging should be paused based on system state.
bool ParkableStringManager::isPaused() const
{
    // For now, only pause when renderer is not backgrounded
    return !m_rendererBackgrounded;
}

// Returns the interval between aging tasks.
Seconds ParkableStringManager::agingInterval()
{
    // Use less aggressive aging for better performance
    return kLessAggressiveAgingInterval;
}

// Notifies NetworkProcess that parkable string candidates are available.
void ParkableStringManager::notifyCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, CompletionHandler<void()>&& callback)
{
#if ENABLE(PARKABLE_STRINGS) && PLATFORM(COCOA)
    // NetworkProcess immediately responds with a controlled request
    auto& provider = ParkableStringProvider::singleton();
    
    provider.notifyCandidatesAvailable(candidateCount, estimatedSize, WTFMove(callback));
#else
    UNUSED_PARAM(candidateCount);
    UNUSED_PARAM(estimatedSize);
    WTFLogAlways("ParkableStringManager: Candidates notification not implemented for this platform");
    callback();
#endif
}

// Grants file access to the DiskDataAllocator's temporary file.
// Enables direct reading of compressed strings without IPC.
void ParkableStringManager::grantFileAccess(const String& tempFilePath)
{
    m_tempFilePath = tempFilePath;
    m_hasFileAccess = !tempFilePath.isEmpty();
}

// ===== Configuration Methods =====

void ParkableStringManager::configureDiskCapacity(size_t capacityMB)
{
    // Configure the local DiskDataAllocator instance
    WebCore::DiskDataAllocator::instance().setCapacityLimit(capacityMB);
}

void ParkableStringManager::disableDiskCapacityLimit()
{
    // Disable limits on the local DiskDataAllocator instance
    WebCore::DiskDataAllocator::instance().disableCapacityLimit();
}


// Transitions a string to OnDisk state.
void ParkableStringManager::onStringTransitionedToDisk(const String& digest)
{
    ASSERT(isMainThread());
    
    if (digest.isEmpty())
        return;
    
    ParkableStringImpl* string = nullptr;
    {
        Locker locker { m_lock };
        
        for (const auto& pair : m_parkedStrings) {
            if (pair.key && pair.value) {
                String stringDigest = pair.value->digestString();
                if (stringDigest == digest) {
                    string = pair.value.get();
                    break;
                }
            }
        }
    }
    
    if (string && string->mayBeParked()) {
        string->transitionToOnDiskWithoutMetadata();
        
        {
            Locker locker { m_lock };
            moveString(string, &m_parkedStrings, &m_onDiskStrings);
        }
    }
}

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
