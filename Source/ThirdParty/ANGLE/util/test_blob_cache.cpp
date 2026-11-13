//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// test_blob_cache.cpp:
//   A disk-backed EGL_ANDROID_blob_cache for tests, modelled on Android's MultifileBlobCache.
//
// Desktop platforms have no system blob cache.  This one stores each entry as a file in a
// directory, so what a test stores survives a Display teardown, and it behaves as Android's does:
//
//   - Entry files are in Android's format: named by a hash of the key, holding a header (magic,
//     CRC, key size, value size), the key and the value.  Each entry is charged its file size.
//   - A key or value larger than the per-entry limit is refused.  Like Android, it logs a warning,
//     but the caller isn't told that its value went nowhere.
//   - Storing an empty value for an existing entry removes it.  For a key with no entry, an empty
//     value is stored like any other.
//   - Crossing the total size or entry limit evicts least recently used entries until the cache is
//     down to half of both limits.
//   - Opening a directory reads the entries already in it, as Android does at startup, in the order
//     they were last used.
//
// Problems are logged the way Android logs them (frameworks/native/opengl/libs/EGL/
// MultifileBlobCache.cpp): warnings for refused stores, a full cache, hash collisions and entries
// changed on disk, and errors for I/O failures.  EGL_ANDROID_blob_cache gives the callbacks no way
// to report failure, so the log is the only place they show up.
//

#include "util/test_blob_cache.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <system_error>
#include <tuple>
#include <vector>

#include "common/debug.h"
#include "common/span.h"
#include "common/system_utils.h"
#include "common/unsafe_buffers.h"
#include "compression_utils_portable.h"

#if defined(ANGLE_PLATFORM_WINDOWS)
#    include <process.h>
#else
#    include <unistd.h>
#endif

namespace
{
// Android's entry file starts with this header (MultifileHeader in
// frameworks/native/opengl/libs/EGL/MultifileBlobCache.h), followed by the key and the value.  The
// CRC covers the key and the value.
struct EntryHeader
{
    uint32_t magic;
    uint32_t crc;
    EGLsizeiANDROID keySize;
    EGLsizeiANDROID valueSize;
};

// 'MFB$' as Android writes it, which is "$BFM" on disk.
constexpr uint32_t kEntryMagic = 0x4D464224;

using ByteSpan = angle::Span<const uint8_t>;

// Crossing a total limit evicts down to the limit divided by this, not just enough to fit.  See
// kCacheLimitDivisor in frameworks/native/opengl/libs/EGL/MultifileBlobCache.cpp.
constexpr size_t kCacheLimitDivisor = 2;

int GetCurrentProcessID()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

struct Entry
{
    size_t keySize      = 0;
    size_t valueSize    = 0;
    uint64_t lastAccess = 0;
};

// Every member is guarded by mutex.  ANGLE calls the callbacks from any thread.
struct State
{
    std::mutex mutex;
    std::filesystem::path directory;
    TestBlobCacheLimits limits;
    TestBlobCacheStats stats;
    // Keyed by the hash of the key, which names the entry's file.  As on Android, keys with the
    // same hash share an entry.
    std::map<uint32_t, Entry> entries;
    // Orders entries by last access for eviction.  A counter rather than a clock, so that entries
    // touched in quick succession never tie.
    uint64_t accessClock = 0;
    // The time recorded for the most recent use, so each use can be given a later one.  See
    // RecordUseLocked().
    std::filesystem::file_time_type lastUseTime = {};
    // Whether the callbacks have warned that no directory is set, so they only warn once.
    bool warnedNoDirectory = false;
};

State &GetState()
{
    static State *state = new State;
    return *state;
}

// The size Android charges an entry: its whole file.
size_t EntryFileSize(size_t keySize, size_t valueSize)
{
    return sizeof(EntryHeader) + keySize + valueSize;
}

// One mixing step of Bob Jenkins' one-at-a-time hash
// (https://en.wikipedia.org/wiki/Jenkins_hash_function), as android::JenkinsHashMix()
// (system/core/libutils/include/utils/JenkinsHash.h) implements it.
uint32_t JenkinsHashMix(uint32_t hash, uint32_t data)
{
    hash += data;
    hash += hash << 10;
    hash ^= hash >> 6;
    return hash;
}

// android::JenkinsHashMixBytes() (system/core/libutils/JenkinsHash.cpp) with a seed of zero, as
// MultifileBlobCache uses to name its entry files.  It mixes in the length, then the key a word at
// a time, and leaves out the final avalanche step (android::JenkinsHashWhiten()), as Android does.
uint32_t HashKey(ByteSpan key)
{
    // Mix in the length, then the key four bytes at a time, little endian
    const size_t size = key.size();
    uint32_t hash     = JenkinsHashMix(0, static_cast<uint32_t>(size));
    size_t i          = 0;
    for (; i + 4 <= size; i += 4)
    {
        hash = JenkinsHashMix(hash, key[i] | (key[i + 1] << 8) | (key[i + 2] << 16) |
                                        (static_cast<uint32_t>(key[i + 3]) << 24));
    }

    // Then the last one to three bytes, if any, as one partial word
    if (i < size)
    {
        uint32_t data = key[i];
        if (i + 1 < size)
        {
            data |= key[i + 1] << 8;
        }
        if (i + 2 < size)
        {
            data |= key[i + 2] << 16;
        }
        hash = JenkinsHashMix(hash, data);
    }
    return hash;
}

// The entry CRC is zlib's crc32(), as Android computes it (GenerateCRC32() in
// frameworks/native/opengl/libs/EGL/FileBlobCache.cpp) and as ANGLE's own angle::GenerateCRC32()
// does.  Continues the CRC |crc| over |bytes|, so that the key and the value can be covered without
// copying them together.  A CRC starts from zero.
uint32_t CRC32(uint32_t crc, ByteSpan bytes)
{
    return static_cast<uint32_t>(crc32_z(crc, bytes.data(), bytes.size()));
}

// Entry files are named with the decimal hash, as std::to_string() writes it.  Anything else in
// the directory is not an entry.
bool ParseEntryName(const std::string &name, uint32_t *hashOut)
{
    // At most ten decimal digits, the length of UINT32_MAX
    if (name.empty() || name.size() > 10 ||
        name.find_first_not_of("0123456789") != std::string::npos)
    {
        return false;
    }

    // Reject values that don't fit in 32 bits, and spellings std::to_string() wouldn't produce,
    // such as leading zeros
    const uint64_t hash = std::stoull(name);
    if (hash > UINT32_MAX || std::to_string(hash) != name)
    {
        return false;
    }
    *hashOut = static_cast<uint32_t>(hash);
    return true;
}

std::filesystem::path EntryPath(const State &state, uint32_t hash)
{
    return state.directory / std::to_string(hash);
}

// Logs, once, that the callbacks are being ignored because no directory is set.  ANGLE calls them
// on every compile, so warning on each call would flood the log.
void WarnNoDirectoryLocked(State &state)
{
    if (!state.warnedNoDirectory)
    {
        WARN() << "TestBlobCache: No directory set, ignoring the cache until "
                  "InitTestBlobCache() is given one";
        state.warnedNoDirectory = true;
    }
}

// Records a use of an entry in its file's write time, which orders the entries when the cache is
// opened again.  Android records uses in the access time, but std::filesystem only offers the write
// time.  The clock can be coarse (on Windows it may only advance every few milliseconds), so
// entries used in quick succession would tie and lose their order.  Each use is therefore recorded
// at least a microsecond after the previous one.
void RecordUseLocked(State &state, const std::filesystem::path &path)
{
    const std::filesystem::file_time_type next =
        state.lastUseTime + std::chrono::duration_cast<std::filesystem::file_time_type::duration>(
                                std::chrono::microseconds(1));
    state.lastUseTime = std::max(std::filesystem::file_time_type::clock::now(), next);

    std::error_code ec;
    std::filesystem::last_write_time(path, state.lastUseTime, ec);
    if (ec)
    {
        WARN() << "TestBlobCache: Failed to record the last use of " << path.string() << ": "
               << ec.message();
    }
}

// Forgets an entry and deletes its file.
void RemoveEntryLocked(State &state, std::map<uint32_t, Entry>::iterator it)
{
    state.stats.totalSize -= EntryFileSize(it->second.keySize, it->second.valueSize);

    // A file that's already gone is fine; only a failure to delete one is worth a warning, as on
    // Android
    const std::filesystem::path path = EntryPath(state, it->first);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec)
    {
        WARN() << "TestBlobCache: Error removing " << path.string() << ": " << ec.message();
    }

    state.entries.erase(it);
}

// Evicts least recently used entries until the cache is within both limits.
void ApplyLRULocked(State &state, size_t sizeLimit, size_t entryLimit)
{
    // Sort the entries oldest first
    std::vector<std::pair<uint64_t, uint32_t>> byAge;
    byAge.reserve(state.entries.size());
    for (const auto &entry : state.entries)
    {
        byAge.emplace_back(entry.second.lastAccess, entry.first);
    }
    std::sort(byAge.begin(), byAge.end());

    // Remove them in that order until both the size and the entry count fit
    const uint64_t evictionsBefore = state.stats.evictions;
    for (const auto &oldest : byAge)
    {
        if (state.stats.totalSize <= sizeLimit && state.entries.size() <= entryLimit)
        {
            break;
        }
        RemoveEntryLocked(state, state.entries.find(oldest.second));
        ++state.stats.evictions;
    }

    INFO() << "TestBlobCache: Evicted " << (state.stats.evictions - evictionsBefore)
           << " entries, leaving " << state.entries.size() << " entries and "
           << state.stats.totalSize << " bytes";
}

// The EGL_ANDROID_blob_cache set callback, following MultifileBlobCache::set().  A store replaces
// any existing entry for the key, and an empty value removes it.
void SetBlob(const void *key, EGLsizeiANDROID keySize, const void *value, EGLsizeiANDROID valueSize)
{
    State &state = GetState();
    // Guards the entries, the files and the statistics.
    std::lock_guard<std::mutex> lock(state.mutex);

    // Without a directory there is nowhere to store anything
    if (state.directory.empty())
    {
        WarnNoDirectoryLocked(state);
        return;
    }

    // Arguments ANGLE should never pass
    if (key == nullptr || keySize <= 0 || valueSize < 0 || (valueSize > 0 && value == nullptr))
    {
        ERR() << "TestBlobCache: Invalid store, key " << key << " keySize " << keySize << " value "
              << value << " valueSize " << valueSize;
        return;
    }

    // Refuse keys and values over the per-entry limits.  Android logs a warning, but the caller
    // isn't told its value went nowhere.
    const size_t keyBytes = static_cast<size_t>(keySize);
    const size_t size     = static_cast<size_t>(valueSize);
    if (keyBytes > state.limits.maxKeySize || size > state.limits.maxValueSize)
    {
        WARN() << "TestBlobCache: Refusing store, keySize (" << keyBytes << " vs "
               << state.limits.maxKeySize << ") or valueSize (" << size << " vs "
               << state.limits.maxValueSize << ") too large";
        ++state.stats.rejectedStores;
        return;
    }

    // SAFETY: EGL_ANDROID_blob_cache passes |keySize| bytes at |key|, checked above.
    const ByteSpan keySpan =
        ANGLE_UNSAFE_BUFFERS(ByteSpan(static_cast<const uint8_t *>(key), keyBytes));
    // SAFETY: EGL_ANDROID_blob_cache passes |valueSize| bytes at |value|, checked above.  |value|
    // may be null when |valueSize| is zero, which makes an empty span.
    const ByteSpan valueSpan =
        ANGLE_UNSAFE_BUFFERS(ByteSpan(static_cast<const uint8_t *>(value), size));

    // A store replaces the whole entry, so the old one is released first.  As in Android's
    // MultifileBlobCache::set(), an empty value for an existing entry is a request to remove it, so
    // there is nothing more to do.  For a key with no entry, an empty value is stored like any
    // other.
    const uint32_t hash = HashKey(keySpan);
    auto existing       = state.entries.find(hash);
    if (existing != state.entries.end())
    {
        RemoveEntryLocked(state, existing);
        if (size == 0)
        {
            INFO() << "TestBlobCache: Removed the entry for hash " << hash
                   << ", which was given an empty value";
            return;
        }
    }

    // If the new entry would cross either total limit, evict down to half of both limits first
    const size_t fileSize = EntryFileSize(keyBytes, size);
    if (state.stats.totalSize + fileSize > state.limits.maxTotalSize ||
        state.entries.size() + 1 > state.limits.maxTotalEntries)
    {
        WARN() << "TestBlobCache: Cache is full (" << state.entries.size() << " entries, "
               << state.stats.totalSize << " bytes), evicting least recently used entries";
        ApplyLRULocked(state, state.limits.maxTotalSize / kCacheLimitDivisor,
                       state.limits.maxTotalEntries / kCacheLimitDivisor);
    }

    // Build the file in Android's format: the header, then the key, then the value, with the CRC
    // covering the key and the value
    const EntryHeader header  = {kEntryMagic, CRC32(CRC32(0, keySpan), valueSpan), keySize,
                                 valueSize};
    const ByteSpan headerSpan = angle::byte_span_from_ref(header);
    std::vector<uint8_t> contents;
    contents.reserve(fileSize);
    contents.insert(contents.end(), headerSpan.begin(), headerSpan.end());
    contents.insert(contents.end(), keySpan.begin(), keySpan.end());
    contents.insert(contents.end(), valueSpan.begin(), valueSpan.end());

    // Write it out.  On failure, don't leave a partial file behind for a later open to find.
    const std::filesystem::path path = EntryPath(state, hash);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(contents.data()),
               static_cast<std::streamsize>(contents.size()));
    file.close();
    if (!file)
    {
        ERR() << "TestBlobCache: Failed to write " << path.string();
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return;
    }

    // Track the new entry as the most recently used, on disk as well as in memory
    RecordUseLocked(state, path);
    state.entries[hash] = {keyBytes, size, ++state.accessClock};
    state.stats.totalSize += fileSize;
    ++state.stats.stores;
    state.stats.storedBytes += size;
}

// The EGL_ANDROID_blob_cache get callback, following MultifileBlobCache::get().  ANGLE asks for the
// size with no buffer, then again with a buffer of that size.  Only the call that returns the value
// counts as a hit.
EGLsizeiANDROID GetBlob(const void *key,
                        EGLsizeiANDROID keySize,
                        void *value,
                        EGLsizeiANDROID valueSize)
{
    State &state = GetState();
    // Guards the entries, the files and the statistics.
    std::lock_guard<std::mutex> lock(state.mutex);

    // Without a directory there is nothing to find
    if (state.directory.empty())
    {
        WarnNoDirectoryLocked(state);
        return 0;
    }

    // Arguments ANGLE should never pass
    if (key == nullptr || keySize <= 0)
    {
        ERR() << "TestBlobCache: Invalid lookup, key " << key << " keySize " << keySize;
        return 0;
    }

    // SAFETY: EGL_ANDROID_blob_cache passes |keySize| bytes at |key|, checked above.
    const ByteSpan keySpan = ANGLE_UNSAFE_BUFFERS(
        ByteSpan(static_cast<const uint8_t *>(key), static_cast<size_t>(keySize)));

    // Look up the entry by the key's hash
    auto it = state.entries.find(HashKey(keySpan));
    if (it == state.entries.end())
    {
        ++state.stats.misses;
        return 0;
    }

    // If the caller's buffer can't hold the value, this is the size query: just report the size
    const size_t size = it->second.valueSize;
    if (value == nullptr || valueSize < 0 || static_cast<size_t>(valueSize) < size)
    {
        return static_cast<EGLsizeiANDROID>(size);
    }

    // Read the whole file, as Android does before returning an entry it read from disk
    const std::filesystem::path path = EntryPath(state, it->first);
    std::vector<uint8_t> contents(EntryFileSize(it->second.keySize, size));
    std::ifstream file(path, std::ios::binary);
    file.read(reinterpret_cast<char *>(contents.data()),
              static_cast<std::streamsize>(contents.size()));

    // Close the file before anything below removes it.  Windows can't delete a file that is open.
    const bool readSucceeded = static_cast<bool>(file);
    file.close();
    if (!readSucceeded)
    {
        ERR() << "TestBlobCache: Failed to read " << path.string() << ", removing the entry";
        RemoveEntryLocked(state, it);
        ++state.stats.misses;
        return 0;
    }

    // Check that the file still holds what the cache stored: the header must match the tracked
    // sizes and the CRC must match the contents
    const auto [headerSpan, keyAndValue] = ByteSpan(contents).split_at(sizeof(EntryHeader));
    EntryHeader header                   = {};
    angle::byte_span_from_ref(header).copy_from(headerSpan);
    if (header.magic != kEntryMagic ||
        header.keySize != static_cast<EGLsizeiANDROID>(it->second.keySize) ||
        header.valueSize != static_cast<EGLsizeiANDROID>(size) ||
        header.crc != CRC32(0, keyAndValue))
    {
        WARN() << "TestBlobCache: " << path.string()
               << " was changed behind the cache's back, removing the entry";
        RemoveEntryLocked(state, it);
        ++state.stats.misses;
        return 0;
    }

    // The entry is keyed by hash, so make sure it holds this key and not another with the same
    // hash
    const auto [storedKey, storedValue] = keyAndValue.split_at(it->second.keySize);
    if (storedKey != keySpan)
    {
        WARN() << "TestBlobCache: Key doesn't match the entry stored for hash " << it->first
               << ", a hash collision or modified file";
        ++state.stats.misses;
        return 0;
    }

    // Return the value and mark the entry as the most recently used, on disk as well as in memory.
    // SAFETY: the caller's buffer holds |valueSize| bytes at |value|, at least |size| as checked
    // above.
    const angle::Span<uint8_t> valueOut =
        ANGLE_UNSAFE_BUFFERS(angle::Span<uint8_t>(static_cast<uint8_t *>(value), size));
    valueOut.copy_from(storedValue);
    it->second.lastAccess = ++state.accessClock;
    RecordUseLocked(state, path);

    ++state.stats.hits;
    return static_cast<EGLsizeiANDROID>(size);
}

// Uses |dir| for storage, creating it if needed, and reads the entries already in it.  Statistics
// start from zero, as in a new process.
void OpenDirectoryLocked(State &state, const std::string &dir)
{
    state.entries.clear();
    state.stats             = {};
    state.accessClock       = 0;
    state.lastUseTime       = {};
    state.warnedNoDirectory = false;
    state.directory         = dir;
    if (state.directory.empty())
    {
        return;
    }

    // Every filesystem call takes an error_code: ANGLE builds without exceptions, so the throwing
    // overloads would abort.
    std::error_code ec;
    std::filesystem::create_directories(state.directory, ec);
    if (ec)
    {
        ERR() << "TestBlobCache: Failed to create " << state.directory.string() << ": "
              << ec.message();
        return;
    }

    // Read the entries already there, least recently used first, so that eviction order survives
    // a restart.  Each is {last use, hash, key size, value size}.
    // |ec| tracks listing the directory, and |entryEc| each entry, so that a problem with one
    // entry doesn't end the listing.
    std::vector<std::tuple<std::filesystem::file_time_type, uint32_t, size_t, size_t>> found;
    std::filesystem::directory_iterator it(state.directory, ec);
    for (std::filesystem::directory_iterator end; !ec && it != end; it.increment(ec))
    {
        std::error_code entryEc;

        // Files that aren't named like entries aren't ours, so leave them alone
        uint32_t hash = 0;
        if (!it->is_regular_file(entryEc) || !ParseEntryName(it->path().filename().string(), &hash))
        {
            continue;
        }

        // Read the header and check that the file is the size it describes.  Unlike Android, the
        // CRC isn't checked until the entry is read.
        EntryHeader header = {};
        std::ifstream file(it->path(), std::ios::binary);
        file.read(reinterpret_cast<char *>(&header), sizeof(header));
        const uintmax_t fileSize = it->file_size(entryEc);
        if (!file || entryEc || header.magic != kEntryMagic || header.keySize <= 0 ||
            header.valueSize < 0 ||
            EntryFileSize(static_cast<size_t>(header.keySize),
                          static_cast<size_t>(header.valueSize)) != fileSize)
        {
            WARN() << "TestBlobCache: Skipping " << it->path().string()
                   << ", which isn't a valid entry";
            continue;
        }

        // Its write time is when it was last used
        const std::filesystem::file_time_type lastUse = it->last_write_time(entryEc);
        if (entryEc)
        {
            WARN() << "TestBlobCache: Skipping " << it->path().string()
                   << ", unable to read its last use: " << entryEc.message();
            continue;
        }
        found.emplace_back(lastUse, hash, static_cast<size_t>(header.keySize),
                           static_cast<size_t>(header.valueSize));
    }
    if (ec)
    {
        ERR() << "TestBlobCache: Failed to list " << state.directory.string() << ": "
              << ec.message();
    }

    // Track them oldest first, so the access clock preserves their order.  Later uses are recorded
    // after the newest of them.
    std::sort(found.begin(), found.end());
    for (const auto &[lastUse, hash, keySize, valueSize] : found)
    {
        state.entries[hash] = {keySize, valueSize, ++state.accessClock};
        state.stats.totalSize += EntryFileSize(keySize, valueSize);
        state.lastUseTime = lastUse;
    }

    INFO() << "TestBlobCache: Opened " << state.directory.string() << " with "
           << state.entries.size() << " entries and " << state.stats.totalSize << " bytes";
}
}  // namespace

// Creates an empty directory that no other process is using, for InitTestBlobCache().  Bots
// run test processes side by side, each building its own fixtures, so a fixed name would let one
// process delete the entries another is still reading.  The caller removes the directory when done.
// A process has one at a time: calling this again returns the same directory, emptied.  Returns an
// empty string, which leaves the cache disabled, if no directory can be created.
std::string CreateTestBlobCacheDirectory()
{
    // Every filesystem call takes an error_code: ANGLE builds without exceptions, so the throwing
    // overloads would abort.
    std::error_code ec;

    // Find the temporary directory, and make sure it exists.  On Android it's TMPDIR, which the
    // framework points at the app's cache directory, and which may not have been created yet.
    const Optional<std::string> tempDir = angle::GetTempDirectory();
    if (!tempDir.valid())
    {
        ERR() << "TestBlobCache: No temporary directory for the cache, is TMPDIR set?";
        return "";
    }
    std::filesystem::create_directories(tempDir.value(), ec);
    if (ec)
    {
        ERR() << "TestBlobCache: Failed to create the temporary directory " << tempDir.value()
              << ": " << ec.message();
        return "";
    }

    // Name the directory after this process, which no other live process shares.  This allows one
    // directory per process at a time, as the cache itself is one per process.
    const std::string dir = (std::filesystem::path(tempDir.value()) /
                             (".angle_blob_cache." + std::to_string(GetCurrentProcessID())))
                                .string();
    if (std::filesystem::create_directory(dir, ec))
    {
        return dir;
    }
    if (ec && ec != std::errc::file_exists)
    {
        ERR() << "TestBlobCache: Failed to create " << dir << ": " << ec.message();
        return "";
    }

    // It's already there, left by a process that didn't clean up: a dead one whose ID was reused,
    // or an earlier fixture in this one.  Nothing is using it, so start over.  That something
    // didn't clean up is worth knowing about.
    WARN() << "TestBlobCache: " << dir << " was left behind by an earlier run, removing it";
    std::filesystem::remove_all(dir, ec);
    if (ec || !std::filesystem::create_directory(dir, ec))
    {
        ERR() << "TestBlobCache: Failed to recreate " << dir << ": " << ec.message();
        return "";
    }
    return dir;
}

// Stops using the cache and removes |dir|, which CreateTestBlobCacheDirectory() returned.  The
// cache forgets the directory first, so that a Display that is still alive stores nothing into it:
// a store into a removed directory would fail and log an error.
void RemoveTestBlobCacheDirectory(const std::string &dir)
{
    ResetTestBlobCache();

    // An empty name means no directory was created
    if (dir.empty())
    {
        return;
    }

    // The error_code overload: ANGLE builds without exceptions, so the throwing one would abort.
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    if (ec)
    {
        WARN() << "TestBlobCache: Failed to remove " << dir << ": " << ec.message();
    }
}

// Sets up the cache for a new Display, as Android does for a new process: applies |limits|, reads
// the entries already in |dir|, creating it if needed, and hands the callbacks to |display| through
// the caller's eglSetBlobCacheFuncsANDROID.  Called again with the same directory for a later
// Display, it is what a process restart looks like to the cache.  Statistics start from zero.
//
// The caller supplies the entry point because how to get one depends on how the binary reaches
// ANGLE.  EGL only accepts the callbacks once per display, so a Display that already has them
// raises EGL_BAD_PARAMETER and keeps using them.
void InitTestBlobCache(EGLDisplay display,
                       PFNEGLSETBLOBCACHEFUNCSANDROIDPROC setFuncs,
                       const std::string &dir,
                       const TestBlobCacheLimits &limits)
{
    // Open the directory before the callbacks are handed out, so that they never see a stale one
    {
        State &state = GetState();
        // Guards the entries, the files, the limits and the statistics.
        std::lock_guard<std::mutex> lock(state.mutex);
        state.limits = limits;
        OpenDirectoryLocked(state, dir);
    }

    if (setFuncs == nullptr)
    {
        ERR() << "TestBlobCache: No eglSetBlobCacheFuncsANDROID entry point, the cache won't be "
                 "used";
        return;
    }
    setFuncs(display, SetBlob, GetBlob);
}

// Returns the statistics since the directory was last set, and what the cache holds now.
TestBlobCacheStats GetTestBlobCacheStats()
{
    State &state = GetState();
    // Updated by the callbacks on any thread.
    std::lock_guard<std::mutex> lock(state.mutex);
    TestBlobCacheStats stats = state.stats;
    stats.entries            = state.entries.size();
    return stats;
}

// Forgets the directory, the entries and the statistics, and restores the default limits.  Files
// on disk are left alone.
void ResetTestBlobCache()
{
    State &state = GetState();
    // Guards everything the callbacks touch.
    std::lock_guard<std::mutex> lock(state.mutex);
    state.directory.clear();
    state.entries.clear();
    state.limits            = {};
    state.stats             = {};
    state.accessClock       = 0;
    state.lastUseTime       = {};
    state.warnedNoDirectory = false;
}
