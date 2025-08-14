#include "config.h"
#include "ParkableStringManager.h"

#if ENABLE(PARKABLE_STRINGS)

#include "Document.h"
#include "ParkableString.h"
#include "SecurityOrigin.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringImpl.h>

// namespace WebKit {
// class WebParkableStringStorageConnection;
// }

namespace WebCore {

// Aging constants definitions
const Seconds ParkableStringManager::kFirstParkingDelay { 5_s };
const Seconds ParkableStringManager::kAgingInterval { 2_s };
const Seconds ParkableStringManager::kLessAggressiveAgingInterval { 10_s };

// Returns singleton instance using WebKit's NeverDestroyed pattern.
ParkableStringManager& ParkableStringManager::instance()
{
    static NeverDestroyed<ParkableStringManager> manager;
    return manager;
}

ParkableStringManager::ParkableStringManager() 
    : m_totalParkingThreadTime(0_s)
    , m_totalUnparkingTime(0_s){}

ParkableStringManager::~ParkableStringManager() = default;

// Returns the centralized work queue for background operations.
ConcurrentWorkQueue& ParkableStringManager::workQueue()
{
    if (!m_workQueue) {
        m_workQueue = ConcurrentWorkQueue::create("org.webkit.ParkableStringManager.background"_s);
    }
    return *m_workQueue;
}

// static
bool ParkableStringManager::shouldPark(const StringImpl& string)
{
    // Don't attempt to park strings smaller than this size
    static constexpr unsigned kSizeThreshold = 10240; // 10KB
    return string.length() > kSizeThreshold && isMainThread();
}

RefPtr<ParkableStringImpl> ParkableStringManager::add(RefPtr<StringImpl> string)
{
    ASSERT(isMainThread());
    
    if (!string)
        return nullptr;

    ASSERT(shouldPark(*string));
    
    // Compute digest for deduplication
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
    
    // Check unparked strings first
    auto it = m_unparkedStrings.find(digest.get());
    if (it != m_unparkedStrings.end()) {
        return it->value;
    }
    
    // Check parked strings
    it = m_parkedStrings.find(digest.get());
    if (it != m_parkedStrings.end()) {
        return it->value;
    }
    
    // No deduplication hit
    auto parkableString = ParkableStringImpl::makeParkable(WTFMove(string), WTFMove(digest));
    
    auto insertResult = m_unparkedStrings.set(parkableString->digest(), parkableString.copyRef());
    ASSERT_UNUSED(insertResult, insertResult.isNewEntry);
    
    // Release lock before scheduling aging task
    locker.unlockEarly();
    
    // Schedule aging task for new string
    scheduleAgingTaskIfNeeded();
    
    return parkableString;
}

void ParkableStringManager::remove(ParkableStringImpl* string)
{
    if (!string)
        return;
    
    // If we're not on the main thread, post to main thread
    if (!isMainThread()) {
        RunLoop::mainSingleton().dispatch([this, protectedString = RefPtr<ParkableStringImpl>(string)]() {
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
    
    const auto* digest = string->digest();
    StringMap* targetMap = nullptr;
    
    // Determine which map contains the string based on its current state
    {
        Locker stringLocker { string->m_metadata->lock };
        
        if (string->isParkedNoLock()) {
            targetMap = &m_parkedStrings;
        } else {
            targetMap = &m_unparkedStrings;
        }
    }
    
    // Remove from the determined map
    {
        Locker managerLocker { m_lock };
        auto it = targetMap->find(digest);
        if (it != targetMap->end()) {
            targetMap->remove(it);
        }
    }
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
    
    // Post to main thread with retained reference
    callOnMainThread([protectedString = RefPtr<ParkableStringImpl>(string)]() {
        ParkableStringManager::instance().onParked(protectedString.get());
    });
}

void ParkableStringManager::completeUnpark(ParkableStringImpl* string, Seconds elapsed)
{
    if (isMainThread()) {
        recordUnparkingTime(elapsed);
        recordDecompression();
        onUnparked(string);
        return;
    }
    
    // Post to main thread with retained reference
    callOnMainThread([protectedString = RefPtr<ParkableStringImpl>(string), elapsed]() {
        auto& manager = ParkableStringManager::instance();
        manager.recordUnparkingTime(elapsed);
        manager.recordDecompression();
        manager.onUnparked(protectedString.get());
    });
}


// ===== Main Thread Callbacks (Updated for Digest-Based Maps) =====

void ParkableStringManager::onParked(ParkableStringImpl* string)
{
    ASSERT(isMainThread());
    ASSERT(string->mayBeParked());
    moveString(string, &m_unparkedStrings, &m_parkedStrings);
}

void ParkableStringManager::onUnparked(ParkableStringImpl* string)
{
    ASSERT(isMainThread());
    ASSERT(string->mayBeParked());
    moveString(string, &m_parkedStrings, &m_unparkedStrings);
    scheduleAgingTaskIfNeeded();
}


// Statistics and testing methods

// Returns total count of tracked strings across all three state maps.
size_t ParkableStringManager::size() const
{
    ASSERT(isMainThread());
    Locker locker { m_lock };
    return m_unparkedStrings.size() + m_parkedStrings.size();
}

// Test helper checking if string exists in parked strings map.
bool ParkableStringManager::isOnParkedMapForTesting(ParkableStringImpl* string)
{
    if (!string || !string->digest())
        return false;
    return m_parkedStrings.contains(string->digest());
}


void ParkableStringManager::purgeMemory()
{
    ASSERT(isMainThread());
    parkAll(ParkableStringImpl::ParkingMode::CompressOnly);
}

// Test helper to clear all manager state and reset statistics.
void ParkableStringManager::resetForTesting()
{
    m_unparkedStrings.clear();
    m_parkedStrings.clear();
    
    m_totalParkingThreadTime = 0_s;
    m_totalUnparkingTime = 0_s;
    m_totalCompressions = 0;
    m_totalDecompressions = 0;
    
    m_hasPendingAgingTask = false;
    m_firstStringAgingWasDelayed = false;
    m_rendererBackgrounded = false;
    
    m_testModeEnabled = false;
}

// ===== Test Control Methods =====

// Advances aging by one cycle for testing.
void ParkableStringManager::fastForwardAgingForTesting()
{
    ASSERT(isMainThread());
    
    if (!m_testModeEnabled)
        return;
    
    // Execute one aging cycle
    ageStringsAndPark();
}

// ===== Memory Dump Provider Implementation =====

ParkableStringManager::MemoryStatistics ParkableStringManager::getMemoryStatistics() const
{
    ASSERT(isMainThread());
    Locker locker { m_lock };
    
    MemoryStatistics stats;
    stats.unparkedStrings = m_unparkedStrings.size();
    stats.parkedStrings = m_parkedStrings.size();
    stats.totalStrings = stats.unparkedStrings + stats.parkedStrings;
    
    size_t totalCompressedBytes = 0;
    size_t totalUncompressedBytes = 0;
    size_t metadataBytes = 0;
    
    // Calculate stats for both string maps
    auto calculateForMap = [&](const StringMap& map) {
        for (const auto& pair : map) {
            RefPtr string = pair.value;
            if (!string)
                continue;
                
            auto usage = string->memoryUsageForSnapshot();
            metadataBytes += usage.thisSize;
            
            // For uncompressed size, use the logical string size
            totalUncompressedBytes += string->sizeInBytes();
            
            if (string->hasCompressedData())
                totalCompressedBytes += string->compressedSize();
        }
    };
    
    calculateForMap(m_unparkedStrings);
    calculateForMap(m_parkedStrings);
    
    stats.totalUncompressedSize = totalUncompressedBytes;
    stats.totalCompressedSize = totalCompressedBytes;
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
    
    return stats;
}

size_t ParkableStringManager::memoryFootprint() const
{
    auto stats = getMemoryStatistics();
    return stats.totalUncompressedSize + stats.totalCompressedSize + stats.metadataOverhead;
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
    
    // Schedule the aging task with proper delay
    RunLoop::mainSingleton().dispatchAfter(delay, [this]() {
        ageStringsAndPark();
    });
}

// Main aging loop that processes all strings and tracks progress.
void ParkableStringManager::ageStringsAndPark()
{
    m_hasPendingAgingTask = false;
    
    if (!m_testModeEnabled && isPaused())
        return;
    
    // Get snapshots of all strings to process
    auto unparkedStrings = enumerateStrings(m_unparkedStrings);
    auto parkedStrings = enumerateStrings(m_parkedStrings);
    
    bool canMakeProgress = false;
    
    for (auto& string : unparkedStrings) {
        auto result = string->maybeParkString();
        if (result == ParkableStringImpl::AgeOrParkResult::SuccessOrTransientFailure) {
            canMakeProgress = true;
        }
    }
    
    for (auto& string : parkedStrings) {
        auto result = string->maybeParkString();
        if (result == ParkableStringImpl::AgeOrParkResult::SuccessOrTransientFailure) {
            canMakeProgress = true;
        }
    }
    
  // Some strings will never be parkable because there are lasting external
  // references to them. Don't endlessely reschedule the aging task if we are
  // not making progress (that is, no new string was either aged or parked).
  //
  // This ensures that the tasks will stop getting scheduled, assuming that
  // the renderer is otherwise idle. Note that we cannot use "idle" tasks as
  // we need to age and park strings after the renderer becomes idle, meaning
  // that this has to run when the idle tasks are not. As a consequence, it
  // is important to make sure that this will not reschedule tasks forever.
    bool reschedule = hasPendingWork() && canMakeProgress;
    if (reschedule) {
        scheduleAgingTaskIfNeeded();
    }
}

// Parks all unparked strings with the specified mode.
void ParkableStringManager::parkAll(ParkableStringImpl::ParkingMode mode)
{
    ASSERT(isMainThread());

    auto stringsToProcess = enumerateStrings(m_unparkedStrings);

    for (auto* string : stringsToProcess) {
        RefPtr stringRef = string;
        if (stringRef)
            stringRef->park(mode);
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
    ASSERT(isMainThread());
    // In test mode, never pause (allow controlled execution)
    if (m_testModeEnabled)
        return false;
    
    // For now, always allow aging to enable automatic compression demonstrations
    return false;
}


// Returns the interval between aging tasks.
Seconds ParkableStringManager::agingInterval()
{
    // Use less aggressive aging for better performance
    return kLessAggressiveAgingInterval;
}

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
