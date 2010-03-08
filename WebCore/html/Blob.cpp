/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "Blob.h"

#include "FileSystem.h"

namespace WebCore {

#if ENABLE(BLOB_SLICE)
const int Blob::toEndOfFile = -1;
const double Blob::doNotCheckFileChange = 0;
#endif

Blob::Blob(const String& path)
    : m_path(path)
#if ENABLE(BLOB_SLICE)
    , m_start(0)
    , m_length(toEndOfFile)
    , m_snapshotCaptured(false)
    , m_snapshotSize(0)
    , m_snapshotModificationTime(doNotCheckFileChange)
#endif
{
}

#if ENABLE(BLOB_SLICE)
Blob::Blob(const String& path, long long start, long long length, long long snapshotSize, double snapshotModificationTime)
    : m_path(path)
    , m_start(start)
    , m_length(length)
    , m_snapshotCaptured(true)
    , m_snapshotSize(snapshotSize)
    , m_snapshotModificationTime(snapshotModificationTime)
{
    ASSERT(start >= 0 && length >= 0 && start + length <= snapshotSize && snapshotModificationTime);
}
#endif

unsigned long long Blob::size() const
{
    // FIXME: JavaScript cannot represent sizes as large as unsigned long long, we need to
    // come up with an exception to throw if file size is not represetable.
#if ENABLE(BLOB_SLICE)
    if (m_snapshotCaptured)
        return m_length;
#endif
    long long size;
    if (!getFileSize(m_path, size))
        return 0;
    return static_cast<unsigned long long>(size);
}

#if ENABLE(BLOB_SLICE)
PassRefPtr<Blob> Blob::slice(long long start, long long length) const
{
    // When we slice a file for the first time, we obtain a snapshot of the file by capturing its current size and modification time.
    // The modification time will be used to verify if the file has been changed or not, when the underlying data are accessed.
    long long snapshotSize;
    double snapshotModificationTime;
    if (m_snapshotCaptured) {
        snapshotSize = m_snapshotSize;
        snapshotModificationTime = m_snapshotModificationTime;
    } else {
        // If we fail to retrieve the size or modification time, probably due to that the file has been deleted, an empty blob will be returned.
        time_t modificationTime;
        if (!getFileSize(m_path, snapshotSize) || !getFileModificationTime(m_path, modificationTime)) {
            snapshotSize = 0;
            snapshotModificationTime = 0;
        } else
            snapshotModificationTime = modificationTime;
    }

    // Clamp the range if it exceeds the size limit.
    if (start < 0)
        start = 0;
    if (length < 0)
        length = 0;

    if (start > snapshotSize) {
        start = 0;
        length = 0;
    } else if (start + length > snapshotSize)
        length = snapshotSize - start;

    return adoptRef(new Blob(m_path, m_start + start, length, snapshotSize, snapshotModificationTime));
}
#endif

} // namespace WebCore
