/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(FILE_READER) || ENABLE(FILE_WRITER)

#include "FileStream.h"

#include "Blob.h"
#include "PlatformString.h"

namespace WebCore {

FileStream::FileStream(FileStreamClient* client)
    : m_client(client)
    , m_handle(invalidPlatformFileHandle)
    , m_bytesProcessed(0)
    , m_totalBytesToRead(0)
{
}

FileStream::~FileStream()
{
    ASSERT(!isHandleValid(m_handle));
}

void FileStream::start()
{
    ASSERT(!isMainThread());
    m_client->didStart();
}

void FileStream::stop()
{
    ASSERT(!isMainThread());
    close();
    m_client->didStop();
}

void FileStream::openForRead(Blob* blob)
{
    ASSERT(!isMainThread());

    if (isHandleValid(m_handle))
        return;

    // Check if the file exists by querying its modification time. We choose not to call fileExists() in order to save an
    // extra file system call when the modification time is needed to check the validity of the sliced file blob.
    // Per the spec, we need to return different error codes to differentiate between non-existent file and permission error.
    // openFile() could not tell use the failure reason.
    time_t currentModificationTime;
    if (!getFileModificationTime(blob->path(), currentModificationTime)) {
        m_client->didFail(NOT_FOUND_ERR);
        return;
    }

    // Open the file blob.
    m_handle = openFile(blob->path(), OpenForRead);
    if (!isHandleValid(m_handle)) {
        m_client->didFail(NOT_READABLE_ERR);
        return;
    }

#if ENABLE(BLOB_SLICE)
    // Check the modificationt time for the possible file change.
    if (blob->modificationTime() != Blob::doNotCheckFileChange && static_cast<time_t>(blob->modificationTime()) != currentModificationTime) {
        m_client->didFail(NOT_READABLE_ERR);
        return;
    }

    // Jump to the beginning position if the file has been sliced.
    if (blob->start() > 0) {
        if (!seekFile(m_handle, blob->start(), SeekFromBeginning)) {
            m_client->didFail(NOT_READABLE_ERR);
            return;
        }
    }
#endif

    // Get the size.
#if ENABLE(BLOB_SLICE)
    m_totalBytesToRead = blob->length();
    if (m_totalBytesToRead == Blob::toEndOfFile)
        m_totalBytesToRead = blob->size() - blob->start();
#else
    m_total = blob->size();
#endif

    m_client->didGetSize(m_totalBytesToRead);
}

void FileStream::openForWrite(const String&)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

void FileStream::close()
{
    ASSERT(!isMainThread());
    if (isHandleValid(m_handle)) {
        closeFile(m_handle);
        m_handle = invalidPlatformFileHandle;
    }
}

void FileStream::read(char* buffer, int length)
{
    ASSERT(!isMainThread());

    if (!isHandleValid(m_handle)) {
        m_client->didFail(NOT_READABLE_ERR);
        return;
    }

    if (m_bytesProcessed >= m_totalBytesToRead) {
        m_client->didFinish();
        return;
    }

    long long remaining = m_totalBytesToRead - m_bytesProcessed;
    int bytesToRead = (remaining < length) ? static_cast<int>(remaining) : length;
    int bytesRead = readFromFile(m_handle, buffer, bytesToRead);
    if (bytesRead < 0) {
        m_client->didFail(NOT_READABLE_ERR);
        return;
    }

    if (!bytesRead) {
        m_client->didFinish();
        return;
    }

    m_bytesProcessed += bytesRead;
    m_client->didRead(buffer, bytesRead);
}

void FileStream::write(Blob*, long long, int)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

void FileStream::truncate(long long)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

} // namespace WebCore

#endif // ENABLE(FILE_READER) || ENABLE(FILE_WRITER)
