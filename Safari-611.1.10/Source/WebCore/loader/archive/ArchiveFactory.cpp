/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "ArchiveFactory.h"

#include "MIMETypeRegistry.h"

#if ENABLE(WEB_ARCHIVE) && USE(CF)
#include "LegacyWebArchive.h"
#endif
#if ENABLE(MHTML)
#include "MHTMLArchive.h"
#endif

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef RefPtr<Archive> RawDataCreationFunction(const URL&, SharedBuffer&);
typedef HashMap<String, RawDataCreationFunction*, ASCIICaseInsensitiveHash> ArchiveMIMETypesMap;

// The create functions in the archive classes return RefPtr to concrete subclasses
// of Archive. This adaptor makes the functions have a uniform return type.
template<typename ArchiveClass> static RefPtr<Archive> archiveFactoryCreate(const URL& url, SharedBuffer& buffer)
{
    return ArchiveClass::create(url, buffer);
}

static ArchiveMIMETypesMap createArchiveMIMETypesMap()
{
    ArchiveMIMETypesMap map;

#if ENABLE(WEB_ARCHIVE) && USE(CF)
    map.add("application/x-webarchive"_s, archiveFactoryCreate<LegacyWebArchive>);
#endif

#if ENABLE(MHTML)
    map.add("multipart/related"_s, archiveFactoryCreate<MHTMLArchive>);
    map.add("application/x-mimearchive"_s, archiveFactoryCreate<MHTMLArchive>);
#endif

    return map;
}

static ArchiveMIMETypesMap& archiveMIMETypes()
{
    static NeverDestroyed<ArchiveMIMETypesMap> map = createArchiveMIMETypesMap();
    return map;
}

bool ArchiveFactory::isArchiveMIMEType(const String& mimeType)
{
    return !mimeType.isEmpty() && archiveMIMETypes().contains(mimeType);
}

RefPtr<Archive> ArchiveFactory::create(const URL& url, SharedBuffer* data, const String& mimeType)
{
    if (!data)
        return nullptr;
    if (mimeType.isEmpty())
        return nullptr;
    auto* function = archiveMIMETypes().get(mimeType);
    if (!function)
        return nullptr;
    return function(url, *data);
}

void ArchiveFactory::registerKnownArchiveMIMETypes(HashSet<String, ASCIICaseInsensitiveHash>& mimeTypes)
{
    for (auto& mimeType : archiveMIMETypes().keys())
        mimeTypes.add(mimeType);
}

}
