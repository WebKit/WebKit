/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

namespace WebCore {

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    if (filePath.isEmpty())
        return 0;

    String nullifiedPath = filePath;
    FILE* fileDescriptor = 0;
    if (_wfopen_s(&fileDescriptor, nullifiedPath.charactersWithNullTermination(), TEXT("r+b")) || !fileDescriptor) {
        LOG_ERROR("Failed to open file %s to create shared buffer", filePath.ascii().data());
        return 0;
    }

    RefPtr<SharedBuffer> result;

    // Stat the file to get its size
    struct _stat64 fileStat;
    if (_fstat64(_fileno(fileDescriptor), &fileStat))
        goto exit;

    result = SharedBuffer::create();
    result->m_buffer.resize(fileStat.st_size);
    if (result->m_buffer.size() != fileStat.st_size) {
        result = 0;
        goto exit;
    }

    if (fread(result->m_buffer.data(), 1, fileStat.st_size, fileDescriptor) != fileStat.st_size)
        LOG_ERROR("Failed to fully read contents of file %s - errno(%i)", filePath.ascii().data(), errno);

exit:
    fclose(fileDescriptor);
    return result.release();
}

}; // namespace WebCore
