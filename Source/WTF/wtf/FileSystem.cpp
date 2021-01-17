/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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

#include <wtf/FileMetadata.h>
#include <wtf/HexNumber.h>
#include <wtf/Scope.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if !OS(WINDOWS)
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#if PLATFORM(HAIKU)
#define MAP_FILE 0
#endif

#if USE(GLIB)
#include <gio/gfiledescriptorbased.h>
#include <gio/gio.h>
#endif

namespace WTF {

namespace FileSystemImpl {

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

String encodeForFileName(const String& inputString)
{
    unsigned length = inputString.length();
    if (!length)
        return inputString;

    StringBuilder result;
    result.reserveCapacity(length);

    UChar previousCharacter;
    UChar character = 0;
    UChar nextCharacter = inputString[0];
    for (unsigned i = 0; i < length; ++i) {
        previousCharacter = character;
        character = nextCharacter;
        nextCharacter = i + 1 < length ? inputString[i + 1] : 0;

        if (shouldEscapeUChar(character, previousCharacter, nextCharacter)) {
            if (character <= 255) {
                result.append('%');
                result.append(hex(static_cast<unsigned char>(character), 2));
            } else {
                result.appendLiteral("%+");
                result.append(hex(static_cast<unsigned char>(character >> 8), 2));
                result.append(hex(static_cast<unsigned char>(character), 2));
            }
        } else
            result.append(character);
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
    Vector<char> buffer(bufferSize);

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
    auto fsRepFileA = fileSystemRepresentation(fileA);
    auto fsRepFileB = fileSystemRepresentation(fileB);

    if (fsRepFileA.isNull() || fsRepFileB.isNull())
        return false;

    bool result = false;

    auto fileADev = getFileDeviceId(fsRepFileA);
    auto fileBDev = getFileDeviceId(fsRepFileB);

    if (fileADev && fileBDev)
        result = (fileADev == fileBDev);

    return result;
}

#if !PLATFORM(MAC)

void setMetadataURL(const String&, const String&, const String&)
{
}

bool canExcludeFromBackup()
{
    return false;
}

bool excludeFromBackup(const String&)
{
    return false;
}

#endif

MappedFileData::~MappedFileData()
{
    if (!m_fileData)
        return;
    unmapViewOfFile(m_fileData, m_fileSize);
}

MappedFileData::MappedFileData(const String& filePath, MappedFileMode mapMode, bool& success)
{
    auto fd = openFile(filePath, FileSystem::FileOpenMode::Read);

    success = mapFileHandle(fd, FileSystem::FileOpenMode::Read, mapMode);
    closeFile(fd);
}

#if HAVE(MMAP)

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

    if (!size) {
        return true;
    }

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

bool unmapViewOfFile(void* buffer, size_t size)
{
    return !munmap(buffer, size);
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

bool fileIsDirectory(const String& path, ShouldFollowSymbolicLinks shouldFollowSymbolicLinks)
{
    auto metadata = shouldFollowSymbolicLinks == ShouldFollowSymbolicLinks::Yes ? fileMetadataFollowingSymlinks(path) : fileMetadata(path);
    if (!metadata)
        return false;
    return metadata.value().type == FileMetadata::Type::Directory;
}

#if !PLATFORM(IOS_FAMILY)
bool isSafeToUseMemoryMapForPath(const String&)
{
    return true;
}

void makeSafeToUseMemoryMapForPath(const String&)
{
}
#endif

#if !PLATFORM(COCOA)
String createTemporaryZipArchive(const String&)
{
    return { };
}
#endif

} // namespace FileSystemImpl
} // namespace WTF
