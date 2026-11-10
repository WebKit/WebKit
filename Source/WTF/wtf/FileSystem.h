/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#pragma once

#include <span>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <utility>
#include <wtf/FileLockMode.h>
#include <wtf/Forward.h>
#include <wtf/MallocSpan.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/UTF8CStringView.h>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <windows.h>
#endif

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

#if USE(CF)
typedef const struct __CFData* CFDataRef;
#endif

#if !OS(WINDOWS)
struct stat;
#endif

OBJC_CLASS NSString;

namespace WTF::FileSystemImpl {

class FileHandle;
class MappedFileData;

enum class MappedFileMode : bool;

#if OS(WINDOWS)
typedef FILE_ID_128 PlatformFileID;
#else
typedef ino_t PlatformFileID;
#endif

enum class FileOpenMode : uint8_t {
    Read,
    Truncate,
    ReadWrite,
#if OS(DARWIN)
    EventsOnly,
#endif
};

enum class FileAccessPermission : bool {
    User,
    All
};

#if OS(WINDOWS)
constexpr char pathSeparator = '\\';
#else
constexpr char pathSeparator = '/';
#endif

WTF_EXPORT_PRIVATE bool fileExists(StringImpl*);
WTF_EXPORT_PRIVATE bool deleteFile(StringImpl*);
WTF_EXPORT_PRIVATE void deleteAllFilesModifiedSince(StringImpl*, WallTime);
WTF_EXPORT_PRIVATE bool deleteEmptyDirectory(StringImpl*);
WTF_EXPORT_PRIVATE bool moveFile(StringImpl* oldPath, StringImpl* newPath);
WTF_EXPORT_PRIVATE std::optional<uint64_t> fileSize(StringImpl*); // Follows symlinks.
WTF_EXPORT_PRIVATE std::optional<uint64_t> directorySize(StringImpl*);
WTF_EXPORT_PRIVATE std::optional<WallTime> fileModificationTime(StringImpl*);
NODELETE WTF_EXPORT_PRIVATE bool fileIDsAreEqual(std::optional<PlatformFileID>, std::optional<PlatformFileID>);
WTF_EXPORT_PRIVATE bool updateFileModificationTime(StringImpl* path); // Sets modification time to now.
WTF_EXPORT_PRIVATE std::optional<WallTime> fileCreationTime(StringImpl*); // Not all platforms store file creation time.
WTF_EXPORT_PRIVATE bool isHiddenFile(StringImpl*);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> pathByAppendingComponent(StringView path, StringView component);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> pathByAppendingComponents(StringView path, std::span<const StringView> components);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> lastComponentOfPathIgnoringTrailingSlash(StringImpl* path);
WTF_EXPORT_PRIVATE void removeTrailingSlash(RefPtr<StringImpl>& path);
WTF_EXPORT_PRIVATE bool makeAllDirectories(StringImpl* path);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> pathFileName(StringImpl*);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> parentPath(StringImpl*);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> lexicallyNormal(StringImpl*);
WTF_EXPORT_PRIVATE bool isAncestor(StringImpl* first, StringImpl* second);
WTF_EXPORT_PRIVATE std::optional<uint64_t> volumeFreeSpace(StringImpl*);
WTF_EXPORT_PRIVATE std::optional<uint64_t> volumeCapacity(StringImpl*);
WTF_EXPORT_PRIVATE std::optional<uint32_t> volumeFileBlockSize(StringImpl*);
WTF_EXPORT_PRIVATE std::optional<int32_t> getFileDeviceId(StringImpl*);
WTF_EXPORT_PRIVATE bool createSymbolicLink(StringImpl* targetPath, StringImpl* symbolicLinkPath);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> createTemporaryZipArchive(StringImpl* directory);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> extractTemporaryZipArchive(StringImpl* filePath);
WTF_EXPORT_PRIVATE FileHandle createDumpFile(StringView filename, StringView extension, StringView path = StringView());

enum class FileType { Regular, Directory, SymbolicLink };
WTF_EXPORT_PRIVATE std::optional<FileType> fileType(StringImpl*);
WTF_EXPORT_PRIVATE std::optional<FileType> fileTypeFollowingSymlinks(StringImpl*);

WTF_EXPORT_PRIVATE void setMetadataURL(StringImpl* path, StringImpl* urlString, StringImpl* referrer = nullptr);
WTF_EXPORT_PRIVATE bool setExcludedFromBackup(StringImpl*, bool); // Returns true if successful.
WTF_EXPORT_PRIVATE bool markPurgeable(StringImpl*);

WTF_EXPORT_PRIVATE Vector<RefPtr<StringImpl>> listDirectory(StringImpl* path); // Returns file names, not full paths.
WTF_EXPORT_PRIVATE void traverseDirectory(StringImpl* path, NOESCAPE const Function<void(StringImpl* fileName, FileType)>&);

WTF_EXPORT_PRIVATE UTF8CString fileSystemRepresentation(StringImpl*);
#if !PLATFORM(WIN)
WTF_EXPORT_PRIVATE RefPtr<StringImpl> stringFromFileSystemRepresentation(UTF8CStringView);
inline RefPtr<StringImpl> stringFromFileSystemRepresentation(const char* path) { return stringFromFileSystemRepresentation(UTF8CStringView::unsafeFromUTF8(path)); }
#endif

// stat() needs a null-terminated path, so these take the span including the terminator and check for it.
WTF_EXPORT_PRIVATE int statFile(std::span<const char> pathIncludingNullTerminator, struct stat&);
inline int statFile(std::span<const char8_t> pathIncludingNullTerminator, struct stat& result) { return statFile(byteCast<char>(pathIncludingNullTerminator), result); }

using Salt = std::array<uint8_t, 8>;
WTF_EXPORT_PRIVATE std::optional<Salt> readOrMakeSalt(StringImpl* path);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> readEntireFile(StringImpl* path);
WTF_EXPORT_PRIVATE std::optional<uint64_t> overwriteEntireFile(StringImpl* path, std::span<const uint8_t>);

// Prefix is what the filename should be prefixed with, not the full path.
WTF_EXPORT_PRIVATE std::pair<RefPtr<StringImpl>, FileHandle> openTemporaryFile(StringView prefix, StringView suffix = { }, StringImpl* temporaryDirectory = nullptr);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> createTemporaryFile(StringView prefix, StringView suffix = { });
#if PLATFORM(COCOA)
WTF_EXPORT_PRIVATE std::pair<FileHandle, RefPtr<StringImpl>> createTemporaryFileInDirectory(StringImpl* directory, StringImpl* suffix);
#endif
WTF_EXPORT_PRIVATE FileHandle openFile(StringImpl* path, FileOpenMode, FileAccessPermission = FileAccessPermission::All, OptionSet<FileLockMode> = { }, bool failIfFileExists = false);

WTF_EXPORT_PRIVATE bool hardLink(StringImpl* targetPath, StringImpl* linkPath);
// Hard links a file if possible, copies it if not.
WTF_EXPORT_PRIVATE bool hardLinkOrCopyFile(StringImpl* targetPath, StringImpl* linkPath);
WTF_EXPORT_PRIVATE std::optional<uint64_t> hardLinkCount(StringImpl* path);
WTF_EXPORT_PRIVATE bool copyFile(StringImpl* targetPath, StringImpl* sourcePath);

// Encode a string for use within a file name.
WTF_EXPORT_PRIVATE RefPtr<StringImpl> encodeForFileName(StringImpl*);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> decodeFromFilename(StringImpl*);

WTF_EXPORT_PRIVATE bool filesHaveSameVolume(StringImpl*, StringImpl*);

#if USE(CF)
WTF_EXPORT_PRIVATE RetainPtr<CFURLRef> pathAsURL(StringImpl*);
#endif

// The name of the running executable, as reported by the platform.
#if USE(GLIB) || PLATFORM(COCOA)
WTF_EXPORT_PRIVATE UTF8CString currentExecutableName();
#endif

#if USE(GLIB)
WTF_EXPORT_PRIVATE RefPtr<StringImpl> filenameForDisplay(StringImpl*);
WTF_EXPORT_PRIVATE UTF8CString currentExecutablePath();
WTF_EXPORT_PRIVATE RefPtr<StringImpl> userCacheDirectory();
WTF_EXPORT_PRIVATE RefPtr<StringImpl> userDataDirectory();
WTF_EXPORT_PRIVATE RefPtr<StringImpl> createTemporaryDirectory(StringImpl* directoryPrefix = nullptr);
#if ENABLE(DEVELOPER_MODE)
WTF_EXPORT_PRIVATE UTF8CString webkitTopLevelDirectory();
#endif
#endif // USE(GLIB)

#if OS(WINDOWS)
WTF_EXPORT_PRIVATE RefPtr<StringImpl> localUserSpecificStorageDirectory();
WTF_EXPORT_PRIVATE RefPtr<StringImpl> roamingUserSpecificStorageDirectory();
WTF_EXPORT_PRIVATE RefPtr<StringImpl> createTemporaryDirectory();
#endif

#if PLATFORM(COCOA)
WTF_EXPORT_PRIVATE NSString *createTemporaryDirectory(NSString *directoryPrefix = nil);
WTF_EXPORT_PRIVATE NSString *systemDirectoryPath();

WTF_EXPORT_PRIVATE RefPtr<StringImpl> darwinCacheDirectory();
WTF_EXPORT_PRIVATE RefPtr<StringImpl> darwinTempDirectory();

// Allow reading cloud files with no local copy.
enum class PolicyScope : uint8_t { Process, Thread };
WTF_EXPORT_PRIVATE bool setAllowsMaterializingDatalessFiles(bool, PolicyScope);
WTF_EXPORT_PRIVATE std::optional<bool> allowsMaterializingDatalessFiles(PolicyScope);
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
WTF_EXPORT_PRIVATE std::optional<RefPtr<StringImpl>> homeDirectory();
#endif

// Impl for systems that do not already have createTemporaryDirectory
#if !OS(WINDOWS) && !PLATFORM(COCOA) && !PLATFORM(PLAYSTATION) && !PLATFORM(GLIB)
WTF_EXPORT_PRIVATE RefPtr<StringImpl> createTemporaryDirectory();
#endif

WTF_EXPORT_PRIVATE bool deleteNonEmptyDirectory(StringImpl*);

WTF_EXPORT_PRIVATE RefPtr<StringImpl> realPath(StringImpl*);

WTF_EXPORT_PRIVATE bool isSafeToUseMemoryMapForPath(StringImpl*);
[[nodiscard]] WTF_EXPORT_PRIVATE bool makeSafeToUseMemoryMapForPath(StringImpl*);

WTF_EXPORT_PRIVATE std::optional<MappedFileData> mapFile(StringImpl* path, MappedFileMode);

// This creates the destination file, maps it, write the provided data to it and returns the mapped file.
// This function fails if there is already a file at the destination path.
WTF_EXPORT_PRIVATE MappedFileData mapToFile(StringImpl* path, size_t bytesSize, NOESCAPE const Function<void(const Function<bool(std::span<const uint8_t>)>&)>& apply, FileHandle* = nullptr);

WTF_EXPORT_PRIVATE MappedFileData createMappedFileData(StringImpl*, size_t, FileHandle* = nullptr);
WTF_EXPORT_PRIVATE void finalizeMappedFileData(MappedFileData&, size_t);

// Purely lexical: unlike isAncestor(), "." and ".." components are not resolved.
inline bool pathIsSameOrDescendantOf(StringView path, StringView ancestor)
{
    if (ancestor.length() > 1 && ancestor.endsWith('/'))
        ancestor = ancestor.left(ancestor.length() - 1);
    if (ancestor.isEmpty() || !path.startsWith(ancestor))
        return false;
    return path.length() == ancestor.length() || ancestor.endsWith('/') || path[ancestor.length()] == '/';
}

} // namespace WTF::FileSystemImpl

namespace FileSystem = WTF::FileSystemImpl;
