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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef PassRefPtr<Archive> RawDataCreationFunction(const URL&, SharedBuffer*);
typedef HashMap<String, RawDataCreationFunction*, CaseFoldingHash> ArchiveMIMETypesMap;

// The create functions in the archive classes return PassRefPtr to concrete subclasses
// of Archive. This adaptor makes the functions have a uniform return type.
template <typename ArchiveClass> static PassRefPtr<Archive> archiveFactoryCreate(const URL& url, SharedBuffer* buffer)
{
    return ArchiveClass::create(url, buffer);
}

static ArchiveMIMETypesMap& archiveMIMETypes()
{
    DEFINE_STATIC_LOCAL(ArchiveMIMETypesMap, mimeTypes, ());
    static bool initialized = false;

    if (initialized)
        return mimeTypes;

    // FIXME: Remove unnecessary 'static_cast<RawDataCreationFunction*>' from the following 'mimeTypes.set' operations
    // once we switch to a non-broken Visual Studio compiler.  https://bugs.webkit.org/show_bug.cgi?id=121235
#if ENABLE(WEB_ARCHIVE) && USE(CF)
    mimeTypes.set("application/x-webarchive", static_cast<RawDataCreationFunction*>(&archiveFactoryCreate<LegacyWebArchive>));
#endif
#if ENABLE(MHTML)
    mimeTypes.set("multipart/related", static_cast<RawDataCreationFunction*>(&archiveFactoryCreate<MHTMLArchive>));
    mimeTypes.set("application/x-mimearchive", static_cast<RawDataCreationFunction*>(&archiveFactoryCreate<MHTMLArchive>));
#endif

    initialized = true;
    return mimeTypes;
}

bool ArchiveFactory::isArchiveMimeType(const String& mimeType)
{
    return !mimeType.isEmpty() && archiveMIMETypes().contains(mimeType);
}

PassRefPtr<Archive> ArchiveFactory::create(const URL& url, SharedBuffer* data, const String& mimeType)
{
    RawDataCreationFunction* function = mimeType.isEmpty() ? 0 : archiveMIMETypes().get(mimeType);
    return function ? function(url, data) : PassRefPtr<Archive>(0);
}

void ArchiveFactory::registerKnownArchiveMIMETypes()
{
    HashSet<String>& mimeTypes = MIMETypeRegistry::getSupportedNonImageMIMETypes();
    
    for (const auto& mimeType : archiveMIMETypes().keys())
        mimeTypes.add(mimeType);
}

}
