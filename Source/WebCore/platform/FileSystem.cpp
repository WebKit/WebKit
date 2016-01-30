/*
 * Copyright (C) 2007, 2011 Apple Inc. All rights reserved.
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
#include "FileSystem.h"

#include <wtf/HexNumber.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if !PLATFORM(WIN)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace WebCore {

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
    /* 00-07 */ true,  true,  true,  true,  true,  true,  true,  true, 
    /* 08-0F */ true,  true,  true,  true,  true,  true,  true,  true, 

    /* 10-17 */ true,  true,  true,  true,  true,  true,  true,  true, 
    /* 18-1F */ true,  true,  true,  true,  true,  true,  true,  true, 

    /* 20-27 */ true,  false, true,  false, false, true,  false, false, 
    /* 28-2F */ false, false, true,  false, false, false, false, true, 
    
    /* 30-37 */ false, false, false, false, false, false, false, false, 
    /* 38-3F */ false, false, true,  false, true,  false, true,  true, 
    
    /* 40-47 */ false, false, false, false, false, false, false, false, 
    /* 48-4F */ false, false, false, false, false, false, false, false,
    
    /* 50-57 */ false, false, false, false, false, false, false, false, 
    /* 58-5F */ false, false, false, false, true,  false, false, false,
    
    /* 60-67 */ false, false, false, false, false, false, false, false, 
    /* 68-6F */ false, false, false, false, false, false, false, false,
    
    /* 70-77 */ false, false, false, false, false, false, false, false, 
    /* 78-7F */ false, false, false, false, true,  false, false, true, 
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
                appendByteAsHex(character, result);
            } else {
                result.appendLiteral("%+");
                appendByteAsHex(character >> 8, result);
                appendByteAsHex(character, result);
            }
        } else
            result.append(character);
    }

    return result.toString();
}

#if !PLATFORM(MAC)

void setMetadataURL(String&, const String&, const String&)
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
#if !PLATFORM(WIN)
    if (!m_fileData)
        return;
    munmap(m_fileData, m_fileSize);
#endif
}

MappedFileData::MappedFileData(const String& filePath, bool& success)
{
#if PLATFORM(WIN)
    // FIXME: Implement mapping
    success = false;
#else
    CString fsRep = fileSystemRepresentation(filePath);
    int fd = !fsRep.isNull() ? open(fsRep.data(), O_RDONLY) : -1;
    if (fd < 0) {
        success = false;
        return;
    }

    struct stat fileStat;
    if (fstat(fd, &fileStat)) {
        close(fd);
        success = false;
        return;
    }

    unsigned size;
    if (!WTF::convertSafely(fileStat.st_size, size)) {
        close(fd);
        success = false;
        return;
    }

    if (!size) {
        close(fd);
        success = true;
        return;
    }

    void* data = mmap(0, size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
    close(fd);

    if (data == MAP_FAILED) {
        success = false;
        return;
    }

    success = true;
    m_fileData = data;
    m_fileSize = size;
#endif
}

} // namespace WebCore
