/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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
#include <wtf/FileHandle.h>
#include <wtf/Function.h>
#include <wtf/HexNumber.h>
#include <wtf/Logging.h>
#include <wtf/MappedFileData.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>

#if !OS(WINDOWS)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)
#include <wtf/StdFilesystem.h>
#endif

namespace WTF::FileSystemImpl {

#if HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

static std::filesystem::path toStdFileSystemPath(StringView path)
{
#if HAVE(MISSING_U8STRING)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return std::filesystem::u8path(path.utf8().data());
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return { std::u8string(path.utf8().data()) };
#endif
}

static RefPtr<StringImpl> fromStdFileSystemPath(const std::filesystem::path& path)
{
#if HAVE(MISSING_U8STRING)
    // FIXME: This uses u8string so how can it be correct inside HAVE(MISSING_U8STRING)?
    return StringImpl::fromUTF8(unsafeSpan(path.u8string().c_str()));
#else
    return StringImpl::fromUTF8(span(path.u8string()));
#endif
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

constexpr std::array<bool, 128> needsEscaping = {
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

NODELETE static inline bool shouldEscapeChar16(char16_t character, char16_t previousCharacter, char16_t nextCharacter)
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

RefPtr<StringImpl> encodeForFileName(StringImpl* inputString)
{
    unsigned length = WTF::length(inputString);
    if (!length)
        return inputString;

    StringBuilder result;
    result.reserveCapacity(length);

    char16_t previousCharacter = 0;
    char16_t nextCharacter = codeUnitAt(inputString, 0);
    for (unsigned i = 0; i < length; ++i) {
        auto character = std::exchange(nextCharacter, i + 1 < length ? codeUnitAt(inputString, i + 1) : 0);
        if (shouldEscapeChar16(character, previousCharacter, nextCharacter)) {
            if (character <= 0xFF)
                result.append('%', hex(character, 2));
            else
                result.append("%+"_s, hex(static_cast<uint8_t>(character >> 8), 2), hex(static_cast<uint8_t>(character), 2));
        } else
            result.append(character);
        previousCharacter = character;
    }

    return result.toString();
}

RefPtr<StringImpl> decodeFromFilename(StringImpl* inputString)
{
    unsigned length = WTF::length(inputString);
    if (!length)
        return inputString;

    StringBuilder result;
    result.reserveCapacity(length);

    for (unsigned i = 0; i < length; ++i) {
        if (codeUnitAt(inputString, i) != '%') {
            result.append(codeUnitAt(inputString, i));
            continue;
        }

        // If the input string is a valid encoded filename, it must be at least 2 characters longer
        // than the current index to account for this percent encoded value.
        if (i + 2 >= length)
            return { };

        if (codeUnitAt(inputString, i+1) != '+') {
            if (!isASCIIHexDigit(codeUnitAt(inputString, i + 1)))
                return { };
            if (!isASCIIHexDigit(codeUnitAt(inputString, i + 2)))
                return { };
            result.append(byteCast<Latin1Character>(toASCIIHexValue(codeUnitAt(inputString, i + 1), codeUnitAt(inputString, i + 2))));
            i += 2;
            continue;
        }

        // If the input string is a valid encoded filename, it must be at least 5 characters longer
        // than the current index to account for this percent encoded value.
        if (i + 5 >= length)
            return { };

        if (!isASCIIHexDigit(codeUnitAt(inputString, i + 2)))
            return { };
        if (!isASCIIHexDigit(codeUnitAt(inputString, i + 3)))
            return { };
        if (!isASCIIHexDigit(codeUnitAt(inputString, i + 4)))
            return { };
        if (!isASCIIHexDigit(codeUnitAt(inputString, i + 5)))
            return { };

        char16_t encodedCharacter = toASCIIHexValue(codeUnitAt(inputString, i + 2), codeUnitAt(inputString, i + 3)) << 8 | toASCIIHexValue(codeUnitAt(inputString, i + 4), codeUnitAt(inputString, i + 5));
        result.append(encodedCharacter);
        i += 5;
    }

    return result.toString();
}

RefPtr<StringImpl> lastComponentOfPathIgnoringTrailingSlash(StringImpl* path)
{
    auto position = reverseFind(path, pathSeparator);
    if (position == notFound)
        return path;

    size_t endOfSubstring = length(path) - 1;
    if (position == endOfSubstring) {
        --endOfSubstring;
        position = reverseFind(path, pathSeparator, endOfSubstring);
    }

    return substring(path, position + 1, endOfSubstring - position);
}

void removeTrailingSlash(RefPtr<StringImpl>& path)
{
    if (length(path) > 1 && endsWith(path, FileSystem::pathSeparator))
        path = left(path, length(path) - 1);
}

bool filesHaveSameVolume(StringImpl* fileA, StringImpl* fileB)
{
    if (!fileA || !fileB)
        return false;

    bool result = false;

    auto fileADev = getFileDeviceId(fileA);
    auto fileBDev = getFileDeviceId(fileB);

    if (fileADev && fileBDev)
        result = (fileADev == fileBDev);

    return result;
}

#if !PLATFORM(MAC)

void setMetadataURL(StringImpl*, StringImpl*, StringImpl*)
{
}

#endif

#if !PLATFORM(IOS_FAMILY)
bool isSafeToUseMemoryMapForPath(StringImpl*)
{
    return true;
}

bool makeSafeToUseMemoryMapForPath(StringImpl*)
{
    return true;
}
#endif

#if !PLATFORM(COCOA)

RefPtr<StringImpl> createTemporaryZipArchive(StringImpl*)
{
    return { };
}

RefPtr<StringImpl> extractTemporaryZipArchive(StringImpl*)
{
    return { };
}

bool setExcludedFromBackup(StringImpl*, bool)
{
    return false;
}

bool markPurgeable(StringImpl*)
{
    return false;
}

#endif

std::optional<MappedFileData> mapFile(StringImpl* filePath, MappedFileMode mode)
{
    auto handle = openFile(filePath, FileSystem::FileOpenMode::Read);
    if (!handle)
        return { };
    return handle.map(mode);
}

MappedFileData createMappedFileData(StringImpl* path, size_t bytesSize, FileHandle* outputHandle)
{
    constexpr bool failIfFileExists = true;
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite, FileSystem::FileAccessPermission::User, { }, failIfFileExists);

    if (!handle)
        return { };

    bool succeeded = false;
    auto removeOrphanOnFailure = makeScopeExit([&] {
        if (!succeeded)
            FileSystem::deleteFile(path);
    });

    if (!handle.truncate(bytesSize)) {
        RELEASE_LOG_FAULT(MemoryPressure, "Unable to truncate file");
        return { };
    }

#if HAVE(FALLOCATE)
    // Reserve real blocks so a later mmap'd memcpy() can't SIGBUS on ENOSPC.
    // EOPNOTSUPP: filesystem doesn't support pre-allocation -> preserve old behavior.
    // posix_fallocate is avoided because it falls back to zero-filling when pre-allocation
    // is not supported by the FS.
    if (fallocate(handle.platformHandle(), 0, 0, bytesSize) == -1 && errno != EOPNOTSUPP) {
        RELEASE_LOG_ERROR(MemoryPressure, "Unable to reserve %zu bytes for cache file (errno=%d)", bytesSize, errno);
        return { };
    }
#endif

    if (!FileSystem::makeSafeToUseMemoryMapForPath(path))
        return { };

    auto mappedFile = handle.map(FileSystem::MappedFileMode::Shared, FileSystem::FileOpenMode::ReadWrite);
    if (!mappedFile)
        return { };

    if (outputHandle)
        *outputHandle = WTF::move(handle);

    succeeded = true;
    return WTF::move(*mappedFile);
}

void finalizeMappedFileData(MappedFileData& mappedFileData, size_t bytesSize)
{
    auto* map = mappedFileData.mutableSpan().data();
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

MappedFileData mapToFile(StringImpl* path, size_t bytesSize, NOESCAPE const Function<void(const Function<bool(std::span<const uint8_t>)>&)>& apply, FileHandle* outputHandle)
{
    auto mappedFile = createMappedFileData(path, bytesSize, outputHandle);
    if (!mappedFile)
        return { };

    auto mapData = mappedFile.mutableSpan();

    apply([&mapData](std::span<const uint8_t> chunk) {
        memcpySpan(consumeSpan(mapData, chunk.size()), chunk);
        return true;
    });

    finalizeMappedFileData(mappedFile, bytesSize);

    return mappedFile;
}

static Salt makeSalt()
{
    Salt salt;
    cryptographicallyRandomValues(salt);
    return salt;
}

std::optional<Salt> readOrMakeSalt(StringImpl* path)
{
    if (FileSystem::fileExists(path)) {
        if (auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Read); handle) {
            Salt salt;
            auto bytesRead = handle.read(salt);
            if (bytesRead == salt.size())
                return salt;
        }
        FileSystem::deleteFile(path);
    }

    Salt salt = makeSalt();
    FileSystem::makeAllDirectories(FileSystem::parentPath(path));
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Truncate, FileSystem::FileAccessPermission::User);
    if (!handle)
        return { };

    bool success = handle.write(salt) == salt.size();
    if (!success)
        return { };

    return salt;
}

std::optional<Vector<uint8_t>> readEntireFile(StringImpl* path)
{
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    return handle.readAll();
}

std::optional<uint64_t> overwriteEntireFile(StringImpl* path, std::span<const uint8_t> span)
{
    auto fileHandle = FileSystem::openFile(path, FileSystem::FileOpenMode::Truncate);
    if (!fileHandle)
        return { };

    return fileHandle.write(span);
}

void deleteAllFilesModifiedSince(StringImpl* directory, WallTime time)
{
    // This function may delete directory folder.
    if (time == -WallTime::infinity()) {
        deleteNonEmptyDirectory(directory);
        return;
    }

    traverseDirectory(directory, [&](StringImpl* child, FileType childType) {
        auto childPath = FileSystem::pathByAppendingComponent(directory, child);
        switch (childType) {
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
    });

    FileSystem::deleteEmptyDirectory(directory);
}

#if HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

bool deleteEmptyDirectory(StringImpl* path)
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
bool moveFile(StringImpl* oldPath, StringImpl* newPath)
{
    auto fsOldPath = toStdFileSystemPath(oldPath);
    auto fsNewPath = toStdFileSystemPath(newPath);

    std::error_code ec;
    std::filesystem::rename(fsOldPath, fsNewPath, ec);
    if (!ec)
        return true;

    if (isAncestor(oldPath, newPath))
        return false;

    // Fall back to copying and then deleting source as rename() does not work across volumes.
    ec = { };
    std::filesystem::copy(fsOldPath, fsNewPath, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, ec);
    if (ec)
        return false;
    return std::filesystem::remove_all(fsOldPath, ec);
}
#endif

int statFile(std::span<const char> pathIncludingNullTerminator, struct stat& result)
{
    RELEASE_ASSERT(!pathIncludingNullTerminator.empty() && !pathIncludingNullTerminator.back());
    return stat(pathIncludingNullTerminator.data(), &result);
}

std::optional<uint64_t> fileSize(StringImpl* path)
{
    std::error_code ec;
    auto size = std::filesystem::file_size(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    return size;
}

std::optional<uint64_t> directorySize(StringImpl* path)
{
    if (isEmpty(path))
        return std::nullopt;

    std::error_code ec;
    auto stdPath = toStdFileSystemPath(path);
    if (!std::filesystem::is_directory(stdPath, ec))
        return std::nullopt;

    CheckedUint64 size = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(stdPath, ec)) {
        if (ec)
            return std::nullopt;
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
std::optional<uint64_t> volumeFreeSpace(StringImpl* path)
{
    std::error_code ec;
    auto spaceInfo = std::filesystem::space(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    return spaceInfo.available;
}

std::optional<uint64_t> volumeCapacity(StringImpl* path)
{
    std::error_code ec;
    auto spaceInfo = std::filesystem::space(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    return spaceInfo.capacity;
}
#endif

bool createSymbolicLink(StringImpl* targetPath, StringImpl* symbolicLinkPath)
{
    std::error_code ec;
    std::filesystem::create_symlink(toStdFileSystemPath(targetPath), toStdFileSystemPath(symbolicLinkPath), ec);
    return !ec;
}

bool hardLink(StringImpl* targetPath, StringImpl* linkPath)
{
    std::error_code ec;
    std::filesystem::create_hard_link(toStdFileSystemPath(targetPath), toStdFileSystemPath(linkPath), ec);
    return !ec;
}

bool hardLinkOrCopyFile(StringImpl* targetPath, StringImpl* linkPath)
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

bool copyFile(StringImpl* targetPath, StringImpl* sourcePath)
{
    auto fsTargetPath = toStdFileSystemPath(targetPath);
    auto fsSourcePath = toStdFileSystemPath(sourcePath);

    std::error_code ec;
    std::filesystem::copy_file(fsSourcePath, fsTargetPath, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

std::optional<uint64_t> hardLinkCount(StringImpl* path)
{
    std::error_code ec;
    uint64_t linkCount = std::filesystem::hard_link_count(toStdFileSystemPath(path), ec);
    return ec ? std::nullopt : std::make_optional(linkCount);
}

#if !PLATFORM(PLAYSTATION)
bool deleteNonEmptyDirectory(StringImpl* path)
{
    std::error_code ec;
    std::filesystem::remove_all(toStdFileSystemPath(path), ec);
    return !ec;
}
#endif

std::optional<WallTime> fileModificationTime(StringImpl* path)
{
    std::error_code ec;
    auto modificationTime = std::filesystem::last_write_time(toStdFileSystemPath(path), ec);
    if (ec)
        return std::nullopt;
    return toWallTime(modificationTime);
}

bool updateFileModificationTime(StringImpl* path)
{
    std::error_code ec;
    std::filesystem::last_write_time(toStdFileSystemPath(path), std::filesystem::file_time_type::clock::now(), ec);
    return !ec;
}

bool isHiddenFile(StringImpl* path)
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

enum class ShouldFollowSymbolicLinks : bool { No, Yes };
static std::optional<FileType> fileTypePotentiallyFollowingSymLinks(StringImpl* path, ShouldFollowSymbolicLinks shouldFollowSymbolicLinks)
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

std::optional<FileType> fileType(StringImpl* path)
{
    return fileTypePotentiallyFollowingSymLinks(path, ShouldFollowSymbolicLinks::No);
}

std::optional<FileType> fileTypeFollowingSymlinks(StringImpl* path)
{
    return fileTypePotentiallyFollowingSymLinks(path, ShouldFollowSymbolicLinks::Yes);
}

RefPtr<StringImpl> pathFileName(StringImpl* path)
{
    return fromStdFileSystemPath(toStdFileSystemPath(path).filename());
}

RefPtr<StringImpl> parentPath(StringImpl* path)
{
    return fromStdFileSystemPath(toStdFileSystemPath(path).parent_path());
}

RefPtr<StringImpl> lexicallyNormal(StringImpl* path)
{
    return fromStdFileSystemPath(toStdFileSystemPath(path).lexically_normal());
}

bool isAncestor(StringImpl* possibleAncestor, StringImpl* possibleChild)
{
    auto possibleChildLexicallyNormal = lexicallyNormal(possibleChild);
    auto possibleAncestorLexicallyNormal = lexicallyNormal(possibleAncestor);
    removeTrailingSlash(possibleChildLexicallyNormal);
    removeTrailingSlash(possibleAncestorLexicallyNormal);
    return startsWith(possibleChildLexicallyNormal, possibleAncestorLexicallyNormal)
        && length(possibleChildLexicallyNormal) > length(possibleAncestorLexicallyNormal)
        && codeUnitAt(possibleChildLexicallyNormal, length(possibleAncestorLexicallyNormal)) == static_cast<char16_t>(std::filesystem::path::preferred_separator);
}

RefPtr<StringImpl> createTemporaryFile(StringView prefix, StringView suffix)
{
    auto [path, handle] = openTemporaryFile(prefix, suffix);
    return path;
}

FileHandle createDumpFile(StringView filename, StringView extension, StringView path)
{
    if (path.isEmpty()) {
        auto [p, handle] = openTemporaryFile(filename, extension);
        return WTF::move(handle);
    }
    // Why ReadWrite? On linux, we need to mmap dump files so that perf can see them, which will fail without read permission.
    return openFile(makeString(path, pathSeparator, filename, extension), FileOpenMode::ReadWrite);
}

#if !PLATFORM(PLAYSTATION)
RefPtr<StringImpl> realPath(StringImpl* path)
{
    std::error_code ec;
    auto canonicalPath = std::filesystem::canonical(toStdFileSystemPath(path), ec);
    return ec ? RefPtr { path } : fromStdFileSystemPath(canonicalPath);
}
#endif

#if !PLATFORM(PLAYSTATION)
Vector<RefPtr<StringImpl>> listDirectory(StringImpl* path)
{
    Vector<RefPtr<StringImpl>> fileNames;
    std::error_code ec;
    auto entries = std::filesystem::directory_iterator(toStdFileSystemPath(path), ec);
    for (auto it = std::filesystem::begin(entries), end = std::filesystem::end(entries); !ec && it != end; it.increment(ec)) {
        auto fileName = fromStdFileSystemPath(it->path().filename());
        if (fileName)
            fileNames.append(WTF::move(fileName));
    }
    return fileNames;
}

void traverseDirectory(StringImpl* path, NOESCAPE const Function<void(StringImpl*, FileType)>& function)
{
    std::error_code ec;
    auto entries = std::filesystem::directory_iterator(toStdFileSystemPath(path), ec);
    for (auto it = std::filesystem::begin(entries), end = std::filesystem::end(entries); !ec && it != end; it.increment(ec)) {
        auto fileName = fromStdFileSystemPath(it->path().filename());
        if (!fileName)
            continue;
        std::error_code statusEC;
        auto status = it->symlink_status(statusEC);
        if (statusEC)
            continue;
        switch (status.type()) {
        case std::filesystem::file_type::directory:
            function(fileName, FileType::Directory);
            break;
        case std::filesystem::file_type::symlink:
            function(fileName, FileType::SymbolicLink);
            break;
        default:
            function(fileName, FileType::Regular);
            break;
        }
    }
}
#endif

#if !ENABLE(FILESYSTEM_POSIX_FAST_PATH)

bool fileExists(StringImpl* path)
{
    std::error_code ec;
    // exists() returns false on error so no need to check ec.
    return std::filesystem::exists(toStdFileSystemPath(path), ec);
}

bool deleteFile(StringImpl* path)
{
    std::error_code ec;
    auto fsPath = toStdFileSystemPath(path);

    auto fileStatus = std::filesystem::symlink_status(fsPath, ec);
    if (ec || fileStatus.type() == std::filesystem::file_type::directory)
        return false;

    // remove() returns false on error so no need to check ec.
    return std::filesystem::remove(fsPath, ec);
}

bool makeAllDirectories(StringImpl* path)
{
    std::error_code ec;
    std::filesystem::create_directories(toStdFileSystemPath(path), ec);
    return !ec;
}

RefPtr<StringImpl> pathByAppendingComponent(StringView path, StringView component)
{
    return fromStdFileSystemPath(toStdFileSystemPath(path) / toStdFileSystemPath(component));
}

RefPtr<StringImpl> pathByAppendingComponents(StringView path, std::span<const StringView> components)
{
    auto fsPath = toStdFileSystemPath(path);
    for (auto& component : components)
        fsPath /= toStdFileSystemPath(component);
    return fromStdFileSystemPath(fsPath);
}

#endif

#if !OS(WINDOWS) && !PLATFORM(COCOA) && !PLATFORM(PLAYSTATION) && !PLATFORM(GLIB)

RefPtr<StringImpl> createTemporaryDirectory()
{
    std::error_code ec;
    std::string tempDir = std::filesystem::temp_directory_path(ec);
    if (ec)
        return nullptr;

    std::string newTempDirTemplate = tempDir + "XXXXXXXX";

    Vector<char> newTempDir(std::span<const char> { newTempDirTemplate });
    if (!mkdtemp(newTempDir.mutableSpan().data()))
        return nullptr;

    return stringFromFileSystemRepresentation(newTempDir.span().data());
}

#endif // !OS(WINDOWS) && !PLATFORM(COCOA)

#endif // HAVE(STD_FILESYSTEM) || HAVE(STD_EXPERIMENTAL_FILESYSTEM)

} // namespace WTF::FileSystemImpl
