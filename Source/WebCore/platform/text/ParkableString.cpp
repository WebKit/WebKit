#include "config.h"
#include "ParkableString.h"

#if ENABLE(PARKABLE_STRINGS)

#include "ParkableStringManager.h"
#include <wtf/WorkQueue.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Assertions.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>
#include <wtf/Vector.h>
#include <wtf/MainThread.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/MakeString.h>
#include <wtf/HexNumber.h>
#include <wtf/MonotonicTime.h>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>

// Direct zlib compression/decompression
#include <zlib.h>
#if PLATFORM(COCOA)
#include <compression.h>
#endif

// WebKit's crypto API for SHA256
#include <pal/crypto/CryptoDigest.h>

namespace WebCore {

// ===== Utilities =====

enum class ParkingAction { Parked, Unparked};

// Records timing statistics for parking operations.
static void recordStatistics(size_t size, Seconds duration, ParkingAction action)
{
    switch (action) {
    case ParkingAction::Parked:
        if (size > 0 && duration > 0_s) {
            double compressionRateMBps = (size / (1024.0 * 1024.0)) / duration.seconds();
            double microseconds = duration.microseconds();
            UNUSED_PARAM(compressionRateMBps);
            UNUSED_PARAM(microseconds);
        }
        break;
        
    case ParkingAction::Unparked:
        if (size > 0 && duration > 0_s) {
            double decompressionRateMBps = (size / (1024.0 * 1024.0)) / duration.seconds();
            double microseconds = duration.microseconds();
            UNUSED_PARAM(decompressionRateMBps);
            UNUSED_PARAM(microseconds);
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

#if ASAN_ENABLED
extern "C" void __asan_poison_memory_region(void const volatile* addr, size_t size);
extern "C" void __asan_unpoison_memory_region(void const volatile* addr, size_t size);
#endif

// Marks string memory as poisoned
static void asanPoisonString(const String& string)
{
#if ASAN_ENABLED
    if (string.isNull())
        return;
    // Since string is not deallocated, it remains in the AtomicStringTable,
    // where its content can be accessed for equality comparison for instance,
    // triggering a poisoned memory access.
    if (string.impl() && string.impl()->isAtom())
        return;
        
    if (string.is8Bit()) {
        auto span = string.span8();
        __asan_poison_memory_region(span.data(), span.size() * sizeof(LChar));
    } else {
        auto span = string.span16();
        __asan_poison_memory_region(span.data(), span.size() * sizeof(char16_t));
    }
#else
    UNUSED_PARAM(string);
#endif // ASAN_ENABLED
}

// Removes AddressSanitizer poisoning from string memory before access
static void asanUnpoisonString(const String& string)
{
#if ASAN_ENABLED
    if (string.isNull())
        return;
        
    if (string.is8Bit()) {
        auto span = string.span8();
        __asan_unpoison_memory_region(span.data(), span.size() * sizeof(LChar));
    } else {
        auto span = string.span16();
        __asan_unpoison_memory_region(span.data(), span.size() * sizeof(char16_t));
    }
#else
    UNUSED_PARAM(string);
#endif // ASAN_ENABLED
}

// ===== Background Task Parameters =====

// Created and destroyed on the same thread, accessed on a background thread as well.
// Object lifetime is managed by the RefPtr to keep the string alive during the entire background operation.
struct BackgroundTaskParams final {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(BackgroundTaskParams);
public:
    BackgroundTaskParams(
        RefPtr<ParkableStringImpl> string,
        std::span<const uint8_t> data, 
        ParkableStringImpl::ParkingMode parkingMode,
        Ref<RunLoop> callbackRunLoop)
        : string(WTFMove(string))
        , data(data) 
        , parkingMode(parkingMode)
        , callbackRunLoop(WTFMove(callbackRunLoop)) {}
    
    ~BackgroundTaskParams() {}
    
    const RefPtr<ParkableStringImpl> string;
    std::span<const uint8_t> data;
    ParkableStringImpl::ParkingMode parkingMode;
    const Ref<RunLoop> callbackRunLoop;
};

// ===== Hash-Based Deduplication =====

// Computes a SHA256 digest of the string content for deduplication.
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
    
// ParkableMetadata constructor
ParkableStringImpl::ParkableMetadata::ParkableMetadata(WTF::String string, std::unique_ptr<SecureDigest> digest)
    : lock()
    , lockCount(0)
    , state(State::Unparked)
    , age(Age::Young)
    , backgroundTaskInProgress(false)
    , compressionFailed(false)
    , compressedData(nullptr)
    , digest(WTFMove(digest))
    , is8Bit(string.is8Bit())
    , length(string.length())
{
}

// Single constructor with direct move semantics and debug thread tracking
ParkableStringImpl::ParkableStringImpl(RefPtr<StringImpl>&& string, std::unique_ptr<SecureDigest> digest)
    : m_string(WTFMove(string))
    , m_metadata(digest ? makeUnique<ParkableMetadata>(m_string, WTFMove(digest)) : nullptr)
#if ASSERT_ENABLED
    , m_owningThreadUID(WTF::Thread::currentSingleton().uid())
#endif
{
    ASSERT(!m_string.isNull());
}

// Creates a non-parkable ParkableStringImpl
Ref<ParkableStringImpl> ParkableStringImpl::makeNonParkable(RefPtr<StringImpl> string)
{
    return adoptRef(*new ParkableStringImpl(WTFMove(string), nullptr));
}

// Creates a parkable ParkableStringImpl with pre-computed digest
Ref<ParkableStringImpl> ParkableStringImpl::makeParkable(RefPtr<StringImpl> string, std::unique_ptr<SecureDigest> digest)
{
    ASSERT(digest);
    return adoptRef(*new ParkableStringImpl(WTFMove(string), WTFMove(digest)));
}

// Destructor for ParkableStringImpl
ParkableStringImpl::~ParkableStringImpl()
{
    if (!mayBeParked())
        return;
    // There is nothing thread-hostile in this method, but the current design should only reach this path through the main thread
    assertOnValidThread();
    ASSERT(lockDepthForTesting() == 0);
    asanUnpoisonString(m_string);
    // Cannot destroy while parking is in progress, as the object is kept alive by the background task
    ASSERT(!m_metadata->backgroundTaskInProgress);
    ASSERT(!m_metadata->compressedData);
}

// ===== Basic String Information =====

size_t ParkableStringImpl::sizeInBytes() const
{
    if (!mayBeParked()) {
        RefPtr stringImpl = m_string.impl();
        return stringImpl ? stringImpl->sizeInBytes() : 0;
    }

    return length() * (is8Bit() ? sizeof(LChar) : sizeof(UChar));
}

// ===== State Management =====

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
    return !!m_metadata->compressedData;
}

bool ParkableStringImpl::hasCompressedDataNoLock() const
{
    // Lock must already be held by caller
    if (!mayBeParked())
        return false;
    return !!m_metadata->compressedData;
}

size_t ParkableStringImpl::compressedSize() const
{
    if (!mayBeParked())
        return 0;
        
    Locker locker { m_metadata->lock };
    return m_metadata->compressedData ? m_metadata->compressedData->size() : 0;
}


// ===== String Access =====

// Returns a String object containing the string content
String ParkableStringImpl::toString()
{
    if (!mayBeParked()) {
        return m_string;
    }
    Locker locker { m_metadata->lock };
    makeYoung();
    asanUnpoisonString(m_string);
    unpark();
    return m_string;
}

RefPtr<StringImpl> ParkableStringImpl::impl()
{
    String str = toString();
    return str.impl();
}

// ===== Locking =====

void ParkableStringImpl::lock()
{
    if (!mayBeParked())
        return;
        
    Locker locker { m_metadata->lock };
    ++m_metadata->lockCount;
    makeYoung();
}

void ParkableStringImpl::unlock()
{
    if (!mayBeParked())
        return;
        
    Locker locker { m_metadata->lock };
    ASSERT(m_metadata->lockCount > 0);
    --m_metadata->lockCount;
    
#if ASAN_ENABLED && ASSERT_ENABLED
    // There are no external references to the data, nobody should touch the data.
    //
    // Note: Only poison the memory if this is on the owning thread, as this is
    // otherwise racy. Indeed |unlock()| may be called on any thread, and
    // the owning thread may concurrently call |toString()|. It is then allowed
    // to use the string until the end of the current owning thread task.
    //
    // Checking the owning thread first as |currentStatus()| can only be called
    // from the owning thread. (Chrome-style per-string thread management)
    if (isOnOwningThread() && currentStatus() == Status::UnreferencedExternally) {
        asanPoisonString(m_string);
    }
#endif // ASAN_ENABLED && ASSERT_ENABLED
}

void ParkableStringImpl::lockWithoutMakingYoung()
{
    if (!mayBeParked())
        return;
        
    Locker locker { m_metadata->lock };
    ++m_metadata->lockCount;
}

// ===== Core State Management =====

// Checks if the string can be parked immediately
bool ParkableStringImpl::canParkNow() const
{
    return currentStatus() == Status::UnreferencedExternally 
        && m_metadata->age != Age::Young 
        && !m_metadata->compressionFailed;
}

ParkableStringImpl::Status ParkableStringImpl::currentStatus() const
{
    ASSERT(isOnOwningThread());
    ASSERT(mayBeParked());
    
    // Can park iff:
    // - |this| is not locked.
    // - There are no external reference to |string_|. Since |this| holds a reference to |string_|, it must be the only one.
    if (m_metadata->lockCount != 0)
        return Status::Locked;
    
    // Can be null if it is compressed.
    if (m_string.isNull())
        return Status::UnreferencedExternally;
        
    RefPtr stringImpl = m_string.impl();
    if (!stringImpl->hasOneRef())
        return Status::TooManyReferences;
        
    return Status::UnreferencedExternally;
}

// Discards the uncompressed StringImpl while keeping compressed data
void ParkableStringImpl::discardUncompressedData()
{
    // Must unpoison the memory before releasing it.
    asanUnpoisonString(m_string);
    m_string = String();
    m_metadata->state = State::Parked;

    ParkableStringManager::instance().completeParked(this);
}

// Discards compressed data to free memory
void ParkableStringImpl::discardCompressedData()
{
    m_metadata->compressedData = nullptr;
    ParkableStringManager::instance().completeParked(this);
}

// ===== Parking Operations =====

void ParkableStringImpl::unpark()
{
    if (m_metadata->state == State::Unparked) {
        return;
    }
    
    String result = unparkInternal();
    
    if (!result.isNull()) {
        m_string = result.impl();
    }
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

    assertOnValidThread();
    Locker locker { m_metadata->lock };
    
    if (m_metadata->backgroundTaskInProgress)
        return AgeOrParkResult::SuccessOrTransientFailure;
    
    // Handle already-parked strings
    if (isParkedNoLock()) {
        if (m_metadata->age == Age::VeryOld) {
            // bool ok = parkInternal(ParkingMode::ToDisk);
            // if (!ok)
            //     return AgeOrParkResult::NonTransientFailure;
        } else {
            ageString();
        }
        return AgeOrParkResult::SuccessOrTransientFailure;
    }
    
    // Handle unparked strings
    Status status = currentStatus();
    Age age = m_metadata->age;
    
    if (age == Age::Young) {
        // Age Young strings if they're unreferenced, but don't park them yet
        if (status == Status::UnreferencedExternally)
            ageString();
    } else if (m_metadata->age == Age::Old) {
        if (!canParkNow()) {
            return AgeOrParkResult::NonTransientFailure;
        }
        
        ParkingMode mode = ParkingMode::CompressOnly;
        
        bool ok = parkInternal(mode);
        if (!ok)
            return AgeOrParkResult::NonTransientFailure;
            
        return AgeOrParkResult::SuccessOrTransientFailure;
    }
    
    // External references to a string can be long-lived, cannot provide a progress guarantee for this string.
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
        if (m_metadata->compressedData) {
            discardUncompressedData();
        } else {
            return false;
        }
        break;
        
    case ParkingMode::CompressOnly:
        if (m_metadata->compressedData) {
            // Synchronous parking using cached compressed data
            discardUncompressedData();
        } else {
            scheduleCompressionTask(mode);
        }
        break;
    }
    
    return true;
}

// Internal unparking implementation that decompresses from memory
String ParkableStringImpl::unparkInternal()
{
    if (m_metadata->state == State::Parked) {
        return unparkFromCompressed();
    }
    
    return String();
}

// Unparks a string from compressed data stored in memory
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


// ===== Background Task Scheduling =====

// Schedules background compression with parameter setup and callback routing
void ParkableStringImpl::scheduleCompressionTask(ParkingMode mode)
{
    ASSERT(!m_metadata->backgroundTaskInProgress);
    
    // |string_|'s data should not be touched except in the compression task.
    asanPoisonString(m_string);
    m_metadata->backgroundTaskInProgress = true;
    
    std::span<const uint8_t> dataSpan;
    RefPtr stringImpl = m_string.impl();
    if (stringImpl) {
        if (stringImpl->is8Bit()) {
            auto chars = stringImpl->span8();
            dataSpan = byteCast<uint8_t>(chars);
        } else {
            auto chars = stringImpl->span16();
            dataSpan = spanReinterpretCast<const uint8_t>(chars);
        }
    }
    
    auto params = makeUnique<BackgroundTaskParams>(
        RefPtr<ParkableStringImpl>(this),
        dataSpan,
        mode,
        RunLoop::currentSingleton()
    );
    
    auto& manager = ParkableStringManager::instance();
    manager.workQueue().dispatch([params = WTFMove(params)]() mutable {
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
    
    // Complete compression on owning thread
    params->callbackRunLoop->dispatch([parkableString = params->string, params = WTFMove(params), compressedData = WTFMove(compressedData), elapsed]() mutable {
        parkableString->onCompressionCompleteOnMainThread(WTFMove(params), WTFMove(compressedData), elapsed);
    });
}

// ===== Background Completion Callbacks =====

// Handles completion of background compression
void ParkableStringImpl::onCompressionCompleteOnMainThread(std::unique_ptr<BackgroundTaskParams> params, std::unique_ptr<WTF::Vector<uint8_t>> compressedData, Seconds parkingThreadTime)
{
    if (!mayBeParked())
        return;
    
    ASSERT(m_metadata->backgroundTaskInProgress);
    Locker locker { m_metadata->lock };
    ASSERT(m_metadata->state == State::Unparked);
    
    m_metadata->backgroundTaskInProgress = false;
            
    // Always keep the compressed data. Compression is expensive, so even if the
    // uncompressed representation cannot be discarded now, avoid compressing
    // multiple times. This will allow synchronous parking next time.
    ASSERT(!m_metadata->compressedData);
    if (compressedData) {
        m_metadata->compressedData = WTFMove(compressedData);
    } else {
        m_metadata->compressionFailed = true;
    }
    
    // Between |park()| and now, things may have happened:
    // 1. |toString()| or
    // 2. |lock()| may have been called.
    //
    // Both of these will make the string young again, and if so we don't
    // discard the compressed representation yet.
    bool canParkNow = this->canParkNow();
    
    if (canParkNow && m_metadata->compressedData) {
        // Discard uncompressed but keep compressed
        discardUncompressedData();
        params->data = {}; // Clear uncompressed data copy
    } else {
        // If the string can not be parked immediately, cancel parking but keep compressed data for next time
        m_metadata->state = State::Unparked;
    }
    
    // Record the time no matter whether the string was parked or not, as the parking cost was paid.
    ParkableStringManager::instance().recordParkingThreadTime(parkingThreadTime);
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
        //m_metadata->age = Age::VeryOld;
        break;
    case Age::VeryOld:
        // VeryOld strings stay VeryOld and remain compressed in memory
        break;
    }
}

// ===== Memory Profiling Support =====

namespace {
void recordStringImplMemoryUsage(ParkableStringImpl::MemoryUsage* result, const RefPtr<StringImpl>& stringImpl)
{
    if (stringImpl) {
        RefPtr impl = stringImpl;
        result->stringImpl = impl.get();
        result->stringImplSize = sizeof(StringImpl) + impl->sizeInBytes();
    }
}
}

ParkableStringImpl::MemoryUsage ParkableStringImpl::memoryUsageForSnapshot() const
{
    assertOnValidThread();
    MemoryUsage result = {0, nullptr, 0};
    
    // Base size of ParkableStringImpl
    result.thisSize = sizeof(ParkableStringImpl);
    
    if (!mayBeParked()) {
        // Non-parkable string: just include StringImpl
        recordStringImplMemoryUsage(&result, m_string.impl());
        return result;
    }
    
    // Parkable string: add metadata overhead
    result.thisSize += sizeof(ParkableMetadata);
    
    Locker locker { m_metadata->lock };
    
    // Include StringImpl if currently unparked (in memory)
    if (!isParkedNoLock()) {
        recordStringImplMemoryUsage(&result, m_string.impl());
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
        // Use manager for parkable strings, which enables deduplication
        m_impl = ParkableStringManager::instance().add(WTFMove(string));
    } else {
        // Create non-parkable string directly
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
    return !m_impl;
}

size_t ParkableString::length() const
{
    RefPtr impl = m_impl;
    return impl ? impl->length() : 0;
}

bool ParkableString::is8Bit() const
{
    RefPtr impl = m_impl;
    return !impl || impl->is8Bit();
}

size_t ParkableString::sizeInBytes() const
{
    RefPtr impl = m_impl;
    return impl ? impl->sizeInBytes() : 0;
}

bool ParkableString::mayBeParked() const
{
    RefPtr impl = m_impl;
    return impl && impl->mayBeParked();
}

bool ParkableString::isParked() const
{
    RefPtr impl = m_impl;
    return impl && impl->isParked();
}


size_t ParkableString::compressedSize() const
{
    RefPtr impl = m_impl;
    return impl ? impl->compressedSize() : 0;
}


String ParkableString::toString() const
{
    RefPtr impl = m_impl;
    return impl ? impl->toString() : String();
}

RefPtr<StringImpl> ParkableString::impl() const
{
    RefPtr impl = m_impl;
    return impl ? impl->impl() : nullptr;
}

ParkableStringImpl* ParkableString::Impl() const
{
    return m_impl.get();
}

void ParkableString::lock()
{
    RefPtr impl = m_impl;
    if (impl)
        impl->lock();
}

void ParkableString::unlock()
{
    RefPtr impl = m_impl;
    if (impl)
        impl->unlock();
}

bool ParkableString::park(ParkableStringImpl::ParkingMode mode)
{
    RefPtr impl = m_impl;
    return impl && impl->park(mode);
}

size_t ParkableString::memoryFootprintForDump() const
{
    RefPtr impl = m_impl;
    return impl ? impl->memoryFootprintForDump() : 0;
}

// Returns parking state without acquiring the string's lock.
bool ParkableStringImpl::isParkedNoLock() const
{
    if (!mayBeParked())
        return false;
    return m_metadata->state == State::Parked;
}


// Checks if the current thread is the owning thread for this string.
bool ParkableStringImpl::isOnOwningThread() const
{
#if ASSERT_ENABLED
    return m_owningThreadUID == WTF::Thread::currentSingleton().uid();
#else
    return WTF::isMainThread();
#endif
}

// Returns the current lock count with proper synchronization.
int ParkableStringImpl::lockDepthForTesting() const
{
    if (!mayBeParked())
        return 0; // Non-parkable strings are never locked
        
    Locker locker { m_metadata->lock };
    return m_metadata->lockCount;
}

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
