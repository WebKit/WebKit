/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "MIMETypeCache.h"

#include "ContentType.h"

namespace WebCore {

HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeCache::supportedTypes()
{
    if (!m_supportedTypes) {
        m_supportedTypes = HashSet<String, ASCIICaseInsensitiveHash> { };
        initializeCache(*m_supportedTypes);
    }

    return *m_supportedTypes;
}

bool MIMETypeCache::supportsContainerType(const String& containerType)
{
    if (!isAvailable() || containerType.isEmpty())
        return false;

    if (isUnsupportedContainerType(containerType))
        return false;

    if (staticContainerTypeList().contains(containerType))
        return true;

    return supportedTypes().contains(containerType);
}

MediaPlayerEnums::SupportsType MIMETypeCache::canDecodeType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    if (m_cachedResults) {
        auto it = m_cachedResults->find(mimeType);
        if (it != m_cachedResults->end())
            return it->value;
    }

    auto result = MediaPlayerEnums::SupportsType::IsNotSupported;
    do {
        if (!isAvailable() || mimeType.isEmpty())
            break;

        auto contentType = ContentType { mimeType };
        auto containerType = contentType.containerType();
        if (!supportsContainerType(containerType))
            break;

        if (contentType.codecs().isEmpty()) {
            result = MediaPlayerEnums::SupportsType::MayBeSupported;
            break;
        }

        if (canDecodeExtendedType(contentType))
            result = MediaPlayerEnums::SupportsType::IsSupported;

    } while (0);

    if (!m_cachedResults)
        m_cachedResults = HashMap<String, MediaPlayerEnums::SupportsType, ASCIICaseInsensitiveHash>();
    m_cachedResults->add(mimeType, result);

    return result;
}

void MIMETypeCache::addSupportedTypes(const Vector<String>& newTypes)
{
    if (!m_supportedTypes)
        m_supportedTypes = HashSet<String, ASCIICaseInsensitiveHash> { };

    for (auto& type : newTypes)
        m_supportedTypes->add(type);
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeCache::staticContainerTypeList()
{
    static const auto cache = makeNeverDestroyed(HashSet<String, ASCIICaseInsensitiveHash> { });
    return cache;
}

bool MIMETypeCache::isUnsupportedContainerType(const String&)
{
    return false;
}

bool MIMETypeCache::isAvailable() const
{
    return true;
}

bool MIMETypeCache::isEmpty() const
{
    return m_supportedTypes && m_supportedTypes->isEmpty();
}

void MIMETypeCache::initializeCache(HashSet<String, ASCIICaseInsensitiveHash>&)
{
}

bool MIMETypeCache::canDecodeExtendedType(const ContentType&)
{
    return false;
}

}

