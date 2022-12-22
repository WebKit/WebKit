/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Canon Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/FileSystem.h>

#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HexNumber.h>
#include <wtf/Logging.h>
#include <wtf/Scope.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if !OS(WINDOWS)
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if USE(GLIB)
#include <gio/gfiledescriptorbased.h>
#include <gio/gio.h>
#endif

#if HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)
#include <wtf/StdFilesystem.h>
#endif

namespace WTF::FileSystemImpl {

#if HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

static std::filesystem::path toStdFileSystemPath(StringView path)
{
#if HAVE(MISSING_STD_FILESYSTEM_PATH_CONSTRUCTOR)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return std::filesystem::u8path(path.utf8().data());
    ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return { std::u8string(reinterpret_cast<const char8_t*>(path.utf8().data())) };
#endif
}

static String fromStdFileSystemPath(const std::filesystem::path& path)
{
    return String::fromUTF8(reinterpret_cast<const LChar*>(path.u8string().c_str()));
}

#endif // HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

// The following lower-ASCII characters need escaping to be used in a filename
// across all systems, including Windows:
//     - Unprintable ASCII (00-1F)
//     - Space             (20)
//     - Double quote      (22)
//     - Percent           (25) (escaped because it is our escape character)
//     - Asterisk          (2A)
//     - Slash             (2F)
//     - Colon             (3A)
//     - Less-than         (3C)
//     - Greater-than      (3E)
//     - Question Mark     (3F)
//     - Backslash         (5C)
//     - Pipe              (7C)
//     - Delete            (7F)

static const bool needsEscaping[128] = {
    true,  true,  true,  true,  true,  true,  true,  true,  /* 00-07 */
    true,  true,  true,  true,  true,  true,  true,  true,  /* 08-0F */

    true,  true,  true,  true,  true,  true,  true,  true,  /* 10-17 */
    true,  true,  true,  true,  true,  true,  true,  true,  /* 18-1F */

    true,  false, true,  false, false, true,  false, false, /* 20-27 */
    false, false, true,  false, false, false, false, true,  /* 28-2F */

    false, false, false, false, false, false, false, false, /* 30-37 */
    false, false, true,  false, true,  false, true,  true,  /* 38-3F */

    false, false, false, false, false, false, false, false, /* 40-47 */
    false, false, false, false, false, false, false, false, /* 48-4F */

    false, false, false, false, false, false, false, false, /* 50-57 */
    false, false, false, false, true,  false, false, false, /* 58-5F */

    false, false, false, false, false, false, false, false, /* 60-67 */
    false, false, false, false, false, false, false, false, /* 68-6F */

    false, false, false, false, false, false, false, false, /* 70-77 */
    false, false, false, false, true,  false, false, true,  /* 78-7F */
};

static inline bool shouldEscapeUChar(UChar character, UChar previousCharacter, UChar nextCharacter)
{
    if (character <= 127)
        return needsEscaping[character];

    if (U16_IS_LEAD(character) && !U16_IS_TRAIL(nextCharacter))
        return true;

    if (U16_IS_TRAIL(character) && !U16_IS_LEAD(previousCharacter))
        return true;

    return false;
}

#if HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

template<typename, typename = void> inline constexpr bool HasToTimeT = false;
template<typename ClockType> inline constexpr bool HasToTimeT<ClockType, std::void_t<
    std::enable_if_t<std::is_same_v<std::time_t, decltype(ClockType::to_time_t(std::filesystem::file_time_type()))>>
>> = true;

template <typename FileTimeType>
typename std::time_t toTimeT(FileTimeType fileTime)
{
    if constexpr (HasToTimeT<typename FileTimeType::clock>)
        return decltype(fileTime)::clock::to_time_t(fileTime);
    else
        return std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(fileTime - decltype(fileTime)::clock::now() + std::chrono::system_clock::now()));
}

static WallTime toWallTime(std::filesystem::file_time_type fileTime)
{
    // FIXME: Use std::chrono::file_clock::to_sys() once we can use C++20.
    return WallTime::fromRawSeconds(toTimeT(fileTime));
}

#endif // HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

String encodeForFileName(const String& inputString)
{
    unsigned length = inputString.length();
    if (!length)
        return inputString;

    StringBuilder result;
    result.reserveCapacity(length);

    UChar previousCharacter = 0;
    UChar nextCharacter = inputString[0];
    for (unsigned i = 0; i < length; ++i) {
        auto character = std::exchange(nextCharacter, i + 1 < length ? inputString[i + 1] : 0);
        if (shouldEscapeUChar(character, previousCharacter, nextCharacter)) {
            if (character <= 0xFF)
                result.append('%', hex(character, 2));
            else
                result.append("%+", hex(static_cast<uint8_t>(character >> 8), 2), hex(static_cast<uint8_t>(character), 2));
        } else
            result.append(character);
        previousCharacter = character;
    }

    return result.toString();
}

String decodeFromFilename(const String& inputString)
{
    unsigned length = inputString.length();
    if (!length)
        return inputString;

    StringBuilder result;
    result.reserveCapacity(length);

    for (unsigned i = 0; i < length; ++i) {
        if (inputString[i] != '%') {
            result.append(inputString[i]);
            continue;
        }

        // If the input string is a valid encoded filename, it must be at least 2 characters longer
        // than the current index to account for this percent encoded value.
        if (i + 2 >= length)
            return { };

        if (inputString[i+1] != '+') {
            if (!isASCIIHexDigit(inputString[i + 1]))
                return { };
            if (!isASCIIHexDigit(inputString[i + 2]))
                return { };
            result.append(toASCIIHexValue(inputString[i + 1], inputString[i + 2]));
            i += 2;
            continue;
        }

        // If the input string is a valid encoded filename, it must be at least 5 characters longer
        // than the current index to account for this percent encoded value.
        if (i + 5 >= length)
            return { };

        if (!isASCIIHexDigit(inputString[i + 2]))
            return { };
        if (!isASCIIHexDigit(inputString[i + 3]))
            return { };
        if (!isASCIIHexDigit(inputString[i + 4]))
            return { };
        if (!isASCIIHexDigit(inputString[i + 5]))
            return { };

        UChar encodedCharacter = toASCIIHexValue(inputString[i + 2], inputString[i + 3]) << 8 | toASCIIHexValue(inputString[i + 4], inputString[i + 5]);
        result.append(encodedCharacter);
        i += 5;
    }

    return result.toString();
}

String lastComponentOfPathIgnoringTrailingSlash(const String& path)
{
#if OS(WINDOWS)
    char pathSeparator = '\\';
#else
    char pathSeparator = '/';
#endif

    auto position = path.reverseFind(pathSeparator);
    if (position == notFound)
        return path;

    size_t endOfSubstring = path.length() - 1;
    if (position == endOfSubstring) {
        --endOfSubstring;
        position = path.reverseFind(pathSeparator, endOfSubstring);
    }

    return path.substring(position + 1, endOfSubstring - position);
}

bool appendFileContentsToFileHandle(const String& path, PlatformFileHandle& target)
{
    auto source = openFile(path, FileOpenMode::Read);

    if (!isHandleValid(source))
        return false;

    static int bufferSize = 1 << 19;
    Vector<uint8_t> buffer(bufferSize);

    auto fileCloser = WTF::makeScopeExit([source]() {
        PlatformFileHandle handle = source;
        closeFile(handle);
    });

    do {
        int readBytes = readFromFile(source, buffer.data(), bufferSize);

        if (readBytes < 0)
            return false;

        if (writeToFile(target, buffer.data(), readBytes) != readBytes)
            return false;

        if (readBytes < bufferSize)
            return true;
    } while (true);

    ASSERT_NOT_REACHED();
}


bool filesHaveSameVolume(const String& fileA, const String& fileB)
{
    if (fileA.isNull() || fileB.isNull())
        return false;

    bool result = false;

    auto fileADev = getFileDeviceId(fileA);
    auto fileBDev = getFileDeviceId(fileB);

    if (fileADev && fileBDev)
        result = (fileADev == fileBDev);

    return result;
}

#if !PLATFORM(MAC)

void setMetadataURL(const String&, const String&, const String&)
{
}

#endif

MappedFileData::MappedFileData(const String& filePath, MappedFileMode mapMode, bool& success)
{
    auto fd = openFile(filePath, FileSystem::FileOpenMode::Read);

    success = mapFileHandle(fd, FileSystem::FileOpenMode::Read, mapMode);
    closeFile(fd);
}

#if HAVE(MMAP)

MappedFileData::~MappedFileData()
{
    if (!m_fileData)
        return;
    munmap(m_fileData, m_fileSize);
}

bool MappedFileData::mapFileHandle(PlatformFileHandle handle, FileOpenMode openMode, MappedFileMode mapMode)
{
    if (!isHandleValid(handle))
        return false;

    int fd;
#if USE(GLIB)
    auto* inputStream = g_io_stream_get_input_stream(G_IO_STREAM(handle));
    fd = g_file_descriptor_based_get_fd(G_FILE_DESCRIPTOR_BASED(inputStream));
#else
    fd = handle;
#endif

    struct stat fileStat;
    if (fstat(fd, &fileStat)) {
        return false;
    }

    unsigned size;
    if (!WTF::convertSafely(fileStat.st_size, size)) {
        return false;
    }

    if (!size)
        return true;

    int pageProtection = PROT_READ;
    switch (openMode) {
    case FileOpenMode::Read:
        pageProtection = PROT_READ;
        break;
    case FileOpenMode::Write:
        pageProtection = PROT_WRITE;
        break;
    case FileOpenMode::ReadWrite:
        pageProtection = PROT_READ | PROT_WRITE;
        break;
#if OS(DARWIN)
    case FileOpenMode::EventsOnly:
        ASSERT_NOT_REACHED();
#endif
    }

    void* data = mmap(0, size, pageProtection, MAP_FILE | (mapMode == MappedFileMode::Shared ? MAP_SHARED : MAP_PRIVATE), fd, 0);

    if (data == MAP_FAILED) {
        return false;
    }

    m_fileData = data;
    m_fileSize = size;
    return true;
}
#endif

PlatformFileHandle openAndLockFile(const String& path, FileOpenMode openMode, OptionSet<FileLockMode> lockMode)
{
    auto handle = openFile(path, openMode);
    if (handle == invalidPlatformFileHandle)
        return invalidPlatformFileHandle;

#if USE(FILE_LOCK)
    bool locked = lockFile(handle, lockMode);
    ASSERT_UNUSED(locked, locked);
#else
    UNUSED_PARAM(lockMode);
#endif

    return handle;
}

void unlockAndCloseFile(PlatformFileHandle handle)
{
#if USE(FILE_LOCK)
    bool unlocked = unlockFile(handle);
    ASSERT_UNUSED(unlocked, unlocked);
#endif
    closeFile(handle);
}

#if !PLATFORM(IOS_FAMILY)
bool isSafeToUseMemoryMapForPath(const String&)
{
    return true;
}

bool makeSafeToUseMemoryMapForPath(const String&)
{
    return true;
}
#endif

#if !PLATFORM(COCOA)

String createTemporaryZipArchive(const String&)
{
    return { };
}

bool setExcludedFromBackup(const String&, bool)
{
    return false;
}

bool markPurgeable(const String&)
{
    return false;
}

#endif

MappedFileData createMappedFileData(const String& path, size_t bytesSize, PlatformFileHandle* outputHandle)
{
    constexpr bool failIfFileExists = true;
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite, FileSystem::FileAccessPermission::User, failIfFileExists);

    auto fileCloser = WTF::makeScopeExit([&handle]() {
        FileSystem::closeFile(handle);
    });

    if (!FileSystem::isHandleValid(handle))
        return { };

    if (!FileSystem::truncateFile(handle, bytesSize)) {
        RELEASE_LOG_FAULT(MemoryPressure, "Unable to truncate file");
        return { };
    }

    if (!FileSystem::makeSafeToUseMemoryMapForPath(path))
        return { };

    bool success;
    FileSystem::MappedFileData mappedFile(handle, FileSystem::FileOpenMode::ReadWrite, FileSystem::MappedFileMode::Shared, success);
    if (!success)
        return { };

    if (outputHandle) {
        fileCloser.release();
        *outputHandle = handle;
    }

    return mappedFile;
}

void finalizeMappedFileData(MappedFileData& mappedFileData, size_t bytesSize)
{
    void* map = const_cast<void*>(mappedFileData.data());
#if OS(WINDOWS)
    DWORD oldProtection;
    VirtualProtect(map, bytesSize, FILE_MAP_READ, &oldProtection);
    FlushViewOfFile(map, bytesSize);
#else
    // Drop the write permission.
    mprotect(map, bytesSize, PROT_READ);

    // Flush (asynchronously) to file, turning this into clean memory.
    msync(map, bytesSize, MS_ASYNC);
#endif
}

MappedFileData mapToFile(const String& path, size_t bytesSize, Function<void(const Function<bool(Span<const uint8_t>)>&)>&& apply, PlatformFileHandle* outputHandle)
{
    auto mappedFile = createMappedFileData(path, bytesSize, outputHandle);
    if (!mappedFile)
        return { };

    void* map = const_cast<void*>(mappedFile.data());
    uint8_t* mapData = static_cast<uint8_t*>(map);

    apply([&mapData](Span<const uint8_t> chunk) {
        memcpy(mapData, chunk.data(), chunk.size());
        mapData += chunk.size();
        return true;
    });

    finalizeMappedFileData(mappedFile, bytesSize);

    return mappedFile;
}

static Salt makeSalt()
{
    Salt salt;
    cryptographicallyRandomValues(&salt, sizeof(Salt));
    return salt;
}

std::optional<Salt> readOrMakeSalt(const String& path)
{
    if (FileSystem::fileExists(path)) {
        auto file = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
        Salt salt;
        auto bytesRead = static_cast<std::size_t>(FileSystem::readFromFile(file, salt.data(), salt.size()));
        FileSystem::closeFile(file);
        if (bytesRead == salt.size())
            return salt;

        FileSystem::deleteFile(path);
    }

    Salt salt = makeSalt();
    FileSystem::makeAllDirectories(FileSystem::parentPath(path));
    auto file = FileSystem::openFile(path, FileSystem::FileOpenMode::Write, FileSystem::FileAccessPermission::User);
    if (!FileSystem::isHandleValid(file))
        return { };

    bool success = static_cast<std::size_t>(FileSystem::writeToFile(file, salt.data(), salt.size())) == salt.size();
    FileSystem::closeFile(file);
    if (!success)
        return { };

    return salt;
}

std::optional<Vector<uint8_t>> readEntireFile(PlatformFileHandle handle)
{
    if (!FileSystem::isHandleValid(handle))
        return std::nullopt;

    auto size = FileSystem::fileSize(handle).value_or(0);
    if (!size)
        return std::nullopt;

    size_t bytesToRead;
    if (!WTF::convertSafely(size, bytesToRead))
        return std::nullopt;

    Vector<uint8_t> buffer(bytesToRead);
    size_t totalBytesRead = 0;
    int bytesRead;

    while ((bytesRead = FileSystem::readFromFile(handle, buffer.data() + totalBytesRead, bytesToRead - totalBytesRead)) > 0)
        totalBytesRead += bytesRead;

    if (totalBytesRead != bytesToRead)
        return std::nullopt;

    return buffer;
}

std::optional<Vector<uint8_t>> readEntireFile(const String& path)
{
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    auto contents = readEntireFile(handle);
    FileSystem::closeFile(handle);

    return contents;
}

int overwriteEntireFile(const String& path, Span<uint8_t> span)
{
    auto fileHandle = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite);
    auto closeFile = makeScopeExit([&] {
        FileSystem::closeFile(fileHandle);
    });

    if (!FileSystem::isHandleValid(fileHandle))
        return -1;

    if (!FileSystem::truncateFile(fileHandle, 0))
        return -1;

    return FileSystem::writeToFile(fileHandle, span.data(), span.size());
}

void deleteAllFilesModifiedSince(const String& directory, WallTime time)
{
    // This function may delete directory folder.
    if (time == -WallTime::infinity()) {
        deleteNonEmptyDirectory(directory);
        return;
    }

    auto children = listDirectory(directory);
    for (auto& child : children) {
        auto childPath = FileSystem::pathByAppendingComponent(directory, child);
        auto childType = fileType(childPath);
        if (!childType)
            continue;

        switch (*childType) {
        case FileType::Regular: {
            if (auto modificationTime = FileSystem::fileModificationTime(childPath); modificationTime && *modificationTime >= time)
                deleteFile(childPath);
            break;
        }
        case FileType::Directory:
            deleteAllFilesModifiedSince(childPath, time);
            deleteEmptyDirectory(childPath);
            break;
        case FileType::SymbolicLink:
            break;
        }
    }

    FileSystem::deleteEmptyDirectory(directory);
}

#if HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

bool deleteEmptyDirectory(const String& path)
{
    std::error_code ec;
    auto fsPath = toStdFileSystemPath(path);

    auto fileStatus = std::filesystem::symlink_status(fsPath, ec);
    if (ec || fileStatus.type() != std::filesystem::file_type::directory)
        return false;

#if PLATFORM(MAC)
    bool containsSingleDSStoreFile = false;
    auto entries = std::filesystem::directory_iterator(fsPath, ec);
    for (auto it = std::filesystem::begin(entries), end = std::filesystem::end(entries); !ec && it != end; it.increment(ec)) {
        if (it->path().filename() == ".DS_Store")
            containsSingleDSStoreFile = true;
        else {
            containsSingleDSStoreFile = false;
            break;
        }
    }
    if (containsSingleDSStoreFile)
        std::filesystem::remove(fsPath / ".DS_Store", ec);
#endif

    // remove() returns false on error so no need to check ec.
    return std::filesystem::remove(fsPath, ec);
}

#if !PLATFORM(PLAYSTATION)
bool moveFile(const String& oldPath, const String& newPath)
{
    auto fsOldPath = toStdFileSystemPath(oldPath);
    auto fsNewPath = toStdFileSystemPath(newPath);

    std::error_code ec;
    std::filesystem::rename(fsOldPath, fsNewPath, ec);
    if (!ec)
        return true;

    // Fall back to copying and then deleting source as rename() does not work across volumes.
    ec = { };
    std::filesystem::copy(fsOldPath, fsNewPath, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, ec);
    if (ec)
        return false;
    return std::filesystem::remove_all(fsOldPath, ec);
}
#endif

std::optional<uint64_t> fileSize(const String& path)
{
    std::error_code ec;
    auto size = std::filesystem::file_size(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    return size;
}

std::optional<uint64_t> directorySize(const String& path)
{
    if (path.isEmpty())
        return std::nullopt;

    std::error_code ec;
    auto stdPath = toStdFileSystemPath(path);
    if (!std::filesystem::is_directory(stdPath, ec))
        return std::nullopt;

    CheckedUint64 size = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(stdPath, ec)) {
        if (ec)
            return std::nullopt;
        auto filePath = fromStdFileSystemPath(entry.path());
        if (entry.is_regular_file(ec) && !ec)
            size += entry.file_size(ec);
        if (ec)
            return std::nullopt;
        if (size.hasOverflowed())
            return std::nullopt;
    }

    return size;
}

#if !PLATFORM(PLAYSTATION)
std::optional<uint64_t> volumeFreeSpace(const String& path)
{
    std::error_code ec;
    auto spaceInfo = std::filesystem::space(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    return spaceInfo.available;
}
#endif

bool createSymbolicLink(const String& targetPath, const String& symbolicLinkPath)
{
    std::error_code ec;
    std::filesystem::create_symlink(toStdFileSystemPath(targetPath), toStdFileSystemPath(symbolicLinkPath), ec);
    return !ec;
}

bool hardLink(const String& targetPath, const String& linkPath)
{
    std::error_code ec;
    std::filesystem::create_hard_link(toStdFileSystemPath(targetPath), toStdFileSystemPath(linkPath), ec);
    return !ec;
}

bool hardLinkOrCopyFile(const String& targetPath, const String& linkPath)
{
    auto fsTargetPath = toStdFileSystemPath(targetPath);
    auto fsLinkPath = toStdFileSystemPath(linkPath);

    std::error_code ec;
    std::filesystem::create_hard_link(fsTargetPath, fsLinkPath, ec);
    if (!ec)
        return true;

    std::filesystem::copy_file(fsTargetPath, fsLinkPath, ec);
    return !ec;
}

std::optional<uint64_t> hardLinkCount(const String& path)
{
    std::error_code ec;
    uint64_t linkCount = std::filesystem::hard_link_count(toStdFileSystemPath(path), ec);
    return ec ? std::nullopt : std::make_optional(linkCount);
}

#if !PLATFORM(PLAYSTATION)
bool deleteNonEmptyDirectory(const String& path)
{
    std::error_code ec;
    std::filesystem::remove_all(toStdFileSystemPath(path), ec);
    return !ec;
}
#endif

std::optional<WallTime> fileModificationTime(const String& path)
{
    std::error_code ec;
    auto modificationTime = std::filesystem::last_write_time(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    return toWallTime(modificationTime);
}

bool updateFileModificationTime(const String& path)
{
    std::error_code ec;
    std::filesystem::last_write_time(toStdFileSystemPath(path), std::filesystem::file_time_type::clock::now(), ec);
    return !ec;
}

bool isHiddenFile(const String& path)
{
#if OS(UNIX)
    auto fsPath = toStdFileSystemPath(path);
    std::filesystem::path::string_type filename = fsPath.filename();
    return !filename.empty() && filename[0] == '.';
#else
    UNUSED_PARAM(path);
    return false;
#endif
}

enum class ShouldFollowSymbolicLinks { No, Yes };
static std::optional<FileType> fileTypePotentiallyFollowingSymLinks(const String& path, ShouldFollowSymbolicLinks shouldFollowSymbolicLinks)
{
    std::error_code ec;
    auto status = shouldFollowSymbolicLinks == ShouldFollowSymbolicLinks::Yes ? std::filesystem::status(toStdFileSystemPath(path), ec) : std::filesystem::symlink_status(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    switch (status.type()) {
    case std::filesystem::file_type::directory:
        return FileType::Directory;
    case std::filesystem::file_type::symlink:
        return FileType::SymbolicLink;
    default:
        break;
    }
    return FileType::Regular;
}

std::optional<FileType> fileType(const String& path)
{
    return fileTypePotentiallyFollowingSymLinks(path, ShouldFollowSymbolicLinks::No);
}

std::optional<FileType> fileTypeFollowingSymlinks(const String& path)
{
    return fileTypePotentiallyFollowingSymLinks(path, ShouldFollowSymbolicLinks::Yes);
}

String pathFileName(const String& path)
{
    return fromStdFileSystemPath(toStdFileSystemPath(path).filename());
}

String parentPath(const String& path)
{
    return fromStdFileSystemPath(toStdFileSystemPath(path).parent_path());
}

#if !PLATFORM(PLAYSTATION)
String realPath(const String& path)
{
    std::error_code ec;
    auto canonicalPath = std::filesystem::canonical(toStdFileSystemPath(path), ec);
    return ec ? path : fromStdFileSystemPath(canonicalPath);
}
#endif

#if !PLATFORM(PLAYSTATION)
Vector<String> listDirectory(const String& path)
{
    Vector<String> fileNames;
    std::error_code ec;
    auto entries = std::filesystem::directory_iterator(toStdFileSystemPath(path), ec);
    for (auto it = std::filesystem::begin(entries), end = std::filesystem::end(entries); !ec && it != end; it.increment(ec)) {
        auto fileName = fromStdFileSystemPath(it->path().filename());
        if (!fileName.isNull())
            fileNames.append(WTFMove(fileName));
    }
    return fileNames;
}
#endif

#if !ENABLE(FILESYSTEM_POSIX_FAST_PATH)

bool fileExists(const String& path)
{
    std::error_code ec;
    // exists() returns false on error so no need to check ec.
    return std::filesystem::exists(toStdFileSystemPath(path), ec);
}

bool deleteFile(const String& path)
{
    std::error_code ec;
    auto fsPath = toStdFileSystemPath(path);

    auto fileStatus = std::filesystem::symlink_status(fsPath, ec);
    if (ec || fileStatus.type() == std::filesystem::file_type::directory)
        return false;

    // remove() returns false on error so no need to check ec.
    return std::filesystem::remove(fsPath, ec);
}

bool makeAllDirectories(const String& path)
{
    std::error_code ec;
    std::filesystem::create_directories(toStdFileSystemPath(path), ec);
    return !ec;
}

String pathByAppendingComponent(StringView path, StringView component)
{
    return fromStdFileSystemPath(toStdFileSystemPath(path) / toStdFileSystemPath(component));
}

String pathByAppendingComponents(StringView path, const Vector<StringView>& components)
{
    auto fsPath = toStdFileSystemPath(path);
    for (auto& component : components)
        fsPath /= toStdFileSystemPath(component);
    return fromStdFileSystemPath(fsPath);
}

#endif

#if !OS(WINDOWS) && !PLATFORM(COCOA) && !PLATFORM(PLAYSTATION)

String createTemporaryDirectory()
{
    std::error_code ec;
    std::string tempDir = std::filesystem::temp_directory_path(ec);
    if (ec)
        return String();

    std::string newTempDirTemplate = tempDir + "XXXXXXXX";

    Vector<char> newTempDir(newTempDirTemplate.c_str(), newTempDirTemplate.size());
    if (!mkdtemp(newTempDir.data()))
        return String();

    return stringFromFileSystemRepresentation(newTempDir.data());
}

#endif // !OS(WINDOWS) && !PLATFORM(COCOA)

#endif // HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

} // namespace WTF::FileSystemImpl
