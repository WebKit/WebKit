/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BlobDataFileReference.h"

#include "File.h"
#include "FileMetadata.h"

namespace WebCore {

BlobDataFileReference::BlobDataFileReference(const String& path)
    : m_path(path)
#if ENABLE(FILE_REPLACEMENT)
    , m_replacementShouldBeGenerated(false)
#endif
    , m_size(0)
    , m_expectedModificationTime(invalidFileTime())
{
}

BlobDataFileReference::~BlobDataFileReference()
{
#if ENABLE(FILE_REPLACEMENT)
    if (!m_replacementPath.isNull())
        deleteFile(m_replacementPath);
#endif
}

const String& BlobDataFileReference::path()
{
#if ENABLE(FILE_REPLACEMENT)
    if (m_replacementShouldBeGenerated)
        generateReplacementFile();

    if (!m_replacementPath.isNull())
        return m_replacementPath;
#endif

    return m_path;
}

unsigned long long BlobDataFileReference::size()
{
#if ENABLE(FILE_REPLACEMENT)
    if (m_replacementShouldBeGenerated)
        generateReplacementFile();
#endif

    return m_size;
}

double BlobDataFileReference::expectedModificationTime()
{
#if ENABLE(FILE_REPLACEMENT)
    // We do not currently track modifications for generated files, because we have a snapshot.
    // Unfortunately, this is inconsistent with regular file handling - File objects should be invalidated when underlying files change.
    if (m_replacementShouldBeGenerated || !m_replacementPath.isNull())
        return invalidFileTime();
#endif

    return m_expectedModificationTime;
}

void BlobDataFileReference::startTrackingModifications()
{
    // This is not done automatically by the constructor, because BlobDataFileReference is
    // also used to pass paths around before registration. Only registered blobs need to pay
    // the cost of tracking file modifications.

    ASSERT(!isValidFileTime(m_expectedModificationTime));

#if ENABLE(FILE_REPLACEMENT)
    m_replacementShouldBeGenerated = File::shouldReplaceFile(m_path);
#endif

    // FIXME: Some platforms provide better ways to listen for file system object changes, consider using these.
    FileMetadata metadata;
    if (!getFileMetadata(m_path, metadata))
        return;

    m_expectedModificationTime = metadata.modificationTime;

#if ENABLE(FILE_REPLACEMENT)
    if (m_replacementShouldBeGenerated)
        return;
#endif

    m_size = metadata.length;
}

void BlobDataFileReference::prepareForFileAccess()
{
}

void BlobDataFileReference::revokeFileAccess()
{
}

}
