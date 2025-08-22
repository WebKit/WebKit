#include "config.h"
#include "ParkableString.h"

#if ENABLE(PARKABLE_STRINGS)

#include "ParkableStringManager.h"
#include "DiskDataAllocator.h"
#include "DiskDataMetadata.h"
#include <wtf/WorkQueue.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Assertions.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>
#include <wtf/Vector.h>
#include <JavaScriptCore/ArrayBuffer.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/FileSystem.h>
#include <wtf/UUID.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/MakeString.h>
#include <wtf/HexNumber.h>
#include <wtf/MonotonicTime.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/SystemTracing.h>

// Direct zlib for simple stateless compression/decompression
#include <zlib.h>
#if PLATFORM(COCOA)
#include <compression.h>
#endif

// WebKit's crypto API for SHA256
#include <pal/crypto/CryptoDigest.h>

namespace WebCore {

enum class ParkingAction { Parked, Unparked, Written, Read };

// Records timing statistics for parking operations.
static void recordStatistics(size_t size, Seconds duration, ParkingAction action)
{
    switch (action) {
    case ParkingAction::Parked:
        if (size > 0 && duration > 0_s) {
            double compressionRateMBps = (size / (1024.0 * 1024.0)) / duration.seconds();
            UNUSED_PARAM(compressionRateMBps);
        }
        break;
        
    case ParkingAction::Unparked:
        if (size > 0 && duration > 0_s) {
            double decompressionRateMBps = (size / (1024.0 * 1024.0)) / duration.seconds();
            UNUSED_PARAM(decompressionRateMBps);
        }
        break;
        
    case ParkingAction::Written:
        if (size > 0 && duration > 0_s) {
            double writeRateMBps = (size / (1024.0 * 1024.0)) / duration.seconds();
            UNUSED_PARAM(writeRateMBps);
        }
        break;
        
    case ParkingAction::Read:
        if (size > 0 && duration > 0_s) {
            double readRateMBps = (size / (1024.0 * 1024.0)) / duration.seconds();
            UNUSED_PARAM(readRateMBps);
        }
        break;
    }
    
    // TODO: Integrate with WebKit's histogram system
}

// Helper class to measure elapsed time
class ElapsedTimer {
public:
    ElapsedTimer() : m_startTime(MonotonicTime::now()) { }
    
    Seconds elapsed() const 
    { 
        return MonotonicTime::now() - m_startTime;
    }
    
private:
    MonotonicTime m_startTime;
};

// ===== ASAN Support =====

#if defined(ADDRESS_SANITIZER)
extern "C" void __asan_poison_memory_region(void const volatile* addr, size_t size);
extern "C" void __asan_unpoison_memory_region(void const volatile* addr, size_t size);
#endif

// Marks string memory as poisoned
static void asanPoisonString(const RefPtr<StringImpl>& stringImpl)
{
#if defined(ADDRESS_SANITIZER)
    if (stringImpl->isNull())
        return;
    if (stringImpl->isAtom())
        return;

    if (stringImpl->is8Bit()) {
        auto span = stringImpl->span8();
        __asan_poison_memory_region(span.data(), span.size() * sizeof(LChar));
    } else {
        auto span = stringImpl->span16();
        __asan_poison_memory_region(span.data(), span.size() * sizeof(char16_t));
    }
#else
    UNUSED_PARAM(stringImpl);
#endif // defined(ADDRESS_SANITIZER)
}

// Removes AddressSanitizer poisoning from string memory before access
static void asanUnpoisonString(const RefPtr<StringImpl>& stringImpl)
{
#if defined(ADDRESS_SANITIZER)
    if (stringImpl->isNull())
        return;

    if (stringImpl->is8Bit()) {
        auto span = stringImpl->span8();
        __asan_unpoison_memory_region(span.data(), span.size() * sizeof(LChar));
    } else {
        auto span = stringImpl->span16();
        __asan_unpoison_memory_region(span.data(), span.size() * sizeof(char16_t));
    }
#else
    UNUSED_PARAM(stringImpl);
#endif // defined(ADDRESS_SANITIZER)
}

// ===== Background Task Parameters =====

// Created and destroyed on the same thread, accessed on a background thread as well.
// Object lifetime is managed by the RefPtr to keep the string alive during the entire background operation.
struct BackgroundTaskParams final {
    WTF_MAKE_FAST_ALLOCATED;
    
public:
    BackgroundTaskParams(
        RefPtr<ParkableStringImpl> string,
        Vector<uint8_t> data,
        ParkableStringImpl::ParkingMode parkingMode)
        : string(WTFMove(string))
        , data(WTFMove(data))
        , parkingMode(parkingMode)
        , elapsed(0_s)
    {
    }
    BackgroundTaskParams(const BackgroundTaskParams&) = delete;
    BackgroundTaskParams& operator=(const BackgroundTaskParams&) = delete;
    ~BackgroundTaskParams() { 
        ASSERT(isMainThread()); 
    }
    
    const RefPtr<ParkableStringImpl> string;
    Vector<uint8_t> data;
    ParkableStringImpl::ParkingMode parkingMode;
    Seconds elapsed;
    
    std::unique_ptr<DiskDataMetadata> diskMetadata;
};

// ===== Hash-Based Deduplication =====

// Computes a SHA256 digest of the string content for deduplication
std::unique_ptr<ParkableStringImpl::SecureDigest> ParkableStringImpl::hashString(StringImpl* string)
{
    if (!string)
        return nullptr;

    auto cryptoDigest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);

    if (string->is8Bit()) {
        auto span = string->span8();
        cryptoDigest->addBytes(byteCast<uint8_t>(span));
    } else {
        auto span = string->span16();
        for (auto ch : span) {
            uint8_t bytes[2] = { static_cast<uint8_t>(ch & 0xFF), static_cast<uint8_t>((ch >> 8) & 0xFF) };
            cryptoDigest->addBytes(std::span<const uint8_t>(bytes, 2));
        }
    }

    // Include encoding information to differentiate 8-bit vs 16-bit strings with same byte content
    uint8_t encodingByte = string->is8Bit() ? 1 : 0;
    cryptoDigest->addBytes(std::span<const uint8_t>(&encodingByte, 1));

    Vector<uint8_t> hashResult = cryptoDigest->computeHash();

    return makeUnique<SecureDigest>(WTFMove(hashResult));
}

// ===== ParkableStringImpl Implementation =====

// Minimum string size to consider for parking (10KB)
constexpr size_t kMinimumSizeForParking = 10 * 1024;
// Minimum size for disk storage (50KB)
constexpr size_t kMinimumSizeForDisk = 50 * 1024;

// Returns singleton work queue for background compression and disk operations
static ConcurrentWorkQueue& backgroundWorkQueue()
{
    static LazyNeverDestroyed<Ref<ConcurrentWorkQueue>> workQueue;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        workQueue.construct(ConcurrentWorkQueue::create("org.webkit.ParkableString.background"_s));
    });
    return workQueue.get();
}

// Constructor for ParkableMetadata
ParkableStringImpl::ParkableMetadata::ParkableMetadata(WTF::String string, std::unique_ptr<SecureDigest> digest)
    : lock()
    , lockCount(0)
    , state(State::Unparked)
    , age(Age::Young)
    , hasCompressedData(false)
    , hasOnDiskData(false)
    , backgroundTaskInProgress(false)
    , compressionFailed(false)
    , compressedData(nullptr)
    , diskMetadata(nullptr)
    , digest(WTFMove(digest))
    , is8Bit(string.is8Bit())
    , length(string.length())
{
}

Ref<ParkableStringImpl> ParkableStringImpl::create(RefPtr<StringImpl> string)
{
    return adoptRef(*new ParkableStringImpl(WTFMove(string)));
}

Ref<ParkableStringImpl> ParkableStringImpl::makeNonParkable(RefPtr<StringImpl> string)
{
    return adoptRef(*new ParkableStringImpl(WTFMove(string), false));
}

Ref<ParkableStringImpl> ParkableStringImpl::makeParkable(RefPtr<StringImpl> string, std::unique_ptr<SecureDigest> digest)
{
    ASSERT(digest);
    return adoptRef(*new ParkableStringImpl(WTFMove(string), WTFMove(digest)));
}

ParkableStringImpl::ParkableStringImpl(RefPtr<StringImpl> string, bool parkable)
    : m_metadata(nullptr) // No metadata for non-parkable strings
{
    UNUSED_PARAM(parkable);
    // Store the string directly - query properties from StringImpl as needed
    if (string && string->refCount() > 1) {
        String copy { String(string.get()).isolatedCopy() };
        m_string = copy.impl();
    } else {
        m_string = string;
    }
}

ParkableStringImpl::ParkableStringImpl(RefPtr<StringImpl> string, std::unique_ptr<SecureDigest> digest)
    : m_metadata(digest ? makeUnique<ParkableMetadata>(String(string.get()), WTFMove(digest)) : nullptr)
{
    // Store the string directly - query properties from StringImpl or use cached metadata
    if (string && string->refCount() > 1) {
        String copy { String(string.get()).isolatedCopy() };
        m_string = copy.impl();
    } else {
        m_string = string;
    }
    
    ASSERT(!digest || m_metadata);
}

ParkableStringImpl::~ParkableStringImpl()
{
    if (!mayBeParked())
        return;
    
    // There is nothing thread-hostile in this method, but the current design should only reach this path through the main thread.
    assertOnValidThread();
    ASSERT(m_metadata->lockCount == 0);
    asanUnpoisonString(m_string);
    
    // Cannot destroy while parking is in progress, as the object is kept alive by the background task.
    ASSERT(!m_metadata->backgroundTaskInProgress);
    ASSERT(!hasOnDiskData());
}

// ===== Basic String Information =====

bool ParkableStringImpl::isNull() const
{
    if (!mayBeParked()) {
        return !m_string;
    }
    
    Locker locker { m_metadata->lock };
    return !m_string && !m_metadata->compressedData && !m_metadata->diskMetadata;
}

size_t ParkableStringImpl::sizeInBytes() const
{
    if (!mayBeParked()) {
        return m_string ? m_string->sizeInBytes() : 0;
    }

    return length() * (is8Bit() ? sizeof(LChar) : sizeof(UChar));
}

bool ParkableStringImpl::isParked() const
{
    if (!mayBeParked()) {
        return false;
    }
        
    Locker locker { m_metadata->lock };
    bool result = m_metadata->state == State::Parked;
    return result;
}

bool ParkableStringImpl::isCompressionFailed() const
{
    if (!mayBeParked())
        return false;
        
    Locker locker { m_metadata->lock };
    return m_metadata->compressionFailed;
}

bool ParkableStringImpl::isOnDisk() const
{
    if (!mayBeParked())
        return false;
        
    Locker locker { m_metadata->lock };
    return m_metadata->state == State::OnDisk;
}

ParkableStringImpl::State ParkableStringImpl::currentState() const
{
    if (!mayBeParked())
        return State::Unparked;
        
    Locker locker { m_metadata->lock };
    return m_metadata->state;
}

ParkableStringImpl::Age ParkableStringImpl::currentAge() const
{
    if (!mayBeParked())
        return Age::Young;
        
    Locker locker { m_metadata->lock };
    return m_metadata->age;
}

bool ParkableStringImpl::hasCompressedData() const
{
    if (!mayBeParked())
        return false;
        
    Locker locker { m_metadata->lock };
    return m_metadata->hasCompressedData;
}

// Used in internal functions where the lock is already acquired
bool ParkableStringImpl::hasCompressedDataNoLock() const
{
    if (!mayBeParked())
        return false;
    return m_metadata->hasCompressedData;
}

size_t ParkableStringImpl::compressedSize() const
{
    if (!mayBeParked())
        return 0;
        
    Locker locker { m_metadata->lock };
    return m_metadata->compressedData ? m_metadata->compressedData->size() : 0;
}

bool ParkableStringImpl::hasOnDiskData() const
{
    if (!mayBeParked())
        return false;
        
    Locker locker { m_metadata->lock };
    return m_metadata->hasOnDiskData;
}

size_t ParkableStringImpl::onDiskSize() const
{
    if (!mayBeParked())
        return 0;
        
    Locker locker { m_metadata->lock };
    return m_metadata->diskMetadata ? m_metadata->diskMetadata->size() : 0;
}

const String& ParkableStringImpl::diskPath() const
{
    static NeverDestroyed<String> centralizedPath("DiskDataAllocator"_s);
    return centralizedPath.get();
}

// ===== String Access =====
// Returns a String object containing the string content
String ParkableStringImpl::toString()
{
    if (!mayBeParked()) {
        return m_string ? String(m_string.get()) : String();
    }
    
    Locker locker { m_metadata->lock };
    makeYoung();
    asanUnpoisonString(m_string);
    unpark();
    return m_string ? String(m_string.get()) : String();
}

RefPtr<StringImpl> ParkableStringImpl::impl()
{
    String str = toString();
    return str.impl();
}

void ParkableStringImpl::lock()
{
    if (!mayBeParked())
        return;
        
    Locker locker { m_metadata->lock };
    ++m_metadata->lockCount;
    makeYoung();
    
    // External code may have a hard reference to the underlying StringImpl via String::impl() for the sake of thread-safety
    // Unpoison the string so that the external code can safely access the string
    asanUnpoisonString(m_string);
}

void ParkableStringImpl::unlock()
{
    if (!mayBeParked())
        return;
        
    Locker locker { m_metadata->lock };
    ASSERT(m_metadata->lockCount > 0);
    --m_metadata->lockCount;
    
#if defined(ADDRESS_SANITIZER) && ASSERT_ENABLED
    // There are no external references to the data, nobody should touch the data
    // Note: Only poison the memory if this is on the owning thread, as this is otherwise racy
    // Indeed |unlock()| may be called on any thread, and the owning thread may concurrently call |toString()|
    // It is then allowed to use the string until the end of the current owning thread task
    // Checking the owning thread first as |currentStatus()| can only be called from the owning thread
    if (isMainThread() && currentStatus() == Status::UnreferencedExternally) {
        asanPoisonString(m_string);
    }
#endif // defined(ADDRESS_SANITIZER) && ASSERT_ENABLED
}

void ParkableStringImpl::lockWithoutMakingYoung()
{
    if (!mayBeParked())
        return;
        
    Locker locker { m_metadata->lock };
    ++m_metadata->lockCount;
}

// Returns true if all conditions for parking are met
bool ParkableStringImpl::canParkNow() const
{
    return currentStatus() == Status::UnreferencedExternally 
        && m_metadata->age != Age::Young 
        && !m_metadata->compressionFailed;
}

// Determines the current reference status for parking eligibility.
ParkableStringImpl::Status ParkableStringImpl::currentStatus() const
{
    ASSERT(isMainThread());
    ASSERT(mayBeParked());

    if (m_metadata->lockCount != 0)
        return Status::Locked;
    
    // Can be null if it is compressed or on disk.
    if (!m_string)
        return Status::UnreferencedExternally;
        
    if (!m_string->hasOneRef())
        return Status::TooManyReferences;
        
    return Status::UnreferencedExternally;
}

// Discards the uncompressed StringImpl while keeping compressed data.
void ParkableStringImpl::discardUncompressedData()
{
    // Only discard uncompressed data, keep compressed data for performance
    asanUnpoisonString(m_string);
    m_string = nullptr;
    m_metadata->state = State::Parked;

    ParkableStringManager::instance().completeParked(this);
}

// Discards compressed data after it has been written to disk.
void ParkableStringImpl::discardCompressedData()
{
    m_metadata->compressedData = nullptr;
    m_metadata->hasCompressedData = false;
    m_metadata->state = State::OnDisk;
}

// Unparks the string by restoring uncompressed StringImpl data
void ParkableStringImpl::unpark()
{
    WTFBeginSignpost(this, ParkableStringUnpark, "size=%zu state=%d", sizeInBytes(), static_cast<int>(m_metadata->state));
    
    if (m_metadata->state == State::Unparked) {
        WTFEndSignpost(this, ParkableStringUnpark);
        return;
    }
    
    String result = unparkInternal();
    
    if (!result.isNull()) {
        m_string = result.impl();
    }
    
    WTFEndSignpost(this, ParkableStringUnpark, "unparked_size=%u", result.isNull() ? 0 : result.sizeInBytes());
}

bool ParkableStringImpl::park(ParkingMode mode)
{
    if (!mayBeParked()) {
        return false;
    }
    
    Locker locker { m_metadata->lock };
    
    assertOnValidThread();
    
    if (m_metadata->state == State::Parked) {
        return true;
    }
    
    // Making the string old to cancel parking if it is accessed/locked before parking is complete
    m_metadata->age = Age::Old;
    
    if (!canParkNow()) {
        return false;
    }
    
    return parkInternal(mode);
}

// Attempts to park the string based on automatic aging policies
ParkableStringImpl::AgeOrParkResult ParkableStringImpl::maybeParkString()
{
    if (!mayBeParked())
        return AgeOrParkResult::NonTransientFailure;
    
    Locker locker { m_metadata->lock };
    
    assertOnValidThread();
    
    if (m_metadata->backgroundTaskInProgress)
        return AgeOrParkResult::SuccessOrTransientFailure;
    
    // Handle already-parked strings
    if (isParkedNoLock()) {
        if (m_metadata->age == Age::VeryOld) {
            bool ok = parkInternal(ParkingMode::WriteToDisk);
            if (!ok)
                return AgeOrParkResult::NonTransientFailure;
        } else {
            ageString();
        }
        return AgeOrParkResult::SuccessOrTransientFailure;
    }
    
    // Handle unparked strings
    Status status = currentStatus();
    Age age = m_metadata->age;
    
    if (age == Age::Young) {
        if (status == Status::UnreferencedExternally)
            ageString();
    } else if (m_metadata->age == Age::Old || m_metadata->age == Age::VeryOld) {
        if (!canParkNow()) {
            return AgeOrParkResult::NonTransientFailure;
        }
        
        ParkingMode mode;
        if (m_metadata->age == Age::VeryOld && sizeInBytes() >= kMinimumSizeForDisk) {
            mode = m_metadata->hasCompressedData ? ParkingMode::WriteToDisk : ParkingMode::CompressThenWriteToDisk;
        } else {
            mode = ParkingMode::CompressOnly;
        }
        
        bool ok = parkInternal(mode);
        if (!ok)
            return AgeOrParkResult::NonTransientFailure;
            
        return AgeOrParkResult::SuccessOrTransientFailure;
    }
    
    // External references to a string can be long-lived, cannot provide a progress guarantee for this string
    return status == Status::TooManyReferences
        ? AgeOrParkResult::NonTransientFailure
        : AgeOrParkResult::SuccessOrTransientFailure;
}

// Internal parking implementation that handles different parking modes
bool ParkableStringImpl::parkInternal(ParkingMode mode)
{
    ASSERT(m_metadata->state == State::Unparked || m_metadata->state == State::Parked);
    ASSERT(m_metadata->age != Age::Young);
    ASSERT(canParkNow());
    
    if (m_metadata->backgroundTaskInProgress) {
        return true;
    }
    
    switch (mode) {
    case ParkingMode::SynchronousOnly:
        if (m_metadata->hasCompressedData) {
            discardUncompressedData();
        } else {
            return false;
        }
        break;
        
    case ParkingMode::CompressOnly:
        if (m_metadata->hasCompressedData) {
            discardUncompressedData(); 
        } else {
            scheduleCompressionTask(mode);
        }
        break;
        
    case ParkingMode::WriteToDisk:
        if (m_metadata->hasOnDiskData) {
            discardCompressedData();
        } else {
            if (!m_metadata->hasCompressedData) {
                return false;
            }
            
            discardUncompressedData();
            
            ParkableStringManager::instance().notifyCandidatesAvailable(1, compressedSize(), [](void) {
            });
        }
        break;
        
    case ParkingMode::CompressThenWriteToDisk:
        if (m_metadata->hasOnDiskData) {
            discardUncompressedData();
            discardCompressedData();
            ASSERT(m_metadata->state == State::OnDisk);
        } else if (m_metadata->hasCompressedData) {
            discardUncompressedData();
            return parkInternal(ParkingMode::WriteToDisk);
        } else {
            scheduleCompressionTask(mode);
        }
        break;
    }
    
    return true;
}

// Internal unparking implementation that routes to appropriate unpark method
String ParkableStringImpl::unparkInternal()
{
    if (m_metadata->state == State::OnDisk) {
        return unparkFromDisk();
    }
    
    if (m_metadata->state == State::Parked) {
        return unparkFromCompressed();
    }
    
    return String();
}

// Unparks a string from compressed data stored in memory.
String ParkableStringImpl::unparkFromCompressed()
{
    if (m_metadata->state != State::Parked) {
        return String();
    }

    if (!m_metadata->compressedData) {
        return String();
    }

    ElapsedTimer timer;

    auto result = decompressData(*m_metadata->compressedData, m_metadata->is8Bit, m_metadata->length);

    if (!result.has_value()) {
        return String();
    }

    String decompressedString = result.value();
    Seconds elapsed = timer.elapsed();

    recordStatistics(decompressedString.sizeInBytes(), elapsed, ParkingAction::Unparked);

    // Change state to unparked, but keep compressed data for fast re-parking
    m_metadata->state = State::Unparked;

    ParkableStringManager::instance().completeUnpark(this, elapsed);


    asanUnpoisonString(m_string);
    return decompressedString;
}

// Unparks a string from disk storage using NetworkProcess-controlled read-only access
String ParkableStringImpl::unparkFromDisk()
{
    if (!m_metadata->hasOnDiskData)
        return String();
    
    ElapsedTimer diskTimer;
    
    String digest = digestString();
    if (digest.isEmpty()) {
        return String();
    }
    
    auto& manager = ParkableStringManager::instance();
    auto compressedDataOpt = manager.readCompressedDataFromDisk(digest);
    
    if (!compressedDataOpt) {
        m_metadata->hasOnDiskData = false;
        return String();
    }
    
    Seconds diskElapsed = diskTimer.elapsed();
    
    auto compressedData = makeUnique<Vector<uint8_t>>(WTFMove(*compressedDataOpt));
    m_metadata->compressedData = WTFMove(compressedData);
    
    ElapsedTimer decompressTimer;
    
    auto result = decompressData(*m_metadata->compressedData, m_metadata->is8Bit, m_metadata->length);
    
    if (!result.has_value()) {
        m_metadata->compressedData = nullptr;
        m_metadata->hasCompressedData = false;
        return String();
    }
    
    String decompressedString = result.value();
    Seconds decompressElapsed = decompressTimer.elapsed();
    
    m_metadata->state = State::Unparked;
    m_metadata->hasCompressedData = true;
    
    ParkableStringManager::instance().completeUnpark(this, decompressElapsed, diskElapsed);
    
    asanUnpoisonString(m_string);
    
    return decompressedString;
}

// Schedules background compression with parameter setup and callback routing
void ParkableStringImpl::scheduleCompressionTask(ParkingMode mode)
{
    ASSERT(!m_metadata->backgroundTaskInProgress);
    
    // |string_|'s data should not be touched except in the compression task
    asanPoisonString(m_string);
    m_metadata->backgroundTaskInProgress = true;
    
    // Get data for compression
    Vector<uint8_t> data;
    if (m_string) {
        if (m_string->is8Bit()) {
            auto chars = m_string->span8();
            auto bytes = byteCast<uint8_t>(chars);
            data.append(bytes);
        } else {
            auto chars = m_string->span16();
            data.reserveInitialCapacity(chars.size() * sizeof(UChar));
            const uint8_t* byteData = reinterpret_cast<const uint8_t*>(chars.data());
            data.append(unsafeMakeSpan(byteData, chars.size() * sizeof(UChar)));
        }
    }
    
    auto params = makeUnique<BackgroundTaskParams>(
        RefPtr<ParkableStringImpl>(this),
        WTFMove(data),
        mode
    );
    
    backgroundWorkQueue().dispatch([params = WTFMove(params)]() mutable {
        compressInBackground(WTFMove(params));
    });
}



// ===== Background Static Methods =====

// Performs actual compression in background thread
void ParkableStringImpl::compressInBackground(std::unique_ptr<BackgroundTaskParams> params)
{
    ElapsedTimer timer;

    RefPtr stringPtr = params->string;

    #if ASAN_ENABLED
        // Lock the string to prevent a concurrent |unlock()| on the owning thread from
        // poisoning the string in the meantime.
        //
        // Don't make the string young at the same time, otherwise parking would
        // always be cancelled on the owning thread with address sanitizer, since the
        // |onCompressionCompleteOnMainThread()| callback would be executed on a young
        // string.
        stringPtr->lockWithoutMakingYoung();
    #endif

    // Compression touches the string.
    asanUnpoisonString(stringPtr->m_string);

    std::unique_ptr<Vector<uint8_t>> compressedData = nullptr;

    if (!params->data.empty()) {
        compressedData = compressData(params->data);
    }

    #if ASAN_ENABLED
        stringPtr->unlock();
    #endif

    Seconds elapsed = timer.elapsed();
    recordStatistics(params->data.size(), elapsed, ParkingAction::Parked);
    
    // Complete compression on main thread.
    callOnMainThread([parkableString = params->string, params = WTFMove(params), compressedData = WTFMove(compressedData)]() mutable {
        parkableString->onCompressionCompleteOnMainThread(WTFMove(params), WTFMove(compressedData));
    });
}



// ===== Background Completion Callbacks =====

// Handles completion of background compression
void ParkableStringImpl::onCompressionCompleteOnMainThread(std::unique_ptr<BackgroundTaskParams> params, std::unique_ptr<WTF::Vector<uint8_t>> compressedData)
{
    if (!mayBeParked())
        return;
    
    Locker locker { m_metadata->lock };
    
    ASSERT(m_metadata->backgroundTaskInProgress);
    ASSERT(m_metadata->state == State::Unparked);
    
    m_metadata->backgroundTaskInProgress = false;
            
    // Always keep the compressed data. Compression is expensive, so even if the
    // uncompressed representation cannot be discarded now, avoid compressing
    // multiple times. This will allow synchronous parking next time.
    ASSERT(!m_metadata->compressedData);
    if (compressedData) {
        m_metadata->compressedData = WTFMove(compressedData);
        m_metadata->hasCompressedData = true;
        m_metadata->compressionFailed = false;
        ParkableStringManager::instance().recordCompression();
    } else {
        m_metadata->compressionFailed = true;
    }
    
    // Between |park()| and now, things may have happened:
    // 1. |toString()| or
    // 2. |lock()| may have been called.
    //
    // Both of these will make the string young again, and if so we don't
    // discard the compressed representation yet.
    if (this->canParkNow() && m_metadata->hasCompressedData) {
        discardUncompressedData();
        params->data = {};
    } else {
        // Cancel parking but keep compressed data for next time
        m_metadata->state = State::Unparked;
    }
    
    // Check if we need to continue to disk
    if (params->parkingMode == ParkingMode::CompressThenWriteToDisk && isParkedNoLock()) {
        if (m_metadata->hasCompressedData) {
            ParkableStringManager::instance().notifyCandidatesAvailable(1, compressedSize(), [](void) {
            });
        }
    }
}



// ===== NetworkProcess-Controlled State Transitions =====

void ParkableStringImpl::transitionToOnDiskWithoutMetadata()
{
    if (!mayBeParked())
        return;
    
    Locker locker { m_metadata->lock };
    
    ASSERT(m_metadata->state == State::Parked);
    ASSERT(m_metadata->hasCompressedData);
    
    m_metadata->hasOnDiskData = true;
    m_metadata->diskMetadata = nullptr;
    
    if (m_string) {
        discardUncompressedData();
    }
    
    discardCompressedData();
    
    ASSERT(m_metadata->state == State::OnDisk);
}

// ===== Compression Operations =====

#if PLATFORM(COCOA)
std::unique_ptr<Vector<uint8_t>> ParkableStringImpl::compressDataWithAppleCompression(std::span<const uint8_t> data)
{
    if (data.empty()) {
        return nullptr;
    }

    // Allocate destination buffer with capacity equal to source size
    auto outputData = makeUnique<Vector<uint8_t>>();
    outputData->resize(data.size());

    auto startTime = MonotonicTime::now();
    size_t compressedSize = compression_encode_buffer(
        outputData->mutableSpan().data(), outputData->size(),
        data.data(), data.size(),
        nullptr, COMPRESSION_LZFSE);
    auto endTime = MonotonicTime::now();
    auto compressionTime = endTime - startTime;

    outputData->resize(compressedSize);

    // Check compression effectiveness (at least 20% reduction)
    if (outputData->size() >= data.size() * 0.8) {
        return nullptr;
    }

    double compressionRatio = (100.0 * outputData->size()) / data.size();
    UNUSED_PARAM(compressionTime);
    UNUSED_PARAM(compressionRatio);

    return outputData;
}
#endif

std::unique_ptr<Vector<uint8_t>> ParkableStringImpl::compressDataWithZlib(std::span<const uint8_t> data)
{
    if (data.empty()) {
        return nullptr;
    }

    auto startTime = MonotonicTime::now();

    z_stream stream = { };

    // Initialize compression
    int result = deflateInit2(&stream, 5, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
    if (result != Z_OK) {
        return nullptr;
    }

    // Prepare input
    stream.next_in = const_cast<Bytef*>(data.data());
    stream.avail_in = data.size();

    // Start with output buffer sized for good compression
    size_t outputCapacity = std::max(data.size(), static_cast<size_t>(16384));
    auto outputData = makeUnique<Vector<uint8_t>>();
    outputData->reserveInitialCapacity(outputCapacity);

    int deflateResult;
    do {
        // Ensure we have output space
        size_t currentSize = outputData->size();
        outputData->resize(currentSize + 16384); // 16KB chunks

        stream.next_out = &(*outputData)[currentSize];
        stream.avail_out = 16384;

        deflateResult = deflate(&stream, Z_FINISH);

        if (deflateResult != Z_OK && deflateResult != Z_STREAM_END) {
            deflateEnd(&stream);
            return nullptr;
        }

        // Adjust result size to actual compressed data
        size_t compressedBytes = 16384 - stream.avail_out;
        outputData->resize(currentSize + compressedBytes);

    } while (deflateResult != Z_STREAM_END && stream.avail_out == 0);

    deflateEnd(&stream);

    auto endTime = MonotonicTime::now();
    auto compressionTime = endTime - startTime;

    // Check compression effectiveness (at least 20% reduction)
    if (outputData->size() >= data.size() * 0.8) {
        return nullptr;
    }

    double compressionRatio = (100.0 * outputData->size()) / data.size();
    UNUSED_PARAM(compressionTime);
    UNUSED_PARAM(compressionRatio);

    return outputData;
}

std::unique_ptr<Vector<uint8_t>> ParkableStringImpl::compressData(std::span<const uint8_t> data)
{
#if PLATFORM(COCOA)
    auto result = compressDataWithAppleCompression(data);
    if (result) {
        return result;
    }
#endif

    // Single zlib path for both COCOA fallback and non-COCOA primary compression
    auto zlibResult = compressDataWithZlib(data);
    if (zlibResult) {
        return zlibResult;
    } else {
        return nullptr;
    }
}

#if PLATFORM(COCOA)
std::optional<String> ParkableStringImpl::decompressDataWithAppleCompression(const Vector<uint8_t>& compressedData, bool is8Bit, unsigned length)
{
    if (compressedData.isEmpty())
        return std::nullopt;

    size_t expectedBytes = length * (is8Bit ? sizeof(LChar) : sizeof(UChar));

    Vector<uint8_t> decompressedBytes;
    decompressedBytes.resize(expectedBytes);

    size_t decompressedSize = compression_decode_buffer(
        decompressedBytes.mutableSpan().data(), decompressedBytes.size(),
        compressedData.span().data(), compressedData.size(),
        nullptr, COMPRESSION_LZFSE);

    if (decompressedSize == 0 || decompressedSize != expectedBytes) {
        return std::nullopt;
    }

    auto decompressedSpan = decompressedBytes.span();

    // Create string according to original format
    if (is8Bit) {
        auto stringImpl = StringImpl::create(byteCast<LChar>(decompressedSpan));
        return String(WTFMove(stringImpl));
    } else {
        // Verify even number of bytes for UChar
        if (decompressedSpan.size() % sizeof(UChar) != 0)
            return std::nullopt;

        // Convert bytes to UChar using safe buffer operations
        Vector<UChar> ucharVector;
        ucharVector.reserveInitialCapacity(length);
        for (size_t i = 0; i < length; ++i) {
            size_t byteIndex = i * sizeof(UChar);
            UChar ch = static_cast<UChar>(decompressedSpan[byteIndex]) | 
                      (static_cast<UChar>(decompressedSpan[byteIndex + 1]) << 8);
            ucharVector.append(ch);
        }

        auto stringImpl = StringImpl::create(ucharVector.span());
        return String(WTFMove(stringImpl));
    }
}
#endif

std::optional<String> ParkableStringImpl::decompressDataWithZlib(const Vector<uint8_t>& compressedData, bool is8Bit, unsigned length)
{
    if (compressedData.isEmpty())
        return std::nullopt;

    z_stream stream = { };

    // Initialize decompression
    int result = inflateInit2(&stream, 15);
    if (result != Z_OK)
        return std::nullopt;

    // Prepare input and output
    stream.next_in = const_cast<Bytef*>(compressedData.span().data());
    stream.avail_in = compressedData.size();

    size_t expectedBytes = length * (is8Bit ? sizeof(LChar) : sizeof(UChar));
    Vector<uint8_t> decompressedBytes;
    decompressedBytes.reserveInitialCapacity(expectedBytes);

    int inflateResult;
    do {
        size_t currentSize = decompressedBytes.size();
        decompressedBytes.resize(currentSize + 16384); // 16KB chunks

        stream.next_out = &decompressedBytes[currentSize];
        stream.avail_out = 16384;

        inflateResult = inflate(&stream, Z_NO_FLUSH);

        if (inflateResult != Z_OK && inflateResult != Z_STREAM_END) {
            inflateEnd(&stream);
            return std::nullopt;
        }
                // Adjust result size to actual decompressed data
        size_t decompressedChunkBytes = 16384 - stream.avail_out;
        decompressedBytes.resize(currentSize + decompressedChunkBytes);

    } while (inflateResult != Z_STREAM_END && stream.avail_out == 0);

    inflateEnd(&stream);

    // Verify we got the expected amount of data
    if (decompressedBytes.size() != expectedBytes) {
        return std::nullopt;
    }

    auto decompressedSpan = decompressedBytes.span();

    // Create string according to original format
    if (is8Bit) {
    auto stringImpl = StringImpl::create(byteCast<LChar>(decompressedSpan));
    return String(WTFMove(stringImpl));
    } else {
        // Verify even number of bytes for UChar
        if (decompressedSpan.size() % sizeof(UChar) != 0)
            return std::nullopt;

        // Convert bytes to UChar using safe buffer operations
        Vector<UChar> ucharVector;
        ucharVector.reserveInitialCapacity(length);

        for (size_t i = 0; i < length; ++i) {
            size_t byteIndex = i * sizeof(UChar);
            UChar ch = static_cast<UChar>(decompressedSpan[byteIndex]) | 
                      (static_cast<UChar>(decompressedSpan[byteIndex + 1]) << 8);
            ucharVector.append(ch);
        }

        auto stringImpl = StringImpl::create(ucharVector.span());
        return String(WTFMove(stringImpl));
    }
}

std::optional<String> ParkableStringImpl::decompressData(const Vector<uint8_t>& compressedData, bool is8Bit, unsigned length)
{
#if PLATFORM(COCOA)
    // Try Apple Compression first on Apple platforms for better performance
    auto result = decompressDataWithAppleCompression(compressedData, is8Bit, length);
    if (result)
        return result;

    // Fallback to zlib if Apple Compression fails
    return decompressDataWithZlib(compressedData, is8Bit, length);
#else
    return decompressDataWithZlib(compressedData, is8Bit, length);
#endif
}

// Resets string age to Young state for recently accessed strings.
void ParkableStringImpl::makeYoung()
{
    if (mayBeParked())
        m_metadata->age = Age::Young;
}

// Advances string through age progression from Young to Old to VeryOld.
void ParkableStringImpl::ageString()
{
    if (!mayBeParked())
        return;
    
    switch (m_metadata->age) {
    case Age::Young:
        m_metadata->age = Age::Old;
        break;
    case Age::Old:
        m_metadata->age = Age::VeryOld;
        break;
    case Age::VeryOld:
        break;
    }
}

// ===== Memory Profiling Support =====

namespace {
void recordStringImplMemoryUsage(ParkableStringImpl::MemoryUsage* result, const RefPtr<StringImpl>& stringImpl)
{
    if (stringImpl) {
        result->stringImpl = stringImpl.get();
        result->stringImplSize = sizeof(StringImpl) + stringImpl->sizeInBytes();
    }
}
} // namespace

ParkableStringImpl::MemoryUsage ParkableStringImpl::memoryUsageForSnapshot() const
{
    assertOnValidThread();
    MemoryUsage result = {0, nullptr, 0};
    
    // Base size of ParkableStringImpl
    result.thisSize = sizeof(ParkableStringImpl);
    
    if (!mayBeParked()) {
        // Non-parkable string: just include StringImpl
        recordStringImplMemoryUsage(&result, m_string);
        return result;
    }
    
    // Parkable string: add metadata overhead
    result.thisSize += sizeof(ParkableMetadata);
    
    Locker locker { m_metadata->lock };
    
    // Include StringImpl if NOT parked AND NOT on disk
    if (!isParkedNoLock() && !isOnDiskNoLock()) {
        recordStringImplMemoryUsage(&result, m_string);
    }
    
    // Trust the compressed data pointer directly
    if (m_metadata->compressedData) {
        result.thisSize += m_metadata->compressedData->size();
    }
    
    return result;
}

size_t ParkableStringImpl::memoryFootprintForDump() const
{
    MemoryUsage usage = memoryUsageForSnapshot();
    return usage.thisSize + usage.stringImplSize;
}

// ===== ParkableString Implementation =====

ParkableString::ParkableString() = default;

ParkableString::ParkableString(RefPtr<StringImpl> string)
{
    if (!string) {
        m_impl = nullptr;
        return;
    }
    
    bool isParkable = ParkableStringManager::shouldPark(*string);
    
    if (isParkable) {
        m_impl = ParkableStringManager::instance().add(WTFMove(string));
    } else {
        m_impl = ParkableStringImpl::makeNonParkable(WTFMove(string));
    }
}

ParkableString::ParkableString(const ParkableString& other) = default;
ParkableString& ParkableString::operator=(const ParkableString& other) = default;
ParkableString::ParkableString(ParkableString&& other) = default;
ParkableString& ParkableString::operator=(ParkableString&& other) = default;
ParkableString::~ParkableString() = default;

bool ParkableString::isNull() const
{
    return !m_impl || m_impl->isNull();
}

size_t ParkableString::length() const
{
    return m_impl ? m_impl->length() : 0;
}

bool ParkableString::is8Bit() const
{
    return !m_impl || m_impl->is8Bit();
}

size_t ParkableString::sizeInBytes() const
{
    return m_impl ? m_impl->sizeInBytes() : 0;
}

bool ParkableString::mayBeParked() const
{
    return m_impl && m_impl->mayBeParked();
}

bool ParkableString::isParked() const
{
    return m_impl && m_impl->isParked();
}

bool ParkableString::isOnDisk() const
{
    return m_impl && m_impl->isOnDisk();
}

size_t ParkableString::compressedSize() const
{
    return m_impl ? m_impl->compressedSize() : 0;
}

size_t ParkableString::onDiskSize() const
{
    return m_impl ? m_impl->onDiskSize() : 0;
}

String ParkableString::toString() const
{
    return m_impl ? m_impl->toString() : String();
}

RefPtr<StringImpl> ParkableString::impl() const
{
    return m_impl ? m_impl->impl() : nullptr;
}

ParkableStringImpl* ParkableString::Impl() const
{
    return m_impl.get();
}

void ParkableString::lock()
{
    if (m_impl)
        m_impl->lock();
}

void ParkableString::unlock()
{
    if (m_impl)
        m_impl->unlock();
}

bool ParkableString::park(ParkableStringImpl::ParkingMode mode)
{
    return m_impl && m_impl->park(mode);
}

size_t ParkableString::memoryFootprintForDump() const
{
    return m_impl ? m_impl->memoryFootprintForDump() : 0;
}

// Returns parking state without acquiring the string's lock.
bool ParkableStringImpl::isParkedNoLock() const
{
    if (!mayBeParked())
        return false;
    return m_metadata->state == State::Parked;
}

// Returns disk state without acquiring the string's lock.
bool ParkableStringImpl::isOnDiskNoLock() const
{
    if (!mayBeParked())
        return false;
    return m_metadata->state == State::OnDisk;
}

String ParkableStringImpl::digestString() const
{
    const auto* digestPtr = digest();
    if (!digestPtr)
        return emptyString();
    
    StringBuilder builder;
    builder.reserveCapacity(digestPtr->size() * 2);
    
    for (uint8_t byte : *digestPtr) {
        builder.append(hex(byte, 2, Lowercase));
    }
    
    return builder.toString();
}

std::optional<Vector<uint8_t>> ParkableStringImpl::compressedData() const
{
    if (!mayBeParked())
        return std::nullopt;
        
    WTF::Locker locker { m_metadata->lock };
    return compressedDataNoLock();
}

std::optional<Vector<uint8_t>> ParkableStringImpl::compressedDataNoLock() const
{
    if (!m_metadata->hasCompressedData || !m_metadata->compressedData)
        return std::nullopt;
        
    return *m_metadata->compressedData;
}

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
