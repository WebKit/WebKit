/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SharedBuffer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wtf/FileSystem.h>
#include <wtf/text/CString.h>

namespace WebCore {

RefPtr<SharedBuffer> SharedBuffer::createFromReadingFile(const String& filePath)
{
    if (filePath.isEmpty())
        return nullptr;

    CString filename = FileSystem::fileSystemRepresentation(filePath);
    int fd = open(filename.data(), O_RDONLY);
    if (fd == -1)
        return nullptr;

    struct stat fileStat;
    if (fstat(fd, &fileStat)) {
        close(fd);
        return nullptr;
    }

    size_t bytesToRead;
    if (!WTF::convertSafely(fileStat.st_size, bytesToRead)) {
        close(fd);
        return nullptr;
    }

    Vector<char> buffer(bytesToRead);

    size_t totalBytesRead = 0;
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer.data() + totalBytesRead, bytesToRead - totalBytesRead)) > 0)
        totalBytesRead += bytesRead;

    close(fd);

    if (totalBytesRead != bytesToRead)
        return nullptr;

    return SharedBuffer::create(WTFMove(buffer));
}

} // namespace WebCore
