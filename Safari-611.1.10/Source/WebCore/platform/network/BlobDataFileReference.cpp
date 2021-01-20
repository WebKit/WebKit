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
#include <wtf/FileMetadata.h>
#include <wtf/FileSystem.h>

namespace WebCore {

BlobDataFileReference::BlobDataFileReference(const String& path, const String& replacementPath)
    : m_path(path)
    , m_replacementPath(replacementPath)
{
}

BlobDataFileReference::~BlobDataFileReference()
{
    if (!m_replacementPath.isNull())
        FileSystem::deleteFile(m_replacementPath);
}

const String& BlobDataFileReference::path()
{
#if ENABLE(FILE_REPLACEMENT)
    if (m_replacementShouldBeGenerated)
        generateReplacementFile();
#endif
    if (!m_replacementPath.isNull())
        return m_replacementPath;

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

Optional<WallTime> BlobDataFileReference::expectedModificationTime()
{
#if ENABLE(FILE_REPLACEMENT)
    // We do not currently track modifications for generated files, because we have a snapshot.
    // Unfortunately, this is inconsistent with regular file handling - File objects should be invalidated when underlying files change.
    if (m_replacementShouldBeGenerated || !m_replacementPath.isNull())
        return WTF::nullopt;
#endif
    return m_expectedModificationTime;
}

void BlobDataFileReference::startTrackingModifications()
{
    // This is not done automatically by the constructor, because BlobDataFileReference is
    // also used to pass paths around before registration. Only registered blobs need to pay
    // the cost of tracking file modifications.

#if ENABLE(FILE_REPLACEMENT)
    m_replacementShouldBeGenerated = File::shouldReplaceFile(m_path);
#endif

    // FIXME: Some platforms provide better ways to listen for file system object changes, consider using these.
    auto metadata = FileSystem::fileMetadataFollowingSymlinks(m_path);
    if (!metadata)
        return;

    m_expectedModificationTime = metadata.value().modificationTime;

#if ENABLE(FILE_REPLACEMENT)
    if (m_replacementShouldBeGenerated)
        return;
#endif

    // This is a registered blob with a replacement file. Get the Metadata of the replacement file.
    if (!m_replacementPath.isNull()) {
        metadata = FileSystem::fileMetadataFollowingSymlinks(m_replacementPath);
        if (!metadata)
            return;
    }

    m_size = metadata.value().length;
}

void BlobDataFileReference::prepareForFileAccess()
{
}

void BlobDataFileReference::revokeFileAccess()
{
}

}
