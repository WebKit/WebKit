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

File::File(const String& path, ContentTypeLookupPolicy policy)
    : Blob(uninitializedContructor)
    , m_path(path)
    , m_name(pathGetFileName(path))
{
    m_internalURL = BlobURL::createInternalURL();
    m_type = contentTypeFromFilePathOrName(path, policy);
    m_size = -1;
    ThreadableBlobRegistry::registerFileBlobURL(m_internalURL, path, m_type);
}

File::File(const String& path, const String& name, ContentTypeLookupPolicy policy)
    : Blob(uninitializedContructor)
    , m_path(path)
    , m_name(name)
{
    m_internalURL = BlobURL::createInternalURL();
    m_type = contentTypeFromFilePathOrName(name, policy);
    m_size = -1;
    ThreadableBlobRegistry::registerFileBlobURL(m_internalURL, path, m_type);
}

File::File(DeserializationContructor, const String& path, const URL& url, const String& type)
    : Blob(deserializationContructor, url, type, -1)
    , m_path(path)
{
    m_name = pathGetFileName(path);
    // FIXME: File object serialization/deserialization does not include m_name.
    // See SerializedScriptValue.cpp
}

double File::lastModifiedDate() const
{
    time_t modificationTime;
    if (getFileModificationTime(m_path, modificationTime) && isValidFileTime(modificationTime))
        return modificationTime * msPerSecond;

    return currentTime() * msPerSecond;
}

unsigned long long File::size() const
{
    // FIXME: JavaScript cannot represent sizes as large as unsigned long long, we need to
    // come up with an exception to throw if file size is not representable.
    long long size;
    if (!getFileSize(m_path, size))
        return 0;
    return static_cast<unsigned long long>(size);
}

String File::contentTypeFromFilePathOrName(const String& name, File::ContentTypeLookupPolicy policy)
{
    String type;
    int index = name.reverseFind('.');
    if (index != -1) {
        if (policy == File::WellKnownContentTypes)
            type = MIMETypeRegistry::getWellKnownMIMETypeForExtension(name.substring(index + 1));
        else {
            ASSERT(policy == File::AllContentTypes);
            type = MIMETypeRegistry::getMIMETypeForExtension(name.substring(index + 1));
        }
    }
    return type;
}

} // namespace WebCore
