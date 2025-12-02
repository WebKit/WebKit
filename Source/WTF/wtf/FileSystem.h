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
#include <sys/types.h>
#include <time.h>
#include <utility>
#include <wtf/FileLockMode.h>
#include <wtf/Forward.h>
#include <wtf/MallocSpan.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

#if USE(CF)
typedef const struct __CFData* CFDataRef;
#endif

OBJC_CLASS NSString;


namespace WTF {

namespace FileSystemImpl {

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

WTF_EXPORT_PRIVATE bool fileExists(const String&);
WTF_EXPORT_PRIVATE bool deleteFile(const String&);
WTF_EXPORT_PRIVATE void deleteAllFilesModifiedSince(const String&, WallTime);
WTF_EXPORT_PRIVATE bool deleteEmptyDirectory(const String&);
WTF_EXPORT_PRIVATE bool moveFile(const String& oldPath, const String& newPath);
WTF_EXPORT_PRIVATE std::optional<uint64_t> fileSize(const String&); // Follows symlinks.
WTF_EXPORT_PRIVATE std::optional<uint64_t> directorySize(const String&);
WTF_EXPORT_PRIVATE std::optional<WallTime> fileModificationTime(const String&);
WTF_EXPORT_PRIVATE bool fileIDsAreEqual(std::optional<PlatformFileID>, std::optional<PlatformFileID>);
WTF_EXPORT_PRIVATE bool updateFileModificationTime(const String& path); // Sets modification time to now.
WTF_EXPORT_PRIVATE std::optional<WallTime> fileCreationTime(const String&); // Not all platforms store file creation time.
WTF_EXPORT_PRIVATE bool isHiddenFile(const String&);
WTF_EXPORT_PRIVATE String pathByAppendingComponent(StringView path, StringView component);
WTF_EXPORT_PRIVATE String pathByAppendingComponents(StringView path, std::span<const StringView> components);
WTF_EXPORT_PRIVATE String lastComponentOfPathIgnoringTrailingSlash(const String& path);
WTF_EXPORT_PRIVATE bool makeAllDirectories(const String& path);
WTF_EXPORT_PRIVATE String pathFileName(const String&);
WTF_EXPORT_PRIVATE String parentPath(const String&);
WTF_EXPORT_PRIVATE String lexicallyNormal(const String&);
WTF_EXPORT_PRIVATE bool isAncestor(const String& first, const String& second);
WTF_EXPORT_PRIVATE std::optional<uint64_t> volumeFreeSpace(const String&);
WTF_EXPORT_PRIVATE std::optional<uint64_t> volumeCapacity(const String&);
WTF_EXPORT_PRIVATE std::optional<uint32_t> volumeFileBlockSize(const String&);
WTF_EXPORT_PRIVATE std::optional<int32_t> getFileDeviceId(const String&);
WTF_EXPORT_PRIVATE bool createSymbolicLink(const String& targetPath, const String& symbolicLinkPath);
WTF_EXPORT_PRIVATE String createTemporaryZipArchive(const String& directory);
WTF_EXPORT_PRIVATE String extractTemporaryZipArchive(const String& filePath);
WTF_EXPORT_PRIVATE FileHandle createDumpFile(StringView filename, StringView path = StringView());

enum class FileType { Regular, Directory, SymbolicLink };
WTF_EXPORT_PRIVATE std::optional<FileType> fileType(const String&);
WTF_EXPORT_PRIVATE std::optional<FileType> fileTypeFollowingSymlinks(const String&);

WTF_EXPORT_PRIVATE void setMetadataURL(const String& path, const String& urlString, const String& referrer = { });
WTF_EXPORT_PRIVATE bool setExcludedFromBackup(const String&, bool); // Returns true if successful.
WTF_EXPORT_PRIVATE bool markPurgeable(const String&);

WTF_EXPORT_PRIVATE Vector<String> listDirectory(const String& path); // Returns file names, not full paths.

WTF_EXPORT_PRIVATE CString fileSystemRepresentation(const String&);
#if !PLATFORM(WIN)
WTF_EXPORT_PRIVATE String stringFromFileSystemRepresentation(const char*);
#endif

using Salt = std::array<uint8_t, 8>;
WTF_EXPORT_PRIVATE std::optional<Salt> readOrMakeSalt(const String& path);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> readEntireFile(const String& path);
WTF_EXPORT_PRIVATE std::optional<uint64_t> overwriteEntireFile(const String& path, std::span<const uint8_t>);

// Prefix is what the filename should be prefixed with, not the full path.
WTF_EXPORT_PRIVATE std::pair<String, FileHandle> openTemporaryFile(StringView prefix, StringView suffix = { });
WTF_EXPORT_PRIVATE String createTemporaryFile(StringView prefix, StringView suffix = { });
#if PLATFORM(COCOA)
WTF_EXPORT_PRIVATE std::pair<FileHandle, CString> createTemporaryFileInDirectory(const String& directory, const String& suffix);
#endif
WTF_EXPORT_PRIVATE FileHandle openFile(const String& path, FileOpenMode, FileAccessPermission = FileAccessPermission::All, OptionSet<FileLockMode> = { }, bool failIfFileExists = false);

WTF_EXPORT_PRIVATE bool hardLink(const String& targetPath, const String& linkPath);
// Hard links a file if possible, copies it if not.
WTF_EXPORT_PRIVATE bool hardLinkOrCopyFile(const String& targetPath, const String& linkPath);
WTF_EXPORT_PRIVATE std::optional<uint64_t> hardLinkCount(const String& path);
WTF_EXPORT_PRIVATE bool copyFile(const String& targetPath, const String& sourcePath);

// Encode a string for use within a file name.
WTF_EXPORT_PRIVATE String encodeForFileName(const String&);
WTF_EXPORT_PRIVATE String decodeFromFilename(const String&);

WTF_EXPORT_PRIVATE bool filesHaveSameVolume(const String&, const String&);

#if USE(CF)
WTF_EXPORT_PRIVATE RetainPtr<CFURLRef> pathAsURL(const String&);
#endif

#if USE(GLIB)
WTF_EXPORT_PRIVATE String filenameForDisplay(const String&);
WTF_EXPORT_PRIVATE CString currentExecutablePath();
WTF_EXPORT_PRIVATE CString currentExecutableName();
WTF_EXPORT_PRIVATE String userCacheDirectory();
WTF_EXPORT_PRIVATE String userDataDirectory();
WTF_EXPORT_PRIVATE String createTemporaryDirectory(const String& directoryPrefix = nullString());
#if ENABLE(DEVELOPER_MODE)
WTF_EXPORT_PRIVATE CString webkitTopLevelDirectory();
#endif
#endif // USE(GLIB)

#if OS(WINDOWS)
WTF_EXPORT_PRIVATE String localUserSpecificStorageDirectory();
WTF_EXPORT_PRIVATE String roamingUserSpecificStorageDirectory();
WTF_EXPORT_PRIVATE String createTemporaryDirectory();
#endif

#if PLATFORM(COCOA)
WTF_EXPORT_PRIVATE NSString *createTemporaryDirectory(NSString *directoryPrefix = nil);
WTF_EXPORT_PRIVATE NSString *systemDirectoryPath();

// Allow reading cloud files with no local copy.
enum class PolicyScope : uint8_t { Process, Thread };
WTF_EXPORT_PRIVATE bool setAllowsMaterializingDatalessFiles(bool, PolicyScope);
WTF_EXPORT_PRIVATE std::optional<bool> allowsMaterializingDatalessFiles(PolicyScope);
#endif

// Impl for systems that do not already have createTemporaryDirectory
#if !OS(WINDOWS) && !PLATFORM(COCOA) && !PLATFORM(PLAYSTATION) && !PLATFORM(GLIB)
WTF_EXPORT_PRIVATE String createTemporaryDirectory();
#endif

WTF_EXPORT_PRIVATE bool deleteNonEmptyDirectory(const String&);

WTF_EXPORT_PRIVATE String realPath(const String&);

WTF_EXPORT_PRIVATE bool isSafeToUseMemoryMapForPath(const String&);
WTF_EXPORT_PRIVATE WARN_UNUSED_RETURN bool makeSafeToUseMemoryMapForPath(const String&);

WTF_EXPORT_PRIVATE std::optional<MappedFileData> mapFile(const String& path, MappedFileMode);

// This creates the destination file, maps it, write the provided data to it and returns the mapped file.
// This function fails if there is already a file at the destination path.
WTF_EXPORT_PRIVATE MappedFileData mapToFile(const String& path, size_t bytesSize, NOESCAPE const Function<void(const Function<bool(std::span<const uint8_t>)>&)>& apply, FileHandle* = nullptr);

WTF_EXPORT_PRIVATE MappedFileData createMappedFileData(const String&, size_t, FileHandle* = nullptr);
WTF_EXPORT_PRIVATE void finalizeMappedFileData(MappedFileData&, size_t);

} // namespace FileSystemImpl
} // namespace WTF

namespace FileSystem = WTF::FileSystemImpl;
