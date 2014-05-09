/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "File.h"

#include "BlobURL.h"
#include "FileMetadata.h"
#include "FileSystem.h"
#include "MIMETypeRegistry.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/CurrentTime.h>
#include <wtf/DateMath.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

File::File(const String& path)
    : Blob(uninitializedContructor)
    , m_path(path)
{
    m_internalURL = BlobURL::createInternalURL();
    m_size = -1;
    computeNameAndContentType(m_path, String(), m_name, m_type);
    ThreadableBlobRegistry::registerFileBlobURL(m_internalURL, path, m_type);
}

File::File(const String& path, const String& nameOverride)
    : Blob(uninitializedContructor)
    , m_path(path)
{
    m_internalURL = BlobURL::createInternalURL();
    m_size = -1;
    computeNameAndContentType(m_path, nameOverride, m_name, m_type);
    ThreadableBlobRegistry::registerFileBlobURL(m_internalURL, path, m_type);
}

File::File(DeserializationContructor, const String& path, const URL& url, const String& type, const String& name)
    : Blob(deserializationContructor, url, type, -1)
    , m_path(path)
    , m_name(name)
{
}

double File::lastModifiedDate() const
{
    time_t modificationTime;
    if (getFileModificationTime(m_path, modificationTime) && isValidFileTime(modificationTime))
        return modificationTime * msPerSecond;

    return currentTime() * msPerSecond;
}

void File::computeNameAndContentType(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType)
{
#if ENABLE(FILE_REPLACEMENT)
    if (shouldReplaceFile(path)) {
        computeNameAndContentTypeForReplacedFile(path, nameOverride, effectiveName, effectiveContentType);
        return;
    }
#endif
    effectiveName = nameOverride.isNull() ? pathGetFileName(path) : nameOverride;
    size_t index = effectiveName.reverseFind('.');
    if (index != notFound)
        effectiveContentType = MIMETypeRegistry::getMIMETypeForExtension(effectiveName.substring(index + 1));
}

String File::contentTypeForFile(const String& path)
{
    String name;
    String type;
    computeNameAndContentType(path, String(), name, type);

    return type;
}

} // namespace WebCore
